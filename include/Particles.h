#pragma once
#include "Constants.h"
#include <SDL2/SDL.h>

struct Particle {
    float x, y;
    float vx, vy;
    float life;   // [0, 1] — dies when <= 0
    float decay;  // life lost per second
    Col   color;
    int   radius;
};

struct ParticleSystem {
    static constexpr int MAX = 2048;
    Particle buf[MAX];
    int      count = 0;

    void reset() { count = 0; }

    // Spawn a fading trail dot near (x,y), drifting away from the ball's velocity.
    void spawnTrail(float x, float y, float vx, float vy, Col color);

    // Spawn an outward burst of n particles at (x,y).
    void spawnBurst(float x, float y, Col color, int n = 16);

    void update(float dt);
    void draw(SDL_Renderer* r) const;
};
