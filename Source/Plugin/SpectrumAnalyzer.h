#pragma once
#include <juce_dsp/juce_dsp.h>

// One audio-thread producer and one editor-thread consumer. Storage is allocated at construction.
class SpectrumCapture {
public:
    struct Packet {
        std::array<float, 256> left {}, right {};
        int count = 0;
        double rate = 48000;
        unsigned stream = 0, view = 0;
    };
    SpectrumCapture() : packets(std::make_unique<Packet[]>(capacity)) {}
    void prepare(double rate) noexcept { sampleRate = rate; capturing = false; }
    void setActive(bool value) noexcept {
        if (active.exchange(value) != value) view.fetch_add(1);
    }
    void push(const juce::AudioBuffer<float>&, bool enabled) noexcept;
    bool pop(Packet&) noexcept;
    unsigned currentView() const noexcept { return view.load(); }
private:
    static constexpr int capacity = 256;
    juce::AbstractFifo fifo {capacity};
    std::unique_ptr<Packet[]> packets;
    Packet pending;
    std::atomic<bool> active {false};
    std::atomic<unsigned> view {0};
    double sampleRate = 48000;
    unsigned stream = 0;
    bool capturing = false;
};

class SpectrumAnalyzer {
public:
    static constexpr float floorDb = -90.0f;
    explicit SpectrumAnalyzer(SpectrumCapture& source) : capture(source) {}
    bool update(double nowSeconds);
    void reset();
    float levelAt(double hz) const;
    float levelInRange(double lowHz, double highHz) const;
private:
    SpectrumCapture& capture;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> left, right, window, transformLeft, transformRight, levels;
    double sampleRate = 0, lastInput = 0, lastUpdate = 0;
    float normalization = 1;
    int position = 0, collected = 0;
    unsigned stream = 0, view = 0;
    void configure(const SpectrumCapture::Packet&);
};
