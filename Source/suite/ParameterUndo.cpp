#include "ParameterUndo.h"
#include <cmath>
namespace suite {
class ParameterUndo::GroupAction : public juce::UndoableAction {
public:
    GroupAction(ParameterUndo& owner, std::vector<Change> changes) : owner(owner), changes(std::move(changes)) {}
    bool perform() override { return apply(false); }
    bool undo() override { return apply(true); }
private:
    bool apply(bool reverse) {
        juce::ScopedValueSetter<int> guard(owner.suppress, owner.suppress + 1);
        for (auto& change : changes)
            change.parameter->setValueNotifyingHost(reverse ? change.from : change.to);
        return true;
    }
    ParameterUndo& owner;
    std::vector<Change> changes;
};
class ParameterUndo::StateAction : public juce::UndoableAction {
public:
    StateAction(ParameterUndo& owner, juce::MemoryBlock before, juce::MemoryBlock after)
        : owner(owner), before(std::move(before)), after(std::move(after)) {}
    bool perform() override { owner.restore(after); return true; }
    bool undo() override { owner.restore(before); return true; }
private:
    ParameterUndo& owner;
    juce::MemoryBlock before, after;
};
ParameterUndo::ParameterUndo(juce::AudioProcessor& p, Export exporter, Import importer)
    : processor(p), exportState(std::move(exporter)), importState(std::move(importer)) {
    for (auto* parameter : processor.getParameters()) parameter->addListener(this);
}
ParameterUndo::~ParameterUndo() {
    for (auto* parameter : processor.getParameters()) parameter->removeListener(this);
}
void ParameterUndo::undo() { lastEdited = nullptr; if (manager().canUndo()) manager().undo(); }
void ParameterUndo::redo() { lastEdited = nullptr; if (manager().canRedo()) manager().redo(); }
void ParameterUndo::reset(int activeSlot) {
    for (auto& history : managers) history.clearUndoHistory();
    selectSlot(activeSlot);
}
juce::MemoryBlock ParameterUndo::capture() const {
    juce::MemoryBlock block;
    exportState(block);
    return block;
}
void ParameterUndo::restore(const juce::MemoryBlock& block) {
    juce::ScopedValueSetter<int> guard(suppress, suppress + 1);
    importState(block);
}
void ParameterUndo::applyChange(const juce::String& name, std::function<void()> change) {
    lastEdited = nullptr;
    const auto before = capture();
    {
        juce::ScopedValueSetter<int> guard(suppress, suppress + 1);
        change();
    }
    const auto after = capture();
    if (before == after) return;
    manager().beginNewTransaction(name);
    manager().perform(new StateAction(*this, before, after));
}
void ParameterUndo::parameterGestureChanged(int index, bool starting) {
    if (suppress > 0) return;
    auto* messages = juce::MessageManager::getInstanceWithoutCreating();
    if (messages == nullptr || !messages->isThisTheMessageThread()) return;
    auto& parameters = processor.getParameters();
    if (!juce::isPositiveAndBelow(index, parameters.size())) return;
    auto* parameter = parameters[index];
    if (starting) {
        // Edits of one parameter less than 400 ms apart share an undo step.
        const auto joins = parameter == lastEdited && juce::Time::getMillisecondCounterHiRes() - lastEditEnd < 400.0;
        if (openGestures == 0 && !joins) manager().beginNewTransaction("Edit");
        ++openGestures;
        for (const auto& entry : gestureStart) if (entry.first == parameter) return;
        gestureStart.emplace_back(parameter, parameter->getValue());
        return;
    }
    if (openGestures == 0) return;
    if (--openGestures > 0) return;
    std::vector<Change> changes;
    for (const auto& entry : gestureStart) {
        const auto to = entry.first->getValue();
        if (std::abs(to - entry.second) > 1.0e-6f) changes.push_back({entry.first, entry.second, to});
    }
    gestureStart.clear();
    if (changes.empty()) return;
    manager().perform(new GroupAction(*this, std::move(changes)));
    lastEdited = parameter; lastEditEnd = juce::Time::getMillisecondCounterHiRes();
    if (onEdit) onEdit();
}
}
