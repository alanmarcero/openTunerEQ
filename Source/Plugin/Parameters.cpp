#include "Parameters.h"
#include "Notes.h"
namespace params {
juce::String bandId(int band, const char* key) { return "band" + juce::String(band) + "." + key; }
State::ParameterLayout layout() {
    State::ParameterLayout result;
    const std::array<int, 5> initialNotes {36, 48, 69, 89, 108};
    for (int i = 0; i < 5; ++i) {
        auto id = [i](const char* key, int version = 1) { return juce::ParameterID(bandId(i, key), version); };
        const auto prefix = "Band " + juce::String(i + 1) + " ";
        result.add(std::make_unique<juce::AudioParameterBool>(id("enabled"), prefix + "Enabled", true));
        result.add(std::make_unique<juce::AudioParameterChoice>(id("shape"), prefix + "Shape", shapes, 0));
        juce::NormalisableRange<float> range(20, 20000);
        range.setSkewForCentre(632.4555f);
        result.add(std::make_unique<juce::AudioParameterFloat>(id("freq"), prefix + "Frequency", range, static_cast<float>(ote::noteFrequency(initialNotes[static_cast<size_t>(i)], ote::standardPitch))));
        result.add(std::make_unique<juce::AudioParameterFloat>(id("gain"), prefix + "Gain", juce::NormalisableRange<float>(-24, 24, 0.1f), 0));
        juce::NormalisableRange<float> qRange(0.1f, 18.0f, 0.01f); qRange.setSkewForCentre(1.0f);
        result.add(std::make_unique<juce::AudioParameterFloat>(id("q"), prefix + "Q", qRange, 0.7071f));
        result.add(std::make_unique<juce::AudioParameterChoice>(id("slope"), prefix + "Slope", slopes, 0));
        result.add(std::make_unique<juce::AudioParameterChoice>(id("placement"), prefix + "Placement", placements, 0));
        result.add(std::make_unique<juce::AudioParameterFloat>(id("dyn", 5), prefix + "Dynamic range", juce::NormalisableRange<float>(-24, 24, 0.1f), 0,
                                                               juce::AudioParameterFloatAttributes().withLabel("dB")));
        result.add(std::make_unique<juce::AudioParameterFloat>(id("drive", 5), prefix + "Drive", juce::NormalisableRange<float>(0, 100, 1), 0,
                                                               juce::AudioParameterFloatAttributes().withLabel("%")));
    }
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("snap", 1), "Snap to notes", true));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("flats", 1), "Flat note spelling", false));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("bypass", 1), "Bypass", false));
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("output", 1), "Output gain", juce::NormalisableRange<float>(-24, 12, 0.1f), 0));
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("a4", 1), "A4 reference", juce::NormalisableRange<float>(400, 480, 0.1f),
                                                           static_cast<float>(ote::standardPitch),
                                                           juce::AudioParameterFloatAttributes().withLabel("Hz")));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("analyzerEnabled", 2), "Live analyzer", true));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("analyzerStyle", 2), "Analyzer style", juce::StringArray {"Spectrum", "Bars", "Spectrogram"}, 0));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("analyzerSource", 3), "Analyzer audio source", juce::StringArray {"Before EQ", "After EQ"}, 0));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("analyzerFreeze", 3), "Freeze spectrum", false));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("analyzerRelease", 3), "Analyzer release", juce::StringArray {"Slow 24 dB/s", "Medium 48 dB/s", "Fast 96 dB/s"}, 1));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("analyzerTilt", 3), "Analyzer tilt", juce::StringArray {"0 dB/oct", "3 dB/oct", "4.5 dB/oct", "6 dB/oct"}, 2));
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("scale", 4), "EQ gain scale", juce::NormalisableRange<float>(0.0f, 200.0f, 1.0f), 100.0f, juce::AudioParameterFloatAttributes().withLabel("%")));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("autoGain", 4), "Auto-gain compensation", false));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("soloSelected", 4), "Solo selected band", false));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("phaseComp", 5), "Phase-align untouched channel", false));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("qMode", 5), "Bell Q law", qModes, 0));
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("dynThreshold", 5), "Dynamic threshold", juce::NormalisableRange<float>(-60, 0, 0.1f), -24,
                                                           juce::AudioParameterFloatAttributes().withLabel("dB")));
    return result;
}
}
