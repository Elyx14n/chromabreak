#pragma once
#include "Constants.h"
#include "Particles.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

struct Map;
struct Paddle;
struct Ball;

struct Render {
  static TTF_Font *font;
  static void setCol(SDL_Renderer *r, Col c, uint8_t a = 255);
  static void drawGrid(SDL_Renderer *r, const Map &map);
  static void drawPaddle(SDL_Renderer *r, const Paddle &p);
  static void drawBall(SDL_Renderer *r, const Ball &b, float time);
  static void drawParticles(SDL_Renderer *r, const ParticleSystem &ps);
  static void drawGameOver(SDL_Renderer *r);
  static void drawScoreboard(SDL_Renderer *r, int score, float totalTime,
                             const Ball &b, float reverserTimer);
};
