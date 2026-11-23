#include "config.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace tj {

using nlohmann::json;

namespace {

// Small helper that reads optional fields and records type errors with a readable path.
class Reader {
public:
    Reader(const std::string& file, std::vector<std::string>& errors) : file_(file), errors_(errors) {}

    template <typename T>
    void get(const json& obj, const char* key, T& out, const std::string& path) {
        if (!obj.is_object() || !obj.contains(key)) return;  // optional: keep the default
        try {
            out = obj.at(key).get<T>();
        } catch (const json::exception&) {
            error(path + "." + key, "has the wrong type");
        }
    }

    template <typename T>
    void require(const json& obj, const char* key, T& out, const std::string& path) {
        if (!obj.is_object() || !obj.contains(key)) {
            error(path + "." + key, "is missing");
            return;
        }
        get(obj, key, out, path);
    }

    void error(const std::string& field, const std::string& what) {
        errors_.push_back(file_ + ": " + field + " " + what);
    }

private:
    std::string file_;
    std::vector<std::string>& errors_;
};

template <typename Band>
void checkCoverage(std::vector<Band> bands, int total, const std::string& listName, const std::string& file,
                   std::vector<std::string>& errors) {
    if (bands.empty()) {
        errors.push_back(file + ": " + listName + " is empty");
        return;
    }
    std::sort(bands.begin(), bands.end(), [](const Band& a, const Band& b) { return a.fromIndex < b.fromIndex; });
    int expected = 1;
    for (const Band& b : bands) {
        const std::string range = "[" + std::to_string(b.fromIndex) + ".." + std::to_string(b.toIndex) + "]";
        if (b.fromIndex > b.toIndex) {
            errors.push_back(file + ": " + listName + " band " + range + " ends before it starts");
            continue;
        }
        if (b.fromIndex > expected) {
            errors.push_back(file + ": " + listName + " leaves a gap at indices " + std::to_string(expected) + ".." +
                             std::to_string(b.fromIndex - 1));
        } else if (b.fromIndex < expected) {
            errors.push_back(file + ": " + listName + " band " + range + " overlaps the previous band");
        }
        expected = std::max(expected, b.toIndex + 1);
    }
    if (expected - 1 != total) {
        errors.push_back(file + ": " + listName + " covers up to " + std::to_string(expected - 1) +
                         " but totalPlatformCount is " + std::to_string(total));
    }
}

}  // namespace

int GameplayConfig::findType(const std::string& id) const {
    for (size_t i = 0; i < platformTypes.size(); ++i)
        if (platformTypes[i].typeId == id) return static_cast<int>(i);
    return -1;
}

float TowerConfig::scrollSpeedAt(int index) const {
    for (const ScrollBand& b : scroll)
        if (index >= b.fromIndex && index <= b.toIndex) return b.unitsPerSecond;
    return scroll.empty() ? 0.0f : scroll.back().unitsPerSecond;
}

