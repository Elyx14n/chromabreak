/*  Particle system
    ─────────────────────────────────────────────────────────────────────────
    There are two particle types (trail, burst) and one ring type (blast ring).
    All live in fixed-size arrays; a swap-remove pattern keeps the arrays packed
    so dead entries never leave holes that waste iteration.

    SPAWNING

    Trail particles are emitted continuously from the ball each frame (2 per
    call). The key idea is the perpendicular vector: if (nx, ny) is the ball's
    normalised direction of travel, then rotating it 90° gives (−ny, nx), which
    points sideways. Trail particles spawn offset randomly along that lateral
    axis and fly back opposite to the ball's direction, so they fan out behind
    it rather than bunching on the travel path. The particle's radius and
		wave amplitude decays with life, tapering to a point via draw and update.

    Burst particles fire on brick destruction (16–28 per burst). They are
    distributed evenly around a full circle: particle i gets angle i/n × 2π,
    which spaces n particles at equal angular gaps. A small random jitter (±0.5
    rad) is added so the ring looks organic rather than perfectly geometric.
    cos(angle) and sin(angle) convert the angle to a 2D velocity vector — this
    is just polar-to-cartesian conversion: x = r·cos(θ), y = r·sin(θ).

    A BlastRing is also spawned per burst: an expanding circle that fades out,
    giving the impact a shockwave feel without needing individual particles for
    the ring itself.

    UPDATE (each frame)

    Position:  p.x += p.vx * dt,  p.y += p.vy * dt
    Drag:      p.vx *= (1 − dt × 4),  p.vy *= (1 − dt × 4)
        The drag factor (1 − dt × 4) is applied every frame. Because dt is
        small this is approximately e^(−4t) decay — particles slow down
        smoothly rather than stopping abruptly.
    Lifetime:  p.life -= p.decay * dt
        When life reaches 0 the particle is removed via swap-remove:
        the dead slot is overwritten with the last element and the count
        is decremented. This keeps the array packed without shifting memory.

    Blast rings shrink their life the same way and grow their radius as
        ring.radius = ring.maxRadius × (1 − ring.life)
    so radius starts at 0 (life = 1) and reaches maxRadius when life = 0.

    DRAWING

    Colour drift (driftedColor): each channel is offset by a sine wave whose
    phase advances as the particle ages. The three channels are 120° apart on
    the wave (0, 2π/3, 4π/3), which places them evenly around a colour wheel,
    so the colour cycles smoothly through nearby hues as the particle fades.

    Wave displacement: each particle wobbles sideways as it travels. The offset
    is  sin(wavePhase + (1−life)×9) × waveAmp × life  applied along perpX/perpY
    (the perpendicular direction stored at spawn time). The (1−life)×9 term
    speeds up the oscillation as the particle ages; multiplying by life shrinks
    the amplitude to zero as it dies, so the wobble fades naturally.

    Glow rendering: both orbs and rings are drawn as several concentric
    shapes at decreasing alpha — wide outer halos at ~8–18% opacity for the
    soft glow, then the core shape at full opacity. This fakes volumetric light
    without any shader support, using SDL's alpha-blended 2D primitives only.

    FORMULAS
      Perpendicular vector:  (px, py)     = (−ny, nx)
      Burst angle:           angle        = (i / n) × 2π + jitter
      Polar → cartesian:     vx           = cos(angle) × speed
                             vy           = sin(angle) × speed
      Euler integration:     p.x         += p.vx × dt
      Drag:                  p.vx        *= (1 − dt × 4)
      Ring radius:           ring.radius  = ring.maxRadius × (1 − ring.life)
      Colour drift phase:    t            = (1 − life) × 2π
      Colour drift channels: r            = base.r + sin(t + 0.000) × amp   (0°)
                             g            = base.g + sin(t + 2.094) × amp   (120°)
                             b            = base.b + sin(t + 4.189) × amp   (240°)
      Wave displacement:     disp         = sin(wavePhase + (1−life)×9) × waveAmp × life
      Draw position:         cx           = p.x + px × disp
    ─────────────────────────────────────────────────────────────────────────*/

#include "Particles.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>

static float randf() { return (float)rand() / (float)RAND_MAX; }

