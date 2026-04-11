#pragma once
#include <cstdint>
#include <locale>

constexpr int TILE = 48;
constexpr int COLS = 12;
constexpr int ROWS = 12;
constexpr int WIN_W = TILE * COLS;
constexpr int TOP_MARGIN = 60;
constexpr int BOTTOM_MARGIN = 120;
constexpr int WIN_H = TOP_MARGIN + (ROWS * TILE) + BOTTOM_MARGIN;

constexpr float PADDLE_X_SPEED = 400.f;
constexpr float PADDLE_Y_SPEED = 380.f;
constexpr float BALL_SPEED = 420.f;
constexpr int PADDLE_W = TILE * 3;
constexpr int PADDLE_H = 14;
constexpr int PADDLE_Y = WIN_H - 70;
constexpr int PADDLE_MIN_Y = WIN_H - BOTTOM_MARGIN;
constexpr int PADDLE_MAX_Y = WIN_H;

constexpr int BALL_R = 12;

constexpr float BLOCK_SPAWN_RATE_DEFAULT = 8.f;
constexpr float BLOCK_SPAWN_RATE_MID = 6.f;
constexpr float BLOCK_SPAWN_RATE_FAST = 4.f;
constexpr float BLOCK_SPAWN_RATE_INTERVAL = 180.f;

constexpr float POWER_DURATION = 12.f;
constexpr int TRANSFORMER_RADIUS = 4;
constexpr int BOMB_RADIUS = 2;
constexpr int SPECIAL_BRICK_CHANCE = 6;
constexpr int BASE_SCORE = 100;

enum class BrickColor : uint8_t {
  EMPTY = 0,
  RED,
  BLUE,
  GREEN,
  YELLOW,
  ORANGE,
  PURPLE,
  COLOR_COUNT
};

enum class BrickType : uint8_t {
  NORMAL = 0,
  BOMB,        // destroys all bricks of same color across the grid
  TRANSFORMER, // converts all bricks within radius to this color
  RAINBOW,     // gives ball rainbow power (breaks any color)
  REVERSER,    // reverses brick row movement (breaking bricks pushed to top)
};

enum class BallPower : uint8_t {
  NONE = 0,
  RAINBOW,
};

struct Col {
  uint8_t r, g, b;
};

namespace Pal {
constexpr Col Floor       = { 35,  30,  45};  // dark purple obsidian
constexpr Col WallBase    = { 45,  38,  58};  // slightly lifted obsidian for HUD bar
constexpr Col WallHi      = {200,  80, 140};  // vivid pink — divider line + HUD border
constexpr Col WallShade   = { 80,  45,  80};  // muted mauve — empty-cell grid lines
constexpr Col Outside     = { 20,  16,  28};  // near-black purple for letterbox
constexpr Col PaddleBase = {255, 50, 150};
constexpr Col PaddleHi = {255, 200, 235};
constexpr Col PaddleShade = {120, 20, 80};
} // namespace Pal

namespace BrickPal {
constexpr Col Colors[static_cast<int>(BrickColor::COLOR_COUNT)] = {
    {20, 20, 30},   // EMPTY
    {220, 60, 60},  // RED
    {60, 130, 220}, // BLUE
    {60, 200, 80},  // GREEN
    {220, 200, 50}, // YELLOW
    {230, 130, 40}, // ORANGE
    {160, 60, 200}, // PURPLE
};
}

// Special-brick colors
namespace SpecialPal {
constexpr Col Base = {240, 245, 255}; // Platinum
constexpr Col Bomb = {255, 80, 40};  // border
constexpr Col Transformer = {200, 80, 255}; // border
constexpr Col Rainbow = {255, 255, 200};    // border
constexpr Col Reverser = {60, 230, 255};    // border
}

static const std::locale LOCALE("");
const char *const FONT_PATH = "assets/font.ttf";
constexpr int FONT_SIZE = 32;
