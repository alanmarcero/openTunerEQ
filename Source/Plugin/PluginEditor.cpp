#include "PluginEditor.h"
#include "Notes.h"
#include <BinaryData.h>
namespace {
constexpr int designWidth = 1160, designHeight = 900;
constexpr int allPanels = 0x1f, pianoRefresh = 1 << 5, anyRefresh = 1 << 6, everything = allPanels | pianoRefresh | anyRefresh;
}
Faceplate::Faceplate() : image(juce::ImageCache::getFromMemory(BinaryData::analogcyberfaceplate_png, BinaryData::analogcyberfaceplate_pngSize)) {
    setComponentID("faceplate"); setOpaque(true); setBufferedToImage(true); setInterceptsMouseClicks(false, false);
}
void Faceplate::paint(juce::Graphics& g) {
    g.fillAll(ui::background);
    g.setOpacity(0.9f); g.drawImage(image, getLocalBounds().toFloat(), juce::RectanglePlacement::fillDestination); g.setOpacity(1);
    g.setColour(ui::background.withAlpha(0.88f)); g.fillRect(getLocalBounds());
    g.setColour(ui::copper.withAlpha(0.35f)); g.drawHorizontalLine(64, 24, 1136);
    g.setColour(ui::muted.withAlpha(0.18f)); g.drawRoundedRectangle(8, 8, 1144, 884, 8, 1);
    ui::text(g, "openTunerEQ", {84, 8, 250, 28}, 26, ui::ink, juce::Justification::centredLeft, true);
    ui::text(g, "VICANEK MATCHED ANALOG FILTER ENGINE", {86, 36, 250, 16}, 11, ui::copper);
    ui::text(g, "Scale", {742, 22, 36, 24}, 11, ui::muted);
    ui::text(g, "Out", {900, 22, 42, 24}, 11, ui::muted, juce::Justification::centredRight);
    ui::text(g, "Dynamic threshold", {984, 70, 152, 14}, 11, ui::muted);
    ui::text(g, "Alan Marcero", {25, 876, 220, 15}, 10, ui::muted);
    ui::text(g, "A4", {25, 846, 24, 24}, 12, ui::muted, juce::Justification::centredLeft, true);
    ui::text(g, "12-tone equal temperament   /   v" JucePlugin_VersionString, {660, 876, 476, 15}, 10, ui::muted, juce::Justification::centredRight);
}
TuningFork::TuningFork(TunerProcessor& p) : processor(p), drawable(juce::Drawable::createFromImageData(BinaryData::tuningfork_svg, BinaryData::tuningfork_svgSize)) {
    setInterceptsMouseClicks(false, false);
}
void TuningFork::paint(juce::Graphics& g) {
    if (drawable == nullptr) return;
    const float decay = 1.0f - processor.artworkPhase();
    const float size = 46.0f + 8.0f * decay * decay;
    drawable->drawWithin(g, juce::Rectangle<float>(size, size).withCentre(getLocalBounds().toFloat().getCentre()), juce::RectanglePlacement::centred, 0.78f + 0.22f * decay);
}
TunerEditor::TunerEditor(TunerProcessor& p) : AudioProcessorEditor(p), processor(p), fork(p), meter(p.peak, juce::Colour(0xff48f0c8)), graph(p), piano(p), suite(p) {
    setLookAndFeel(&theme);
    addAndMakeVisible(content);
    for (juce::Component* c : std::initializer_list<juce::Component*>{&faceplate, &fork, &meter}) content.addAndMakeVisible(c);
    meter.setTooltip("Output level. Click to clear the clip light.");
    for (auto* button : {&snap, &flats, &bypass, &autoGain, &soloSelected, &phaseComp}) { button->setClickingTogglesState(true); content.addAndMakeVisible(button); }
    octaveDown.setTitle("Tune down one octave"); octaveUp.setTitle("Tune up one octave"); qMode.setTitle("Bell Q law");
    output.setTitle("Output gain"); reference.setTitle("A4 reference"); scale.setTitle("Scale"); dynThreshold.setTitle("Dynamic threshold");
    snap.setComponentID("snap"); flats.setComponentID("flats"); bypass.setComponentID("bypass");
    autoGain.setComponentID("autoGain"); soloSelected.setComponentID("soloSelected");
    snap.setTooltip("Snap every band to the nearest chromatic note of the A4 reference, including host automation");
    flats.setTooltip("Display flats instead of sharps; pitch is unchanged");
    bypass.setTooltip("Bypass the whole EQ, including output gain");
    bypass.setColour(juce::TextButton::buttonOnColourId, ui::warning);
    soloSelected.setTooltip("Audition the currently selected EQ band in isolation");
    soloSelected.setColour(juce::TextButton::buttonOnColourId, ui::warning);
    phaseComp.setComponentID("phaseComp");
    qMode.setComponentID("qMode"); qMode.addItemList(params::qModes, 1);
    dynThreshold.setComponentID("dynThreshold");
    dynThreshold.setSliderStyle(juce::Slider::LinearHorizontal); dynThreshold.setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 24);
    dynThreshold.setDoubleClickReturnValue(true, -24);
    scale.setComponentID("scale");
    scale.setSliderStyle(juce::Slider::LinearHorizontal); scale.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 24);
    scale.setDoubleClickReturnValue(true, 100); scale.setTooltip("Global EQ gain scale (0% flat to 200% exaggerated)");
    output.setSliderStyle(juce::Slider::LinearHorizontal); output.setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 24);
    output.setDoubleClickReturnValue(true, 0); output.setTooltip("Output trim");
    snapAttachment = std::make_unique<params::State::ButtonAttachment>(p.state, "snap", snap);
    flatsAttachment = std::make_unique<params::State::ButtonAttachment>(p.state, "flats", flats);
    bypassAttachment = std::make_unique<params::State::ButtonAttachment>(p.state, "bypass", bypass);
    autoGainAttachment = std::make_unique<params::State::ButtonAttachment>(p.state, "autoGain", autoGain);
    soloSelectedAttachment = std::make_unique<params::State::ButtonAttachment>(p.state, "soloSelected", soloSelected);
    phaseCompAttachment = std::make_unique<params::State::ButtonAttachment>(p.state, "phaseComp", phaseComp);
    qModeAttachment = std::make_unique<params::State::ComboBoxAttachment>(p.state, "qMode", qMode);
    dynThresholdAttachment = std::make_unique<params::State::SliderAttachment>(p.state, "dynThreshold", dynThreshold);
    scaleAttachment = std::make_unique<params::State::SliderAttachment>(p.state, "scale", scale);
    outputAttachment = std::make_unique<params::State::SliderAttachment>(p.state, "output", output);
    reference.setComponentID("a4");
    reference.setSliderStyle(juce::Slider::LinearHorizontal); reference.setTextBoxStyle(juce::Slider::TextBoxRight, false, 66, 22);
    reference.setTooltip("Tuning reference: the frequency of A4. Notes, snapping and cents follow it.");
    referenceAttachment = std::make_unique<params::State::SliderAttachment>(p.state, "a4", reference);
    for (juce::Component* c : std::initializer_list<juce::Component*>{&graph, &piano, &suite, &qMode, &dynThreshold, &scale, &output, &reference, &selectedLabel, &rangeLabel, &octaveDown, &octaveUp}) content.addAndMakeVisible(c);
    selectedLabel.setComponentID("selectedBand"); selectedLabel.setFont(ui::font(16, true)); selectedLabel.setColour(juce::Label::textColourId, ui::ink);
    rangeLabel.setFont(ui::font(12)); rangeLabel.setColour(juce::Label::textColourId, ui::muted);
    for (int i = 0; i < 5; ++i) {
        panels[static_cast<size_t>(i)] = std::make_unique<BandPanel>(p, i);
        panels[static_cast<size_t>(i)]->onSelect = [this](int band) { select(band); };
        content.addAndMakeVisible(*panels[static_cast<size_t>(i)]);
    }
    graph.onSelect = [this](int band) { select(band); };
    piano.onNote = [this](int midi) { panels[static_cast<size_t>(selected)]->chooseNote(midi); };
    octaveDown.setTooltip("Tune the selected band down one octave");
    octaveUp.setTooltip("Tune the selected band up one octave");
    octaveDown.onClick = [this] { panels[static_cast<size_t>(selected)]->chooseNote(ote::nearestNote(processor.bandFrequency(selected), processor.referencePitch()) - 12); };
    octaveUp.onClick = [this] { panels[static_cast<size_t>(selected)]->chooseNote(ote::nearestNote(processor.bandFrequency(selected), processor.referencePitch()) + 12); };
    // setResizeLimits calls resized(), which overwrites the saved width.
    const auto width = juce::jlimit(designWidth * 7 / 10, designWidth * 3 / 2, processor.editorWidth.load());
    setResizable(true, true); setResizeLimits(designWidth * 7 / 10, designHeight * 7 / 10, designWidth * 3 / 2, designHeight * 3 / 2);
    if (auto* constrainer = getConstrainer()) constrainer->setFixedAspectRatio(static_cast<double>(designWidth) / designHeight);
    setSize(width, juce::roundToInt(width * static_cast<double>(designHeight) / designWidth));
    setWantsKeyboardFocus(true);
    // Snap, flats, A4, Scale and the threshold change what every panel shows.
    for (auto* parameter : processor.getParameters()) {
        const auto& id = dynamic_cast<juce::RangedAudioParameter&>(*parameter).paramID;
        auto mask = anyRefresh;
        if (id.startsWith("band")) mask |= 1 << (id[4] - '0');
        if (id == "snap" || id == "flats" || id == "a4" || id == "scale" || id == "dynThreshold") mask |= allPanels;
        if (id.endsWith(".freq") || id.endsWith(".enabled") || id == "snap" || id == "flats" || id == "a4") mask |= pianoRefresh;
        refreshMasks.push_back(mask);
    }
    for (auto* parameter : processor.getParameters()) parameter->addListener(this);
    select(processor.activeSelectedBand.load()); startTimerHz(24);
}
TunerEditor::~TunerEditor() {
    for (auto* parameter : processor.getParameters()) parameter->removeListener(this);
    stopTimer(); setLookAndFeel(nullptr);
}
// Host automation can call this on the audio thread.
void TunerEditor::parameterValueChanged(int index, float) { pendingRefresh.fetch_or(refreshMasks[static_cast<size_t>(index)]); }
void TunerEditor::select(int band) {
    selected = band;
    processor.activeSelectedBand.store(band);
    graph.setSelected(band); piano.setSelected(band);
    for (int i = 0; i < 5; ++i) panels[static_cast<size_t>(i)]->setSelected(i == band);
    pendingRefresh.fetch_or(everything);
    timerCallback();
}
// Panels, piano and labels refresh only after a parameter change.
void TunerEditor::timerCallback() {
    // A state load can change the selected band, and Solo follows that band.
    if (const auto band = processor.activeSelectedBand.load(); band != selected) { select(band); return; }
    suite.refresh();
    graph.animate();
    const auto phase = processor.artworkPhase();
    if (forkPhase != phase) { forkPhase = phase; fork.repaint(); }
    auto changed = pendingRefresh.exchange(0);
    if (shownRate != processor.displaySampleRate()) changed = everything;
    if (changed == 0) return;
    shownRate = processor.displaySampleRate();
    for (int i = 0; i < 5; ++i) if ((changed & (1 << i)) != 0) panels[static_cast<size_t>(i)]->refresh();
    graph.refresh();
    if ((changed & pianoRefresh) != 0) piano.repaint();
    const auto hz = processor.bandFrequency(selected); const auto midi = ote::nearestNote(hz, processor.referencePitch());
    selectedLabel.setText("Band " + juce::String(selected + 1) + "  /  " + juce::String(ote::noteName(midi, processor.usesFlats())), juce::dontSendNotification);
    selectedLabel.setColour(juce::Label::textColourId, ui::bandColours[static_cast<size_t>(selected)]);
    rangeLabel.setText(params::frequencyText(hz), juce::dontSendNotification);
    octaveDown.setEnabled(ote::noteFrequency(midi - 12, processor.referencePitch()) >= 20);
    octaveUp.setEnabled(ote::noteFrequency(midi + 12, processor.referencePitch()) <= std::min(ote::maxFrequency, processor.displaySampleRate() * 0.49));
    const auto settings = processor.readSettings();
    std::array<ote::BandSettings, 5> bands;
    for (size_t i = 0; i < bands.size(); ++i) bands[i] = ote::audibleBand(settings, i);
    const bool oneSidedCut = std::any_of(bands.begin(), bands.end(), [](const ote::BandSettings& b) {
        return b.enabled && ote::isOneSidedCut(b);
    });
    const bool shapedBell = std::any_of(bands.begin(), bands.end(), [](const ote::BandSettings& b) {
        return b.enabled && ote::usesQLaw(b);
    });
    const bool dynamicBand = std::any_of(bands.begin(), bands.end(), [](const ote::BandSettings& b) { return b.enabled && ote::usesDynamics(b); });
    ui::showActive(phaseComp, "Give the untouched channel of a Left, Right, Mid or Side cut the cut's passband phase.",
                   oneSidedCut ? "" : "Place a high-pass or low-pass band on Left, Right, Mid or Side to use it.");
    ui::showActive(qMode, "Bell width law. Symmetric matches boost and cut widths, Constant keeps Q fixed, Proportional narrows as gain grows.",
                   shapedBell ? "" : "Boost or cut a Bell band to use it.");
    ui::showActive(dynThreshold, "Level at which dynamic bands start to act.", dynamicBand ? "" : "Set a band's dynamic range to use it.");
    ui::showActive(autoGain, "Match the output's K-weighted loudness to the input so EQ moves are heard at equal level.",
                   settings.soloBand < 0 ? "" : "Solo Band turns it off. Turn off Solo Band to use it.");
}
void TunerEditor::resized() {
    content.setBounds(0, 0, designWidth, designHeight);
    content.setTransform(juce::RectanglePlacement(juce::RectanglePlacement::centred).getTransformToFit(content.getLocalBounds().toFloat(), getLocalBounds().toFloat()));
    processor.editorWidth = getWidth();
    faceplate.setBounds(content.getLocalBounds()); fork.setBounds(20, 8, 56, 56); meter.setBounds(944, 56, 122, 5);
    snap.setBounds(348, 16, 114, 40); flats.setBounds(468, 16, 86, 40);
    soloSelected.setBounds(560, 16, 86, 40); autoGain.setBounds(652, 16, 86, 40);
    scale.setBounds(778, 18, 116, 36);
    output.setBounds(944, 18, 122, 36);
    bypass.setBounds(1072, 16, 68, 40);
    suite.setBounds(24, 70, 700, 40);
    phaseComp.setBounds(732, 70, 100, 40); qMode.setBounds(840, 70, 140, 40);
    dynThreshold.setBounds(984, 84, 152, 26);
    graph.setBounds(24, 118, 1112, 228);
    for (int i = 0; i < 5; ++i) panels[static_cast<size_t>(i)]->setBounds(24 + i * 224, 354, 216, 400);
    selectedLabel.setBounds(25, 760, 205, 20); rangeLabel.setBounds(25, 780, 205, 16);
    octaveDown.setBounds(25, 800, 90, 40); octaveUp.setBounds(123, 800, 90, 40);
    reference.setBounds(50, 846, 182, 24);
    piano.setBounds(240, 760, 896, 110);
}
bool TunerEditor::keyPressed(const juce::KeyPress& key) {
    const auto command = juce::ModifierKeys::commandModifier;
    if (dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()) != nullptr) return false;
    if (key == juce::KeyPress('z', command, 0)) { processor.history().undo(); suite.refresh(); return true; }
    if (key == juce::KeyPress('z', command | juce::ModifierKeys::shiftModifier, 0)
        || key == juce::KeyPress('y', command, 0)) {
        processor.history().redo(); suite.refresh(); return true;
    }
    return false;
}
void TunerEditor::paint(juce::Graphics& g) { g.fillAll(ui::background); }
