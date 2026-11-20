#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace ui {
inline const juce::Colour background {0xff0d1317}, panel {0xf0152026}, ink {0xffe6f0f5}, muted {0xff869ca8}, copper {0xffffbe3b};
inline const std::array<juce::Colour, 5> bandColours {
    juce::Colour(0xffffbe3b), juce::Colour(0xff48f0c8), juce::Colour(0xff78c6c1), juce::Colour(0xff84b9db), juce::Colour(0xffb9a3d5)};
juce::Font font(float size, bool bold = false);
void text(juce::Graphics&, const juce::String&, juce::Rectangle<float>, float size, juce::Colour = ink, juce::Justification = juce::Justification::centredLeft, bool bold = false);
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
