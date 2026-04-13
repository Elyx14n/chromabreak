/*  Ball movement and collision
    ─────────────────────────────────────────────────────────────────────────
    Every frame, update() is called with the elapsed time (dt, in seconds).
    The ball has a position (x_, y_) and a velocity (vx_, vy_) in pixels/second.
    The update steps run in order; the first collision that fires returns early
    so only one response is processed per frame.

    STEP 1 — Move
        x_ += vx_ * dt
        y_ += vy_ * dt
    Simple Euler integration. dt is small enough (~16 ms) that the error is
    invisible; no need for anything fancier.

    STEP 2 — Wall bounces
    After moving, we check whether the ball's edge has crossed a boundary.
    If it has, we clamp the position back to the boundary and flip the relevant
    velocity component. We use fabsf() rather than just negating — if the ball
    somehow tunnelled past the wall in a single frame (e.g. a lag spike), the
    sign might already be pointing the right way, and negating would send it
    further in. fabsf always forces the component to point away from the wall.

    STEP 3 — Paddle bounce
    We check for AABB overlap (two rectangles are touching if each axis's ranges
    overlap) and only react when the ball is moving downward (vy_ > 0), which
    prevents the ball from sticking if it spawns inside the paddle.

    The bounce direction is controlled by where the ball hits the paddle:
        rel = (ball_x − paddle_center_x) / (paddle_half_width)
    rel lands in [−1, 1]: −1 is the far left edge, 0 is the centre, +1 is far
    right. vx is set to rel × max_speed × 0.85, so hitting the edges gives steep
    angles and hitting the centre gives a gentle one.

    vy must be recalculated to keep the ball at constant speed. Since we know
    the desired total speed and already know vx, Pythagoras gives us:
        vy = √(speed² − vx²)
    (negative because the ball must go upward after bouncing). A minimum vy
    floor stops the player from creating a near-horizontal shot that never
    reaches the bricks.

    STEP 4 — Brick collision
    We iterate every cell (ROWS × COLS) and check for AABB overlap with the
    ball. For each overlapping brick we compute the penetration depth on both
    axes:
        ox = horizontal overlap width
        oy = vertical overlap height
    The smaller axis is the one the ball entered through — that's where the
    collision face is. If ox ≤ oy the ball came from the side, so we flip vx
    and push it horizontally. If oy < ox the ball came from the top or bottom,
    so we flip vy and push it vertically. fabsf is used again to guarantee we
    always push away from the brick regardless of accumulated penetration.

    Special bricks (BOMB, TRANSFORMER, RAINBOW, REVERSER) trigger their effects
    regardless of ball colour. Normal bricks are only destroyed on a colour match
    (or if the ball has RAINBOW power), which triggers a flood-fill that clears
    all connected same-colour normal bricks in one hit.

    FORMULAS
      Euler integration:  x_       += vx_ × dt
      Wall enforce:       vx_       = ±fabsf(vx_)   (sign set per wall)
      Paddle hit position:rel       = (ball_x − paddle_cx) / (paddle_w × 0.5)
                          rel       = clamp(rel, −1, 1)
      Paddle exit angle:  vx_       = rel × BALL_SPEED × 0.85
      Speed conservation: vy_       = −√(BALL_SPEED² − vx_²)
      Min vertical speed: vy_       = −√max(vy², minVy²)
      Brick overlap axes: ox        = min(ball.r, brick.r) − max(ball.l, brick.l)
                          oy        = min(ball.b, brick.b) − max(ball.t, brick.t)
      Collision axis:     if ox ≤ oy → side hit → flip vx
                          if oy < ox → top/bottom hit → flip vy
    ─────────────────────────────────────────────────────────────────────────*/

#include "Ball.h"
#include "Audio.h"
#include "Map.h"
#include "Paddle.h"
#include <algorithm>
#include <cmath>

static bool rectsOverlap(const SDL_Rect &a, const SDL_Rect &b) {
  return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h &&
         a.y + a.h > b.y;
}

Ball::Ball() {
  x_ = WIN_W / 2.f;
  y_ = PADDLE_MIN_Y + BOTTOM_MARGIN * 0.35f;
  vx_ = BALL_SPEED * 0.55f;
  vy_ = -BALL_SPEED * 0.835f;
  color_ = BrickColor::RED;
  power_ = BallPower::NONE;
  powerTimer_ = 0.f;
}

