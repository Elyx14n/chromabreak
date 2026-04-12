#include "Map.h"
#include "Audio.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>

void Map::init() {
  memset(cells_, 0, sizeof(cells_)); // BrickColor::EMPTY == 0
  memset(types_, 0, sizeof(types_)); // BrickType::NORMAL == 0
  spawnTimer_ = 0.f;
  totalTime_ = 0.f;
  reverserTimer_ = 0.f;
  srand((unsigned)time(nullptr));
  BrickType firstTypes[COLS];
  spawnRow(cells_[0], firstTypes);
  memcpy(types_[0], firstTypes, sizeof(firstTypes));
}

float Map::currentSpawnRate() const {
  if (totalTime_ >= BLOCK_SPAWN_RATE_INTERVAL * 2.f)
    return BLOCK_SPAWN_RATE_FAST;
  if (totalTime_ >= BLOCK_SPAWN_RATE_INTERVAL)
    return BLOCK_SPAWN_RATE_MID;
  return BLOCK_SPAWN_RATE_DEFAULT;
}

int Map::floodFill(int startR, int startC, BrickColor color, int &score,
                   ParticleSystem &ps) {
  struct Pos {
    int8_t r, c;
  };
  Pos stack[ROWS * COLS];
  int top = 0;
  int currScore = 0;

  {
    SDL_Rect cell = cellRect(startR, startC);
    ps.spawnBurst(cell.x + cell.w * 0.5f, cell.y + cell.h * 0.5f,
                  BrickPal::Colors[static_cast<int>(color)]);
  }
  cells_[startR][startC] = BrickColor::EMPTY;
  types_[startR][startC] = BrickType::NORMAL;
  score += BASE_SCORE;
  currScore += BASE_SCORE;
  stack[top++] = {(int8_t)startR, (int8_t)startC};

  static const int8_t dr[] = {-1, 1, 0, 0};
  static const int8_t dc[] = {0, 0, -1, 1};

  while (top > 0) {
    Pos p = stack[--top];
    for (int i = 0; i < 4; i++) {
      int nr = p.r + dr[i], nc = p.c + dc[i];
      if ((unsigned)nr >= ROWS || (unsigned)nc >= COLS)
        continue;
      if (cells_[nr][nc] != color)
        continue;
      if (types_[nr][nc] != BrickType::NORMAL)
        continue;

      SDL_Rect cell = cellRect(nr, nc);
      ps.spawnBurst(cell.x + cell.w * 0.5f, cell.y + cell.h * 0.5f,
                    BrickPal::Colors[static_cast<int>(color)], 8);
      cells_[nr][nc] = BrickColor::EMPTY;
      types_[nr][nc] = BrickType::NORMAL;
      score += BASE_SCORE;
      currScore += BASE_SCORE;
      stack[top++] = {(int8_t)nr, (int8_t)nc};
    }
  }
  return currScore;
}

