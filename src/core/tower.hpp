#pragma once
// Builds the platform layout of a tower from its config. Pure function of (config, seed):
// the same file always produces the same tower, byte for byte.

#include <vector>

#include "config.hpp"

namespace tj {

struct PlatformSpec {
    int index = 0;        // 1-based floor number; floor shown to the player is index - 1
    float x = 0.0f;       // centre
    float y = 0.0f;       // top surface
    float width = 0.0f;
    int typeIndex = 0;    // into GameplayConfig::platformTypes
    bool landmark = false;
};

std::vector<PlatformSpec> generateTower(const TowerConfig& tower, const GameplayConfig& gameplay);

}  // namespace tj
