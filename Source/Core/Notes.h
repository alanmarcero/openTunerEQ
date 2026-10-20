#pragma once
#include <string>
namespace ote {
inline constexpr double minFrequency = 20.0;
inline constexpr double maxFrequency = 20000.0;
inline constexpr double standardPitch = 440.0;
// reference is the A4 frequency of the 12-tone equal-tempered grid.
double noteFrequency(int midiNote, double reference) noexcept;
double frequencyNote(double frequency, double reference) noexcept;
int nearestNote(double frequency, double reference) noexcept;
double snapFrequency(double frequency, double reference, double upper = maxFrequency) noexcept;
double effectiveFrequency(double frequency, bool snap, double sampleRate, double reference) noexcept;
double centsFromNote(double frequency, int midiNote, double reference) noexcept;
std::string noteName(int midiNote, bool flats);
}
