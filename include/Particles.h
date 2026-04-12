#pragma once
#include "Constants.h"
#include <SDL2/SDL.h>

class ParticleSystem {
public:
  ParticleSystem() = default;

  void reset();
  void spawnTrail(float x, float y, float vx, float vy, Col color);
  void spawnBurst(float x, float y, Col color, int n = 16);
  void update(float dt);
  void draw(SDL_Renderer *r) const;

private:
  static constexpr int MAX = 2048;
  static constexpr int MAX_RINGS = 32;

  struct Particle {
    float x, y;
    float vx, vy;
    float life, decay;
    Col color;
    int radius;
    float rgbDrift;
    float perpX, perpY;
    float waveAmp;
    float wavePhase;
    bool isTrail;
  };

  struct BlastRing {
    float x, y;
    float life, decay;
    float radius;
    float maxRadius;
    Col color;
  };

  Particle buf_[MAX];
  int count_ = 0;
  BlastRing rings_[MAX_RINGS];
  int ringCount_ = 0;
};
