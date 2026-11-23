#include <doctest/doctest.h>

#include <cmath>

#include "core/tower.hpp"
#include "helpers.hpp"

using namespace tj;

TEST_CASE("same seed, same tower") {
    GameplayConfig g;
    TowerConfig t;
    tjtest::loadShipped(g, t);
    auto a = generateTower(t, g), b = generateTower(t, g);
    REQUIRE(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        CHECK(a[i].x == b[i].x);
        CHECK(a[i].y == b[i].y);
        CHECK(a[i].width == b[i].width);
        CHECK(a[i].typeIndex == b[i].typeIndex);
    }
    t.seed += 1;
    auto c = generateTower(t, g);
    CHECK(c[5].x != a[5].x);
}

TEST_CASE("layout respects bands, walls and landmarks") {
    GameplayConfig g;
    TowerConfig t;
    tjtest::loadShipped(g, t);
    auto tower = generateTower(t, g);
    REQUIRE(tower.size() == 400);
    CHECK(tower[0].landmark);
    CHECK(tower[100].landmark);  // index 101 = floor 100
    CHECK(tower[100].width == doctest::Approx(t.towerHalfWidth * 2));
    for (size_t i = 1; i < tower.size(); ++i) {
        const float gap = tower[i].y - tower[i - 1].y;
        CHECK(gap >= 1.44f - 1e-4f);
        CHECK(gap <= 1.52f + 1e-4f);
        CHECK(std::fabs(tower[i].x) + tower[i].width * 0.5f <= t.towerHalfWidth + 1e-4f);
    }
    for (int i = 0; i < 25; ++i) CHECK(g.platformTypes[tower[i].typeIndex].typeId == "normal");
}

TEST_CASE("changing the landmark interval does not re-roll other floors") {
    GameplayConfig g;
    TowerConfig t;
    tjtest::loadShipped(g, t);
    auto a = generateTower(t, g);
    t.fullWidthInterval = 50;
    auto b = generateTower(t, g);
    CHECK(a[60].x == b[60].x);
    CHECK(a[60].width == b[60].width);
    CHECK(b[50].landmark);
    CHECK_FALSE(a[50].landmark);
}
