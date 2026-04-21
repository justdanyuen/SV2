#include "surround_plugin.h"

namespace surround_vis {

SurroundVisualizerComponent::SurroundVisualizerComponent(PluginProcessor& p)
    : svProcessor(p) {
  for (int i = 0; i < 30; ++i)
    generateTestData();
}



// =========================================================================
// Timer — pull snapshots from shared memory, fall back to test data
// =========================================================================
void SurroundVisualizerComponent::tick() {
  // Only use real data if the processor is actually running audio.
  // This prevents stale shared memory from blocking test mode.
  const bool audioRunning = svProcessor.isAudioRunning();
  bool anyReal = false;

  if (audioRunning) {
    for (int gi = 0; gi < kGroupCount; ++gi) {
      PluginProcessor::GroupSnapshot snap;
      if (svProcessor.readGroupSnapshot(gi, snap) && snap.enabled) {
        anyReal = true;
        auto& g = groups[gi];
        g.enabled = true;
        g.colorId = snap.colorId;
        // Smooth raw RMS values to avoid choppy polar curve movement.
        // 0.15f gives ~200ms settling time at 30fps which feels natural.
        const float rmsSmooth = 0.15f;
        g.dirRms[0] += (snap.rms[0] - g.dirRms[0]) * rmsSmooth;
        g.dirRms[1] += (snap.rms[1] - g.dirRms[1]) * rmsSmooth;
        g.dirRms[2] += (snap.rms[2] - g.dirRms[2]) * rmsSmooth;
        g.dirRms[3] += (snap.rms[4] - g.dirRms[3]) * rmsSmooth;
        g.dirRms[4] += (snap.rms[5] - g.dirRms[4]) * rmsSmooth;
        g.lfeRms     = snap.rms[3];
        g.lfeSmooth += (g.lfeRms - g.lfeSmooth) * 0.12f;
      } else {
        // Slot stale — decay RMS toward zero so curve fades out cleanly.
        auto& g = groups[gi];
        bool anyNonZero = false;
        for (int ch = 0; ch < 5; ++ch) {
          g.dirRms[ch] *= 0.85f;
          if (g.dirRms[ch] > 0.0001f) anyNonZero = true;
        }
        g.lfeRms    *= 0.85f;
        g.lfeSmooth *= 0.85f;
        if (!anyNonZero) g.enabled = false;
      }
    }
  }

  // Always use test data when no real audio slots are active.
  // generateTestData() sets enabled=true for all groups, so it
  // must run AFTER the real-data loop to avoid being overwritten.
  // Increment testPhase every tick — wrap to avoid float precision loss.
  testPhase += 0.05f;
  if (testPhase > juce::MathConstants<float>::twoPi * 100.f)
    testPhase -= juce::MathConstants<float>::twoPi * 100.f;

  if (!anyReal) {
    useTestData = true;
    generateTestData();
  } else {
    useTestData = false;
  }

}

// =========================================================================
// Test data generator
// =========================================================================
void SurroundVisualizerComponent::generateTestData() {
  // Base levels per group per directional channel: L, R, C, Ls, Rs
  const float baseRms[kGroupCount][5] = {
    {.82f,.72f,.08f,.28f,.38f},  // Soprano
    {.52f,.62f,.12f,.48f,.44f},  // Mezzo
    {.38f,.32f,.18f,.62f,.58f},  // Alto
    {.28f,.33f,.22f,.52f,.42f},  // Tenor
    {.22f,.18f,.28f,.32f,.28f},  // Baritone
    {.14f,.14f,.32f,.18f,.22f},  // Bass
  };
  const float baseLfe[kGroupCount] = {.08f,.18f,.12f,.22f,.28f,.45f};

  for (int gi = 0; gi < kGroupCount; ++gi) {
    auto& g   = groups[gi];
    g.enabled = true;
    g.colorId = gi;

    // Primary slow breath — each group breathes at a slightly different rate
    const float breath = 0.30f * std::sin(testPhase * 0.6f + gi * 1.1f);

    // Secondary faster shimmer per channel for spatial movement
    for (int ch = 0; ch < 5; ++ch) {
      const float shimmer = 0.15f * std::sin(testPhase * 1.8f + gi * 1.3f + ch * 0.9f);
      g.dirRms[ch] = juce::jlimit(0.05f, 1.f, baseRms[gi][ch] + breath + shimmer);
    }

    // LFE pulses slower and more dramatically
    const float lfePulse = 0.20f * std::sin(testPhase * 0.4f + gi * 0.7f);
    g.lfeRms = juce::jlimit(0.05f, 1.f, baseLfe[gi] + lfePulse);
    if (g.lfeSmooth < 0.001f)
      g.lfeSmooth = g.lfeRms;
    else
      g.lfeSmooth += (g.lfeRms - g.lfeSmooth) * 0.12f;
  }
}

// =========================================================================
// Paint
// =========================================================================
void SurroundVisualizerComponent::paint(juce::Graphics& g) {
  // When called directly from editor paint(), getWidth/getHeight may
  // return 0 if the component is hidden. Use clip bounds instead.
  const auto clip = g.getClipBounds();
  const float W = clip.getWidth()  > 0 ? static_cast<float>(clip.getWidth())
                                       : static_cast<float>(getWidth());
  const float H = clip.getHeight() > 0 ? static_cast<float>(clip.getHeight())
                                       : static_cast<float>(getHeight());

  g.fillAll(juce::Colour(0xff0e0e12));

  if (W < 10.f || H < 10.f) return;

  // In test mode, compute animation values directly from wall clock.
  // This guarantees smooth animation regardless of timer state.
  if (useTestData) {
    const float t = static_cast<float>(
        juce::Time::getMillisecondCounterHiRes() * 0.001);

    const float baseLfe[6] = {.08f,.18f,.12f,.22f,.28f,.45f};
    const float baseDir[6][5] = {
      {.82f,.72f,.08f,.28f,.38f},
      {.52f,.62f,.12f,.48f,.44f},
      {.38f,.32f,.18f,.62f,.58f},
      {.28f,.33f,.22f,.52f,.42f},
      {.22f,.18f,.28f,.32f,.28f},
      {.14f,.14f,.32f,.18f,.22f},
    };

    for (int gi = 0; gi < kGroupCount; ++gi) {
      auto& grp = groups[gi];
      const float breath = 0.28f * std::sin(t * 0.7f + gi * 1.1f);
      for (int ch = 0; ch < 5; ++ch) {
        const float sh = 0.12f * std::sin(t * 2.1f + gi * 1.3f + ch * 0.9f);
        grp.dirRms[ch] = juce::jlimit(0.05f, 1.f, baseDir[gi][ch] + breath + sh);
      }
      const float lp = 0.18f * std::sin(t * 0.4f + gi * 0.7f);
      grp.lfeRms    = juce::jlimit(0.05f, 1.f, baseLfe[gi] + lp);
      grp.lfeSmooth = grp.lfeSmooth * 0.88f + grp.lfeRms * 0.12f;
    }

  }

  const float cx = W * 0.5f;
  const float cy = H * 0.5f - 10.f;
  const float maxR = std::min(W, H) * 0.36f;

  drawGrid       (g, cx, cy, maxR);
  drawSpeakers   (g, cx, cy, maxR);
  drawLfeZone    (g, cx, cy, maxR);
  drawLfeArcs    (g, cx, cy, maxR);
  drawPolarCurves(g, cx, cy, maxR);
  drawListener   (g, cx, cy);

  if (useTestData) {
    g.setColour(juce::Colour(0x88ffffff));
    g.setFont(juce::Font(juce::FontOptions().withHeight(10.f)));
    g.drawText("test mode — no audio", 8, static_cast<int>(H) - 18,
               200, 14, juce::Justification::left);
  }

  // Schedule next repaint via async message to keep animation running.
  // This is the only reliable way to self-animate a child component
  // inside a plugin editor on macOS.

}

// =========================================================================
// drawInto — called from editor paint() with explicit bounds
// =========================================================================
void SurroundVisualizerComponent::drawInto(juce::Graphics& g,
                                            juce::Rectangle<int> bounds,
                                            uint8_t enabledMask) {
  const float W = static_cast<float>(bounds.getWidth());
  const float H = static_cast<float>(bounds.getHeight());

  if (W < 10.f || H < 10.f) return;

  juce::Graphics::ScopedSaveState state(g);
  g.setOrigin(bounds.getTopLeft());

  g.fillAll(juce::Colour(0xff0e0e12));

  // Wall-clock animation for test mode
  if (useTestData) {
    const float t = static_cast<float>(
        juce::Time::getMillisecondCounterHiRes() * 0.001);
    const float baseLfe[6] = {.08f,.18f,.12f,.22f,.28f,.45f};
    const float baseDir[6][5] = {
      {.82f,.72f,.08f,.28f,.38f},
      {.52f,.62f,.12f,.48f,.44f},
      {.38f,.32f,.18f,.62f,.58f},
      {.28f,.33f,.22f,.52f,.42f},
      {.22f,.18f,.28f,.32f,.28f},
      {.14f,.14f,.32f,.18f,.22f},
    };
    for (int gi = 0; gi < kGroupCount; ++gi) {
      auto& grp = groups[gi];
      const float breath = 0.28f * std::sin(t * 0.7f + gi * 1.1f);
      for (int ch = 0; ch < 5; ++ch) {
        const float sh = 0.12f * std::sin(t * 2.1f + gi * 1.3f + ch * 0.9f);
        grp.dirRms[ch] = juce::jlimit(0.05f, 1.f, baseDir[gi][ch] + breath + sh);
      }
      const float lp = 0.18f * std::sin(t * 0.4f + gi * 0.7f);
      grp.lfeRms    = juce::jlimit(0.05f, 1.f, baseLfe[gi] + lp);
      grp.lfeSmooth = grp.lfeSmooth * 0.88f + grp.lfeRms * 0.12f;
    }
  }

  const float cx   = W * 0.5f;
  const float cy   = H * 0.5f - 10.f;
  const float maxR = std::min(W, H) * 0.36f;

  // Apply visibility mask — temporarily disable hidden groups.
  bool savedEnabled[kGroupCount];
  for (int gi = 0; gi < kGroupCount; ++gi) {
    savedEnabled[gi]  = groups[static_cast<size_t>(gi)].enabled;
    if (!(enabledMask & (1u << gi)))
      groups[static_cast<size_t>(gi)].enabled = false;
  }

  drawGrid       (g, cx, cy, maxR);
  drawSpeakers   (g, cx, cy, maxR);
  drawLfeZone    (g, cx, cy, maxR);
  drawLfeArcs    (g, cx, cy, maxR);
  drawPolarCurves(g, cx, cy, maxR);
  drawListener   (g, cx, cy);

  // Restore enabled flags
  for (int gi = 0; gi < kGroupCount; ++gi)
    groups[static_cast<size_t>(gi)].enabled = savedEnabled[gi];

  if (useTestData) {
    g.setColour(juce::Colour(0x88ffffff));
    g.setFont(juce::Font(juce::FontOptions().withHeight(10.f)));
    g.drawText("test mode — no audio", 8, static_cast<int>(H) - 18,
               200, 14, juce::Justification::left);
  }
}

// =========================================================================
// Drawing helpers
// =========================================================================
void SurroundVisualizerComponent::drawGrid(juce::Graphics& g,
                                            float cx, float cy,
                                            float maxR) const {
  g.setColour(juce::Colour(0xff1c1c24));
  for (float f : {0.33f, 0.66f, 1.0f}) {
    g.drawEllipse(cx - maxR * f, cy - maxR * f,
                  maxR * f * 2.f, maxR * f * 2.f, 0.5f);
  }
  g.drawLine(cx, cy - maxR - 12.f, cx, cy + maxR + 12.f, 0.5f);
  g.drawLine(cx - maxR - 12.f, cy, cx + maxR + 12.f, cy, 0.5f);
}

void SurroundVisualizerComponent::drawSpeakers(juce::Graphics& g,
                                                float cx, float cy,
                                                float maxR) const {
  static const char* names[] = {"L","R","C","Ls","Rs"};
  for (int i = 0; i < 5; ++i) {
    const float a  = DIR_ANGLES[i];
    const float sx = cx + std::cos(a) * (maxR + 22.f);
    const float sy = cy + std::sin(a) * (maxR + 22.f);

    g.setColour(juce::Colour(0xff2e2e3a));
    g.fillEllipse(sx - 5.f, sy - 5.f, 10.f, 10.f);
    g.setColour(juce::Colour(0xff444444));
    g.drawEllipse(sx - 5.f, sy - 5.f, 10.f, 10.f, 0.5f);

    g.setColour(juce::Colour(0xff666666));
    g.setFont(juce::Font(juce::FontOptions().withHeight(10.f)));
    const bool above = std::sin(a) < -0.2f;
    g.drawText(names[i],
               static_cast<int>(sx) - 14,
               static_cast<int>(above ? sy - 18 : sy + 6),
               28, 14, juce::Justification::centred);
  }
}

void SurroundVisualizerComponent::drawLfeZone(juce::Graphics& g,
                                               float cx, float cy,
                                               float maxR) const {
  // Draw a subtle reference ring showing the LFE radius range
  // Inner ring = minimum LFE radius, outer = maximum
  const float minR = 8.f;
  const float maxLfeR = maxR * 0.55f;

  // Dashed reference circle
  juce::Path ref;
  ref.addEllipse(cx - maxLfeR, cy - maxLfeR, maxLfeR * 2.f, maxLfeR * 2.f);
  float dashLen[] = {2.f, 5.f};
  juce::Path dashed;
  juce::PathStrokeType(0.5f).createDashedStroke(dashed, ref, dashLen, 2);
  g.setColour(juce::Colour(0xff222230));
  g.strokePath(dashed, juce::PathStrokeType(0.5f));

  // LFE label below listener
  g.setColour(juce::Colour(0xff444455));
  g.setFont(juce::Font(juce::FontOptions().withHeight(9.f)));
  g.drawText("LFE", static_cast<int>(cx) - 14,
             static_cast<int>(cy) + 14, 28, 12,
             juce::Justification::centred);

  juce::ignoreUnused(minR, maxR);
}

void SurroundVisualizerComponent::drawLfeArcs(juce::Graphics& g,
                                               float cx, float cy,
                                               float maxR) const {
  // Each enabled group gets a full concentric ring centered on the listener.
  // Ring radius = LFE level mapped to dB scale.
  // Rings are drawn largest-first so smaller rings appear on top.
  const float maxLfeR = maxR * 0.55f;

  int order[kGroupCount];
  int count = 0;
  for (int i = 0; i < kGroupCount; ++i)
    if (groups[static_cast<size_t>(i)].enabled) order[count++] = i;

  // Sort largest radius first (draw behind)
  std::sort(order, order + count, [this](int a, int b) {
    return groups[static_cast<size_t>(a)].lfeSmooth
         > groups[static_cast<size_t>(b)].lfeSmooth;
  });

  for (int oi = 0; oi < count; ++oi) {
    const int   gi  = order[oi];
    const float lv  = groups[static_cast<size_t>(gi)].lfeSmooth;
    if (lv < 0.001f) continue;

    const float lvDb   = juce::Decibels::gainToDecibels(
                             juce::jlimit(0.0001f, 1.f, lv));
    const float lvNorm = juce::jlimit(0.f, 1.f, (lvDb + 60.f) / 60.f);
    const float ringR  = 4.f + lvNorm * maxLfeR;

    const juce::Colour col = groupColour(
        groups[static_cast<size_t>(gi)].colorId);

    // Outer glow
    g.setColour(col.withAlpha(lvNorm * 0.18f));
    g.drawEllipse(cx - ringR, cy - ringR, ringR * 2.f, ringR * 2.f,
                  8.f + lvNorm * 10.f);

    // Main ring stroke
    g.setColour(col.withAlpha(0.35f + lvNorm * 0.55f));
    g.drawEllipse(cx - ringR, cy - ringR, ringR * 2.f, ringR * 2.f,
                  1.5f + lvNorm * 2.f);
  }
}

void SurroundVisualizerComponent::drawPolarCurves(juce::Graphics& g,
                                                   float cx, float cy,
                                                   float maxR) const {
  for (int gi = 0; gi < kGroupCount; ++gi) {
    const auto& grp = groups[gi];
    if (!grp.enabled) continue;

    const juce::Colour col = groupColour(grp.colorId);
    const float lfe        = grp.lfeSmooth;

    // Build raw level array around the circle
    float levels[POLAR_STEPS];
    for (int deg = 0; deg < POLAR_STEPS; ++deg) {
      const float ang = static_cast<float>(deg) * DEG;
      float tw = 0.f, tl = 0.f;
      for (int si = 0; si < 5; ++si) {
        float diff = ang - DIR_ANGLES[si];
        while (diff >  juce::MathConstants<float>::pi) diff -= juce::MathConstants<float>::twoPi;
        while (diff < -juce::MathConstants<float>::pi) diff += juce::MathConstants<float>::twoPi;
        const float w = juce::jmax(0.f, 1.f - std::abs(diff) / 1.4f);
        tw += w;
        tl += w * grp.dirRms[si];
      }
      levels[deg] = tw > 0.f ? tl / tw : 0.f;
    }

    // Smooth with a rolling average
    float smoothed[POLAR_STEPS];
    for (int i = 0; i < POLAR_STEPS; ++i) {
      float s = 0.f;
      for (int k = -SMOOTH_HALF; k <= SMOOTH_HALF; ++k)
        s += levels[(i + k + POLAR_STEPS) % POLAR_STEPS];
      smoothed[i] = s / static_cast<float>(2 * SMOOTH_HALF + 1);
    }

    // Build path
    juce::Path curve;
    for (int deg = 0; deg < POLAR_STEPS; ++deg) {
      const float ang = static_cast<float>(deg) * DEG;
      // Map linear RMS to dB scale (-60dB to 0dB) then to radius.
      // This makes -40dBFS signals use ~33% of the radius,
      // -20dBFS use ~67%, matching how ears perceive loudness.
      const float lin  = juce::jlimit(0.0001f, 1.f, smoothed[deg]);
      const float dB   = juce::Decibels::gainToDecibels(lin);
      const float norm = juce::jlimit(0.f, 1.f, (dB + 60.f) / 60.f);
      const float rad  = 4.f + norm * maxR * 0.92f;
      const float px  = cx + std::cos(ang) * rad;
      const float py  = cy + std::sin(ang) * rad;
      if (deg == 0) curve.startNewSubPath(px, py);
      else          curve.lineTo(px, py);
    }
    curve.closeSubPath();

    // Fill with LFE glow boost
    g.setColour(col.withAlpha(0.06f + lfe * 0.08f));
    g.fillPath(curve);

    // Stroke
    g.setColour(col.withAlpha(0.80f + lfe * 0.18f));
    g.strokePath(curve, juce::PathStrokeType(1.5f));

    // Group label at dominant direction
    int domDeg = 0;
    float domLv = 0.f;
    for (int d = 0; d < POLAR_STEPS; ++d) {
      if (smoothed[d] > domLv) { domLv = smoothed[d]; domDeg = d; }
    }
    if (domLv > 0.05f) {
      const float dang   = static_cast<float>(domDeg) * DEG;
      const float domDb   = juce::Decibels::gainToDecibels(juce::jlimit(0.0001f,1.f,domLv));
      const float domNorm = juce::jlimit(0.f, 1.f, (domDb + 60.f) / 60.f);
      const float labelR  = 4.f + domNorm * maxR * 0.92f + 14.f;
      const float lx     = cx + std::cos(dang) * labelR;
      const float ly     = cy + std::sin(dang) * labelR;
      g.setColour(col.withAlpha(0.85f));
      g.setFont(juce::Font(juce::FontOptions().withHeight(10.f).withStyle("Bold")));
      g.drawText(juce::StringArray{"Soprano","Mezzo","Alto","Tenor","Baritone","Bass"}[gi],
                 static_cast<int>(lx) - 28, static_cast<int>(ly) - 7,
                 56, 14, juce::Justification::centred);
    }
  }
}

void SurroundVisualizerComponent::drawListener(juce::Graphics& g,
                                                float cx, float cy) const {
  g.setColour(juce::Colour(0xffe0e0e0));
  g.fillEllipse(cx - 6.f, cy - 6.f, 12.f, 12.f);
  g.setColour(juce::Colour(0xff666666));
  g.setFont(juce::Font(juce::FontOptions().withHeight(10.f)));
  g.drawText("listener", static_cast<int>(cx) - 28,
             static_cast<int>(cy) + 10, 56, 14,
             juce::Justification::centred);
}

// =========================================================================
// Colour table
// =========================================================================
juce::Colour SurroundVisualizerComponent::groupColour(int id) {
  static const juce::Colour colours[kGroupCount] = {
    juce::Colour(0xff7F77DD),  // Soprano  — purple
    juce::Colour(0xffD4537E),  // Mezzo    — pink
    juce::Colour(0xff378ADD),  // Alto     — blue
    juce::Colour(0xff1D9E75),  // Tenor    — teal
    juce::Colour(0xffBA7517),  // Baritone — amber
    juce::Colour(0xffD85A30),  // Bass     — coral
  };
  if (id >= 0 && id < kGroupCount) return colours[id];
  return juce::Colours::white;
}

float SurroundVisualizerComponent::polarLevel(int gi, float ang) const {
  const auto& grp = groups[gi];
  float tw = 0.f, tl = 0.f;
  for (int si = 0; si < 5; ++si) {
    float diff = ang - DIR_ANGLES[si];
    while (diff >  juce::MathConstants<float>::pi) diff -= juce::MathConstants<float>::twoPi;
    while (diff < -juce::MathConstants<float>::pi) diff += juce::MathConstants<float>::twoPi;
    const float w = juce::jmax(0.f, 1.f - std::abs(diff) / 1.4f);
    tw += w; tl += w * grp.dirRms[si];
  }
  return tw > 0.f ? tl / tw : 0.f;
}

}  // namespace surround_vis
