#include "BandPanel.h"
#include "Notes.h"
double NoteSlider::snapValue(double attempted, DragMode) {
    return ote::effectiveFrequency(attempted, processor.readSettings().snap, processor.displaySampleRate(), processor.referencePitch());
}
BandPanel::BandPanel(TunerProcessor& p, int band) : processor(p), index(band), colour(ui::bandColours[static_cast<size_t>(band)]), frequency(p) {
    auto id = [band](const char* key) { return params::bandId(band, key); };
    setName("Band " + juce::String(band + 1));
    enabled.setClickingTogglesState(true); enabled.setColour(juce::TextButton::buttonOnColourId, colour);
    enabled.setTooltip("Enable or bypass this band");
    note.setComponentID(id("note")); note.setTooltip("Choose the musical note directly");
    note.setColour(juce::ComboBox::textColourId, colour);
    octave.setTooltip("Octave (C4 is middle C)");
    for (int i = 0; i <= 10; ++i) octave.addItem(juce::String(i), i + 1);
    populateNotes();
    shape.addItemList(params::shapes, 1); slope.addItemList(params::slopes, 1);
    shape.setTooltip("Filter shape"); slope.setTooltip("Cut steepness in dB per octave. 96 is steep, not an ideal brick wall.");
    frequency.setSliderStyle(juce::Slider::LinearHorizontal);
    frequency.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 126, 18);
    frequency.setComponentID(id("freq")); frequency.setTooltip("Frequency in Hz. Double-click to reset; enter a number to tune precisely.");
    frequency.setNumDecimalPlacesToDisplay(2);
    frequency.setTextValueSuffix(" Hz");
    frequency.setDoubleClickReturnValue(true, p.state.getParameter(id("freq"))->convertFrom0to1(p.state.getParameter(id("freq"))->getDefaultValue()));
    for (auto* slider : {&gain, &q}) {
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 86, 20);
        slider->setColour(juce::Slider::rotarySliderFillColourId, colour);
        slider->setRotaryParameters(juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    }
    frequency.setColour(juce::Slider::rotarySliderFillColourId, colour);
    gain.setTextValueSuffix(" dB"); gain.setDoubleClickReturnValue(true, 0); gain.setComponentID(id("gain"));
    gain.setTooltip("Boost or cut for bell and shelf shapes");
    q.setDoubleClickReturnValue(true, 0.7071); q.setComponentID(id("q")); q.setTooltip("Width / resonance. Higher Q is narrower. Steep cuts use fixed Butterworth Q.");
    hz.setFont(ui::font(17, true)); hz.setColour(juce::Label::textColourId, ui::ink); hz.setJustificationType(juce::Justification::centred);
    cents.setFont(ui::font(11)); cents.setColour(juce::Label::textColourId, ui::muted); cents.setJustificationType(juce::Justification::centred);
    for (juce::Component* c : std::initializer_list<juce::Component*>{&enabled, &note, &octave, &down, &up, &frequency, &gain, &q, &shape, &slope, &hz, &cents}) addAndMakeVisible(c);
    enabledAttachment = std::make_unique<params::State::ButtonAttachment>(p.state, id("enabled"), enabled);
    shapeAttachment = std::make_unique<params::State::ComboBoxAttachment>(p.state, id("shape"), shape);
    slopeAttachment = std::make_unique<params::State::ComboBoxAttachment>(p.state, id("slope"), slope);
    frequencyAttachment = std::make_unique<params::State::SliderAttachment>(p.state, id("freq"), frequency);
    gainAttachment = std::make_unique<params::State::SliderAttachment>(p.state, id("gain"), gain);
    qAttachment = std::make_unique<params::State::SliderAttachment>(p.state, id("q"), q);
    frequency.setTextValueSuffix("");
    frequency.textFromValueFunction = [this](double value) {
        return ui::frequencyText(ote::effectiveFrequency(value, processor.readSettings().snap, processor.displaySampleRate(), processor.referencePitch()));
    };
    note.onChange = [this] { if (!updating) chooseNote((octave.getSelectedId()) * 12 + note.getSelectedId() - 1); };
    octave.onChange = note.onChange;
    down.onClick = [this] { chooseNote(ote::nearestNote(processor.bandFrequency(index), processor.referencePitch()) - 1); };
    up.onClick = [this] { chooseNote(ote::nearestNote(processor.bandFrequency(index), processor.referencePitch()) + 1); };
    down.setTooltip("One semitone down"); up.setTooltip("One semitone up");
    addMouseListener(this, true);
    refresh();
}
void BandPanel::populateNotes() {
    updating = true; lastFlats = processor.usesFlats();
    const auto old = note.getSelectedId(); note.clear(juce::dontSendNotification);
    for (int i = 0; i < 12; ++i) note.addItem(juce::String(ote::noteName(60 + i, lastFlats)).dropLastCharacters(1), i + 1);
    note.setSelectedId(old, juce::dontSendNotification); updating = false;
}
void BandPanel::chooseNote(int midi) {
    const auto hzValue = ote::noteFrequency(midi, processor.referencePitch());
    if (hzValue < ote::minFrequency || hzValue > std::min(ote::maxFrequency, processor.displaySampleRate() * 0.49)) { refresh(); return; }
    processor.setParameter(params::bandId(index, "freq"), static_cast<float>(hzValue));
    if (onSelect) onSelect(index);
    refresh();
}
void BandPanel::refresh() {
    if (lastFlats != processor.usesFlats()) populateNotes();
    updating = true;
    const auto frequencyValue = processor.bandFrequency(index);
    const auto midi = ote::nearestNote(frequencyValue, processor.referencePitch());
    note.setSelectedId((midi % 12) + 1, juce::dontSendNotification);
    octave.setSelectedId(midi / 12, juce::dontSendNotification);
    const auto max = std::min(ote::maxFrequency, processor.displaySampleRate() * 0.49);
    for (int pitch = 0; pitch < 12; ++pitch) {
        const auto f = ote::noteFrequency((midi / 12) * 12 + pitch, processor.referencePitch());
        note.setItemEnabled(pitch + 1, f >= 20 && f <= max);
    }
    for (int oct = 0; oct <= 10; ++oct) {
        const auto f = ote::noteFrequency((oct + 1) * 12 + midi % 12, processor.referencePitch());
        octave.setItemEnabled(oct + 1, f >= 20 && f <= max);
    }
    down.setEnabled(ote::noteFrequency(midi - 1, processor.referencePitch()) >= 20);
    up.setEnabled(ote::noteFrequency(midi + 1, processor.referencePitch()) <= max);
    hz.setText(ui::frequencyText(frequencyValue), juce::dontSendNotification);
    if (!frequency.isMouseButtonDown()) frequency.updateText();
    const auto settings = processor.readSettings();
    const auto centsValue = ote::centsFromNote(frequencyValue, midi, processor.referencePitch());
    cents.setText(settings.snap ? "Locked to note" : ((centsValue >= 0 ? "+" : "") + juce::String(centsValue, 1) + " cents / free"), juce::dontSendNotification);
    const auto& b = settings.bands[static_cast<size_t>(index)];
    enabled.setButtonText(b.enabled ? "On" : "Off");
    gain.setEnabled(ote::usesGain(b.shape)); q.setEnabled(ote::usesQ(b)); slope.setEnabled(ote::isCut(b.shape));
    updating = false;
    repaint();
}
void BandPanel::setSelected(bool value) { selected = value; repaint(); }
void BandPanel::mouseDown(const juce::MouseEvent&) { if (onSelect) onSelect(index); }
void BandPanel::paint(juce::Graphics& g) {
    auto area = getLocalBounds().toFloat().reduced(1);
    g.setColour(ui::panel.withAlpha(0.97f)); g.fillRoundedRectangle(area, 9);
    g.setColour(selected ? colour.withAlpha(0.8f) : ui::muted.withAlpha(0.18f)); g.drawRoundedRectangle(area, 9, selected ? 1.5f : 1.0f);
    g.setColour(colour.withAlpha(enabled.getToggleState() ? 1.0f : 0.25f)); g.fillRoundedRectangle(15, 18, 6, 6, 3);
    ui::text(g, "Band " + juce::String(index + 1), {28, 10, 95, 22}, 13, ui::ink, juce::Justification::centredLeft, true);
    ui::text(g, "Octave", {153, 36, 50, 16}, 10, ui::muted);
    ui::text(g, "Gain", {15, 166, 83, 18}, 11, ui::muted, juce::Justification::centred);
    ui::text(g, "Q / width", {115, 166, 83, 18}, 11, ui::muted, juce::Justification::centred);
}
void BandPanel::resized() {
    const auto w = getWidth();
    enabled.setBounds(w - 65, 10, 50, 23);
    note.setBounds(14, 39, 119, 43); octave.setBounds(w - 63, 54, 48, 26);
    hz.setBounds(35, 84, w - 70, 25); cents.setBounds(34, 110, w - 68, 18);
    down.setBounds(14, 88, 23, 23); up.setBounds(w - 37, 88, 23, 23);
    frequency.setBounds(14, 129, w - 28, 36);
    gain.setBounds(12, 185, (w - 28) / 2, 82); q.setBounds(w / 2 + 2, 185, (w - 28) / 2, 82);
    shape.setBounds(14, 275, w - 28, 26); slope.setBounds(14, 307, w - 28, 24);
}
