#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Notes.h"
TunerProcessor::TunerProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "openTunerEQ", params::layout()) {
    for (int i = 0; i < 5; ++i) {
        auto get = [&](const char* key) { return state.getRawParameterValue(params::bandId(i, key)); };
        values[static_cast<size_t>(i)] = {get("enabled"), get("shape"), get("freq"), get("gain"), get("q"), get("slope")};
    }
    snap = state.getRawParameterValue("snap"); flats = state.getRawParameterValue("flats");
    bypass = state.getRawParameterValue("bypass"); output = state.getRawParameterValue("output");
}
void TunerProcessor::prepareToPlay(double rate, int) { uiSampleRate.store(rate); engine.prepare(rate); engine.setSettings(readSettings(), true); }
bool TunerProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    auto main = layout.getMainOutputChannelSet();
    return (main == juce::AudioChannelSet::mono() || main == juce::AudioChannelSet::stereo()) && main == layout.getMainInputChannelSet();
}
ote::Settings TunerProcessor::readSettings() const noexcept {
    auto safe = [](const std::atomic<float>* p, float fallback) { const auto v = p->load(); return std::isfinite(v) ? v : fallback; };
    ote::Settings result;
    result.snap = snap->load() > 0.5f; result.bypass = bypass->load() > 0.5f; result.outputDb = safe(output, 0);
    for (size_t i = 0; i < values.size(); ++i) {
        const auto& v = values[i]; auto& b = result.bands[i];
        b.enabled = v.enabled->load() > 0.5f;
        b.shape = static_cast<ote::Shape>(juce::jlimit(0, 6, static_cast<int>(safe(v.shape, 0))));
        b.frequency = safe(v.freq, 440); b.gainDb = safe(v.gain, 0); b.q = safe(v.q, 0.7071f);
        b.slope = params::slopeValues[static_cast<size_t>(juce::jlimit(0, 3, static_cast<int>(safe(v.slope, 0))))];
    }
    return result;
}
double TunerProcessor::bandFrequency(int index) const noexcept {
    return ote::effectiveFrequency(values[static_cast<size_t>(index)].freq->load(), snap->load() > 0.5f, uiSampleRate.load());
}
void TunerProcessor::process(juce::AudioBuffer<float>& buffer, bool forceBypass) {
    juce::ScopedNoDenormals noDenormals;
    for (int i = getTotalNumInputChannels(); i < buffer.getNumChannels(); ++i) buffer.clear(i, 0, buffer.getNumSamples());
    auto settings = readSettings(); settings.bypass = settings.bypass || forceBypass;
    engine.setSettings(settings);
    engine.process(buffer.getArrayOfWritePointers(), std::min(2, buffer.getNumChannels()), buffer.getNumSamples());
    float level = 0;
    for (int i = 0; i < buffer.getNumChannels(); ++i) level = std::max(level, buffer.getMagnitude(i, 0, buffer.getNumSamples()));
    peak.store(level);
}
void TunerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { process(buffer, false); }
void TunerProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { process(buffer, true); }
void TunerProcessor::getStateInformation(juce::MemoryBlock& dest) { if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, dest); }
void TunerProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size); xml && xml->hasTagName(state.state.getType()))
        state.replaceState(juce::ValueTree::fromXml(*xml));
}
juce::AudioProcessorParameter* TunerProcessor::getBypassParameter() const { return state.getParameter("bypass"); }
void TunerProcessor::setParameter(const juce::String& id, float value) {
    if (auto* parameter = state.getParameter(id)) {
        parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(value)); parameter->endChangeGesture();
    }
}
juce::AudioProcessorEditor* TunerProcessor::createEditor() { return new TunerEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TunerProcessor(); }