LoadResult parseGameplayConfig(const std::string& text, GameplayConfig& out, const std::string& file) {
    LoadResult result;
    json root = json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        result.errors.push_back(file + ": is not valid JSON");
        return result;
    }
    Reader r(file, result.errors);

    const json& m = root.value("movement", json::object());
    MovementConfig& mv = out.movement;
    r.get(m, "maxSpeed", mv.maxSpeed, "movement");
    r.get(m, "acceleration", mv.acceleration, "movement");
    r.get(m, "deceleration", mv.deceleration, "movement");
    r.get(m, "airControlMultiplier", mv.airControlMultiplier, "movement");
    r.get(m, "jumpBaseSpeed", mv.jumpBaseSpeed, "movement");
    r.get(m, "jumpSpeedBonus", mv.jumpSpeedBonus, "movement");
    r.get(m, "jumpSpeedCurveExponent", mv.jumpSpeedCurveExponent, "movement");
    r.get(m, "gravityScale", mv.gravityScale, "movement");
    r.get(m, "fallGravityMultiplier", mv.fallGravityMultiplier, "movement");
    r.get(m, "maxFallSpeed", mv.maxFallSpeed, "movement");
    r.get(m, "coyoteTime", mv.coyoteTime, "movement");
    r.get(m, "jumpBufferTime", mv.jumpBufferTime, "movement");
    r.get(m, "jumpCutMultiplier", mv.jumpCutMultiplier, "movement");
    r.get(m, "groundOverspeedDecay", mv.groundOverspeedDecay, "movement");
    r.get(m, "jumpChargeDuration", mv.jumpChargeDuration, "movement");
    r.get(m, "standStillGrace", mv.standStillGrace, "movement");
    r.get(m, "standStillSpeedThreshold", mv.standStillSpeedThreshold, "movement");
    if (mv.maxSpeed <= 0) r.error("movement.maxSpeed", "must be positive");
    if (mv.gravityScale <= 0) r.error("movement.gravityScale", "must be positive");

    const json& w = root.value("wallBounce", json::object());
    WallBounceConfig& wb = out.wallBounce;
    r.get(w, "speedMultiplier", wb.speedMultiplier, "wallBounce");
    r.get(w, "speedBonus", wb.speedBonus, "wallBounce");
    r.get(w, "maxBounceSpeed", wb.maxBounceSpeed, "wallBounce");
    r.get(w, "minimumBounceSpeed", wb.minimumBounceSpeed, "wallBounce");
    r.get(w, "ascendingVelocityThreshold", wb.ascendingVelocityThreshold, "wallBounce");
    r.get(w, "verticalTransfer", wb.verticalTransfer, "wallBounce");
    r.get(w, "maxVerticalBoost", wb.maxVerticalBoost, "wallBounce");

    const json& c = root.value("camera", json::object());
    r.get(c, "orthographicSize", out.camera.orthographicSize, "camera");
    r.get(c, "verticalOffset", out.camera.verticalOffset, "camera");
    r.get(c, "smoothTime", out.camera.smoothTime, "camera");

    const json& run = root.value("run", json::object());
    r.get(run, "deathZoneOffset", out.run.deathZoneOffset, "run");

    const json& s = root.value("streaming", json::object());
    r.get(s, "spawnAheadDistance", out.streaming.spawnAheadDistance, "streaming");
    r.get(s, "despawnBelowDistance", out.streaming.despawnBelowDistance, "streaming");
    r.get(s, "platformPoolPrewarm", out.streaming.platformPoolPrewarm, "streaming");

    const json& co = root.value("combo", json::object());
    r.get(co, "minimumFloors", out.combo.minimumFloors, "combo");
    r.get(co, "window", out.combo.window, "combo");

    const json& p = root.value("platforms", json::object());
    r.get(p, "thickness", out.platforms.thickness, "platforms");
    r.get(p, "restingTolerance", out.platforms.restingTolerance, "platforms");

    out.platformTypes.clear();
    if (root.contains("platformTypes") && root["platformTypes"].is_array()) {
        int i = 0;
        for (const json& t : root["platformTypes"]) {
            const std::string path = "platformTypes[" + std::to_string(i++) + "]";
            PlatformType pt;
            r.require(t, "typeId", pt.typeId, path);
            r.get(t, "tintHex", pt.tintHex, path);
            r.get(t, "launchSpeed", pt.launchSpeed, path);
            r.get(t, "accelerationMultiplier", pt.accelerationMultiplier, path);
            r.get(t, "decelerationMultiplier", pt.decelerationMultiplier, path);
            r.get(t, "maxSpeedMultiplier", pt.maxSpeedMultiplier, path);
            r.get(t, "landingSpeedRetention", pt.landingSpeedRetention, path);
            if (pt.landingSpeedRetention < 0 || pt.landingSpeedRetention > 1)
                r.error(path + ".landingSpeedRetention", "must be between 0 and 1");
            out.platformTypes.push_back(pt);
        }
    }
    if (out.findType("normal") < 0) r.error("platformTypes", "must define a \"normal\" type");
    return result;
}

