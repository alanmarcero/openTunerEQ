#include "Theme.h"
namespace ui {
juce::Font font(float size, bool bold) { return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)); }
void text(juce::Graphics& g, const juce::String& value, juce::Rectangle<float> bounds, float size, juce::Colour colour, juce::Justification alignment, bool bold) {
    g.setColour(colour); g.setFont(font(size, bold)); g.drawText(value, bounds, alignment, false);
}
juce::String frequencyText(double hz) { return juce::String(hz, hz < 1000 ? 2 : 1) + " Hz"; }
Theme::Theme() {
    setColour(juce::Slider::textBoxTextColourId, ink);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxHighlightColourId, copper.withAlpha(0.3f));
    setColour(juce::Slider::rotarySliderFillColourId, copper);
    setColour(juce::ComboBox::textColourId, ink);
    setColour(juce::ComboBox::backgroundColourId, panel);
    setColour(juce::ComboBox::outlineColourId, muted.withAlpha(0.2f));
    setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::textColourId, ink);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, copper.withAlpha(0.2f));
    setColour(juce::PopupMenu::highlightedTextColourId, ink);
    setColour(juce::TextButton::textColourOnId, background);
    setColour(juce::TextButton::textColourOffId, muted);
    setColour(juce::TextButton::buttonOnColourId, copper);
    setColour(juce::TextEditor::backgroundColourId, panel);
    setColour(juce::TextEditor::textColourId, ink);
    setColour(juce::TextEditor::highlightColourId, copper.withAlpha(0.3f));
}
juce::Label* Theme::createSliderTextBox(juce::Slider& slider) {
    auto* label = juce::LookAndFeel_V4::createSliderTextBox(slider);
    label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour(juce::Label::textColourId, ink);
    label->setFont(font(13));
    return label;
}
void Theme::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float position, float start, float end, juce::Slider& slider) {
    const auto area = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)).reduced(7);
    const auto radius = std::min(area.getWidth(), area.getHeight()) / 2.0f;
    const auto centre = area.getCentre();
    const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId).withMultipliedAlpha(slider.isEnabled() ? 1.0f : 0.25f);
    juce::Path track; track.addCentredArc(centre.x, centre.y, radius, radius, 0, start, end, true);
    g.setColour(juce::Colour(0xff38414a)); g.strokePath(track, juce::PathStrokeType(2.5f));
    juce::Path arc; arc.addCentredArc(centre.x, centre.y, radius, radius, 0, start, start + position * (end - start), true);
    g.setColour(accent); g.strokePath(arc, juce::PathStrokeType(2.5f));
    const auto knob = juce::Rectangle<float>(radius * 1.6f, radius * 1.6f).withCentre(centre);
    g.setColour(juce::Colours::black.withAlpha(0.4f)); g.fillEllipse(knob.translated(0, 3));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff414b55), knob.getTopLeft(), juce::Colour(0xff151c22), knob.getBottomRight(), false));
    g.fillEllipse(knob); g.setColour(juce::Colour(0xff606870)); g.drawEllipse(knob, 0.7f);
    const auto angle = start + position * (end - start) - juce::MathConstants<float>::halfPi;
    const auto direction = juce::Point<float>(std::cos(angle), std::sin(angle));
    g.setColour(accent); g.drawLine({centre + direction * radius * 0.42f, centre + direction * radius * 0.68f}, 2.3f);
}
void Theme::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float, float, const juce::Slider::SliderStyle, juce::Slider& slider) {
    const auto cy = static_cast<float>(y) + static_cast<float>(h) / 2;
    g.setColour(juce::Colour(0xff3b4650)); g.fillRoundedRectangle(static_cast<float>(x), cy - 1.5f, static_cast<float>(w), 3, 1.5f);
    auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
    g.setColour(accent.withMultipliedAlpha(slider.isEnabled() ? 1.0f : 0.25f));
    g.fillRoundedRectangle(static_cast<float>(x), cy - 1.5f, std::max(0.0f, sliderPos - x), 3, 1.5f);
    g.fillEllipse(sliderPos - 5, cy - 5, 10, 10);
}
void Theme::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down) {
    auto area = button.getLocalBounds().toFloat().reduced(0.5f);
    const auto on = button.getToggleState();
    g.setColour(on ? button.findColour(juce::TextButton::buttonOnColourId) : juce::Colour(over || down ? 0xff35404b : 0xff242d35));
    g.fillRoundedRectangle(area, 5); g.setColour(muted.withAlpha(0.18f)); g.drawRoundedRectangle(area, 5, 1);
}
void Theme::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) {
    text(g, button.getButtonText(), button.getLocalBounds().toFloat(), 12, button.getToggleState() ? background : ink, juce::Justification::centred, true);
}
void Theme::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) {
    const bool note = box.getComponentID().endsWith(".note");
    auto area = juce::Rectangle<float>(0.5f, 0.5f, w - 1.0f, h - 1.0f);
    g.setColour(note ? juce::Colours::transparentBlack : juce::Colour(0xff131a20)); g.fillRoundedRectangle(area, 4);
    if (!note) { g.setColour(muted.withAlpha(box.isEnabled() ? 0.2f : 0.08f)); g.drawRoundedRectangle(area, 4, 1); }
    juce::Path arrow; const float xx = w - 15.0f, yy = h / 2.0f;
    arrow.startNewSubPath(xx - 3, yy - 2); arrow.lineTo(xx, yy + 1); arrow.lineTo(xx + 3, yy - 2);
    g.setColour(muted.withAlpha(box.isEnabled() ? 1.0f : 0.2f)); g.strokePath(arrow, juce::PathStrokeType(1.2f));
}
juce::Font Theme::getComboBoxFont(juce::ComboBox& box) { return font(box.getComponentID().endsWith(".note") ? 32.0f : 12.0f, box.getComponentID().endsWith(".note")); }
}