void Map::bombEffect(int r, int c, BrickColor color, int &score,
                     ParticleSystem &ps) {
  Col brickCol = BrickPal::Colors[static_cast<int>(color)];
  {
    SDL_Rect cell = cellRect(r, c);
    ps.spawnBurst(cell.x + cell.w * 0.5f, cell.y + cell.h * 0.5f, brickCol, 28);
  }
  cells_[r][c] = BrickColor::EMPTY;
  types_[r][c] = BrickType::NORMAL;
  score += BASE_SCORE;

  int rMin = std::max(0, r - BOMB_RADIUS);
  int rMax = std::min(ROWS - 1, r + BOMB_RADIUS);
  int cMin = std::max(0, c - BOMB_RADIUS);
  int cMax = std::min(COLS - 1, c + BOMB_RADIUS);

  for (int row = rMin; row <= rMax; row++) {
    for (int col = cMin; col <= cMax; col++) {
      if (types_[row][col] == BrickType::NORMAL) {
        SDL_Rect cell = cellRect(row, col);
        ps.spawnBurst(cell.x + cell.w * 0.5f, cell.y + cell.h * 0.5f, brickCol,
                      8);
        cells_[row][col] = BrickColor::EMPTY;
        types_[row][col] = BrickType::NORMAL;
        score += BASE_SCORE;
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
  cells_[r][c] = BrickColor::EMPTY;
  types_[r][c] = BrickType::NORMAL;
  score += BASE_SCORE;

  int rMin = std::max(0, r - TRANSFORMER_RADIUS);
  int rMax = std::min(ROWS - 1, r + TRANSFORMER_RADIUS);
  int cMin = std::max(0, c - TRANSFORMER_RADIUS);
  int cMax = std::min(COLS - 1, c + TRANSFORMER_RADIUS);

  for (int row = rMin; row <= rMax; row++) {
    for (int col = cMin; col <= cMax; col++) {
      if (cells_[row][col] != BrickColor::EMPTY)
        cells_[row][col] = color;
    }
  }
}

void Map::reverserEffect(int r, int c, int &score, ParticleSystem &ps) {
  SDL_Rect cell = cellRect(r, c);
  ps.spawnBurst(cell.x + cell.w * 0.5f, cell.y + cell.h * 0.5f,
                SpecialPal::Reverser, 28);
  cells_[r][c] = BrickColor::EMPTY;
  types_[r][c] = BrickType::NORMAL;
  score += BASE_SCORE;
  reverserTimer_ = POWER_DURATION;
}

int Map::countNormalBricks() const {
  int count = 0;
  for (int r = 0; r < ROWS; r++)
    for (int c = 0; c < COLS; c++)
      if (cells_[r][c] != BrickColor::EMPTY && types_[r][c] == BrickType::NORMAL)
        count++;
  return count;
}

BrickType Map::spawnBrickType(BrickColor *out, BrickType *outTypes,
                              int pos) const {
  if (rand() % 100 >= SPECIAL_BRICK_CHANCE)
    return BrickType::NORMAL;

  for (int dc = -SPECIAL_ADJACENCY_RADIUS; dc <= SPECIAL_ADJACENCY_RADIUS; dc++) {
    int nc = pos + dc;
    if (nc < 0 || nc >= COLS)
      continue;
    if (outTypes[nc] != BrickType::NORMAL)
      return BrickType::NORMAL;
    for (int dr = 0; dr < SPECIAL_ADJACENCY_ROW_DEPTH; dr++)
      if (types_[dr][nc] != BrickType::NORMAL)
        return BrickType::NORMAL;
  }

	if (countNormalBricks() < SPECIAL_BRICK_MIN_COLORED_COUNT)
    return BrickType::NORMAL;

  int roll = rand() % 4;
  return roll == 0   ? BrickType::BOMB
         : roll == 1 ? BrickType::TRANSFORMER
         : roll == 2 ? BrickType::RAINBOW
                     : BrickType::REVERSER;
}

void Map::spawnRow(BrickColor *out, BrickType *outTypes) const {
  memset(out, 0, COLS * sizeof(BrickColor));
  memset(outTypes, 0, COLS * sizeof(BrickType));
  int col = 0;
  while (col < COLS) {
    if (rand() % 100 < 15) {
      col++;
      continue;
    }

    int runLen = (rand() % 100 < 70) ? (2 + rand() % 4) : 1;
    runLen = std::min(runLen, COLS - col);

    BrickColor color;
    if (cells_[0][col] != BrickColor::EMPTY && rand() % 100 < 35)
      color = cells_[0][col];
    else
      color = static_cast<BrickColor>(1 + rand() % 6);

    for (int i = 0; i < runLen; i++) {
      int pos = col + i;
      outTypes[col + i] = spawnBrickType(out, outTypes, pos);
      out[col + i] = color;
    }

    col += runLen;
  }
}

void Map::update(float dt, bool &gameOver, int &score, ParticleSystem &ps,
                 Audio &a) {
  totalTime_ += dt;
  spawnTimer_ += dt;
  int currScore = 0;

  if (reverserTimer_ > 0.f)
    reverserTimer_ = std::max(0.f, reverserTimer_ - dt);

  if (spawnTimer_ < currentSpawnRate())
    return;
  spawnTimer_ = 0.f;

  if (reverserTimer_ > 0.f) {
    bool hasRowShift = false;
    for (int c = 0; c < COLS; c++) {
      if (cells_[0][c] != BrickColor::EMPTY) {
        BrickColor dc = cells_[0][c];
        SDL_Rect cr = cellRect(0, c);
        ps.spawnBurst(cr.x + cr.w * 0.5f, cr.y + cr.h * 0.5f,
                      BrickPal::Colors[static_cast<int>(dc)], 10);
        score += BASE_SCORE;
        currScore += BASE_SCORE;
        hasRowShift = true;
      }

      for (int r = 0; r < ROWS - 1; r++) {
        cells_[r][c] = cells_[r + 1][c];
        types_[r][c] = types_[r + 1][c];
      }
      cells_[ROWS - 1][c] = BrickColor::EMPTY;
      types_[ROWS - 1][c] = BrickType::NORMAL;
    }
    if (hasRowShift)
      a.playSound(SFXLib::RowShift);
    if (currScore > BASE_SCORE)
      a.playRandomBreakSingle();
    if (currScore > BASE_SCORE * 3)
      a.playRandomBreakMultiple();
  } else {
    for (int c = 0; c < COLS; c++) {
      if (cells_[0][c] == BrickColor::EMPTY)
        continue;
      int bot = 0;
      while (bot + 1 < ROWS && cells_[bot + 1][c] != BrickColor::EMPTY)
        bot++;
      if (bot == ROWS - 1) {
        gameOver = true;
        return;
      }
    }

    BrickColor newRow[COLS];
    BrickType newTypes[COLS];
    spawnRow(newRow, newTypes);
    a.playSound(SFXLib::RowShift);

    for (int c = 0; c < COLS; c++) {
      if (cells_[0][c] != BrickColor::EMPTY) {
        int bot = 0;
        while (bot + 1 < ROWS && cells_[bot + 1][c] != BrickColor::EMPTY)
          bot++;
        for (int r = bot + 1; r > 0; r--) {
          cells_[r][c] = cells_[r - 1][c];
          types_[r][c] = types_[r - 1][c];
        }
      }
      cells_[0][c] = newRow[c];
      types_[0][c] = newTypes[c];
    }
  }
}
