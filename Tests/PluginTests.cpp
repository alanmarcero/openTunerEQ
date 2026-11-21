#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Notes.h"
#include <iostream>
#include <cmath>
#include <cstdlib>
void require(bool value, const char* name) { if (!value) { std::cerr << "FAIL: " << name << '\n'; std::exit(1); } }
struct GestureListener : juce::AudioProcessorParameter::Listener {
    int begins = 0, ends = 0;
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int, bool starting) override { if (starting) ++begins; else ++ends; }
};
juce::MouseEvent mouseAt(juce::Component& component, juce::Point<float> position, int modifiers = juce::ModifierKeys::leftButtonModifier) {
    return {juce::Desktop::getInstance().getMainMouseSource(), position,
            juce::ModifierKeys(modifiers), 0, 0, 0, 0, 0,
            &component, &component, juce::Time::getCurrentTime(), position, juce::Time::getCurrentTime(), 1, false};
}
void checkAnalyzerSources() {
    for (double rate : {8000., 48000., 192000.}) {
        TunerProcessor processor;
        processor.setParameter("snap", 0);
        processor.setParameter("band0.freq", 1000);
        processor.setParameter("band0.gain", -12);
        processor.setParameter("output", 3);
        processor.prepareToPlay(rate, 257);
        processor.spectrum.setActive(true);
        SpectrumAnalyzer analyzer(processor.spectrum);
        juce::AudioBuffer<float> audio(2, 257);
        juce::MidiBuffer events;
        int sample = 0;
        auto measure = [&](bool afterEQ, bool hostBypass = false) {
            processor.setParameter("analyzerSource", afterEQ ? 1.0f : 0.0f);
            const auto end = sample + static_cast<int>(rate * 0.6);
            int untilRefresh = 0;
            for (; sample < end; sample += 257) {
                for (int i = 0; i < 257; ++i) {
                    const auto value = static_cast<float>(0.25 * std::sin(2 * juce::MathConstants<double>::pi * 1000 * (sample + i) / rate));
                    audio.setSample(0, i, value); audio.setSample(1, i, -value);
                }
                if (hostBypass) processor.processBlockBypassed(audio, events); else processor.processBlock(audio, events);
                untilRefresh += 257;
                if (untilRefresh >= rate / 24) { analyzer.update((sample + 257) / rate, afterEQ); untilRefresh = 0; }
            }
            return analyzer.levelAt(1000);
        };
        const auto before = measure(false);
        require(before > -15 && before < -11, "before-EQ spectrum measures the incoming track");
        require(std::abs(measure(true) - before + 9) < 0.2, "after-EQ spectrum includes the actual 12 dB cut and 3 dB output trim");
        require(std::abs(measure(false) - before) < 0.2, "switching back restores the unchanged input spectrum");
        processor.setParameter("bypass", 1);
        require(std::abs(measure(true) - before) < 0.2, "after-EQ spectrum follows global bypass");
        processor.setParameter("bypass", 0);
        require(std::abs(measure(true, true) - before) < 0.2, "after-EQ spectrum follows host bypass");
    }
}
void checkParameterText() {
    TunerProcessor processor;
    for (const auto* parameter : processor.getParameters()) {
        if (dynamic_cast<const juce::AudioParameterFloat*>(parameter) == nullptr) continue;
        for (float value : {0.0f, 0.13f, 0.5f, 0.77f, 1.0f}) {
            const auto shown = parameter->getText(value, 0);
            if (parameter->getText(parameter->getValueForText(shown), 0) != shown) std::cerr << parameter->getName(64) << " shows " << shown << '\n';
            require(parameter->getText(parameter->getValueForText(shown), 0) == shown, "typing a displayed value gives the same value");
        }
    }
    auto shown = [&](const char* id, float value) { const auto* p = processor.state.getParameter(id); return p->getText(p->convertTo0to1(value), 0); };
    auto typed = [&](const char* id, const char* text) { const auto* p = processor.state.getParameter(id); return p->convertFrom0to1(p->getValueForText(text)); };
    require(shown("band0.gain", 3) == "+3.0 dB" && shown("band0.gain", -0.04f) == "0.0 dB" && shown("output", -6) == "-6.0 dB",
            "gain shows one decimal, a plus on boosts and no negative zero");
    require(shown("band0.freq", 850) == "850 Hz" && shown("band0.freq", 6800) == "6.80 kHz" && shown("band0.freq", 16744) == "16.7 kHz",
            "frequency shows Hz below 1 kHz and kHz above");
    require(std::abs(typed("band0.freq", "6.8k") - 6800) < 1 && std::abs(typed("band0.freq", "6.8 kHz") - 6800) < 1
            && std::abs(typed("band0.freq", "850 Hz") - 850) < 0.01f, "frequency accepts Hz, k and kHz");
    require(typed("band0.drive", "50") == 50 && typed("band0.drive", "50%") == 50 && shown("scale", 55) == "55%", "percent controls accept 50 and 50%");
    require(std::abs(typed("band0.gain", "+3.0 dB") - 3) < 0.01f && std::abs(typed("a4", "442.5 Hz") - 442.5f) < 0.01f, "dB and Hz suffixes are accepted");
}
void checkButtonLayout(juce::Component& parent, int minimum) {
    // Labels and the resize corner may sit over other parts; every other clickable part needs its own space.
    auto takesSpace = [](const juce::Component& c) {
        bool clicksSelf = false, clicksChildren = false;
        c.getInterceptsMouseClicks(clicksSelf, clicksChildren);
        return c.isVisible() && clicksSelf && dynamic_cast<const juce::Label*>(&c) == nullptr
            && dynamic_cast<const juce::ResizableCornerComponent*>(&c) == nullptr;
    };
    for (int i = 0; i < parent.getNumChildComponents(); ++i) {
        auto* child = parent.getChildComponent(i);
        if (!child->isVisible()) continue;
        auto* slider = dynamic_cast<juce::Slider*>(child);
        if (slider != nullptr && slider->isRotary()) {
            const auto dial = slider->getLookAndFeel().getSliderLayout(*slider).sliderBounds;
            require(std::min(dial.getWidth(), dial.getHeight()) >= 48, "A dial is too small");
        }
        const bool actionable = dynamic_cast<const juce::Button*>(child) != nullptr
                             || dynamic_cast<const juce::ComboBox*>(child) != nullptr;
        if (actionable) require(child->getWidth() >= minimum && child->getHeight() >= minimum, "An action target is too small");
        if (!actionable && slider == nullptr) checkButtonLayout(*child, minimum);
        if (!takesSpace(*child)) continue;
        for (int j = i + 1; j < parent.getNumChildComponents(); ++j)
            if (takesSpace(*parent.getChildComponent(j)))
                require(!child->getBounds().intersects(parent.getChildComponent(j)->getBounds()), "Two controls or panels overlap");
    }
}

