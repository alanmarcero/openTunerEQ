#pragma once
#include "PluginProcessor.h"
#include "Theme.h"
class ResponseGraph final : public juce::Component {
public:
    explicit ResponseGraph(TunerProcessor&);
    ~ResponseGraph() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    // Call after a parameter change.
    void refresh();
    // Call each timer tick.
    void animate();
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
    juce::TextButton analyzerToggle {"Analyzer"};
    juce::TextButton freezeToggle {"Freeze"};
    juce::ComboBox analyzerStyle, analyzerSource, analyzerRelease, analyzerTilt;
    std::unique_ptr<params::State::ButtonAttachment> analyzerAttachment;
    std::unique_ptr<params::State::ButtonAttachment> freezeAttachment;
    std::unique_ptr<params::State::ComboBoxAttachment> styleAttachment, sourceAttachment, releaseAttachment, tiltAttachment;
    juce::Image history {juce::Image::RGB, 512, 128, true};
    std::array<juce::Path, 5> curves;
    juce::Path total, totalFill;
    ote::Settings curveSettings;
    double curveRate = 0;
    bool analyzerOn = true;
    bool afterEQ = false;
    bool analyzerFrozen = false;
    int style = 0;
    float tiltDbPerOctave = 4.5f;
    float hoverX = -1;
    int selected = 0, dragging = -1;
    bool gainGesture = false;
    juce::Point<float> lastDrag;
    float dragX = 0;
    double dragDb = 0;
    juce::Rectangle<float> plot() const;
    float xFor(double hz) const;
    float yFor(double db) const;
    juce::Point<float> pointFor(int band) const;
    int nodeAt(juce::Point<float>) const;
    void endGesture();
    void paintSpectrum(juce::Graphics&);
    double frequencyAt(float x) const;
};
class PianoStrip final : public juce::Component {
public:
    explicit PianoStrip(TunerProcessor& p) : processor(p) { setName("Piano note selector"); }
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void setSelected(int band) { selected = band; repaint(); }
    std::function<void(int)> onNote;
private:
    static constexpr int keyCount = 48;
    TunerProcessor& processor;
    int selected = 0, first = -1, hoverNote = -1, lastDraggedNote = -1, lastTrackedBand = -1, lastTrackedNote = -1;
    int noteAt(juce::Point<float>) const;
    juce::Rectangle<float> keyBounds(int offset) const;
};
