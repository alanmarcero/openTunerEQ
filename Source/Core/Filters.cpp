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
    return s.shape == Shape::Bell || s.shape == Shape::Notch || s.shape == Shape::BandPass || (isCut(s.shape) && s.slope == 12);
}
namespace {
// Vicanek, "Matched Second Order Digital Filters" (2016). The poles come from impulse invariance and the
// numerator makes the magnitude equal the analog prototype at DC, at hz and at Nyquist. The bilinear
// transform instead squeezes every response into 0..Nyquist, so bells narrow and lean near the top.
Coefficients makeMatchedSection(Shape shape, double hz, double gainDb, double q, double rate) noexcept {
    using Complex = std::complex<double>;
    const auto a = std::pow(10.0, gainDb / 40.0);
    const auto poleQ = shape == Shape::Bell ? q * a : q;
    const auto analogPowerAt = [&](double frequency) {
        const Complex s(0.0, frequency / hz);
        const auto denominator = s * s + s / poleQ + 1.0;
        switch (shape) {
        case Shape::Bell: return std::norm((s * s + s * a / q + 1.0) / denominator);
        case Shape::HighPass: return std::norm(s * s / denominator);
        case Shape::Notch: return std::norm((s * s + 1.0) / denominator);
        case Shape::BandPass: return std::norm(s / q / denominator);
        default: return std::norm(1.0 / denominator);
        }
    };

    const auto w0 = 2.0 * std::numbers::pi * hz / rate;
    const auto zeta = 1.0 / (2.0 * poleQ);
    const auto ringing = zeta <= 1.0 ? std::cos(w0 * std::sqrt(1.0 - zeta * zeta)) : std::cosh(w0 * std::sqrt(zeta * zeta - 1.0));
    const auto a1 = -2.0 * std::exp(-zeta * w0) * ringing;
    const auto a2 = std::exp(-2.0 * zeta * w0);

    // |H|^2 = (B0 p0 + B1 p1 + B2 p2) / (A0 p0 + A1 p1 + A2 p2), with p1 = sin^2(w/2), p0 = 1 - p1, p2 = 4 p0 p1.
    const auto A0 = (1 + a1 + a2) * (1 + a1 + a2), A1 = (1 - a1 + a2) * (1 - a1 + a2), A2 = -4 * a2;
    const auto p1 = std::pow(std::sin(w0 / 2), 2), p0 = 1 - p1, p2 = 4 * p0 * p1;

    // A 3-point fit gives only single zeros. High-pass needs a double zero at DC and notch a double zero at hz.
    if (shape == Shape::HighPass) {
        const auto b0 = std::sqrt(analogPowerAt(hz) * (A0 * p0 + A1 * p1 + A2 * p2)) / (4 * p1);
        return {b0, -2 * b0, b0, a1, a2};
    }
    if (shape == Shape::Notch) {
        const auto b0 = (1 + a1 + a2) / (2 - 2 * std::cos(w0));
        return {b0, -2 * b0 * std::cos(w0), b0, a1, a2};
    }

    const auto B0 = analogPowerAt(0) * A0;
    const auto B1 = analogPowerAt(rate / 2) * A1;
    const auto B2 = (analogPowerAt(hz) * (A0 * p0 + A1 * p1 + A2 * p2) - B0 * p0 - B1 * p1) / p2;

    // Factor the numerator. The larger root goes to b0, which keeps the zeros inside the unit circle (minimum phase).
    const auto rootB0 = std::sqrt(B0), rootB1 = std::sqrt(B1);
    const auto w = 0.5 * (rootB0 + rootB1);
    const auto b0 = 0.5 * (w + std::sqrt(std::max(0.0, w * w + B2)));
    const auto b1 = 0.5 * (rootB0 - rootB1);
    const auto b2 = b0 > 0 ? -B2 / (4 * b0) : 0.0;
    return {b0, b1, b2, a1, a2};
}

// Shelves keep the cookbook design (W3C Audio EQ Cookbook, S = 1). Their pole sits above or below hz by the gain,
// so it can pass Nyquist, where impulse invariance aliases.
Coefficients makeSection(Shape shape, double hz, double gainDb, double q, double rate) noexcept {
    if (shape != Shape::LowShelf && shape != Shape::HighShelf) return makeMatchedSection(shape, hz, gainDb, q, rate);
    const auto w = 2.0 * std::numbers::pi * hz / rate;
    const auto c = std::cos(w), s = std::sin(w);
    const auto a = std::pow(10.0, gainDb / 40.0);
    const auto t = s * std::sqrt(2.0 * a);
    const auto sign = shape == Shape::LowShelf ? 1.0 : -1.0;
    const auto b0 = a * ((a + 1) - sign * (a - 1) * c + t);
    const auto b1 = sign * 2 * a * ((a - 1) - sign * (a + 1) * c);
    const auto b2 = a * ((a + 1) - sign * (a - 1) * c - t);
    const auto a0 = (a + 1) + sign * (a - 1) * c + t;
    const auto a1 = -sign * 2 * ((a - 1) + sign * (a + 1) * c);
    const auto a2 = (a + 1) + sign * (a - 1) * c - t;
    return {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}
}
FilterDesign designFilter(const BandSettings& settings, double rate) noexcept {
    rate = std::max(100.0, rate);
    const auto hz = effectiveFrequency(settings.frequency, false, rate);
    const auto gain = std::clamp(settings.gainDb, -24.0, 24.0);
    const auto q = std::clamp(settings.q, 0.1, 18.0);
    FilterDesign result;
    if (isCut(settings.shape)) result.count = std::clamp(settings.slope / 12, 1, 8);
    for (int i = 0; i < result.count; ++i) {
        // Low-Q sections first reduce intermediate levels in steep cascades.
        const auto sectionQ = result.count == 1 ? q : 1.0 / (2.0 * std::cos((2.0 * i + 1.0) * std::numbers::pi / (4.0 * result.count)));
        result.stages[static_cast<size_t>(i)] = makeSection(settings.shape, hz, gain, sectionQ, rate);
    }
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
    auto y = c.b0 * x + c.b1 * x1 + c.b2 * x2 - c.a1 * y1 - c.a2 * y2;
    if (std::abs(y) < 1e-30) y = 0;
    x2 = x1; x1 = x; y2 = y1; y1 = y;
    return y;
}
}
