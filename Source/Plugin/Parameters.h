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
inline const juce::StringArray analyzerReleases {"Slow 24 dB/s", "Medium 48 dB/s", "Fast 96 dB/s"};
// A 0.1 dB step stores 0 dB as about 4e-7.
inline constexpr double zeroDb = 1.0e-3;
inline bool usesDynamics(const ote::BandSettings& band) { return band.enabled && ote::usesGain(band.shape) && std::abs(band.dynamicDb) > zeroDb; }
}
