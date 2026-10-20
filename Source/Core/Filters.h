#pragma once
#include <array>
namespace ote {
enum class Shape { Bell, LowShelf, HighShelf, HighPass, LowPass, Notch, BandPass };
struct BandSettings {
    bool enabled = true;
    Shape shape = Shape::Bell;
    double frequency = 1000.0;
    double gainDb = 0.0;
    double q = 0.7071067811865476;
    int slope = 12;
};
bool usesGain(Shape shape) noexcept;
bool isCut(Shape shape) noexcept;
bool usesQ(const BandSettings& settings) noexcept;
struct Coefficients {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double magnitude(double radians) const noexcept;
};
struct FilterDesign {
    std::array<Coefficients, 8> stages {};
    int count = 1;
};
FilterDesign designFilter(const BandSettings& settings, double sampleRate) noexcept;
double responseDb(const BandSettings& settings, double sampleRate, double frequency) noexcept;
class Biquad {
public:
    Coefficients coefficients;
    double process(double sample) noexcept;
    void reset() noexcept { x1 = x2 = y1 = y2 = 0; }
private:
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
};
}
