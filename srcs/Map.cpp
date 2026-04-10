#include "Map.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>

void Map::init() {
  memset(cells, 0, sizeof(cells)); // BrickColor::EMPTY == 0
  memset(types, 0, sizeof(types)); // BrickType::NORMAL == 0
  spawnTimer    = 0.f;
  totalTime     = 0.f;
  reverserTimer = 0.f;
  srand((unsigned)time(nullptr));
  BrickType firstTypes[COLS];
  spawnRow(cells[0], firstTypes);
  memcpy(types[0], firstTypes, sizeof(firstTypes));
}

float Map::currentSpawnRate() const {
  if (totalTime >= BLOCK_SPAWN_RATE_INTERVAL * 2.f)
    return BLOCK_SPAWN_RATE_FAST;
  if (totalTime >= BLOCK_SPAWN_RATE_INTERVAL)
    return BLOCK_SPAWN_RATE_MID;
  return BLOCK_SPAWN_RATE_DEFAULT;
}

void Map::floodFill(int startR, int startC, BrickColor color, int &score,
                    ParticleSystem &ps) {
  struct Pos {
    int8_t r, c;
  };
  Pos stack[ROWS * COLS];
  int top = 0;

  // Destroy starting brick
  {
    SDL_Rect cell = cellRect(startR, startC);
    ps.spawnBurst(cell.x + cell.w * 0.5f, cell.y + cell.h * 0.5f,
                  BrickPal::Colors[static_cast<int>(color)]);
  }
  cells[startR][startC] = BrickColor::EMPTY;
  types[startR][startC] = BrickType::NORMAL;
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
      if (types[nr][nc] != BrickType::NORMAL)
        continue; // never chain into special bricks

      SDL_Rect cell = cellRect(nr, nc);
      ps.spawnBurst(cell.x + cell.w * 0.5f, cell.y + cell.h * 0.5f,
                    BrickPal::Colors[static_cast<int>(color)], 8);
      cells[nr][nc] = BrickColor::EMPTY;
      types[nr][nc] = BrickType::NORMAL;
      score += 10;
      stack[top++] = {(int8_t)nr, (int8_t)nc};
    }
  }
}

void Map::bombEffect(int r, int c, BrickColor color, int &score,
                     ParticleSystem &ps) {
  Col brickCol = BrickPal::Colors[static_cast<int>(color)];
  {
    SDL_Rect cell = cellRect(r, c);
    ps.spawnBurst(cell.x + cell.w * 0.5f, cell.y + cell.h * 0.5f, brickCol, 28);
  }
  cells[r][c] = BrickColor::EMPTY;
  types[r][c] = BrickType::NORMAL;
  score += 10;

  // Destroy all remaining normal bricks of the same color
  for (int row = 0; row < ROWS; row++) {
    for (int col = 0; col < COLS; col++) {
      if (cells[row][col] == color && types[row][col] == BrickType::NORMAL) {
        SDL_Rect cell = cellRect(row, col);
        ps.spawnBurst(cell.x + cell.w * 0.5f, cell.y + cell.h * 0.5f, brickCol,
                      8);
        cells[row][col] = BrickColor::EMPTY;
        types[row][col] = BrickType::NORMAL;
        score += 10;
      }
    }
  }
}

void Map::transformerEffect(int r, int c, BrickColor color, int &score,
                            ParticleSystem &ps) {
  Col brickCol = BrickPal::Colors[static_cast<int>(color)];
  {
    SDL_Rect cell = cellRect(r, c);
    ps.spawnBurst(cell.x + cell.w * 0.5f, cell.y + cell.h * 0.5f, brickCol, 32);
  }
  cells[r][c] = BrickColor::EMPTY;
  types[r][c] = BrickType::NORMAL;
  score += 10;

  // Recolor all non-empty bricks within TRANSFORMER_RADIUS (Chebyshev)
  int rMin = std::max(0, r - TRANSFORMER_RADIUS);
  int rMax = std::min(ROWS - 1, r + TRANSFORMER_RADIUS);
  int cMin = std::max(0, c - TRANSFORMER_RADIUS);
  int cMax = std::min(COLS - 1, c + TRANSFORMER_RADIUS);

  for (int row = rMin; row <= rMax; row++) {
	for (int col = cMin; col <= cMax; col++) {
      if (cells[row][col] != BrickColor::EMPTY && types[row][col] == BrickType::NORMAL) {
        cells[row][col] = color;
        types[row][col] = BrickType::NORMAL; // reset any special status
      }
    }
  }
}

