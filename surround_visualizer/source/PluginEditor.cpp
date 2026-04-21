#include "surround_plugin.h"

namespace surround_vis {

juce::Colour PluginEditor::groupColour(int id) {
  static const juce::Colour cols[kGroupCount] = {
    juce::Colour(0xff7F77DD),
    juce::Colour(0xffD4537E),
    juce::Colour(0xff378ADD),
    juce::Colour(0xff1D9E75),
    juce::Colour(0xffBA7517),
    juce::Colour(0xffD85A30),
  };
  return (id >= 0 && id < kGroupCount) ? cols[id] : juce::Colours::grey;
}

// =========================================================================
// Constructor
// =========================================================================
PluginEditor::PluginEditor(PluginProcessor& p)
    : juce::AudioProcessorEditor(&p),
      svProcessor(p),
      surroundView(p),
      spectrumView(p) {

  setSize(kEditorW, kEditorH);

  // Tab buttons
  surroundTab.setClickingTogglesState(false);
  spectrumTab.setClickingTogglesState(false);
  surroundTab.onClick = [this] { switchTab(0); };
  spectrumTab.onClick = [this] { switchTab(1); };
  addAndMakeVisible(surroundTab);
  addAndMakeVisible(spectrumTab);

  // Visualizer panels
  addAndMakeVisible(surroundView);
  addChildComponent(spectrumView);

  // Components drawn directly via drawInto() — hide as children
  // to prevent double-drawing, bounds still set in resized().
  surroundView.setVisible(false);
  spectrumView.setVisible(false);

  buildControls();
  switchTab(0);
}

void PluginEditor::visibilityChanged() {
  // Start timer only once the component is actually visible and
  // the message thread is fully running — fixes frozen UI when
  // launched via macOS 'open' command.
  if (isVisible() && !isTimerRunning())
    startTimerHz(30);
}

PluginEditor::~PluginEditor() {
  stopTimer();
}

void PluginEditor::timerCallback() {
  surroundView.tick();
  spectrumView.tick();
  // Editor paint() calls the visualizer draw methods directly,
  // so repainting the editor repaints the visualizers.
  repaint();
}

// =========================================================================
// buildControls
// =========================================================================
void PluginEditor::buildControls() {
  // Instance selector — sets which voice group THIS instance writes to
  instanceSelector.addItem("Soprano",  1);
  instanceSelector.addItem("Mezzo",    2);
  instanceSelector.addItem("Alto",     3);
  instanceSelector.addItem("Tenor",    4);
  instanceSelector.addItem("Baritone", 5);
  instanceSelector.addItem("Bass",     6);
  const int currentGroup = juce::jlimit(0, 5,
      svProcessor.getParameterRefs().voiceGroup.getIndex());
  instanceSelector.setSelectedId(currentGroup + 1, juce::dontSendNotification);
  instanceSelector.setColour(juce::ComboBox::backgroundColourId,
                             juce::Colour(0xff2a2a35));
  instanceSelector.setColour(juce::ComboBox::textColourId,
                             juce::Colours::white);
  instanceSelector.setColour(juce::ComboBox::outlineColourId,
                             juce::Colour(0xff666666));
  instanceSelector.onChange = [this] {
    const int sel = juce::jlimit(0, 5, instanceSelector.getSelectedId() - 1);
    const float norm = static_cast<float>(sel) / 5.f;
    svProcessor.getParameterRefs().voiceGroup.setValueNotifyingHost(norm);
  };
  addAndMakeVisible(instanceSelector);

  instanceLabel.setText("This instance:", juce::dontSendNotification);
  instanceLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.f)));
  instanceLabel.setColour(juce::Label::textColourId,
                          juce::Colour(0xff888888));
  addAndMakeVisible(instanceLabel);

  // Per-slot display columns (read-only status)
  for (int gi = 0; gi < kGroupCount; ++gi) {
    auto& gc = groupControls[gi];
    const juce::Colour col = groupColour(gi);

    gc.groupSelector.addItem("Soprano",  1);
    gc.groupSelector.addItem("Mezzo",    2);
    gc.groupSelector.addItem("Alto",     3);
    gc.groupSelector.addItem("Tenor",    4);
    gc.groupSelector.addItem("Baritone", 5);
    gc.groupSelector.addItem("Bass",     6);
    gc.groupSelector.setSelectedId(gi + 1, juce::dontSendNotification);
    gc.groupSelector.setEnabled(false);
    gc.groupSelector.setColour(juce::ComboBox::backgroundColourId,
                               juce::Colour(0xff1e1e24));
    gc.groupSelector.setColour(juce::ComboBox::textColourId,
                               col.withAlpha(0.7f));
    gc.groupSelector.setColour(juce::ComboBox::outlineColourId,
                               col.withAlpha(0.3f));
    addAndMakeVisible(gc.groupSelector);

    gc.enableButton.setButtonText("on");
    gc.enableButton.setToggleState(true, juce::dontSendNotification);
    gc.enableButton.setColour(juce::ToggleButton::textColourId, col);
    addAndMakeVisible(gc.enableButton);

    gc.trimSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    gc.trimSlider.setRange(-12.0, 12.0, 0.1);
    gc.trimSlider.setValue(0.0, juce::dontSendNotification);
    gc.trimSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    gc.trimSlider.setColour(juce::Slider::thumbColourId, col);
    gc.trimSlider.setColour(juce::Slider::trackColourId,
                            col.withAlpha(0.4f));
    addAndMakeVisible(gc.trimSlider);

    gc.trimLabel.setText("0 dB", juce::dontSendNotification);
    gc.trimLabel.setFont(juce::Font(juce::FontOptions().withHeight(9.f)));
    gc.trimLabel.setColour(juce::Label::textColourId,
                           juce::Colour(0xff666666));
    gc.trimLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(gc.trimLabel);

    gc.trimSlider.onValueChange = [this, gi] {
      const double v = groupControls[gi].trimSlider.getValue();
      groupControls[gi].trimLabel.setText(
          juce::String(v, 1) + " dB", juce::dontSendNotification);
    };
  }
}

