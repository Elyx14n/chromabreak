#include "Particles.h"
#include <cstdlib>
#include <cmath>

static float randf() { return (float)rand() / (float)RAND_MAX; }

void ParticleSystem::spawnTrail(float x, float y, float vx, float vy, Col color) {
    if (count >= MAX) return;
    Particle& p = buf[count++];
    p.x = x + (randf() - 0.5f) * 5.f;
    p.y = y + (randf() - 0.5f) * 5.f;
    float speed = 15.f + randf() * 25.f;
    float angle = randf() * 6.2832f;
    p.vx = cosf(angle) * speed - vx * 0.08f;
    p.vy = sinf(angle) * speed - vy * 0.08f;
    p.life  = 0.6f + randf() * 0.5f;
    p.decay = 2.8f + randf() * 1.5f;
    p.color = color;
    p.radius = 1 + (int)(randf() * 3.f);
}

void ParticleSystem::spawnBurst(float x, float y, Col color, int n) {
    for (int i = 0; i < n && count < MAX; i++) {
        Particle& p = buf[count++];
        p.x = x + (randf() - 0.5f) * 10.f;
        p.y = y + (randf() - 0.5f) * 10.f;
        float speed = 50.f + randf() * 160.f;
        float angle = (float)i / (float)n * 6.2832f + randf() * 0.6f;
        p.vx = cosf(angle) * speed;
        p.vy = sinf(angle) * speed;
        p.life  = 1.f;
        p.decay = 1.1f + randf() * 1.2f;
        p.color = color;
        p.radius = 2 + (int)(randf() * 4.f);
    }
}

void ParticleSystem::update(float dt) {
    for (int i = 0; i < count; ) {
        Particle& p = buf[i];
        p.x    += p.vx * dt;
        p.y    += p.vy * dt;
        p.vy   += 70.f * dt; // gentle gravity
        p.life -= p.decay * dt;
        if (p.life <= 0.f) {
            buf[i] = buf[--count]; // swap-remove
        } else {
            i++;
        }
    }
}

void ParticleSystem::draw(SDL_Renderer* r) const {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < count; i++) {
        const Particle& p = buf[i];
        uint8_t a = (uint8_t)(p.life * 210.f);
        SDL_SetRenderDrawColor(r, p.color.r, p.color.g, p.color.b, a);
        int rad = p.radius;
        for (int dy = -rad; dy <= rad; dy++) {
            int dx = (int)sqrtf((float)(rad * rad - dy * dy));
            SDL_RenderDrawLine(r, (int)p.x - dx, (int)p.y + dy,
                                  (int)p.x + dx, (int)p.y + dy);
        }
    }
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}
