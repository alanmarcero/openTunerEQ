#include "Parameters.h"
#include "Notes.h"
namespace params {
juce::String bandId(int band, const char* key) { return "band" + juce::String(band) + "." + key; }
State::ParameterLayout layout() {
    State::ParameterLayout result;
    const std::array<int, 5> initialNotes {36, 48, 69, 89, 108};
    for (int i = 0; i < 5; ++i) {
        auto id = [i](const char* key) { return juce::ParameterID(bandId(i, key), 1); };
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
    }
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("snap", 1), "Snap to notes", true));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("flats", 1), "Flat note spelling", false));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("bypass", 1), "Bypass", false));
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("output", 1), "Output gain", juce::NormalisableRange<float>(-24, 12, 0.1f), 0));
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("a4", 1), "A4 reference", juce::NormalisableRange<float>(400, 480, 0.1f),
                                                           static_cast<float>(ote::standardPitch),
                                                           juce::AudioParameterFloatAttributes().withLabel("Hz")));
    return result;
}
}
