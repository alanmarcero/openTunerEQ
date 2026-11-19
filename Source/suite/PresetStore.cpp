#include "PresetStore.h"
namespace suite {
PresetStore::PresetStore(juce::String pluginName, juce::StringArray factoryNames, juce::File directory)
    : factory(std::move(factoryNames)),
      root(directory.getFullPathName().isEmpty()
               ? juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                     .getChildFile("Alan Marcero").getChildFile(pluginName).getChildFile("Presets")
               : std::move(directory)) {
    rescan();
}
juce::String PresetStore::cleanName(juce::String name) {
    name = name.trim();
    if (name.isEmpty() || name.length() > 64) return {};
    if (name.containsAnyOf("\\/:*?\"<>|") || name == "." || name == "..") return {};
    return name;
}
juce::String PresetStore::favoriteKey(bool factoryItem, const juce::String& name) {
    return (factoryItem ? "factory:" : "user:") + name;
}
bool PresetStore::isFavorite(bool factoryItem, const juce::String& name) const {
    return favorites.contains(favoriteKey(factoryItem, name));
}
void PresetStore::readFavorites() {
    favorites.clear();
    for (auto line : juce::StringArray::fromLines(root.getChildFile("favorites.txt").loadFileAsString())) {
        line = line.trim();
        if ((line.startsWith("factory:") || line.startsWith("user:")) && !favorites.contains(line)) favorites.add(line);
    }
}
void PresetStore::writeFavorites() const {
    root.createDirectory();
    root.getChildFile("favorites.txt").replaceWithText(favorites.joinIntoString("\n"));
}
void PresetStore::rescan() {
    readFavorites();
    items.clear();
    for (int i = 0; i < factory.size(); ++i)
        items.add({factory[i], true, isFavorite(true, factory[i]), i, {}});
    if (!root.isDirectory()) return;
    for (const auto& entry : root.findChildFiles(juce::File::findFiles, false, "*.xml")) {
        const auto name = cleanName(entry.getFileNameWithoutExtension());
        if (name.isEmpty() || factory.contains(name)) continue;
        items.add({name, false, isFavorite(false, name), -1, entry});
    }
}
juce::Array<PresetStore::Item> PresetStore::filtered(const juce::String& query) const {
    const auto needle = query.trim().toLowerCase();
    juce::Array<Item> favoritesFirst, rest;
    for (const auto& item : items) {
        if (needle.isNotEmpty() && !item.name.toLowerCase().contains(needle)) continue;
        (item.favorite ? favoritesFirst : rest).add(item);
    }
    favoritesFirst.addArray(rest);
    return favoritesFirst;
}
bool PresetStore::save(const juce::String& requested, const juce::XmlElement& state) {
    const auto name = cleanName(requested);
    if (name.isEmpty() || factory.contains(name)) return false;
    root.createDirectory();
    if (!state.writeTo(root.getChildFile(name + ".xml"))) return false;
    rescan();
    return true;
}
bool PresetStore::rename(const Item& item, const juce::String& requested) {
    if (item.factory || !item.file.existsAsFile()) return false;
    const auto name = cleanName(requested);
    if (name.isEmpty() || factory.contains(name)) return false;
    const auto dest = root.getChildFile(name + ".xml");
    if (dest == item.file) return true;
    if (dest.exists()) return false;
    const auto wasFavorite = item.favorite;
    if (!item.file.moveFileTo(dest)) return false;
    if (wasFavorite) {
        favorites.removeString(favoriteKey(false, item.name));
        favorites.addIfNotAlreadyThere(favoriteKey(false, name));
        writeFavorites();
    }
    rescan();
    return true;
}
bool PresetStore::remove(const Item& item) {
    if (item.factory || !item.file.existsAsFile()) return false;
    if (!item.file.deleteFile()) return false;
    favorites.removeString(favoriteKey(false, item.name));
    writeFavorites();
    rescan();
    return true;
}
void PresetStore::setFavorite(const Item& item, bool favorite) {
    const auto key = favoriteKey(item.factory, item.name);
    if (favorite) favorites.addIfNotAlreadyThere(key);
    else favorites.removeString(key);
    writeFavorites();
    rescan();
}
std::unique_ptr<juce::XmlElement> PresetStore::load(const Item& item) const {
    if (item.factory || !item.file.existsAsFile()) return {};
    return juce::XmlDocument::parse(item.file);
}
}
