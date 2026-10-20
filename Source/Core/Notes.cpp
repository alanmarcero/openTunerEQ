#include "Notes.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace ote {
namespace {
double validFrequency(double value) noexcept {
    return std::isfinite(value) ? std::max(0.001, value) : 440.0;
}
}
double noteFrequency(int note, double reference) noexcept { return reference * std::exp2((note - 69.0) / 12.0); }
double frequencyNote(double hz, double reference) noexcept { return 69.0 + 12.0 * std::log2(validFrequency(hz) / reference); }
int nearestNote(double hz, double reference) noexcept { return static_cast<int>(std::lround(frequencyNote(hz, reference))); }
double snapFrequency(double hz, double reference, double upper) noexcept {
    upper = std::max(minFrequency * 1.06, std::min(upper, maxFrequency));
    const auto low = static_cast<int>(std::ceil(frequencyNote(minFrequency, reference)));
    const auto high = static_cast<int>(std::floor(frequencyNote(upper, reference)));
    return noteFrequency(std::clamp(nearestNote(hz, reference), low, high), reference);
}
double effectiveFrequency(double hz, bool snap, double rate, double reference) noexcept {
    const auto upper = std::clamp(rate * 0.49, minFrequency * 1.06, maxFrequency);
    return snap ? snapFrequency(hz, reference, upper) : std::clamp(validFrequency(hz), minFrequency, upper);
}
double centsFromNote(double hz, int note, double reference) noexcept { return 100.0 * (frequencyNote(hz, reference) - note); }
std::string noteName(int note, bool flats) {
    static constexpr std::array<const char*, 12> sharps {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static constexpr std::array<const char*, 12> flatNames {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
    const auto pitch = (note % 12 + 12) % 12;
    const auto octave = static_cast<int>(std::floor(note / 12.0)) - 1;
    return std::string((flats ? flatNames : sharps)[static_cast<size_t>(pitch)]) + std::to_string(octave);
}
}
