#include "Constants.h"
#include "Paddle.h"
#include "Ball.h"
#include "Map.h"
#include "Render.h"
#include <SDL2/SDL.h>
#include <algorithm>
#include <SDL2/SDL_ttf.h>

enum GameState { PLAYING, GAME_OVER };

int main(int, char* []) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    SDL_Window* w = SDL_CreateWindow(
        "Chromabreak",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, 0);
    SDL_Renderer* r = SDL_CreateRenderer(
        w, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    Render::font = TTF_OpenFont(FONT_PATH, FONT_SIZE);

    Map    map;
    Paddle paddle;
    Ball   ball;
    int score = 0;
    bool   gameOver = false;
    GameState state = PLAYING;

    map.init();

    Uint64 prev    = SDL_GetTicks64();
    bool   running = true;
    SDL_Event event;

    while (running) {
        Uint64 now = SDL_GetTicks64();
        float  dt  = (now - prev) / 1000.f;
        prev = now;
        dt = std::min(dt, 0.05f); // cap to 50 ms to avoid tunneling on lag spike

        const uint8_t* keys = SDL_GetKeyboardState(nullptr);

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                running = false;

            if (event.type == SDL_KEYDOWN) {
                if (state == PLAYING && event.key.keysym.sym == SDLK_SPACE) {
                    ball.cycleColor();
                }
                if (state == GAME_OVER && event.key.keysym.sym == SDLK_r) {
                    map.init();
                    paddle   = Paddle();
                    ball     = Ball();
                    score    = 0;
                    gameOver = false;
                    state    = PLAYING;
                }
            }
        }

        if (state == PLAYING) {
            paddle.update(dt, keys);
            ball.update(dt, paddle, map, score, gameOver);
            map.update(dt, gameOver);
            if (gameOver) state = GAME_OVER;
        }

        // --- Render ---
        SDL_SetRenderDrawColor(r,
            Pal::Outside.r, Pal::Outside.g, Pal::Outside.b, 255);
        SDL_RenderClear(r);

        Render::drawGrid(r, map);
        Render::drawPaddle(r, paddle);
        Render::drawBall(r, ball);
        Render::drawScoreboard(r, score, map.totalTime, ball.color);

        if (state == GAME_OVER)
            Render::drawGameOver(r);

        SDL_RenderPresent(r);
    }

    TTF_CloseFont(Render::font);
    TTF_Quit();
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(w);
    SDL_Quit();
    return 0;
}
