#pragma once
// The whole simulation, with no dependency on rendering, input devices or time sources.
// The caller feeds it an InputState and a fixed dt; the same inputs always produce the same run.
// That is what makes it unit-testable and lets the renderer interpolate between steps.

#include <vector>

#include "config.hpp"
#include "ecs.hpp"
#include "tower.hpp"

namespace tj {

// ---- Components ---------------------------------------------------------------------------

struct Transform {
    float x = 0, y = 0;          // for the player, y is the feet; for platforms, the top surface
    float prevX = 0, prevY = 0;  // previous step, for render interpolation
};

struct Body {
    float vx = 0, vy = 0;
    float halfWidth = 0.3f, height = 0.7f;
};

struct Platform {
    int index = 0;
    float width = 0;
    int typeIndex = 0;
    bool landmark = false;
};

struct PlayerState {
    bool grounded = false;
    Entity ground = kNullEntity;
    int groundType = 0;
    float coyoteTimer = 0;
    float jumpBufferTimer = 0;
    float jumpCharge = 0;       // landing speed banked for the next jump
    float jumpChargeTimer = 0;
    float standStillTimer = 0;
    int floorIndex = 1;         // platform index the player last landed on
};

// ---- Input / output -----------------------------------------------------------------------

struct InputState {
    float axis = 0;            // -1 left, +1 right
    bool jumpHeld = false;
    bool jumpPressed = false;  // true on the step the button went down
};

enum class EventType { Jump, Land, WallBounce, ComboEnd, Milestone, GameOver, Victory };

struct GameEvent {
    EventType type;
    float x = 0, y = 0;
    int value = 0;  // floors for Land/Milestone, points for ComboEnd
};

struct RunStats {
    int bestFloor = 0;      // highest floor reached (index - 1, as shown to the player)
    int score = 0;
    int comboBonus = 0;
    int bestCombo = 0;      // floors in the best finished combo
    int comboJumps = 0;     // current combo
    int comboFloors = 0;
    float comboTimer = 0;   // time left to extend the current combo
    float time = 0;
};

// ---- Game ---------------------------------------------------------------------------------

class Game {
public:
    static constexpr float kGravity = 9.81f;  // matches Unity's default, scaled by gravityScale

    void init(const GameplayConfig& gameplay, const TowerConfig& tower);
    void reset();
    void step(const InputState& input, float dt);

    // Events produced by the last step(); the renderer uses them for effects.
    const std::vector<GameEvent>& events() const { return events_; }

    Registry& registry() { return registry_; }
    Entity player() const { return player_; }
    const RunStats& stats() const { return stats_; }
    const GameplayConfig& gameplay() const { return gameplay_; }
    const TowerConfig& tower() const { return tower_; }
    const std::vector<PlatformSpec>& layout() const { return layout_; }

    float cameraY() const { return cameraY_; }
    float prevCameraY() const { return prevCameraY_; }
    bool gameOver() const { return gameOver_; }
    bool victory() const { return victory_; }

    // Jump take-off speed for a given horizontal speed (exposed for tests and tuning tools).
    float jumpSpeedFor(float horizontalSpeed) const;

private:
    void movePlayer(const InputState& input, float dt);
    void landOn(Entity platform, float inputAxis);
    void hitWall(float direction);
    void updateCombo(int floorsGained);
    void endCombo();
    void updateCamera(float dt);
    void streamPlatforms();
    void spawnPlatform(const PlatformSpec& spec);

    GameplayConfig gameplay_;
    TowerConfig tower_;
    std::vector<PlatformSpec> layout_;

    Registry registry_;
    Entity player_ = kNullEntity;
    size_t nextSpawn_ = 0;

    RunStats stats_;
    float cameraY_ = 0, prevCameraY_ = 0;
    bool gameOver_ = false, victory_ = false;
    std::vector<GameEvent> events_;
    std::vector<Entity> scratch_;  // reused buffer, avoids a per-step allocation
};

}  // namespace tj
