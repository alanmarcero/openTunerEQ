#include "PluginProcessor.h"
#include "FactoryPresets.h"
#include "PluginEditor.h"
#include "Notes.h"
#include <cmath>
namespace {
juce::StringArray factoryNames() {
    juce::StringArray names;
    for (const auto& preset : factoryPresets()) names.add(preset.name);
    return names;
}
}
TunerProcessor::TunerProcessor(juce::File presetFolder)
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "openTunerEQ", params::layout()),
      undoHistory(*this, [this](juce::MemoryBlock& block) { writeParameters(block); },
                  [this](const juce::MemoryBlock& block) { readParameters(block); }),
      abCompare(undoHistory),
      presetStore("openTunerEQ", factoryNames(), std::move(presetFolder)) {
    for (int i = 0; i < 5; ++i) {
        auto get = [&](const char* key) { return state.getRawParameterValue(params::bandId(i, key)); };
        values[static_cast<size_t>(i)] = {get("enabled"), get("shape"), get("freq"), get("gain"), get("q"), get("slope"), get("placement"), get("dyn"), get("drive")};
    }
    snap = state.getRawParameterValue("snap"); flats = state.getRawParameterValue("flats");
    bypass = state.getRawParameterValue("bypass"); output = state.getRawParameterValue("output");
    reference = state.getRawParameterValue("a4");
    analyzerEnabled = state.getRawParameterValue("analyzerEnabled");
    analyzerSource = state.getRawParameterValue("analyzerSource");
    scale = state.getRawParameterValue("scale");
    autoGain = state.getRawParameterValue("autoGain");
    soloSelected = state.getRawParameterValue("soloSelected");
    phaseComp = state.getRawParameterValue("phaseComp");
    qMode = state.getRawParameterValue("qMode");
    dynThreshold = state.getRawParameterValue("dynThreshold");
    undoHistory.onEdit = [this] { preset.edited = true; };
    preset.name = factoryPresets().front().name;
    abCompare.prime();
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
    // The 0.1 dB grid stores 0 dB as about 4e-7. Rounding to the grid makes 0 exact for the engine and the UI.
    auto tenths = [](double value) { return std::round(value * 10.0) / 10.0; };
    ote::Settings result;
    result.snap = snap->load() > 0.5f; result.bypass = bypass->load() > 0.5f; result.outputDb = safe(output, 0);
    result.a4 = safe(reference, static_cast<float>(ote::standardPitch));
    result.scale = safe(scale, 100.0f) * 0.01;
    result.autoGain = autoGain->load() > 0.5f;
    result.phaseCompensation = phaseComp->load() > 0.5f;
    result.dynamicThresholdDb = safe(dynThreshold, -24);
    const auto bellLaw = static_cast<ote::QMode>(juce::jlimit(0, 2, static_cast<int>(std::lround(safe(qMode, 0)))));
    result.soloBand = soloSelected->load() > 0.5f ? juce::jlimit(0, 4, activeSelectedBand.load()) : -1;
    for (size_t i = 0; i < values.size(); ++i) {
        const auto& v = values[i]; auto& b = result.bands[i];
        b.enabled = v.enabled->load() > 0.5f;
        b.shape = static_cast<ote::Shape>(juce::jlimit(0, 6, static_cast<int>(safe(v.shape, 0))));
        b.frequency = safe(v.freq, 440); b.gainDb = tenths(safe(v.gain, 0)) * result.scale; b.q = safe(v.q, 0.7071f);
        b.slope = params::slopeValues[static_cast<size_t>(juce::jlimit(0, 3, static_cast<int>(safe(v.slope, 0))))];
        b.placement = static_cast<ote::Placement>(juce::jlimit(0, 4, static_cast<int>(std::lround(safe(v.placement, 0)))));
        b.qMode = bellLaw;
        b.dynamicDb = tenths(safe(v.dyn, 0)) * result.scale;
        b.drive = juce::jlimit(0.0, 1.0, safe(v.drive, 0) * 0.01);
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
    // The editor reads with exchange(0). Keep the highest value until that read.
    auto raise = [](std::atomic<float>& meter, float value) {
        auto current = meter.load();
        while (value > current && !meter.compare_exchange_weak(current, value)) {}
    };
    for (size_t i = 0; i < bandLevels.size(); ++i) {
        dynamicGains[i].store(static_cast<float>(engine.dynamicGainDb(i)));
        raise(bandLevels[i], static_cast<float>(engine.detectorLevel(i)));
    }
    float level = 0;
    for (int i = 0; i < buffer.getNumChannels(); ++i) level = std::max(level, buffer.getMagnitude(i, 0, buffer.getNumSamples()));
    raise(peak, level);
}
void TunerProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { process(buffer, false); }
void TunerProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { process(buffer, true); }
void TunerProcessor::injectDefaults(juce::ValueTree& tree) const {
    const std::pair<const char*, float> defaults[] {
        {"analyzerEnabled", 1.0f}, {"analyzerStyle", 0.0f}, {"analyzerSource", 0.0f},
        {"analyzerFreeze", 0.0f}, {"analyzerRelease", 1.0f}, {"analyzerTilt", 2.0f},
        {"scale", 100.0f}, {"autoGain", 0.0f}, {"soloSelected", 0.0f},
        {"phaseComp", 0.0f}, {"qMode", 0.0f}, {"dynThreshold", -24.0f}
    };
    auto add = [&](const juce::String& id, float value) {
        if (tree.getChildWithProperty("id", id).isValid()) return;
        juce::ValueTree child("PARAM");
        child.setProperty("id", id, nullptr);
        child.setProperty("value", value, nullptr);
        tree.appendChild(child, nullptr);
    };
    for (const auto& [id, value] : defaults) add(id, value);
    for (int i = 0; i < 5; ++i)
        for (const char* key : {"placement", "dyn", "drive"}) add(params::bandId(i, key), 0.0f);
}
void TunerProcessor::Preset::write(juce::XmlElement& xml) const {
    xml.setAttribute("name", name);
    xml.setAttribute("program", program);
    xml.setAttribute("edited", edited);
}
void TunerProcessor::Preset::read(const juce::XmlElement& xml) {
    name = xml.getStringAttribute("name");
    program = xml.getIntAttribute("program", program);
    edited = xml.getBoolAttribute("edited");
}
// A slot stores the preset name, so A/B and undo restore it.
void TunerProcessor::writeParameters(juce::MemoryBlock& dest) {
    dest.reset();
    auto xml = state.copyState().createXml();
    if (xml == nullptr) return;
    preset.write(*xml->createNewChildElement("preset"));
    copyXmlToBinary(*xml, dest);
}
void TunerProcessor::readParameters(const juce::MemoryBlock& block) {
    auto xml = getXmlFromBinary(block.getData(), static_cast<int>(block.getSize()));
    if (xml == nullptr || !xml->hasTagName(state.state.getType())) return;
    if (auto* extra = xml->getChildByName("suite")) xml->removeChildElement(extra, true);
    if (auto* loaded = xml->getChildByName("preset")) {
        preset.read(*loaded);
        xml->removeChildElement(loaded, true);
    }
    auto tree = juce::ValueTree::fromXml(*xml);
    if (!tree.isValid()) return;
    injectDefaults(tree);
    state.replaceState(tree);
}
void TunerProcessor::getStateInformation(juce::MemoryBlock& dest) {
    abCompare.flushActive();
    if (auto xml = state.copyState().createXml()) {
        auto* extra = xml->createNewChildElement("suite");
        preset.write(*extra);
        extra->setAttribute("editorWidth", editorWidth.load());
        extra->setAttribute("selectedBand", activeSelectedBand.load());
        abCompare.writeXml(*extra);
        copyXmlToBinary(*xml, dest);
    }
}
void TunerProcessor::setStateInformation(const void* data, int size) {
    auto xml = getXmlFromBinary(data, size);
    if (xml == nullptr || !xml->hasTagName(state.state.getType())) return;
    auto* extra = xml->getChildByName("suite");
    const bool hadSuite = extra != nullptr;
    bool slotsOk = false;
    if (hadSuite) {
        preset.read(*extra);
        preset.program = juce::jlimit(0, juce::jmax(0, getNumPrograms() - 1), preset.program);
        editorWidth = extra->getIntAttribute("editorWidth", editorWidth.load());
        activeSelectedBand = juce::jlimit(0, 4, extra->getIntAttribute("selectedBand", 0));
        slotsOk = abCompare.readXml(*extra);
        xml->removeChildElement(extra, true);
    }
    auto tree = juce::ValueTree::fromXml(*xml);
    if (!tree.isValid()) return;
    injectDefaults(tree);
    state.replaceState(tree);
    if (!hadSuite) preset = {};
    if (!hadSuite || !slotsOk) abCompare.prime();
}
int TunerProcessor::getNumPrograms() { return static_cast<int>(factoryPresets().size()); }
const juce::String TunerProcessor::getProgramName(int index) {
    const auto& presets = factoryPresets();
    return juce::isPositiveAndBelow(index, static_cast<int>(presets.size())) ? presets[static_cast<size_t>(index)].name : juce::String();
}
void TunerProcessor::assignParameter(const juce::String& id, float value) {
    if (auto* parameter = state.getParameter(id)) parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
void TunerProcessor::applyFactory(int index) {
    const auto& presets = factoryPresets();
    if (!juce::isPositiveAndBelow(index, static_cast<int>(presets.size()))) return;
    auto reset = [&](const juce::String& id) {
        if (auto* parameter = state.getParameter(id)) parameter->setValueNotifyingHost(parameter->getDefaultValue());
    };
    for (int i = 0; i < 5; ++i)
        for (const char* key : {"enabled", "shape", "freq", "gain", "q", "slope", "placement", "dyn", "drive"}) reset(params::bandId(i, key));
    for (const char* id : {"output", "bypass", "snap", "scale", "autoGain", "soloSelected", "phaseComp", "qMode", "dynThreshold"}) reset(id);
    for (const auto& [id, value] : presets[static_cast<size_t>(index)].values) assignParameter(id, value);
    preset = {presets[static_cast<size_t>(index)].name, index, false};
}
void TunerProcessor::setCurrentProgram(int index) {
    if (!juce::isPositiveAndBelow(index, getNumPrograms())) return;
    auto* messages = juce::MessageManager::getInstanceWithoutCreating();
    if (messages != nullptr && messages->isThisTheMessageThread()) undoHistory.applyChange("Program", [this, index] { applyFactory(index); });
    else applyFactory(index);
}
std::unique_ptr<juce::XmlElement> TunerProcessor::parameterXml() { return state.copyState().createXml(); }
void TunerProcessor::loadPreset(const suite::PresetStore::Item& item) {
    if (item.factory) { setCurrentProgram(item.factoryIndex); return; }
    if (auto xml = presetStore.load(item)) loadPresetXml(*xml, item.name);
}
bool TunerProcessor::savePreset(const juce::String& name) {
    const auto xml = parameterXml();
    if (xml == nullptr || !presetStore.save(name, *xml)) return false;
    preset.name = suite::PresetStore::cleanName(name);
    preset.edited = false;
    return true;
}
void TunerProcessor::loadPresetXml(const juce::XmlElement& xml, const juce::String& name) {
    undoHistory.applyChange("Preset", [this, &xml, &name] {
        auto tree = juce::ValueTree::fromXml(xml);
        if (!tree.isValid() || tree.getType() != state.state.getType()) return;
        if (auto extra = tree.getChildWithName("suite"); extra.isValid()) tree.removeChild(extra, nullptr);
        injectDefaults(tree);
        state.replaceState(tree);
        preset.name = name;
        preset.edited = false;
    });
}
juce::AudioProcessorParameter* TunerProcessor::getBypassParameter() const { return state.getParameter("bypass"); }
void TunerProcessor::setParameter(const juce::String& id, float value) {
    if (auto* parameter = state.getParameter(id)) {
        parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(value)); parameter->endChangeGesture();
    }
}
juce::AudioProcessorEditor* TunerProcessor::createEditor() { return new TunerEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TunerProcessor(); }
