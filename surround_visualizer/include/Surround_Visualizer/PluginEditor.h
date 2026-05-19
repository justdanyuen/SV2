#pragma once

namespace surround_vis {

class PluginProcessor;

class PluginEditor : public juce::AudioProcessorEditor,
                     private juce::Timer {
public:
  explicit PluginEditor(PluginProcessor& p);
  ~PluginEditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;
  void visibilityChanged() override;

private:
  PluginProcessor& svProcessor;

  juce::TextButton surroundTab{"Surround field"};
  juce::TextButton spectrumTab{"Frequency spectrum"};
  int activeTab{0};

  // Instance role selector
  juce::Label    instanceLabel;
  juce::ComboBox instanceSelector;

  // Global controls
  juce::TextButton showAllButton{"Show All"};
  juce::TextButton hideAllButton{"Hide All"};

  SurroundVisualizerComponent surroundView;
  SpectrumVisualizerComponent spectrumView;

  struct GroupControls {
    juce::ComboBox     groupSelector;
    juce::TextButton   enableButton;
    juce::TextButton   soloButton;
  };
  std::array<GroupControls, kGroupCount> groupControls;

  // Bitmask — bit gi set means group gi is visible in the visualizers.
  // All groups visible by default.
  uint8_t enabledMask{0x3F};  // 0011 1111 = all 6 groups on
  uint8_t soloMask{0x00};     // 0 = no solos active
  int startupTicksRemaining{0};

  void buildControls();
  void switchTab(int index);
  void timerCallback() override;

  static constexpr int kEditorW = 800;
  static constexpr int kEditorH = 590;
  static constexpr int kTabBarH = 38;
  static constexpr int kCtrlH   = 110;
  static constexpr int kViewH   = kEditorH - kTabBarH - kCtrlH - 8;

  static juce::Colour groupColour(int id);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

}  // namespace surround_vis
