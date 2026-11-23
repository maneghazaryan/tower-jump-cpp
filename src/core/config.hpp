#pragma once
// Game configuration, loaded from the same JSON files the Unity version of Tower Jump ships with.
// Only the fields this port uses are read; everything is validated on load and every problem is
// reported (file + field), instead of failing on the first one.

#include <string>
#include <vector>

namespace tj {

struct MovementConfig {
    float maxSpeed = 10.5f;
    float acceleration = 26.0f;
    float deceleration = 35.0f;
    float airControlMultiplier = 0.65f;
    float jumpBaseSpeed = 12.9f;
    float jumpSpeedBonus = 11.0f;
    float jumpSpeedCurveExponent = 1.35f;
    float gravityScale = 4.5f;
    float fallGravityMultiplier = 1.35f;
    float maxFallSpeed = 30.0f;
    float coyoteTime = 0.12f;
    float jumpBufferTime = 0.15f;
    float jumpCutMultiplier = 0.5f;
    float groundOverspeedDecay = 40.0f;
    float jumpChargeDuration = 0.6f;
    float standStillGrace = 0.25f;
    float standStillSpeedThreshold = 0.4f;
};

struct WallBounceConfig {
    float speedMultiplier = 0.82f;
    float speedBonus = 1.0f;
    float maxBounceSpeed = 21.0f;
    float minimumBounceSpeed = 1.5f;
    float ascendingVelocityThreshold = 0.5f;
    float verticalTransfer = 0.45f;
    float maxVerticalBoost = 10.5f;
};

struct CameraConfig {
    float orthographicSize = 7.0f;  // half of the visible height, world units
    float verticalOffset = 2.0f;
    float smoothTime = 0.12f;
};

struct RunConfig {
    float deathZoneOffset = 2.0f;
};

struct StreamingConfig {
    float spawnAheadDistance = 12.0f;
    float despawnBelowDistance = 1.0f;
    int platformPoolPrewarm = 32;
};

struct ComboConfig {
    int minimumFloors = 2;
    float window = 3.0f;
};

struct PlatformPhysicsConfig {
    float thickness = 0.26f;
    float restingTolerance = 0.08f;
};

struct PlatformType {
    std::string typeId;
    std::string tintHex;
    float launchSpeed = 0.0f;
    float accelerationMultiplier = 1.0f;
    float decelerationMultiplier = 1.0f;
    float maxSpeedMultiplier = 1.0f;
    float landingSpeedRetention = 0.0f;
};

struct GameplayConfig {
    MovementConfig movement;
    WallBounceConfig wallBounce;
    CameraConfig camera;
    RunConfig run;
    StreamingConfig streaming;
    ComboConfig combo;
    PlatformPhysicsConfig platforms;
    std::vector<PlatformType> platformTypes;

    // Index into platformTypes, or -1.
    int findType(const std::string& id) const;
};

// A band covers platform indices [fromIndex, toIndex], 1-based and inclusive.
struct GapBand    { int fromIndex, toIndex; float minGap, maxGap; };
struct WidthBand  { int fromIndex, toIndex; float minWidth, maxWidth; };
struct ScrollBand { int fromIndex, toIndex; float unitsPerSecond; };
struct TypeWeight { std::string typeId; float weight; };
struct TypeBand   { int fromIndex, toIndex; std::vector<TypeWeight> weights; };

struct TowerConfig {
    std::string id;
    std::string displayName;
    unsigned long long seed = 0;
    int totalPlatformCount = 0;
    float towerHalfWidth = 4.5f;
    float startHeight = -3.0f;
    int fullWidthInterval = 100;
    std::vector<GapBand> gaps;
    std::vector<WidthBand> widths;
    std::vector<ScrollBand> scroll;
    std::vector<TypeBand> types;

    // Camera auto-scroll speed while the player is around platform `index`.
    float scrollSpeedAt(int index) const;
};

struct LoadResult {
    bool ok() const { return errors.empty(); }
    std::vector<std::string> errors;
};

LoadResult parseGameplayConfig(const std::string& jsonText, GameplayConfig& out, const std::string& fileName = "gameplay.json");
LoadResult parseTowerConfig(const std::string& jsonText, TowerConfig& out, const std::string& fileName = "tower.json");

// Checks that every band list covers 1..totalPlatformCount exactly, with no gaps or overlaps.
// Appends a precise message per problem.
void validateTower(const TowerConfig& tower, const GameplayConfig& gameplay, std::vector<std::string>& errors,
                   const std::string& fileName = "tower.json");

bool readTextFile(const std::string& path, std::string& out);

}  // namespace tj
