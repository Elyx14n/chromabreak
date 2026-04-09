#pragma once
#include <cstdint>
#include <locale>
#include <SDL2/SDL_ttf.h>

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

constexpr float BLOCK_SPAWN_RATE_DEFAULT  = 8.f;
constexpr float BLOCK_SPAWN_RATE_MID      = 6.f;
constexpr float BLOCK_SPAWN_RATE_FAST     = 4.f;
constexpr float BLOCK_SPAWN_RATE_INTERVAL = 180.f;

enum class BrickColor : uint8_t {
    EMPTY = 0,
    RED, BLUE, GREEN, YELLOW, ORANGE, PURPLE,
    COLOR_COUNT
};

static const std::locale LOCALE("");
const char *const FONT_PATH = "assets/font.ttf";
constexpr int FONT_SIZE = 32;