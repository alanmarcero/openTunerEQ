#include "Equalizer.h"
#include "Notes.h"
#include <algorithm>
#include <cmath>
#include <complex>
namespace ote {
namespace {
double approach(double value, double target, double step) noexcept {
    return value + std::clamp(target - value, -step, step);
}
bool topologyDiffers(const BandSettings& a, const BandSettings& b) noexcept {
    return a.shape != b.shape || (isCut(a.shape) && a.slope != b.slope);
}
}
double estimateTailSeconds(const Settings& settings, double sampleRate) noexcept {
    if (settings.bypass) return 0;
    double slowest = 0, boost = 0;
    int sections = 0;
    for (auto band : settings.bands) {
        if (!band.enabled || (usesGain(band.shape) && std::abs(band.gainDb) < 0.001)) continue;
        band.frequency = effectiveFrequency(band.frequency, settings.snap, sampleRate);
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
void Equalizer::Bank::reset() noexcept {
    for (auto& channel : filters) for (auto& filter : channel) filter.reset();
}
void Equalizer::Bank::update(const BandSettings& target, double rate, double smoothing) noexcept {
    current.frequency = std::exp(std::log(target.frequency) + smoothing * (std::log(current.frequency) - std::log(target.frequency)));
    current.gainDb = target.gainDb + smoothing * (current.gainDb - target.gainDb);
    current.q = target.q + smoothing * (current.q - target.q);
    const auto design = designFilter(current, rate);
    count = design.count;
    for (auto& channel : filters)
        for (int i = 0; i < count; ++i) channel[static_cast<size_t>(i)].coefficients = design.stages[static_cast<size_t>(i)];
}
double Equalizer::Bank::process(double sample, int channel) noexcept {
    for (int i = 0; i < count; ++i) sample = filters[static_cast<size_t>(channel)][static_cast<size_t>(i)].process(sample);
    return sample;
}
void Equalizer::prepare(double sampleRate) noexcept {
    rate = std::max(100.0, sampleRate);
    smooth = std::exp(-16.0 / (0.02 * rate));
    gainSmoothing = 1.0 - std::exp(-1.0 / (0.01 * rate));
    fadeSamples = std::max(1, static_cast<int>(0.01 * rate));
    fadeStep = 1.0 / fadeSamples;
    controlCounter = 0;
    for (auto& band : bands) {
        band = {};
        for (auto& bank : band.banks) bank.reset();
    }
    setSettings({}, true);
}
void Equalizer::setSettings(const Settings& settings, bool immediately) noexcept {
    bypass = settings.bypass;
    targetOutput = std::pow(10.0, std::clamp(settings.outputDb, -24.0, 12.0) / 20.0);
    for (size_t i = 0; i < bands.size(); ++i) {
        auto& band = bands[i];
        band.target = settings.bands[i];
        band.target.frequency = effectiveFrequency(band.target.frequency, settings.snap, rate);
        if (immediately) {
            band.active = 0; band.remaining = 0; band.wet = band.target.enabled ? 1 : 0;
            for (auto& bank : band.banks) {
                bank.current = band.target; bank.reset(); bank.update(band.target, rate, 0);
            }
        }
    }
    if (immediately) { bypassWet = bypass ? 0 : 1; outputGain = targetOutput; controlCounter = 0; }
}
void Equalizer::updateFilters() noexcept {
    for (auto& band : bands) {
        auto& active = band.banks[static_cast<size_t>(band.active)];
        auto& next = band.banks[static_cast<size_t>(1 - band.active)];
        if (band.remaining == 0 && topologyDiffers(active.current, band.target)) {
            next.current = band.target;
            next.reset(); next.update(band.target, rate, 0);
            band.remaining = fadeSamples;
        }
        active.update(band.target, rate, smooth);
        if (band.remaining > 0) next.update(band.target, rate, smooth);
    }
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
            band.wet = approach(band.wet, band.target.enabled ? 1 : 0, fadeStep);
            const auto blend = 1.0 - static_cast<double>(band.remaining) / fadeSamples;
            for (int c = 0; c < channelCount; ++c) {
                const auto input = wet[static_cast<size_t>(c)];
                auto filtered = band.banks[static_cast<size_t>(band.active)].process(input, c);
                if (band.remaining > 0) {
                    const auto next = band.banks[static_cast<size_t>(1 - band.active)].process(input, c);
                    filtered += blend * (next - filtered);
                }
                wet[static_cast<size_t>(c)] = input + band.wet * (filtered - input);
            }
            if (band.remaining > 0 && --band.remaining == 0) band.active = 1 - band.active;
        }
        for (int c = 0; c < channelCount; ++c) {
            const auto idx = static_cast<size_t>(c);
            channels[c][frame] = static_cast<float>(dry[idx] + bypassWet * (wet[idx] * outputGain - dry[idx]));
        }
    }
}
}
