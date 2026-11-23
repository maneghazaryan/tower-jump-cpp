#include "app.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "raylib.h"

namespace tj {

namespace {

constexpr int kWidth = 540;
constexpr int kHeight = 900;

Color lerpColor(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return Color{static_cast<unsigned char>(a.r + (b.r - a.r) * t), static_cast<unsigned char>(a.g + (b.g - a.g) * t),
                 static_cast<unsigned char>(a.b + (b.b - a.b) * t), static_cast<unsigned char>(a.a + (b.a - a.a) * t)};
}

Color hexColor(const std::string& hex, Color fallback) {
    if (hex.size() != 7 || hex[0] != '#') return fallback;
    unsigned int v = 0;
    if (std::sscanf(hex.c_str() + 1, "%x", &v) != 1) return fallback;
    return Color{static_cast<unsigned char>(v >> 16), static_cast<unsigned char>(v >> 8), static_cast<unsigned char>(v), 255};
}

// Theme per hundred floors, like the Unity version's moss / stone / sand / obsidian bands.
struct Theme {
    Color skyTop, skyBottom, platform, platformEdge, wall;
};
const Theme kThemes[] = {
    {{24, 44, 38, 255}, {54, 92, 70, 255}, {104, 164, 92, 255}, {60, 104, 56, 255}, {44, 64, 52, 255}},     // moss
    {{30, 32, 44, 255}, {72, 76, 96, 255}, {150, 150, 162, 255}, {92, 92, 108, 255}, {52, 54, 68, 255}},    // stone
    {{60, 40, 26, 255}, {150, 104, 60, 255}, {222, 184, 118, 255}, {160, 120, 70, 255}, {96, 66, 40, 255}}, // sand
    {{12, 8, 20, 255}, {44, 24, 60, 255}, {90, 70, 120, 255}, {50, 36, 76, 255}, {26, 18, 38, 255}},        // obsidian
};
const Theme& themeFor(int floor) { return kThemes[std::clamp(floor / 100, 0, 3)]; }

}  // namespace

bool App::init(const std::string& root) {
    GameplayConfig gameplay;
    TowerConfig tower;
    std::string text;

    const std::string gp = root + "config/gameplay.json";
    const std::string tw = root + "config/tower_00.json";
    if (!readTextFile(gp, text)) errors_.push_back("cannot read " + gp);
    else for (auto& e : parseGameplayConfig(text, gameplay, "gameplay.json").errors) errors_.push_back(e);
    if (!readTextFile(tw, text)) errors_.push_back("cannot read " + tw);
    else for (auto& e : parseTowerConfig(text, tower, "tower_00.json").errors) errors_.push_back(e);
    if (errors_.empty()) validateTower(tower, gameplay, errors_, "tower_00.json");

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(kWidth, kHeight, "Tower Jump - C++ edition");
    SetTargetFPS(0);  // vsync paces us; the fixed timestep keeps physics independent of it

    if (!errors_.empty()) return false;
    game_.init(gameplay, tower);
    return true;
}

bool App::shouldClose() const { return WindowShouldClose(); }

InputState App::pollInput() {
    InputState in;
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) in.axis -= 1.0f;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) in.axis += 1.0f;
    bool jump = IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_UP) || IsKeyDown(KEY_W);

    // Touch: three pads along the bottom edge (left, right, jump), as in the Unity build.
    const int touches = GetTouchPointCount();
    if (touches > 0) touchSeen_ = true;
    for (int i = 0; i < touches; ++i) {
        const Vector2 p = GetTouchPosition(i);
        const float w = static_cast<float>(GetScreenWidth());
        if (p.y < GetScreenHeight() * 0.55f) continue;
        if (p.x < w / 3) in.axis -= 1.0f;
        else if (p.x < 2 * w / 3) in.axis += 1.0f;
        else jump = true;
    }
    in.axis = std::clamp(in.axis, -1.0f, 1.0f);
    in.jumpHeld = jump;
    in.jumpPressed = jump && !jumpWasHeld_;
    jumpWasHeld_ = jump;
    return in;
}

