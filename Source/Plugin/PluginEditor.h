#pragma once
#include "BandPanel.h"
#include "ResponseGraph.h"
class TunerEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit TunerEditor(TunerProcessor&);
    ~TunerEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    TunerProcessor& processor;
    ui::Theme theme;
    juce::Component content;
    juce::Image faceplate, emblem;
    juce::TextButton snap {"Snap to notes"}, flats {"Flats (b)"}, bypass {"Bypass"}, octaveDown {"- Oct"}, octaveUp {"+ Oct"};
    juce::Slider output, reference;
    juce::Label selectedLabel, rangeLabel;
    ResponseGraph graph;
    PianoStrip piano;
    std::array<std::unique_ptr<BandPanel>, 5> panels;
    std::unique_ptr<params::State::ButtonAttachment> snapAttachment, flatsAttachment, bypassAttachment;
    std::unique_ptr<params::State::SliderAttachment> outputAttachment, referenceAttachment;
    juce::TooltipWindow tooltips {this, 700};
    int selected = 0;
    float meter = 0;
    void select(int band);
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TunerEditor)
};
