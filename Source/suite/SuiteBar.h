#pragma once
#include "ABCompare.h"
#include "ParameterUndo.h"
#include "PresetStore.h"
class TunerProcessor;
namespace suite {
// The preset browser that the Presets button opens.
std::unique_ptr<juce::Component> createPresetBrowser(TunerProcessor&, juce::LookAndFeel&);
class SuiteBar final : public juce::Component {
public:
    explicit SuiteBar(TunerProcessor&);
    ~SuiteBar() override;
    void resized() override;
    void refresh();
private:
    TunerProcessor& processor;
    ParameterUndo& history;
    ABCompare& compare;
    PresetStore& presets;
    juce::TextButton undoButton {"Undo"}, redoButton {"Redo"}, slotA {"A"}, slotB {"B"};
    juce::TextButton copyAtoB {"A to B"}, copyBtoA {"B to A"}, presetButton {"Presets"}, previousButton {"<"}, nextButton {">"};
    juce::Component::SafePointer<juce::CallOutBox> callout;
    void openBrowser();
    void step(int direction);
};
}
