#pragma once
#include <string>
namespace ote {
inline constexpr double minFrequency = 20.0;
inline constexpr double maxFrequency = 20000.0;
double noteFrequency(int midiNote) noexcept;
double frequencyNote(double frequency) noexcept;
int nearestNote(double frequency) noexcept;
double snapFrequency(double frequency, double upper = maxFrequency) noexcept;
double effectiveFrequency(double frequency, bool snap, double sampleRate) noexcept;
double centsFromNote(double frequency, int midiNote) noexcept;
std::string noteName(int midiNote, bool flats);
}
