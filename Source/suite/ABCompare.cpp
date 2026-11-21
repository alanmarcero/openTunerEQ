#include "ABCompare.h"
#include "ParameterUndo.h"
namespace suite {
ABCompare::ABCompare(Export exporter, Import importer, ParameterUndo& undo)
    : exportState(std::move(exporter)), importState(std::move(importer)), history(undo) {}
void ABCompare::prime() {
    active = 0;
    exportState(slots[0]);
    slots[1] = slots[0];
    history.clearSlot(0); history.clearSlot(1); history.selectSlot(0);
}
void ABCompare::flushActive() { exportState(slots[static_cast<size_t>(active)]); }
// A switch swaps histories and is not an undo step, so undo never applies the other slot's state.
void ABCompare::toggle() {
    flushActive();
    active = 1 - active;
    history.selectSlot(active);
    if (slots[static_cast<size_t>(active)].getSize() > 0)
        importState(slots[static_cast<size_t>(active)]);
}
void ABCompare::copyAtoB() { copyBetween(0, 1); }
void ABCompare::copyBtoA() { copyBetween(1, 0); }
// A copy replaces the target slot, so its history no longer applies.
void ABCompare::copyBetween(int from, int to) {
    flushActive();
    slots[static_cast<size_t>(to)] = slots[static_cast<size_t>(from)];
    history.clearSlot(to);
    if (active == to && slots[static_cast<size_t>(to)].getSize() > 0)
        importState(slots[static_cast<size_t>(to)]);
}
void ABCompare::writeXml(juce::XmlElement& suite) const {
    suite.setAttribute("active", active);
    suite.setAttribute("a", slots[0].toBase64Encoding());
    suite.setAttribute("b", slots[1].toBase64Encoding());
}
bool ABCompare::readXml(const juce::XmlElement& suite) {
    active = suite.getIntAttribute("active", 0) == 1 ? 1 : 0;
    const auto a = slots[0].fromBase64Encoding(suite.getStringAttribute("a"));
    const auto b = slots[1].fromBase64Encoding(suite.getStringAttribute("b"));
    history.clearSlot(0); history.clearSlot(1); history.selectSlot(active);
    return a && b && slots[0].getSize() > 0 && slots[1].getSize() > 0;
}
}
