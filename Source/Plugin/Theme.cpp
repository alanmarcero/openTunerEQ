#include "Theme.h"
#include <juce_audio_basics/juce_audio_basics.h>
namespace ui {
juce::Font font(float size, bool bold) { return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)); }
void text(juce::Graphics& g, const juce::String& value, juce::Rectangle<float> bounds, float size, juce::Colour colour, juce::Justification alignment, bool bold) {
    g.setColour(colour); g.setFont(font(size, bold)); g.drawText(value, bounds, alignment, false);
}
void showActive(juce::Component& control, const juce::String& tooltip, const juce::String& hint) {
    auto tip = tooltip;
    if (hint.isNotEmpty()) tip << " " << hint;
    control.setAlpha(hint.isEmpty() ? 1.0f : inactiveAlpha);
    if (auto* client = dynamic_cast<juce::SettableTooltipClient*>(&control)) client->setTooltip(tip);
    // A slider copies its tooltip to its text box only at creation.
    for (auto* child : control.getChildren())
        if (auto* client = dynamic_cast<juce::SettableTooltipClient*>(child)) client->setTooltip(tip);
}
LevelMeter::LevelMeter(std::atomic<float>& peak, juce::Colour meterColour) : source(peak), colour(meterColour) { timerCallback(); }
// A hidden meter does not read levels.
void LevelMeter::visibilityChanged() {
    if (!isVisible()) { stopTimer(); return; }
    startTimerHz(24);
}
void LevelMeter::setMarker(float decibels) {
    if (marker == decibels) return;
    marker = decibels;
    repaint();
}
void LevelMeter::timerCallback() {
    const auto latest = source.exchange(0.0f);
    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto before = std::tuple(level, held, clipped);
    level = std::max(latest, level * 0.88f);
    if (level < 1.0e-4f) level = 0;
    if (latest >= held || now - heldAt > 1.0) { held = latest; heldAt = now; }
    clipped = clipped || latest > 1.0f;
    if (std::tuple(level, held, clipped) != before) repaint();
}
void LevelMeter::mouseDown(const juce::MouseEvent&) {
    clipped = false;
    repaint();
}
void LevelMeter::paint(juce::Graphics& g) {
    // A wide meter rotates a quarter turn, so up becomes right.
    auto length = static_cast<float>(getHeight()), breadth = static_cast<float>(getWidth());
    if (getWidth() > getHeight()) {
        std::swap(length, breadth);
        g.addTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::halfPi).translated(static_cast<float>(getWidth()), 0));
    }
    // -60 dBFS is the bottom. The top 3 px hold the clip light.
    auto top = [length](float gain) { return 3 + (length - 3) * (1 - juce::jlimit(0.0f, 1.0f, juce::Decibels::gainToDecibels(gain, -60.0f) / 60 + 1)); };
    g.setColour(muted.withAlpha(0.2f)); g.fillRect(0.0f, 0.0f, breadth, length);
    g.setColour(warning);
    if (clipped) g.fillRect(0.0f, 0.0f, breadth, 3.0f);
    g.setColour(colour); g.fillRect(0.0f, top(level), breadth, length - top(level));
    if (held > 0) g.fillRect(0.0f, top(held), breadth, 1.5f);
    g.setColour(ink.withAlpha(0.8f));
    if (marker) g.fillRect(0.0f, top(juce::Decibels::decibelsToGain(*marker)), breadth, 1.5f);
}
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
    setColour(juce::TooltipWindow::backgroundColourId, panel);
    setColour(juce::TooltipWindow::textColourId, ink);
    setColour(juce::TooltipWindow::outlineColourId, muted);
    setColour(juce::TextEditor::highlightColourId, copper.withAlpha(0.3f));
}
juce::Label* Theme::createSliderTextBox(juce::Slider& slider) {
    auto* label = juce::LookAndFeel_V4::createSliderTextBox(slider);
    label->setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour(juce::Label::textColourId, ink);
    label->setFont(font(13));
    // The value sits below the bar's caption.
    if (slider.getSliderStyle() == juce::Slider::LinearBar) label->setBorderSize({14, 2, 2, 2});
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
    for (int tick = 0; tick <= 16; ++tick) {
        const auto angle = start + static_cast<float>(tick) / 16 * (end - start) - juce::MathConstants<float>::halfPi;
        const juce::Point<float> direction {std::cos(angle), std::sin(angle)};
        g.setColour(muted.withAlpha(slider.isEnabled() ? 0.4f : 0.12f));
        g.drawLine({centre + direction * (radius + 3), centre + direction * (radius + 5)}, 0.8f);
    }
    const auto knob = juce::Rectangle<float>(radius * 1.6f, radius * 1.6f).withCentre(centre);
    g.setColour(juce::Colours::black.withAlpha(0.4f)); g.fillEllipse(knob.translated(0, 3));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff607076), knob.getTopLeft(), juce::Colour(0xff151c22), knob.getBottomRight(), false));
    g.fillEllipse(knob); g.setColour(juce::Colour(0xff606870)); g.drawEllipse(knob, 0.7f);
    g.setColour(background.withAlpha(0.6f)); g.drawEllipse(knob.reduced(2), 1);
    const auto angle = start + position * (end - start) - juce::MathConstants<float>::halfPi;
    const auto direction = juce::Point<float>(std::cos(angle), std::sin(angle));
    g.setColour(accent); g.drawLine({centre + direction * radius * 0.42f, centre + direction * radius * 0.68f}, 2.3f);
}
void Theme::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float, float, const juce::Slider::SliderStyle style, juce::Slider& slider) {
    if (style == juce::Slider::LinearBar) {
        const auto area = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)).reduced(0.5f);
        juce::Path outline; outline.addRoundedRectangle(area, 4);
        g.setColour(field); g.fillPath(outline);
        {
            // A range across zero fills from zero, so 0 dB draws empty.
            auto zero = area.getX();
            if (slider.getMinimum() < 0) zero = static_cast<float>(slider.getPositionOfValue(0));
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(outline);
            g.setColour(slider.findColour(juce::Slider::trackColourId).withMultipliedAlpha(slider.isEnabled() ? 1.0f : 0.25f));
            g.fillRect(juce::Rectangle<float>(std::min(zero, sliderPos), area.getY(), std::abs(sliderPos - zero), area.getHeight()));
        }
        g.setColour(muted.withAlpha(0.2f)); g.strokePath(outline, juce::PathStrokeType(1));
        text(g, slider.getName(), area.withHeight(16).withTrimmedLeft(6), 10, muted);
        return;
    }
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
    if (!button.isEnabled()) g.setOpacity(0.5f);
    g.fillRoundedRectangle(area, 5);
    const bool focus = button.hasKeyboardFocus(false);
    g.setColour(focus ? ink : muted.withAlpha(0.35f));
    g.drawRoundedRectangle(area.reduced(0.5f), 5, focus ? 2.0f : 1.0f);
}
void Theme::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) {
    const auto colour = button.getToggleState() ? background : ink;
    text(g, button.getButtonText(), button.getLocalBounds().toFloat(), 14,
         button.isEnabled() ? colour : muted, juce::Justification::centred, true);
}
void Theme::drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) {
    const bool note = box.getComponentID().endsWith(".note");
    auto area = juce::Rectangle<float>(0.5f, 0.5f, w - 1.0f, h - 1.0f);
    g.setColour(note ? juce::Colours::transparentBlack : field); g.fillRoundedRectangle(area, 4);
    if (!note) { g.setColour(muted.withAlpha(box.isEnabled() ? 0.2f : 0.08f)); g.drawRoundedRectangle(area, 4, 1); }
    juce::Path arrow; const float xx = w - 15.0f, yy = h / 2.0f;
    arrow.startNewSubPath(xx - 3, yy - 2); arrow.lineTo(xx, yy + 1); arrow.lineTo(xx + 3, yy - 2);
    g.setColour(muted.withAlpha(box.isEnabled() ? 1.0f : 0.2f)); g.strokePath(arrow, juce::PathStrokeType(1.2f));
    if (box.getName().isNotEmpty()) text(g, box.getName(), {6, 3, w - 12.0f, 12}, 10, muted);
}
// A named box shows its name as a caption above the value.
void Theme::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
    juce::LookAndFeel_V4::positionComboBoxText(box, label);
    if (box.getName().isNotEmpty()) label.setBounds(label.getBounds().withTrimmedTop(12));
}
juce::Font Theme::getComboBoxFont(juce::ComboBox& box) { return font(box.getComponentID().endsWith(".note") ? 32.0f : 14.0f, box.getComponentID().endsWith(".note")); }
}
