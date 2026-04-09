#pragma once
#include "Constants.h"
#include <SDL2/SDL.h>
#include <cstdint>

struct Paddle {
  float x, y;

  Paddle() : x(WIN_W / 2), y(PADDLE_Y) {}

  void update(float dt, const uint8_t *keys);
  SDL_Rect rect() const;
}; 