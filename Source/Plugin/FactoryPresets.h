#pragma once
#include <utility>
#include <vector>
struct FactoryPreset {
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};
const std::vector<FactoryPreset>& factoryPresets();
