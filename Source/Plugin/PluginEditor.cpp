#include "PluginEditor.h"
#include "Notes.h"
#include <BinaryData.h>
TunerEditor::TunerEditor(TunerProcessor& p) : AudioProcessorEditor(p), processor(p), graph(p), piano(p) {
    setLookAndFeel(&theme);
    faceplate = juce::ImageCache::getFromMemory(BinaryData::analogcyberfaceplate_png, BinaryData::analogcyberfaceplate_pngSize);
    emblem = juce::ImageCache::getFromMemory(BinaryData::emblem_png, BinaryData::emblem_pngSize);
    addAndMakeVisible(content);
    for (auto* button : {&snap, &flats, &bypass}) { button->setClickingTogglesState(true); content.addAndMakeVisible(button); }
    snap.setTooltip("Snap every band to the nearest chromatic note of the A4 reference, including host automation");
    flats.setTooltip("Display flats instead of sharps; pitch is unchanged");
    bypass.setTooltip("Bypass the whole EQ, including output gain");
    bypass.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffdd8e83));
    output.setSliderStyle(juce::Slider::LinearHorizontal); output.setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 24);
    output.setTextValueSuffix(" dB"); output.setDoubleClickReturnValue(true, 0); output.setTooltip("Output trim");
    snapAttachment = std::make_unique<params::State::ButtonAttachment>(p.state, "snap", snap);
    flatsAttachment = std::make_unique<params::State::ButtonAttachment>(p.state, "flats", flats);
    bypassAttachment = std::make_unique<params::State::ButtonAttachment>(p.state, "bypass", bypass);
    outputAttachment = std::make_unique<params::State::SliderAttachment>(p.state, "output", output);
    reference.setSliderStyle(juce::Slider::LinearHorizontal); reference.setTextBoxStyle(juce::Slider::TextBoxRight, false, 66, 22);
    reference.setTextValueSuffix(" Hz"); reference.setTooltip("Tuning reference: the frequency of A4. Notes, snapping and cents follow it.");
    referenceAttachment = std::make_unique<params::State::SliderAttachment>(p.state, "a4", reference);
    for (juce::Component* c : std::initializer_list<juce::Component*>{&graph, &piano, &output, &reference, &selectedLabel, &rangeLabel, &octaveDown, &octaveUp}) content.addAndMakeVisible(c);
    selectedLabel.setFont(ui::font(16, true)); selectedLabel.setColour(juce::Label::textColourId, ui::ink);
    rangeLabel.setFont(ui::font(12)); rangeLabel.setColour(juce::Label::textColourId, ui::muted);
    for (int i = 0; i < 5; ++i) {
        panels[static_cast<size_t>(i)] = std::make_unique<BandPanel>(p, i);
        panels[static_cast<size_t>(i)]->onSelect = [this](int band) { select(band); };
        content.addAndMakeVisible(*panels[static_cast<size_t>(i)]);
    }
    graph.onSelect = [this](int band) { select(band); };
    piano.onNote = [this](int midi) { panels[static_cast<size_t>(selected)]->chooseNote(midi); };
    octaveDown.onClick = [this] { panels[static_cast<size_t>(selected)]->chooseNote(ote::nearestNote(processor.bandFrequency(selected), processor.referencePitch()) - 12); };
    octaveUp.onClick = [this] { panels[static_cast<size_t>(selected)]->chooseNote(ote::nearestNote(processor.bandFrequency(selected), processor.referencePitch()) + 12); };
    setResizable(true, true); setResizeLimits(928, 672, 1740, 1260);
    if (auto* constrainer = getConstrainer()) constrainer->setFixedAspectRatio(1160.0 / 840.0);
    setSize(1160, 840);
    select(0); timerCallback(); startTimerHz(24);
}
TunerEditor::~TunerEditor() { stopTimer(); setLookAndFeel(nullptr); }
void TunerEditor::select(int band) {
    selected = band; graph.setSelected(band); piano.setSelected(band);
    for (int i = 0; i < 5; ++i) panels[static_cast<size_t>(i)]->setSelected(i == band);
    timerCallback();
}
void TunerEditor::timerCallback() {
    for (auto& panel : panels) panel->refresh();
    graph.repaint(); piano.repaint();
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
    snap.setBounds(433, 27, 126, 31); flats.setBounds(567, 27, 86, 31); bypass.setBounds(661, 27, 80, 31);
    output.setBounds(842, 30, 215, 27);
    graph.setBounds(24, 94, 1112, 242);
    for (int i = 0; i < 5; ++i) panels[static_cast<size_t>(i)]->setBounds(24 + i * 224, 350, 216, 346);
    selectedLabel.setBounds(25, 737, 205, 25); rangeLabel.setBounds(25, 763, 205, 21);
    octaveDown.setBounds(25, 791, 72, 24); octaveUp.setBounds(105, 791, 72, 24);
    piano.setBounds(240, 739, 896, 74);
    reference.setBounds(966, 705, 170, 26);
}
void TunerEditor::paint(juce::Graphics& g) {
    g.fillAll(ui::background);
    g.addTransform(juce::AffineTransform::scale(getWidth() / 1160.0f));
    g.setOpacity(0.9f); g.drawImage(faceplate, juce::Rectangle<float>(0, 0, 1160, 840), juce::RectanglePlacement::fillDestination); g.setOpacity(1);
    g.setColour(ui::background.withAlpha(0.26f)); g.fillRect(0, 84, 1160, 756);
    g.setColour(ui::copper.withAlpha(0.35f)); g.drawHorizontalLine(82, 24, 1136);
    g.setColour(ui::muted.withAlpha(0.18f)); g.drawRoundedRectangle(8, 8, 1144, 824, 8, 1);
    g.setOpacity(1.0f);
    g.drawImageWithin(emblem, 21, 12, 58, 62, juce::RectanglePlacement::centred, false);
    ui::text(g, "openTunerEQ", {91, 16, 310, 34}, 30, ui::ink, juce::Justification::centredLeft, true);
    ui::text(g, "Equalize in tune.", {93, 54, 280, 18}, 13, ui::muted);
    ui::text(g, "Output", {787, 31, 55, 24}, 12, ui::muted);
    const auto db = juce::Decibels::gainToDecibels(meter, -60.0f);
    g.setColour(ui::muted.withAlpha(0.2f)); g.fillRoundedRectangle(1073, 26, 63, 7, 3);
    g.setColour(db > 0 ? juce::Colour(0xffe98779) : ui::bandColours[2]); g.fillRoundedRectangle(1073, 26, juce::jlimit(0.0f, 63.0f, (db + 60) / 60 * 63), 7, 3);
    ui::text(g, meter < 0.001f ? "-inf dB" : juce::String(db, 1) + " dB", {1060, 38, 76, 19}, 11, ui::muted, juce::Justification::centredRight);
    ui::text(g, "Play the frequency.", {26, 705, 200, 24}, 15, ui::ink, juce::Justification::centredLeft, true);
    ui::text(g, "Choose a key for the selected band. The keyboard follows its octave.", {240, 709, 850, 20}, 12, ui::muted);
    ui::text(g, "Alan Marcero", {25, 820, 220, 15}, 10, ui::muted);
    ui::text(g, "A4", {934, 709, 28, 20}, 12, ui::muted, juce::Justification::centredRight);
    ui::text(g, "12-tone equal temperament   /   v1.1.0", {660, 820, 476, 15}, 10, ui::muted, juce::Justification::centredRight);
}
