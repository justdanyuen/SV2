#pragma once

namespace surround_vis {

enum class VoiceGroup : int {
  Soprano  = 0,
  Mezzo    = 1,
  Alto     = 2,
  Tenor    = 3,
  Baritone = 4,
  Bass     = 5,
  Count    = 6
};

struct Parameters {
  explicit Parameters(juce::AudioProcessor&);

  juce::AudioParameterChoice& voiceGroup;
  juce::AudioParameterBool&   enabled;
  juce::AudioParameterChoice& role;
  juce::AudioParameterFloat&  analysisTrim;

  JUCE_DECLARE_NON_COPYABLE(Parameters)
  JUCE_DECLARE_NON_MOVEABLE(Parameters)
};

}  // namespace surround_vis