// As the particle ages (life 1→0), t sweeps 0→2π. Each channel is driven by
// a sine offset by 2π/3 (120°) — evenly spaced around the colour wheel —
// so R, G, B each peak at a different point in the cycle, producing a
// smooth hue shift as the particle fades out.
static void driftedColor(Col base, float life, float amp, uint8_t &or_,
                         uint8_t &og, uint8_t &ob) {
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

  uint8_t hr, hg, hb;
  hotColor(cr, cg, cb, hr, hg, hb);
  int hr2 = std::max(1, rad / 2);
  SDL_SetRenderDrawColor(r, hr, hg, hb, (uint8_t)(alpha * 0.75f));
  for (int dy = -hr2; dy <= hr2; dy++) {
    int dx = (int)sqrtf((float)(hr2 * hr2 - dy * dy));
    SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
  }
}

static void glowRing(SDL_Renderer *r, int cx, int cy, int radius, uint8_t cr,
                     uint8_t cg, uint8_t cb, uint8_t alpha) {
  // Bresenham's circle traces the outline using only integer arithmetic.
  //
  // The true circle satisfies x²+y²=r². We start at (rad, 0) and increment
  // y each step, deciding whether to also pull x inward to stay close to the
  // true radius. err is the running tally of that deviation:
  //
  //   err ≤ 0 → on/inside the circle: just move y up.
  //             Add 2y+1 to err — that's exactly how much y² grew this step
  //             ((y+1)²−y² = 2y+1), so err stays current without recomputing.
  //
  //   err > 0 → drifted outside: pull x inward first (--x), then
  //             add 2(y−x)+1 to account for both the y growth and x shrink.
  //
  // By accumulating the *difference* each step rather than recomputing x²+y²
  // from scratch, the whole loop runs with additions only — no multiplications,
  // no floats, no sqrt. Each computed point is mirrored into all 8 octants.
  auto bresenham = [&](int rad, uint8_t a) {
    if (rad <= 0 || a == 0)
      return;
    SDL_SetRenderDrawColor(r, cr, cg, cb, a);

    // We trace one octant (x decreasing, y increasing, x >= y throughout)
    // and mirror each point into all 8 by swapping/negating x and y.
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

      y++;  // always climb the octant by one row
      // err tracks deviation from the true circle x²+y²=r².
      // Positive means the current point is outside the circle; negative means inside.
      if (err <= 0) {
        err += 2 * y + 1;
      } else {
        x--;
        err += 2 * (y - x) + 1;
      }
    }
  };

  // Concentric rings at decreasing alpha simulate a soft glow falloff:
  // outer halos are nearly invisible (8%, 18%), the core ring is fully opaque,
  // and inner rings taper off to give the band some thickness.
  bresenham(radius + 4, (uint8_t)(alpha * 0.08f));
  bresenham(radius + 2, (uint8_t)(alpha * 0.18f));
  bresenham(radius + 1, (uint8_t)(alpha * 0.35f));
  bresenham(radius, alpha);
  bresenham(radius - 2, (uint8_t)(alpha * 0.50f));
  bresenham(radius - 4, (uint8_t)(alpha * 0.30f));
  bresenham(radius - 6, (uint8_t)(alpha * 0.12f));
}

void ParticleSystem::reset() {
  count_ = 0;
  ringCount_ = 0;
}

void ParticleSystem::spawnTrail(float x, float y, float vx, float vy,
                                Col color) {
  float speed = sqrtf(vx * vx + vy * vy);
  float nx = (speed > 1.f) ? vx / speed : 1.f;
  float ny = (speed > 1.f) ? vy / speed : 0.f;
  // Rotate the direction vector 90°: if (nx, ny) is forward, (−ny, nx) is
  // perpendicular. Trail particles spawn along that lateral axis so they
  // spread sideways rather than back along the travel path.
	// Prevents from stacking on the same pixel
  float px = -ny, py = nx;

  for (int i = 0; i < 2 && count_ < MAX; i++) {
    Particle &p = buf_[count_++];

    float lateralOff = (randf() - 0.5f) * 2.f;
    p.x = x + px * lateralOff;
    p.y = y + py * lateralOff;

    p.vx = -nx * (2.f + randf() * 4.f);
    p.vy = -ny * (2.f + randf() * 4.f);

    p.life = 0.55f + randf() * 0.30f;
    p.decay = 1.5f + randf() * 1.0f;
    p.color = color;
    p.radius = 2 + (int)(randf() * 2.f);
    p.rgbDrift = 55.f + randf() * 40.f;

    p.perpX = px;
    p.perpY = py;
    p.waveAmp = 3.f + randf() * 4.f;
    p.wavePhase = randf() * 6.2832f;
    p.isTrail = true;
  }
}

