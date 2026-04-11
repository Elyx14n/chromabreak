#pragma once
#include "Constants.h"
#include "Particles.h"
#include <SDL2/SDL.h>

struct Paddle;
struct Map;

struct Ball {
  float x, y;
  float vx, vy;
  BrickColor color;
  BallPower power;
  float powerTimer;

  Ball();

  void update(float dt, const Paddle &paddle, Map &map, int &score,
              bool &gameOver, ParticleSystem &ps);

  void cycleColor();
  void handleColor(SDL_Keycode key);
  SDL_Rect rect() const;
};
