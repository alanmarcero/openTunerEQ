#include "Equalizer.h"
#include "Notes.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <numbers>
namespace ote {
namespace {
double approach(double value, double target, double step) noexcept {
    return value + std::clamp(target - value, -step, step);
}
bool topologyDiffers(const BandSettings& a, const BandSettings& b) noexcept {
    return a.shape != b.shape || (isCut(a.shape) && a.slope != b.slope) || a.compensatePhase != b.compensatePhase;
}
double logCosh(double u) noexcept {
    const auto magnitude = std::abs(u);
    return magnitude + std::log1p(std::exp(-2.0 * magnitude)) - std::numbers::ln2;
}
// First-order antiderivative antialiasing, centred on the current sample so it adds no half-sample delay.
template <typename Curve, typename Integral>
double centredAdaa(double x, double& previous, Curve curve, Integral integral) noexcept {
    const auto lo = 0.5 * (x + previous), hi = 1.5 * x - 0.5 * previous;
    previous = x;
    if (std::abs(hi - lo) < 1.0e-7) return curve(x);
    return (integral(hi) - integral(lo)) / (hi - lo);
}
// Biased tanh with unity small-signal gain: odd and even harmonics like a driven transformer or tube stage.
double saturate(double x, double& previous, double drive) noexcept {
    const auto k = 1.0 + 4.0 * drive, bias = 0.3 * drive;
    const auto offset = std::tanh(bias), scale = 1.0 / (k * (1.0 - offset * offset));
    return centredAdaa(x, previous,
                       [&](double v) { return scale * (std::tanh(k * v + bias) - offset); },
                       [&](double v) { return scale * (logCosh(k * v + bias) / k - v * offset); });
}
// g(x) = s y / sqrt(1 + y^2) * sum c_k t^k with y = x / s, t = y^2 / (1 + y^2) and c_k the series of 1 / sqrt(1 - t).
// It is x to within float precision below about -14 dBFS and approaches the ceiling smoothly, with an exact antiderivative.
struct SoftCeiling {
    static constexpr int order = 12;
    static constexpr double ceiling = 1.4;
    std::array<double, order + 1> series {}, integral {};
    double scale = 0;
    SoftCeiling() noexcept {
        double term = 1, sum = 0;
        for (int k = 0; k <= order; ++k) {
            series[static_cast<size_t>(k)] = term; sum += term;
            term *= (2.0 * k + 1.0) / (2.0 * k + 2.0);
        }
        scale = ceiling / sum;
        for (int j = 0; j <= order; ++j) {
            double choose = 1;
            for (int k = j; k <= order; ++k) {
                integral[static_cast<size_t>(j)] += (j % 2 == 0 ? 1.0 : -1.0) * series[static_cast<size_t>(k)] * choose;
                choose *= (k + 1.0) / (k + 1.0 - j);
            }
        }
    }
    double curve(double x) const noexcept {
        const auto y = x / scale, t = y * y / (1.0 + y * y);
        double sum = 0;
        for (int k = order; k >= 0; --k) sum = sum * t + series[static_cast<size_t>(k)];
        return scale * y / std::sqrt(1.0 + y * y) * sum;
    }
    double antiderivative(double x) const noexcept {
        const auto y = x / scale, logR = 0.5 * std::log1p(y * y);
        auto total = integral[0] * y * y / (std::exp(logR) + 1.0);
        for (int j = 1; j <= order; ++j) total -= integral[static_cast<size_t>(j)] * std::expm1((1.0 - 2.0 * j) * logR) / (2.0 * j - 1.0);
        return scale * scale * total;
    }
    double process(double x, double& previous) const noexcept {
        // The centred interval spans at most twice these magnitudes, where the curve is the identity in double precision.
        if (std::max(std::abs(x), std::abs(previous)) < 0.04) {
            previous = x;
            return x;
        }
        return centredAdaa(x, previous, [this](double v) { return curve(v); }, [this](double v) { return antiderivative(v); });
    }
};
const SoftCeiling softCeiling;
}
double estimateTailSeconds(const Settings& settings, double sampleRate) noexcept {
    if (settings.bypass) return 0;
    double slowest = 0, boost = 0;
    int sections = 0;
    for (auto band : settings.bands) {
        if (!band.enabled || (usesGain(band.shape) && std::abs(band.gainDb) < 0.001 && band.dynamicDb == 0.0)) continue;
        if (usesGain(band.shape)) band.gainDb += std::max(0.0, band.dynamicDb);
        band.frequency = effectiveFrequency(band.frequency, settings.snap, sampleRate, settings.a4);
        const auto design = designFilter(band, sampleRate);
        sections += design.count;
        if (usesGain(band.shape)) boost += std::max(0.0, band.gainDb);
        for (int i = 0; i < design.count; ++i) {
            const auto c = design.stages[static_cast<size_t>(i)];
            const auto root = std::sqrt(std::complex<double>(c.a1 * c.a1 - 4 * c.a2, 0));
            slowest = std::max({slowest, std::abs((-c.a1 + root) * 0.5), std::abs((-c.a1 - root) * 0.5)});
        }
    }
    if (sections == 0 || slowest <= 0) return 0;
    // Pole decay plus margin for cascaded sections and boosted internal levels.
    const auto timeConstant = -1.0 / (sampleRate * std::log(std::min(slowest, 1.0 - 1e-12)));
    return timeConstant * (std::log(1e9) + 2.0 * sections + boost * std::log(10.0) / 20.0);
}
BandSettings audibleBand(const Settings& settings, size_t index) noexcept {
    auto band = settings.bands[index];
    if (settings.soloBand < 0 || settings.soloBand >= 5) return band;
    band.enabled = static_cast<int>(index) == settings.soloBand;
    if (!band.enabled) return band;
    band.shape = Shape::BandPass;
    band.q = std::max(1.2, band.q);
    return band;
}
void Equalizer::Bank::reset() noexcept {
    designedRate = -1.0;
    for (auto& channel : filters) for (auto& filter : channel) filter.reset();
    for (auto& channel : companions) for (auto& filter : channel) filter.reset();
}
void Equalizer::Bank::update(const BandSettings& target, double rate, double smoothing) noexcept {
    if (designedRate == rate
        && std::abs(current.frequency - target.frequency) < 1.0e-4
        && std::abs(current.gainDb - target.gainDb) < 1.0e-4
        && std::abs(current.q - target.q) < 1.0e-4
        && current.shape == target.shape
        && current.slope == target.slope
        && current.qMode == target.qMode
        && current.compensatePhase == target.compensatePhase) {
        return;
    }
    if (smoothing <= 0.0 || (std::abs(current.frequency - target.frequency) < 1.0e-4
        && std::abs(current.gainDb - target.gainDb) < 1.0e-4
        && std::abs(current.q - target.q) < 1.0e-4)) {
        current.frequency = target.frequency;
        current.gainDb = target.gainDb;
        current.q = target.q;
    } else {
        current.frequency = std::exp(std::log(target.frequency) + smoothing * (std::log(current.frequency) - std::log(target.frequency)));
        current.gainDb = target.gainDb + smoothing * (current.gainDb - target.gainDb);
        current.q = target.q + smoothing * (current.q - target.q);
    }
    current.shape = target.shape;
    current.slope = target.slope;
    current.qMode = target.qMode;
    current.compensatePhase = target.compensatePhase;
    const auto design = designFilter(current, rate);
    count = design.count;
    designedRate = rate;
    for (auto& channel : filters)
        for (int i = 0; i < count; ++i) channel[static_cast<size_t>(i)].coefficients = design.stages[static_cast<size_t>(i)];
    const auto companion = current.compensatePhase ? designPhaseCompensator(current, rate) : FilterDesign {{}, 0};
    companionCount = companion.count;
    for (auto& channel : companions)
        for (int i = 0; i < companionCount; ++i) channel[static_cast<size_t>(i)].coefficients = companion.stages[static_cast<size_t>(i)];
}
double Equalizer::Bank::process(double sample, int channel) noexcept {
    for (int i = 0; i < count; ++i) sample = filters[static_cast<size_t>(channel)][static_cast<size_t>(i)].process(sample);
    return sample;
}
double Equalizer::Bank::align(double sample, int channel) noexcept {
    for (int i = 0; i < companionCount; ++i) sample = companions[static_cast<size_t>(channel)][static_cast<size_t>(i)].process(sample);
    return sample;
}
void Equalizer::prepare(double sampleRate) noexcept {
    rate = std::clamp(std::isfinite(sampleRate) ? sampleRate : 48000.0, 8000.0, 384000.0);
    smooth = std::exp(-16.0 / (0.02 * rate));
    gainSmoothing = 1.0 - std::exp(-1.0 / (0.01 * rate));
    fadeSamples = std::max(1, static_cast<int>(0.01 * rate));
    fadeStep = 1.0 / fadeSamples;
    controlCounter = 0;

    // ITU-R BS.1770 K-weighting (high shelf, then RLB high-pass) designed for any rate, as in libebur128.
    const auto shelfK = std::tan(std::numbers::pi * 1681.974450955533 / rate);
    const auto shelfQ = 0.7071752369554196;
    const auto vh = std::pow(10.0, 3.999843853973347 / 20.0), vb = std::pow(vh, 0.4996667741545416);
    const auto shelfA0 = 1.0 + shelfK / shelfQ + shelfK * shelfK;
    const auto shelf = directForm((vh + vb * shelfK / shelfQ + shelfK * shelfK) / shelfA0, 2.0 * (shelfK * shelfK - vh) / shelfA0,
                                  (vh - vb * shelfK / shelfQ + shelfK * shelfK) / shelfA0, 2.0 * (shelfK * shelfK - 1.0) / shelfA0,
                                  (1.0 - shelfK / shelfQ + shelfK * shelfK) / shelfA0);
    const auto rlbK = std::tan(std::numbers::pi * 38.13547087602444 / rate);
    const auto rlbQ = 0.5003270373238773;
    const auto rlbA0 = 1.0 + rlbK / rlbQ + rlbK * rlbK;
    const auto rlb = directForm(1.0, -2.0, 1.0, 2.0 * (rlbK * rlbK - 1.0) / rlbA0, (1.0 - rlbK / rlbQ + rlbK * rlbK) / rlbA0);
    for (auto* weighting : {&inputWeighting, &outputWeighting})
        for (auto& channel : *weighting) {
            channel[0].reset(); channel[0].coefficients = shelf;
            channel[1].reset(); channel[1].coefficients = rlb;
        }
    loudnessSmoothing = 1.0 - std::exp(-1.0 / (0.4 * rate));
    dynamicAttack = 1.0 - std::exp(-1.0 / (0.005 * rate));
    dynamicRelease = 1.0 - std::exp(-1.0 / (0.12 * rate));
    inputPower = outputPower = 0;
    autoGainLevel = 1;
    clipHistory = {};
    for (auto& band : bands) {
        band = {};
        for (auto& bank : band.banks) bank.reset();
    }
    setSettings({}, true);
}
void Equalizer::setSettings(const Settings& settings, bool immediately) noexcept {
    bypass = settings.bypass;
    for (size_t i = 0; i < bands.size(); ++i) {
        auto& band = bands[i];
        band.target = audibleBand(settings, i);
        band.target.frequency = effectiveFrequency(band.target.frequency, settings.snap, rate, settings.a4);
        band.target.compensatePhase = settings.phaseCompensation && isOneSidedCut(settings.bands[i]);
        if (immediately) {
            band.active = 0; band.remaining = band.priming = 0; band.wet = band.target.enabled ? 1 : 0;
            for (auto& bank : band.banks) {
                bank.current = band.target; bank.reset(); bank.update(band.target, rate, 0);
            }
        }
    }
    autoGain = settings.autoGain && settings.soloBand < 0;
    dynamicThresholdDb = settings.dynamicThresholdDb;
    targetOutput = std::pow(10.0, std::clamp(settings.outputDb, -24.0, 12.0) / 20.0);
    if (immediately) { bypassWet = bypass ? 0 : 1; outputGain = targetOutput; controlCounter = 0; }
}
void Equalizer::updateFilters() noexcept {
    for (auto& band : bands) {
        band.dynamicGainDb = 0;
        if (!band.target.enabled && band.wet == 0.0 && band.remaining == 0) {
            continue;
        }
        auto target = band.target;
        if (usesDynamics(target)) {
            if (band.detectorHz != target.frequency || band.detectorQ != target.q) {
                BandSettings detector;
                detector.shape = Shape::BandPass; detector.frequency = target.frequency; detector.q = target.q;
                for (auto& filter : band.detectors) filter.coefficients = designFilter(detector, rate).stages[0];
                band.detectorHz = target.frequency; band.detectorQ = target.q;
            }
            const auto overDb = 20.0 * std::log10(std::max(band.envelope, 1.0e-9)) - dynamicThresholdDb;
            band.dynamicGainDb = target.dynamicDb * std::clamp(overDb / 12.0, 0.0, 1.0);
            target.gainDb += band.dynamicGainDb;
        }
        auto& active = band.banks[static_cast<size_t>(band.active)];
        auto& next = band.banks[static_cast<size_t>(1 - band.active)];
        if (band.remaining == 0 && topologyDiffers(active.current, target)) {
            next.current = target;
            next.reset(); next.update(target, rate, 0);
            band.remaining = band.priming = fadeSamples;
        }
        active.update(target, rate, smooth);
        if (band.remaining > 0) next.update(target, rate, smooth);
    }
}
double Equalizer::dynamicGainDb(size_t band) const noexcept { return bands[band].dynamicGainDb; }
double Equalizer::detectorLevel(size_t band) const noexcept {
    const auto& target = bands[band].target;
    return target.enabled && usesDynamics(target) ? bands[band].envelope : 0.0;
}
void Equalizer::process(float* const* channels, int channelCount, int samples) noexcept {
    channelCount = std::clamp(channelCount, 0, 2);
    for (int frame = 0; frame < samples; ++frame) {
        if (controlCounter == 0) updateFilters();
        controlCounter = (controlCounter + 1) % 16;
        outputGain += (targetOutput - outputGain) * gainSmoothing;
        bypassWet = approach(bypassWet, bypass ? 0 : 1, fadeStep);
        std::array<double, 2> dry {}, wet {};
        for (int c = 0; c < channelCount; ++c) dry[static_cast<size_t>(c)] = wet[static_cast<size_t>(c)] = channels[c][frame];
        for (auto& band : bands) {
            if (!band.target.enabled && band.wet == 0.0 && band.remaining == 0) {
                continue;
            }
            band.wet = approach(band.wet, band.target.enabled ? 1 : 0, fadeStep);
            // The next topology runs silently until its state is primed, then takes over on an equal-power fade.
            const auto blend = 0.5 * std::numbers::pi * (1.0 - static_cast<double>(band.remaining) / fadeSamples);
            const auto wetMix = 0.5 - 0.5 * std::cos(std::numbers::pi * band.wet);
            // Flat Mid/Side is unity: M = (L+R)/2, S = (L-R)/2, then L = M+S and R = M-S.
            auto through = [&](double (Bank::*path)(double, int) noexcept, double input, int channel) {
                auto filtered = (band.banks[static_cast<size_t>(band.active)].*path)(input, channel);
                if (band.remaining > 0) {
                    const auto next = (band.banks[static_cast<size_t>(1 - band.active)].*path)(input, channel);
                    if (band.priming == 0) filtered = std::cos(blend) * filtered + std::sin(blend) * next;
                }
                if (path == &Bank::process && band.target.drive > 0.0 && usesGain(band.target.shape))
                    filtered = input + saturate(filtered - input, band.driveHistory[static_cast<size_t>(channel)], band.target.drive);
                return input + wetMix * (filtered - input);
            };
            auto shaped = [&](double input, int channel) { return through(&Bank::process, input, channel); };
            // The untouched channel gets the cut's passband phase so one-sided cuts do not smear transients.
            auto aligned = [&](double input, int channel) { return through(&Bank::align, input, channel); };
            const auto place = band.target.placement;

            // The detector hears what the band processes, band-limited to the band's note.
            if (usesDynamics(band.target)) {
                auto input = wet[0];
                if (channelCount == 2 && place == Placement::Right) input = wet[1];
                if (channelCount == 2 && place == Placement::Mid) input = 0.5 * (wet[0] + wet[1]);
                if (channelCount == 2 && place == Placement::Side) input = 0.5 * (wet[0] - wet[1]);
                auto level = std::abs(band.detectors[0].process(input));
                if (channelCount == 2 && place == Placement::Stereo) level = std::max(level, std::abs(band.detectors[1].process(wet[1])));
                band.envelope += (level - band.envelope) * (level > band.envelope ? dynamicAttack : dynamicRelease);
            }
            if (channelCount < 2) {
                if (place != Placement::Right && place != Placement::Side)
                    wet[0] = shaped(wet[0], 0);
            } else if (place == Placement::Left) {
                wet[0] = shaped(wet[0], 0);
                wet[1] = aligned(wet[1], 1);
            } else if (place == Placement::Right) {
                wet[1] = shaped(wet[1], 1);
                wet[0] = aligned(wet[0], 0);
            } else if (place == Placement::Mid || place == Placement::Side) {
                const auto mid = 0.5 * (wet[0] + wet[1]);
                const auto side = 0.5 * (wet[0] - wet[1]);
                const auto filtered = shaped(place == Placement::Mid ? mid : side, 0);
                const auto other = aligned(place == Placement::Mid ? side : mid, 0);
                const auto m = place == Placement::Mid ? filtered : other;
                const auto s = place == Placement::Side ? filtered : other;
                wet[0] = m + s;
                wet[1] = m - s;
            } else {
                wet[0] = shaped(wet[0], 0);
                wet[1] = shaped(wet[1], 1);
            }
            if (band.priming > 0) --band.priming;
            else if (band.remaining > 0 && --band.remaining == 0) band.active = 1 - band.active;
        }

        // Match K-weighted loudness of the EQ output to the input. The gate holds the gain through silence.
        auto autoGainTarget = 1.0;
        if (autoGain) {
            double inputSquare = 0, outputSquare = 0;
            for (size_t c = 0; c < static_cast<size_t>(channelCount); ++c) {
                const auto in = inputWeighting[c][1].process(inputWeighting[c][0].process(dry[c]));
                const auto out = outputWeighting[c][1].process(outputWeighting[c][0].process(wet[c]));
                inputSquare += in * in; outputSquare += out * out;
            }
            inputPower += (inputSquare - inputPower) * loudnessSmoothing;
            outputPower += (outputSquare - outputPower) * loudnessSmoothing;
            autoGainTarget = autoGainLevel;
            if (inputPower > 1.0e-9) autoGainTarget = std::clamp(std::sqrt(inputPower / std::max(outputPower, 1.0e-12)), 0.25, 4.0);
        }
        autoGainLevel += (autoGainTarget - autoGainLevel) * gainSmoothing;
        for (int c = 0; c < channelCount; ++c) {
            const auto idx = static_cast<size_t>(c);
            double sample = dry[idx] + (0.5 - 0.5 * std::cos(std::numbers::pi * bypassWet)) * (wet[idx] * outputGain * autoGainLevel - dry[idx]);
            channels[c][frame] = static_cast<float>(softCeiling.process(sample, clipHistory[idx]));
        }
    }
}
}