void App::frame() {
    const float dt = GetFrameTime();
    float alpha = 1.0f;
    if (!errors_.empty()) {
        draw(alpha);
        return;
    }

    if (screen_ == Screen::Playing) {
        InputState in = pollInput();
        alpha = clock_.advance(dt, [&](float step) {
            game_.step(in, step);
            in.jumpPressed = false;  // an edge belongs to the first sub-step only
            handleEvents();
        });
        if (game_.gameOver() || game_.victory()) {
            screen_ = Screen::Over;
            overTimer_ = 0;
            bestScore_ = std::max(bestScore_, game_.stats().score);
            bestFloor_ = std::max(bestFloor_, game_.stats().bestFloor);
        }
    } else {
        const InputState in = pollInput();
        overTimer_ += dt;
        const bool go = in.jumpPressed || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_R) ||
                        IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        if (go && (screen_ == Screen::Title || overTimer_ > 0.6f)) {
            if (screen_ == Screen::Over) game_.reset();
            screen_ = Screen::Playing;
            clock_.reset();
            popups_.clear();
            jumpWasHeld_ = true;  // the key that started the run is not a jump
        }
    }

    landSquash_ = std::max(0.0f, landSquash_ - dt * 6.0f);
    shake_ = std::max(0.0f, shake_ - dt * 3.0f);
    for (Popup& p : popups_) p.age += dt;
    popups_.erase(std::remove_if(popups_.begin(), popups_.end(), [](const Popup& p) { return p.age > 1.2f; }), popups_.end());

    draw(alpha);
}

void App::handleEvents() {
    const RunStats& st = game_.stats();
    for (const GameEvent& e : game_.events()) {
        switch (e.type) {
            case EventType::Land:
                landSquash_ = 1.0f;
                spin_ = 0;
                break;
            case EventType::WallBounce:
                shake_ = std::max(shake_, 0.25f);
                break;
            case EventType::ComboEnd:
                popups_.push_back({"COMBO +" + std::to_string(e.value), e.x, e.y + 1.5f, 0, 1});
                shake_ = 0.6f;
                break;
            case EventType::Milestone:
                popups_.push_back({"FLOOR " + std::to_string(e.value) + "!", 0, e.y + 2.0f, 0, 2});
                break;
            default:
                break;
        }
    }
    // Spin while a combo is being chained in the air (the Icy Tower salto).
    const PlayerState& ps = game_.registry().get<PlayerState>(game_.player());
    if (st.comboJumps > 0 && !ps.grounded) spin_ += 6.0f;
}

void App::draw(float alpha) {
    BeginDrawing();
    ClearBackground(BLACK);
    if (!errors_.empty()) {
        DrawText("Config errors:", 20, 20, 24, RED);
        int y = 56;
        for (const std::string& e : errors_) {
            DrawText(e.c_str(), 20, y, 16, RAYWHITE);
            y += 22;
        }
        EndDrawing();
        return;
    }

    drawWorld(alpha);
    drawHud();
    if (touchSeen_ && screen_ == Screen::Playing) drawTouchPads();

    if (screen_ == Screen::Title) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.55f));
        drawCentered("TOWER JUMP", GetScreenHeight() / 3, 56, 255);
        drawCentered("C++ edition", GetScreenHeight() / 3 + 64, 24, 200);
        drawCentered("A/D or arrows: run     Space / W / Up: jump", GetScreenHeight() / 2 + 20, 18, 220);
        drawCentered("Run faster, jump higher. Bounce off the walls.", GetScreenHeight() / 2 + 48, 18, 220);
        drawCentered("Press Space or tap to start", GetScreenHeight() / 2 + 110, 24,
                     static_cast<int>(160 + 95 * std::sin(GetTime() * 4)));
    } else if (screen_ == Screen::Over) {
        const RunStats& st = game_.stats();
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.6f));
        drawCentered(game_.victory() ? "YOU REACHED THE TOP!" : "GAME OVER", GetScreenHeight() / 3, 44, 255);
        char line[128];
        std::snprintf(line, sizeof line, "Score %d     Floor %d     Best combo %d", st.score, st.bestFloor, st.bestCombo);
        drawCentered(line, GetScreenHeight() / 3 + 70, 20, 230);
        std::snprintf(line, sizeof line, "Session best: score %d, floor %d", bestScore_, bestFloor_);
        drawCentered(line, GetScreenHeight() / 3 + 100, 18, 180);
        if (overTimer_ > 0.6f)
            drawCentered("Press Space / R or tap to play again", GetScreenHeight() / 2 + 60, 22,
                         static_cast<int>(160 + 95 * std::sin(GetTime() * 4)));
    }
    EndDrawing();
}

