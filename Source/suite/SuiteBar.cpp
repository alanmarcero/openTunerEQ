#include "SuiteBar.h"
#include "PluginProcessor.h"
#include "Theme.h"
namespace suite {
namespace {
class Browser final : public juce::Component, private juce::ListBoxModel, private juce::Timer {
public:
    Browser(TunerProcessor& processorIn, juce::LookAndFeel& feel) : processor(processorIn), presets(processorIn.presets()) {
        setLookAndFeel(&feel);
        search.setTextToShowWhenEmpty("Search presets", ui::muted);
        search.setComponentID("presetSearch");
        search.onTextChange = [this] { reload(); };
        list.setComponentID("presetList");
        list.setRowHeight(40);
        list.setColour(juce::ListBox::backgroundColourId, ui::panel);
        list.setColour(juce::ListBox::outlineColourId, ui::muted.withAlpha(0.2f));
        addAndMakeVisible(search);
        addAndMakeVisible(list);
        for (auto* button : {&loadButton, &saveButton, &renameButton, &deleteButton}) addAndMakeVisible(button);
        search.setTitle("Search presets"); list.setTitle("Presets");
        loadButton.setTitle("Load preset"); saveButton.setTitle("Save preset"); renameButton.setTitle("Rename preset"); deleteButton.setTitle("Delete preset");
        loadButton.setComponentID("presetLoad");
        saveButton.setComponentID("presetSave");
        renameButton.setComponentID("presetRename");
        deleteButton.setComponentID("presetDelete");
        loadButton.onClick = [this] { loadSelected(); };
        saveButton.onClick = [this] {
            if (armed == &saveButton) { const auto replace = pending; disarm(); replace(); return; }
            askName("Save preset", "Saved presets go in your Documents folder.", "My preset", [this](const juce::String& name) {
                auto save = [this, name] {
                    if (processor.savePreset(name)) reload();
                    else status.setText("That name is reserved or invalid.", juce::dontSendNotification);
                };
                if (!presets.directory().getChildFile(PresetStore::cleanName(name) + ".xml").existsAsFile()) { save(); return; }
                arm(saveButton, "Replace?", save);
                status.setText("A preset named " + name + " exists. Click Replace? to overwrite it.", juce::dontSendNotification);
            });
        };
        renameButton.onClick = [this] {
            if (const auto* item = selected()) if (!item->factory)
                askName("Rename preset", "Factory presets stay read-only.", item->name, [this, old = *item](const juce::String& name) {
                    processor.presets().rename(old, name) ? reload() : status.setText("Could not rename that preset.", juce::dontSendNotification);
                });
        };
        deleteButton.onClick = [this] {
            if (armed == &deleteButton) { const auto remove = pending; disarm(); remove(); return; }
            if (const auto* item = selected()) if (!item->factory)
                arm(deleteButton, "Delete?", [this, item = *item] { if (presets.remove(item)) reload(); });
        };
        status.setColour(juce::Label::textColourId, ui::muted);
        status.setFont(ui::font(12));
        addAndMakeVisible(status);
        reload();
    }
    ~Browser() override { setLookAndFeel(nullptr); }
    void resized() override {
        search.setBounds(8, 8, getWidth() - 16, 32);
        list.setBounds(8, 48, getWidth() - 16, getHeight() - 148);
        status.setBounds(8, getHeight() - 96, getWidth() - 16, 20);
        const int width = (getWidth() - 28) / 4;
        loadButton.setBounds(8, getHeight() - 68, width, 40);
        saveButton.setBounds(12 + width, getHeight() - 68, width, 40);
        renameButton.setBounds(16 + width * 2, getHeight() - 68, width, 40);
        deleteButton.setBounds(20 + width * 3, getHeight() - 68, width, 40);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(ui::background);
        g.setColour(ui::muted.withAlpha(0.25f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 8, 1);
    }
private:
    int getNumRows() override { return rows.size(); }
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selectedRow) override {
        if (!juce::isPositiveAndBelow(row, rows.size())) return;
        const auto& item = rows.getReference(row);
        g.setColour(selectedRow ? ui::copper.withAlpha(0.16f) : juce::Colours::transparentBlack);
        g.fillRect(0, 0, width, height);
        g.setColour(item.favorite ? ui::copper : ui::muted);
        g.setFont(ui::font(16, true));
        g.drawText(item.favorite ? "Fav" : "+", 0, 0, 40, height, juce::Justification::centred);
        g.setColour(ui::ink);
        g.setFont(ui::font(14, selectedRow));
        g.drawText(item.name, 44, 0, width - 150, height, juce::Justification::centredLeft);
        g.setColour(ui::muted);
        g.setFont(ui::font(12));
        g.drawText(item.factory ? "Factory" : "User", width - 100, 0, 88, height, juce::Justification::centredRight);
    }
    void listBoxItemClicked(int row, const juce::MouseEvent& event) override {
        if (event.x < 44 && juce::isPositiveAndBelow(row, rows.size())) {
            auto item = rows.getReference(row);
            presets.setFavorite(item, !item.favorite);
            reload();
            return;
        }
        disarm();
        updateActions();
    }
    void listBoxItemDoubleClicked(int, const juce::MouseEvent&) override { loadSelected(); }
    void reload() {
        disarm();
        rows = presets.filtered(search.getText());
        list.updateContent();
        list.repaint();
        updateActions();
    }
    const PresetStore::Item* selected() const {
        const auto row = list.getSelectedRow();
        return juce::isPositiveAndBelow(row, rows.size()) ? &rows.getReference(row) : nullptr;
    }
    void updateActions() {
        const auto* item = selected();
        loadButton.setEnabled(item != nullptr);
        renameButton.setEnabled(item != nullptr && !item->factory);
        deleteButton.setEnabled(item != nullptr && !item->factory);
    }
    void loadSelected() {
        const auto* item = selected();
        if (item == nullptr) return;
        processor.loadPreset(*item);
        status.setText(item->factory ? "Factory presets stay read-only." : "Loaded " + item->name + ".", juce::dontSendNotification);
    }
    // Replace and delete take a second click within 3 s. Meanwhile the button asks the question.
    void arm(juce::TextButton& button, const juce::String& question, std::function<void()> action) {
        disarm();
        armed = &button; pending = std::move(action);
        button.setButtonText(question);
        startTimer(3000);
    }
    void disarm() {
        stopTimer();
        armed = nullptr; pending = nullptr;
        saveButton.setButtonText("Save"); deleteButton.setButtonText("Delete");
    }
    void timerCallback() override { disarm(); }
    void askName(const juce::String& title, const juce::String& message, const juce::String& initial, std::function<void(const juce::String&)> done) {
        auto* window = new juce::AlertWindow(title, message, juce::AlertWindow::NoIcon);
        window->addTextEditor("name", initial, "Preset name");
        window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
        window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        window->setLookAndFeel(&getLookAndFeel());
        juce::Component::SafePointer<Browser> self(this);
        window->enterModalState(true, juce::ModalCallbackFunction::create([self, window, done](int result) {
            const auto name = window->getTextEditorContents("name");
            window->setLookAndFeel(nullptr);
            if (result == 1 && self != nullptr) done(name);
        }), true);
    }
    TunerProcessor& processor;
    PresetStore& presets;
    juce::TextEditor search;
    juce::ListBox list {"Presets", this};
    juce::TextButton loadButton {"Load"}, saveButton {"Save"}, renameButton {"Rename"}, deleteButton {"Delete"};
    juce::Label status;
    juce::Array<PresetStore::Item> rows;
    juce::TextButton* armed = nullptr;
    std::function<void()> pending;
};
}
std::unique_ptr<juce::Component> createPresetBrowser(TunerProcessor& processor, juce::LookAndFeel& feel) { return std::make_unique<Browser>(processor, feel); }
SuiteBar::SuiteBar(TunerProcessor& p) : processor(p), history(p.history()), compare(p.compare()), presets(p.presets()) {
    for (auto* button : {&undoButton, &redoButton, &slotA, &slotB, &copyAtoB, &copyBtoA, &presetButton, &previousButton, &nextButton}) addAndMakeVisible(button);
    undoButton.setComponentID("undo"); redoButton.setComponentID("redo");
    slotA.setComponentID("slotA"); slotB.setComponentID("slotB");
    copyAtoB.setComponentID("copyAtoB"); copyBtoA.setComponentID("copyBtoA");
    presetButton.setComponentID("presets");
    previousButton.setComponentID("presetPrevious"); nextButton.setComponentID("presetNext");
    undoButton.setTitle("Undo"); redoButton.setTitle("Redo"); slotA.setTitle("Slot A"); slotB.setTitle("Slot B");
    copyAtoB.setTitle("Copy A to B"); copyBtoA.setTitle("Copy B to A"); previousButton.setTitle("Previous preset"); nextButton.setTitle("Next preset");
    previousButton.setTooltip("Previous preset"); nextButton.setTooltip("Next preset");
    previousButton.onClick = [this] { step(-1); };
    nextButton.onClick = [this] { step(1); };
    undoButton.setTooltip("Undo the last parameter edit");
    redoButton.setTooltip("Redo the last undone edit");
    slotA.setTooltip("Hear slot A"); slotB.setTooltip("Hear slot B");
    copyAtoB.setTooltip("Copy slot A onto slot B");
    copyBtoA.setTooltip("Copy slot B onto slot A");
    presetButton.setTooltip("Factory and user presets. Search, save, rename, delete, and mark favorites.");
    slotA.setColour(juce::TextButton::buttonOnColourId, ui::copper);
    slotB.setColour(juce::TextButton::buttonOnColourId, ui::copper);
    undoButton.onClick = [this] { history.undo(); refresh(); };
    redoButton.onClick = [this] { history.redo(); refresh(); };
    slotA.onClick = [this] { if (compare.isB()) { compare.toggle(); refresh(); } };
    slotB.onClick = [this] { if (!compare.isB()) { compare.toggle(); refresh(); } };
    copyAtoB.onClick = [this] { compare.copyAtoB(); refresh(); };
    copyBtoA.onClick = [this] { compare.copyBtoA(); refresh(); };
    presetButton.onClick = [this] { openBrowser(); };
    refresh();
}
SuiteBar::~SuiteBar() { if (callout != nullptr) callout->dismiss(); }
void SuiteBar::resized() {
    undoButton.setBounds(0, 0, 64, 40);
    redoButton.setBounds(68, 0, 64, 40);
    slotA.setBounds(144, 0, 40, 40);
    slotB.setBounds(188, 0, 40, 40);
    copyAtoB.setBounds(240, 0, 76, 40);
    copyBtoA.setBounds(320, 0, 76, 40);
    previousButton.setBounds(408, 0, 40, 40);
    presetButton.setBounds(452, 0, juce::jmax(160, getWidth() - 496), 40);
    nextButton.setBounds(presetButton.getRight() + 4, 0, 40, 40);
}
void SuiteBar::refresh() {
    undoButton.setEnabled(history.manager().canUndo());
    redoButton.setEnabled(history.manager().canRedo());
    slotA.setToggleState(!compare.isB(), juce::dontSendNotification);
    slotB.setToggleState(compare.isB(), juce::dontSendNotification);
    const auto name = processor.presetTitle();
    const auto shown = name.length() > 24 ? name.substring(0, 23) + "..." : name;
    presetButton.setButtonText(name.isEmpty() ? "Presets" : shown + (processor.presetEdited() ? " *" : ""));
    presetButton.setTitle("Presets: " + presetButton.getButtonText());
}
// Steps through the browser's list order and wraps at the ends. An unknown name steps from either end.
void SuiteBar::step(int direction) {
    const auto items = presets.filtered({});
    if (items.isEmpty()) return;
    const auto name = processor.presetTitle();
    const auto found = std::find_if(items.begin(), items.end(), [&name](const PresetStore::Item& item) { return item.name == name; });
    auto from = static_cast<int>(std::distance(items.begin(), found));
    if (found == items.end() && direction > 0) from = -1;
    processor.loadPreset(items[(from + direction + items.size()) % items.size()]);
    refresh();
}
void SuiteBar::openBrowser() {
    if (callout != nullptr) { callout->dismiss(); return; }
    auto content = createPresetBrowser(processor, getLookAndFeel());
    content->setSize(440, 460);
    auto& box = juce::CallOutBox::launchAsynchronously(std::move(content), presetButton.getScreenBounds(), nullptr);
    callout = &box;
}
}
