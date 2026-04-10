#include "Particles.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>

static float randf() { return (float)rand() / (float)RAND_MAX; }

static void driftedColor(Col base, float life, float amp, uint8_t &or_,
                         uint8_t &og, uint8_t &ob) {
  // Each channel offset by 2π/3 so they cycle out of phase (neon shimmer).
  float t = (1.f - life) * 6.2832f;
  or_ = (uint8_t)SDL_clamp((int)base.r + (int)(sinf(t + 0.000f) * amp), 0, 255);
  og = (uint8_t)SDL_clamp((int)base.g + (int)(sinf(t + 2.094f) * amp), 0, 255);
  ob = (uint8_t)SDL_clamp((int)base.b + (int)(sinf(t + 4.189f) * amp), 0, 255);
}

static void hotColor(uint8_t r, uint8_t g, uint8_t b, uint8_t &or_, uint8_t &og,
                     uint8_t &ob) {
  or_ = (uint8_t)std::min(255, (int)r + 100);
  og = (uint8_t)std::min(255, (int)g + 100);
  ob = (uint8_t)std::min(255, (int)b + 100);
}

static void glowOrb(SDL_Renderer *r, int cx, int cy, int rad, uint8_t cr,
                    uint8_t cg, uint8_t cb, uint8_t alpha) {
  if (rad < 1 || alpha == 0)
    return;

  auto circle = [&](int radius, uint8_t a) {
    if (radius < 1 || a == 0)
      return;
    SDL_SetRenderDrawColor(r, cr, cg, cb, a);
    for (int dy = -radius; dy <= radius; dy++) {
      int dx = (int)sqrtf((float)(radius * radius - dy * dy));
      SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
  };

  circle(rad + 5, (uint8_t)(alpha * 0.07f));
  circle(rad + 3, (uint8_t)(alpha * 0.14f));
  circle(rad + 1, (uint8_t)(alpha * 0.28f));
  circle(rad, alpha);

  // Hot bright centre
  uint8_t hr, hg, hb;
  hotColor(cr, cg, cb, hr, hg, hb);
  int hr2 = std::max(1, rad / 2);
  SDL_SetRenderDrawColor(r, hr, hg, hb, (uint8_t)(alpha * 0.75f));
  for (int dy = -hr2; dy <= hr2; dy++) {
    int dx = (int)sqrtf((float)(hr2 * hr2 - dy * dy));
    SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
  }
}

// Blast ring — Bresenham circle outline with bloom layers
static void glowRing(SDL_Renderer *r, int cx, int cy, int radius, uint8_t cr,
                     uint8_t cg, uint8_t cb, uint8_t alpha) {
  auto bresenham = [&](int rad, uint8_t a) {
    if (rad <= 0 || a == 0)
      return;
    SDL_SetRenderDrawColor(r, cr, cg, cb, a);
    int x = rad, y = 0, err = 0;
    while (x >= y) {
      SDL_RenderDrawPoint(r, cx + x, cy + y);
      SDL_RenderDrawPoint(r, cx + y, cy + x);
      SDL_RenderDrawPoint(r, cx - x, cy + y);
      SDL_RenderDrawPoint(r, cx - y, cy + x);
      SDL_RenderDrawPoint(r, cx + x, cy - y);
      SDL_RenderDrawPoint(r, cx + y, cy - x);
      SDL_RenderDrawPoint(r, cx - x, cy - y);
      SDL_RenderDrawPoint(r, cx - y, cy - x);
      y++;
      err += (err <= 0) ? 2 * y + 1 : 2 * (y - --x) + 1;
    }
  };

  bresenham(radius + 4, (uint8_t)(alpha * 0.08f));
  bresenham(radius + 2, (uint8_t)(alpha * 0.18f));
  bresenham(radius + 1, (uint8_t)(alpha * 0.35f));
  bresenham(radius, alpha);
  // Inner fill-ring for a "colored blast" look
  bresenham(radius - 2, (uint8_t)(alpha * 0.50f));
  bresenham(radius - 4, (uint8_t)(alpha * 0.30f));
  bresenham(radius - 6, (uint8_t)(alpha * 0.12f));
}

void ParticleSystem::spawnTrail(float x, float y, float vx, float vy,
                                Col color) {
  float speed = sqrtf(vx * vx + vy * vy);
  float nx = (speed > 1.f) ? vx / speed : 1.f;
  float ny = (speed > 1.f) ? vy / speed : 0.f;
  float px = -ny, py = nx; // perpendicular = wave axis

  // Two small dots per call — enough for a continuous flame without bulk.
  for (int i = 0; i < 2 && count < MAX; i++) {
    Particle &p = buf[count++];

    // Tiny lateral spread so the trail is a thin ribbon, not a band.
    float lateralOff = (randf() - 0.5f) * 2.f;
    p.x = x + px * lateralOff;
    p.y = y + py * lateralOff;

    // Almost stationary — dots hang at the spawn point while the ball moves
    // away.
    p.vx = -nx * (2.f + randf() * 4.f);
    p.vy = -ny * (2.f + randf() * 4.f);

    p.life = 0.55f + randf() * 0.30f;
    p.decay = 1.5f + randf() * 1.0f;
    p.color = color;
    p.radius = 2 + (int)(randf() * 2.f); // small: 2–3 px base
    p.rgbDrift = 55.f + randf() * 40.f;

    p.perpX = px;
    p.perpY = py;
    p.waveAmp = 3.f + randf() * 4.f; // subtle curve, not a rocket exhaust
    p.wavePhase = randf() * 6.2832f;
    p.isTrail = true;
  }
}

void ParticleSystem::spawnBurst(float x, float y, Col color, int n) {
  // Shockwave ring
  if (ringCount < MAX_RINGS) {
    BlastRing &ring = rings[ringCount++];
    ring.x = x;
    ring.y = y;
    ring.life = 1.f;
    ring.decay = 1.8f;
    ring.radius = 0.f;
    ring.maxRadius = 80.f + randf() * 50.f;
    ring.color = color;
  }

  // Thick wavy-orb trails radiating outward
  for (int i = 0; i < n && count < MAX; i++) {
    Particle &p = buf[count++];

    p.x = x + (randf() - 0.5f) * 8.f;
    p.y = y + (randf() - 0.5f) * 8.f;

    float angle = (float)i / (float)n * 6.2832f + randf() * 0.5f;
    float spd = 120.f + randf() * 220.f;
    p.vx = cosf(angle) * spd;
    p.vy = sinf(angle) * spd;

    p.life = 0.85f + randf() * 0.35f;
    p.decay = 1.0f + randf() * 0.9f;
    p.color = color;
    p.radius = 6 + (int)(randf() * 5.f);
    p.rgbDrift = 45.f + randf() * 45.f;

    // Wave axis perpendicular to burst direction
    p.perpX = -sinf(angle);
    p.perpY = cosf(angle);
    p.waveAmp = 9.f + randf() * 10.f;
    p.wavePhase = randf() * 6.2832f;
    p.isTrail = false;
  }
}

void ParticleSystem::update(float dt) {
  // Particles
  for (int i = 0; i < count;) {
    Particle &p = buf[i];
    p.x += p.vx * dt;
    p.y += p.vy * dt;
    // Drag — burst orbs decelerate as they travel outward
    p.vx *= (1.f - dt * 4.f);
    p.vy *= (1.f - dt * 4.f);
    p.life -= p.decay * dt;
    if (p.life <= 0.f)
      buf[i] = buf[--count];
    else
      i++;
  }

  // Blast rings — expand outward as life drains
  for (int i = 0; i < ringCount;) {
    BlastRing &ring = rings[i];
    ring.life -= ring.decay * dt;
    ring.radius = ring.maxRadius * (1.f - ring.life);
    if (ring.life <= 0.f)
      rings[i] = rings[--ringCount];
    else
      i++;
  }
}

void ParticleSystem::draw(SDL_Renderer *r) const {
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  // Blast rings first (behind particles)
  for (int i = 0; i < ringCount; i++) {
    const BlastRing &ring = rings[i];
    uint8_t cr, cg, cb;
    driftedColor(ring.color, ring.life, 40.f, cr, cg, cb);
    uint8_t alpha = (uint8_t)(ring.life * ring.life * 200.f); // ease out
    glowRing(r, (int)ring.x, (int)ring.y, (int)ring.radius, cr, cg, cb, alpha);
  }

  // Particles
  for (int i = 0; i < count; i++) {
    const Particle &p = buf[i];

    uint8_t cr, cg, cb;
    driftedColor(p.color, p.life, p.rgbDrift, cr, cg, cb);

    // Wave displacement: older particles sit at a different sine phase,
    // creating a continuous curve rather than all wiggling in sync.
    float waveDisp =
        sinf(p.wavePhase + (1.f - p.life) * 9.f) * p.waveAmp * p.life;
    int cx = (int)(p.x + p.perpX * waveDisp);
    int cy = (int)(p.y + p.perpY * waveDisp);

    // Radius tapers to a point at the old end of the trail.
    int rad = std::max(1, (int)(p.radius * p.life));

    if (p.isTrail) {
      // ── Candlestick flame dot ──────────────────────────────────────────
      // Soft outer haze + bright core only — no heavy bloom stack.
      uint8_t alpha = (uint8_t)(p.life * p.life * 200.f);
      SDL_SetRenderDrawColor(r, cr, cg, cb, (uint8_t)(alpha * 0.18f));
      for (int dy = -(rad + 2); dy <= rad + 2; dy++) {
        int dx = (int)sqrtf((float)((rad + 2) * (rad + 2) - dy * dy));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
      }
      SDL_SetRenderDrawColor(r, cr, cg, cb, alpha);
      for (int dy = -rad; dy <= rad; dy++) {
        int dx = (int)sqrtf((float)(rad * rad - dy * dy));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
      }
      // Single-pixel hot white centre
      uint8_t hr, hg, hb;
      hotColor(cr, cg, cb, hr, hg, hb);
      SDL_SetRenderDrawColor(r, hr, hg, hb, (uint8_t)(alpha * 0.85f));
      SDL_RenderDrawPoint(r, cx, cy);
    } else {
      // ── Burst orb — full layered bloom ────────────────────────────────
      uint8_t alpha = (uint8_t)(p.life * p.life * 230.f);
      glowOrb(r, cx, cy, rad, cr, cg, cb, alpha);
    }
  }

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}
