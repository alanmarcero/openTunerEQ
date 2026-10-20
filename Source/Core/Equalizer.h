#pragma once
#include "Filters.h"
#include <array>
namespace ote {
struct Settings {
    std::array<BandSettings, 5> bands {};
    bool snap = true;
    bool bypass = false;
    double outputDb = 0;
};
double estimateTailSeconds(const Settings&, double sampleRate) noexcept;
class Equalizer {
public:
    void prepare(double sampleRate) noexcept;
    void setSettings(const Settings& settings, bool immediately = false) noexcept;
    void process(float* const* channels, int channelCount, int samples) noexcept;
private:
    struct Bank {
        std::array<std::array<Biquad, 8>, 2> filters {};
        BandSettings current;
        int count = 1;
        void reset() noexcept;
        void update(const BandSettings&, double rate, double smoothing) noexcept;
        double process(double sample, int channel) noexcept;
    };
    struct Band {
        std::array<Bank, 2> banks;
        BandSettings target;
        int active = 0;
        int remaining = 0;
        double wet = 1;
    };
    std::array<Band, 5> bands;
    double rate = 48000;
    double smooth = 0;
    double gainSmoothing = 1;
    double fadeStep = 1.0 / 480.0;
    double bypassWet = 1;
    double outputGain = 1;
    double targetOutput = 1;
    bool bypass = false;
    int fadeSamples = 480;
    int controlCounter = 0;
    void updateFilters() noexcept;
};
}
