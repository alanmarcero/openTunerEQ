#include "SpectrumAnalyzer.h"
#include <cmath>
#include <cstdlib>
#include <iostream>

void require(bool value, const char* name) {
    if (!value) { std::cerr << "FAIL: " << name << '\n'; std::exit(1); }
}

int main() {
    SpectrumCapture capture;
    SpectrumAnalyzer analyzer(capture);
    juce::AudioBuffer<float> audio(2, 256);
    double time = 0;
    auto tone = [&](double rate, double frequency, bool opposite, int channels = 2) {
        capture.prepare(rate);
        audio.setSize(channels, 256);
        for (int block = 0; block < static_cast<int>(rate * 0.4 / 256); ++block) {
            for (int i = 0; i < 256; ++i) {
                const auto sample = static_cast<float>(0.5 * std::sin(2 * juce::MathConstants<double>::pi * frequency * (block * 256 + i) / rate));
                audio.setSample(0, i, sample);
                if (channels == 2) audio.setSample(1, i, opposite ? -sample : sample);
            }
            capture.push(audio, true);
            time += 256 / rate;
            analyzer.update(time);
        }
    };
    tone(48000, 1000, false);
    require(analyzer.levelAt(1000) == SpectrumAnalyzer::floorDb, "closed editor captures no audio");
    capture.setActive(true);
    for (double rate : {8000., 22050., 44100., 48000., 96000., 192000.}) {
        tone(rate, 1000, false);
        require(std::abs(analyzer.levelAt(1000) + 6.02f) < 2.0f, "calibrated 1 kHz peak across sample rates");
        require(analyzer.levelAt(1500) < -65, "unrelated frequency stays quiet");
        require(analyzer.levelAt(rate * 0.5) == SpectrumAnalyzer::floorDb, "no spectrum at or above Nyquist");
    }
    tone(48000, 1000, true);
    require(analyzer.levelAt(1000) > -8, "opposite-polarity stereo does not cancel");
    capture.push(audio, true);
    analyzer.update(time + 0.01, true);
    require(analyzer.levelAt(1000) == SpectrumAnalyzer::floorDb, "source switch clears old spectrum and discards queued input packets");
    audio.clear();
    for (int i = 0; i < 32; ++i) capture.push(audio, true, true);
    analyzer.update(time + 0.05, true);
    require(analyzer.levelAt(1000) == SpectrumAnalyzer::floorDb, "after-EQ silence does not inherit before-EQ peaks");
    capture.prepare(48000);
    audio.setSize(2, 31);
    for (int c = 0; c < 2; ++c) for (int i = 0; i < 31; ++i) audio.setSample(c, i, 1.0f);
    capture.push(audio, true, false);
    audio.setSize(2, 256); audio.clear();
    capture.push(audio, true, true);
    SpectrumCapture::Packet packet;
    require(capture.pop(packet) && packet.afterEQ, "source switch publishes a packet from the new tap");
    require(std::all_of(packet.left.begin(), packet.left.end(), [](float sample) { return sample == 0; }), "source switch discards a partial packet from the old tap");
    tone(48000, 65, false, 1);
    require(analyzer.levelAt(65) > -8 && analyzer.levelAt(1000) < -65, "mono bass replaces previous spectrum");
    analyzer.update(time + 0.5);
    require(analyzer.levelAt(65) == SpectrumAnalyzer::floorDb, "stopped transport clears stale spectrum");
    capture.setActive(false);
    analyzer.reset();
    tone(48000, 1000, false);
    require(analyzer.levelAt(1000) == SpectrumAnalyzer::floorDb, "disabled analyzer stays empty");
    capture.setActive(true);
    audio.clear();
    for (int i = 0; i < 400; ++i) capture.push(audio, true);
    analyzer.update(time + 0.6);
    tone(48000, 1000, false);
    require(analyzer.levelAt(1000) > -8, "capture recovers after a full queue");
    capture.setActive(false);
    capture.setActive(true);
    analyzer.reset();
    analyzer.update(time + 0.7);
    require(analyzer.levelAt(1000) == SpectrumAnalyzer::floorDb, "reopened editor ignores packets from its previous lifetime");
    capture.prepare(48000);
    audio.setSize(2, 256);
    for (int block = 0; block < 80; ++block) {
        audio.clear();
        for (int i = 0; i < 256; ++i)
            audio.setSample(1, i, static_cast<float>(0.5 * std::sin(2 * juce::MathConstants<double>::pi * 3000 * (block * 256 + i) / 48000)));
        capture.push(audio, true);
        time += 256.0 / 48000;
        analyzer.update(time);
    }
    require(analyzer.levelAt(3000) > -8 && analyzer.levelAt(1000) < -65, "right-only audio is visible at the correct frequency");
    capture.push(audio, false);
    analyzer.reset();
    analyzer.update(time + 1.0);
    require(analyzer.levelAt(1000) == SpectrumAnalyzer::floorDb, "off parameter stops capture");
    for (int blockSize : {16, 31, 32, 64, 257}) {
        capture.prepare(192000);
        analyzer.reset();
        audio.setSize(2, blockSize);
        int untilRefresh = 0;
        for (int sample = 0; sample < 192000; sample += blockSize) {
            for (int i = 0; i < blockSize; ++i) {
                const auto value = static_cast<float>(0.5 * std::sin(2 * juce::MathConstants<double>::pi * 1000 * (sample + i) / 192000));
                audio.setSample(0, i, value); audio.setSample(1, i, value);
            }
            capture.push(audio, true);
            untilRefresh += blockSize;
            if (untilRefresh >= 8000) {
                time += untilRefresh / 192000.0;
                analyzer.update(time);
                untilRefresh = 0;
            }
        }
        require(analyzer.levelAt(1000) > -8, "small host buffers at 192 kHz keep up with a 24 Hz editor");
    }
    std::cout << "PASS: spectrum calibration, sample rates, stereo, mono, silence, gating, overflow\n";
}
