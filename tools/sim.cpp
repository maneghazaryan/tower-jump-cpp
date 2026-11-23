// Headless simulator: plays Tower Jump with a simple bot and prints what happened.
// Useful for tuning: change gameplay.json, run the sim, compare the numbers.
//
//   tj_sim [seconds=60] [runs=5]

#include <cstdio>
#include <cstdlib>
#include <string>

#include "core/game.hpp"

using namespace tj;

int main(int argc, char** argv) {
    const float seconds = argc > 1 ? std::strtof(argv[1], nullptr) : 60.0f;
    const int runs = argc > 2 ? std::atoi(argv[2]) : 5;

    GameplayConfig g;
    TowerConfig t;
    std::string text;
    if (!readTextFile(std::string(TJ_ASSETS) + "config/gameplay.json", text)) return 1;
    for (auto& e : parseGameplayConfig(text, g).errors) std::fprintf(stderr, "%s\n", e.c_str());
    if (!readTextFile(std::string(TJ_ASSETS) + "config/tower_00.json", text)) return 1;
    for (auto& e : parseTowerConfig(text, t).errors) std::fprintf(stderr, "%s\n", e.c_str());

    Game game;
    game.init(g, t);
    const float dt = 1.0f / 120.0f;

    for (int run = 0; run < runs; ++run) {
        game.reset();
        float dir = (run % 2) ? -1.0f : 1.0f;
        int jumps = 0, bounces = 0, steps = 0;
        float sinceJump = 0;
        // Bot: run back and forth between the walls and jump at speed, like a human would.
        while (steps * dt < seconds && !game.gameOver() && !game.victory()) {
            const Transform& tr = game.registry().get<Transform>(game.player());
            const Body& b = game.registry().get<Body>(game.player());
            const PlayerState& ps = game.registry().get<PlayerState>(game.player());
            if (std::abs(tr.x) > t.towerHalfWidth - 0.6f && ps.grounded) dir = tr.x > 0 ? -1.0f : 1.0f;
            const bool fast = std::abs(b.vx) > g.movement.maxSpeed * (0.6f + 0.1f * float(run % 4));
            const bool jump = ps.grounded && (fast || sinceJump > 1.0f);
            game.step(InputState{dir, jump, jump}, dt);
            sinceJump = jump ? 0 : sinceJump + dt;
            for (const GameEvent& e : game.events()) {
                if (e.type == EventType::Jump) ++jumps;
                if (e.type == EventType::WallBounce) ++bounces;
            }
            ++steps;
        }
        const RunStats& s = game.stats();
        std::printf("run %d: %5.1fs  floor %3d  score %5d  best combo %3d  jumps %3d  wall bounces %3d  %s  entities %zu (slots %zu)\n",
                    run, steps * dt, s.bestFloor, s.score, s.bestCombo, jumps, bounces,
                    game.gameOver() ? "fell" : (game.victory() ? "TOP" : "time"), game.registry().aliveCount(),
                    game.registry().slotCount());
    }
    return 0;
}
