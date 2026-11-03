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
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    TunerProcessor processor;
    auto* analyzerEnabled = processor.state.getRawParameterValue("analyzerEnabled");
    require(analyzerEnabled != nullptr && analyzerEnabled->load() == 1.0f, "live analyzer is available and on by default");
    processor.setParameter("analyzerEnabled", 0);
    processor.setParameter("analyzerStyle", 2);
    juce::MemoryBlock analyzerState;
    processor.getStateInformation(analyzerState);
    TunerProcessor analyzerRestored;
    analyzerRestored.setStateInformation(analyzerState.getData(), static_cast<int>(analyzerState.getSize()));
    require(analyzerRestored.state.getRawParameterValue("analyzerEnabled")->load() == 0.0f, "analyzer off survives session recall");
    require(analyzerRestored.state.getRawParameterValue("analyzerStyle")->load() == 2.0f, "analyzer style survives session recall");
    auto legacy = processor.state.copyState();
    for (int i = legacy.getNumChildren(); --i >= 0;) {
        const auto id = legacy.getChild(i).getProperty("id").toString();
        if (id == "analyzerEnabled" || id == "analyzerStyle") legacy.removeChild(i, nullptr);
    }
    juce::MemoryBlock legacyState;
    juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(), legacyState);
    analyzerRestored.setStateInformation(legacyState.getData(), static_cast<int>(legacyState.getSize()));
    require(analyzerRestored.state.getRawParameterValue("analyzerEnabled")->load() == 1.0f, "old sessions enable the new visualizer");
    require(analyzerRestored.state.getRawParameterValue("analyzerStyle")->load() == 0.0f, "old sessions use spectrum style");
    processor.setParameter("analyzerEnabled", 1);
    {
        TunerProcessor visible, hidden;
        visible.spectrum.setActive(true);
        hidden.setParameter("analyzerEnabled", 0);
        visible.setParameter("band0.gain", 9); hidden.setParameter("band0.gain", 9);
        visible.prepareToPlay(48000, 257); hidden.prepareToPlay(48000, 257);
        juce::AudioBuffer<float> shownAudio(2, 257), hiddenAudio(2, 257);
        juce::MidiBuffer events;
        for (int block = 0; block < 64; ++block) {
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 257; ++i) {
                const auto value = static_cast<float>(0.2 * std::sin((block * 257 + i) * (c + 1) * 0.07));
                shownAudio.setSample(c, i, value); hiddenAudio.setSample(c, i, value);
            }
            visible.processBlock(shownAudio, events); hidden.processBlock(hiddenAudio, events);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 257; ++i)
                require(shownAudio.getSample(c, i) == hiddenAudio.getSample(c, i), "visualizer on/off leaves processed audio bit-identical");
        }
    }
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
    require(processor.getTailLengthSeconds() > 10, "host tail accommodates low-note high-Q ringing");
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
    auto find = [&](auto&& self, juce::Component& root, const juce::String& id) -> juce::Component* {
        if (root.getComponentID() == id) return &root;
        for (auto* child : root.getChildren()) if (auto* match = self(self, *child, id)) return match;
        return nullptr;
    };
    auto* freq = dynamic_cast<juce::Slider*>(find(find, *editor, "band0.freq"));
    require(freq != nullptr, "frequency UI attached");
    freq->setValue(880, juce::sendNotificationSync);
    require(std::abs(processor.bandFrequency(0) - 880) < 0.01, "frequency control changes DSP");
    auto* pitch = dynamic_cast<juce::ComboBox*>(find(find, *editor, "band0.note"));
    require(pitch != nullptr, "direct note selector present");
    pitch->setSelectedId(1, juce::sendNotificationSync);
    require(ote::nearestNote(processor.bandFrequency(0), processor.referencePitch()) % 12 == 0, "direct C selection changes frequency");
    processor.setParameter("snap", 1);
    processor.setParameter("band0.freq", 445);
    require(freq->getTextFromValue(445) == "440.00 Hz", "frequency text uses snapped value and one unit");
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
    require(visualizer != nullptr && visualizer->getToggleState(), "visualizer starts on in the actual editor");
    require(visualizerStyle != nullptr, "visualizer style selector present");
    visualizerStyle->setSelectedId(3, juce::sendNotificationSync);
    require(processor.state.getRawParameterValue("analyzerStyle")->load() == 2, "style selection reaches session state");
    visualizer->setToggleState(false, juce::sendNotificationSync);
    graph->refresh();
    require(processor.state.getRawParameterValue("analyzerEnabled")->load() == 0 && !visualizerStyle->isEnabled(), "visualizer toggle disables analysis and style selection");
    visualizer->setToggleState(true, juce::sendNotificationSync);
    visualizerStyle->setSelectedId(1, juce::sendNotificationSync);
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
    editor.reset();
    require(listener.ends == 1, "closing editor ends active host gesture");
    frequencyParameter->removeListener(&listener);
    if (argc > 1) {
        processor.setParameter("snap", 1);
        processor.setParameter("band0.freq", 65.4064f); processor.setParameter("band0.gain", 3.5f);
        processor.setParameter("band1.freq", 261.6256f); processor.setParameter("band1.gain", -4.0f);
        processor.setParameter("band2.freq", 880); processor.setParameter("band2.gain", 2.5f);
        processor.setParameter("band3.freq", 2793.826f); processor.setParameter("band3.gain", -3.0f);
        processor.setParameter("band4.freq", 8372.018f); processor.setParameter("band4.gain", 4.0f);
        editor.reset(processor.createEditor());
        graph = dynamic_cast<ResponseGraph*>(findType(findType, *editor, false));
        for (int style = 0; style < 3; ++style) {
            processor.setParameter("analyzerStyle", static_cast<float>(style));
            for (int block = 0; block < 512; ++block) {
                for (int i = 0; i < 512; ++i) {
                    const auto t = (block * 512 + i) / 48000.0;
                    double value = 0;
                    for (double hz : {65.4, 130.8, 261.6, 880., 1760., 2793.8, 8372.})
                        value += 0.05 * (0.7 + 0.3 * std::sin(t * hz / 100)) * std::sin(2 * juce::MathConstants<double>::pi * hz * t);
                    audio.setSample(0, i, static_cast<float>(value)); audio.setSample(1, i, static_cast<float>(value));
                }
                processor.processBlock(audio, midi);
                if (block % 4 == 0) { juce::Thread::sleep(42); graph->refresh(); }
            }
            const auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.5f);
            auto file = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
            if (style > 0) file = file.getSiblingFile(file.getFileNameWithoutExtension() + (style == 1 ? "-bars.png" : "-spectrogram.png"));
            file.deleteFile(); juce::FileOutputStream stream(file);
            require(stream.openedOk() && juce::PNGImageFormat().writeImageToStream(snapshot, stream), "live editor screenshot saved");
        }
    }
    std::cout << "PASS: processor audio, automation, state, bypass, stereo, editor controls\n";
}
