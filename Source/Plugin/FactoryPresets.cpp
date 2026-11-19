#include "FactoryPresets.h"
const std::vector<FactoryPreset>& factoryPresets() {
    static const std::vector<FactoryPreset> presets {
        {"Flat", {}},
        {"Low-end cleanup", {
            {"band0.shape", 3}, {"band0.freq", 32.7032f}, {"band0.slope", 1},
            {"band1.freq", 246.9417f}, {"band1.gain", -3.5f}, {"band1.q", 1.2f},
            {"band2.enabled", 0}, {"band3.enabled", 0}, {"band4.enabled", 0}}},
        {"Kick punch", {
            {"band0.freq", 65.4064f}, {"band0.gain", 5.5f}, {"band0.q", 1.2f},
            {"band1.freq", 246.9417f}, {"band1.gain", -4.0f}, {"band1.q", 2.0f},
            {"band2.freq", 3135.96f}, {"band2.gain", 3.0f}, {"band2.q", 1.4f},
            {"band3.enabled", 0}, {"band4.enabled", 0}}},
        {"Vocal presence", {
            {"band0.shape", 3}, {"band0.freq", 82.4069f},
            {"band1.freq", 246.9417f}, {"band1.gain", -2.5f}, {"band1.q", 1.0f},
            {"band2.freq", 2637.02f}, {"band2.gain", 3.5f}, {"band2.q", 1.0f},
            {"band3.shape", 2}, {"band3.freq", 8372.02f}, {"band3.gain", 2.5f},
            {"band4.enabled", 0}}},
        {"Hat air", {
            {"band0.enabled", 0}, {"band1.enabled", 0}, {"band2.enabled", 0}, {"band3.enabled", 0},
            {"band4.shape", 2}, {"band4.freq", 10548.08f}, {"band4.gain", 4.5f}}},
        {"Off-beat notch", {
            {"band0.shape", 5}, {"band0.freq", 739.989f}, {"band0.q", 8.0f},
            {"band1.shape", 5}, {"band1.freq", 1760.0f}, {"band1.q", 6.0f},
            {"band2.enabled", 0}, {"band3.enabled", 0}, {"band4.enabled", 0}}},
        {"Side high-pass", {
            {"band0.shape", 3}, {"band0.freq", 164.814f}, {"band0.slope", 1}, {"band0.placement", 4},
            {"band1.enabled", 0}, {"band2.enabled", 0}, {"band3.enabled", 0}, {"band4.enabled", 0}}},
        {"Mid focus", {
            {"band0.shape", 3}, {"band0.freq", 174.614f}, {"band0.slope", 1}, {"band0.placement", 4},
            {"band1.freq", 329.628f}, {"band1.gain", -2.0f}, {"band1.q", 1.2f}, {"band1.placement", 3},
            {"band2.freq", 1046.50f}, {"band2.gain", 3.0f}, {"band2.q", 1.0f}, {"band2.placement", 3},
            {"band3.enabled", 0}, {"band4.enabled", 0}}},
        {"Warm lows", {
            {"band0.shape", 1}, {"band0.freq", 195.998f}, {"band0.gain", 3.5f},
            {"band1.enabled", 0}, {"band2.enabled", 0}, {"band3.enabled", 0}, {"band4.enabled", 0}}},
        {"Harshness tame", {
            {"band0.freq", 3520.0f}, {"band0.gain", -3.5f}, {"band0.q", 3.0f},
            {"band1.enabled", 0}, {"band2.enabled", 0}, {"band3.enabled", 0}, {"band4.enabled", 0}}},
        {"Side air", {
            {"band0.shape", 2}, {"band0.freq", 8372.02f}, {"band0.gain", 3.5f}, {"band0.placement", 4},
            {"band1.enabled", 0}, {"band2.enabled", 0}, {"band3.enabled", 0}, {"band4.enabled", 0}}},
    };
    return presets;
}
