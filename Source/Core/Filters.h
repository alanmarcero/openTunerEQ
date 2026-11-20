#pragma once
#include <array>
namespace ote {
enum class Shape { Bell, LowShelf, HighShelf, HighPass, LowPass, Notch, BandPass };
enum class Placement { Stereo, Left, Right, Mid, Side };
enum class QMode { Symmetric, Constant, Proportional };
struct BandSettings {
    bool enabled = true;
    Shape shape = Shape::Bell;
    double frequency = 1000.0;
    double gainDb = 0.0;
    double q = 0.7071067811865476;
    int slope = 12;
    Placement placement = Placement::Stereo;
    QMode qMode = QMode::Symmetric;
    bool compensatePhase = false;
    double dynamicDb = 0.0;
    double drive = 0.0;
};
bool usesGain(Shape shape) noexcept;
bool isCut(Shape shape) noexcept;
bool usesQ(const BandSettings& settings) noexcept;
struct Coefficients {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double svfA1 = 0.25, svfA2 = 0.25, svfA3 = 0.25, m0 = 1, m1 = 0, m2 = 0;
    double magnitude(double radians) const noexcept;
};
// The hints pass exact 1 + a1 + a2 and b0 + b1 + b2 when cancellation would lose them.
Coefficients directForm(double b0, double b1, double b2, double a1, double a2, double dPosHint = 0.0, double sumBHint = -1.0) noexcept;
struct FilterDesign {
    std::array<Coefficients, 8> stages {};
    int count = 1;
};
FilterDesign designFilter(const BandSettings& settings, double sampleRate) noexcept;
// All-pass network that matches a cut's passband phase, for the channel the cut does not touch.
FilterDesign designPhaseCompensator(const BandSettings& settings, double sampleRate) noexcept;
double responseDb(const BandSettings& settings, double sampleRate, double frequency) noexcept;
class Biquad {
public:
    Coefficients coefficients;
    double process(double sample) noexcept;
    void reset() noexcept { ic1eq = ic2eq = 0; }
private:
    double ic1eq = 0, ic2eq = 0;
};
}
