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
Coefficients makeSection(Shape shape, double hz, double gainDb, double q, double rate) noexcept {
    const auto w = 2.0 * std::numbers::pi * hz / rate;
    const auto c = std::cos(w), s = std::sin(w), alpha = s / (2.0 * q);
    const auto a = std::pow(10.0, gainDb / 40.0);
    double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;
    switch (shape) {
    case Shape::Bell:
        b0 = 1 + alpha * a; b1 = -2 * c; b2 = 1 - alpha * a;
        a0 = 1 + alpha / a; a1 = -2 * c; a2 = 1 - alpha / a; break;
    case Shape::LowPass:
        b0 = (1 - c) / 2; b1 = 1 - c; b2 = b0;
        a0 = 1 + alpha; a1 = -2 * c; a2 = 1 - alpha; break;
    case Shape::HighPass:
        b0 = (1 + c) / 2; b1 = -(1 + c); b2 = b0;
        a0 = 1 + alpha; a1 = -2 * c; a2 = 1 - alpha; break;
    case Shape::Notch:
        b0 = 1; b1 = -2 * c; b2 = 1;
        a0 = 1 + alpha; a1 = -2 * c; a2 = 1 - alpha; break;
    case Shape::BandPass:
        b0 = alpha; b1 = 0; b2 = -alpha;
        a0 = 1 + alpha; a1 = -2 * c; a2 = 1 - alpha; break;
    case Shape::LowShelf: {
        const auto t = s * std::sqrt(2.0 * a);
        b0 = a * ((a + 1) - (a - 1) * c + t);
        b1 = 2 * a * ((a - 1) - (a + 1) * c);
        b2 = a * ((a + 1) - (a - 1) * c - t);
        a0 = (a + 1) + (a - 1) * c + t;
        a1 = -2 * ((a - 1) + (a + 1) * c);
        a2 = (a + 1) + (a - 1) * c - t; break;
    }
    case Shape::HighShelf: {
        const auto t = s * std::sqrt(2.0 * a);
        b0 = a * ((a + 1) + (a - 1) * c + t);
        b1 = -2 * a * ((a - 1) + (a + 1) * c);
        b2 = a * ((a + 1) + (a - 1) * c - t);
        a0 = (a + 1) - (a - 1) * c + t;
        a1 = 2 * ((a - 1) - (a + 1) * c);
        a2 = (a + 1) - (a - 1) * c - t; break;
    }
    }
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
