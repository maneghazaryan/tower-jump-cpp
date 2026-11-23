#pragma once
// The raylib front end: window, input devices, drawing. All game rules live in tj::Game.

#include <string>
#include <vector>

#include "../core/fixed_timestep.hpp"
#include "../core/game.hpp"

namespace tj {

class App {
public:
    bool init(const std::string& assetRoot);
    void frame();  // one display frame: input, fixed-step simulation, render
    bool shouldClose() const;
    const std::vector<std::string>& errors() const { return errors_; }

private:
    enum class Screen { Title, Playing, Over };

    struct Popup {
        std::string text;
        float x, y, age;
        int color;  // 0 white, 1 gold, 2 cyan
    };

    InputState pollInput();
    void handleEvents();
    void draw(float alpha);
    void drawWorld(float alpha);
    void drawHud();
    void drawTouchPads();
    void drawCentered(const char* text, int y, int size, int alpha255);

    Game game_;
    FixedTimestep clock_{1.0f / 120.0f};
    Screen screen_ = Screen::Title;
    std::vector<std::string> errors_;
    std::vector<Popup> popups_;

    bool jumpWasHeld_ = false;
    bool touchSeen_ = false;
    float landSquash_ = 0;     // 0..1, short squash after landings
    float spin_ = 0;           // degrees, while chaining a combo in the air
    float shake_ = 0;
    float overTimer_ = 0;
    int bestScore_ = 0;
    int bestFloor_ = 0;
};

}  // namespace tj