void App::drawWorld(float alpha) {
    const GameplayConfig& g = game_.gameplay();
    const float sw = static_cast<float>(GetScreenWidth());
    const float sh = static_cast<float>(GetScreenHeight());
    const float ppu = sh / (2.0f * g.camera.orthographicSize);  // pixels per world unit
    const float camY = game_.prevCameraY() + (game_.cameraY() - game_.prevCameraY()) * alpha;
    const float shakeX = shake_ > 0 ? std::sin(GetTime() * 90.0) * shake_ * 4.0f : 0.0f;

    auto toScreenX = [&](float x) { return sw * 0.5f + x * ppu + shakeX; };
    auto toScreenY = [&](float y) { return sh * 0.5f - (y - camY) * ppu; };

    const int floorAtCamera = std::max(0, game_.registry().get<PlayerState>(game_.player()).floorIndex - 1);
    const Theme& theme = themeFor(floorAtCamera);
    DrawRectangleGradientV(0, 0, static_cast<int>(sw), static_cast<int>(sh), theme.skyTop, theme.skyBottom);

    // Parallax bricks on the back wall.
    const float halfW = game_.tower().towerHalfWidth;
    const float left = toScreenX(-halfW), right = toScreenX(halfW);
    const float brickH = 0.6f * ppu, brickW = 1.2f * ppu;
    const float offset = std::fmod(camY * ppu * 0.5f, brickH * 2);
    for (float y = -brickH * 2 + offset; y < sh + brickH; y += brickH) {
        const int row = static_cast<int>(std::floor((y - offset) / brickH));
        const float shift = (row & 1) ? brickW * 0.5f : 0.0f;
        for (float x = left - brickW + shift; x < right; x += brickW)
            DrawRectangleLinesEx({x, y, brickW, brickH}, 1.0f, Fade(WHITE, 0.04f));
    }

    // Side walls.
    DrawRectangle(0, 0, static_cast<int>(left), static_cast<int>(sh), theme.wall);
    DrawRectangle(static_cast<int>(right), 0, static_cast<int>(sw - right) + 1, static_cast<int>(sh), theme.wall);
    DrawRectangle(static_cast<int>(left) - 4, 0, 4, static_cast<int>(sh), Fade(BLACK, 0.35f));
    DrawRectangle(static_cast<int>(right), 0, 4, static_cast<int>(sh), Fade(BLACK, 0.35f));

    // Platforms.
    Registry& reg = game_.registry();
    const float thick = std::max(g.platforms.thickness, 0.22f) * ppu;
    reg.each<Platform, Transform>([&](Entity, Platform& p, Transform& t) {
        const Theme& th = themeFor(p.index - 1);
        const PlatformType& type = g.platformTypes[static_cast<size_t>(p.typeIndex)];
        Color fill = th.platform, edge = th.platformEdge;
        if (type.typeId == "ice") {
            fill = hexColor(type.tintHex, Color{159, 223, 255, 255});
            edge = Color{90, 160, 210, 255};
        } else if (type.typeId == "bouncy") {
            fill = Color{255, 150, 60, 255};
            edge = Color{190, 90, 30, 255};
        }
        const Rectangle r{toScreenX(t.x - p.width * 0.5f), toScreenY(t.y), p.width * ppu, thick};
        if (r.y > sh + 20 || r.y < -40) return;
        DrawRectangleRounded(r, 0.5f, 6, fill);
        DrawRectangleRoundedLinesEx(r, 0.5f, 6, 2.0f, edge);
        if (type.typeId == "bouncy")
            for (float x = r.x + 10; x < r.x + r.width - 6; x += 14) DrawLineEx({x, r.y + 3}, {x + 6, r.y + thick - 3}, 2, edge);
        if ((p.index - 1) % 10 == 0 && p.index > 1) {
            const std::string label = std::to_string(p.index - 1);
            const int tw = MeasureText(label.c_str(), 18);
            DrawText(label.c_str(), static_cast<int>(r.x + r.width * 0.5f - tw * 0.5f), static_cast<int>(r.y + thick + 4), 18,
                     Fade(WHITE, 0.7f));
        }
    });

    // Player (interpolated between the last two simulation steps).
    const Transform& pt = reg.get<Transform>(game_.player());
    const Body& body = reg.get<Body>(game_.player());
    const float px = pt.prevX + (pt.x - pt.prevX) * alpha;
    const float py = pt.prevY + (pt.y - pt.prevY) * alpha;
    const float squash = landSquash_ * 0.25f;
    const float w = body.halfWidth * 2 * ppu * (1 + squash), h = body.height * ppu * (1 - squash);
    const Vector2 feet{toScreenX(px), toScreenY(py)};
    const Rectangle bodyRect{feet.x, feet.y - h * 0.5f, w, h};
    DrawRectanglePro(bodyRect, {w * 0.5f, h * 0.5f}, spin_, Color{240, 90, 90, 255});
    if (std::fmod(spin_, 360.0f) == 0.0f) {
        const float look = body.vx > 0.5f ? 1.0f : (body.vx < -0.5f ? -1.0f : 0.0f);
        const float eyeY = feet.y - h * 0.68f;
        DrawCircleV({feet.x - w * 0.18f + look * w * 0.1f, eyeY}, w * 0.12f, WHITE);
        DrawCircleV({feet.x + w * 0.18f + look * w * 0.1f, eyeY}, w * 0.12f, WHITE);
        DrawCircleV({feet.x - w * 0.18f + look * w * 0.16f, eyeY}, w * 0.06f, BLACK);
        DrawCircleV({feet.x + w * 0.18f + look * w * 0.16f, eyeY}, w * 0.06f, BLACK);
    }

    // Floating texts.
    for (const Popup& p : popups_) {
        const float a = 1.0f - p.age / 1.2f;
        const Color c = p.color == 1 ? GOLD : (p.color == 2 ? SKYBLUE : RAYWHITE);
        const int size = 30;
        const int tw = MeasureText(p.text.c_str(), size);
        const float x = std::clamp(toScreenX(p.x) - tw * 0.5f, 8.0f, sw - tw - 8.0f);
        DrawText(p.text.c_str(), static_cast<int>(x), static_cast<int>(toScreenY(p.y) - p.age * 40), size, Fade(c, a));
    }
}

