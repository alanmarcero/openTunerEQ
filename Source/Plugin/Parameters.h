#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Equalizer.h"
namespace params {
using State = juce::AudioProcessorValueTreeState;
juce::String bandId(int band, const char* parameter);
State::ParameterLayout layout();
inline const juce::StringArray shapes {"Bell", "Low shelf", "High shelf", "High pass", "Low pass", "Notch", "Band pass"};
inline const juce::StringArray slopes {"12 dB/oct / Soft", "24 dB/oct", "48 dB/oct", "96 dB/oct / Hard"};
inline constexpr std::array<int, 4> slopeValues {12, 24, 48, 96};
}