void ParticleSystem::spawnBurst(float x, float y, Col color, int n) {
  if (ringCount_ < MAX_RINGS) {
    BlastRing &ring = rings_[ringCount_++];
    ring.x = x;
    ring.y = y;
    ring.life = 1.f;
    ring.decay = 1.8f;
    ring.radius = 0.f;
    ring.maxRadius = 80.f + randf() * 50.f;
    ring.color = color;
  }

  for (int i = 0; i < n && count_ < MAX; i++) {
    Particle &p = buf_[count_++];

    p.x = x + (randf() - 0.5f) * 8.f;
    p.y = y + (randf() - 0.5f) * 8.f;

    // Distribute n particles evenly around a full circle (i/n * 2π),
    // plus a small random jitter so the burst doesn't look mechanical.
    // cos/sin converts the angle to a cartesian velocity vector.
    float angle = (float)i / (float)n * 6.2832f + randf() * 0.5f;
    float spd = 120.f + randf() * 220.f;
    p.vx = cosf(angle) * spd;
    p.vy = sinf(angle) * spd;

    p.life = 0.85f + randf() * 0.35f;
    p.decay = 1.0f + randf() * 0.9f;
    p.color = color;
    p.radius = 6 + (int)(randf() * 5.f);
    p.rgbDrift = 45.f + randf() * 45.f;

    p.perpX = -sinf(angle);
    p.perpY = cosf(angle);
    p.waveAmp = 9.f + randf() * 10.f;
    p.wavePhase = randf() * 6.2832f;
    p.isTrail = false;
  }
}

void ParticleSystem::update(float dt) {
  for (int i = 0; i < count_;) {
    Particle &p = buf_[i];
    p.x += p.vx * dt;
    p.y += p.vy * dt;
    p.vx *= (1.f - dt * 4.f);
    p.vy *= (1.f - dt * 4.f);
    p.life -= p.decay * dt;
    if (p.life <= 0.f)
      buf_[i] = buf_[--count_];
    else
      i++;
  }

  for (int i = 0; i < ringCount_;) {
    BlastRing &ring = rings_[i];
    ring.life -= ring.decay * dt;
    ring.radius = ring.maxRadius * (1.f - ring.life);
    if (ring.life <= 0.f)
      rings_[i] = rings_[--ringCount_];
    else
      i++;
  }
}

void ParticleSystem::draw(SDL_Renderer *r) const {
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  for (int i = 0; i < ringCount_; i++) {
    const BlastRing &ring = rings_[i];
    uint8_t cr, cg, cb;
    driftedColor(ring.color, ring.life, 40.f, cr, cg, cb);
    uint8_t alpha = (uint8_t)(ring.life * ring.life * 200.f);
    glowRing(r, (int)ring.x, (int)ring.y, (int)ring.radius, cr, cg, cb, alpha);
  }

  for (int i = 0; i < count_; i++) {
    const Particle &p = buf_[i];

    uint8_t cr, cg, cb;
    driftedColor(p.color, p.life, p.rgbDrift, cr, cg, cb);

    // (1−life) grows 0→1 as the particle ages, so the *9 factor speeds up
    // the wave frequency over time. Multiplying by life fades the swing out
    // as it dies. Displacement is along perpX/Y — always sideways to travel.
    float waveDisp =
        sinf(p.wavePhase + (1.f - p.life) * 9.f) * p.waveAmp * p.life;
    int cx = (int)(p.x + p.perpX * waveDisp);
    int cy = (int)(p.y + p.perpY * waveDisp);

    int rad = std::max(1, (int)(p.radius * p.life));

    if (p.isTrail) {
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
      uint8_t hr, hg, hb;
      hotColor(cr, cg, cb, hr, hg, hb);
      SDL_SetRenderDrawColor(r, hr, hg, hb, (uint8_t)(alpha * 0.85f));
      SDL_RenderDrawPoint(r, cx, cy);
    } else {
      uint8_t alpha = (uint8_t)(p.life * p.life * 230.f);
      glowOrb(r, cx, cy, rad, cr, cg, cb, alpha);
    }
  }

  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
}
