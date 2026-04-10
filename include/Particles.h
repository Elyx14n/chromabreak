#pragma once
#include "Constants.h"
#include <SDL2/SDL.h>

struct Particle {
  float x, y;
  float vx, vy;
  float life, decay;
  Col color;
  int radius; // base radius; actual drawn = radius * life (tapers to a point)
  float
      rgbDrift; // amplitude of per-channel sinusoidal hue shift as life drains
  float perpX, perpY; // normalised perpendicular to spawn velocity (wave axis)
  float waveAmp;      // transverse wave amplitude in pixels
  float wavePhase;    // per-particle phase offset so neighbours are staggered
  bool isTrail; // true = soft small flame dot; false = full bloom burst orb
};

struct BlastRing {
  float x, y;
  float life, decay;
  float radius; // current radius (grows from 0 → maxRadius as life drains)
  float maxRadius;
  Col color;
};

struct ParticleSystem {
  static constexpr int MAX = 2048;
  static constexpr int MAX_RINGS = 32;

  Particle buf[MAX];
  int count = 0;

  BlastRing rings[MAX_RINGS];
  int ringCount = 0;

  void reset() {
    count = 0;
    ringCount = 0;
  }

  // Spawns a cluster of thick wavy orbs that hang in place behind the ball.
  void spawnTrail(float x, float y, float vx, float vy, Col color);

  // Spawns radial wavy-orb trails + an expanding neon shockwave ring.
  void spawnBurst(float x, float y, Col color, int n = 16);

  void update(float dt);
  void draw(SDL_Renderer *r) const;
};
