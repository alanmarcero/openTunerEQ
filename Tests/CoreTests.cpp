#include "Notes.h"
#include "Filters.h"
#include "Equalizer.h"
#include <cmath>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <numbers>
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
        const auto bellPoleQ = b.q / std::sqrt(a);
        const auto h = b.shape == Shape::Bell ? (j * j + j * (a * a / bellPoleQ) + 1.0) / (j * j + j / bellPoleQ + 1.0)
                     : b.shape == Shape::LowPass ? 1.0 / (j * j + j / b.q + 1.0)
                     : j * j / (j * j + j / b.q + 1.0);
        return 20 * std::log10(std::abs(h));
    };
    BandSettings top;
    top.shape = Shape::Bell; top.frequency = 16000; top.gainDb = 12; top.q = 0.7071;
    for (double hz : {4000.0, 8000.0, 12000.0, 19000.0}) near(responseDb(top, 44100, hz), analogDb(top, hz), 1.5, "matched bell near Nyquist");
    top.shape = Shape::LowPass; top.frequency = 10000; top.q = 4;
    for (double hz : {5000.0, 10000.0}) near(responseDb(top, 44100, hz), analogDb(top, hz), 1.0, "matched low-pass passband and resonance");
    check(responseDb(top, 44100, 22050) < -250.0, "matched low-pass reaches -inf dB at Nyquist");
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
    check(tail > 2, "resonant bass tail exceeds two seconds");
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
    {
        Biquad filter;
        BandSettings lowNotch;
        lowNotch.shape = Shape::Notch;
        lowNotch.q = 18.0;
        double maxJump = 0.0;
        double prevOut = 0.0;
        for (int i = 0; i < 16384; ++i) {
            if (i % 16 == 0) {
                const int step = (i / 16) % 24;
                lowNotch.frequency = noteFrequency(24 + (step < 12 ? step * 2 : (24 - step) * 2), standardPitch);
                filter.coefficients = designFilter(lowNotch, 48000.0).stages[0];
            }
            const double in = 0.25 * std::sin(2.0 * 3.141592653589793 * 32.7032 * i / 48000.0);
            const double out = filter.process(in);
            if (i > 0) maxJump = std::max(maxJump, std::abs(out - prevOut));
            prevOut = out;
        }
        check(maxJump < 0.08, "ZDF TPT SVF stays click-free across fast pitch-snapped notch sweeps");

        Biquad subNotch;
        lowNotch.frequency = 20.601722;
        subNotch.coefficients = designFilter(lowNotch, 192000.0).stages[0];
        double dcOut = 0.0;
        for (int i = 0; i < 1600000; ++i) dcOut = subNotch.process(0.5);
        near(dcOut, 0.5, 1.0e-12, "sub-bass high-Q notch preserves exact DC in ZDF TPT SVF");
    }
    {
        BandSettings lowShelf;
        lowShelf.shape = Shape::LowShelf;
        lowShelf.frequency = 100.0;
        lowShelf.gainDb = 6.0;
        lowShelf.q = 4.0;
        check(usesQ(lowShelf), "low shelf enables user Q");
        check(responseDb(lowShelf, 48000.0, 82.0) > 9.5, "resonant low shelf produces Pultec peak");
        check(responseDb(lowShelf, 48000.0, 130.0) < -1.5, "resonant low shelf produces Pultec dip");

        BandSettings airShelf;
        airShelf.shape = Shape::HighShelf;
        airShelf.frequency = 15000.0;
        airShelf.gainDb = 12.0;
        airShelf.q = 2.0;
        check(usesQ(airShelf), "high shelf enables user Q");
        const auto analogHighShelfDb = [](const BandSettings& b, double hz) {
            const std::complex<double> j(0.0, hz / b.frequency);
            const auto a = std::pow(10.0, b.gainDb / 40.0);
            const auto rootA = std::sqrt(a);
            const auto h = a * (a * j * j + (rootA / b.q) * j + 1.0) / (j * j + (rootA / b.q) * j + a);
            return 20.0 * std::log10(std::abs(h));
        };
        for (double hz : {6000.0, 12000.0, 15000.0, 19500.0}) {
            near(responseDb(airShelf, 44100.0, hz), analogHighShelfDb(airShelf, hz), 1.0, "matched air-band high shelf avoids Nyquist cramping");
        }
        BandSettings airCut = airShelf;
        airCut.gainDb = -12.0;
        near(responseDb(airShelf, 44100.0, 12000.0) + responseDb(airCut, 44100.0, 12000.0), 0.0, 1.0e-6, "matched shelf boost/cut reciprocity");
    }
    {
        BandSettings boostBell;
        boostBell.shape = Shape::Bell;
        boostBell.frequency = 1000.0;
        boostBell.gainDb = 24.0;
        boostBell.q = 4.0;
        check(responseDb(boostBell, 48000.0, 1122.46) > 12.0, "symmetric root-Q bell boost stays warm instead of 4x narrow spike");

        BandSettings cutBell = boostBell;
        cutBell.gainDb = -24.0;
        check(responseDb(cutBell, 48000.0, 1259.92) > -1.0, "symmetric root-Q bell cut stays surgical on neighboring notes");

        BandSettings constantBell = boostBell;
        constantBell.qMode = QMode::Constant;
        BandSettings propBell = boostBell;
        propBell.qMode = QMode::Proportional;
        near(responseDb(constantBell, 48000.0, 1000.0), 24.0, 1.0e-6, "constant-Q bell preserves exact center gain");
        near(responseDb(propBell, 48000.0, 1000.0), 24.0, 1.0e-6, "proportional-Q bell preserves exact center gain");
        check(responseDb(boostBell, 48000.0, 1122.46) > responseDb(constantBell, 48000.0, 1122.46)
              && responseDb(constantBell, 48000.0, 1122.46) > responseDb(propBell, 48000.0, 1122.46),
              "Q modes order boost width from warm symmetric to constant to proportional");
    }
    {
        BandSettings lp12;
        lp12.shape = Shape::LowPass;
        lp12.frequency = 4000.0;
        lp12.slope = 12;
        const auto d12 = designFilter(lp12, 48000.0);
        auto totalMag = [&](const FilterDesign& d, double w) {
            double m = 1.0;
            for (int i = 0; i < d.count; ++i) m *= d.stages[static_cast<size_t>(i)].magnitude(w);
            return m;
        };
        const double ratio = totalMag(d12, 3.141592653589793 - 1.0e-4) / totalMag(d12, 3.141592653589793 - 2.0e-4);
        near(ratio, 0.25, 1.0e-3, "12 dB/oct low-pass has exact double zero at Nyquist");

        for (int slope : {48, 96}) {
            BandSettings steepLp = lp12;
            steepLp.slope = slope;
            check(responseDb(steepLp, 48000.0, 24000.0) < -250.0, "steep low-pass eliminates Nyquist stopband leakage");
            check(responseDb(steepLp, 48000.0, 22000.0) < (slope == 48 ? -120.0 : -240.0), "steep low-pass rolls off cleanly into Nyquist");
        }
    }
    {
        // L = sin, R = 0 is M = S = sin / 2. If Mid and Side rotate equally, R stays silent.
        auto rightLeak = [&](Placement placement, Shape shape, int slope, bool compensate) {
            Settings cut = isolated();
            cut.bands[0].shape = shape;
            cut.bands[0].slope = slope;
            cut.bands[0].frequency = shape == Shape::HighPass ? 500.0 : 2000.0;
            cut.bands[0].placement = placement;
            cut.phaseCompensation = compensate;
            Equalizer eq;
            eq.prepare(48000); eq.setSettings(cut, true);
            std::vector<float> left(16384), right(16384);
            sine(left, right, 1000, 0.1f, 0);
            const auto dry = rmsFrom(left, 8192);
            if (placement == Placement::Side) std::fill(right.begin(), right.end(), 0.0f);
            float* channels[] = {left.data(), right.data()};
            eq.process(channels, 2, 16384);
            std::vector<float> difference(16384);
            for (size_t i = 0; i < difference.size(); ++i)
                difference[i] = placement == Placement::Side ? right[i] : left[i] - right[i];
            return rmsFrom(difference, 8192) / dry;
        };
        for (auto shape : {Shape::HighPass, Shape::LowPass}) {
            for (int slope : {12, 24, 48, 96}) {
                check(rightLeak(Placement::Side, shape, slope, false) > 0.15, "uncompensated Side cut smears a one-sided transient");
                check(rightLeak(Placement::Side, shape, slope, true) < 0.12, "phase-compensated Side cut keeps Mid and Side aligned");
                check(rightLeak(Placement::Left, shape, slope, true) < 0.2, "phase-compensated Left cut keeps a centered signal centered");
            }
        }
        check(designPhaseCompensator(isolated().bands[0], 48000).count == 0, "bells need no phase compensation");
    }
    {
        auto levelDb = [&](const Settings& eqSettings, double hz) {
            Equalizer eq;
            eq.prepare(48000); eq.setSettings(eqSettings, true);
            std::vector<float> left(144000), right(144000);
            sine(left, right, hz, 0.05f, 0);
            float* channels[] = {left.data(), right.data()};
            eq.process(channels, 2, 144000);
            return 20 * std::log10(rmsFrom(left, 120000) / (0.05 / std::sqrt(2.0)));
        };
        Settings air = isolated();
        air.bands[0].frequency = 16000; air.bands[0].gainDb = 10;
        air.autoGain = true;
        check(levelDb(air, 50) > -1.5, "auto-gain does not duck sub-bass for an air boost");
        Settings broad = isolated();
        broad.bands[0].shape = Shape::LowShelf; broad.bands[0].frequency = 19000; broad.bands[0].gainDb = 9;
        broad.autoGain = true;
        near(levelDb(broad, 1000), 0, 1.0, "auto-gain matches loudness for a broadband boost");
        broad.bands[0].gainDb = -9;
        near(levelDb(broad, 1000), 0, 1.0, "auto-gain matches loudness for a broadband cut");
    }
    {
        auto dynamicDb = [&](double rangeDb, double hz, float amplitude) {
            Settings dynamic = isolated();
            dynamic.bands[0].q = 4;
            dynamic.bands[0].dynamicDb = rangeDb;
            dynamic.dynamicThresholdDb = -30;
            dynamic.snap = false;
            Equalizer eq;
            eq.prepare(48000); eq.setSettings(dynamic, true);
            std::vector<float> left(48000), right(48000);
            sine(left, right, hz, amplitude, 0);
            float* channels[] = {left.data(), right.data()};
            eq.process(channels, 2, 48000);
            return 20 * std::log10(rmsFrom(left, 36000) / (amplitude / std::sqrt(2.0)));
        };
        near(dynamicDb(-12, 1000, 0.5f), -12, 1.0, "dynamic band suppresses a loud resonance on its note");
        near(dynamicDb(-12, 1000, 0.005f), 0, 0.1, "dynamic band leaves a quiet note untouched");
        near(dynamicDb(-12, 100, 0.5f), 0, 0.5, "dynamic band ignores loud content away from its note");
        near(dynamicDb(6, 1000, 0.5f), 6, 1.0, "dynamic band enhances upward above threshold");
    }
    {
        Settings morph = isolated();
        morph.snap = false;
        morph.bands[0].frequency = 40; morph.bands[0].gainDb = 6;
        Equalizer eq;
        eq.prepare(48000); eq.setSettings(morph, true);
        std::vector<float> left(48000), right(48000);
        sine(left, right, 40, 0.2f, 0);
        float* channels[] = {left.data(), right.data()};
        eq.process(channels, 2, 24000);
        morph.bands[0].shape = Shape::LowShelf;
        eq.setSettings(morph);
        float* rest[] = {left.data() + 24000, right.data() + 24000};
        eq.process(rest, 2, 24000);
        Equalizer settledShelf;
        settledShelf.prepare(48000); settledShelf.setSettings(morph, true);
        std::vector<float> refLeft(48000), refRight(48000);
        sine(refLeft, refRight, 40, 0.2f, 0);
        float* refChannels[] = {refLeft.data(), refRight.data()};
        settledShelf.process(refChannels, 2, 48000);
        double worst = 0;
        for (size_t i = 24000; i < 30000; ++i) worst = std::max(worst, std::abs(static_cast<double>(left[i]) - refLeft[i]));
        check(worst < 0.1, "primed equal-power topology crossfade lands on the settled new shape");

        Settings shelf = isolated();
        shelf.bands[0].shape = Shape::LowShelf; shelf.bands[0].gainDb = 6;
        eq.prepare(48000); eq.setSettings(shelf, true);
        std::vector<float> dc(4800, 0.1f), dcRight(4800, 0.1f);
        float* dcChannels[] = {dc.data(), dcRight.data()};
        eq.process(dcChannels, 2, 2400);
        const auto settled = dc[2399];
        shelf.bypass = true;
        eq.setSettings(shelf);
        float* fade[] = {dc.data() + 2400, dcRight.data() + 2400};
        eq.process(fade, 2, 2400);
        check(std::abs(dc[2400] - settled) < 1.0e-5, "bypass fade starts on an S-curve, not a linear step");
        near(dc[4799], 0.1, 1.0e-6, "bypass fade still lands on the dry signal");
    }
    {
        auto secondHarmonicDb = [&](double drive) {
            Settings driven = isolated();
            driven.snap = false;
            driven.bands[0].gainDb = 12; driven.bands[0].drive = drive;
            Equalizer eq;
            eq.prepare(48000); eq.setSettings(driven, true);
            std::vector<float> left(48000), right(48000);
            sine(left, right, 1000, 0.1f, 0);
            float* channels[] = {left.data(), right.data()};
            eq.process(channels, 2, 48000);
            std::complex<double> fundamental, harmonic;
            for (int i = 24000; i < 48000; ++i) {
                const auto phase = 2.0 * std::numbers::pi * 1000.0 * i / 48000.0;
                fundamental += static_cast<double>(left[static_cast<size_t>(i)]) * std::polar(1.0, -phase);
                harmonic += static_cast<double>(left[static_cast<size_t>(i)]) * std::polar(1.0, -2.0 * phase);
            }
            return 20.0 * std::log10(std::abs(harmonic) / std::abs(fundamental));
        };
        check(secondHarmonicDb(0) < -100, "undriven band adds no harmonics");
        const auto driven = secondHarmonicDb(1);
        check(driven > -50, "driven band adds asymmetric second harmonic");
        check(driven < -10, "driven band saturation stays musical");
    }
    {
        auto hot = [&](float amplitude, double outputDb) {
            Settings loud = isolated();
            loud.bands[0].enabled = false;
            loud.outputDb = outputDb;
            Equalizer eq;
            eq.prepare(48000); eq.setSettings(loud, true);
            std::vector<float> left(96000), right(96000);
            sine(left, right, 7350, amplitude, 0);
            float* channels[] = {left.data(), right.data()};
            eq.process(channels, 2, 96000);
            return left;
        };
        const auto clipped = hot(0.9f, 12);
        double peak = 0, total = 0, musical = 0;
        for (size_t i = 48000; i < 96000; ++i) {
            peak = std::max(peak, std::abs(static_cast<double>(clipped[i])));
            total += static_cast<double>(clipped[i]) * clipped[i];
        }
        for (int harmonic = 0; harmonic <= 3; ++harmonic) {
            std::complex<double> bin;
            for (int i = 48000; i < 96000; ++i)
                bin += static_cast<double>(clipped[static_cast<size_t>(i)]) * std::polar(1.0, -2.0 * std::numbers::pi * 7350.0 * harmonic * i / 48000.0);
            musical += std::norm(bin) / 48000.0 * (harmonic == 0 || harmonic == 3 ? 1.0 : 2.0);
        }
        check(peak <= 1.4, "output ceiling holds at the soft-saturation limit");
        check(10.0 * std::log10((total - musical) / total) < -16, "antialiased output saturation keeps folded harmonics down");
        const auto quiet = hot(0.1f, 0);
        std::vector<float> reference(96000), unused(96000);
        sine(reference, unused, 7350, 0.1f, 0);
        double quietError = 0;
        for (size_t i = 0; i < 96000; ++i) quietError = std::max(quietError, std::abs(static_cast<double>(quiet[i]) - reference[i]));
        check(quietError < 1.0e-7, "soft saturation is transparent at normal levels");
    }
    {
        Settings soloed;
        soloed.bands[2].q = 0.5;
        soloed.soloBand = 2;
        check(audibleBand(soloed, 2).enabled && audibleBand(soloed, 2).shape == Shape::BandPass && audibleBand(soloed, 2).q == 1.2,
              "solo plays the soloed band as a band pass with Q of at least 1.2");
        check(!audibleBand(soloed, 0).enabled && !audibleBand(soloed, 4).enabled, "solo mutes the other bands");
        BandSettings bell, proportional;
        proportional.qMode = QMode::Proportional;
        check(!changesGain(bell) && !usesQLaw(bell) && !usesDynamics(bell), "a flat bell changes nothing");
        near(responseDb(bell, 48000, 1500), responseDb(proportional, 48000, 1500), 1e-9, "a flat bell ignores the Q law");
        bell.gainDb = proportional.gainDb = 6;
        check(usesQLaw(bell) && std::abs(responseDb(bell, 48000, 1500) - responseDb(proportional, 48000, 1500)) > 0.01, "a boosted bell follows the Q law");
        bell.gainDb = 0; bell.dynamicDb = 3;
        check(usesDynamics(bell) && changesGain(bell), "a dynamic range alone changes gain");
        BandSettings cut;
        cut.shape = Shape::HighPass; cut.placement = Placement::Side;
        check(isOneSidedCut(cut) && !changesGain(cut), "a Side high-pass is a one-sided cut");
        cut.placement = Placement::Stereo;
        check(!isOneSidedCut(cut), "a Stereo high-pass is not one-sided");
        soloed.soloBand = -1;
        check(audibleBand(soloed, 2).shape == Shape::Bell && audibleBand(soloed, 0).enabled, "without solo every band plays as set");
        check(std::isfinite(noteFrequency(69, 0.0)) && noteFrequency(69, 0.0) > 0.0, "non-positive A4 reference falls back safely");
        check(std::isfinite(frequencyNote(440.0, -440.0)), "negative A4 reference avoids NaN");
    }
    std::cout << "PASS: " << checks << " checks\n";
}
