#include "surround_plugin.h"

namespace surround_vis {

// =========================================================================
// FftAnalyzer — background thread for FFT + RMS + shared memory writes
// =========================================================================
class PluginProcessor::FftAnalyzer : private juce::Thread {
public:
  static constexpr int kFftOrder = 11;
  static constexpr int kFftSize  = 1 << kFftOrder;  // 2048

  FftAnalyzer(std::array<SampleFifo<float>, kChannelCount>& fifos,
              SharedMemoryBridge& bridge,
              Parameters& params,
              std::atomic<int>& dbgGroup,
              std::atomic<bool>& dbgEn,
              std::atomic<int>& dbgCount)
      : juce::Thread("SV2 FFT"),
        channelFifos(fifos),
        sharedMemory(bridge),
        parameters(params),
        dbgLastGroupId(dbgGroup),
        dbgLastEnabled(dbgEn),
        dbgWriteCount(dbgCount),
        fft(kFftOrder),
        window(kFftSize, juce::dsp::WindowingFunction<float>::hann) {
    startThread(juce::Thread::Priority::low);
  }

  ~FftAnalyzer() override { stopThread(500); }

  void setSampleRate(double sr) noexcept {
    sampleRate.store(sr, std::memory_order_relaxed);
  }

private:
  void run() override {
    juce::AudioBuffer<float> scratchBuf;
    std::array<float, kFftSize * 2> fftData{};
    float rmsAccum[kChannelCount]{};
    float rmsPeak[kChannelCount]{};

    while (!threadShouldExit()) {
      bool anyData = false;

      // Clear FFT accumulation buffer before mixing channels
      std::fill(fftData.begin(), fftData.end(), 0.f);
      int fftContribCount = 0;

      for (int ch = 0; ch < kChannelCount; ++ch) {
        channelFifos[ch].popAll(scratchBuf);
        const int n = scratchBuf.getNumSamples();
        if (n == 0) continue;
        anyData = true;

        // RMS per channel
        float sumSq = 0.f;
        const float* data = scratchBuf.getReadPointer(0);
        for (int i = 0; i < n; ++i) sumSq += data[i] * data[i];
        rmsAccum[ch] = std::sqrt(sumSq / static_cast<float>(n));

        // Mix ALL active channels into FFT buffer so spatial placement
        // (rear, center, surround) doesn't affect spectrum display.
        // LFE (ch 3) excluded — sub content would skew the spectrum.
        if (ch != 3) {
          const int copyLen = std::min(n, kFftSize);
          for (int i = 0; i < copyLen; ++i)
            fftData[i] += data[i];
          ++fftContribCount;
        }
      }

      // Normalize mixed FFT buffer by number of contributing channels
      if (fftContribCount > 1) {
        const float inv = 1.f / static_cast<float>(fftContribCount);
        for (auto& s : fftData) s *= inv;
      }

      if (!anyData) {
        wait(5);
        continue;
      }

      window.multiplyWithWindowingTable(fftData.data(), kFftSize);
      fft.performFrequencyOnlyForwardTransform(fftData.data());

      float outFft[kFftBinCount]{};
      const float normFactor = 4.f / static_cast<float>(kFftSize);
      for (int bin = 0; bin < kFftBinCount; ++bin)
        outFft[bin] = fftData[bin] * normFactor;

      // Apply analysis trim
      const float trimGain = juce::Decibels::decibelsToGain(
          parameters.analysisTrim.get());
      for (auto& v : outFft)   v *= trimGain;
      for (auto& v : rmsAccum) v *= trimGain;

      const int rawIndex = parameters.voiceGroup.getIndex();
      if (rawIndex == 0) { wait(33); continue; }  // placeholder
      const int  groupId   = rawIndex - 1;  // 1=Soprano->0 ... 6=Bass->5
      const bool isEnabled = parameters.enabled.get();
      dbgLastGroupId.store(groupId, std::memory_order_relaxed);
      dbgLastEnabled.store(isEnabled, std::memory_order_relaxed);
      dbgWriteCount.fetch_add(1, std::memory_order_relaxed);
      sharedMemory.writeSlot(groupId, groupId, isEnabled, rmsPeak, outFft);

      // Apply peak hold with decay rather than resetting to zero
      // so quiet passages still show some level.
      for (int ch = 0; ch < kChannelCount; ++ch) {
        rmsPeak[ch] = juce::jmax(rmsAccum[ch], rmsPeak[ch] * 0.85f);
        rmsAccum[ch] = 0.f;
      }

      wait(33);  // ~30 fps
    }
  }

