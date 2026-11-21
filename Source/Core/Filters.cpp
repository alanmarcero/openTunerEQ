#include "Filters.h"
#include "Notes.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
namespace ote {
bool usesGain(Shape s) noexcept { return s == Shape::Bell || s == Shape::LowShelf || s == Shape::HighShelf; }
bool isCut(Shape s) noexcept { return s == Shape::HighPass || s == Shape::LowPass; }
bool usesQ(const BandSettings& s) noexcept {
    return usesGain(s.shape) || s.shape == Shape::Notch || s.shape == Shape::BandPass || (isCut(s.shape) && s.slope == 12);
}
bool changesGain(const BandSettings& s) noexcept { return usesGain(s.shape) && (s.gainDb != 0.0 || s.dynamicDb != 0.0); }
bool usesQLaw(const BandSettings& s) noexcept { return s.shape == Shape::Bell && changesGain(s); }
bool usesDynamics(const BandSettings& s) noexcept { return usesGain(s.shape) && s.dynamicDb != 0.0; }
bool isOneSidedCut(const BandSettings& s) noexcept { return isCut(s.shape) && s.placement != Placement::Stereo; }
Coefficients directForm(double b0, double b1, double b2, double a1, double a2, double dPosHint, double sumBHint) noexcept {
    const auto dPos = dPosHint > 0.0 ? dPosHint : std::max(1.0e-30, 1.0 + a1 + a2);
    const auto dNeg = std::max(1.0e-30, 1.0 - a1 + a2);
    const auto svfA1 = 0.25 * dNeg;
    const auto svfA2 = 0.25 * std::sqrt(dPos * dNeg);
    const auto svfA3 = 0.25 * dPos;
    const auto m0 = (b0 - b1 + b2) / dNeg;
    const auto m1 = ((b0 - b2) - m0 * (1.0 - a2)) / std::max(1.0e-30, 2.0 * svfA2);
    const auto sumB = sumBHint >= 0.0 ? sumBHint : (b0 + b1 + b2);
    const auto m2 = sumB / dPos - m0;
    return {b0, b1, b2, a1, a2, svfA1, svfA2, svfA3, m0, m1, m2};
}
namespace {
struct MatchedPoles {
    double a1 = 0, a2 = 0, dPos = 1, A0 = 1, A1 = 1, A2 = 0;
};

MatchedPoles makeMatchedPoles(double poleQ, double w0) noexcept {
    const auto zeta = 1.0 / (2.0 * poleQ);
    const auto radius = std::exp(-zeta * w0);
    double ringing = 0.0;
    double dPos = 0.0;
    if (zeta <= 1.0) {
        const auto wd = w0 * std::sqrt(1.0 - zeta * zeta);
        const auto oneMinusR = -std::expm1(-zeta * w0);
        const auto sinHalf = std::sin(0.5 * wd);
        ringing = std::cos(wd);
        dPos = oneMinusR * oneMinusR + 4.0 * radius * sinHalf * sinHalf;
    }
    if (zeta > 1.0) {
        const auto delta = w0 * std::sqrt(zeta * zeta - 1.0);
        ringing = std::cosh(delta);
        dPos = (-std::expm1(-zeta * w0 - delta)) * (-std::expm1(-zeta * w0 + delta));
    }
    const auto a1 = -2.0 * radius * ringing;
    const auto a2 = radius * radius;
    const auto dNeg = 1.0 - a1 + a2;
    return {a1, a2, dPos, dPos * dPos, dNeg * dNeg, -4.0 * a2};
}

// Vicanek, "Matched Second Order Digital Filters" (2016). The poles come from impulse invariance and the
// numerator makes the magnitude equal the analog prototype at DC, at hz and at Nyquist. The bilinear
// transform instead squeezes every response into 0..Nyquist, so bells narrow and lean near the top.
Coefficients makeMatchedSection(Shape shape, double hz, double gainDb, double q, QMode qMode, double rate) noexcept {
    using Complex = std::complex<double>;
    const auto a = std::pow(10.0, gainDb / 40.0);
    double poleQ = q;
    if (shape == Shape::Bell) {
        if (qMode == QMode::Symmetric) poleQ = q / std::sqrt(a);
        if (qMode == QMode::Proportional) poleQ = q * std::sqrt(std::max(a, 1.0 / a));
    }
    const auto analogPowerAt = [&](double frequency) {
        const Complex s(0.0, frequency / hz);
        const auto denominator = s * s + s / poleQ + 1.0;
        switch (shape) {
        case Shape::Bell: return std::norm((s * s + s * (a * a / poleQ) + 1.0) / denominator);
        case Shape::HighPass: return std::norm(s * s / denominator);
        case Shape::Notch: return std::norm((s * s + 1.0) / denominator);
        case Shape::BandPass: return std::norm(s / q / denominator);
        default: return std::norm(1.0 / denominator);
        }
    };

    const auto w0 = 2.0 * std::numbers::pi * hz / rate;
    const auto poles = makeMatchedPoles(poleQ, w0);
    const auto p1 = std::pow(std::sin(w0 / 2), 2), p0 = 1 - p1, p2 = 4 * p0 * p1;
    const auto denomAtW0 = poles.A0 * p0 + poles.A1 * p1 + poles.A2 * p2;

    // A 3-point fit gives only single zeros. High-pass needs a double zero at DC and notch a double zero at hz.
    if (shape == Shape::HighPass) {
        const auto b0 = std::sqrt(analogPowerAt(hz) * denomAtW0) / (4 * p1);
        return directForm(b0, -2 * b0, b0, poles.a1, poles.a2, poles.dPos, 0.0);
    }
    if (shape == Shape::Notch) {
        const auto b0 = poles.dPos / std::max(1.0e-30, 4.0 * p1);
        return directForm(b0, -2.0 * b0 * std::cos(w0), b0, poles.a1, poles.a2, poles.dPos, poles.dPos);
    }

    const auto B0 = analogPowerAt(0) * poles.A0;
    const auto B1 = analogPowerAt(rate / 2) * poles.A1;
    const auto B2 = (analogPowerAt(hz) * denomAtW0 - B0 * p0 - B1 * p1) / p2;

    // Factor the numerator. The larger root goes to b0, which keeps the zeros inside the unit circle (minimum phase).
    const auto rootB0 = std::sqrt(B0), rootB1 = std::sqrt(B1);
    const auto w = 0.5 * (rootB0 + rootB1);
    const auto b0 = 0.5 * (w + std::sqrt(std::max(0.0, w * w + B2)));
    const auto b1 = 0.5 * (rootB0 - rootB1);
    const auto b2 = b0 > 0 ? -B2 / (4 * b0) : 0.0;
    return directForm(b0, b1, b2, poles.a1, poles.a2, poles.dPos, rootB0);
}

// Design shelves on the side whose analog pole stays at or below hz (so impulse invariance never
// aliases across Nyquist), then invert the minimum-phase section for the opposite gain sign.
Coefficients makeSection(Shape shape, double hz, double gainDb, double q, QMode qMode, double rate) noexcept {
    // Low-pass uses the prewarped ZDF SVF: the matched 3-point fit leaves finite leakage at Nyquist,
    // while the trapezoidal design has an exact double zero there and keeps |H(hz)| exact.
    if (shape == Shape::LowPass) {
        const auto g = std::tan(std::numbers::pi * hz / rate);
        const auto k = 1.0 / q;
        const auto d = 1.0 + g * k + g * g;
        const auto b0 = g * g / d;
        return directForm(b0, 2.0 * b0, b0, 2.0 * (g * g - 1.0) / d, (1.0 - g * k + g * g) / d, 4.0 * b0, 4.0 * b0);
    }
    if (shape != Shape::LowShelf && shape != Shape::HighShelf) return makeMatchedSection(shape, hz, gainDb, q, qMode, rate);
    if (std::abs(gainDb) < 1.0e-9) return {};

    using Complex = std::complex<double>;
    const bool isLow = shape == Shape::LowShelf;
    const bool invert = isLow ? (gainDb < 0.0) : (gainDb > 0.0);
    const auto workDb = invert ? -gainDb : gainDb;
    const auto a = std::pow(10.0, workDb / 40.0);
    const auto rootA = std::sqrt(a);
    const auto poleHz = isLow ? (hz / rootA) : (hz * rootA);
    const auto poles = makeMatchedPoles(q, 2.0 * std::numbers::pi * poleHz / rate);

    const auto analogPowerAt = [&](double frequency) {
        const Complex s(0.0, frequency / hz);
        const auto damp = (rootA / q) * s;
        if (isLow) return std::norm(a * (s * s + damp + a) / (a * s * s + damp + 1.0));
        return std::norm(a * (a * s * s + damp + 1.0) / (s * s + damp + a));
    };

    const auto wm = 2.0 * std::numbers::pi * hz / rate;
    const auto p1 = std::pow(std::sin(wm * 0.5), 2), p0 = 1.0 - p1, p2 = 4.0 * p0 * p1;
    const auto B0 = analogPowerAt(0.0) * poles.A0;
    const auto B1 = analogPowerAt(rate * 0.5) * poles.A1;
    const auto B2 = (analogPowerAt(hz) * (poles.A0 * p0 + poles.A1 * p1 + poles.A2 * p2) - B0 * p0 - B1 * p1) / p2;
    const auto rootB0 = std::sqrt(B0), rootB1 = std::sqrt(B1);
    const auto w = 0.5 * (rootB0 + rootB1);
    const auto b0 = 0.5 * (w + std::sqrt(std::max(1.0e-30, w * w + B2)));
    const auto b1 = 0.5 * (rootB0 - rootB1);
    const auto b2 = -B2 / (4.0 * b0);
    if (invert) return directForm(1.0 / b0, poles.a1 / b0, poles.a2 / b0, b1 / b0, b2 / b0, rootB0 / b0, poles.dPos / b0);
    return directForm(b0, b1, b2, poles.a1, poles.a2, poles.dPos, rootB0);
}
}
FilterDesign designFilter(const BandSettings& settings, double rate) noexcept {
    rate = std::max(100.0, rate);
    const auto hz = effectiveFrequency(settings.frequency, false, rate, standardPitch);
    const auto gain = std::clamp(settings.gainDb, -24.0, 24.0);
    const auto q = std::clamp(settings.q, 0.1, 18.0);
    FilterDesign result;
    if (isCut(settings.shape)) result.count = std::clamp(settings.slope / 12, 1, 8);
    for (int i = 0; i < result.count; ++i) {
        // Low-Q sections first reduce intermediate levels in steep cascades.
        const auto sectionQ = result.count == 1 ? q : 1.0 / (2.0 * std::cos((2.0 * i + 1.0) * std::numbers::pi / (4.0 * result.count)));
        result.stages[static_cast<size_t>(i)] = makeSection(settings.shape, hz, gain, sectionQ, settings.qMode, rate);
    }
    return result;
}
FilterDesign designPhaseCompensator(const BandSettings& settings, double rate) noexcept {
    FilterDesign result;
    result.count = 0;
    if (!isCut(settings.shape)) return result;
    rate = std::max(100.0, rate);
    const auto hz = effectiveFrequency(settings.frequency, false, rate, standardPitch);
    const auto sections = std::clamp(settings.slope / 12, 1, 8);
    const bool highPass = settings.shape == Shape::HighPass;

    // One section: a first-order all-pass whose passband group delay equals the cut's (1 / (q w0)).
    if (sections == 1) {
        const auto q = std::clamp(settings.q, 0.1, 18.0);
        const auto cornerHz = std::min(highPass ? hz / (2.0 * q) : hz * 2.0 * q, rate * 0.49);
        const auto t = std::tan(std::numbers::pi * cornerHz / rate);
        const auto c = (t - 1.0) / (t + 1.0);
        const auto sign = highPass ? -1.0 : 1.0;
        result.stages[0] = directForm(sign * c, sign, 0.0, c, 0.0);
        result.count = 1;
        return result;
    }

    // n Butterworth sections: n / 2 Butterworth all-pass sections at hz track the passband phase within ~11 degrees.
    const auto allPasses = sections / 2;
    const auto g = std::tan(std::numbers::pi * hz / rate);
    for (int i = 0; i < allPasses; ++i) {
        const auto k = 2.0 * std::cos((2.0 * i + 1.0) * std::numbers::pi / (4.0 * allPasses));
        const auto d = 1.0 + g * k + g * g;
        const auto a1 = 2.0 * (g * g - 1.0) / d, a2 = (1.0 - g * k + g * g) / d;
        result.stages[static_cast<size_t>(i)] = directForm(a2, a1, 1.0, a1, a2);
    }
    result.count = allPasses;
    return result;
}
double Coefficients::magnitude(double w) const noexcept {
    const auto z = std::polar(1.0, -w);
    return std::abs((b0 + b1 * z + b2 * z * z) / (1.0 + a1 * z + a2 * z * z));
}
double responseDb(const BandSettings& settings, double rate, double frequency) noexcept {
    if (!settings.enabled) return 0;
    const auto design = designFilter(settings, rate);
    double magnitude = 1;
    for (int i = 0; i < design.count; ++i)
        magnitude *= design.stages[static_cast<size_t>(i)].magnitude(2 * std::numbers::pi * frequency / rate);
    return 20 * std::log10(std::max(1e-15, magnitude));
}
double Biquad::process(double x) noexcept {
    const auto& c = coefficients;
    const auto v3 = x - ic2eq;
    const auto v1 = c.svfA1 * ic1eq + c.svfA2 * v3;
    const auto v2 = ic2eq + c.svfA2 * ic1eq + c.svfA3 * v3;
    ic1eq = 2.0 * v1 - ic1eq;
    ic2eq = 2.0 * v2 - ic2eq;
    if (std::abs(ic1eq) < 1.0e-30) ic1eq = 0.0;
    if (std::abs(ic2eq) < 1.0e-30) ic2eq = 0.0;
    return c.m0 * x + c.m1 * v1 + c.m2 * v2;
}
}
