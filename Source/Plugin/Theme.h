#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace ui {
inline const juce::Colour background {0xff0d1317}, panel {0xf0152026}, ink {0xffe6f0f5}, muted {0xff869ca8}, copper {0xffffbe3b}, warning {0xffe98779};
inline const std::array<juce::Colour, 5> bandColours {
    juce::Colour(0xffffbe3b), juce::Colour(0xff48f0c8), juce::Colour(0xff78c6c1), juce::Colour(0xff84b9db), juce::Colour(0xffb9a3d5)};
juce::Font font(float size, bool bold = false);
void text(juce::Graphics&, const juce::String&, juce::Rectangle<float>, float size, juce::Colour = ink, juce::Justification = juce::Justification::centredLeft, bool bold = false);
inline constexpr float inactiveAlpha = 0.4f;
// An empty hint marks the control active. Otherwise the control dims and its tooltip adds the hint.
void showActive(juce::Component& control, const juce::String& tooltip, const juce::String& hint);
// A dBFS meter from -60 to 0 with a 1 s peak hold and a clip light that a click clears.
// The audio thread keeps the peak since the last read in `source`, and the meter reads it with exchange.
class LevelMeter final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    LevelMeter(std::atomic<float>& source, juce::Colour colour);
    void setMarker(float decibels);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void timerCallback() override;
private:
    std::atomic<float>& source;
    juce::Colour colour;
    float level = 0, held = 0;
    double heldAt = 0;
    bool clipped = false;
    std::optional<float> marker;
};
class Theme final : public juce::LookAndFeel_V4 {
public:
    Theme();
    juce::Label* createSliderTextBox(juce::Slider&) override;
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int, int, int, int, float, float, float, const juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawComboBox(juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
};
}
