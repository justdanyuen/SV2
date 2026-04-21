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

  SurroundVisualizerComponent surroundView;
  SpectrumVisualizerComponent spectrumView;

  struct GroupControls {
    juce::ComboBox     groupSelector;
    juce::ToggleButton enableButton;
    juce::Slider       trimSlider;
    juce::Label        trimLabel;
  };
  std::array<GroupControls, kGroupCount> groupControls;

  void buildControls();
  void switchTab(int index);
  void timerCallback() override;

  static constexpr int kEditorW = 720;
  static constexpr int kEditorH = 600;
  static constexpr int kTabBarH = 34;
  static constexpr int kCtrlH   = 130;
  static constexpr int kViewH   = kEditorH - kTabBarH - kCtrlH - 8;

  static juce::Colour groupColour(int id);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

}  // namespace surround_vis
