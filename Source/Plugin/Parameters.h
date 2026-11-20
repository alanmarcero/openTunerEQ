#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Equalizer.h"
namespace params {
using State = juce::AudioProcessorValueTreeState;
juce::String bandId(int band, const char* parameter);
State::ParameterLayout layout();
juce::String frequencyText(double hz);
juce::String decibelText(double decibels);
inline const juce::StringArray shapes {"Bell", "Low shelf", "High shelf", "High pass", "Low pass", "Notch", "Band pass"};
inline const juce::StringArray placements {"Stereo", "Left", "Right", "Mid", "Side"};
inline const juce::StringArray slopes {"12 dB/oct / Soft", "24 dB/oct", "48 dB/oct", "96 dB/oct / Hard"};
inline constexpr std::array<int, 4> slopeValues {12, 24, 48, 96};
inline const juce::StringArray qModes {"Symmetric Q", "Constant Q", "Proportional Q"};
}
