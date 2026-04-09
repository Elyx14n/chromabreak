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
                  bool &gameOver) {
  x += vx * dt;
  y += vy * dt;

  // Wall bounce with explicit ball separation
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

      // Color match -> flood-fill destroy the connected group
      if (map.cells[r][c] == color)
        map.floodFill(r, c, map.cells[r][c], score);

      // Separate ball completely from the brick, then force velocity
      // direction AWAY from it (fabsf guards against double-reflection).
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
