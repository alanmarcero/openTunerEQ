#include "SpectrumAnalyzer.h"

void SpectrumCapture::push(const juce::AudioBuffer<float>& audio, bool enabled, bool afterEQ) noexcept {
    const auto current = view.load();
    if (!enabled || !active.load() || audio.getNumChannels() == 0) { capturing = false; return; }
    if (!capturing || pending.view != current || pending.afterEQ != afterEQ) {
        ++stream; capturing = true;
        pending.count = 0; pending.rate = sampleRate; pending.stream = stream; pending.view = current;
        pending.afterEQ = afterEQ;
    }
    for (int offset = 0; offset < audio.getNumSamples();) {
        const auto count = std::min(256 - pending.count, audio.getNumSamples() - offset);
        std::copy_n(audio.getReadPointer(0, offset), count, pending.left.begin() + pending.count);
        std::copy_n(audio.getReadPointer(std::min(1, audio.getNumChannels() - 1), offset), count, pending.right.begin() + pending.count);
        offset += count; pending.count += count;
        if (pending.count < 256) continue;
        const auto write = fifo.write(1);
        if (write.blockSize1 == 0) { capturing = false; return; }
        packets[write.startIndex1] = pending;
        pending.count = 0;
    }
}
bool SpectrumCapture::pop(Packet& packet) noexcept {
    const auto read = fifo.read(1);
    if (read.blockSize1 == 0) return false;
    packet = packets[read.startIndex1];
    return true;
}
void SpectrumAnalyzer::configure(const SpectrumCapture::Packet& packet) {
    sampleRate = packet.rate; stream = packet.stream; view = packet.view;
    const auto order = juce::jlimit(10, 15, static_cast<int>(std::ceil(std::log2(sampleRate * 0.12))));
    const auto size = 1 << order;
    if (!fft || fft->getSize() != size) {
        fft = std::make_unique<juce::dsp::FFT>(order);
        left.resize(size); right.resize(size); window.resize(size);
        transformLeft.resize(size * 2); transformRight.resize(size * 2); levels.resize(size / 2 + 1);
        double sum = 0;
        for (int i = 0; i < size; ++i) {
            window[static_cast<size_t>(i)] = static_cast<float>(0.5 - 0.5 * std::cos(2 * juce::MathConstants<double>::pi * i / (size - 1)));
            sum += window[static_cast<size_t>(i)];
        }
        normalization = static_cast<float>(2.0 / sum);
    }
    reset();
}
void SpectrumAnalyzer::reset() {
    position = 0; collected = 0;
    std::fill(levels.begin(), levels.end(), floorDb);
}
bool SpectrumAnalyzer::update(double now, bool useAfterEQ) {
    if (afterEQ != useAfterEQ) { reset(); afterEQ = useAfterEQ; }
    SpectrumCapture::Packet packet;
    bool received = false;
    // Bound draining even if an offline host produces audio faster than the UI can consume it.
    for (int count = 0; count < 256 && capture.pop(packet); ++count) {
        if (packet.view != capture.currentView() || packet.afterEQ != afterEQ) continue;
        if (packet.stream != stream || packet.view != view || packet.rate != sampleRate) configure(packet);
        const int size = fft->getSize();
        for (int i = 0; i < packet.count; ++i) {
            const auto l = packet.left[static_cast<size_t>(i)], r = packet.right[static_cast<size_t>(i)];
            left[static_cast<size_t>(position)] = std::isfinite(l) ? l : 0.0f;
            right[static_cast<size_t>(position)] = std::isfinite(r) ? r : 0.0f;
            position = (position + 1) % size;
        }
        collected = std::min(size, collected + packet.count);
        received = true;
    }
    const auto elapsed = juce::jlimit(0.0, 0.25, now - lastUpdate);
    lastUpdate = now;
    if (!received) {
        if (now - lastInput > 0.25) reset();
        return false;
    }
    lastInput = now;
    const auto size = fft->getSize();
    if (collected < size) return false;
    for (int i = 0; i < size; ++i) {
        const auto index = static_cast<size_t>((position + i) % size);
        transformLeft[static_cast<size_t>(i)] = left[index] * window[static_cast<size_t>(i)];
        transformRight[static_cast<size_t>(i)] = right[index] * window[static_cast<size_t>(i)];
    }
    fft->performFrequencyOnlyForwardTransform(transformLeft.data(), true);
    fft->performFrequencyOnlyForwardTransform(transformRight.data(), true);
    for (size_t i = 0; i < levels.size(); ++i) {
        const auto amplitude = std::max(transformLeft[i], transformRight[i]) * normalization;
        const auto db = juce::Decibels::gainToDecibels(amplitude, floorDb);
        levels[i] = std::max(db, levels[i] - static_cast<float>(elapsed * 48));
    }
    return true;
}
float SpectrumAnalyzer::levelAt(double hz) const {
    if (!fft || collected < fft->getSize() || hz < 20 || hz >= sampleRate * 0.5) return floorDb;
    const auto bin = hz * fft->getSize() / sampleRate;
    const auto index = static_cast<size_t>(bin);
    const auto fraction = static_cast<float>(bin - index);
    return levels[index] + fraction * (levels[index + 1] - levels[index]);
}
float SpectrumAnalyzer::levelInRange(double low, double high) const {
    auto result = levelAt(std::sqrt(low * high));
    if (!fft || collected < fft->getSize()) return result;
    const int first = std::max(1, static_cast<int>(std::ceil(low * fft->getSize() / sampleRate)));
    const int last = std::min(fft->getSize() / 2 - 1, static_cast<int>(std::floor(high * fft->getSize() / sampleRate)));
    for (int i = first; i <= last; ++i) result = std::max(result, levels[static_cast<size_t>(i)]);
    return result;
}
