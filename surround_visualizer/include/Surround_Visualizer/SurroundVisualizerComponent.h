#pragma once

namespace surround_vis {

class PluginProcessor;

class SurroundVisualizerComponent : public juce::Component {
public:
  explicit SurroundVisualizerComponent(PluginProcessor& p);
  ~SurroundVisualizerComponent() override = default;

  void paint(juce::Graphics&) override;
  void resized() override {}

  // Called by PluginEditor's timer every ~33ms to update data.
  void tick();

  // Draw directly into a graphics context with explicit bounds.
  // Used when the editor calls this from its own paint() method.
  void drawInto(juce::Graphics& g, juce::Rectangle<int> bounds, uint8_t enabledMask = 0x3F);

private:

  // -----------------------------------------------------------------------
  // Snapshot data — written on timer thread, read on paint thread (same
  // message thread, so no lock needed).
  // -----------------------------------------------------------------------
  struct GroupData {
    bool  enabled{false};
    int   colorId{-1};
    float dirRms[5]{};   // L, R, C, Ls, Rs
    float lfeRms{0.f};
    float lfeSmooth{0.f};
  };
  std::array<GroupData, kGroupCount> groups;

  // -----------------------------------------------------------------------
  // Drawing helpers
  // -----------------------------------------------------------------------
  void drawGrid        (juce::Graphics&, float cx, float cy, float maxR) const;
  void drawSpeakers    (juce::Graphics&, float cx, float cy, float maxR) const;
  void drawLfeZone     (juce::Graphics&, float cx, float cy, float maxR) const;
  void drawLfeArcs     (juce::Graphics&, float cx, float cy, float maxR) const;
  void drawPolarCurves (juce::Graphics&, float cx, float cy, float maxR) const;
  void drawListener    (juce::Graphics&, float cx, float cy) const;

  // Interpolated polar level for a group at a given angle (radians).
  // Uses only the 5 directional channels — LFE excluded.
  float polarLevel(int groupIdx, float angleRad) const;

  // Geometry — angles in radians, canvas convention (0=right, CW positive).
  // C=12, R=~2, Rs=~4:30, Ls=~7:30, L=~9:30, LFE zone=6
  static constexpr float DEG = juce::MathConstants<float>::pi / 180.f;
  static constexpr float DIR_ANGLES[5] = {
      210.f * DEG,  // L
      330.f * DEG,  // R
      270.f * DEG,  // C
      150.f * DEG,  // Ls
       30.f * DEG,  // Rs
  };

  static constexpr int POLAR_STEPS = 360;
  static constexpr int SMOOTH_HALF = 8;

  // Color table matching the voice group order
  static juce::Colour groupColour(int id);

  PluginProcessor& svProcessor;

  // Test-mode phase accumulator — produces animated dummy data when
  // no real audio is flowing.
  float testPhase{0.f};
  bool  useTestData{true};

  void generateTestData();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SurroundVisualizerComponent)
};

}  // namespace surround_vis
