#pragma once
#include "BandPanel.h"
#include "ResponseGraph.h"
#include "SuiteBar.h"
// The static background. JUCE caches it as an image, so a child's repaint does not redraw it.
class Faceplate final : public juce::Component {
public:
    Faceplate();
    void paint(juce::Graphics&) override;
private:
    juce::Image image;
};
// The tuning fork in the header pulses on the host's quarter notes.
class TuningFork final : public juce::Component {
public:
    explicit TuningFork(TunerProcessor&);
    void paint(juce::Graphics&) override;
private:
    TunerProcessor& processor;
    std::unique_ptr<juce::Drawable> drawable;
};
class TunerEditor final : public juce::AudioProcessorEditor, private juce::Timer, private juce::AudioProcessorParameter::Listener {
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
    Faceplate faceplate;
    TuningFork fork;
    ui::LevelMeter meter;
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
    float forkPhase = -1;
    double shownRate = 0;
    std::atomic<bool> parametersChanged {false};
    void select(int band);
    void refresh();
    void timerCallback() override;
    void parameterValueChanged(int, float) override;
    void parameterGestureChanged(int, bool) override {}
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TunerEditor)
};
