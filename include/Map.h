#pragma once
#include "Constants.h"
#include <SDL2/SDL.h>
#include <cstdint>

struct Map {
    BrickColor cells[ROWS][COLS];
    float   spawnTimer;
    float   totalTime;

    void init();
    void update(float dt, bool& gameOver);

    void floodFill(int r, int c, BrickColor color, int& score);

    float currentSpawnRate() const;

    SDL_Rect cellRect(int r, int c) const {
        return {c * TILE, TOP_MARGIN + r * TILE, TILE, TILE};
    }

private:
    // Fills `out[COLS]` with a new row using run-based patterns.
    // Reads current cells[0] for vertical colour-hint adjacency.
    void spawnRow(BrickColor* out);
};