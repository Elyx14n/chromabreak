#include "Audio.h"
#include "Ball.h"
#include "Constants.h"
#include "Map.h"
#include "Paddle.h"
#include "Particles.h"
#include "Render.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>

int main(int, char *[]) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Initialization Error",
                             "Could not initialize SDL", nullptr);
    return 1;
  }
  if (TTF_Init() < 0) {
    SDL_Log("TTF_Init Error: %s", TTF_GetError());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Initialization Error",
                             "Could not initialize SDL_ttf", nullptr);
    return 1;
  }

  Audio audio;
  if (!audio.ok()) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Initialization Error",
                             "Could not initialize SDL_mixer", nullptr);
    return 1;
  }

  SDL_Window *w = SDL_CreateWindow("Chromabreak", SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H, 0);
  SDL_Renderer *r = SDL_CreateRenderer(
      w, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

  Render render(r);

  Map map;
  Paddle paddle;
  Ball ball;
  ParticleSystem particles;
  int score = 0;
  bool gameOver = false;
  GameState state = GameState::PLAYING;

  map.init();

  Uint64 prev = SDL_GetTicks64();
  bool running = true;
  SDL_Event event;

  while (running) {
    Uint64 now = SDL_GetTicks64();
    float dt = (now - prev) / 1000.f;
    prev = now;
    dt = std::min(dt, 0.05f);

    const uint8_t *keys = SDL_GetKeyboardState(nullptr);

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT)
        running = false;

      if (event.type == SDL_KEYDOWN) {
        if (state == GameState::PLAYING)
          ball.handleColor(event.key.keysym.sym, audio);
        if (state == GameState::GAME_OVER && event.key.keysym.sym == SDLK_r) {
          map.init();
          paddle = Paddle();
          ball = Ball();
          particles.reset();
          score = 0;
          gameOver = false;
          state = GameState::PLAYING;
        }
      }
    }

    if (state == GameState::PLAYING) {
      paddle.update(dt, keys);
      ball.update(dt, paddle, map, score, gameOver, particles, audio);
      map.update(dt, gameOver, score, particles);
      particles.update(dt);
      if (gameOver)
        state = GameState::GAME_OVER;
    }

    SDL_SetRenderDrawColor(r, Pal::Outside.r, Pal::Outside.g, Pal::Outside.b,
                           255);
    SDL_RenderClear(r);

    render.drawGrid(map);
    render.drawParticles(particles);
    render.drawPaddle(paddle);
    render.drawBall(ball, map.getTotalTime());
    render.drawScoreboard(score, map.getTotalTime(), ball,
                          map.getReverserTimer());

    if (state == GameState::GAME_OVER)
      render.drawGameOver();

    SDL_RenderPresent(r);
  }

  TTF_Quit();
  SDL_DestroyRenderer(r);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return 0;
}
