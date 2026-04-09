#pragma once
#include "Constants.h"
#include <SDL2/SDL.h>

struct Paddle;
struct Map;

struct Ball {
  float x, y;
  float vx, vy;
  BrickColor color;

  Ball();

  void update(float dt, const Paddle &paddle, Map &map, int &score,
              bool &gameOver);

  void cycleColor();
  SDL_Rect rect() const;
};
