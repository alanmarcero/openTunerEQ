#pragma once
#include "Filters.h"
#include "Notes.h"
#include <array>
namespace ote {
struct Settings {
    std::array<BandSettings, 5> bands {};
    bool snap = true;
    double a4 = standardPitch;
    bool bypass = false;
    double outputDb = 0;
    double scale = 1.0;
    bool autoGain = false;
    bool phaseCompensation = false;
    double dynamicThresholdDb = -24;
    int soloBand = -1;
    bool operator==(const Settings&) const = default;
};
double estimateTailSeconds(const Settings&, double sampleRate) noexcept;
// The band as the engine plays it: Solo plays the soloed band as a band pass and mutes the others.
BandSettings audibleBand(const Settings&, size_t index) noexcept;
class Equalizer {
public:
    void prepare(double sampleRate) noexcept;
    void setSettings(const Settings& settings, bool immediately = false) noexcept;
    void process(float* const* channels, int channelCount, int samples) noexcept;
    // The gain the dynamic EQ adds to a band now, in dB.
    double dynamicGainDb(size_t band) const noexcept;
    // The amplitude the band's detector hears, or zero while the detector is off.
    double detectorLevel(size_t band) const noexcept;
private:
    struct Bank {
        std::array<std::array<Biquad, 8>, 2> filters {};
        std::array<std::array<Biquad, 4>, 2> companions {};
        BandSettings current;
        int count = 1;
        int companionCount = 0;
        double designedRate = -1.0;
        void reset() noexcept;
        void update(const BandSettings&, double rate, double smoothing) noexcept;
        double process(double sample, int channel) noexcept;
        double align(double sample, int channel) noexcept;
    };
    struct Band {
        std::array<Bank, 2> banks;
        BandSettings target;
        int active = 0;
        int remaining = 0, priming = 0;
        double wet = 1;
        std::array<Biquad, 2> detectors {};
        double envelope = 0, detectorHz = -1, detectorQ = -1, dynamicGainDb = 0;
        std::array<double, 2> driveHistory {};
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
    bool autoGain = false;
    std::array<std::array<Biquad, 2>, 2> inputWeighting {}, outputWeighting {};
    double inputPower = 0, outputPower = 0, loudnessSmoothing = 0, autoGainLevel = 1;
    std::array<double, 2> clipHistory {};
    double dynamicThresholdDb = -24, dynamicAttack = 0, dynamicRelease = 0;
    int fadeSamples = 480;
    int controlCounter = 0;
    void updateFilters() noexcept;
};
}
