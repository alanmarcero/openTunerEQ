#include "ResponseGraph.h"
#include "Notes.h"
juce::Rectangle<float> ResponseGraph::plot() const { return getLocalBounds().toFloat().withTrimmedLeft(48).withTrimmedRight(24).withTrimmedTop(36).withTrimmedBottom(32); }
float ResponseGraph::xFor(double hz) const { const auto p = plot(); return p.getX() + static_cast<float>(std::log(hz / 20.0) / std::log(1000.0)) * p.getWidth(); }
float ResponseGraph::yFor(double db) const { auto p = plot(); return p.getCentreY() - static_cast<float>(std::clamp(db, -25.0, 25.0)) / 48.0f * p.getHeight(); }
juce::Point<float> ResponseGraph::pointFor(int index) const {
    auto b = processor.readSettings().bands[static_cast<size_t>(index)];
    const auto db = ote::usesGain(b.shape) ? b.gainDb : (ote::isCut(b.shape) ? -3.0103 : b.shape == ote::Shape::Notch ? -20.0 : 0.0);
    return {xFor(processor.bandFrequency(index)), yFor(db)};
}
void ResponseGraph::paint(juce::Graphics& g) {
    auto area = getLocalBounds().toFloat(); auto p = plot();
    g.setColour(juce::Colour(0xff0e151b).withAlpha(0.97f)); g.fillRoundedRectangle(area, 9);
    g.setColour(ui::muted.withAlpha(0.18f)); g.drawRoundedRectangle(area.reduced(0.5f), 9, 1);
    ui::text(g, "Frequency response", {18, 8, 220, 20}, 13, ui::ink, juce::Justification::centredLeft, true);
    ui::text(g, "Drag a band to tune  /  Scroll for Q  /  Double-click to bypass", {300, 8, area.getWidth() - 320, 20}, 11, ui::muted, juce::Justification::centredRight);
    for (int db = -24; db <= 24; db += 6) {
        const auto y = yFor(db); g.setColour(ui::muted.withAlpha(db == 0 ? 0.28f : 0.09f)); g.drawHorizontalLine(static_cast<int>(y), p.getX(), p.getRight());
        if (db % 12 == 0) ui::text(g, (db > 0 ? "+" : "") + juce::String(db), {8, y - 7, 32, 14}, 10, ui::muted, juce::Justification::centredRight);
    }
    for (double hz : {20., 50., 100., 200., 500., 1000., 2000., 5000., 10000., 20000.}) {
        const auto x = xFor(hz); g.setColour(ui::muted.withAlpha(0.1f)); g.drawVerticalLine(static_cast<int>(x), p.getY(), p.getBottom());
        ui::text(g, hz >= 1000 ? juce::String(hz / 1000, 0) + "k" : juce::String(hz, 0), {x - 20, p.getBottom() + 8, 40, 16}, 10, ui::muted, juce::Justification::centred);
    }
    auto settings = processor.readSettings();
    for (size_t i = 0; i < 5; ++i) settings.bands[i].frequency = processor.bandFrequency(static_cast<int>(i));
    const auto rate = processor.displaySampleRate();
    {
        juce::Graphics::ScopedSaveState save(g); g.reduceClipRegion(p.toNearestInt());
        std::array<juce::Path, 5> curves;
        juce::Path total;
        const int steps = static_cast<int>(p.getWidth() / 2);
        for (int step = 0; step <= steps; ++step) {
            const auto x = p.getX() + p.getWidth() * step / steps;
            const auto frequency = 20.0 * std::pow(1000.0, static_cast<double>(step) / steps);
            double sum = settings.outputDb;
            for (size_t i = 0; i < 5; ++i) {
                const auto db = frequency < rate * 0.5 ? ote::responseDb(settings.bands[i], rate, frequency) : -150.0;
                sum += db;
                if (step == 0) curves[i].startNewSubPath(x, yFor(db)); else curves[i].lineTo(x, yFor(db));
            }
            if (settings.bypass) sum = 0;
            if (step == 0) total.startNewSubPath(x, yFor(sum)); else total.lineTo(x, yFor(sum));
        }
        auto fill = total; fill.lineTo(p.getRight(), yFor(0)); fill.lineTo(p.getX(), yFor(0)); fill.closeSubPath();
        g.setColour(ui::copper.withAlpha(0.07f)); g.fillPath(fill);
        for (size_t i = 0; i < 5; ++i) {
            g.setColour(ui::bandColours[i].withAlpha(static_cast<int>(i) == selected ? 0.6f : 0.22f));
            g.strokePath(curves[i], juce::PathStrokeType(1.1f));
        }
        g.setColour(ui::ink.withAlpha(0.9f)); g.strokePath(total, juce::PathStrokeType(2.0f));
    }
    for (int i = 0; i < 5; ++i) {
        const auto pt = pointFor(i); const auto color = ui::bandColours[static_cast<size_t>(i)];
        const bool active = settings.bands[static_cast<size_t>(i)].enabled;
        if (i == selected) {
            g.setColour(color.withAlpha(0.13f)); g.drawLine(pt.x, p.getY(), pt.x, p.getBottom(), 1);
            g.fillEllipse(pt.x - 16, pt.y - 16, 32, 32);
            const auto hz = processor.bandFrequency(i);
            const auto caption = juce::String(ote::noteName(ote::nearestNote(hz), processor.usesFlats())) + "  " + juce::String(hz, 1) + " Hz";
            auto label = juce::Rectangle<float>(132, 21).withCentre({juce::jlimit(p.getX() + 66, p.getRight() - 66, pt.x), std::max(p.getY() + 10, pt.y - 27)});
            g.setColour(ui::background); g.fillRoundedRectangle(label, 4);
            ui::text(g, caption, label, 11, color, juce::Justification::centred, true);
        }
        g.setColour(active ? color : ui::panel); g.fillEllipse(pt.x - 9, pt.y - 9, 18, 18);
        g.setColour(color); g.drawEllipse(pt.x - 9, pt.y - 9, 18, 18, 1.2f);
        ui::text(g, juce::String(i + 1), {pt.x - 9, pt.y - 9, 18, 18}, 11, active ? ui::background : color, juce::Justification::centred, true);
    }
}
void ResponseGraph::mouseDown(const juce::MouseEvent& e) {
    endGesture();
    float nearest = 28;
    for (int i = 0; i < 5; ++i) {
        const auto distance = pointFor(i).getDistanceFrom(e.position);
        if (distance < nearest) { nearest = distance; dragging = i; }
    }
    if (dragging < 0) return;
    selected = dragging; if (onSelect) onSelect(selected);
    processor.state.getParameter(params::bandId(dragging, "freq"))->beginChangeGesture();
    gainGesture = ote::usesGain(processor.readSettings().bands[static_cast<size_t>(dragging)].shape);
    if (gainGesture) processor.state.getParameter(params::bandId(dragging, "gain"))->beginChangeGesture();
}
void ResponseGraph::mouseDrag(const juce::MouseEvent& e) {
    if (dragging < 0) return;
    auto p = plot();
    auto hz = 20 * std::pow(1000.0, juce::jlimit(0.0f, 1.0f, (e.position.x - p.getX()) / p.getWidth()));
    hz = ote::effectiveFrequency(hz, processor.readSettings().snap, processor.displaySampleRate());
    auto* parameter = processor.state.getParameter(params::bandId(dragging, "freq"));
    parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(hz)));
    if (gainGesture) {
        auto* gain = processor.state.getParameter(params::bandId(dragging, "gain"));
        const auto db = juce::jlimit(-24.0f, 24.0f, (p.getCentreY() - e.position.y) / p.getHeight() * 48);
        gain->setValueNotifyingHost(gain->convertTo0to1(db));
    }
    repaint();
}
void ResponseGraph::endGesture() {
    if (dragging < 0) return;
    processor.state.getParameter(params::bandId(dragging, "freq"))->endChangeGesture();
    if (gainGesture) processor.state.getParameter(params::bandId(dragging, "gain"))->endChangeGesture();
    dragging = -1; gainGesture = false;
}
void ResponseGraph::mouseUp(const juce::MouseEvent&) { endGesture(); }
void ResponseGraph::mouseDoubleClick(const juce::MouseEvent& e) {
    for (int i = 0; i < 5; ++i) if (pointFor(i).getDistanceFrom(e.position) < 18) {
        processor.setParameter(params::bandId(i, "enabled"), processor.readSettings().bands[static_cast<size_t>(i)].enabled ? 0.0f : 1.0f); break;
    }
}
void ResponseGraph::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) {
    const auto b = processor.readSettings().bands[static_cast<size_t>(selected)];
    if (ote::usesQ(b)) processor.setParameter(params::bandId(selected, "q"), static_cast<float>(std::clamp(b.q * std::exp(wheel.deltaY * 2), 0.1, 18.0)));
}
namespace {
bool blackKey(int midi) { const auto p = midi % 12; return p == 1 || p == 3 || p == 6 || p == 8 || p == 10; }
}
int PianoStrip::firstNote() const { return juce::jlimit(12, 120, (ote::nearestNote(processor.bandFrequency(selected)) / 12 - 1) * 12); }
juce::Rectangle<float> PianoStrip::keyBounds(int offset) const {
    int whites = 0;
    for (int i = 0; i < offset; ++i) if (!blackKey(i)) ++whites;
    const auto width = getWidth() / 14.0f;
    if (blackKey(offset)) return {width * whites - width * 0.29f, 0, width * 0.58f, getHeight() * 0.60f};
    return {width * whites + 1, 0, width - 2, static_cast<float>(getHeight())};
}
void PianoStrip::paint(juce::Graphics& g) {
    const auto first = firstNote(); const auto current = ote::nearestNote(processor.bandFrequency(selected));
    const auto colour = ui::bandColours[static_cast<size_t>(selected)];
    for (bool black : {false, true}) for (int i = 0; i < 24; ++i) {
        if (blackKey(i) != black) continue;
        auto rect = keyBounds(i);
        const int midi = first + i; const auto hz = ote::noteFrequency(midi);
        const bool valid = hz >= 20 && hz <= std::min(ote::maxFrequency, processor.displaySampleRate() * 0.49);
        g.setColour(midi == current ? colour : black ? juce::Colour(0xff11191f) : juce::Colour(0xffb9bdbb));
        if (!valid) g.setColour(black ? juce::Colour(0xff0f151a) : juce::Colour(0xff454c50));
        g.fillRoundedRectangle(rect, 3);
        g.setColour(juce::Colours::black.withAlpha(0.5f)); g.drawRoundedRectangle(rect, 3, 1);
        ui::text(g, juce::String(ote::noteName(midi, processor.usesFlats())), rect.withTrimmedTop(rect.getHeight() - 19), black ? 9.0f : 11.0f,
                 black && midi != current ? ui::muted : ui::background, juce::Justification::centred, midi == current);
    }
}
void PianoStrip::mouseDown(const juce::MouseEvent& e) {
    for (bool black : {true, false}) for (int i = 0; i < 24; ++i) {
        if (blackKey(i) != black || !keyBounds(i).contains(e.position)) continue;
        const auto midi = firstNote() + i; const auto hz = ote::noteFrequency(midi);
        if (hz >= 20 && hz <= std::min(ote::maxFrequency, processor.displaySampleRate() * 0.49) && onNote) onNote(midi);
        return;
    }
}
