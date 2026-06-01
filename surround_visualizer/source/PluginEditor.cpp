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
  if (isVisible() && !isTimerRunning()) {
    startTimerHz(60);
    // Poll aggressively for first 3 seconds to catch late-initializing
    // satellite instances during DAW session load.
    startupTicksRemaining = 60 * 3;  // 3 seconds at 60fps
  }
}

PluginEditor::~PluginEditor() {
  stopTimer();
}

void PluginEditor::timerCallback() {
  surroundView.tick();
  spectrumView.tick();

  // During startup, force more aggressive repaints to catch
  // late-initializing satellite instances.
  if (startupTicksRemaining > 0)
    --startupTicksRemaining;

  repaint();
}

// =========================================================================
// buildControls
// =========================================================================
void PluginEditor::buildControls() {
  // Instance selector — sets which voice group THIS instance writes to
  instanceSelector.addItem("-- Select Group --", 1);
  instanceSelector.addItem("Soprano",   2);
  instanceSelector.addItem("Mezzo",     3);
  instanceSelector.addItem("Alto",      4);
  instanceSelector.addItem("Tenor",     5);
  instanceSelector.addItem("Baritone",  6);
  instanceSelector.addItem("Bass",      7);
  const int currentGroup = svProcessor.getParameterRefs().voiceGroup.getIndex();
  instanceSelector.setSelectedId(currentGroup + 1, juce::dontSendNotification);
  instanceSelector.setColour(juce::ComboBox::backgroundColourId,
                             juce::Colour(0xff2a2a35));
  instanceSelector.setColour(juce::ComboBox::textColourId,
                             juce::Colours::white);
  instanceSelector.setColour(juce::ComboBox::outlineColourId,
                             juce::Colour(0xff666666));
  instanceSelector.onChange = [this] {
    const int selId    = instanceSelector.getSelectedId();
    const int paramIdx = selId - 1;  // 0=placeholder, 1=Soprano ... 6=Bass
    svProcessor.getParameterRefs().voiceGroup.setValueNotifyingHost(
        svProcessor.getParameterRefs().voiceGroup.convertTo0to1(paramIdx));
    if (selId > 1) {
      const int groupIdx = selId - 2;  // 0=Soprano ... 5=Bass for colour
      const juce::Colour col = groupColour(groupIdx);
      instanceSelector.setColour(juce::ComboBox::textColourId, col);
      instanceSelector.setColour(juce::ComboBox::outlineColourId, col.withAlpha(0.6f));
      instanceSelector.setColour(juce::ComboBox::arrowColourId, col);
    } else {
      instanceSelector.setColour(juce::ComboBox::textColourId,  juce::Colour(0xff888888));
      instanceSelector.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff444444));
      instanceSelector.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff888888));
    }
    repaint();
  };
  // Set initial color
  {
    const int cur = svProcessor.getParameterRefs().voiceGroup.getIndex();
    if (cur > 0) {
      const juce::Colour col = groupColour(cur - 1);
      instanceSelector.setColour(juce::ComboBox::textColourId, col);
      instanceSelector.setColour(juce::ComboBox::outlineColourId, col.withAlpha(0.6f));
      instanceSelector.setColour(juce::ComboBox::arrowColourId, col);
    }
  }
  addAndMakeVisible(instanceSelector);

  instanceLabel.setText("This instance:", juce::dontSendNotification);
  instanceLabel.setFont(juce::Font(juce::FontOptions().withHeight(10.f)));
  instanceLabel.setColour(juce::Label::textColourId,
                          juce::Colour(0xff888888));
  addAndMakeVisible(instanceLabel);

  // Show All — enables all groups and clears solos
  showAllButton.setColour(juce::TextButton::buttonColourId,
                          juce::Colour(0xff2a2a35));
  showAllButton.setColour(juce::TextButton::textColourOffId,
                          juce::Colour(0xff888888));
  showAllButton.onClick = [this] {
    soloMask    = 0x00;
    enabledMask = 0x3F;
    for (int i = 0; i < kGroupCount; ++i) {
      groupControls[static_cast<size_t>(i)].enableButton
          .setToggleState(true,  juce::dontSendNotification);
      groupControls[static_cast<size_t>(i)].soloButton
          .setToggleState(false, juce::dontSendNotification);
    }
    repaint();
  };
  addAndMakeVisible(showAllButton);

  // Hide All — disables all groups and clears solos
  hideAllButton.setColour(juce::TextButton::buttonColourId,
                          juce::Colour(0xff2a2a35));
  hideAllButton.setColour(juce::TextButton::textColourOffId,
                          juce::Colour(0xff888888));
  hideAllButton.onClick = [this] {
    soloMask    = 0x00;
    enabledMask = 0x00;
    for (int i = 0; i < kGroupCount; ++i) {
      groupControls[static_cast<size_t>(i)].enableButton
          .setToggleState(false, juce::dontSendNotification);
      groupControls[static_cast<size_t>(i)].soloButton
          .setToggleState(false, juce::dontSendNotification);
    }
    repaint();
  };
  addAndMakeVisible(hideAllButton);


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

    // enableButton — TextButton acting as toggle
    gc.enableButton.setName("show_" + juce::String(gi));
    gc.enableButton.setClickingTogglesState(true);
    gc.enableButton.setToggleState(true, juce::dontSendNotification);
    gc.enableButton.setColour(juce::TextButton::buttonColourId,
                              juce::Colour(0xff2a2a35));
    gc.enableButton.setColour(juce::TextButton::buttonOnColourId,
                              col.withAlpha(0.35f));
    gc.enableButton.setColour(juce::TextButton::textColourOffId,
                              col.withAlpha(0.5f));
    gc.enableButton.setColour(juce::TextButton::textColourOnId, col);
    gc.enableButton.onClick = [this, gi] {
      const bool on = groupControls[static_cast<size_t>(gi)].enableButton
                          .getToggleState();
      const uint8_t bit = static_cast<uint8_t>(1u << static_cast<unsigned>(gi));
      if (on)
        enabledMask = static_cast<uint8_t>(enabledMask | bit);
      else
        enabledMask = static_cast<uint8_t>(enabledMask & ~bit);
      repaint();
    };
    addAndMakeVisible(gc.enableButton);

    // Solo button — additive toggle
    gc.soloButton.setColour(juce::TextButton::buttonColourId,
                            juce::Colour(0xff2a2a35));
    gc.soloButton.setColour(juce::TextButton::textColourOffId,
                            col.withAlpha(0.7f));
    gc.soloButton.setButtonText("S");
    gc.soloButton.setName("solo_" + juce::String(gi));
    gc.soloButton.setClickingTogglesState(true);
    gc.soloButton.setColour(juce::TextButton::buttonOnColourId,
                            col.withAlpha(0.4f));
    gc.soloButton.onClick = [this, gi] {
      const bool nowSoloed = groupControls[static_cast<size_t>(gi)].soloButton
                                 .getToggleState();
      const uint8_t bit = static_cast<uint8_t>(1u << static_cast<unsigned>(gi));
      if (nowSoloed)
        soloMask = static_cast<uint8_t>(soloMask | bit);
      else
        soloMask = static_cast<uint8_t>(soloMask & ~bit);
      repaint();
    };
    addAndMakeVisible(gc.soloButton);


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

  // Title bar
  g.setColour(juce::Colour(0xff1e1e26));
  g.fillRect(0, 0, getWidth(), kTabBarH);

  // Title — left aligned, inline with tab buttons
  // SV2: large, bold, Avenir/Helvetica Neue for clean modern look
  // Full name: lighter weight, slightly smaller, same family
  const int th = kTabBarH;

  g.setFont(juce::Font(juce::FontOptions()
      .withName("Futura")
      .withHeight(28.f)
      .withStyle("Bold")));
  g.setColour(juce::Colour(0xffffffff));
  g.drawText("SV2", 10, 0, 66, th, juce::Justification::centredLeft);

  g.setFont(juce::Font(juce::FontOptions()
      .withName("Futura")
      .withHeight(18.f)
      .withStyle("Medium")));
  g.setColour(juce::Colour(0xff3a3a55));
  g.drawText("|", 78, 0, 14, th, juce::Justification::centred);

  g.setFont(juce::Font(juce::FontOptions()
      .withName("Futura")
      .withHeight(18.f)
      .withStyle("Medium")));
  g.setColour(juce::Colour(0xff8888bb));
  g.drawText("Surround Vocal Spectrum Visualizer",
             94, 0, 380, th, juce::Justification::centredLeft);

  // Separator below full tab bar
  g.setColour(juce::Colour(0xff2a2a35));
  g.drawHorizontalLine(kTabBarH, 0.f, static_cast<float>(getWidth()));

  // Draw visualizer background from editor — guarantees full coverage
  const auto viewBounds = juce::Rectangle<int>(0, kTabBarH, getWidth(), kViewH);
  g.setColour(juce::Colour(0xff0e0e12));
  g.fillRect(viewBounds);

  // When solos active, only show soloed groups.
  // When no solos, use the normal show/hide mask.
  const uint8_t effectiveMask = (soloMask != 0) ? soloMask : enabledMask;

  if (activeTab == 0)
    surroundView.drawInto(g, viewBounds, effectiveMask);
  else
    spectrumView.drawInto(g, viewBounds, effectiveMask);

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

  // Debug status — shows both write side and read side for Alto (slot 2)
  {
    const int   wGroup = svProcessor.dbgLastGroupId.load(std::memory_order_relaxed);
    const bool  wEn    = svProcessor.dbgLastEnabled.load(std::memory_order_relaxed);
    const int   wCnt   = svProcessor.dbgWriteCount .load(std::memory_order_relaxed);
    const bool  rValid = svProcessor.dbgReadValid  .load(std::memory_order_relaxed);
    const bool  rEn    = svProcessor.dbgReadEnabled.load(std::memory_order_relaxed);
    static const char* gn[] = {"So","Me","Al","Te","Ba","Bs"};
    const juce::String txt =
        juce::String("W:") +
        (wGroup >= 0 && wGroup < 6 ? gn[wGroup] : "?") +
        " idx:" + juce::String(wGroup) +
        " en:" + (wEn ? "Y" : "N") +
        " cnt:" + juce::String(wCnt % 10000) +
        "  | R[2] valid:" + (rValid ? "Y" : "N") +
        " en:" + (rEn ? "Y" : "N");
    g.setFont(juce::Font(juce::FontOptions().withHeight(9.f)));
    g.setColour(juce::Colour(0xff888888));
    g.drawText(txt, 4, getHeight() - 14, getWidth() - 8, 12,
               juce::Justification::centredLeft);
  }
}