// =========================================================================
// switchTab
// =========================================================================
void PluginEditor::switchTab(int index) {
  activeTab = index;

  const auto activeCol   = juce::Colour(0xff2a2a35);
  const auto inactiveCol = juce::Colours::transparentBlack;

  surroundTab.setColour(juce::TextButton::buttonColourId,
                        index == 0 ? activeCol : inactiveCol);
  spectrumTab.setColour(juce::TextButton::buttonColourId,
                        index == 1 ? activeCol : inactiveCol);
  repaint();
}

// =========================================================================
// paint
// =========================================================================
void PluginEditor::paint(juce::Graphics& g) {
  g.fillAll(juce::Colour(0xff141418));

  // Draw active visualizer with explicit bounds via drawInto()
  const auto viewBounds = juce::Rectangle<int>(0, kTabBarH, getWidth(), kViewH);
  if (activeTab == 0)
    surroundView.drawInto(g, viewBounds);
  else
    spectrumView.drawInto(g, viewBounds);

  // Control area separator
  const int ctrlY = kTabBarH + kViewH + 4;
  g.setColour(juce::Colour(0xff2a2a35));
  g.drawHorizontalLine(ctrlY, 0.f, static_cast<float>(getWidth()));

  // Group color swatches in control area header
  const float colW = static_cast<float>(getWidth()) / kGroupCount;
  for (int gi = 0; gi < kGroupCount; ++gi) {
    g.setColour(groupColour(gi).withAlpha(0.15f));
    g.fillRect(static_cast<int>(colW * gi), ctrlY,
               static_cast<int>(colW), kCtrlH);
  }

  // Shared memory status
  g.setFont(9.f);
  juce::String status = "shm: ";
  for (int gi = 0; gi < kGroupCount; ++gi) {
    PluginProcessor::GroupSnapshot snap;
    const bool active = svProcessor.readGroupSnapshot(gi, snap) && snap.enabled;
    status += active ? "1" : "0";
  }
  const int grpIdx = juce::jlimit(0, 5,
      svProcessor.getParameterRefs().voiceGroup.getIndex());
  const char* grpNames[] = {"Soprano","Mezzo","Alto","Tenor","Baritone","Bass"};
  status += "  this: " + juce::String(grpNames[grpIdx]);
  status += isTimerRunning() ? "  [live]" : "  [stopped]";
  g.setColour(juce::Colour(0x88ffffff));
  g.drawText(status, 4, getHeight() - 16, getWidth() - 8, 12,
             juce::Justification::left);
}

// =========================================================================
// resized
// =========================================================================
void PluginEditor::resized() {
  const int W = getWidth();

  // Tab bar
  surroundTab.setBounds(8, 4, 140, kTabBarH - 8);
  spectrumTab.setBounds(154, 4, 160, kTabBarH - 8);

  // Visualizer area
  const auto viewBounds = juce::Rectangle<int>(0, kTabBarH, W, kViewH);
  surroundView.setBounds(viewBounds);
  spectrumView.setBounds(viewBounds);

  // Instance selector bar — sits above the group columns
  const int ctrlY  = kTabBarH + kViewH + 8;
  instanceLabel   .setBounds(4,   ctrlY,     90, 18);
  instanceSelector.setBounds(96,  ctrlY,     140, 18);

  // Per-group status columns
  const int colsY  = ctrlY + 26;
  const int colW   = W / kGroupCount;

  for (int gi = 0; gi < kGroupCount; ++gi) {
    auto& gc = groupControls[gi];
    const int x = colW * gi + 4;
    const int w = colW - 8;

    gc.groupSelector.setBounds(x, colsY,      w, 16);
    gc.enableButton .setBounds(x, colsY + 20, w, 16);
    gc.trimSlider   .setBounds(x, colsY + 40, w, 22);
    gc.trimLabel    .setBounds(x, colsY + 64, w, 14);
  }
}

}  // namespace surround_vis
