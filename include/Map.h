#pragma once
#include "Constants.h"
#include "Particles.h"
#include "Audio.h"
#include <SDL2/SDL.h>
#include <cstdint>

class Map {
public:
  void init();
  void update(float dt, bool &gameOver, int &score, ParticleSystem &ps, Audio &a);

  int floodFill(int r, int c, BrickColor color, int &score,
                 ParticleSystem &ps);
  void bombEffect(int r, int c, BrickColor color, int &score,
                  ParticleSystem &ps);
  void transformerEffect(int r, int c, BrickColor color, int &score,
                         ParticleSystem &ps);
  void reverserEffect(int r, int c, int &score, ParticleSystem &ps);

  SDL_Rect cellRect(int r, int c) const {
    return {c * TILE, TOP_MARGIN + r * TILE, TILE, TILE};
  }

  BrickColor colorAt(int r, int c) const { return cells_[r][c]; }
  BrickType typeAt(int r, int c) const { return types_[r][c]; }
  void clearCell(int r, int c) {
    cells_[r][c] = BrickColor::EMPTY;
    types_[r][c] = BrickType::NORMAL;
  }

  float getTotalTime() const { return totalTime_; }
  float getReverserTimer() const { return reverserTimer_; }

private:
  BrickColor cells_[ROWS][COLS];
  BrickType types_[ROWS][COLS];
  float spawnTimer_ = 0.f;
  float totalTime_ = 0.f;
  float reverserTimer_ = 0.f;

  float currentSpawnRate() const;
  void spawnRow(BrickColor *out, BrickType *outTypes) const;
  BrickType spawnBrickType(BrickColor bc) const;
};
