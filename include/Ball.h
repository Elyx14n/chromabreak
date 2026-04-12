#pragma once
#include "Constants.h"
#include "Particles.h"
#include <SDL2/SDL.h>
#include <unordered_map>

class Paddle;
class Map;
class Audio;

class Ball {
public:
  Ball();

  void update(float dt, const Paddle &paddle, Map &map, int &score,
              bool &gameOver, ParticleSystem &ps, Audio &audio);
  void handleColor(SDL_Keycode key, Audio &audio);
  SDL_Rect rect() const;

  float getX() const { return x_; }
  float getY() const { return y_; }
  BrickColor getColor() const { return color_; }
  BallPower getPower() const { return power_; }
  float getPowerTimer() const { return powerTimer_; }

private:
  float x_, y_;
  float vx_, vy_;
  BrickColor color_;
  BallPower power_;
  float powerTimer_;

  static inline const std::unordered_map<SDL_Keycode, BrickColor> colorMap_ = {
      {SDLK_q, BrickColor::RED},    {SDLK_w, BrickColor::BLUE},
      {SDLK_e, BrickColor::GREEN},  {SDLK_a, BrickColor::YELLOW},
      {SDLK_s, BrickColor::ORANGE}, {SDLK_d, BrickColor::PURPLE}};

  void cycleColor();
};
