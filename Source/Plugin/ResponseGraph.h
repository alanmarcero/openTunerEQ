#pragma once
#include "PluginProcessor.h"
#include "Theme.h"
class ResponseGraph final : public juce::Component {
public:
    explicit ResponseGraph(TunerProcessor&);
    ~ResponseGraph() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void refresh();
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void setSelected(int band) { selected = band; repaint(); }
    std::function<void(int)> onSelect;
private:
    TunerProcessor& processor;
    SpectrumAnalyzer analyzer;
    juce::TextButton analyzerToggle {"Visualizer"};
    juce::ComboBox analyzerStyle;
    std::unique_ptr<params::State::ButtonAttachment> analyzerAttachment;
    std::unique_ptr<params::State::ComboBoxAttachment> styleAttachment;
    juce::Image history {juce::Image::RGB, 512, 128, true};
    bool analyzerOn = true;
    int style = 0;
    float hoverX = -1;
    int selected = 0, dragging = -1;
    bool gainGesture = false;
    juce::Rectangle<float> plot() const;
    float xFor(double hz) const;
    float yFor(double db) const;
    juce::Point<float> pointFor(int band) const;
    void endGesture();
    void paintSpectrum(juce::Graphics&);
    double frequencyAt(float x) const;
};
class PianoStrip final : public juce::Component {
public:
    explicit PianoStrip(TunerProcessor& p) : processor(p) { setName("Piano note selector"); }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void setSelected(int band) { selected = band; repaint(); }
    std::function<void(int)> onNote;
private:
    TunerProcessor& processor;
    int selected = 0;
    int firstNote() const;
    juce::Rectangle<float> keyBounds(int offset) const;
};
