#include <doctest/doctest.h>

#include "helpers.hpp"

using namespace tj;

TEST_CASE("shipped configs load and validate") {
    GameplayConfig g;
    TowerConfig t;
    std::string text;
    REQUIRE(readTextFile(std::string(TJ_ASSETS) + "config/gameplay.json", text));
    CHECK(parseGameplayConfig(text, g).ok());
    REQUIRE(readTextFile(std::string(TJ_ASSETS) + "config/tower_00.json", text));
    CHECK(parseTowerConfig(text, t).ok());
    std::vector<std::string> errors;
    validateTower(t, g, errors);
    CHECK(errors.empty());
    CHECK(g.findType("ice") >= 0);
    CHECK(t.totalPlatformCount == 400);
}

TEST_CASE("broken JSON is reported, not crashed on") {
    GameplayConfig g;
    LoadResult r = parseGameplayConfig("{ not json", g, "bad.json");
    REQUIRE_FALSE(r.ok());
    CHECK(r.errors[0].find("bad.json") != std::string::npos);
}

TEST_CASE("band gaps and overlaps are named precisely") {
    GameplayConfig g;
    TowerConfig t;
    tjtest::loadShipped(g, t);
    t.widths = {{1, 10, 2, 3}, {15, 400, 2, 3}};      // gap 11..14
    t.scroll = {{1, 100, 0}, {90, 400, 1}};          // overlap
    std::vector<std::string> errors;
    validateTower(t, g, errors);
    REQUIRE(errors.size() == 2);
    CHECK(errors[0].find("gap at indices 11..14") != std::string::npos);
    CHECK(errors[1].find("[90..400] overlaps") != std::string::npos);
}

TEST_CASE("unknown platform types are rejected") {
    GameplayConfig g;
    TowerConfig t;
    tjtest::loadShipped(g, t);
    t.types = {{1, 400, {{"lava", 1.0f}}}};
    std::vector<std::string> errors;
    validateTower(t, g, errors);
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("lava") != std::string::npos);
}
