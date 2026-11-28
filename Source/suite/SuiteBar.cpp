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
        nameInput.setComponentID("presetNameInput");
        nameInput.setTitle("Preset name");
        nameInput.setTextToShowWhenEmpty("Preset name", ui::muted);
        nameInput.onReturnKey = [this] { commitName(); };
        nameInput.onEscapeKey = [this] { cancelName(); };
        nameInput.onTextChange = [this] { disarm(); };
        addChildComponent(nameInput);
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
            askName("Type a preset name and press Enter (Esc to cancel).", "My preset", [this](const juce::String& name) {
                auto save = [this, name] {
                    if (!processor.savePreset(name)) { status.setText("That name is reserved or invalid.", juce::dontSendNotification); return; }
                    reload();
                };
                if (!presets.exists(name)) { save(); return; }
                disarm();
                armed = &saveButton; pending = save; saveButton.setButtonText("Replace?"); startTimer(3000);
                status.setText("A preset named " + name + " exists. Click Replace? to overwrite it.", juce::dontSendNotification);
            });
        };
        renameButton.onClick = [this] {
            if (const auto* item = selected()) if (!item->factory)
                askName("Type a new name and press Enter (Esc to cancel).", item->name, [this, old = *item](const juce::String& name) {
                    processor.presets().rename(old, name) ? reload() : status.setText("Could not rename that preset.", juce::dontSendNotification);
                });
        };
        deleteButton.onClick = [this] {
            if (armed == &deleteButton) { const auto remove = pending; disarm(); remove(); return; }
            const auto* item = selected();
            if (item == nullptr || item->factory) return;
            disarm();
            armed = &deleteButton; pending = [this, doomed = *item] { if (presets.remove(doomed)) reload(); };
            deleteButton.setButtonText("Delete?"); startTimer(3000);
        };
        status.setColour(juce::Label::textColourId, ui::muted);
        status.setFont(ui::font(12));
        addAndMakeVisible(status);
        reload();
    }
    ~Browser() override { list.setModel(nullptr); setLookAndFeel(nullptr); }
    void resized() override {
        search.setBounds(8, 8, getWidth() - 16, 32);
        const int inputSpace = nameInput.isVisible() ? 36 : 0;
        list.setBounds(8, 48, getWidth() - 16, getHeight() - 148 - inputSpace);
        if (nameInput.isVisible()) nameInput.setBounds(8, getHeight() - 130, getWidth() - 16, 30);
        status.setBounds(8, getHeight() - 96, getWidth() - 16, 20);
        const int width = (getWidth() - 28) / 4;
        loadButton.setBounds(8, getHeight() - 68, width, 40);
        saveButton.setBounds(12 + width, getHeight() - 68, width, 40);
        renameButton.setBounds(16 + width * 2, getHeight() - 68, width, 40);
        deleteButton.setBounds(20 + width * 3, getHeight() - 68, width, 40);
    }
    // Replace and delete need a second click within 3 s.
    void disarm() {
        stopTimer();
        armed = nullptr; pending = nullptr;
        saveButton.setButtonText("Save"); deleteButton.setButtonText("Delete");
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
        juce::Path star;
        star.addStar({20.0f, static_cast<float>(height) * 0.5f}, 5, 3.6f, 8.0f, 0.0f);
        if (item.favorite) {
            g.setColour(ui::copper);
            g.fillPath(star);
        } else {
            g.setColour(ui::muted.withAlpha(0.65f));
            g.strokePath(star, juce::PathStrokeType(1.3f));
        }
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
    void selectedRowsChanged(int) override {
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
    void timerCallback() override { disarm(); }
    void askName(const juce::String& prompt, const juce::String& initial, std::function<void(const juce::String&)> done) {
        disarm();
        nameAction = std::move(done);
        status.setText(prompt, juce::dontSendNotification);
        nameInput.setText(initial, juce::dontSendNotification);
        nameInput.setVisible(true);
        resized();
        nameInput.grabKeyboardFocus();
        nameInput.selectAll();
    }
    void commitName() {
        const auto name = nameInput.getText();
        const auto action = std::move(nameAction);
        nameAction = nullptr;
        nameInput.setVisible(false);
        resized();
        if (action) action(name);
    }
    void cancelName() {
        nameAction = nullptr;
        nameInput.setVisible(false);
        resized();
        status.setText({}, juce::dontSendNotification);
    }
    TunerProcessor& processor;
    PresetStore& presets;
    juce::TextEditor search, nameInput;
    juce::ListBox list {"Presets", this};
    juce::TextButton loadButton {"Load"}, saveButton {"Save"}, renameButton {"Rename"}, deleteButton {"Delete"};
    juce::Label status;
    juce::Array<PresetStore::Item> rows;
    juce::TextButton* armed = nullptr;
    std::function<void()> pending;
    std::function<void(const juce::String&)> nameAction;
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
    slotA.setTitle("Slot A"); slotB.setTitle("Slot B");
    copyAtoB.setTitle("Copy A to B"); copyBtoA.setTitle("Copy B to A"); previousButton.setTitle("Previous preset"); nextButton.setTitle("Next preset");
    previousButton.setTooltip("Previous preset"); nextButton.setTooltip("Next preset");
    // The arrows follow the browser's order and wrap.
    for (auto* arrow : {&previousButton, &nextButton}) arrow->onClick = [this, direction = arrow == &nextButton ? 1 : -1] {
        const auto items = presets.filtered({});
        if (items.isEmpty()) return;
        const auto name = processor.presetTitle();
        const auto found = std::find_if(items.begin(), items.end(), [&name](const PresetStore::Item& item) { return item.name == name; });
        auto from = static_cast<int>(std::distance(items.begin(), found));
        if (found == items.end() && direction > 0) from = -1;
        processor.loadPreset(items[(from + direction + items.size()) % items.size()]);
        refresh();
    };
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
    const bool slotBActive = compare.isB();
    const auto name = processor.presetTitle();
    if (slotBActive != lastSlotB || name != lastPresetTitle) {
        lastSlotB = slotBActive;
        lastPresetTitle = name;
        if (auto* b = dynamic_cast<Browser*>(activeBrowser.getComponent())) b->disarm();
    }
    undoButton.setEnabled(history.manager().canUndo());
    redoButton.setEnabled(history.manager().canRedo());
    slotA.setToggleState(!slotBActive, juce::dontSendNotification);
    slotB.setToggleState(slotBActive, juce::dontSendNotification);
    auto shown = name;
    if (name.length() > 24) shown = name.substring(0, 23) + "...";
    if (processor.presetEdited()) shown += " *";
    if (name.isEmpty()) shown = "Presets";
    presetButton.setButtonText(shown);
    presetButton.setTitle("Presets: " + shown);
}
void SuiteBar::openBrowser() {
    if (callout != nullptr) { callout->dismiss(); return; }
    auto content = createPresetBrowser(processor, getLookAndFeel());
    content->setSize(440, 460);
    activeBrowser = content.get();
    auto& box = juce::CallOutBox::launchAsynchronously(std::move(content), presetButton.getScreenBounds(), nullptr);
    callout = &box;
}
}
