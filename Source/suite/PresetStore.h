#pragma once
#include <juce_data_structures/juce_data_structures.h>
namespace suite {
class PresetStore {
public:
    struct Item {
        juce::String name;
        bool factory = false;
        bool favorite = false;
        int factoryIndex = -1;
        juce::File file;
    };
    PresetStore(juce::String pluginName, juce::StringArray factoryNames, juce::File root = {});
    juce::File directory() const { return root; }
    void rescan();
    const juce::Array<Item>& all() const { return items; }
    juce::Array<Item> filtered(const juce::String& query) const;
    static juce::String cleanName(juce::String);
    bool save(const juce::String& name, const juce::XmlElement&);
    bool rename(const Item&, const juce::String& name);
    bool remove(const Item&);
    void setFavorite(const Item&, bool favorite);
    std::unique_ptr<juce::XmlElement> load(const Item&) const;
private:
    juce::StringArray factory;
    juce::File root;
    juce::Array<Item> items;
    juce::StringArray favorites;
    void readFavorites();
    void writeFavorites() const;
    bool isFavorite(bool factoryItem, const juce::String& name) const;
    static juce::String favoriteKey(bool factoryItem, const juce::String& name);
};
}
