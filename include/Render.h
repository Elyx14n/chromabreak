#pragma once
#include "Constants.h"
#include "Particles.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

class Map;
class Paddle;
class Ball;

class Render {
public:
  explicit Render(SDL_Renderer *r);
  ~Render();

  Render(const Render &) = delete;
  Render &operator=(const Render &) = delete;

  void drawGrid(const Map &map);
  void drawPaddle(const Paddle &p);
  void drawBall(const Ball &b, float time);
  void drawParticles(const ParticleSystem &ps);
  void drawGameOver();
  void drawScoreboard(int score, float totalTime, const Ball &b,
                      float reverserTimer);

private:
  SDL_Renderer *r_;
  TTF_Font *font_;

  void setCol(Col c, uint8_t a = 255);
};
