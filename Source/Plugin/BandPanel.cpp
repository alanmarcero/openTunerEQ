#include "BandPanel.h"
#include "Notes.h"
double NoteSlider::snapValue(double attempted, DragMode) {
    return ote::effectiveFrequency(attempted, processor.readSettings().snap, processor.displaySampleRate(), processor.referencePitch());
}
BandPanel::BandPanel(TunerProcessor& p, int band)
    : processor(p), index(band), colour(ui::bandColours[static_cast<size_t>(band)]), frequency(p),
      level(p.bandLevels[static_cast<size_t>(band)], ui::bandColours[static_cast<size_t>(band)]) {
    auto id = [band](const char* key) { return params::bandId(band, key); };
    setName("Band " + juce::String(band + 1));
    enabled.setClickingTogglesState(true); enabled.setColour(juce::TextButton::buttonOnColourId, colour);
    enabled.setTooltip("Enable or bypass this EQ band");
    note.setComponentID(id("note"));
    note.setColour(juce::ComboBox::textColourId, colour);
    for (int i = 0; i <= 10; ++i) octave.addItem(juce::String(i), i + 1);
    populateNotes();
    shape.addItemList(params::shapes, 1); slope.addItemList(params::slopes, 1); placement.addItemList(params::placements, 1);
    placement.setComponentID(id("placement"));
    frequency.setSliderStyle(juce::Slider::LinearHorizontal);
    frequency.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    frequency.setComponentID(id("freq"));
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
    gain.setDoubleClickReturnValue(true, 0); gain.setComponentID(id("gain"));
    q.setDoubleClickReturnValue(true, 0.7071); q.setComponentID(id("q"));
    for (auto* slider : {&dynamic, &drive}) {
        slider->setSliderStyle(juce::Slider::LinearBar);
        slider->setColour(juce::Slider::trackColourId, colour.withAlpha(0.45f));
        slider->setDoubleClickReturnValue(true, 0);
    }
    dynamic.setName("Dynamic"); dynamic.setComponentID(id("dyn"));
    drive.setName("Drive"); drive.setComponentID(id("drive"));
    hz.setFont(ui::font(17, true)); hz.setColour(juce::Label::textColourId, ui::ink); hz.setJustificationType(juce::Justification::centred);
    cents.setFont(ui::font(11)); cents.setColour(juce::Label::textColourId, ui::muted); cents.setJustificationType(juce::Justification::centred);
    for (juce::Component* c : std::initializer_list<juce::Component*>{&enabled, &placement, &note, &octave, &down, &up, &frequency, &gain, &q, &shape, &slope, &dynamic, &drive, &hz, &cents, &level}) addAndMakeVisible(c);
    level.setTooltip("Level at this band's note. The white line marks the dynamic threshold. Click to clear the clip light.");
    enabledAttachment = std::make_unique<params::State::ButtonAttachment>(p.state, id("enabled"), enabled);
    shapeAttachment = std::make_unique<params::State::ComboBoxAttachment>(p.state, id("shape"), shape);
    slopeAttachment = std::make_unique<params::State::ComboBoxAttachment>(p.state, id("slope"), slope);
    placementAttachment = std::make_unique<params::State::ComboBoxAttachment>(p.state, id("placement"), placement);
    frequencyAttachment = std::make_unique<params::State::SliderAttachment>(p.state, id("freq"), frequency);
    gainAttachment = std::make_unique<params::State::SliderAttachment>(p.state, id("gain"), gain);
    qAttachment = std::make_unique<params::State::SliderAttachment>(p.state, id("q"), q);
    dynamicAttachment = std::make_unique<params::State::SliderAttachment>(p.state, id("dyn"), dynamic);
    driveAttachment = std::make_unique<params::State::SliderAttachment>(p.state, id("drive"), drive);
    frequency.setTextValueSuffix("");
    frequency.textFromValueFunction = [this](double value) {
        return params::frequencyText(ote::effectiveFrequency(value, processor.readSettings().snap, processor.displaySampleRate(), processor.referencePitch()));
    };
    note.onChange = [this] { if (!updating) chooseNote((octave.getSelectedId()) * 12 + note.getSelectedId() - 1); };
    octave.onChange = note.onChange;
    down.onClick = [this] { chooseNote(ote::nearestNote(processor.bandFrequency(index), processor.referencePitch()) - 1); };
    up.onClick = [this] { chooseNote(ote::nearestNote(processor.bandFrequency(index), processor.referencePitch()) + 1); };
    down.setTitle("Lower band by one semitone");
    up.setTitle("Raise band by one semitone");
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
    hz.setText(params::frequencyText(frequencyValue), juce::dontSendNotification);
    if (!frequency.isMouseButtonDown()) frequency.updateText();
    const auto settings = processor.readSettings();
    const auto centsValue = ote::centsFromNote(frequencyValue, midi, processor.referencePitch());
    cents.setText(settings.snap ? "Locked to note" : ((centsValue >= 0 ? "+" : "") + juce::String(centsValue, 1) + " cents / free"), juce::dontSendNotification);
    const auto& b = settings.bands[static_cast<size_t>(index)];
    enabled.setButtonText(b.enabled ? "Enabled" : "Disabled");
    const juce::String bandOff = b.enabled ? "" : "Turn the band on to use it.";
    auto hint = [&bandOff](bool active, const juce::String& howToTurnOn) -> juce::String {
        if (bandOff.isNotEmpty()) return bandOff;
        if (active) return {};
        return howToTurnOn;
    };
    ui::showActive(placement, "Stereo, Left, Right, Mid, or Side. Mid and Side keep a centered signal unchanged when the band is flat. Stereo is the default.", bandOff);
    ui::showActive(note, "Choose the musical note directly.", bandOff);
    ui::showActive(octave, "Octave (C4 is middle C).", bandOff);
    ui::showActive(down, "Lower this band's frequency by one semitone.", bandOff);
    ui::showActive(up, "Raise this band's frequency by one semitone.", bandOff);
    ui::showActive(frequency, "Frequency in Hz. Double-click to reset.", bandOff);
    ui::showActive(shape, "Filter shape.", bandOff);
    ui::showActive(gain, "Boost or cut for bell and shelf shapes.", hint(ote::usesGain(b.shape), "Choose Bell, Low shelf or High shelf to use it."));
    ui::showActive(q, "Width / resonance. Higher Q is narrower.", hint(ote::usesQ(b), "Steep cuts use a fixed Butterworth Q. Choose 12 dB/oct to use it."));
    ui::showActive(slope, "Cut steepness in dB per octave. 96 is steep, not an ideal brick wall.", hint(ote::isCut(b.shape), "Choose High pass or Low pass to use it."));
    ui::showActive(dynamic, "Dynamic range. The band adds this much gain as its note rises above the dynamic threshold. Negative tames resonances.", bandOff);
    ui::showActive(drive, "Analog-style saturation of this band's boost or cut.", hint(std::abs(b.gainDb) > params::zeroDb || std::abs(b.dynamicDb) > params::zeroDb, "Set a gain or dynamic range to use it."));
    for (auto* label : {&hz, &cents}) label->setAlpha(b.enabled ? 1.0f : ui::inactiveAlpha);
    slope.setVisible(!ote::usesGain(b.shape)); dynamic.setVisible(ote::usesGain(b.shape)); drive.setVisible(ote::usesGain(b.shape));
    level.setVisible(ote::usesGain(b.shape)); level.setMarker(static_cast<float>(settings.dynamicThresholdDb));
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
    const auto caption = ui::muted.withMultipliedAlpha(enabled.getToggleState() ? 1.0f : ui::inactiveAlpha);
    ui::text(g, "Place", {16, 32, 70, 14}, 12, caption);
    ui::text(g, "Note", {16, 90, 100, 16}, 12, caption);
    ui::text(g, "Octave", {153, 90, 50, 16}, 12, caption);
    ui::text(g, "Gain", {15, 198, 83, 16}, 12, caption, juce::Justification::centred);
    ui::text(g, "Q / width", {115, 198, 83, 16}, 12, caption, juce::Justification::centred);
}
void BandPanel::resized() {
    const auto w = getWidth();
    enabled.setBounds(w - 90, 2, 75, 40);
    placement.setBounds(14, 46, w - 28, 40);
    note.setBounds(14, 106, 119, 40); octave.setBounds(w - 63, 106, 48, 40);
    hz.setBounds(50, 148, w - 100, 20); cents.setBounds(50, 166, w - 100, 16);
    down.setBounds(7, 146, 40, 40); up.setBounds(w - 47, 146, 40, 40);
    frequency.setBounds(14, 186, w - 28, 24);
    gain.setBounds(12, 214, (w - 28) / 2, 78); q.setBounds(w / 2 + 2, 214, (w - 28) / 2, 78);
    shape.setBounds(14, 300, w - 28, 40); slope.setBounds(14, 348, w - 28, 40);
    const auto bar = (w - 44) / 2;
    dynamic.setBounds(14, 348, bar, 40); level.setBounds(18 + bar, 350, 8, 36); drive.setBounds(w - 14 - bar, 348, bar, 40);
}
