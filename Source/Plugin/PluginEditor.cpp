#include "PluginEditor.h"
#include "Notes.h"
#include <BinaryData.h>
TunerEditor::TunerEditor(TunerProcessor& p) : AudioProcessorEditor(p), processor(p), graph(p), piano(p) {
    setLookAndFeel(&theme);
    faceplate = juce::ImageCache::getFromMemory(BinaryData::analogcyberfaceplate_png, BinaryData::analogcyberfaceplate_pngSize);
    tuningFork = juce::Drawable::createFromImageData(BinaryData::tuningfork_svg, BinaryData::tuningfork_svgSize);
    addAndMakeVisible(content);
    for (auto* button : {&snap, &flats, &bypass, &autoGain, &soloSelected}) { button->setClickingTogglesState(true); content.addAndMakeVisible(button); }
    snap.setComponentID("snap"); flats.setComponentID("flats"); bypass.setComponentID("bypass");
    autoGain.setComponentID("autoGain"); soloSelected.setComponentID("soloSelected");
    snap.setTooltip("Snap every band to the nearest chromatic note of the A4 reference, including host automation");
    flats.setTooltip("Display flats instead of sharps; pitch is unchanged");
    bypass.setTooltip("Bypass the whole EQ, including output gain");
    bypass.setColour(juce::TextButton::buttonOnColourId, ui::copper);
    autoGain.setTooltip("Automatically compensate output level for active bell and shelf boosts/cuts");
    soloSelected.setTooltip("Audition the currently selected EQ band in isolation");
    soloSelected.setColour(juce::TextButton::buttonOnColourId, ui::copper);
    scale.setComponentID("scale");
    scale.setSliderStyle(juce::Slider::LinearHorizontal); scale.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 24);
    scale.setTextValueSuffix("%"); scale.setDoubleClickReturnValue(true, 100); scale.setTooltip("Global EQ gain scale (0% flat to 200% exaggerated)");
    output.setSliderStyle(juce::Slider::LinearHorizontal); output.setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 24);
    output.setTextValueSuffix(" dB"); output.setDoubleClickReturnValue(true, 0); output.setTooltip("Output trim");
    snapAttachment = std::make_unique<params::State::ButtonAttachment>(p.state, "snap", snap);
    flatsAttachment = std::make_unique<params::State::ButtonAttachment>(p.state, "flats", flats);
    bypassAttachment = std::make_unique<params::State::ButtonAttachment>(p.state, "bypass", bypass);
    autoGainAttachment = std::make_unique<params::State::ButtonAttachment>(p.state, "autoGain", autoGain);
    soloSelectedAttachment = std::make_unique<params::State::ButtonAttachment>(p.state, "soloSelected", soloSelected);
    scaleAttachment = std::make_unique<params::State::SliderAttachment>(p.state, "scale", scale);
    outputAttachment = std::make_unique<params::State::SliderAttachment>(p.state, "output", output);
    reference.setSliderStyle(juce::Slider::LinearHorizontal); reference.setTextBoxStyle(juce::Slider::TextBoxRight, false, 66, 22);
    reference.setTextValueSuffix(" Hz"); reference.setTooltip("Tuning reference: the frequency of A4. Notes, snapping and cents follow it.");
    referenceAttachment = std::make_unique<params::State::SliderAttachment>(p.state, "a4", reference);
    for (juce::Component* c : std::initializer_list<juce::Component*>{&graph, &piano, &scale, &output, &reference, &selectedLabel, &rangeLabel, &octaveDown, &octaveUp}) content.addAndMakeVisible(c);
    selectedLabel.setFont(ui::font(16, true)); selectedLabel.setColour(juce::Label::textColourId, ui::ink);
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
    setResizable(true, true); setResizeLimits(1160, 840, 1740, 1260);
    if (auto* constrainer = getConstrainer()) constrainer->setFixedAspectRatio(1160.0 / 840.0);
    setSize(1160, 840);
    select(0); timerCallback(); startTimerHz(24);
}
TunerEditor::~TunerEditor() { stopTimer(); setLookAndFeel(nullptr); }
void TunerEditor::select(int band) {
    selected = band;
    processor.activeSelectedBand.store(band);
    graph.setSelected(band); piano.setSelected(band);
    for (int i = 0; i < 5; ++i) panels[static_cast<size_t>(i)]->setSelected(i == band);
    timerCallback();
}
void TunerEditor::timerCallback() {
    for (auto& panel : panels) panel->refresh();
    graph.refresh(); piano.repaint();
    const auto hz = processor.bandFrequency(selected); const auto midi = ote::nearestNote(hz, processor.referencePitch());
    selectedLabel.setText("Band " + juce::String(selected + 1) + "  /  " + juce::String(ote::noteName(midi, processor.usesFlats())), juce::dontSendNotification);
    selectedLabel.setColour(juce::Label::textColourId, ui::bandColours[static_cast<size_t>(selected)]);
    rangeLabel.setText(ui::frequencyText(hz), juce::dontSendNotification);
    octaveDown.setEnabled(ote::noteFrequency(midi - 12, processor.referencePitch()) >= 20);
    octaveUp.setEnabled(ote::noteFrequency(midi + 12, processor.referencePitch()) <= std::min(ote::maxFrequency, processor.displaySampleRate() * 0.49));
    meter = std::max(processor.peak.load(), meter * 0.88f);
    repaint();
}
void TunerEditor::resized() {
    content.setBounds(0, 0, 1160, 840); content.setTransform(juce::AffineTransform::scale(getWidth() / 1160.0f));
    snap.setBounds(348, 24, 114, 40); flats.setBounds(468, 24, 86, 40);
    soloSelected.setBounds(560, 24, 86, 40); autoGain.setBounds(652, 24, 86, 40);
    scale.setBounds(778, 26, 116, 36);
    output.setBounds(944, 26, 122, 36);
    bypass.setBounds(1072, 24, 68, 40);
    graph.setBounds(24, 94, 1112, 242);
    for (int i = 0; i < 5; ++i) panels[static_cast<size_t>(i)]->setBounds(24 + i * 224, 340, 216, 366);
    selectedLabel.setBounds(25, 735, 205, 25); rangeLabel.setBounds(25, 761, 205, 21);
    octaveDown.setBounds(25, 780, 90, 40); octaveUp.setBounds(123, 780, 90, 40);
    piano.setBounds(240, 739, 896, 74);
    reference.setBounds(966, 710, 170, 26);
}
void TunerEditor::paint(juce::Graphics& g) {
    g.fillAll(ui::background);
    g.addTransform(juce::AffineTransform::scale(getWidth() / 1160.0f));
    g.setOpacity(0.9f); g.drawImage(faceplate, juce::Rectangle<float>(0, 0, 1160, 840), juce::RectanglePlacement::fillDestination); g.setOpacity(1);
    g.setColour(ui::background.withAlpha(0.88f)); g.fillRect(0, 0, 1160, 840);
    g.setColour(ui::copper.withAlpha(0.35f)); g.drawHorizontalLine(82, 24, 1136);
    g.setColour(ui::muted.withAlpha(0.18f)); g.drawRoundedRectangle(8, 8, 1144, 824, 8, 1);
    g.setOpacity(1.0f);
    if (tuningFork) {
        const float decay = 1.0f - processor.artworkPhase();
        const float size = 50.0f + 10.0f * decay * decay;
        const auto bounds = juce::Rectangle<float>(size, size).withCentre({50.0f, 43.0f});
        tuningFork->drawWithin(g, bounds, juce::RectanglePlacement::centred, 0.78f + 0.22f * decay);
    }
    ui::text(g, "openTunerEQ", {91, 16, 250, 34}, 30, ui::ink, juce::Justification::centredLeft, true);
    ui::text(g, "VICANEK MATCHED ANALOG FILTER ENGINE", {93, 50, 250, 18}, 11, ui::copper);
    ui::text(g, "Scale", {742, 31, 36, 24}, 11, ui::muted);
    ui::text(g, "Out", {900, 31, 42, 24}, 11, ui::muted, juce::Justification::centredRight);
    const auto db = juce::Decibels::gainToDecibels(meter, -60.0f);
    g.setColour(ui::muted.withAlpha(0.2f)); g.fillRoundedRectangle(944, 65, 122, 5, 2.5f);
    g.setColour(db > 0 ? juce::Colour(0xffe98779) : juce::Colour(0xff48f0c8)); g.fillRoundedRectangle(944, 65, juce::jlimit(0.0f, 122.0f, (db + 60) / 60 * 122), 5, 2.5f);
    ui::text(g, "Choose a key for the selected band. The keyboard follows its octave.", {26, 709, 880, 24}, 13, ui::muted);
    ui::text(g, "Alan Marcero", {25, 820, 220, 15}, 10, ui::muted);
    ui::text(g, "A4", {930, 709, 30, 24}, 12, ui::muted, juce::Justification::centredRight, true);
    ui::text(g, "12-tone equal temperament   /   v" JucePlugin_VersionString, {660, 820, 476, 15}, 10, ui::muted, juce::Justification::centredRight);
}
