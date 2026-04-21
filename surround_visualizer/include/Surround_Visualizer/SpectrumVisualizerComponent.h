#pragma once

namespace surround_vis {

class PluginProcessor;

class SpectrumVisualizerComponent : public juce::Component {
public:
  explicit SpectrumVisualizerComponent(PluginProcessor& p);
  ~SpectrumVisualizerComponent() override = default;

  void paint(juce::Graphics&) override;
  void resized() override {}

  // Called by PluginEditor's timer every ~33ms to update data.
  void tick();

  // Draw directly into a graphics context with explicit bounds.
  void drawInto(juce::Graphics& g, juce::Rectangle<int> bounds);

private:

  struct GroupData {
    bool  enabled{false};
    int   colorId{-1};
    float lfeLevel{0.f};
    float bins[kFftBinCount]{};
    float smoothBins[kFftBinCount]{};
  };
  std::array<GroupData, kGroupCount> groups;

  void generateTestData();
  void drawGrid    (juce::Graphics&, juce::Rectangle<float> area) const;
  void drawCurves  (juce::Graphics&, juce::Rectangle<float> area) const;
  void drawLegend  (juce::Graphics&, juce::Rectangle<float> area) const;

  // Maps a linear bin index to an x position using a log frequency scale.
  float binToX(int bin, float areaX, float areaW, double sampleRate) const;

  static juce::Colour groupColour(int id);

  PluginProcessor& svProcessor;
  float testPhase{0.f};
  bool  useTestData{true};
  double cachedSampleRate{48000.0};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumVisualizerComponent)
};

}  // namespace surround_vis