void Map::reverserEffect(int r, int c, int &score, ParticleSystem &ps) {
  // Destroy the reverser brick and activate timed reverse-shift mode.
  SDL_Rect cell = cellRect(r, c);
  ps.spawnBurst(cell.x + cell.w * 0.5f, cell.y + cell.h * 0.5f,
                SpecialPal::Reverser, 28);
  cells[r][c]    = BrickColor::EMPTY;
  types[r][c]    = BrickType::NORMAL;
  score         += 10;
  reverserTimer  = POWER_DURATION;
}

void Map::spawnRow(BrickColor *out, BrickType *outTypes) const {
  memset(out, 0, COLS * sizeof(BrickColor));
  memset(outTypes, 0, COLS * sizeof(BrickType)); // NORMAL == 0
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

    for (int i = 0; i < runLen; i++) {
      out[col + i] = color;
      // Assign a random special type at SPECIAL_BRICK_CHANCE % per brick
      outTypes[col + i] = (rand() % 100 < SPECIAL_BRICK_CHANCE)
                              ? static_cast<BrickType>(1 + rand() % 4)
                              : BrickType::NORMAL;
    }

    col += runLen;
  }
}

void Map::update(float dt, bool &gameOver, int &score, ParticleSystem &ps) {
  totalTime  += dt;
  spawnTimer += dt;

  if (reverserTimer > 0.f)
    reverserTimer = std::max(0.f, reverserTimer - dt);

  if (spawnTimer < currentSpawnRate())
    return;
  spawnTimer = 0.f;

  if (reverserTimer > 0.f) {
    // ── Reversed mode ──────────────────────────────────────────────────────
    // No new row is spawned.  Each top-contiguous segment shifts UP by 1
    // (copy direction flipped vs. normal: r+1 → r instead of r-1 → r).
    // The brick at row 0 is destroyed; the slot at `bot` becomes empty.
    for (int c = 0; c < COLS; c++) {
      if (cells[0][c] == BrickColor::EMPTY) continue;

      // Find bottom of top-contiguous segment (same as normal mode).
      int bot = 0;
      while (bot + 1 < ROWS && cells[bot + 1][c] != BrickColor::EMPTY)
        bot++;

      // Destroy the brick being pushed off the top.
      BrickColor dc = cells[0][c];
      SDL_Rect   cr = cellRect(0, c);
      ps.spawnBurst(cr.x + cr.w * 0.5f, cr.y + cr.h * 0.5f,
                    BrickPal::Colors[static_cast<int>(dc)], 10);
      score += 10;

      // Shift segment UP: copy r+1 into r from top to bot-1.
      for (int r = 0; r < bot; r++) {
        cells[r][c] = cells[r + 1][c];
        types[r][c] = types[r + 1][c];
      }
      // Bottom of segment vacated.
      cells[bot][c] = BrickColor::EMPTY;
      types[bot][c] = BrickType::NORMAL;
    }

  } else {
    // ── Normal mode ────────────────────────────────────────────────────────
    // Game-over: any top-contiguous segment whose bottom has reached ROWS-1.
    for (int c = 0; c < COLS; c++) {
      if (cells[0][c] == BrickColor::EMPTY) continue;
      int bot = 0;
      while (bot + 1 < ROWS && cells[bot + 1][c] != BrickColor::EMPTY)
        bot++;
      if (bot == ROWS - 1) {
        gameOver = true;
        return;
      }
    }

    // New bricks enter from the TOP (row 0).
    // Only TOP-contiguous segments shift DOWN by 1.
    BrickColor newRow[COLS];
    BrickType  newTypes[COLS];
    spawnRow(newRow, newTypes);

    for (int c = 0; c < COLS; c++) {
      if (cells[0][c] != BrickColor::EMPTY) {
        int bot = 0;
        while (bot + 1 < ROWS && cells[bot + 1][c] != BrickColor::EMPTY)
          bot++;
        for (int r = bot + 1; r > 0; r--) {
          cells[r][c] = cells[r - 1][c];
          types[r][c] = types[r - 1][c];
        }
      }
      cells[0][c] = newRow[c];
      types[0][c] = newTypes[c];
    }
  }
}
