#include "ResponseGraph.h"
#include "Notes.h"
namespace {
// Grey, so the analyzer does not look like a band.
const juce::Colour analyzerColour {0xffa3a9ad};
}
ResponseGraph::ResponseGraph(TunerProcessor& p) : processor(p), analyzer(p.spectrum) {
    setName("Equalizer response graph");
    auto choices = [&p](const char* id) { return dynamic_cast<juce::AudioParameterChoice&>(*p.state.getParameter(id)).choices; };
    analyzerToggle.setComponentID("analyzerEnabled");
    analyzerToggle.setClickingTogglesState(true);
    analyzerToggle.setTooltip("Show the track's live audio spectrum behind the EQ response. Turning this off stops analysis.");
    analyzerStyle.setComponentID("analyzerStyle");
    analyzerStyle.addItemList(choices("analyzerStyle"), 1);
    analyzerSource.setComponentID("analyzerSource");
    analyzerSource.addItemList(choices("analyzerSource"), 1);
    freezeToggle.setComponentID("analyzerFreeze");
    freezeToggle.setClickingTogglesState(true);
    analyzerRelease.setComponentID("analyzerRelease");
    analyzerRelease.addItemList(choices("analyzerRelease"), 1);
    analyzerTilt.setComponentID("analyzerTilt");
    analyzerSource.setName("Source"); analyzerStyle.setName("Style"); analyzerTilt.setName("Tilt"); analyzerRelease.setName("Release");
    analyzerSource.setTitle("Analyzer source");
    analyzerStyle.setTitle("Analyzer style"); analyzerTilt.setTitle("Analyzer tilt"); analyzerRelease.setTitle("Analyzer release");
    analyzerTilt.addItemList(choices("analyzerTilt"), 1);
    addAndMakeVisible(analyzerToggle); addAndMakeVisible(freezeToggle); addAndMakeVisible(analyzerRelease);
    addAndMakeVisible(analyzerTilt); addAndMakeVisible(analyzerStyle); addAndMakeVisible(analyzerSource);
    analyzerAttachment = std::make_unique<params::State::ButtonAttachment>(p.state, "analyzerEnabled", analyzerToggle);
    freezeAttachment = std::make_unique<params::State::ButtonAttachment>(p.state, "analyzerFreeze", freezeToggle);
    styleAttachment = std::make_unique<params::State::ComboBoxAttachment>(p.state, "analyzerStyle", analyzerStyle);
    sourceAttachment = std::make_unique<params::State::ComboBoxAttachment>(p.state, "analyzerSource", analyzerSource);
    releaseAttachment = std::make_unique<params::State::ComboBoxAttachment>(p.state, "analyzerRelease", analyzerRelease);
    tiltAttachment = std::make_unique<params::State::ComboBoxAttachment>(p.state, "analyzerTilt", analyzerTilt);
    refresh(); animate();
}
ResponseGraph::~ResponseGraph() { processor.spectrum.setActive(false); endGesture(); }
void ResponseGraph::resized() {
    curveRate = 0;
    auto row = getLocalBounds().removeFromTop(40).withTrimmedRight(8);
    freezeToggle.setBounds(row.removeFromRight(64)); row.removeFromRight(6);
    analyzerRelease.setBounds(row.removeFromRight(132)); row.removeFromRight(6);
    analyzerTilt.setBounds(row.removeFromRight(96)); row.removeFromRight(6);
    analyzerStyle.setBounds(row.removeFromRight(120)); row.removeFromRight(6);
    analyzerSource.setBounds(row.removeFromRight(104)); row.removeFromRight(6);
    analyzerToggle.setBounds(row.removeFromRight(92));
}
void ResponseGraph::refresh() {
    const auto enabled = processor.state.getRawParameterValue("analyzerEnabled")->load() > 0.5f;
    const auto nextStyle = juce::jlimit(0, 2, static_cast<int>(processor.state.getRawParameterValue("analyzerStyle")->load()));
    const auto nextAfterEQ = processor.state.getRawParameterValue("analyzerSource")->load() > 0.5f;
    analyzerFrozen = processor.state.getRawParameterValue("analyzerFreeze")->load() > 0.5f;
    const auto releaseIndex = juce::jlimit(0, 2, static_cast<int>(processor.state.getRawParameterValue("analyzerRelease")->load()));
    const auto tiltIndex = juce::jlimit(0, 3, static_cast<int>(processor.state.getRawParameterValue("analyzerTilt")->load()));
    constexpr std::array<float, 3> releaseRates {24.0f, 48.0f, 96.0f};
    constexpr std::array<float, 4> tiltAmounts {0.0f, 3.0f, 4.5f, 6.0f};
    tiltDbPerOctave = tiltAmounts[static_cast<size_t>(tiltIndex)];
    const auto analyzerChanged = enabled != analyzerOn || nextStyle != style || nextAfterEQ != afterEQ;
    if (analyzerChanged) history.clear(history.getBounds());
    analyzerOn = enabled; style = nextStyle; afterEQ = nextAfterEQ;
    analyzer.setFrozen(analyzerFrozen);
    analyzer.setReleaseDbPerSecond(releaseRates[static_cast<size_t>(releaseIndex)]);
    const juce::String analyzerOff = enabled ? "" : "Turn on the analyzer to use it.";
    ui::showActive(analyzerStyle, "Spectrum: detailed peaks. Bars: frequency regions. Spectrogram: recent frequency history, newest at the top.", analyzerOff);
    ui::showActive(analyzerSource, "Before EQ: the incoming track. After EQ: the processed track, including output gain and bypass.", analyzerOff);
    ui::showActive(freezeToggle, "Hold the analyzer display until Freeze is turned off.", analyzerOff);
    ui::showActive(analyzerRelease, "Peak decay speed in dB per second.", analyzerOff);
    ui::showActive(analyzerTilt, "Tilt the spectrum around 1 kHz to make low-frequency balance easier to read.", analyzerOff);
    processor.spectrum.setActive(enabled);
    if (!enabled) {
        SpectrumCapture::Packet discarded;
        for (int i = 0; i < 256 && processor.spectrum.pop(discarded); ++i) {}
        analyzer.reset();
    }
    // Only analyzer changes move the legend and the scale labels.
    if (analyzerChanged) { repaint(); return; }
    repaint(plot().getSmallestIntegerContainer());
}
void ResponseGraph::animate() {
    const auto spectrumChanged = analyzerOn && analyzer.update(juce::Time::getMillisecondCounterHiRes() * 0.001, afterEQ);
    const auto historyScrolls = analyzerOn && style == 2 && !analyzerFrozen;
    if (historyScrolls) {
        history.moveImageSection(0, 1, 0, 0, 512, 127);
        juce::Image::BitmapData pixels(history, juce::Image::BitmapData::writeOnly);
        static const auto heat = [] {
            juce::ColourGradient gradient(juce::Colour(0xff0e151b), 0, 0, ui::ink, 1, 0, false);
            gradient.addColour(0.6, analyzerColour);
            return gradient;
        }();
        for (int x = 0; x < 512; ++x) {
            const auto db = analyzer.levelInRange(20 * std::pow(1000.0, x / 512.0), 20 * std::pow(1000.0, (x + 1) / 512.0), tiltDbPerOctave);
            const auto strength = juce::jlimit(0.0f, 1.0f, (db + 90) / 90);
            pixels.setPixelColour(x, 0, heat.getColourAtPosition(strength));
        }
    }
    const auto bands = processor.readSettings().bands;
    const auto dynamicBand = std::any_of(bands.begin(), bands.end(), [](const ote::BandSettings& b) { return b.enabled && ote::usesDynamics(b); });
    if (!spectrumChanged && !historyScrolls && !dynamicBand) return;
    repaint(plot().getSmallestIntegerContainer());
    // The hover readout shows the level under the cursor.
    if (hoverX >= 0) repaint(0, 0, analyzerToggle.getX(), 40);
}
juce::Rectangle<float> ResponseGraph::plot() const { return getLocalBounds().toFloat().withTrimmedLeft(48).withTrimmedRight(48).withTrimmedTop(44).withTrimmedBottom(32); }
double ResponseGraph::frequencyAt(float x) const { const auto p = plot(); return 20 * std::pow(1000.0, juce::jlimit(0.0f, 1.0f, (x - p.getX()) / p.getWidth())); }
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
    ui::text(g, "EQ response", {18, 8, 100, 20}, 13, ui::ink, juce::Justification::centredLeft, true);
    juce::String caption = "Drag a band, Shift for fine / Scroll a band for Q / Double-click to bypass";
    if (analyzerOn) caption = "Grey: tilted audio spectrum / White: EQ response";
    if (analyzerOn && style == 2) caption = "Grey: tilted audio history, newest at top / White: EQ response";
    if (hoverX >= 0) {
        const auto hz = frequencyAt(hoverX);
        caption = params::frequencyText(hz) + " / " + ote::noteName(ote::nearestNote(hz, processor.referencePitch()), processor.usesFlats());
        if (analyzerOn) caption += " / " + juce::String(analyzer.levelAt(hz, tiltDbPerOctave), 1) + (afterEQ ? " dB tilted after EQ" : " dB tilted before EQ");
        caption += " / Alt-click to tune";
    }
    ui::text(g, caption, {124, 8, analyzerToggle.getX() - 132.0f, 20}, 11, ui::muted);
    if (analyzerOn) paintSpectrum(g);
    for (int db = -24; db <= 24; db += 6) {
        const auto y = yFor(db); g.setColour(ui::muted.withAlpha(db == 0 ? 0.28f : 0.09f)); g.drawHorizontalLine(static_cast<int>(y), p.getX(), p.getRight());
        if (db % 12 == 0) ui::text(g, (db > 0 ? "+" : "") + juce::String(db), {8, y - 7, 32, 14}, 10, ui::muted, juce::Justification::centredRight);
    }
    for (double hz : {20., 50., 100., 200., 500., 1000., 2000., 5000., 10000., 20000.}) {
        const auto x = xFor(hz); g.setColour(ui::muted.withAlpha(0.1f)); g.drawVerticalLine(static_cast<int>(x), p.getY(), p.getBottom());
        ui::text(g, hz >= 1000 ? juce::String(hz / 1000, 0) + "k" : juce::String(hz, 0), {x - 20, p.getBottom() + 8, 40, 16}, 10, ui::muted, juce::Justification::centred);
    }
    if (hoverX >= p.getX() && hoverX <= p.getRight()) {
        g.setColour(ui::ink.withAlpha(0.35f)); g.drawVerticalLine(static_cast<int>(hoverX), p.getY(), p.getBottom());
    }
    auto settings = processor.readSettings();
    for (size_t i = 0; i < 5; ++i) {
        settings.bands[i] = ote::audibleBand(settings, i);
        settings.bands[i].frequency = processor.bandFrequency(static_cast<int>(i));
    }
    const auto rate = processor.displaySampleRate();
    // A rebuild costs five filter designs and about 2,500 magnitudes.
    // Dynamic range, drive and the threshold do not change the static curves.
    auto curveKey = settings;
    for (auto& band : curveKey.bands) band.dynamicDb = band.drive = 0;
    curveKey.dynamicThresholdDb = 0;
    if (curveKey != curveSettings || rate != curveRate) {
        curveSettings = curveKey; curveRate = rate;
        for (auto& curve : curves) curve.clear();
        total.clear();
        std::array<ote::FilterDesign, 5> designs{};
        for (size_t i = 0; i < 5; ++i) {
            if (settings.bands[i].enabled) {
                designs[i] = ote::designFilter(settings.bands[i], rate);
            }
        }
        const int steps = static_cast<int>(p.getWidth() / 2);
        for (int step = 0; step <= steps; ++step) {
            const auto x = p.getX() + p.getWidth() * step / steps;
            const auto frequency = 20.0 * std::pow(1000.0, static_cast<double>(step) / steps);
            const double w = 2.0 * juce::MathConstants<double>::pi * frequency / rate;
            double sum = settings.outputDb;
            for (size_t i = 0; i < 5; ++i) {
                double db = 0.0;
                if (settings.bands[i].enabled) {
                    if (frequency < rate * 0.5) {
                        double mag = 1.0;
                        for (int s = 0; s < designs[i].count; ++s) {
                            mag *= designs[i].stages[static_cast<size_t>(s)].magnitude(w);
                        }
                        db = 20.0 * std::log10(std::max(1.0e-15, mag));
                    } else {
                        db = -150.0;
                    }
                }
                if (settings.bands[i].placement == ote::Placement::Stereo) sum += db;
                if (step == 0) curves[i].startNewSubPath(x, yFor(db)); else curves[i].lineTo(x, yFor(db));
            }
            if (settings.bypass) sum = 0;
            if (step == 0) total.startNewSubPath(x, yFor(sum)); else total.lineTo(x, yFor(sum));
        }
        totalFill = total; totalFill.lineTo(p.getRight(), yFor(0)); totalFill.lineTo(p.getX(), yFor(0)); totalFill.closeSubPath();
        const float dashPattern[] = {5.0f, 4.0f};
        for (size_t i = 0; i < 5; ++i) {
            if (settings.bands[i].placement == ote::Placement::Stereo) continue;
            juce::Path dashed;
            juce::PathStrokeType(1.15f).createDashedStroke(dashed, curves[i], dashPattern, 2);
            curves[i] = dashed;
        }
    }
    {
        juce::Graphics::ScopedSaveState save(g); g.reduceClipRegion(p.toNearestInt());
        g.setColour(ui::copper.withAlpha(0.07f)); g.fillPath(totalFill);
        for (size_t i = 0; i < 5; ++i) {
            g.setColour(ui::bandColours[i].withAlpha(static_cast<int>(i) == selected ? 0.75f : 0.28f));
            g.strokePath(curves[i], juce::PathStrokeType(settings.bands[i].placement == ote::Placement::Stereo ? 1.1f : 1.15f));
        }
        g.setColour(ui::ink.withAlpha(0.9f)); g.strokePath(total, juce::PathStrokeType(2.0f));
    }
    for (int i = 0; i < 5; ++i) {
        const auto pt = pointFor(i); const auto color = ui::bandColours[static_cast<size_t>(i)];
        const auto& band = settings.bands[static_cast<size_t>(i)];
        const bool active = band.enabled;
        if (i == selected) {
            g.setColour(color.withAlpha(0.13f)); g.drawLine(pt.x, p.getY(), pt.x, p.getBottom(), 1);
            g.fillEllipse(pt.x - 16, pt.y - 16, 32, 32);
            const auto hz = processor.bandFrequency(i);
            const auto place = band.placement;
            auto caption = juce::String(ote::noteName(ote::nearestNote(hz, processor.referencePitch()), processor.usesFlats())) + "  " + params::frequencyText(hz);
            if (ote::usesGain(band.shape)) caption += "  " + processor.state.getParameter(params::bandId(i, "gain"))->getCurrentValueAsText();
            if (ote::usesQ(band)) caption += "  Q " + processor.state.getParameter(params::bandId(i, "q"))->getCurrentValueAsText();
            if (place != ote::Placement::Stereo) caption += "  " + params::placements[static_cast<int>(place)];
            const auto width = juce::GlyphArrangement::getStringWidth(ui::font(11, true), caption) + 16;
            auto label = juce::Rectangle<float>(width, 21.0f).withCentre({juce::jlimit(p.getX() + width / 2, p.getRight() - width / 2, pt.x), std::max(p.getY() + 10, pt.y - 27)});
            g.setColour(ui::background); g.fillRoundedRectangle(label, 4);
            ui::text(g, caption, label, 11, color, juce::Justification::centred, true);
        }
        if (band.enabled && ote::usesDynamics(band)) {
            // The line spans the dynamic range. The ring shows the live gain.
            g.setColour(color.withAlpha(0.45f)); g.drawLine(pt.x, pt.y, pt.x, yFor(band.gainDb + band.dynamicDb), 3);
            const auto live = yFor(band.gainDb + processor.dynamicGains[static_cast<size_t>(i)].load());
            g.setColour(color.withAlpha(0.8f)); g.drawEllipse(pt.x - 7, live - 7, 14, 14, 1.5f);
        }
        g.setColour(active ? color : ui::panel); g.fillEllipse(pt.x - 9, pt.y - 9, 18, 18);
        g.setColour(color); g.drawEllipse(pt.x - 9, pt.y - 9, 18, 18, 1.2f);
        ui::text(g, juce::String(i + 1), {pt.x - 9, pt.y - 9, 18, 18}, 11, active ? ui::background : color, juce::Justification::centred, true);
        const char* mark = nullptr;
        switch (band.placement) {
            case ote::Placement::Left: mark = "L"; break;
            case ote::Placement::Right: mark = "R"; break;
            case ote::Placement::Mid: mark = "M"; break;
            case ote::Placement::Side: mark = "S"; break;
            case ote::Placement::Stereo: break;
        }
        if (mark != nullptr) ui::text(g, mark, {pt.x + 8, pt.y - 22, 16, 14}, 11, color, juce::Justification::centred, true);
    }
}
void ResponseGraph::paintSpectrum(juce::Graphics& g) {
    const auto p = plot();
    const auto colour = analyzerColour;
    auto y = [&](float db) { return p.getBottom() - juce::jlimit(0.0f, 1.0f, (db + 90) / 90) * p.getHeight(); };
    if (style == 2) {
        g.setOpacity(0.6f); g.drawImage(history, p); g.setOpacity(1.0f);
        ui::text(g, "now", {p.getRight() + 4, p.getY() - 5, 40, 14}, 9, ui::muted);
        ui::text(g, "~5s", {p.getRight() + 4, p.getBottom() - 10, 40, 14}, 9, ui::muted);
        return;
    }
    for (int db : {0, -45, -90}) ui::text(g, juce::String(db), {p.getRight() + 5, y(static_cast<float>(db)) - 7, 36, 14}, 9, colour.withAlpha(0.7f));
    juce::Graphics::ScopedSaveState save(g); g.reduceClipRegion(p.toNearestInt());
    const int columns = style == 1 ? 120 : 512;
    juce::Path line;
    for (int i = 0; i < columns; ++i) {
        const auto low = 20 * std::pow(1000.0, static_cast<double>(i) / columns);
        const auto high = 20 * std::pow(1000.0, static_cast<double>(i + 1) / columns);
        const auto top = y(analyzer.levelInRange(low, high, tiltDbPerOctave));
        const auto x = p.getX() + p.getWidth() * (i + 0.5f) / columns;
        if (style == 1) {
            g.setColour(colour.withAlpha(0.32f));
            g.fillRect(p.getX() + p.getWidth() * i / columns, top, p.getWidth() / columns - 1, p.getBottom() - top);
            g.setColour(colour.withAlpha(0.7f)); g.drawHorizontalLine(static_cast<int>(top), x - p.getWidth() / columns * 0.4f, x + p.getWidth() / columns * 0.4f);
        } else if (i == 0) line.startNewSubPath(x, top); else line.lineTo(x, top);
    }
    if (style == 0) {
        auto fill = line; fill.lineTo(p.getRight(), p.getBottom()); fill.lineTo(p.getX(), p.getBottom()); fill.closeSubPath();
        g.setGradientFill(juce::ColourGradient(colour.withAlpha(0.4f), p.getCentreX(), p.getY(), colour.withAlpha(0.05f), p.getCentreX(), p.getBottom(), false));
        g.fillPath(fill); g.setColour(colour.withAlpha(0.75f)); g.strokePath(line, juce::PathStrokeType(1.3f));
    }
}
void ResponseGraph::mouseMove(const juce::MouseEvent& e) { hoverX = plot().contains(e.position) ? e.position.x : -1; repaint(); }
void ResponseGraph::mouseExit(const juce::MouseEvent&) { hoverX = -1; repaint(); }
int ResponseGraph::nodeAt(juce::Point<float> position) const {
    int nearest = -1;
    float distance = 28;
    for (int i = 0; i < 5; ++i) {
        const auto away = pointFor(i).getDistanceFrom(position);
        if (away >= distance) continue;
        distance = away;
        nearest = i;
    }
    return nearest;
}
void ResponseGraph::mouseDown(const juce::MouseEvent& e) {
    endGesture();
    if (e.mods.isAltDown() && plot().contains(e.position)) {
        const auto hz = ote::effectiveFrequency(frequencyAt(e.position.x), processor.readSettings().snap, processor.displaySampleRate(), processor.referencePitch());
        processor.setParameter(params::bandId(selected, "freq"), static_cast<float>(hz));
        repaint(); return;
    }
    dragging = nodeAt(e.position);
    if (dragging < 0) return;
    selected = dragging; if (onSelect) onSelect(selected);
    // The drag starts from the node, so an off-centre grab does not jump.
    const auto band = processor.readSettings().bands[static_cast<size_t>(dragging)];
    lastDrag = e.position; dragX = pointFor(dragging).x; dragDb = band.gainDb;
    processor.state.getParameter(params::bandId(dragging, "freq"))->beginChangeGesture();
    gainGesture = ote::usesGain(band.shape);
    if (gainGesture) processor.state.getParameter(params::bandId(dragging, "gain"))->beginChangeGesture();
}
void ResponseGraph::mouseDrag(const juce::MouseEvent& e) {
    if (dragging < 0) return;
    const auto p = plot();
    const auto settings = processor.readSettings();
    const auto speed = e.mods.isShiftDown() ? 0.25f : 1.0f;
    dragX = juce::jlimit(p.getX(), p.getRight(), dragX + (e.position.x - lastDrag.x) * speed);
    dragDb = juce::jlimit(-24.0 * settings.scale, 24.0 * settings.scale, dragDb - (e.position.y - lastDrag.y) * speed / p.getHeight() * 48);
    lastDrag = e.position;
    const auto hz = ote::effectiveFrequency(frequencyAt(dragX), settings.snap, processor.displaySampleRate(), processor.referencePitch());
    auto* parameter = processor.state.getParameter(params::bandId(dragging, "freq"));
    parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(hz)));
    // The node shows gain times Scale.
    if (gainGesture && settings.scale > 0) {
        auto* gain = processor.state.getParameter(params::bandId(dragging, "gain"));
        gain->setValueNotifyingHost(gain->convertTo0to1(static_cast<float>(dragDb / settings.scale)));
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
    const auto band = nodeAt(e.position);
    if (band >= 0) processor.setParameter(params::bandId(band, "enabled"), processor.readSettings().bands[static_cast<size_t>(band)].enabled ? 0.0f : 1.0f);
}
void ResponseGraph::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    const auto band = nodeAt(e.position);
    if (band < 0 || !ote::usesQ(processor.readSettings().bands[static_cast<size_t>(band)])) { juce::Component::mouseWheelMove(e, wheel); return; }
    if (band != selected) { selected = band; if (onSelect) onSelect(band); }
    const auto current = processor.readSettings().bands[static_cast<size_t>(band)].q;
    processor.setParameter(params::bandId(band, "q"), static_cast<float>(std::clamp(current * std::exp(wheel.deltaY * 2), 0.1, 18.0)));
}
namespace {
bool blackKey(int midi) { const auto p = midi % 12; return p == 1 || p == 3 || p == 6 || p == 8 || p == 10; }
}
juce::Rectangle<float> PianoStrip::keyBounds(int offset) const {
    int whites = 0;
    for (int i = 0; i < offset; ++i) if (!blackKey(i)) ++whites;
    const auto width = getWidth() / (keyCount / 12 * 7.0f);
    if (blackKey(offset)) return {width * whites - width * 0.29f, 0, width * 0.58f, getHeight() * 0.60f};
    return {width * whites + 1, 0, width - 2, static_cast<float>(getHeight())};
}
void PianoStrip::paint(juce::Graphics& g) {
    // Scroll only when the note leaves the keys, so a clicked key stays under the cursor.
    const auto current = ote::nearestNote(processor.bandFrequency(selected), processor.referencePitch());
    if (first < 0 || current < first || current >= first + keyCount) first = juce::jlimit(12, 96, (current / 12 - 1) * 12);
    const auto colour = ui::bandColours[static_cast<size_t>(selected)];
    for (bool black : {false, true}) for (int i = 0; i < keyCount; ++i) {
        if (blackKey(i) != black) continue;
        auto rect = keyBounds(i);
        const int midi = first + i; const auto hz = ote::noteFrequency(midi, processor.referencePitch());
        const bool valid = hz >= 20 && hz <= std::min(ote::maxFrequency, processor.displaySampleRate() * 0.49);
        g.setColour(midi == current ? colour : black ? juce::Colour(0xff11191f) : juce::Colour(0xffb9bdbb));
        if (!valid) g.setColour(black ? juce::Colour(0xff0f151a) : juce::Colour(0xff454c50));
        g.fillRoundedRectangle(rect, 3);
        g.setColour(juce::Colours::black.withAlpha(0.5f)); g.drawRoundedRectangle(rect, 3, 1);
        ui::text(g, juce::String(ote::noteName(midi, processor.usesFlats())), rect.withTrimmedTop(rect.getHeight() - 19), black ? 8.0f : 10.0f,
                 black && midi != current ? ui::muted : ui::background, juce::Justification::centred, midi == current);
    }
    const auto settings = processor.readSettings();
    std::array<int, 5> notes {};
    for (size_t band = 0; band < notes.size(); ++band) notes[band] = ote::nearestNote(processor.bandFrequency(static_cast<int>(band)), processor.referencePitch());
    for (size_t band = 0; band < notes.size(); ++band) {
        const auto offset = notes[band] - first;
        if (!settings.bands[band].enabled || offset < 0 || offset >= keyCount) continue;
        const auto below = std::count_if(notes.begin(), notes.begin() + static_cast<long>(band), [&](int note) { return note == notes[band]; });
        const auto key = keyBounds(offset);
        const auto dot = juce::Rectangle<float>(9, 9).withCentre({key.getCentreX(), key.getBottom() - 27 - static_cast<float>(below) * 10});
        g.setColour(ui::bandColours[band]); g.fillEllipse(dot);
        g.setColour(ui::background); g.drawEllipse(dot, 1.2f);
    }
}
// A click maps to the keys as last painted.
void PianoStrip::mouseDown(const juce::MouseEvent& e) {
    if (first < 0) return;
    for (bool black : {true, false}) for (int i = 0; i < keyCount; ++i) {
        if (blackKey(i) != black || !keyBounds(i).contains(e.position)) continue;
        const auto midi = first + i; const auto hz = ote::noteFrequency(midi, processor.referencePitch());
        if (hz >= 20 && hz <= std::min(ote::maxFrequency, processor.displaySampleRate() * 0.49) && onNote) onNote(midi);
        return;
    }
}
