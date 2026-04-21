#pragma once

namespace surround_vis {

struct AnalysisData {
  float rms[kChannelCount]{};
  float fft[kFftBinCount]{};
};

class PluginProcessor : public juce::AudioProcessor {
public:
  PluginProcessor();
  ~PluginProcessor() override;

  void prepareToPlay(double sampleRate, int maxBlockSize) override;
  void releaseResources() override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  using AudioProcessor::processBlock;

  bool isBusesLayoutSupported(const BusesLayout&) const override;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override;

  const juce::String getName() const override;
  bool acceptsMidi()  const override { return false; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }

  int getNumPrograms()    override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String&) override {}

  void getStateInformation(juce::MemoryBlock&) override;
  void setStateInformation(const void*, int) override;

  [[nodiscard]] Parameters& getParameterRefs() noexcept { return parameters; }

  struct GroupSnapshot {
    int   colorId{-1};
    bool  enabled{false};
    float rms[kChannelCount]{};
    float fft[kFftBinCount]{};
  };

  bool readGroupSnapshot(int groupIndex, GroupSnapshot& out) const;

  double getSampleRateThreadSafe() const noexcept {
    return currentSampleRate.load(std::memory_order_relaxed);
  }

  bool isAudioRunning() const noexcept {
    return audioRunning.load(std::memory_order_relaxed);
  }

private:
  Parameters parameters{*this};
  std::array<SampleFifo<float>, kChannelCount> channelFifos;

  class FftAnalyzer;
  std::unique_ptr<FftAnalyzer> fftAnalyzer;

  SharedMemoryBridge sharedMemory;
  std::atomic<double> currentSampleRate{0.0};
  std::atomic<bool>   audioRunning{false};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

}  // namespace surround_vis