LoadResult parseTowerConfig(const std::string& text, TowerConfig& out, const std::string& file) {
    LoadResult result;
    json root = json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        result.errors.push_back(file + ": is not valid JSON");
        return result;
    }
    Reader r(file, result.errors);
    r.require(root, "id", out.id, "tower");
    r.get(root, "displayName", out.displayName, "tower");
    r.require(root, "seed", out.seed, "tower");
    r.require(root, "totalPlatformCount", out.totalPlatformCount, "tower");
    r.get(root, "towerHalfWidth", out.towerHalfWidth, "tower");
    r.get(root, "startHeight", out.startHeight, "tower");
    r.get(root, "fullWidthInterval", out.fullWidthInterval, "tower");

    out.gaps.clear();
    const json& spacing = root.value("spacing", json::object());
    if (spacing.contains("gapByHeight")) {
        for (const json& b : spacing["gapByHeight"])
            out.gaps.push_back({b.value("fromIndex", 0), b.value("toIndex", 0), b.value("minGap", 0.0f), b.value("maxGap", 0.0f)});
    } else {
        // No bands: the top-level gap applies to the whole tower.
        out.gaps.push_back({1, out.totalPlatformCount, spacing.value("minGap", 1.4f), spacing.value("maxGap", 1.5f)});
    }

    out.widths.clear();
    for (const json& b : root.value("widthByHeight", json::array()))
        out.widths.push_back({b.value("fromIndex", 0), b.value("toIndex", 0), b.value("minWidth", 0.0f), b.value("maxWidth", 0.0f)});

    out.scroll.clear();
    for (const json& b : root.value("scrollSpeedByHeight", json::array()))
        out.scroll.push_back({b.value("fromIndex", 0), b.value("toIndex", 0), b.value("unitsPerSecond", 0.0f)});

    out.types.clear();
    for (const json& b : root.value("platformTypesByHeight", json::array())) {
        TypeBand band{b.value("fromIndex", 0), b.value("toIndex", 0), {}};
        for (const json& w : b.value("weights", json::array()))
            band.weights.push_back({w.value("typeId", std::string()), w.value("weight", 0.0f)});
        out.types.push_back(band);
    }
    return result;
}

void validateTower(const TowerConfig& t, const GameplayConfig& g, std::vector<std::string>& errors, const std::string& file) {
    if (t.totalPlatformCount <= 0) {
        errors.push_back(file + ": totalPlatformCount must be positive");
        return;
    }
    checkCoverage(t.gaps, t.totalPlatformCount, "spacing.gapByHeight", file, errors);
    checkCoverage(t.widths, t.totalPlatformCount, "widthByHeight", file, errors);
    checkCoverage(t.scroll, t.totalPlatformCount, "scrollSpeedByHeight", file, errors);
    checkCoverage(t.types, t.totalPlatformCount, "platformTypesByHeight", file, errors);

    for (const WidthBand& b : t.widths) {
        if (b.maxWidth > t.towerHalfWidth * 2.0f)
            errors.push_back(file + ": widthByHeight band [" + std::to_string(b.fromIndex) + ".." + std::to_string(b.toIndex) +
                             "] maxWidth does not fit inside the tower");
        if (b.minWidth <= 0 || b.minWidth > b.maxWidth)
            errors.push_back(file + ": widthByHeight band [" + std::to_string(b.fromIndex) + ".." + std::to_string(b.toIndex) +
                             "] needs 0 < minWidth <= maxWidth");
    }
    for (const GapBand& b : t.gaps)
        if (b.minGap <= 0 || b.minGap > b.maxGap)
            errors.push_back(file + ": gap band [" + std::to_string(b.fromIndex) + ".." + std::to_string(b.toIndex) +
                             "] needs 0 < minGap <= maxGap");
    for (const TypeBand& b : t.types) {
        float total = 0;
        for (const TypeWeight& w : b.weights) {
            if (g.findType(w.typeId) < 0)
                errors.push_back(file + ": platformTypesByHeight uses unknown type \"" + w.typeId + "\"");
            total += w.weight;
        }
        if (total <= 0)
            errors.push_back(file + ": platformTypesByHeight band [" + std::to_string(b.fromIndex) + ".." +
                             std::to_string(b.toIndex) + "] has no positive weight");
    }
}

bool readTextFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

}  // namespace tj
