#include "Notes.h"
#include "Filters.h"
#include "Equalizer.h"
#include <cmath>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

int checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void near(double actual, double expected, double tolerance, const char* message) {
    if (std::abs(actual - expected) > tolerance || !std::isfinite(actual)) {
        std::cerr << "actual=" << actual << " expected=" << expected << '\n';
        check(false, message);
    }
    ++checks;
}
int main() {
    using namespace ote;
    near(noteFrequency(69, standardPitch), 440.0, 1e-9, "A4 tuning");
    near(noteFrequency(60, standardPitch), 261.625565, 1e-5, "C4 tuning");
    near(snapFrequency(448, standardPitch), 440, 1e-9, "nearest semitone");
    near(snapFrequency(20, standardPitch), 20.601722, 1e-5, "lowest in-range note");
    near(snapFrequency(20000, standardPitch), 19912.126958, 1e-5, "highest in-range note");
    check(noteName(61, false) == "C#4" && noteName(61, true) == "Db4", "enharmonic spelling");
    near(centsFromNote(445, 69, standardPitch), 19.5621748, 1e-5, "free frequency cents");
    check(std::isfinite(snapFrequency(std::numeric_limits<double>::quiet_NaN(), standardPitch)), "invalid frequency sanitized");
    check(effectiveFrequency(20000, true, 32000, standardPitch) < 16000, "Nyquist bound");
    near(effectiveFrequency(445, false, 48000, standardPitch), 445, 1e-9, "free frequency preserved");
    near(noteFrequency(60, 432), 256.868737, 1e-5, "C4 at A4 = 432 Hz");
    near(snapFrequency(440, 432), 432, 1e-9, "snapping follows the reference");
    near(centsFromNote(440, 69, 432), 31.7667, 1e-3, "cents follow the reference");
    BandSettings s;
    s.frequency = 1000; s.q = 0.7071067811865476;
    s.shape = Shape::Bell; s.gainDb = 12;
    near(responseDb(s, 48000, 1000), 12, 1e-6, "bell center gain");
    s.gainDb = -12;
    near(responseDb(s, 48000, 1000), -12, 1e-6, "bell center cut");
    s.shape = Shape::Notch;
    check(responseDb(s, 48000, 1000) < -100, "notch rejects center");
    s.shape = Shape::BandPass;
    near(responseDb(s, 48000, 1000), 0, 1e-6, "band-pass normalized peak");
    for (auto shape : {Shape::HighPass, Shape::LowPass}) {
        s.shape = shape;
        for (int slope : {12, 24, 48, 96}) {
            s.slope = slope;
            near(responseDb(s, 48000, 1000), -3.0103, 0.001, "Butterworth cutoff consistent across slopes");
            const auto rejected = shape == Shape::HighPass ? 250.0 : 4000.0;
            check(responseDb(s, 48000, rejected) < -slope * 1.9, "cut slope attenuation");
        }
    }
    // Matched sections follow the analog prototype near Nyquist, where the bilinear cookbook design was off by 5 to 26 dB.
    const auto analogDb = [](const BandSettings& b, double hz) {
        const std::complex<double> j(0, hz / b.frequency);
        const auto a = std::pow(10.0, b.gainDb / 40.0);
        const auto h = b.shape == Shape::Bell ? (j * j + j * a / b.q + 1.0) / (j * j + j / (a * b.q) + 1.0)
                     : b.shape == Shape::LowPass ? 1.0 / (j * j + j / b.q + 1.0)
                     : j * j / (j * j + j / b.q + 1.0);
        return 20 * std::log10(std::abs(h));
    };
    BandSettings top;
    top.shape = Shape::Bell; top.frequency = 16000; top.gainDb = 12; top.q = 0.7071;
    for (double hz : {4000.0, 8000.0, 12000.0, 19000.0}) near(responseDb(top, 44100, hz), analogDb(top, hz), 1.5, "matched bell near Nyquist");
    top.shape = Shape::LowPass; top.frequency = 10000; top.q = 4;
    for (double hz : {5000.0, 15000.0, 19900.0}) near(responseDb(top, 44100, hz), analogDb(top, hz), 1.5, "matched low-pass near Nyquist");
    top.shape = Shape::HighPass; top.frequency = 16000; top.q = 0.7071;
    for (double hz : {1600.0, 8000.0, 19900.0}) near(responseDb(top, 44100, hz), analogDb(top, hz), 1.0, "matched high-pass keeps 12 dB/oct");
    s.shape = Shape::LowShelf; s.gainDb = -12;
    near(responseDb(s, 48000, 1), -12, 0.001, "low shelf finite attenuation");
    near(responseDb(s, 48000, 20000), 0, 0.001, "low shelf leaves highs");
    s.shape = Shape::HighShelf;
    near(responseDb(s, 48000, 22000), -12, 0.001, "high shelf finite attenuation");
    s.enabled = false;
    near(responseDb(s, 48000, 1000), 0, 1e-9, "disabled response unity");

    Equalizer eq;
    Settings settings;
    eq.prepare(48000); eq.setSettings(settings, true);
    std::vector<float> l(8192, 0.25f), r(8192, -0.125f);
    float* channels[] = {l.data(), r.data()};
    eq.process(channels, 2, static_cast<int>(l.size()));
    near(l.back(), 0.25, 1e-6, "flat EQ transparent");
    near(r.back(), -0.125, 1e-6, "stereo independent");
    settings.bands[0].shape = Shape::HighPass; settings.bands[0].frequency = 1000; settings.bands[0].slope = 96;
    eq.setSettings(settings, true);
    eq.process(channels, 2, static_cast<int>(l.size()));
    check(std::abs(l.back()) < 1e-6, "high pass removes DC in actual processing");
    settings.bypass = true; eq.setSettings(settings, true);
    std::fill(l.begin(), l.end(), 0.25f); std::fill(r.begin(), r.end(), -0.125f);
    eq.process(channels, 2, static_cast<int>(l.size()));
    near(l.back(), 0.25, 1e-6, "global bypass transparent");

    settings = {};
    near(estimateTailSeconds(settings, 48000), 0, 1e-9, "flat EQ has no tail");
    settings.bands[0].frequency = 20.601722;
    settings.bands[0].gainDb = 24;
    settings.bands[0].q = 18;
    const auto tail = estimateTailSeconds(settings, 48000);
    check(tail > 10, "resonant bass tail exceeds two seconds");
    eq.prepare(48000); eq.setSettings(settings, true);
    double phase = 0;
    for (int block = 0; block < 2000; ++block) {
        for (int i = 0; i < 256; ++i) {
            l[i] = static_cast<float>(0.01 * std::sin(phase));
            phase += 2.0 * 3.141592653589793 * 20.601722 / 48000;
        }
        eq.process(channels, 1, 256);
    }
    for (int block = 0; block <= static_cast<int>(std::ceil(tail * 48000 / 256)); ++block) {
        std::fill(l.begin(), l.begin() + 256, 0.0f);
        eq.process(channels, 1, 256);
    }
    for (int i = 0; i < 256; ++i) check(std::abs(l[i]) < 1e-6f, "signal decays within advertised tail");

    std::mt19937 rng(314);
    std::uniform_real_distribution<float> noise(-0.05f, 0.05f);
    for (double rate : {8000.0, 22050.0, 44100.0, 48000.0, 96000.0, 192000.0}) {
        eq.prepare(rate); settings = {};
        for (int block = 0; block < 180; ++block) {
            for (int band = 0; band < 5; ++band) {
                auto& b = settings.bands[static_cast<size_t>(band)];
                b.shape = static_cast<Shape>((block / 3 + band) % 7);
                b.frequency = block % 2 ? 20.0 : 20000.0;
                b.q = block % 2 ? 0.1 : 18;
                b.gainDb = block % 2 ? -24 : 24;
                b.slope = block % 2 ? 12 : 96;
                b.enabled = block % 7 != 0;
                b.placement = static_cast<Placement>((block + band) % 5);
            }
            settings.bypass = block % 11 == 0;
            eq.setSettings(settings);
            for (int i = 0; i < 256; ++i) { l[i] = noise(rng); r[i] = noise(rng); }
            eq.process(channels, 2, 256);
            for (int i = 0; i < 256; ++i) check(std::isfinite(l[i]) && std::isfinite(r[i]), "automation stays finite");
        }
    }
    auto sine = [](std::vector<float>& left, std::vector<float>& right, double hz, float amplitude, double rightPhase) {
        for (int i = 0; i < static_cast<int>(left.size()); ++i) {
            const auto sample = amplitude * std::sin(2.0 * 3.141592653589793 * hz * i / 48000.0);
            left[static_cast<size_t>(i)] = static_cast<float>(sample);
            right[static_cast<size_t>(i)] = static_cast<float>(amplitude * std::sin(2.0 * 3.141592653589793 * hz * i / 48000.0 + rightPhase));
        }
    };
    auto rmsFrom = [](const std::vector<float>& buffer, int start) {
        double sum = 0;
        for (int i = start; i < static_cast<int>(buffer.size()); ++i) sum += static_cast<double>(buffer[static_cast<size_t>(i)]) * buffer[static_cast<size_t>(i)];
        return std::sqrt(sum / (buffer.size() - static_cast<size_t>(start)));
    };
    auto isolated = [] {
        Settings next;
        for (auto& band : next.bands) band.enabled = false;
        next.bands[0].enabled = true;
        next.bands[0].frequency = 1000;
        next.bands[0].q = 0.7071067811865476;
        next.bands[0].shape = Shape::Bell;
        return next;
    };
    {
        Equalizer flat;
        flat.prepare(48000);
        flat.setSettings({}, true);
        std::vector<float> left(8192), right(8192), dryLeft, dryRight;
        sine(left, right, 440, 0.2f, 0.4);
        dryLeft = left; dryRight = right;
        float* channels[] = {left.data(), right.data()};
        flat.process(channels, 2, static_cast<int>(left.size()));
        for (int i = 4096; i < 8192; ++i) {
            near(left[static_cast<size_t>(i)], dryLeft[static_cast<size_t>(i)], 1.0e-5, "null test at default, left");
            near(right[static_cast<size_t>(i)], dryRight[static_cast<size_t>(i)], 1.0e-5, "null test at default, right");
        }
    }
    {
        Settings placed = isolated();
        placed.bands[0].placement = Placement::Mid;
        Equalizer mid;
        mid.prepare(48000); mid.setSettings(placed, true);
        std::vector<float> left(4096, 0.2f), right(4096, -0.15f);
        float* channels[] = {left.data(), right.data()};
        mid.process(channels, 2, 4096);
        for (int i = 2048; i < 4096; ++i) {
            near(left[static_cast<size_t>(i)], 0.2, 1.0e-4, "flat Mid placement null, left");
            near(right[static_cast<size_t>(i)], -0.15, 1.0e-4, "flat Mid placement null, right");
        }
        placed.bands[0].placement = Placement::Side;
        mid.setSettings(placed, true);
        std::fill(left.begin(), left.end(), 0.2f); std::fill(right.begin(), right.end(), -0.15f);
        mid.process(channels, 2, 4096);
        for (int i = 2048; i < 4096; ++i) {
            near(left[static_cast<size_t>(i)], 0.2, 1.0e-4, "flat Side placement null, left");
            near(right[static_cast<size_t>(i)], -0.15, 1.0e-4, "flat Side placement null, right");
        }
    }
    {
        Settings leftOnly = isolated();
        leftOnly.bands[0].gainDb = 12;
        leftOnly.bands[0].placement = Placement::Left;
        Equalizer eq;
        eq.prepare(48000); eq.setSettings(leftOnly, true);
        std::vector<float> left(16384), right(16384);
        sine(left, right, 1000, 0.05f, 0.7);
        auto dryRight = right;
        float* channels[] = {left.data(), right.data()};
        eq.process(channels, 2, 16384);
        for (int i = 0; i < 16384; ++i) check(right[static_cast<size_t>(i)] == dryRight[static_cast<size_t>(i)], "Left placement leaves the right channel untouched");
        check(rmsFrom(left, 8192) > 0.10, "Left placement boosts the left channel");
        Settings rightOnly = leftOnly;
        rightOnly.bands[0].placement = Placement::Right;
        eq.prepare(48000); eq.setSettings(rightOnly, true);
        sine(left, right, 1000, 0.05f, 0.7);
        auto dryLeft = left;
        eq.process(channels, 2, 16384);
        for (int i = 0; i < 16384; ++i) check(left[static_cast<size_t>(i)] == dryLeft[static_cast<size_t>(i)], "Right placement leaves the left channel untouched");
        check(rmsFrom(right, 8192) > 0.10, "Right placement boosts the right channel");
    }
    {
        Settings mid = isolated();
        mid.bands[0].gainDb = 12;
        mid.bands[0].placement = Placement::Mid;
        Equalizer eq;
        eq.prepare(48000); eq.setSettings(mid, true);
        std::vector<float> left(16384), right(16384);
        sine(left, right, 1000, 0.05f, 3.141592653589793);
        auto dryLeft = left, dryRight = right;
        float* channels[] = {left.data(), right.data()};
        eq.process(channels, 2, 16384);
        for (int i = 8192; i < 16384; ++i) {
            near(left[static_cast<size_t>(i)], dryLeft[static_cast<size_t>(i)], 1.0e-6, "Mid placement ignores a pure side signal");
            near(right[static_cast<size_t>(i)], dryRight[static_cast<size_t>(i)], 1.0e-6, "Mid placement ignores a pure side signal, right");
        }
        sine(left, right, 1000, 0.05f, 0);
        eq.process(channels, 2, 16384);
        check(rmsFrom(left, 8192) > 0.10 && rmsFrom(right, 8192) > 0.10, "Mid placement boosts a centered signal on both channels");
        for (int i = 8192; i < 16384; ++i) near(left[static_cast<size_t>(i)], right[static_cast<size_t>(i)], 1.0e-5, "Mid placement keeps a centered signal centered");
        Settings side = mid;
        side.bands[0].placement = Placement::Side;
        eq.prepare(48000); eq.setSettings(side, true);
        sine(left, right, 1000, 0.05f, 0);
        dryLeft = left; dryRight = right;
        eq.process(channels, 2, 16384);
        for (int i = 0; i < 16384; ++i) {
            check(left[static_cast<size_t>(i)] == dryLeft[static_cast<size_t>(i)], "Side placement leaves a centered signal untouched");
            check(right[static_cast<size_t>(i)] == dryRight[static_cast<size_t>(i)], "Side placement leaves a centered right channel untouched");
        }
        sine(left, right, 1000, 0.05f, 3.141592653589793);
        eq.process(channels, 2, 16384);
        check(rmsFrom(left, 8192) > 0.10, "Side placement boosts a pure side signal");
        side.bands[0].shape = Shape::HighPass;
        side.bands[0].gainDb = 0;
        side.bands[0].slope = 96;
        side.bands[0].frequency = 1000;
        eq.prepare(48000); eq.setSettings(side, true);
        std::vector<float> lowLeft(48000), lowRight(48000);
        sine(lowLeft, lowRight, 80, 0.2f, 3.141592653589793);
        float* low[] = {lowLeft.data(), lowRight.data()};
        eq.process(low, 2, 48000);
        check(rmsFrom(lowLeft, 36000) < 1.0e-3, "Side high-pass removes low side information");
        eq.prepare(48000); eq.setSettings(side, true);
        sine(lowLeft, lowRight, 80, 0.2f, 0);
        auto centered = lowLeft;
        eq.process(low, 2, 48000);
        for (int i = 0; i < 48000; ++i) check(lowLeft[static_cast<size_t>(i)] == centered[static_cast<size_t>(i)], "Side high-pass keeps low mid information");
    }
    std::cout << "PASS: " << checks << " checks\n";
}
