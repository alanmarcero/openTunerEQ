#pragma once
#include "BandPanel.h"
#include "ResponseGraph.h"
#include "SuiteBar.h"
class TunerEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit TunerEditor(TunerProcessor&);
    ~TunerEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
private:
    TunerProcessor& processor;
    ui::Theme theme;
    juce::Component content;
    juce::Image faceplate;
    std::unique_ptr<juce::Drawable> tuningFork;
    juce::TextButton snap {"Snap to notes"}, flats {"Use flats"}, bypass {"Bypass"}, octaveDown {"- Octave"}, octaveUp {"+ Octave"};
    juce::TextButton autoGain {"Auto Gain"}, soloSelected {"Solo Band"}, phaseComp {"Phase Align"};
    juce::ComboBox qMode;
    juce::Slider output, reference, scale, dynThreshold;
    juce::Label selectedLabel, rangeLabel;
    ResponseGraph graph;
    PianoStrip piano;
    suite::SuiteBar suite;
    std::array<std::unique_ptr<BandPanel>, 5> panels;
    std::unique_ptr<params::State::ButtonAttachment> snapAttachment, flatsAttachment, bypassAttachment, autoGainAttachment, soloSelectedAttachment, phaseCompAttachment;
    std::unique_ptr<params::State::ComboBoxAttachment> qModeAttachment;
    std::unique_ptr<params::State::SliderAttachment> outputAttachment, referenceAttachment, scaleAttachment, dynThresholdAttachment;
    juce::TooltipWindow tooltips {this, 700};
    int selected = 0;
    float meter = 0;
    void select(int band);
    void timerCallback() override;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TunerEditor)
};
