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
              Parameters& params)
      : juce::Thread("SV2 FFT"),
        channelFifos(fifos),
        sharedMemory(bridge),
        parameters(params),
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

      for (int ch = 0; ch < kChannelCount; ++ch) {
        channelFifos[ch].popAll(scratchBuf);
        const int n = scratchBuf.getNumSamples();
        if (n == 0) continue;
        anyData = true;

        // RMS
        float sumSq = 0.f;
        const float* data = scratchBuf.getReadPointer(0);
        for (int i = 0; i < n; ++i) sumSq += data[i] * data[i];
        rmsAccum[ch] = std::sqrt(sumSq / static_cast<float>(n));

        // Fill FFT buffer from channel 0 (L) for spectrum display
        if (ch == 0) {
          const int copyLen = std::min(n, kFftSize);
          std::copy_n(data, copyLen, fftData.begin());
          if (copyLen < kFftSize)
            std::fill(fftData.begin() + copyLen, fftData.begin() + kFftSize, 0.f);
        }
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

      const int  groupId = parameters.voiceGroup.getIndex();
      const bool isEnabled = parameters.enabled.get();
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
  Parameters& parameters;
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

  fftAnalyzer = std::make_unique<FftAnalyzer>(channelFifos, sharedMemory, parameters);
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

  // Write slot here for fresh inserts where setStateInformation
  // was never called. For session loads, setStateInformation already
  // wrote the correct slot so this is a safe redundant write.
  if (sharedMemory.isOpen()) {
    const int groupId = juce::jlimit(0, 5,
        parameters.voiceGroup.getIndex());
    float zeroRms[kChannelCount]{};
    float zeroFft[kFftBinCount]{};
    sharedMemory.writeSlot(groupId, groupId, true, zeroRms, zeroFft);
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

bool PluginProcessor::readGroupSnapshot(int groupIndex, GroupSnapshot& out) const {
  return sharedMemory.readSlot(groupIndex,
                               out.colorId, out.enabled,
                               out.rms, out.fft);
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
  if (sharedMemory.isOpen()) {
    const int groupId = juce::jlimit(0, 5,
        parameters.voiceGroup.getIndex());
    float zeroRms[kChannelCount]{};
    float zeroFft[kFftBinCount]{};
    sharedMemory.writeSlot(groupId, groupId, true, zeroRms, zeroFft);
  }
}

}  // namespace surround_vis

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new surround_vis::PluginProcessor();
}
