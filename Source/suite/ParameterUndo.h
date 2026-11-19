#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include <vector>

namespace suite {
class ParameterUndo : private juce::AudioProcessorParameter::Listener {
public:
    explicit ParameterUndo(juce::AudioProcessor&);
    ~ParameterUndo() override;
    juce::UndoManager& manager() noexcept { return undoManager; }
    void undo();
    void redo();
    void applyChange(const juce::String& name, std::function<void()> change);
    juce::MemoryBlock capture() const;
    void restore(const juce::MemoryBlock&);

private:
    struct Change { juce::AudioProcessorParameter* parameter; float from; float to; };
    class GroupAction;
    class StateAction;
    juce::AudioProcessor& processor;
    juce::UndoManager undoManager;
    int suppress = 0;
    int openGestures = 0;
    std::vector<std::pair<juce::AudioProcessorParameter*, float>> gestureStart;
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int, bool) override;
};
}
