#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <functional>
#include <vector>

namespace suite {
// Each A/B slot keeps its own history. Export and import move one slot's state.
class ParameterUndo : private juce::AudioProcessorParameter::Listener {
public:
    using Export = std::function<void(juce::MemoryBlock&)>;
    using Import = std::function<void(const juce::MemoryBlock&)>;
    ParameterUndo(juce::AudioProcessor&, Export, Import);
    ~ParameterUndo() override;
    juce::UndoManager& manager() noexcept { return managers[static_cast<size_t>(slot)]; }
    void undo();
    void redo();
    void applyChange(const juce::String& name, std::function<void()> change);
    void selectSlot(int index) noexcept { slot = index; }
    void clearSlot(int index) { managers[static_cast<size_t>(index)].clearUndoHistory(); }
    // Called when an edit gesture becomes an undo step.
    std::function<void()> onEdit;

private:
    struct Change { juce::AudioProcessorParameter* parameter; float from; float to; };
    class GroupAction;
    class StateAction;
    juce::AudioProcessor& processor;
    Export exportState;
    Import importState;
    std::array<juce::UndoManager, 2> managers;
    int slot = 0;
    int suppress = 0;
    int openGestures = 0;
    std::vector<std::pair<juce::AudioProcessorParameter*, float>> gestureStart;
    juce::MemoryBlock capture() const;
    void restore(const juce::MemoryBlock&);
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int, bool) override;
};
}
