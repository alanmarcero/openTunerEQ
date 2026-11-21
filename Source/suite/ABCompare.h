#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
namespace suite {
class ParameterUndo;
class ABCompare {
public:
    explicit ABCompare(ParameterUndo&);
    void prime();
    void flushActive();
    void toggle();
    void copyAtoB();
    void copyBtoA();
    bool isB() const noexcept;
    void writeXml(juce::XmlElement&) const;
    bool readXml(const juce::XmlElement&);
private:
    ParameterUndo& history;
    juce::MemoryBlock slots[2];
    void copyBetween(int from, int to);
};
}
