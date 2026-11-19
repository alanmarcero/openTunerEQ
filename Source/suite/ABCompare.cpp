#include "ABCompare.h"
#include "ParameterUndo.h"
namespace suite {
ABCompare::ABCompare(Export exporter, Import importer, ParameterUndo& undo)
    : exportState(std::move(exporter)), importState(std::move(importer)), history(undo) {}
void ABCompare::prime() {
    active = 0;
    exportState(slots[0]);
    slots[1] = slots[0];
}
void ABCompare::flushActive() { exportState(slots[static_cast<size_t>(active)]); }
void ABCompare::toggle() {
    history.applyChange("A/B", [this] {
        active = 1 - active;
        if (slots[static_cast<size_t>(active)].getSize() > 0)
            importState(slots[static_cast<size_t>(active)]);
    });
}
void ABCompare::copyAtoB() { copyBetween(0, 1); }
void ABCompare::copyBtoA() { copyBetween(1, 0); }
void ABCompare::copyBetween(int from, int to) {
    history.applyChange(from == 0 ? "Copy A to B" : "Copy B to A", [this, from, to] {
        slots[static_cast<size_t>(to)] = slots[static_cast<size_t>(from)];
        if (active == to && slots[static_cast<size_t>(to)].getSize() > 0)
            importState(slots[static_cast<size_t>(to)]);
    });
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
    return a && b && slots[0].getSize() > 0 && slots[1].getSize() > 0;
}
}
