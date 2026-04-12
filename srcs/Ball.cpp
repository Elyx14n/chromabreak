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
  (void)audio; // reserved for future per-event sfx calls

  if (power_ != BallPower::NONE) {
    powerTimer_ -= dt;
    if (powerTimer_ <= 0.f) {
      power_ = BallPower::NONE;
      powerTimer_ = 0.f;
    }
  }

  Col trailCol;
  if (power_ == BallPower::RAINBOW) {
    float hue = fmodf((POWER_DURATION - powerTimer_) * 2.5f, 6.f);
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
    trailCol = BrickPal::Colors[static_cast<int>(color_)];
  }
  ps.spawnTrail(x_, y_, vx_, vy_, trailCol);

  x_ += vx_ * dt;
  y_ += vy_ * dt;

  if (x_ - BALL_R < 0) {
    x_ = (float)BALL_R;
    vx_ = fabsf(vx_);
  }
  if (x_ + BALL_R > WIN_W) {
    x_ = WIN_W - (float)BALL_R;
    vx_ = -fabsf(vx_);
  }
  if (y_ - BALL_R < TOP_MARGIN) {
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
    y_ = (float)(pr.y - BALL_R);
    float rel =
        std::max(-1.f, std::min(1.f, (x_ - paddle.getX()) / (PADDLE_W * 0.5f)));
    vx_ = rel * BALL_SPEED * 0.85f;
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

      if (bt == BrickType::BOMB) {
        map.bombEffect(r, c, bc, score, ps);

      } else if (bt == BrickType::TRANSFORMER) {
        map.transformerEffect(r, c, bc, score, ps);

      } else if (bt == BrickType::RAINBOW) {
        power_ = BallPower::RAINBOW;
        powerTimer_ = POWER_DURATION;
        map.clearCell(r, c);
        score += BASE_SCORE;
        ps.spawnBurst(cx, cy, brickCol, 28);

      } else if (bt == BrickType::REVERSER) {
        map.reverserEffect(r, c, score, ps);

      } else {
        bool colorMatch = (bc == color_) || (power_ == BallPower::RAINBOW);
        if (colorMatch) {
          map.floodFill(r, c, bc, score, ps);
        } else {
          brickDestroyed = false;
        }
      }

      (void)brickDestroyed;

      // Physical separation and direction-safe velocity flip
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