void App::drawHud() {
    const RunStats& st = game_.stats();
    char buf[64];
    DrawRectangle(0, 0, GetScreenWidth(), 44, Fade(BLACK, 0.45f));
    std::snprintf(buf, sizeof buf, "FLOOR %d", st.bestFloor);
    DrawText(buf, 14, 10, 24, RAYWHITE);
    std::snprintf(buf, sizeof buf, "SCORE %d", st.score);
    const int tw = MeasureText(buf, 24);
    DrawText(buf, GetScreenWidth() - tw - 14, 10, 24, GOLD);

    // Combo meter: floors in the chain, and the time left to extend it.
    if (st.comboJumps > 0) {
        const float frac = std::clamp(st.comboTimer / game_.gameplay().combo.window, 0.0f, 1.0f);
        const int x = 14, y = 54, w = 150, h = 10;
        DrawRectangle(x, y, w, h, Fade(BLACK, 0.5f));
        DrawRectangle(x, y, static_cast<int>(w * frac), h, lerpColor(RED, GOLD, frac));
        std::snprintf(buf, sizeof buf, "COMBO x%d  (%d floors)", st.comboJumps, st.comboFloors);
        DrawText(buf, x, y + 14, 18, GOLD);
    }
}

void App::drawTouchPads() {
    const float sw = static_cast<float>(GetScreenWidth()), sh = static_cast<float>(GetScreenHeight());
    const float size = std::clamp(sw * 0.24f, 90.0f, 160.0f);
    const char* labels[3] = {"<", ">", "JUMP"};
    for (int i = 0; i < 3; ++i) {
        const float cx = sw * (i * 2 + 1) / 6.0f;
        DrawCircleV({cx, sh - size * 0.75f}, size * 0.42f, Fade(WHITE, 0.15f));
        const int tw = MeasureText(labels[i], 26);
        DrawText(labels[i], static_cast<int>(cx - tw * 0.5f), static_cast<int>(sh - size * 0.75f - 13), 26, Fade(WHITE, 0.6f));
    }
}

void App::drawCentered(const char* text, int y, int size, int alpha255) {
    const int tw = MeasureText(text, size);
    DrawText(text, (GetScreenWidth() - tw) / 2, y, size, Fade(RAYWHITE, std::clamp(alpha255, 0, 255) / 255.0f));
}

}  // namespace tj
