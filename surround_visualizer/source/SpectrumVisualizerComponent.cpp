#include "surround_plugin.h"

namespace surround_vis {

SpectrumVisualizerComponent::SpectrumVisualizerComponent(PluginProcessor& p)
    : svProcessor(p) {
  for (int i = 0; i < 30; ++i)
    generateTestData();
}

// =========================================================================
// Timer
// =========================================================================
void SpectrumVisualizerComponent::tick() {
  cachedSampleRate = svProcessor.getSampleRateThreadSafe();
  if (cachedSampleRate <= 0.0) cachedSampleRate = 44100.0;

  const bool audioRunning = svProcessor.isAudioRunning();
  bool anyReal = false;
  for (int gi = 0; gi < kGroupCount && audioRunning; ++gi) {
    PluginProcessor::GroupSnapshot snap;
    if (svProcessor.readGroupSnapshot(gi, snap) && snap.enabled) {
      anyReal = true;
      auto& g  = groups[gi];
      g.enabled  = true;
      g.colorId  = snap.colorId;
      g.lfeLevel = snap.rms[3];
      for (int b = 0; b < kFftBinCount; ++b) {
        g.bins[b]        = snap.fft[b];
        // Attack fast (0.4f), decay slow (0.08f) — classic RTA ballistics.
        const float target = snap.fft[b];
        const float rate   = target > g.smoothBins[b] ? 0.40f : 0.08f;
        g.smoothBins[b]   += (target - g.smoothBins[b]) * rate;
      }
    } else {
      // Slot went stale — decay bins to zero so the curve fades out
      // rather than freezing at its last value.
      auto& grp = groups[gi];
      bool anyNonZero = false;
      for (int b = 0; b < kFftBinCount; ++b) {
        grp.smoothBins[b] *= 0.85f;
        if (grp.smoothBins[b] > 0.0001f) anyNonZero = true;
      }
      grp.lfeLevel *= 0.85f;
      if (!anyNonZero) grp.enabled = false;
    }
    // Do NOT set enabled=false here — generateTestData() handles
    // that when no real slots are active.
  }

  testPhase += 0.05f;

  if (!anyReal) {
    useTestData = true;
    generateTestData();
  } else {
    useTestData = false;
  }

}

// =========================================================================
// Test data
// =========================================================================
void SpectrumVisualizerComponent::generateTestData() {
  // Voice group frequency peaks: [fundamental, formant]
  // Based on real choral voice ranges:
  // Soprano  F4-C6  ~350-1050Hz fundamental, formants 2-4kHz
  // Mezzo    A3-A5  ~220-880Hz  fundamental, formants 1.5-3.5kHz
  // Alto     F3-F5  ~175-700Hz  fundamental, formants 1-3kHz
  // Tenor    C3-C5  ~130-520Hz  fundamental, formants 1-2.5kHz
  // Baritone A2-A4  ~110-440Hz  fundamental, formants 0.8-2kHz
  // Bass     E2-E4  ~82-330Hz   fundamental, formants 0.6-1.5kHz
  const float peaks[kGroupCount][2] = {
    {500.f,  3000.f},   // Soprano
    {350.f,  2500.f},   // Mezzo
    {260.f,  2000.f},   // Alto
    {200.f,  1500.f},   // Tenor
    {160.f,  1200.f},   // Baritone
    {100.f,   800.f},   // Bass
  };
  const float amps[kGroupCount] = {.70f,.65f,.60f,.55f,.50f,.60f};
  const float baseLfe[kGroupCount] = {.04f,.12f,.08f,.16f,.22f,.38f};

  for (int gi = 0; gi < kGroupCount; ++gi) {
    auto& g   = groups[gi];
    g.enabled = true;
    g.colorId = gi;

    const float lw = 0.10f * std::sin(testPhase * 1.3f + gi * 0.8f);
    g.lfeLevel = juce::jlimit(0.f, 1.f, baseLfe[gi] + lw);

    for (int b = 0; b < kFftBinCount; ++b) {
      // Map bin to frequency (log scale approximation)
      // Map bin to frequency using same log scale as the display
      const float binHz = static_cast<float>(cachedSampleRate) / (2.f * kFftBinCount);
      const float freq  = juce::jmax(20.f, static_cast<float>(b) * binHz);
      const float p1   = peaks[gi][0], p2 = peaks[gi][1];
      const float amp  = amps[gi];
      const float v1   = amp * std::exp(-std::pow((freq - p1) / (p1 * 0.6f), 2.f));
      const float v2   = amp * 0.45f * std::exp(-std::pow((freq - p2) / (p2 * 0.5f), 2.f));
      const float breath = 0.20f * std::sin(testPhase * 0.5f + gi * 1.1f);
      const float wobble = 0.08f * std::sin(testPhase * 1.5f + gi * 0.9f + b * 0.05f);
      const float raw  = juce::jlimit(0.f, 1.f, (v1 + v2) * (1.f + breath) + wobble);
      g.bins[b] = raw;
      if (g.smoothBins[b] < 0.001f)
        g.smoothBins[b] = raw;
      else
        g.smoothBins[b] += (raw - g.smoothBins[b]) * 0.15f;
    }
  }
}

// =========================================================================
// Paint
// =========================================================================
void SpectrumVisualizerComponent::paint(juce::Graphics& g) {
  const auto clip = g.getClipBounds();
  const auto bounds = (clip.getWidth() > 0 && clip.getHeight() > 0)
      ? clip.toFloat()
      : getLocalBounds().toFloat();
  const float legendH = 20.f;
  const auto plotArea = bounds.withTrimmedBottom(legendH).reduced(40.f, 12.f);

  g.fillAll(juce::Colour(0xff0e0e12));

  drawGrid  (g, plotArea);
  drawCurves(g, plotArea);
  drawLegend(g, bounds.withTop(bounds.getBottom() - legendH));

  if (useTestData) {
    g.setColour(juce::Colour(0x44ffffff));
    g.setFont(juce::Font(juce::FontOptions().withHeight(10.f)));
    g.drawText("test mode — no audio", 8,
               getHeight() - 18, 200, 14,
               juce::Justification::left);
  }


}

// =========================================================================
// drawInto — called from editor paint() with explicit bounds
// =========================================================================
void SpectrumVisualizerComponent::drawInto(juce::Graphics& g,
                                            juce::Rectangle<int> bounds,
                                            uint8_t enabledMask) {
  if (bounds.getWidth() < 10 || bounds.getHeight() < 10) return;

  juce::Graphics::ScopedSaveState state(g);
  g.setOrigin(bounds.getTopLeft());

  const auto fb     = bounds.withZeroOrigin().toFloat();
  const float legendH = 20.f;
  const auto plotArea = fb.withTrimmedBottom(legendH).reduced(40.f, 12.f);

  g.fillAll(juce::Colour(0xff0e0e12));

  // Wall-clock animation for test mode
  if (useTestData) {
    const float t = static_cast<float>(
        juce::Time::getMillisecondCounterHiRes() * 0.001);
    const float peaks[kGroupCount][2] = {
      {500.f,3000.f},{350.f,2500.f},{260.f,2000.f},
      {200.f,1500.f},{160.f,1200.f},{100.f,800.f}
    };
    const float amps[kGroupCount] = {.70f,.65f,.60f,.55f,.50f,.60f};
    for (int gi = 0; gi < kGroupCount; ++gi) {
      auto& grp = groups[gi];
      const float breath = 0.20f * std::sin(t * 0.5f + gi * 1.1f);
      for (int b = 0; b < kFftBinCount; ++b) {
        const float binHz = static_cast<float>(cachedSampleRate) / (2.f * kFftBinCount);
        const float freq  = juce::jmax(20.f, static_cast<float>(b) * binHz);
        const float p1 = peaks[gi][0], p2 = peaks[gi][1], amp = amps[gi];
        const float v1 = amp * std::exp(-std::pow((freq-p1)/(p1*.6f),2.f));
        const float v2 = amp*.45f*std::exp(-std::pow((freq-p2)/(p2*.5f),2.f));
        const float wobble = 0.08f * std::sin(t * 1.5f + gi * 0.9f + b * 0.05f);
        const float raw = juce::jlimit(0.f, 1.f, (v1+v2)*(1.f+breath)+wobble);
        grp.bins[b] = raw;
        if (grp.smoothBins[b] < 0.001f)
          grp.smoothBins[b] = raw;
        else
          grp.smoothBins[b] += (raw - grp.smoothBins[b]) * 0.25f;
      }
    }
  }

  // Apply visibility mask
  bool savedEnabled[kGroupCount];
  for (int gi = 0; gi < kGroupCount; ++gi) {
    savedEnabled[gi] = groups[static_cast<size_t>(gi)].enabled;
    if (!(enabledMask & (1u << gi)))
      groups[static_cast<size_t>(gi)].enabled = false;
  }

  drawGrid  (g, plotArea);
  drawCurves(g, plotArea);
  drawLegend(g, fb.withTop(fb.getBottom() - legendH));

  for (int gi = 0; gi < kGroupCount; ++gi)
    groups[static_cast<size_t>(gi)].enabled = savedEnabled[gi];

  if (useTestData) {
    g.setColour(juce::Colour(0x44ffffff));
    g.setFont(juce::Font(juce::FontOptions().withHeight(10.f)));
    g.drawText("test mode — no audio", 8,
               bounds.getHeight() - 18, 200, 14,
               juce::Justification::left);
  }
}

// =========================================================================
// Drawing helpers
// =========================================================================
void SpectrumVisualizerComponent::drawGrid(juce::Graphics& g,
                                            juce::Rectangle<float> a) const {
  const float freqMarkers[] = {20.f, 50.f, 100.f, 200.f, 500.f,
                                1000.f, 2000.f, 5000.f, 10000.f, 20000.f};
  const char* freqLabels[]  = {"20","50","100","200","500",
                                "1k","2k","5k","10k","20k"};

  g.setColour(juce::Colour(0xff1e1e26));
  g.setFont(juce::Font(juce::FontOptions().withHeight(9.f)));

  // Horizontal amplitude grid lines
  for (float v : {0.25f, 0.5f, 0.75f, 1.0f}) {
    const float y = a.getY() + a.getHeight() * (1.f - v);
    g.drawHorizontalLine(static_cast<int>(y), a.getX(), a.getRight());
    g.setColour(juce::Colour(0xff444444));
    g.drawText(juce::String(static_cast<int>(v * 100)) + "%",
               0, static_cast<int>(y) - 6, 36, 12,
               juce::Justification::right);
    g.setColour(juce::Colour(0xff1e1e26));
  }

  // Vertical frequency grid lines (log spaced)
  const float minFreq = 20.f, maxFreq = 20000.f;
  for (int i = 0; i < 10; ++i) {
    const float freq = freqMarkers[i];
    const float logX = a.getX() + a.getWidth() *
        (std::log10(freq / minFreq) / std::log10(maxFreq / minFreq));
    g.drawVerticalLine(static_cast<int>(logX), a.getY(), a.getBottom());
    g.setColour(juce::Colour(0xff444444));
    g.drawText(freqLabels[i],
               static_cast<int>(logX) - 14,
               static_cast<int>(a.getBottom()) + 2,
               28, 12, juce::Justification::centred);
    g.setColour(juce::Colour(0xff1e1e26));
  }
}

void SpectrumVisualizerComponent::drawCurves(juce::Graphics& g,
                                              juce::Rectangle<float> a) const {
  for (int gi = 0; gi < kGroupCount; ++gi) {
    const auto& grp = groups[gi];
    if (!grp.enabled) continue;

    const juce::Colour col = groupColour(grp.colorId);
    const float lfe        = grp.lfeLevel;

    juce::Path curve;
    for (int b = 0; b < kFftBinCount; ++b) {
      const float x   = binToX(b, a.getX(), a.getWidth(), cachedSampleRate);
      // Map FFT magnitude to dB scale (-80dB to 0dB) for display.
      // FFT values are tiny linear amplitudes — dB mapping makes
      // quiet signals visible at normal listening levels.
      const float lin = juce::jlimit(0.00001f, 1.f, grp.smoothBins[b]);
      const float dB  = juce::Decibels::gainToDecibels(lin);
      const float v   = juce::jlimit(0.f, 1.f, (dB + 80.f) / 80.f);
      const float y   = a.getY() + a.getHeight() * (1.f - v);
      if (b == 0) curve.startNewSubPath(x, y);
      else        curve.lineTo(x, y);
    }

    // Filled area
    juce::Path filled = curve;
    filled.lineTo(a.getRight(), a.getBottom());
    filled.lineTo(a.getX(), a.getBottom());
    filled.closeSubPath();
    g.setColour(col.withAlpha(0.05f + lfe * 0.07f));
    g.fillPath(filled);

    // Curve stroke — brightens with LFE
    g.setColour(col.withAlpha(0.75f + lfe * 0.20f));
    g.strokePath(curve, juce::PathStrokeType(1.5f));
  }
}

void SpectrumVisualizerComponent::drawLegend(juce::Graphics& g,
                                              juce::Rectangle<float> a) const {
  static const char* names[] = {"Soprano","Mezzo","Alto","Tenor","Baritone","Bass"};
  const float itemW = a.getWidth() / static_cast<float>(kGroupCount);
  g.setFont(juce::Font(juce::FontOptions().withHeight(10.f)));
  for (int gi = 0; gi < kGroupCount; ++gi) {
    if (!groups[gi].enabled) continue;
    const float x = a.getX() + itemW * gi;
    const juce::Colour col = groupColour(gi);
    g.setColour(col);
    g.fillRect(x + 4.f, a.getCentreY() - 4.f, 10.f, 8.f);
    g.setColour(juce::Colour(0xff888888));
    g.drawText(names[gi],
               static_cast<int>(x + 18.f),
               static_cast<int>(a.getY()),
               static_cast<int>(itemW - 20.f),
               static_cast<int>(a.getHeight()),
               juce::Justification::centredLeft);
  }
}

// =========================================================================
// Frequency mapping
// =========================================================================
float SpectrumVisualizerComponent::binToX(int bin, float areaX,
                                           float areaW,
                                           double sampleRate) const {
  // FFT bin frequency: bin * (sampleRate / fftSize)
  // fftSize = kFftBinCount * 2 = 2048
  const float binHz   = static_cast<float>(sampleRate) / (2.f * static_cast<float>(kFftBinCount));
  const float freq    = juce::jmax(20.f, static_cast<float>(juce::jmax(1, bin)) * binHz);
  const float minFreq = 20.f, maxFreq = 20000.f;
  const float logFreq = std::log10(freq / minFreq) / std::log10(maxFreq / minFreq);
  return areaX + areaW * juce::jlimit(0.f, 1.f, logFreq);
}

// =========================================================================
// Colour table
// =========================================================================
juce::Colour SpectrumVisualizerComponent::groupColour(int id) {
  static const juce::Colour colours[kGroupCount] = {
    juce::Colour(0xff7F77DD),
    juce::Colour(0xffD4537E),
    juce::Colour(0xff378ADD),
    juce::Colour(0xff1D9E75),
    juce::Colour(0xffBA7517),
    juce::Colour(0xffD85A30),
  };
  if (id >= 0 && id < kGroupCount) return colours[id];
  return juce::Colours::white;
}

}  // namespace surround_vis