  std::array<SampleFifo<float>, kChannelCount>& channelFifos;
  SharedMemoryBridge& sharedMemory;
  Parameters&               parameters;
  std::atomic<int>&         dbgLastGroupId;
  std::atomic<bool>&        dbgLastEnabled;
  std::atomic<int>&         dbgWriteCount;
  juce::dsp::FFT fft;
  juce::dsp::WindowingFunction<float> window;
  std::atomic<double> sampleRate{44100.0};
};

// =========================================================================
// PluginProcessor
// =========================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor(
          BusesProperties()
              .withInput("Input",  juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
  sharedMemory.open();

  fftAnalyzer = std::make_unique<FftAnalyzer>(channelFifos, sharedMemory, parameters,
                                               dbgLastGroupId, dbgLastEnabled, dbgWriteCount);
}

PluginProcessor::~PluginProcessor() {
  fftAnalyzer.reset();
  sharedMemory.close();
}

const juce::String PluginProcessor::getName() const {
  return SV2_PLUGIN_NAME;
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  const auto& in  = layouts.getMainInputChannelSet();
  const auto& out = layouts.getMainOutputChannelSet();
  if (in != out) return false;
  return in == juce::AudioChannelSet::create5point1()
      || in == juce::AudioChannelSet::stereo()
      || in == juce::AudioChannelSet::mono();
}

void PluginProcessor::prepareToPlay(double sampleRate, int maxBlockSize) {
  currentSampleRate.store(sampleRate, std::memory_order_relaxed);
  audioRunning.store(true, std::memory_order_relaxed);
  fftAnalyzer->setSampleRate(sampleRate);
  for (auto& fifo : channelFifos)
    fifo.prepare(sampleRate);

  // Always write slot in prepareToPlay so lastWriteMs is fresh.
  // Without this, fresh inserts (no saved state) have lastWriteMs=0
  // which makes age = now - 0 = huge, failing the 2000ms stale check.
  // The FftAnalyzer overwrites this with real data almost immediately.
  if (sharedMemory.isOpen()) {
    const int rawIdx = parameters.voiceGroup.getIndex();
    if (rawIdx > 0) {
      const int groupId = rawIdx - 1;
      float zeroRms[kChannelCount]{};
      float zeroFft[kFftBinCount]{};
      sharedMemory.writeSlot(groupId, groupId, true, zeroRms, zeroFft);
    }
  }

  juce::ignoreUnused(maxBlockSize);
}

void PluginProcessor::releaseResources() {
  audioRunning.store(false, std::memory_order_relaxed);
  for (auto& fifo : channelFifos)
    fifo.reset();
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                   juce::MidiBuffer& midiMessages) {
  juce::ignoreUnused(midiMessages);
  juce::ScopedNoDenormals noDenormals;

  // Analysis only — audio passes through unmodified.
  if (!parameters.enabled.get()) return;

  const int numChannels = std::min(buffer.getNumChannels(), kChannelCount);
  const int numSamples  = buffer.getNumSamples();

  for (int ch = 0; ch < numChannels; ++ch) {
    const float* data = buffer.getReadPointer(ch);
    for (int i = 0; i < numSamples; ++i)
      channelFifos[ch].push(data[i]);
  }
}

bool PluginProcessor::readGroupSnapshot(int groupIndex, GroupSnapshot& out) {
  const bool valid = sharedMemory.readSlot(groupIndex,
                                           out.colorId, out.enabled,
                                           out.rms, out.fft);
  // Store read-side debug for slot 2 (Alto) specifically
  if (groupIndex == 2) {
    dbgReadSlot   .store(groupIndex,  std::memory_order_relaxed);
    dbgReadValid  .store(valid,       std::memory_order_relaxed);
    dbgReadEnabled.store(out.enabled, std::memory_order_relaxed);
  }
  return valid;
}

bool PluginProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* PluginProcessor::createEditor() {
  return new PluginEditor(*this);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData) {
  juce::MemoryOutputStream out{destData, true};
  JsonSerializer::serialize(parameters, out);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes) {
  juce::MemoryInputStream in{data, static_cast<size_t>(sizeInBytes), false};
  const auto result = JsonSerializer::deserialize(in, parameters);
  if (result.failed())
    DBG(result.getErrorMessage());

  // Write slot AFTER state is restored so we use the correct voice group.
  // This is the reliable initialization point — state is fully loaded here.
  stateRestored = true;
  if (sharedMemory.isOpen()) {
    const int rawIdx = parameters.voiceGroup.getIndex();
    if (rawIdx > 0) {
      const int groupId = rawIdx - 1;
      float zeroRms[kChannelCount]{};
      float zeroFft[kFftBinCount]{};
      sharedMemory.writeSlot(groupId, groupId, true, zeroRms, zeroFft);
    }
  }
}

}  // namespace surround_vis

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new surround_vis::PluginProcessor();
}
