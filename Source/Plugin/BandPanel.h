#pragma once
#include "PluginProcessor.h"
#include "Theme.h"
class NoteSlider final : public juce::Slider {
public:
    explicit NoteSlider(TunerProcessor& p) : processor(p) {}
    double snapValue(double attempted, DragMode) override;
private:
    TunerProcessor& processor;
};
class BandPanel final : public juce::Component {
public:
    BandPanel(TunerProcessor&, int band);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void refresh();
    void setSelected(bool selected);
    void chooseNote(int midi);
    std::function<void(int)> onSelect;
private:
    TunerProcessor& processor;
    int index;
    bool selected = false, updating = false;
    juce::Colour colour;
    juce::TextButton enabled {"On"}, down {"-"}, up {"+"};
    juce::ComboBox note, octave, shape, slope;
    NoteSlider frequency;
    juce::Slider gain, q;
    juce::Label hz, cents;
    std::unique_ptr<params::State::ButtonAttachment> enabledAttachment;
    std::unique_ptr<params::State::ComboBoxAttachment> shapeAttachment, slopeAttachment;
    std::unique_ptr<params::State::SliderAttachment> frequencyAttachment, gainAttachment, qAttachment;
    bool lastFlats = false;
    void populateNotes();
};
