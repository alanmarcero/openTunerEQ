#include "Parameters.h"
#include "Notes.h"
namespace params {
juce::String bandId(int band, const char* key) { return "band" + juce::String(band) + "." + key; }
juce::String frequencyText(double hz) {
    if (hz < 999.5) return juce::String(juce::roundToInt(hz)) + " Hz";
    return juce::String(hz / 1000.0, hz < 9995.0 ? 2 : 1) + " kHz";
}
juce::String decibelText(double decibels) {
    const auto tenths = std::round(decibels * 10.0) / 10.0;
    if (tenths > 0) return "+" + juce::String(tenths, 1) + " dB";
    return juce::String(tenths == 0 ? 0.0 : tenths, 1) + " dB";
}
State::ParameterLayout layout() {
    State::ParameterLayout result;
    const auto decibels = juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float value, int) { return decibelText(value); });
    const auto percent = juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float value, int) {
        return juce::String(juce::roundToInt(value)) + "%";
    });
    // "6.8k" and "6.8 kHz" mean 6800 Hz.
    const auto parseHz = [](const juce::String& text) { return text.getFloatValue() * (text.containsIgnoreCase("k") ? 1000.0f : 1.0f); };
    const std::array<int, 5> initialNotes {36, 48, 69, 89, 108};
    for (int i = 0; i < 5; ++i) {
        auto id = [i](const char* key, int version = 1) { return juce::ParameterID(bandId(i, key), version); };
        const auto prefix = "Band " + juce::String(i + 1) + " ";
        result.add(std::make_unique<juce::AudioParameterBool>(id("enabled"), prefix + "Enabled", true));
        result.add(std::make_unique<juce::AudioParameterChoice>(id("shape"), prefix + "Shape", shapes, 0));
        juce::NormalisableRange<float> range(20, 20000);
        range.setSkewForCentre(632.4555f);
        result.add(std::make_unique<juce::AudioParameterFloat>(id("freq"), prefix + "Frequency", range, static_cast<float>(ote::noteFrequency(initialNotes[static_cast<size_t>(i)], ote::standardPitch)),
                                                               juce::AudioParameterFloatAttributes()
                                                                   .withStringFromValueFunction([](float value, int) { return frequencyText(value); })
                                                                   .withValueFromStringFunction(parseHz)));
        result.add(std::make_unique<juce::AudioParameterFloat>(id("gain"), prefix + "Gain", juce::NormalisableRange<float>(-24, 24, 0.1f), 0, decibels));
        juce::NormalisableRange<float> qRange(0.1f, 18.0f, 0.01f); qRange.setSkewForCentre(1.0f);
        result.add(std::make_unique<juce::AudioParameterFloat>(id("q"), prefix + "Q", qRange, 0.7071f));
        result.add(std::make_unique<juce::AudioParameterChoice>(id("slope"), prefix + "Slope", slopes, 0));
        result.add(std::make_unique<juce::AudioParameterChoice>(id("placement"), prefix + "Placement", placements, 0));
        result.add(std::make_unique<juce::AudioParameterFloat>(id("dyn", 5), prefix + "Dynamic range", juce::NormalisableRange<float>(-24, 24, 0.1f), 0, decibels));
        result.add(std::make_unique<juce::AudioParameterFloat>(id("drive", 5), prefix + "Drive", juce::NormalisableRange<float>(0, 100, 1), 0, percent));
    }
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("snap", 1), "Snap to notes", true));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("flats", 1), "Flat note spelling", false));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("bypass", 1), "Bypass", false));
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("output", 1), "Output gain", juce::NormalisableRange<float>(-24, 12, 0.1f), 0, decibels));
    // One decimal shows the 0.1 Hz step.
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("a4", 1), "A4 reference", juce::NormalisableRange<float>(400, 480, 0.1f),
                                                           static_cast<float>(ote::standardPitch),
                                                           juce::AudioParameterFloatAttributes()
                                                               .withStringFromValueFunction([](float value, int) { return juce::String(value, 1) + " Hz"; })
                                                               .withValueFromStringFunction(parseHz)));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("analyzerEnabled", 2), "Analyzer", true));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("analyzerStyle", 2), "Analyzer style", juce::StringArray {"Spectrum", "Bars", "Spectrogram"}, 0));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("analyzerSource", 3), "Analyzer audio source", juce::StringArray {"Before EQ", "After EQ"}, 0));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("analyzerFreeze", 3), "Analyzer freeze", false));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("analyzerRelease", 3), "Analyzer release", analyzerReleases, 1));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("analyzerTilt", 3), "Analyzer tilt", juce::StringArray {"0 dB/oct", "3 dB/oct", "4.5 dB/oct", "6 dB/oct"}, 2));
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("scale", 4), "EQ gain scale", juce::NormalisableRange<float>(0.0f, 200.0f, 1.0f), 100.0f, percent));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("autoGain", 4), "Auto-gain compensation", false));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("soloSelected", 4), "Solo selected band", false));
    result.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("phaseComp", 5), "Phase-align untouched channel", false));
    result.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID("qMode", 5), "Bell Q law", qModes, 0));
    result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("dynThreshold", 5), "Dynamic threshold", juce::NormalisableRange<float>(-60, 0, 0.1f), -24, decibels));
    return result;
}
}
