#include "game.hpp"

#include <algorithm>
#include <cmath>

namespace tj {

namespace {

float sign(float v) { return v > 0.0f ? 1.0f : (v < 0.0f ? -1.0f : 0.0f); }

float moveTowards(float current, float target, float maxDelta) {
    if (std::fabs(target - current) <= maxDelta) return target;
    return current + sign(target - current) * maxDelta;
}

}  // namespace

void Game::init(const GameplayConfig& gameplay, const TowerConfig& tower) {
    gameplay_ = gameplay;
    tower_ = tower;
    layout_ = generateTower(tower_, gameplay_);
    reset();
}

void Game::reset() {
    registry_.clear();
    // Pre-size the platform storage so streaming never reallocates during a run.
    registry_.storage<Platform>().reserve(static_cast<size_t>(gameplay_.streaming.platformPoolPrewarm));
    registry_.storage<Transform>().reserve(static_cast<size_t>(gameplay_.streaming.platformPoolPrewarm) + 1);

    stats_ = RunStats{};
    gameOver_ = victory_ = false;
    nextSpawn_ = 0;
    events_.clear();

    const float startY = layout_.empty() ? 0.0f : layout_.front().y;
    player_ = registry_.create();
    registry_.add<Transform>(player_, 0.0f, startY, 0.0f, startY);
    registry_.add<Body>(player_);
    PlayerState& ps = registry_.add<PlayerState>(player_);
    ps.floorIndex = 1;

    cameraY_ = prevCameraY_ = startY + gameplay_.camera.verticalOffset;
    streamPlatforms();

    // Start standing on the first (wall-to-wall) floor.
    if (!layout_.empty()) {
        for (Entity e : registry_.storage<Platform>().entities())
            if (registry_.get<Platform>(e).index == 1) {
                ps.grounded = true;
                ps.ground = e;
            }
    }
}

float Game::jumpSpeedFor(float horizontalSpeed) const {
    const MovementConfig& m = gameplay_.movement;
    // Bounces can push speed above the run cap; allow up to twice the cap to count.
    const float ratio = std::clamp(std::fabs(horizontalSpeed) / m.maxSpeed, 0.0f, 2.0f);
    return m.jumpBaseSpeed + m.jumpSpeedBonus * std::pow(ratio, m.jumpSpeedCurveExponent);
}

void Game::step(const InputState& input, float dt) {
    events_.clear();
    if (gameOver_ || victory_) return;

    stats_.time += dt;
    prevCameraY_ = cameraY_;
    for (Entity e : registry_.storage<Transform>().entities()) {
        Transform& t = registry_.get<Transform>(e);
        t.prevX = t.x;
        t.prevY = t.y;
    }

    movePlayer(input, dt);

    if (stats_.comboJumps > 0) {
        stats_.comboTimer -= dt;
        if (stats_.comboTimer <= 0) endCombo();
    }

    updateCamera(dt);
    streamPlatforms();

    const Transform& pt = registry_.get<Transform>(player_);
    const float bottom = cameraY_ - gameplay_.camera.orthographicSize;
    if (pt.y < bottom - gameplay_.run.deathZoneOffset) {
        endCombo();
        gameOver_ = true;
        events_.push_back({EventType::GameOver, pt.x, pt.y, stats_.score});
    }
}

void Game::movePlayer(const InputState& in, float dt) {
    Transform& t = registry_.get<Transform>(player_);
    Body& b = registry_.get<Body>(player_);
    PlayerState& s = registry_.get<PlayerState>(player_);
    const MovementConfig& m = gameplay_.movement;

    // Lose the ground if the platform streamed out or we walked off its edge.
    if (s.grounded) {
        const Platform* p = registry_.tryGet<Platform>(s.ground);
        const Transform* pt = registry_.tryGet<Transform>(s.ground);
        if (!p || !pt || std::fabs(t.x - pt->x) > p->width * 0.5f + b.halfWidth) {
            s.grounded = false;
            s.ground = kNullEntity;
        }
    }

    const PlatformType* surface = s.grounded ? &gameplay_.platformTypes[static_cast<size_t>(s.groundType)] : nullptr;
    const float maxSpeed = m.maxSpeed * (surface ? surface->maxSpeedMultiplier : 1.0f);
    const float axis = std::clamp(in.axis, -1.0f, 1.0f);
    const bool pushingWithMotion = axis != 0 && sign(axis) == sign(b.vx);

    // ---- horizontal -------------------------------------------------------------------------
    if (s.grounded) {
        if (axis != 0) {
            const float target = axis * maxSpeed;
            if (pushingWithMotion && std::fabs(b.vx) > maxSpeed)
                b.vx = moveTowards(b.vx, target, m.groundOverspeedDecay * dt);  // scrub bounce excess
            else
                b.vx = moveTowards(b.vx, target, m.acceleration * surface->accelerationMultiplier * dt);
        } else {
            b.vx = moveTowards(b.vx, 0.0f, m.deceleration * surface->decelerationMultiplier * dt);
        }
    } else if (axis != 0 && !(pushingWithMotion && std::fabs(b.vx) >= maxSpeed)) {
        // In the air, speed above the cap is preserved: a bounce carries across the gap it earned.
        b.vx = moveTowards(b.vx, axis * maxSpeed, m.acceleration * m.airControlMultiplier * dt);
    }

    // ---- jump charge, coyote time, buffer -----------------------------------------------------
    if (s.jumpChargeTimer > 0 && (s.jumpChargeTimer -= dt) <= 0) s.jumpCharge = 0;
    if (s.grounded && std::fabs(b.vx) < m.standStillSpeedThreshold) {
        if ((s.standStillTimer += dt) >= m.standStillGrace) s.jumpCharge = 0;  // standing still spends it
    } else {
        s.standStillTimer = 0;
    }
    s.coyoteTimer = s.grounded ? m.coyoteTime : s.coyoteTimer - dt;
    s.jumpBufferTimer = in.jumpPressed ? m.jumpBufferTime : s.jumpBufferTimer - dt;

    if (s.jumpBufferTimer > 0 && (s.grounded || s.coyoteTimer > 0)) {
        b.vy = jumpSpeedFor(std::max(std::fabs(b.vx), s.jumpCharge));
        s.grounded = false;
        s.ground = kNullEntity;
        s.coyoteTimer = s.jumpBufferTimer = 0;
        s.jumpCharge = 0;
        events_.push_back({EventType::Jump, t.x, t.y, s.floorIndex - 1});
    }

    // ---- gravity and integration --------------------------------------------------------------
    if (!s.grounded) {
        float g = kGravity * m.gravityScale;
        if (b.vy < 0) g *= m.fallGravityMultiplier;
        b.vy = std::max(b.vy - g * dt, -m.maxFallSpeed);
    } else {
        b.vy = 0;
    }

    const float prevFeet = t.y;
    t.x += b.vx * dt;
    t.y += b.vy * dt;

    // ---- walls --------------------------------------------------------------------------------
    const float limit = tower_.towerHalfWidth - b.halfWidth;
    if (t.x > limit) {
        t.x = limit;
        if (b.vx > 0) hitWall(-1.0f);
    } else if (t.x < -limit) {
        t.x = -limit;
        if (b.vx < 0) hitWall(1.0f);
    }

    // ---- one-way platforms --------------------------------------------------------------------
    // A platform is solid only to a player whose feet were above its surface at the end of the
    // previous step. Jumping up through, touching a side or the underside can never land you.
    if (!s.grounded && b.vy <= 0) {
        const float tol = gameplay_.platforms.restingTolerance;
        Entity best = kNullEntity;
        float bestY = -1e9f;
        registry_.each<Platform, Transform>([&](Entity e, Platform& p, Transform& pt) {
            if (prevFeet >= pt.y - tol && t.y <= pt.y && std::fabs(t.x - pt.x) <= p.width * 0.5f + b.halfWidth &&
                pt.y > bestY) {
                best = e;
                bestY = pt.y;
            }
        });
        if (best != kNullEntity) {
            t.y = bestY;
            landOn(best, axis);
        }
    } else if (s.grounded) {
        t.y = registry_.get<Transform>(s.ground).y;
    }
}

void Game::hitWall(float direction) {
    Transform& t = registry_.get<Transform>(player_);
    Body& b = registry_.get<Body>(player_);
    const PlayerState& s = registry_.get<PlayerState>(player_);
    const WallBounceConfig& w = gameplay_.wallBounce;
    const float speed = std::fabs(b.vx);

    if (s.grounded || speed < w.minimumBounceSpeed) {
        b.vx = 0;
        return;
    }
    b.vx = direction * std::min(speed * w.speedMultiplier + w.speedBonus, w.maxBounceSpeed);
    if (b.vy > w.ascendingVelocityThreshold) b.vy += std::min(speed * w.verticalTransfer, w.maxVerticalBoost);
    events_.push_back({EventType::WallBounce, t.x, t.y, static_cast<int>(speed)});
}

void Game::landOn(Entity e, float axis) {
    Body& b = registry_.get<Body>(player_);
    PlayerState& s = registry_.get<PlayerState>(player_);
    const Platform& p = registry_.get<Platform>(e);
    const Transform& pt = registry_.get<Transform>(e);
    const PlatformType& type = gameplay_.platformTypes[static_cast<size_t>(p.typeIndex)];
    const MovementConfig& m = gameplay_.movement;

    // Bank the arrival speed for the next jump before grip scrubs any of it.
    const float arrival = std::fabs(b.vx);
    s.jumpCharge = std::max(s.jumpCharge, arrival);
    s.jumpChargeTimer = m.jumpChargeDuration;

    // Grip removes drift, never input: speed the player is asking for survives, the rest is
    // multiplied by the surface's landingSpeedRetention (0 = stone, ~1 = ice).
    const float dir = sign(b.vx);
    const float wanted = (axis != 0 && sign(axis) == dir) ? std::min(arrival, m.maxSpeed) : 0.0f;
    b.vx = dir * (wanted + (arrival - wanted) * type.landingSpeedRetention);

    b.vy = 0;
    s.grounded = true;
    s.ground = e;
    s.groundType = p.typeIndex;

    const int gained = p.index - s.floorIndex;
    s.floorIndex = p.index;
    events_.push_back({EventType::Land, pt.x, pt.y, gained});
    updateCombo(gained);

    const int floor = p.index - 1;
    if (floor > stats_.bestFloor) {
        if (p.landmark) events_.push_back({EventType::Milestone, pt.x, pt.y, floor});
        stats_.bestFloor = floor;
    }
    stats_.score = stats_.bestFloor * 10 + stats_.comboBonus;

    if (p.index >= tower_.totalPlatformCount) {
        endCombo();
        victory_ = true;
        events_.push_back({EventType::Victory, pt.x, pt.y, stats_.score});
    }

    if (type.launchSpeed > 0) {  // bouncy platform: throw the player straight back up
        b.vy = type.launchSpeed;
        s.grounded = false;
        s.ground = kNullEntity;
    }
}

void Game::updateCombo(int gained) {
    if (gained >= gameplay_.combo.minimumFloors) {
        stats_.comboJumps += 1;
        stats_.comboFloors += gained;
        stats_.comboTimer = gameplay_.combo.window;
    } else if (gained != 0) {
        endCombo();  // a small hop or a fall breaks the chain; re-landing on the same floor does not
    }
}

void Game::endCombo() {
    if (stats_.comboJumps >= 2) {
        const int points = stats_.comboFloors * stats_.comboFloors;
        stats_.comboBonus += points;
        stats_.bestCombo = std::max(stats_.bestCombo, stats_.comboFloors);
        stats_.score = stats_.bestFloor * 10 + stats_.comboBonus;
        const Transform& t = registry_.get<Transform>(player_);
        events_.push_back({EventType::ComboEnd, t.x, t.y, points});
    }
    stats_.comboJumps = stats_.comboFloors = 0;
    stats_.comboTimer = 0;
}

void Game::updateCamera(float dt) {
    const Transform& t = registry_.get<Transform>(player_);
    const PlayerState& s = registry_.get<PlayerState>(player_);
    const CameraConfig& c = gameplay_.camera;

    // Follow upward only (frame-rate independent exponential smoothing)...
    const float target = t.y + c.verticalOffset;
    if (target > cameraY_) cameraY_ += (target - cameraY_) * (1.0f - std::exp(-dt / std::max(c.smoothTime, 1e-4f)));
    // ...and climb on its own once the tower decides it is time. This is the difficulty curve.
    cameraY_ += tower_.scrollSpeedAt(s.floorIndex) * dt;
}

void Game::streamPlatforms() {
    const float half = gameplay_.camera.orthographicSize;
    const float spawnLine = cameraY_ + half + gameplay_.streaming.spawnAheadDistance;
    while (nextSpawn_ < layout_.size() && layout_[nextSpawn_].y < spawnLine) spawnPlatform(layout_[nextSpawn_++]);

    // Despawn below the death line; ids and component slots are recycled by the registry.
    const float killLine = cameraY_ - half - gameplay_.run.deathZoneOffset - gameplay_.streaming.despawnBelowDistance;
    std::vector<Entity>& doomed = scratch_;
    doomed.clear();
    registry_.each<Platform, Transform>([&](Entity e, Platform&, Transform& pt) {
        if (pt.y < killLine) doomed.push_back(e);
    });
    for (Entity e : doomed) registry_.destroy(e);
}

void Game::spawnPlatform(const PlatformSpec& spec) {
    const Entity e = registry_.create();
    registry_.add<Transform>(e, spec.x, spec.y, spec.x, spec.y);
    registry_.add<Platform>(e, spec.index, spec.width, spec.typeIndex, spec.landmark);
}

}  // namespace tj