// =========================================================================
// resized
// =========================================================================
void PluginEditor::resized() {
  const int W = getWidth();

  // Tab bar
  // Tabs right-aligned in header bar
  // Tabs: fixed height with equal top/bottom margin inside the bar
  const int tabH   = 30;
  const int tabY   = (kTabBarH - tabH) / 2;
  const int tabR   = W - 8;
  spectrumTab .setBounds(tabR - 168, tabY, 164, tabH);
  surroundTab .setBounds(tabR - 168 - 152, tabY, 148, tabH);

  // Visualizer area
  const auto viewBounds = juce::Rectangle<int>(0, kTabBarH, W, kViewH);
  surroundView.setBounds(viewBounds);
  spectrumView.setBounds(viewBounds);

  // Instance selector bar
  const int ctrlY = kTabBarH + kViewH + 8;
  instanceLabel   .setBounds(6,       ctrlY + 2, 88,  16);
  instanceSelector.setBounds(96,      ctrlY,     160, 22);
  showAllButton   .setBounds(W - 170, ctrlY,     80,  22);
  hideAllButton   .setBounds(W - 86,  ctrlY,     80,  22);

  // Per-group columns
  const int colsY = ctrlY + 30;
  const int colW  = W / kGroupCount;

  for (int gi = 0; gi < kGroupCount; ++gi) {
    auto& gc   = groupControls[static_cast<size_t>(gi)];
    const int x    = colW * gi + 5;
    const int w    = colW - 10;
    const int half = (w - 4) / 2;

    gc.groupSelector.setBounds(x,            colsY,      w,    22);
    gc.enableButton .setBounds(x,            colsY + 26, half, 26);
    gc.soloButton   .setBounds(x + half + 4, colsY + 26, half, 26);
  }
}

}  // namespace surround_vis
