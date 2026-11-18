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
    reference = state.getRawParameterValue("a4");
    analyzerEnabled = state.getRawParameterValue("analyzerEnabled");
    analyzerSource = state.getRawParameterValue("analyzerSource");
    scale = state.getRawParameterValue("scale");
    autoGain = state.getRawParameterValue("autoGain");
    soloSelected = state.getRawParameterValue("soloSelected");
}
void TunerProcessor::prepareToPlay(double rate, int) {
    artworkClock.reset();
    uiSampleRate.store(rate);
    spectrum.prepare(rate);
    engine.prepare(rate);
    engine.setSettings(readSettings(), true);
}
bool TunerProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    auto main = layout.getMainOutputChannelSet();
    return (main == juce::AudioChannelSet::mono() || main == juce::AudioChannelSet::stereo()) && main == layout.getMainInputChannelSet();
}
ote::Settings TunerProcessor::readSettings() const noexcept {
    auto safe = [](const std::atomic<float>* p, float fallback) { const auto v = p->load(); return std::isfinite(v) ? v : fallback; };
    ote::Settings result;
    result.snap = snap->load() > 0.5f; result.bypass = bypass->load() > 0.5f; result.outputDb = safe(output, 0);
    result.a4 = safe(reference, static_cast<float>(ote::standardPitch));
    result.scale = safe(scale, 100.0f) * 0.01;
    result.autoGain = autoGain->load() > 0.5f;
    result.soloBand = soloSelected->load() > 0.5f ? juce::jlimit(0, 4, activeSelectedBand.load()) : -1;
    for (size_t i = 0; i < values.size(); ++i) {
        const auto& v = values[i]; auto& b = result.bands[i];
        b.enabled = v.enabled->load() > 0.5f;
        b.shape = static_cast<ote::Shape>(juce::jlimit(0, 6, static_cast<int>(safe(v.shape, 0))));
        b.frequency = safe(v.freq, 440); b.gainDb = safe(v.gain, 0) * result.scale; b.q = safe(v.q, 0.7071f);
        b.slope = params::slopeValues[static_cast<size_t>(juce::jlimit(0, 3, static_cast<int>(safe(v.slope, 0))))];
    }
    return result;
}
double TunerProcessor::bandFrequency(int index) const noexcept {
    return ote::effectiveFrequency(values[static_cast<size_t>(index)].freq->load(), snap->load() > 0.5f, uiSampleRate.load(), referencePitch());
}
void TunerProcessor::process(juce::AudioBuffer<float>& buffer, bool forceBypass) {
    juce::ScopedNoDenormals noDenormals;
    auto* playHead = getPlayHead();
    artworkClock.update(playHead ? playHead->getPosition() : juce::Optional<juce::AudioPlayHead::PositionInfo>{},
                        buffer.getNumSamples(), uiSampleRate.load(std::memory_order_relaxed));
    for (int i = getTotalNumInputChannels(); i < buffer.getNumChannels(); ++i) buffer.clear(i, 0, buffer.getNumSamples());
    const bool visualize = analyzerEnabled->load() > 0.5f;
    const bool afterEQ = analyzerSource->load() > 0.5f;
    if (!afterEQ) spectrum.push(buffer, visualize, false);
    auto settings = readSettings(); settings.bypass = settings.bypass || forceBypass;
    engine.setSettings(settings);
    engine.process(buffer.getArrayOfWritePointers(), std::min(2, buffer.getNumChannels()), buffer.getNumSamples());
    if (afterEQ) spectrum.push(buffer, visualize, true);
    float level = 0;
    for (int i = 0; i < buffer.getNumChannels(); ++i) level = std::max(level, buffer.getMagnitude(i, 0, buffer.getNumSamples()));
    peak.store(level);
}
void TunerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { process(buffer, false); }
void TunerProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { process(buffer, true); }
void TunerProcessor::getStateInformation(juce::MemoryBlock& dest) { if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, dest); }
void TunerProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size); xml && xml->hasTagName(state.state.getType())) {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (!tree.isValid()) return;
        const std::pair<const char*, float> defaults[] {
            {"analyzerEnabled", 1.0f}, {"analyzerStyle", 0.0f}, {"analyzerSource", 0.0f},
            {"analyzerFreeze", 0.0f}, {"analyzerRelease", 1.0f}, {"analyzerTilt", 2.0f},
            {"scale", 100.0f}, {"autoGain", 0.0f}, {"soloSelected", 0.0f}
        };
        for (const auto& [id, def] : defaults) {
            if (!tree.getChildWithProperty("id", id).isValid()) {
                juce::ValueTree child("PARAM");
                child.setProperty("id", id, nullptr);
                child.setProperty("value", def, nullptr);
                tree.appendChild(child, nullptr);
            }
        }
        state.replaceState(tree);
    }
}
juce::AudioProcessorParameter* TunerProcessor::getBypassParameter() const { return state.getParameter("bypass"); }
void TunerProcessor::setParameter(const juce::String& id, float value) {
    if (auto* parameter = state.getParameter(id)) {
        parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(value)); parameter->endChangeGesture();
    }
}
juce::AudioProcessorEditor* TunerProcessor::createEditor() { return new TunerEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TunerProcessor(); }
