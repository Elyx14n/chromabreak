#include "Map.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>

void Map::init() {
  memset(cells, 0, sizeof(cells)); // BrickColor::EMPTY == 0
  spawnTimer = 0.f;
  totalTime = 0.f;
  srand((unsigned)time(nullptr));
  spawnRow(cells[0]);
}

float Map::currentSpawnRate() const {
  if (totalTime >= BLOCK_SPAWN_RATE_INTERVAL * 2.f)
    return BLOCK_SPAWN_RATE_FAST;
  if (totalTime >= BLOCK_SPAWN_RATE_INTERVAL)
    return BLOCK_SPAWN_RATE_MID;
  return BLOCK_SPAWN_RATE_DEFAULT;
}

void Map::floodFill(int startR, int startC, BrickColor color, int &score) {
  struct Pos {
    int8_t r, c;
  };
  Pos stack[ROWS * COLS];
  int top = 0;

  cells[startR][startC] = BrickColor::EMPTY;
  score += 10;
  stack[top++] = {(int8_t)startR, (int8_t)startC};

  static const int8_t dr[] = {-1, 1, 0, 0};
  static const int8_t dc[] = {0, 0, -1, 1};

  while (top > 0) {
    Pos p = stack[--top];
    for (int i = 0; i < 4; i++) {
      int nr = p.r + dr[i], nc = p.c + dc[i];
      if ((unsigned)nr >= ROWS || (unsigned)nc >= COLS)
        continue;
      if (cells[nr][nc] != color)
        continue;
      cells[nr][nc] = BrickColor::EMPTY;
      score += 10;
      stack[top++] = {(int8_t)nr, (int8_t)nc};
    }
  }
}

void Map::spawnRow(BrickColor *out) {
  memset(out, 0, COLS * sizeof(BrickColor)); // fill with EMPTY
  int col = 0;
  while (col < COLS) {
    if (rand() % 100 < 15) {
      col++;
      continue;
    }

    int runLen = (rand() % 100 < 70) ? (2 + rand() % 4) : 1;
    runLen = std::min(runLen, COLS - col);

    BrickColor color;
    if (cells[0][col] != BrickColor::EMPTY && rand() % 100 < 35)
      color = cells[0][col];
    else
      color = static_cast<BrickColor>(1 + rand() % 6);

    for (int i = 0; i < runLen; i++)
      out[col + i] = color;

    col += runLen;
  }
}

void Map::update(float dt, bool &gameOver) {
  totalTime += dt;
  spawnTimer += dt;
  if (spawnTimer < currentSpawnRate())
    return;
  spawnTimer = 0.f;

  for (int c = 0; c < COLS; c++) {
    if (cells[0][c] == BrickColor::EMPTY)
      continue;
    int bot = 0;
    while (bot + 1 < ROWS && cells[bot + 1][c] != BrickColor::EMPTY)
      bot++;
    if (bot == ROWS - 1) {
      gameOver = true;
      return;
    }
  }

  BrickColor newRow[COLS];
  spawnRow(newRow);

  for (int c = 0; c < COLS; c++) {
    if (cells[0][c] != BrickColor::EMPTY) {
      int bot = 0;
      while (bot + 1 < ROWS && cells[bot + 1][c] != BrickColor::EMPTY)
        bot++;
      for (int r = bot + 1; r > 0; r--)
        cells[r][c] = cells[r - 1][c];
    }
    cells[0][c] = newRow[c];
  }
}
