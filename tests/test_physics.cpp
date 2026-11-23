#include <doctest/doctest.h>

#include <cmath>

#include "core/fixed_timestep.hpp"
#include "helpers.hpp"

using namespace tj;

namespace {

// A tiny tower where every platform spans wall to wall, so jumps always pass through the next one.
TowerConfig flatTower(int count = 10, float scroll = 0.0f) {
    TowerConfig t;
    t.id = "test";
    t.seed = 1;
    t.totalPlatformCount = count;
    t.towerHalfWidth = 4.5f;
    t.startHeight = 0.0f;
    t.fullWidthInterval = 1;  // every floor is a landmark = full width
    t.gaps = {{1, count, 1.5f, 1.5f}};
    t.widths = {{1, count, 9.0f, 9.0f}};
    t.scroll = {{1, count, scroll}};
    t.types = {{1, count, {{"normal", 1.0f}}}};
    return t;
}

struct Fixture {
    GameplayConfig g;
    TowerConfig t;
    Game game;
    Fixture() {
        tjtest::loadShipped(g, t);
        game.init(g, t);
    }
    Transform& tr() { return game.registry().get<Transform>(game.player()); }
    Body& body() { return game.registry().get<Body>(game.player()); }
    PlayerState& ps() { return game.registry().get<PlayerState>(game.player()); }
};

InputState jumpNow() { return InputState{0.0f, true, true}; }

}  // namespace

TEST_CASE("a standing jump clears exactly one floor") {
    Fixture f;
    const float start = f.tr().y;
    const float apex = tjtest::runFor(f.game, jumpNow(), 1.0f) - start;
    CHECK(apex > 1.52f);         // higher than the widest gap...
    CHECK(apex < 2 * 1.44f);     // ...but not two floors
}

TEST_CASE("running speed turns into jump height") {
    Fixture f;
    CHECK(f.game.jumpSpeedFor(0) == doctest::Approx(f.g.movement.jumpBaseSpeed));
    CHECK(f.game.jumpSpeedFor(f.g.movement.maxSpeed) ==
          doctest::Approx(f.g.movement.jumpBaseSpeed + f.g.movement.jumpSpeedBonus));
    CHECK(f.game.jumpSpeedFor(5) < f.game.jumpSpeedFor(8));

    // Build up speed, then jump: a full-speed jump should clear several floors.
    tjtest::runFor(f.game, InputState{1.0f, false, false}, 0.45f);
    CHECK(std::fabs(f.body().vx) == doctest::Approx(f.g.movement.maxSpeed));
    const float start = f.tr().y;
    const float apex = tjtest::runFor(f.game, InputState{1.0f, true, true}, 1.2f) - start;
    CHECK(apex > 3 * 1.52f);
}

TEST_CASE("platforms are one-way: up through from below, land from above") {
    GameplayConfig g;
    TowerConfig unused;
    tjtest::loadShipped(g, unused);
    Game game;
    game.init(g, flatTower());
    auto& ps = game.registry().get<PlayerState>(game.player());
    REQUIRE(ps.grounded);
    REQUIRE(ps.floorIndex == 1);

    InputState in = jumpNow();
    bool landedWhileRising = false;
    for (int i = 0; i < 240; ++i) {
        game.step(in, 1.0f / 120.0f);
        in.jumpPressed = false;
        const Body& b = game.registry().get<Body>(game.player());
        if (ps.grounded && b.vy > 0) landedWhileRising = true;
    }
    CHECK_FALSE(landedWhileRising);
    CHECK(ps.grounded);
    CHECK(ps.floorIndex == 2);  // went up through floor 2 and landed on top of it
    CHECK(game.stats().bestFloor == 1);
}

TEST_CASE("wall bounce reverses and boosts horizontal speed") {
    Fixture f;
    tjtest::runFor(f.game, InputState{1.0f, false, false}, 0.3f);
    f.game.step(InputState{1.0f, true, true}, 1.0f / 120.0f);  // take off
    // Keep pushing right into the wall until we bounce; remember the speed on impact.
    float before = 0;
    for (int i = 0; i < 240 && f.body().vx > 0; ++i) {
        before = f.body().vx;
        f.game.step(InputState{1.0f, true, false}, 1.0f / 120.0f);
    }
    REQUIRE(f.body().vx < 0);
    const auto& w = f.g.wallBounce;
    CHECK(-f.body().vx == doctest::Approx(std::min(before * w.speedMultiplier + w.speedBonus, w.maxBounceSpeed)).epsilon(0.05));
}

