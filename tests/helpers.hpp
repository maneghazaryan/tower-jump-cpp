#pragma once
#include <string>

#include "core/config.hpp"
#include "core/game.hpp"

namespace tjtest {

inline void loadShipped(tj::GameplayConfig& g, tj::TowerConfig& t) {
    std::string text;
    tj::readTextFile(std::string(TJ_ASSETS) + "config/gameplay.json", text);
    tj::parseGameplayConfig(text, g);
    tj::readTextFile(std::string(TJ_ASSETS) + "config/tower_00.json", text);
    tj::parseTowerConfig(text, t);
}

// Runs the game for `seconds` with a constant input; returns the highest feet position reached.
inline float runFor(tj::Game& game, const tj::InputState& input, float seconds, float dt = 1.0f / 120.0f) {
    float best = -1e9f;
    tj::InputState in = input;
    for (float t = 0; t < seconds; t += dt) {
        game.step(in, dt);
        in.jumpPressed = false;
        const auto& tr = game.registry().get<tj::Transform>(game.player());
        if (tr.y > best) best = tr.y;
    }
    return best;
}

}  // namespace tjtest
