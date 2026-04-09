#include "Paddle.h"
#include "Constants.h"
#include "SDL2/SDL.h"

void Paddle::update(float dt, const uint8_t *keys) {
  if (keys[SDL_SCANCODE_LEFT])
    x -= PADDLE_X_SPEED * dt;
  if (keys[SDL_SCANCODE_RIGHT])
    x += PADDLE_X_SPEED * dt;
  if (keys[SDL_SCANCODE_UP])
    y -= PADDLE_Y_SPEED * dt;
  if (keys[SDL_SCANCODE_DOWN])
    y += PADDLE_Y_SPEED * dt;

  x = SDL_clamp(x, PADDLE_W / 2.f, WIN_W - PADDLE_W / 2.f);
  y = SDL_clamp(y, PADDLE_MIN_Y + PADDLE_H / 2.f,
                PADDLE_MAX_Y - PADDLE_H / 2.f);
}

SDL_Rect Paddle::rect() const {
  return {int(x - PADDLE_W / 2), int(y - PADDLE_H / 2), PADDLE_W, PADDLE_H};
}