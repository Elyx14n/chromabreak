#include "Ball.h"
#include "Map.h"
#include "Paddle.h"
#include <algorithm>
#include <cmath>

static bool rectsOverlap(const SDL_Rect &a, const SDL_Rect &b) {
  return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h &&
         a.y + a.h > b.y;
}

Ball::Ball() {
  x = WIN_W / 2.f;
  y = PADDLE_MIN_Y + BOTTOM_MARGIN * 0.35f;
  vx = BALL_SPEED * 0.55f;
  vy = -BALL_SPEED * 0.835f;
  color = BrickColor::RED;
  power = BallPower::NONE;
  powerTimer = 0.f;
}

void Ball::cycleColor() {
  int next = static_cast<int>(color) + 1;
  if (next >= static_cast<int>(BrickColor::COLOR_COUNT))
    next = 1;
  color = static_cast<BrickColor>(next);
}

SDL_Rect Ball::rect() const {
  return {(int)(x - BALL_R), (int)(y - BALL_R), BALL_R * 2, BALL_R * 2};
}

void Ball::update(float dt, const Paddle &paddle, Map &map, int &score,
                  bool &gameOver, ParticleSystem &ps) {
  if (power != BallPower::NONE) {
    powerTimer -= dt;
    if (powerTimer <= 0.f) {
      power = BallPower::NONE;
      powerTimer = 0.f;
    }
  }

  Col trailCol;
  if (power == BallPower::RAINBOW) {
    // Cycle through full hue spectrum using powerTimer
    float hue = fmodf((POWER_DURATION - powerTimer) * 2.5f, 6.f);
    int hi = (int)hue;
    float f = hue - (float)hi;
    uint8_t t = (uint8_t)(f * 255.f), q = (uint8_t)((1.f - f) * 255.f);
    switch (hi % 6) {
    case 0:
      trailCol = {255, t, 0};
      break;
    case 1:
      trailCol = {q, 255, 0};
      break;
    case 2:
      trailCol = {0, 255, t};
      break;
    case 3:
      trailCol = {0, q, 255};
      break;
    case 4:
      trailCol = {t, 0, 255};
      break;
    default:
      trailCol = {255, 0, q};
      break;
    }
  } else {
    trailCol = BrickPal::Colors[static_cast<int>(color)];
  }
  ps.spawnTrail(x, y, vx, vy, trailCol);

  x += vx * dt;
  y += vy * dt;

  // Wall bounce with explicit separation
  if (x - BALL_R < 0) {
    x = (float)BALL_R;
    vx = fabsf(vx);
  }
  if (x + BALL_R > WIN_W) {
    x = WIN_W - (float)BALL_R;
    vx = -fabsf(vx);
  }
  if (y - BALL_R < TOP_MARGIN) {
    y = TOP_MARGIN + (float)BALL_R;
    vy = fabsf(vy);
  }

  if (y - BALL_R > WIN_H) {
    gameOver = true;
    return;
  }

  // Paddle bounce
  SDL_Rect pr = paddle.rect();
  if (vy > 0.f && rectsOverlap(rect(), pr)) {
    y = (float)(pr.y - BALL_R);
    float rel =
        std::max(-1.f, std::min(1.f, (x - paddle.x) / (PADDLE_W * 0.5f)));
    vx = rel * BALL_SPEED * 0.85f;
    float minVy = BALL_SPEED * 0.35f;
    float vy2 = BALL_SPEED * BALL_SPEED - vx * vx;
    vy = -sqrtf(vy2 > minVy * minVy ? vy2 : minVy * minVy);
    return;
  }

  // Brick collision
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      if (map.cells[r][c] == BrickColor::EMPTY)
        continue;

      SDL_Rect br = rect();
      SDL_Rect cell = map.cellRect(r, c);
      if (!rectsOverlap(br, cell))
        continue;

      // Penetration depth on each axis
      int ox = std::min(br.x + br.w, cell.x + cell.w) - std::max(br.x, cell.x);
      int oy = std::min(br.y + br.h, cell.y + cell.h) - std::max(br.y, cell.y);

      BrickType bt = map.types[r][c];
      BrickColor bc = map.cells[r][c];
      float cx = cell.x + cell.w * 0.5f;
      float cy = cell.y + cell.h * 0.5f;
      Col brickCol = BrickPal::Colors[static_cast<int>(bc)];

      bool brickDestroyed = true;

      if (bt == BrickType::BOMB) {
        map.bombEffect(r, c, bc, score, ps);

      } else if (bt == BrickType::TRANSFORMER) {
        map.transformerEffect(r, c, bc, score, ps);

      } else if (bt == BrickType::RAINBOW) {
        power = BallPower::RAINBOW;
        powerTimer = POWER_DURATION;
        map.cells[r][c] = BrickColor::EMPTY;
        map.types[r][c] = BrickType::NORMAL;
        score += BASE_SCORE;
        ps.spawnBurst(cx, cy, brickCol, 28);

      } else if (bt == BrickType::REVERSER) {
        map.reverserEffect(r, c, score, ps);
      } else {
        bool colorMatch = (bc == color) || (power == BallPower::RAINBOW);
        if (colorMatch) {
          map.floodFill(r, c, bc, score, ps);
        } else {
          brickDestroyed = false;
        }
      }

      // Physical separation and direction-safe velocity flip
      if (ox <= oy) {
        if (x < cell.x + cell.w * 0.5f) {
          x -= ox;
          vx = -fabsf(vx);
        } else {
          x += ox;
          vx = fabsf(vx);
        }
      } else {
        if (y < cell.y + cell.h * 0.5f) {
          y -= oy;
          vy = -fabsf(vy);
        } else {
          y += oy;
          vy = fabsf(vy);
        }
      }
      return;
    }
  }
}
