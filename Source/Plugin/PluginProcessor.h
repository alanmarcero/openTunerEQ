#pragma once
#include "Parameters.h"
class TunerProcessor final : public juce::AudioProcessor {
public:
    TunerProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "openTunerEQ"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return ote::estimateTailSeconds(readSettings(), displaySampleRate()); }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;
    ote::Settings readSettings() const noexcept;
    double bandFrequency(int band) const noexcept;
    bool usesFlats() const noexcept { return flats->load() > 0.5f; }
    double displaySampleRate() const noexcept { return uiSampleRate.load(); }
    void setParameter(const juce::String& id, float value);
    params::State state;
    std::atomic<float> peak {0};
private:
    struct BandValues { std::atomic<float> *enabled, *shape, *freq, *gain, *q, *slope; };
    std::array<BandValues, 5> values;
    std::atomic<float> *snap, *flats, *bypass, *output;
    std::atomic<double> uiSampleRate {48000};
    ote::Equalizer engine;
    void process(juce::AudioBuffer<float>&, bool forceBypass);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TunerProcessor)
};