TEST_CASE("grip removes drift but never the speed the player is asking for") {
    GameplayConfig g;
    TowerConfig unused;
    tjtest::loadShipped(g, unused);
    Game game;
    game.init(g, flatTower());
    // Note: reset() rebuilds the registry, so component references are fetched after it.
    auto dropOnto = [&](float vx, float axis) {
        game.reset();
        auto& ps = game.registry().get<PlayerState>(game.player());
        auto& b = game.registry().get<Body>(game.player());
        auto& t = game.registry().get<Transform>(game.player());
        ps.grounded = false;
        t.y += 0.5f;
        b.vx = vx;
        b.vy = -1.0f;
        for (int i = 0; i < 60 && !ps.grounded; ++i) game.step(InputState{axis, false, false}, 1.0f / 120.0f);
        REQUIRE(ps.grounded);
    };
    auto b = [&]() -> Body& { return game.registry().get<Body>(game.player()); };
    auto ps = [&]() -> PlayerState& { return game.registry().get<PlayerState>(game.player()); };

    dropOnto(15.0f, 0.0f);  // stone, no key held: all drift is scrubbed
    CHECK(std::fabs(b().vx) < 0.5f);
    CHECK(ps().jumpCharge == doctest::Approx(15.0f).epsilon(0.05));  // ...but banked for the next jump

    dropOnto(9.0f, 1.0f);  // holding the direction of travel: keep running
    CHECK(b().vx > 8.0f);
}

TEST_CASE("standing still spends the banked jump charge") {
    Fixture f;
    f.ps().jumpCharge = 20.0f;
    f.ps().jumpChargeTimer = 10.0f;
    tjtest::runFor(f.game, InputState{}, f.g.movement.standStillGrace + 0.05f);
    CHECK(f.ps().jumpCharge == 0.0f);
}

TEST_CASE("the camera's auto-scroll eventually catches an idle player") {
    GameplayConfig g;
    TowerConfig unused;
    tjtest::loadShipped(g, unused);
    Game game;
    game.init(g, flatTower(40, 3.0f));
    for (int i = 0; i < 120 * 20 && !game.gameOver(); ++i) game.step(InputState{}, 1.0f / 120.0f);
    CHECK(game.gameOver());
}

TEST_CASE("platforms stream in and out without growing the pool") {
    GameplayConfig g;
    TowerConfig unused;
    tjtest::loadShipped(g, unused);
    Game game;
    game.init(g, flatTower(400, 0.0f));
    auto& t = game.registry().get<Transform>(game.player());
    auto& ps = game.registry().get<PlayerState>(game.player());
    size_t maxAlive = 0;
    // Teleport the player up the tower; streaming should keep only a window of platforms alive.
    for (int floor = 1; floor < 380; floor += 3) {
        ps.grounded = false;
        t.y = 1.5f * floor + 0.2f;
        for (int i = 0; i < 30; ++i) game.step(InputState{}, 1.0f / 120.0f);
        maxAlive = std::max(maxAlive, game.registry().aliveCount());
    }
    CHECK(maxAlive < 40);
    CHECK(game.registry().slotCount() < 45);  // ids were recycled, not leaked
}

TEST_CASE("runs are deterministic") {
    auto play = []() {
        GameplayConfig g;
        TowerConfig t;
        tjtest::loadShipped(g, t);
        Game game;
        game.init(g, t);
        float dir = 1.0f;
        for (int i = 0; i < 120 * 30 && !game.gameOver(); ++i) {
            const auto& b = game.registry().get<Body>(game.player());
            if (b.vx == 0 && i > 10) dir = -dir;
            game.step(InputState{dir, true, i % 50 == 0}, 1.0f / 120.0f);
        }
        return game.stats();
    };
    const RunStats a = play(), b = play();
    CHECK(a.score == b.score);
    CHECK(a.bestFloor == b.bestFloor);
}

TEST_CASE("fixed timestep runs the same number of steps regardless of frame rate") {
    auto count = [](float frame, float total) {
        FixedTimestep clock(1.0f / 120.0f);
        int steps = 0;
        for (float t = 0; t < total - 1e-6f; t += frame) clock.advance(frame, [&](float) { ++steps; });
        return steps;
    };
    CHECK(std::abs(count(1.0f / 30, 2.0f) - 240) <= 1);
    CHECK(std::abs(count(1.0f / 144, 2.0f) - 240) <= 1);
}
