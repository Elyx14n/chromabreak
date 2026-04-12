#pragma once
#include "Constants.h"
#include <SDL2/SDL.h>
#include <cstdint>

class Paddle {
public:
  Paddle();

  void update(float dt, const uint8_t *keys);
  SDL_Rect rect() const;
  float getX() const { return x_; }

private:
  float x_, y_;
};
