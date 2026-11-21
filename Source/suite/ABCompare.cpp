#include "ABCompare.h"
#include "ParameterUndo.h"
namespace suite {
ABCompare::ABCompare(ParameterUndo& undo) : history(undo) {}
void ABCompare::prime() {
    slots[0] = history.capture();
    slots[1] = slots[0];
    history.reset(0);
}
void ABCompare::flushActive() { slots[static_cast<size_t>(history.activeSlot())] = history.capture(); }
bool ABCompare::isB() const noexcept { return history.activeSlot() == 1; }
// A switch is not an undo step, so undo never applies the other slot's state.
void ABCompare::toggle() {
    flushActive();
    history.selectSlot(1 - history.activeSlot());
    if (slots[static_cast<size_t>(history.activeSlot())].getSize() > 0)
        history.restore(slots[static_cast<size_t>(history.activeSlot())]);
}
void ABCompare::copyAtoB() { copyBetween(0, 1); }
void ABCompare::copyBtoA() { copyBetween(1, 0); }
// A copy replaces the target slot, so its history no longer applies.
void ABCompare::copyBetween(int from, int to) {
    flushActive();
    slots[static_cast<size_t>(to)] = slots[static_cast<size_t>(from)];
    history.clearSlot(to);
    if (history.activeSlot() == to && slots[static_cast<size_t>(to)].getSize() > 0)
        history.restore(slots[static_cast<size_t>(to)]);
}
void ABCompare::writeXml(juce::XmlElement& suite) const {
    suite.setAttribute("active", history.activeSlot());
    suite.setAttribute("a", slots[0].toBase64Encoding());
    suite.setAttribute("b", slots[1].toBase64Encoding());
}
bool ABCompare::readXml(const juce::XmlElement& suite) {
    const auto a = slots[0].fromBase64Encoding(suite.getStringAttribute("a"));
    const auto b = slots[1].fromBase64Encoding(suite.getStringAttribute("b"));
    history.reset(suite.getIntAttribute("active", 0) == 1 ? 1 : 0);
    return a && b && slots[0].getSize() > 0 && slots[1].getSize() > 0;
}
}
