#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
namespace suite {
class ParameterUndo;
class ABCompare {
public:
    using Export = std::function<void(juce::MemoryBlock&)>;
    using Import = std::function<void(const juce::MemoryBlock&)>;
    ABCompare(Export, Import, ParameterUndo&);
    void prime();
    void flushActive();
    void toggle();
    void copyAtoB();
    void copyBtoA();
    bool isB() const noexcept { return active == 1; }
    void writeXml(juce::XmlElement&) const;
    bool readXml(const juce::XmlElement&);
private:
    Export exportState;
    Import importState;
    ParameterUndo& history;
    juce::MemoryBlock slots[2];
    int active = 0;
    void copyBetween(int from, int to);
};
}
