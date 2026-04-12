#include "Paddle.h"
#include "Constants.h"
#include "SDL2/SDL.h"

Paddle::Paddle() : x_(WIN_W / 2.f), y_(PADDLE_Y) {}

void Paddle::update(float dt, const uint8_t *keys) {
  if (keys[SDL_SCANCODE_LEFT])
    x_ -= PADDLE_X_SPEED * dt;
  if (keys[SDL_SCANCODE_RIGHT])
    x_ += PADDLE_X_SPEED * dt;
  if (keys[SDL_SCANCODE_UP])
    y_ -= PADDLE_Y_SPEED * dt;
  if (keys[SDL_SCANCODE_DOWN])
    y_ += PADDLE_Y_SPEED * dt;

  x_ = SDL_clamp(x_, PADDLE_W / 2.f, WIN_W - PADDLE_W / 2.f);
  y_ = SDL_clamp(y_, PADDLE_MIN_Y + PADDLE_H / 2.f,
                 PADDLE_MAX_Y - PADDLE_H / 2.f);
}

SDL_Rect Paddle::rect() const {
  return {int(x_ - PADDLE_W / 2), int(y_ - PADDLE_H / 2), PADDLE_W, PADDLE_H};
}
