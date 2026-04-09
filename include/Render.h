#pragma once
#include "Constants.h"
#include <SDL2/SDL.h>
#include <cstdint>
#include <SDL2/SDL_ttf.h>

struct Col { uint8_t r, g, b; };

namespace Pal {
    constexpr Col Floor       = {190, 185, 175};
    constexpr Col WallBase    = { 50,  48,  58};
    constexpr Col WallHi      = {120, 115, 130};
    constexpr Col WallShade   = { 30,  28,  36};
    constexpr Col Outside     = { 20,  20,  30};
    constexpr Col PaddleBase  = {100, 160, 220};
    constexpr Col PaddleHi    = {160, 210, 255};
    constexpr Col PaddleShade = { 50,  90, 140};
}

namespace BrickPal {
    constexpr Col Colors[static_cast<int>(BrickColor::COLOR_COUNT)] = {
        { 20,  20,  30},   // EMPTY
        {220,  60,  60},   // RED
        { 60, 130, 220},   // BLUE
        { 60, 200,  80},   // GREEN
        {220, 200,  50},   // YELLOW
        {230, 130,  40},   // ORANGE
        {160,  60, 200},   // PURPLE
    };
}

struct Map;
struct Paddle;
struct Ball;

struct Render {
  static TTF_Font *font;
  static void setCol(SDL_Renderer *r, Col c, uint8_t a = 255);
  static void drawGrid(SDL_Renderer *r, const Map &map);
  static void drawPaddle(SDL_Renderer *r, const Paddle &p);
  static void drawBall(SDL_Renderer *r, const Ball &b);
  static void drawGameOver(SDL_Renderer *r);
  static void drawScoreboard(SDL_Renderer *r, int score, float totalTime, BrickColor ballColor);
};