void saveFactoryPreview(const char* path) {
    TunerProcessor processor;
    processor.setCurrentProgram(0);
    processor.history().manager().clearUndoHistory();
    processor.prepareToPlay(48000, 512);
    processor.spectrum.setActive(true);
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    juce::Random random(5412);
    double lowNoise = 0;
    for (int block = 0; block < 96; ++block) {
        for (int i = 0; i < 512; ++i) {
            const auto t = (block * 512 + i) / 48000.0;
            double value = 0;
            const auto beat = std::fmod(t, 0.5);
            for (double hz : {65.406, 130.813, 164.814, 195.998, 261.626})
                for (int harmonic = 1; harmonic <= 12; ++harmonic)
                    value += 0.025 / harmonic * (0.35 + 0.65 * std::exp(-beat * 6)) * std::sin(2 * juce::MathConstants<double>::pi * hz * harmonic * t);
            const auto noise = random.nextFloat() * 2 - 1;
            lowNoise = 0.985 * lowNoise + 0.015 * noise;
            value += 0.07 * lowNoise + 0.03 * noise * (0.3 + 0.7 * std::exp(-std::fmod(t + 0.25, 0.5) * 25));
            value += 0.15 * std::exp(-beat * 16) * std::sin(2 * juce::MathConstants<double>::pi * 55 * t);
            audio.setSample(0, i, static_cast<float>(value));
            audio.setSample(1, i, static_cast<float>(value));
        }
        processor.processBlock(audio, midi);
    }
    juce::Desktop::getInstance().setGlobalScaleFactor(1.0f);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    require(editor != nullptr && editor->getWidth() == 1160 && editor->getHeight() == 900, "editor stays at its default size");
    const auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
    require(snapshot.getWidth() == 1160 && snapshot.getHeight() == 900, "preview is the default editor size");
    auto file = juce::File::getCurrentWorkingDirectory().getChildFile(path);
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    juce::FileOutputStream stream(file);
    require(stream.openedOk() && juce::PNGImageFormat().writeImageToStream(snapshot, stream), "factory preview saved");
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc == 3 && juce::String(argv[1]) == "--preview") {
        saveFactoryPreview(argv[2]);
        std::cout << "PASS: factory default editor preview\n";
        return 0;
    }
    TunerProcessor processor;
    auto* analyzerEnabled = processor.state.getRawParameterValue("analyzerEnabled");
    require(analyzerEnabled != nullptr && analyzerEnabled->load() == 1.0f, "live analyzer is available and on by default");
    auto* analyzerSource = processor.state.getRawParameterValue("analyzerSource");
    require(analyzerSource != nullptr && analyzerSource->load() == 0.0f, "live audio source defaults to before EQ");
    require(processor.state.getRawParameterValue("analyzerFreeze") != nullptr, "spectrum freeze parameter is registered");
    require(processor.state.getRawParameterValue("analyzerRelease") != nullptr, "spectrum release parameter is registered");
    require(processor.state.getRawParameterValue("analyzerTilt") != nullptr, "spectrum tilt parameter is registered");
    require(processor.state.getRawParameterValue("scale") != nullptr && processor.state.getRawParameterValue("scale")->load() == 100.0f, "EQ scale parameter defaults to 100%");
    require(processor.state.getRawParameterValue("autoGain") != nullptr && processor.state.getRawParameterValue("autoGain")->load() == 0.0f, "autoGain parameter defaults off");
    require(processor.state.getRawParameterValue("soloSelected") != nullptr && processor.state.getRawParameterValue("soloSelected")->load() == 0.0f, "soloSelected parameter defaults off");
    processor.setParameter("analyzerEnabled", 0);
    processor.setParameter("analyzerStyle", 2);
    processor.setParameter("analyzerSource", 1);
    processor.setParameter("analyzerFreeze", 1);
    processor.setParameter("analyzerRelease", 2);
    processor.setParameter("analyzerTilt", 3);
    juce::MemoryBlock analyzerState;
    processor.getStateInformation(analyzerState);
    TunerProcessor analyzerRestored;
    analyzerRestored.setStateInformation(analyzerState.getData(), static_cast<int>(analyzerState.getSize()));
    require(analyzerRestored.state.getRawParameterValue("analyzerEnabled")->load() == 0.0f, "analyzer off survives session recall");
    require(analyzerRestored.state.getRawParameterValue("analyzerStyle")->load() == 2.0f, "analyzer style survives session recall");
    require(analyzerRestored.state.getRawParameterValue("analyzerSource")->load() == 1.0f, "after-EQ source survives session recall");
    require(analyzerRestored.state.getRawParameterValue("analyzerFreeze")->load() == 1.0f, "spectrum freeze survives session recall");
    require(analyzerRestored.state.getRawParameterValue("analyzerRelease")->load() == 2.0f, "spectrum release survives session recall");
    require(analyzerRestored.state.getRawParameterValue("analyzerTilt")->load() == 3.0f, "spectrum tilt survives session recall");
    auto legacy = processor.state.copyState();
    for (int i = legacy.getNumChildren(); --i >= 0;) {
        const auto id = legacy.getChild(i).getProperty("id").toString();
        if (id == "analyzerEnabled" || id == "analyzerStyle" || id == "analyzerSource" || id == "analyzerFreeze" || id == "analyzerRelease" || id == "analyzerTilt"
            || id == "scale" || id == "autoGain" || id == "soloSelected" || id == "phaseComp" || id == "qMode" || id == "dynThreshold"
            || id.endsWith(".dyn") || id.endsWith(".drive")) legacy.removeChild(i, nullptr);
    }
    juce::MemoryBlock legacyState;
    juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(), legacyState);
    analyzerRestored.setStateInformation(legacyState.getData(), static_cast<int>(legacyState.getSize()));
    require(analyzerRestored.state.getRawParameterValue("analyzerEnabled")->load() == 1.0f, "old sessions enable the new visualizer");
    require(analyzerRestored.state.getRawParameterValue("analyzerStyle")->load() == 0.0f, "old sessions use spectrum style");
    require(analyzerRestored.state.getRawParameterValue("analyzerSource")->load() == 0.0f, "old sessions use before-EQ audio");
    require(analyzerRestored.state.getRawParameterValue("analyzerFreeze")->load() == 0.0f, "old sessions leave spectrum freeze off");
    require(analyzerRestored.state.getRawParameterValue("analyzerRelease")->load() == 1.0f, "old sessions use the medium spectrum release");
    require(analyzerRestored.state.getRawParameterValue("analyzerTilt")->load() == 2.0f, "old sessions use the natural spectrum tilt");
    require(analyzerRestored.state.getRawParameterValue("scale")->load() == 100.0f, "old sessions default EQ scale to 100%");
    require(analyzerRestored.state.getRawParameterValue("autoGain")->load() == 0.0f, "old sessions default autoGain off");
    require(analyzerRestored.state.getRawParameterValue("soloSelected")->load() == 0.0f, "old sessions default soloSelected off");
    require(analyzerRestored.state.getRawParameterValue("phaseComp")->load() == 0.0f, "old sessions leave phase alignment off");
    require(analyzerRestored.state.getRawParameterValue("qMode")->load() == 0.0f, "old sessions use the symmetric bell Q law");
    require(std::abs(analyzerRestored.state.getRawParameterValue("dynThreshold")->load() + 24.0f) < 0.01f, "old sessions use the default dynamic threshold");
    require(std::abs(analyzerRestored.state.getRawParameterValue(params::bandId(0, "dyn"))->load()) < 0.01f
            && analyzerRestored.state.getRawParameterValue(params::bandId(0, "drive"))->load() == 0.0f, "old sessions keep bands static and clean");
    processor.setParameter("analyzerFreeze", 0);
    processor.setParameter("analyzerEnabled", 1);
    processor.setParameter("analyzerSource", 0);
    {
        TunerProcessor visible, hidden;
        visible.spectrum.setActive(true);
        hidden.setParameter("analyzerEnabled", 0);
        visible.setParameter("band0.gain", 9); hidden.setParameter("band0.gain", 9);
        visible.prepareToPlay(48000, 257); hidden.prepareToPlay(48000, 257);
        juce::AudioBuffer<float> shownAudio(2, 257), hiddenAudio(2, 257);
        juce::MidiBuffer events;
        for (int block = 0; block < 64; ++block) {
            visible.setParameter("analyzerSource", static_cast<float>((block / 16) % 2));
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 257; ++i) {
                const auto value = static_cast<float>(0.2 * std::sin((block * 257 + i) * (c + 1) * 0.07));
                shownAudio.setSample(c, i, value); hiddenAudio.setSample(c, i, value);
            }
            visible.processBlock(shownAudio, events); hidden.processBlock(hiddenAudio, events);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 257; ++i)
                require(shownAudio.getSample(c, i) == hiddenAudio.getSample(c, i), "visualizer on/off leaves processed audio bit-identical");
        }
    }
    checkAnalyzerSources();
    checkParameterText();
    processor.prepareToPlay(48000, 512);
    processor.setParameter("band0.freq", 445);
    auto settings = processor.readSettings();
    require(std::abs(processor.bandFrequency(0) - 440) < 0.001, "host automation snapped");
    processor.setParameter("a4", 445);
    require(std::abs(processor.bandFrequency(0) - 445) < 0.001, "snapping follows the A4 reference");
    processor.setParameter("a4", 440);
    processor.setParameter("snap", 0);
    require(std::abs(processor.bandFrequency(0) - 445) < 0.01, "host free frequency");
    processor.setParameter("band0.freq", 20.601722f);
    processor.setParameter("band0.gain", 24);
    processor.setParameter("band0.q", 18);
    require(processor.getTailLengthSeconds() > 2, "host tail accommodates low-note high-Q ringing");
    processor.setParameter("band0.freq", 445);
    processor.setParameter("band0.gain", 12);
    processor.setParameter("band0.q", 2);
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    TunerProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    restored.prepareToPlay(48000, 512);
    require(std::abs(restored.bandFrequency(0) - 445) < 0.01, "state restores frequency and snap");
    require(std::abs(restored.readSettings().bands[0].gainDb - 12) < 0.001, "state restores gain");
    restored.setStateInformation("garbage", 7);
    require(std::abs(restored.bandFrequency(0) - 445) < 0.01, "invalid state ignored");
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    double phase = 0, sumIn = 0, sumOut = 0;
    for (int block = 0; block < 100; ++block) {
        for (int i = 0; i < 512; ++i) {
            const auto x = static_cast<float>(0.01 * std::sin(phase));
            phase += 2 * juce::MathConstants<double>::pi * 445 / 48000;
            audio.setSample(0, i, x); audio.setSample(1, i, 0);
            if (block > 50) sumIn += x*x;
        }
        restored.processBlock(audio, midi);
        for (int i = 0; i < 512; ++i) {
            if (block > 50) sumOut += audio.getSample(0, i) * audio.getSample(0, i);
            require(audio.getSample(1, i) == 0, "no stereo crosstalk");
        }
    }
    require(std::abs(10 * std::log10(sumOut / sumIn) - 12) < 0.03, "processor actual bell gain");
    restored.setParameter("bypass", 1);
    for (int block = 0; block < 4; ++block) {
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) audio.setSample(c, i, 0.1f);
        restored.processBlock(audio, midi);
    }
    require(std::abs(audio.getSample(0, 511) - 0.1f) < 1e-6, "host bypass transparent");
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    require(editor != nullptr && editor->getWidth() >= 1000, "editor creation");
    checkButtonLayout(*editor, 40);
    struct ArtworkHost : juce::AudioPlayHead {
        PositionInfo position;
        juce::Optional<PositionInfo> getPosition() const override { return position; }
    } host;
    processor.setPlayHead(&host);
    juce::AudioBuffer<float> clockAudio(2, 256);
    juce::MidiBuffer clockMidi;
    clockAudio.clear();
    host.position.setIsPlaying(true);
    host.position.setBpm(140.0);
    host.position.setPpqPosition(-1.75);
    processor.processBlock(clockAudio, clockMidi);
    require(std::abs(processor.artworkPhase() - 0.25f) < 0.0001f, "SVG follows a negative host position");
    const auto onbeat = editor->createComponentSnapshot(juce::Rectangle<int>(18, 12, 64, 62));
    host.position.setPpqPosition(22.875);
    processor.processBlock(clockAudio, clockMidi);
    require(std::abs(processor.artworkPhase() - 0.875f) < 0.0001f, "SVG follows a host seek");
    const auto offbeat = editor->createComponentSnapshot(juce::Rectangle<int>(18, 12, 64, 62));
    int changed = 0;
    for (int y = 0; y < onbeat.getHeight(); ++y)
        for (int x = 0; x < onbeat.getWidth(); ++x)
            if (onbeat.getPixelAt(x, y) != offbeat.getPixelAt(x, y)) ++changed;
    require(changed > 30, "Native SVG pixels move with beat phase");
    host.position.setIsPlaying(false);
    host.position.setPpqPosition(0);
    processor.processBlock(clockAudio, clockMidi);
    require(std::abs(processor.artworkPhase() - 0.875f) < 0.0001f, "SVG holds phase when the host stops");
    processor.setPlayHead(nullptr);
    processor.processBlock(clockAudio, clockMidi);
    processor.processBlock(clockAudio, clockMidi);
    require(processor.artworkPhase() > 0.875f, "SVG advances on audio time without host transport");

    auto find = [&](auto&& self, juce::Component& root, const juce::String& id) -> juce::Component* {
        if (root.getComponentID() == id) return &root;
        for (auto* child : root.getChildren()) if (auto* match = self(self, *child, id)) return match;
        return nullptr;
    };
    auto* freq = dynamic_cast<juce::Slider*>(find(find, *editor, "band0.freq"));
    require(freq != nullptr, "frequency UI attached");
    auto* tuning = dynamic_cast<juce::Slider*>(find(find, *editor, "a4"));
    require(tuning != nullptr && tuning->isDoubleClickReturnEnabled() && std::abs(tuning->getDoubleClickReturnValue() - 440) < 0.01,
            "A4 reference double-clicks back to 440 Hz");
    freq->setValue(880, juce::sendNotificationSync);
    require(std::abs(processor.bandFrequency(0) - 880) < 0.01, "frequency control changes DSP");
    auto* pitch = dynamic_cast<juce::ComboBox*>(find(find, *editor, "band0.note"));
    require(pitch != nullptr, "direct note selector present");
    pitch->setSelectedId(1, juce::sendNotificationSync);
    require(ote::nearestNote(processor.bandFrequency(0), processor.referencePitch()) % 12 == 0, "direct C selection changes frequency");
    processor.setParameter("snap", 1);
    processor.setParameter("band0.freq", 445);
    require(freq->getTextFromValue(445) == "440 Hz", "frequency text uses snapped value and one unit");
    auto findType = [&](auto&& self, juce::Component& root, bool wantPiano) -> juce::Component* {
        if (wantPiano ? dynamic_cast<PianoStrip*>(&root) != nullptr : dynamic_cast<ResponseGraph*>(&root) != nullptr) return &root;
        for (auto* child : root.getChildren()) if (auto* match = self(self, *child, wantPiano)) return match;
        return nullptr;
    };
    auto* piano = dynamic_cast<PianoStrip*>(findType(findType, *editor, true));
    require(piano != nullptr, "piano present");
    processor.setParameter("band0.freq", 16744.036f);
    piano->mouseDown(mouseAt(*piano, {piano->getWidth() * 0.5f + 5, piano->getHeight() - 8.0f}));
    require(ote::nearestNote(processor.bandFrequency(0), processor.referencePitch()) == 132, "piano follows octave 10");
    processor.setParameter("band0.freq", 440); processor.setParameter("band0.gain", 0);
    auto* graph = dynamic_cast<ResponseGraph*>(findType(findType, *editor, false));
    require(graph != nullptr, "graph present");
    auto* visualizer = dynamic_cast<juce::TextButton*>(find(find, *editor, "analyzerEnabled"));
    auto* visualizerStyle = dynamic_cast<juce::ComboBox*>(find(find, *editor, "analyzerStyle"));
    auto* visualizerSource = dynamic_cast<juce::ComboBox*>(find(find, *editor, "analyzerSource"));
    auto* freezeToggle = dynamic_cast<juce::TextButton*>(find(find, *editor, "analyzerFreeze"));
    auto* visualizerRelease = dynamic_cast<juce::ComboBox*>(find(find, *editor, "analyzerRelease"));
    auto* visualizerTilt = dynamic_cast<juce::ComboBox*>(find(find, *editor, "analyzerTilt"));
    require(visualizer != nullptr && visualizer->getToggleState(), "visualizer starts on in the actual editor");
    require(visualizerStyle != nullptr, "visualizer style selector present");
    require(visualizerSource != nullptr && visualizerSource->getSelectedId() == 1, "before-EQ audio source shown in the editor");
    require(freezeToggle != nullptr && visualizerRelease != nullptr && visualizerTilt != nullptr,
            "freeze, release speed, and tilt controls appear in the native editor");
    freezeToggle->setToggleState(true, juce::sendNotificationSync);
    visualizerRelease->setSelectedId(3, juce::sendNotificationSync);
    visualizerTilt->setSelectedId(4, juce::sendNotificationSync);
    require(processor.state.getRawParameterValue("analyzerFreeze")->load() == 1.0f, "freeze toggle reaches session state");
    require(processor.state.getRawParameterValue("analyzerRelease")->load() == 2.0f, "release selector reaches session state");
    require(processor.state.getRawParameterValue("analyzerTilt")->load() == 3.0f, "tilt selector reaches session state");
    freezeToggle->setToggleState(false, juce::sendNotificationSync);
    visualizerRelease->setSelectedId(2, juce::sendNotificationSync);
    visualizerTilt->setSelectedId(3, juce::sendNotificationSync);
    visualizerSource->setSelectedId(2, juce::sendNotificationSync);
    require(processor.state.getRawParameterValue("analyzerSource")->load() == 1, "after-EQ selection reaches audio source parameter");
    visualizerStyle->setSelectedId(3, juce::sendNotificationSync);
    require(processor.state.getRawParameterValue("analyzerStyle")->load() == 2, "style selection reaches session state");
    visualizer->setToggleState(false, juce::sendNotificationSync);
    graph->refresh();
    require(processor.state.getRawParameterValue("analyzerEnabled")->load() == 0, "analyzer toggle turns analysis off");
    require(visualizerStyle->isEnabled() && visualizerStyle->getAlpha() < 1 && visualizerSource->getAlpha() < 1 && freezeToggle->getAlpha() < 1
            && visualizerRelease->getAlpha() < 1 && visualizerTilt->getAlpha() < 1, "analyzer off dims its controls and keeps them editable");
    require(visualizerTilt->getTooltip().contains("Turn on the analyzer"), "an inactive control's tooltip says what turns it on");
    visualizer->setToggleState(true, juce::sendNotificationSync);
    visualizerStyle->setSelectedId(1, juce::sendNotificationSync);
    visualizerSource->setSelectedId(1, juce::sendNotificationSync);
    graph->refresh();
    SpectrumAnalyzer inputSpectrum(processor.spectrum);
    processor.setParameter("snap", 0);
    processor.setParameter("band0.freq", 1000);
    processor.setParameter("band0.gain", -24);
    for (int block = 0; block < 32; ++block) {
        for (int i = 0; i < 512; ++i) {
            const auto sample = static_cast<float>(0.5 * std::sin(2 * juce::MathConstants<double>::pi * 1000 * (block * 512 + i) / 48000));
            audio.setSample(0, i, sample); audio.setSample(1, i, -sample);
        }
        processor.processBlock(audio, midi);
        inputSpectrum.update(block * 512.0 / 48000);
    }
    require(inputSpectrum.levelAt(1000) > -8, "analyzer taps input before a deep EQ cut, including anti-phase stereo");
    processor.setParameter("band0.gain", 3);
    processor.setParameter("band0.freq", 440);
    const auto gainBeforeClick = processor.readSettings().bands[0].gainDb;
    const auto targetX = 48.0f + static_cast<float>(std::log(1000.0 / 20) / std::log(1000.0)) * (graph->getWidth() - 96);
    graph->mouseDown(mouseAt(*graph, {targetX, 180}, juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier));
    require(std::abs(processor.bandFrequency(0) - 1000) < 0.1 && processor.readSettings().bands[0].gainDb == gainBeforeClick, "Alt-click places selected band at the analyzed frequency without changing gain");
    processor.setParameter("snap", 1);
    processor.setParameter("band0.freq", 440); processor.setParameter("band0.gain", 0);
    GestureListener listener;
    auto* frequencyParameter = processor.state.getParameter("band0.freq");
    frequencyParameter->addListener(&listener);
    const auto graphX = 48.0f + static_cast<float>(std::log(440.0 / 20) / std::log(1000.0)) * (graph->getWidth() - 96);
    graph->mouseDown(mouseAt(*graph, {graphX, 36.0f + (graph->getHeight() - 68) / 2.0f}));
    require(listener.begins == 1 && listener.ends == 0, "drag begins host gesture");
    auto* place = dynamic_cast<juce::ComboBox*>(find(find, *editor, "band0.placement"));
    require(place != nullptr && place->getNumItems() == 5 && place->getSelectedId() == 1, "each band has a Stereo placement selector");
    place->setSelectedId(5, juce::sendNotificationSync);
    require(std::lround(processor.state.getRawParameterValue("band0.placement")->load()) == 4, "Side placement reaches the parameter");
    {
        TunerProcessor inactive;
        inactive.setParameter("band1.shape", 5);
        inactive.setParameter("band2.enabled", 0);
        inactive.history().manager().clearUndoHistory();
        std::unique_ptr<juce::AudioProcessorEditor> view(inactive.createEditor());
        auto* notchGain = dynamic_cast<juce::Slider*>(find(find, *view, "band1.gain"));
        auto* offFrequency = dynamic_cast<juce::Slider*>(find(find, *view, "band2.freq"));
        require(notchGain != nullptr && notchGain->isEnabled() && notchGain->getAlpha() < 1 && notchGain->getTooltip().contains("Choose Bell"),
                "gain on a notch is dimmed, editable and says what turns it on");
        require(offFrequency != nullptr && offFrequency->getAlpha() < 1 && offFrequency->getTooltip().contains("Turn the band on"), "a disabled band dims its controls");
        require(find(find, *view, "phaseComp")->getAlpha() < 1 && find(find, *view, "dynThreshold")->getAlpha() < 1,
                "Phase Align and the dynamic threshold dim when no band uses them");
        require(!find(find, *view, "undo")->isEnabled(), "Undo is disabled with an empty history");
    }
    place->setSelectedId(1, juce::sendNotificationSync);
    editor.reset();
    require(listener.ends == 1, "closing editor ends active host gesture");
    frequencyParameter->removeListener(&listener);
    {
        TunerProcessor dragged;
        dragged.setParameter("band0.gain", 6);
        std::unique_ptr<juce::AudioProcessorEditor> view(dragged.createEditor());
        auto* nodes = dynamic_cast<ResponseGraph*>(findType(findType, *view, false));
        const auto plotHeight = nodes->getHeight() - 76.0f;
        auto nodeX = [&](double hz) { return 48.0f + static_cast<float>(std::log(hz / 20) / std::log(1000.0)) * (nodes->getWidth() - 96); };
        auto nodeY = [&](double db) { return 44.0f + plotHeight / 2 - static_cast<float>(db) / 48 * plotHeight; };
        auto gain = [&] { return dragged.state.getRawParameterValue("band0.gain")->load(); };
        const juce::Point<float> offCentre {nodeX(dragged.bandFrequency(0)), nodeY(6) + 10};
        nodes->mouseDown(mouseAt(*nodes, offCentre));
        nodes->mouseDrag(mouseAt(*nodes, offCentre));
        nodes->mouseUp(mouseAt(*nodes, offCentre));
        require(std::abs(gain() - 6) < 0.05f, "an off-centre grab does not jump the node");
        dragged.setParameter("scale", 50);
        const juce::Point<float> scaled {nodeX(dragged.bandFrequency(0)), nodeY(3)};
        nodes->mouseDown(mouseAt(*nodes, scaled));
        nodes->mouseDrag(mouseAt(*nodes, scaled.translated(0, -plotHeight / 16)));
        nodes->mouseUp(mouseAt(*nodes, scaled));
        require(std::abs(gain() - 12) < 0.05f, "a 3 dB drag at 50% Scale adds 6 dB to the parameter");
        dragged.setParameter("scale", 100);
        nodes->mouseDown(mouseAt(*nodes, {nodeX(dragged.bandFrequency(0)), nodeY(12)}));
        nodes->mouseDrag(mouseAt(*nodes, {nodeX(dragged.bandFrequency(0)), nodeY(12) + plotHeight / 4}, juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier));
        nodes->mouseUp(mouseAt(*nodes, {0, 0}));
        require(std::abs(gain() - 9) < 0.05f, "Shift drags at a quarter of the speed");
        auto q = [&](int band) { return dragged.state.getRawParameterValue(params::bandId(band, "q"))->load(); };
        const juce::MouseWheelDetails tick {0, 0.1f, false, false, false};
        dragged.history().manager().clearUndoHistory();
        nodes->mouseWheelMove(mouseAt(*nodes, {nodeX(dragged.bandFrequency(3)), nodeY(0)}), tick);
        nodes->mouseWheelMove(mouseAt(*nodes, {nodeX(dragged.bandFrequency(3)), nodeY(0)}), tick);
        nodes->mouseWheelMove(mouseAt(*nodes, {nodeX(1000), nodeY(-20)}), tick);
        nodes->mouseExit(mouseAt(*nodes, {0, 0}));
        require(q(3) > 0.9f && std::abs(q(0) - 0.71f) < 0.01f, "the wheel changes the Q of the band under the cursor only");
        dragged.history().undo();
        require(std::abs(q(3) - 0.71f) < 0.01f, "one scroll is one undo step");
    }
    {
        require(processor.getNumPrograms() == 11, "factory bank is in the DAW program list");
        require(processor.getProgramName(0) == "Flat" && processor.getProgramName(6) == "Side high-pass", "factory program names");
        TunerProcessor programs;
        programs.setCurrentProgram(6);
        require(std::lround(programs.state.getRawParameterValue("band0.placement")->load()) == 4, "Side high-pass preset places the first band on Side");
        require(programs.state.getRawParameterValue("band1.enabled")->load() < 0.5f, "Side high-pass preset disables the unused bands");
        programs.setCurrentProgram(0);
        require(std::lround(programs.state.getRawParameterValue("band0.placement")->load()) == 0, "Flat preset restores Stereo");
        require(programs.state.getRawParameterValue("band1.enabled")->load() > 0.5f, "Flat preset restores the default bands");
        TunerProcessor legacyPlacement;
        legacyPlacement.setParameter("band2.placement", 3);
        auto legacyTree = legacyPlacement.state.copyState();
        for (int i = legacyTree.getNumChildren(); --i >= 0;)
            if (legacyTree.getChild(i).getProperty("id").toString().endsWith(".placement")) legacyTree.removeChild(i, nullptr);
        juce::MemoryBlock legacyBlock;
        juce::AudioProcessor::copyXmlToBinary(*legacyTree.createXml(), legacyBlock);
        legacyPlacement.setStateInformation(legacyBlock.getData(), static_cast<int>(legacyBlock.getSize()));
        require(std::lround(legacyPlacement.state.getRawParameterValue("band2.placement")->load()) == 0, "old sessions default every band to Stereo");
        TunerProcessor history;
        history.setParameter("band0.gain", 6);
        require(std::abs(history.state.getRawParameterValue("band0.gain")->load() - 6) < 0.05f, "gain edit is audible before undo");
        auto* historyEditor = history.createEditor();
        require(historyEditor->keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)), "Cmd/Ctrl+Z is handled");
        require(std::abs(history.state.getRawParameterValue("band0.gain")->load()) < 0.05f, "undo restores the previous gain");
        require(historyEditor->keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)), "Shift+Cmd/Ctrl+Z is handled");
        require(std::abs(history.state.getRawParameterValue("band0.gain")->load() - 6) < 0.05f, "redo restores the edited gain");
        delete historyEditor;
        auto* freq = history.state.getParameter("band0.freq");
        auto* gain = history.state.getParameter("band0.gain");
        const auto freqStart = freq->getValue();
        const auto gainStart = gain->getValue();
        freq->beginChangeGesture();
        gain->beginChangeGesture();
        freq->setValueNotifyingHost(freq->convertTo0to1(1000));
        gain->setValueNotifyingHost(gain->convertTo0to1(4));
        gain->endChangeGesture();
        freq->endChangeGesture();
        history.history().undo();
        require(std::abs(freq->getValue() - freqStart) < 1.0e-4f && std::abs(gain->getValue() - gainStart) < 1.0e-4f, "one drag of frequency and gain undoes together");
        TunerProcessor slots;
        slots.setParameter("output", -6);
        slots.compare().toggle();
        require(std::abs(slots.state.getRawParameterValue("output")->load()) < 0.05f, "A/B toggle recalls the other slot");
        slots.compare().toggle();
        require(std::abs(slots.state.getRawParameterValue("output")->load() + 6) < 0.05f, "A/B toggle returns to slot A");
        slots.compare().copyAtoB();
        slots.compare().toggle();
        require(std::abs(slots.state.getRawParameterValue("output")->load() + 6) < 0.05f, "copy A to B duplicates slot A");
        auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("openTunerEQ-preset-test");
        directory.deleteRecursively();
        suite::PresetStore store("openTunerEQ", {"Flat", "Kick punch"}, directory);
        require(store.filtered("").size() == 2, "preset browser lists factory presets");
        slots.setParameter("band0.placement", 4);
        auto saved = slots.parameterXml();
        require(saved != nullptr && store.save("Room notch", *saved), "user preset saves to the preset folder");
        require(!store.save("Flat", *saved), "factory preset names stay read-only");
        require(store.filtered("room").size() == 1 && !store.filtered("room")[0].factory, "text search finds the user preset");
        auto user = store.filtered("room")[0];
        store.setFavorite(user, true);
        require(store.filtered("")[0].favorite, "favorites sort to the front");
        require(store.rename(store.filtered("room")[0], "Room air"), "user presets can be renamed");
        auto renamed = store.filtered("air");
        require(renamed.size() == 1 && renamed[0].favorite, "rename keeps the favorite");
        slots.setParameter("band0.placement", 0);
        auto loaded = store.load(renamed[0]);
        require(loaded != nullptr, "user preset reloads from disk");
        slots.loadPresetXml(*loaded, renamed[0].name);
        require(std::lround(slots.state.getRawParameterValue("band0.placement")->load()) == 4, "loaded user preset restores placement");
        require(store.remove(store.filtered("air")[0]) && store.filtered("air").isEmpty(), "user presets can be deleted");
        require(!directory.getChildFile("Room air.xml").existsAsFile(), "delete removes the preset file");
        directory.deleteRecursively();
    }
    if (argc > 1) {
        processor.setParameter("analyzerFreeze", 1);
        processor.setParameter("analyzerRelease", 2);
        processor.setParameter("analyzerTilt", 3);
        processor.setParameter("snap", 1);
        processor.setParameter("band0.freq", 65.4064f); processor.setParameter("band0.gain", 3.5f);
        processor.setParameter("band1.freq", 261.6256f); processor.setParameter("band1.gain", -12.0f);
        processor.setParameter("band2.freq", 880); processor.setParameter("band2.gain", 2.5f);
        processor.setParameter("band3.freq", 2793.826f); processor.setParameter("band3.gain", -9.0f);
        processor.setParameter("band4.freq", 8372.018f); processor.setParameter("band4.gain", 4.0f);
        processor.setParameter("band4.placement", 4);
        editor.reset(processor.createEditor());
        graph = dynamic_cast<ResponseGraph*>(findType(findType, *editor, false));
        for (int capture = 0; capture < 4; ++capture) {
            const int style = capture < 3 ? capture : 0;
            processor.setParameter("analyzerStyle", static_cast<float>(style));
            processor.setParameter("analyzerSource", capture == 3 ? 1.0f : 0.0f);
            juce::Random random(5412);
            double lowNoise = 0;
            for (int block = 0; block < 512; ++block) {
                for (int i = 0; i < 512; ++i) {
                    const auto t = (block * 512 + i) / 48000.0;
                    double value = 0;
                    const auto beat = std::fmod(t, 0.5);
                    for (double hz : {65.406, 130.813, 164.814, 195.998, 261.626})
                        for (int harmonic = 1; harmonic <= 12; ++harmonic)
                            value += 0.025 / harmonic * (0.35 + 0.65 * std::exp(-beat * 6)) * std::sin(2 * juce::MathConstants<double>::pi * hz * harmonic * t);
                    const auto noise = random.nextFloat() * 2 - 1;
                    lowNoise = 0.985 * lowNoise + 0.015 * noise;
                    value += 0.07 * lowNoise + 0.03 * noise * (0.3 + 0.7 * std::exp(-std::fmod(t + 0.25, 0.5) * 25));
                    value += 0.15 * std::exp(-beat * 16) * std::sin(2 * juce::MathConstants<double>::pi * 55 * t);
                    audio.setSample(0, i, static_cast<float>(value)); audio.setSample(1, i, static_cast<float>(value));
                }
                processor.processBlock(audio, midi);
                if (block % 4 == 0) { juce::Thread::sleep(42); graph->refresh(); }
            }
            const auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.5f);
            auto file = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
            if (style > 0) file = file.getSiblingFile(file.getFileNameWithoutExtension() + (style == 1 ? "-bars.png" : "-spectrogram.png"));
            if (capture == 3) file = file.getSiblingFile(file.getFileNameWithoutExtension() + "-after.png");
            file.deleteFile(); juce::FileOutputStream stream(file);
            require(stream.openedOk() && juce::PNGImageFormat().writeImageToStream(snapshot, stream), "live editor screenshot saved");
        }
    }
    std::cout << "PASS: processor audio, automation, state, bypass, stereo, editor controls\n";
}