void Ball::cycleColor() {
  int next = static_cast<int>(color_) + 1;
  if (next >= static_cast<int>(BrickColor::COLOR_COUNT))
    next = 1;
  color_ = static_cast<BrickColor>(next);
}

SDL_Rect Ball::rect() const {
  return {(int)(x_ - BALL_R), (int)(y_ - BALL_R), BALL_R * 2, BALL_R * 2};
}

void Ball::update(float dt, const Paddle &paddle, Map &map, int &score,
                  bool &gameOver, ParticleSystem &ps, Audio &audio) {
  if (power_ != BallPower::NONE) {
    powerTimer_ -= dt;
    if (powerTimer_ <= 0.f) {
      power_ = BallPower::NONE;
      powerTimer_ = 0.f;
    }
  }

  Col trailCol;
  if (power_ == BallPower::RAINBOW) {
    // Animate the trail colour through the full hue wheel for the duration
    // of the power-up. (POWER_DURATION - powerTimer_) is the elapsed time
    // since the power started (0 at pickup, POWER_DURATION at expiry).
    // Multiplying by 2.5 is purely a visual tuning choice: over 12 seconds it
    // produces 12×2.5/6 = 5 full colour cycles. fmodf wraps into
		// [0, 6] — the color wheel array length.
    float hue = fmodf((POWER_DURATION - powerTimer_) * 2.5f, 6.f);

    // Same sector/interpolation logic as hueToRgb in Render.cpp:
    // hi = which of the 6 sectors (Red→Yellow, Yellow→Green, etc.)
    // f  = fractional position within that sector (0.0 at entry, 1.0 at exit)
    // t  = tint:   the incoming channel, rising  0→255 across the sector
    // q  = quench: the outgoing channel, falling 255→0 across the sector
    // Each case routes t and q to the right RGB slots so the colour transitions
    // smoothly around the wheel without any explicit lerp calls.
    int     hi = (int)hue;
    float   f  = hue - (float)hi;
    uint8_t t = (uint8_t)(f * 255.f);
		uint8_t q = (uint8_t)((1.f - f) * 255.f);

    // Each case covers one 60° sector. The channel that changes between the
    // two anchor colours gets t or q; the others are pinned to 255 or 0.
    // Anchor RGBs: Red(255,0,0) Yellow(255,255,0) Green(0,255,0)
    //              Cyan(0,255,255) Blue(0,0,255) Magenta(255,0,255)
    switch (hi % 6) {
    case 0: trailCol = {255, t,   0};   break; // Red→Yellow:   G rises
    case 1: trailCol = {q,   255, 0};   break; // Yellow→Green: R falls
    case 2: trailCol = {0,   255, t};   break; // Green→Cyan:   B rises
    case 3: trailCol = {0,   q,   255}; break; // Cyan→Blue:    G falls
    case 4: trailCol = {t,   0,   255}; break; // Blue→Magenta: R rises
    default:trailCol = {255, 0,   q};   break; // Magenta→Red:  B falls
    }
  } else {
    trailCol = BrickPal::Colors[static_cast<int>(color_)];
  }
  ps.spawnTrail(x_, y_, vx_, vy_, trailCol);

  x_ += vx_ * dt;
  y_ += vy_ * dt;

  // fabsf instead of negating: if the ball tunnelled past the wall in one
  // frame the sign might already be correct after a naïve negate. fabsf always
  // forces the component away from the wall regardless of current sign.
  // Negating would flip it back into the wall; fabsf leaves it alone.
  // Each wall has one required sign: left→positive, right→negative,
  // top→positive. (no bottom—game over)
  if (x_ - BALL_R < 0) {
    audio.playSound(SFXLib::BallRebound);
    x_ = (float)BALL_R;
    vx_ = fabsf(vx_);
  }
  if (x_ + BALL_R > WIN_W) {
    audio.playSound(SFXLib::BallRebound);
    x_ = WIN_W - (float)BALL_R;
    vx_ = -fabsf(vx_);
  }
  if (y_ - BALL_R < TOP_MARGIN) {
    audio.playSound(SFXLib::BallRebound);
    y_ = TOP_MARGIN + (float)BALL_R;
    vy_ = fabsf(vy_);
  }

  if (y_ - BALL_R > WIN_H) {
    gameOver = true;
    return;
  }

  // Paddle bounce
  SDL_Rect pr = paddle.rect();
  if (vy_ > 0.f && rectsOverlap(rect(), pr)) {
    audio.playSound(SFXLib::BallPad);
    y_ = (float)(pr.y - BALL_R);

    // rel ∈ [−1, 1]: −1 = far left edge, 0 = centre, 1 = far right edge.
    // vx is set proportional to rel so the player steers with hit position.
		// rel = (ball_x − paddle_center_x) / (paddle_half_width)
    float rel =
        std::max(-1.f, std::min(1.f, (x_ - paddle.getX()) / (PADDLE_W * 0.5f)));
    vx_ = rel * BALL_SPEED * 0.85f;

    // Keep |v| constant: vy = √(speed² − vx²). The minVy floor prevents a
    // near-horizontal shot that takes forever to reach the brick field.
    float minVy = BALL_SPEED * 0.35f;
    float vy2 = BALL_SPEED * BALL_SPEED - vx_ * vx_;
    vy_ = -sqrtf(vy2 > minVy * minVy ? vy2 : minVy * minVy);
    return;
  }

  // Brick collision
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      if (map.colorAt(r, c) == BrickColor::EMPTY)
        continue;

      SDL_Rect br = rect();
      SDL_Rect cell = map.cellRect(r, c);
      if (!rectsOverlap(br, cell))
        continue;

      int ox = std::min(br.x + br.w, cell.x + cell.w) - std::max(br.x, cell.x);
      int oy = std::min(br.y + br.h, cell.y + cell.h) - std::max(br.y, cell.y);

      BrickType bt = map.typeAt(r, c);
      BrickColor bc = map.colorAt(r, c);
      float cx = cell.x + cell.w * 0.5f;
      float cy = cell.y + cell.h * 0.5f;
      Col brickCol = BrickPal::Colors[static_cast<int>(bc)];

      bool brickDestroyed = true;
      if (bc != BrickColor::EMPTY)
        audio.playSound(SFXLib::BallRebound);

      if (bt == BrickType::BOMB) {
        audio.playSound(SFXLib::Bomb);
        map.bombEffect(r, c, bc, score, ps);

      } else if (bt == BrickType::TRANSFORMER) {
        audio.playSound(SFXLib::Transformer);
        map.transformerEffect(r, c, bc, score, ps);

      } else if (bt == BrickType::RAINBOW) {
        audio.playSound(SFXLib::Rainbow);
        power_ = BallPower::RAINBOW;
        powerTimer_ = POWER_DURATION;
        map.clearCell(r, c);
        score += BASE_SCORE;
        ps.spawnBurst(cx, cy, brickCol, 28);

      } else if (bt == BrickType::REVERSER) {
        audio.playSound(SFXLib::Reverser);
        map.reverserEffect(r, c, score, ps);

      } else {
        bool colorMatch = (bc == color_) || (power_ == BallPower::RAINBOW);
        if (colorMatch) {
          int gained = map.floodFill(r, c, bc, score, ps);
          audio.playRandomBreakSingle();
          if (gained > BASE_SCORE * 3)
            audio.playRandomBreakMultiple();
        } else {
          brickDestroyed = false;
        }
      }

      (void)brickDestroyed;

      // Minimum-penetration response: ox and oy are the overlap depths on each
      // axis. The smaller axis is the one the ball entered through, so that's
      // the face the collision normal points along — flip only that component.
      // fabsf again ensures we push away from the brick rather than further in.
      if (ox <= oy) {
        if (x_ < cell.x + cell.w * 0.5f) {
          x_ -= ox;
          vx_ = -fabsf(vx_);
        } else {
          x_ += ox;
          vx_ = fabsf(vx_);
        }
      } else {
        if (y_ < cell.y + cell.h * 0.5f) {
          y_ -= oy;
          vy_ = -fabsf(vy_);
        } else {
          y_ += oy;
          vy_ = fabsf(vy_);
        }
      }
      return;
    }
  }
}

void Ball::handleColor(SDL_Keycode key, Audio &audio) {
  if (key == SDLK_SPACE) {
    audio.playSound(SFXLib::BallColorSwap);
    cycleColor();
    return;
  }

  auto it = colorMap_.find(key);
  if (it != colorMap_.end()) {
    audio.playSound(SFXLib::BallColorSwap);
    color_ = it->second;
  }
}
