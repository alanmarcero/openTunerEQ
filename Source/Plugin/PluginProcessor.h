#pragma once
#include "ArtworkBeatClock.h"
#include "Parameters.h"
#include "SpectrumAnalyzer.h"
#include "ABCompare.h"
#include "ParameterUndo.h"
#include "PresetStore.h"
class TunerProcessor final : public juce::AudioProcessor {
public:
    // Tests pass a temporary preset folder.
    explicit TunerProcessor(juce::File presetFolder = {});
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
    int getNumPrograms() override;
    int getCurrentProgram() override { return preset.program; }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;
    ote::Settings readSettings() const noexcept;
    double bandFrequency(int band) const noexcept;
    double referencePitch() const noexcept { return reference->load(); }
    bool usesFlats() const noexcept { return flats->load() > 0.5f; }
    double displaySampleRate() const noexcept { return uiSampleRate.load(); }
    void setParameter(const juce::String& id, float value);
    suite::ParameterUndo& history() noexcept { return undoHistory; }
    suite::ABCompare& compare() noexcept { return abCompare; }
    suite::PresetStore& presets() noexcept { return presetStore; }
    juce::String presetTitle() const { return preset.name; }
    // True after an edit since the last load or save.
    bool presetEdited() const noexcept { return preset.edited; }
    std::unique_ptr<juce::XmlElement> parameterXml();
    void loadPresetXml(const juce::XmlElement&, const juce::String& name);
    void loadPreset(const suite::PresetStore::Item&);
    bool savePreset(const juce::String& name);
    params::State state;
    SpectrumCapture spectrum;
    float artworkPhase() const noexcept { return artworkClock.currentPhase(); }
    std::atomic<float> peak {0};
    // For each band: the live dynamic gain in dB, and the detector peak since the last read.
    std::array<std::atomic<float>, 5> dynamicGains {}, bandLevels {};
    std::atomic<int> activeSelectedBand {0};
    std::atomic<int> editorWidth {1160};
private:
    ArtworkBeatClock artworkClock;
    struct BandValues { std::atomic<float> *enabled, *shape, *freq, *gain, *q, *slope, *placement, *dyn, *drive; };
    std::array<BandValues, 5> values;
    suite::ParameterUndo undoHistory;
    suite::ABCompare abCompare;
    suite::PresetStore presetStore;
    // The loaded preset. Each A/B slot and the session store it.
    struct Preset {
        juce::String name;
        int program = 0;
        bool edited = false;
        void write(juce::XmlElement&) const;
        void read(const juce::XmlElement&);
    };
    Preset preset;
    void writeParameters(juce::MemoryBlock&);
    void readParameters(const juce::MemoryBlock&);
    void injectDefaults(juce::ValueTree&) const;
    void applyFactory(int index);
    void assignParameter(const juce::String& id, float value);
    std::atomic<float> *snap, *flats, *bypass, *output, *reference, *analyzerEnabled, *analyzerSource, *scale, *autoGain, *soloSelected,
        *phaseComp, *qMode, *dynThreshold;
    std::atomic<double> uiSampleRate {48000};
    ote::Equalizer engine;
    void process(juce::AudioBuffer<float>&, bool forceBypass);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TunerProcessor)
};
