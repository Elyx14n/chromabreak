#pragma once
#include "Constants.h"
#include "Particles.h"
#include <SDL2/SDL.h>
#include <cstdint>

struct Map {
  BrickColor cells[ROWS][COLS];
  BrickType types[ROWS][COLS];
  float spawnTimer;
  float totalTime;
  float reverserTimer; // > 0 while reverse-shift mode is active

  void init();
  // score and ps needed so reversed-mode can destroy bricks pushed off the top.
  void update(float dt, bool &gameOver, int &score, ParticleSystem &ps);

  void floodFill(int r, int c, BrickColor color, int &score,
                 ParticleSystem &ps);

  // Special-brick effects — called from Ball after physics separation.
  void bombEffect(int r, int c, BrickColor color, int &score,
                  ParticleSystem &ps);
  void transformerEffect(int r, int c, BrickColor color, int &score,
                         ParticleSystem &ps);
  void reverserEffect(int r, int c, int &score, ParticleSystem &ps);

  float currentSpawnRate() const;

  SDL_Rect cellRect(int r, int c) const {
    return {c * TILE, TOP_MARGIN + r * TILE, TILE, TILE};
  }

private:
  void spawnRow(BrickColor *out, BrickType *outTypes) const;
  BrickType spawnBrickType(BrickColor bc) const;
};
