#include "Render.h"
#include "Ball.h"
#include "Map.h"
#include "Paddle.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

TTF_Font *Render::font = nullptr;

// helpers

static void renderText(SDL_Renderer *r, TTF_Font *font, const char *text, int x,
                       int y, SDL_Color color, bool rightAlign = false) {
  if (!font)
    return;
  SDL_Surface *surf = TTF_RenderText_Blended(font, text, color);
  if (!surf)
    return;
  SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
  int tw = surf->w, th = surf->h;
  SDL_FreeSurface(surf);
  if (!tex)
    return;
  SDL_Rect dst = {rightAlign ? x - tw : x, y - th / 2, tw, th};
  SDL_RenderCopy(r, tex, nullptr, &dst);
  SDL_DestroyTexture(tex);
}

void Render::setCol(SDL_Renderer *r, Col c, uint8_t a) {
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, a);
}

static void fill(SDL_Renderer *r, int x, int y, int w, int h) {
  SDL_Rect rc = {x, y, w, h};
  SDL_RenderFillRect(r, &rc);
}

static void filledCircle(SDL_Renderer *r, int cx, int cy, int radius) {
  for (int dy = -radius; dy <= radius; dy++) {
    int dx = (int)sqrtf((float)(radius * radius - dy * dy));
    SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
  }
}

static Col lighter(Col c, int amount) {
  return {(uint8_t)std::min(255, c.r + amount),
          (uint8_t)std::min(255, c.g + amount),
          (uint8_t)std::min(255, c.b + amount)};
}
static Col darker(Col c, int amount) {
  return {(uint8_t)std::max(0, c.r - amount),
          (uint8_t)std::max(0, c.g - amount),
          (uint8_t)std::max(0, c.b - amount)};
}

static std::string formatScore(const int score) {
  struct Suffix {
    double threshold;
    const char *symbol;
  };

  static const Suffix suffixes[] = {
      {1e12, "T"}, // Trillions
      {1e9, "B"},  // Billions
      {1e6, "M"},  // Millions
  };

  for (const auto &s : suffixes) {
    if (abs(score) >= s.threshold) {
      std::stringstream ss;
      ss << std::fixed << std::setprecision(1) << (score / s.threshold)
         << s.symbol;
      return ss.str();
    }
  }

  std::stringstream ss;
  ss.imbue(LOCALE);
  ss << score;
  return ss.str();
}

// scoreboard

void Render::drawScoreboard(SDL_Renderer *r, int score, float totalTime,
                            BrickColor ballColor) {
  // Background bar
  setCol(r, Pal::WallBase);
  fill(r, 0, 0, WIN_W, TOP_MARGIN);

  // Bottom border
  setCol(r, Pal::WallHi);
  fill(r, 0, TOP_MARGIN - 1, WIN_W, 1);

  if (!font)
    return;

  const int midY = TOP_MARGIN / 2;
  const SDL_Color white = {220, 220, 220, 255};
  const SDL_Color dim = {140, 140, 140, 255};

  // --- Score (left) ---
  std::string scoreStr = formatScore(score);
  renderText(r, font, "SCORE", 14, midY, dim);
  int labelW = 0, labelH = 0;
  TTF_SizeText(font, "SCORE", &labelW, &labelH);
  renderText(r, font, scoreStr.c_str(), 14 + labelW + 10, midY, white);

  // --- Timer (center, MM:SS) ---
  int secs = (int)totalTime;
  char timeBuf[8];
  SDL_snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", secs / 60, secs % 60);
  int tw = 0, th = 0;
  TTF_SizeText(font, timeBuf, &tw, &th);
  renderText(r, font, timeBuf, WIN_W / 2 - tw / 2, midY, white);

  // --- Ball color indicator (right) ---
  Col ballCol = BrickPal::Colors[static_cast<int>(ballColor)];
  SDL_SetRenderDrawColor(r, ballCol.r, ballCol.g, ballCol.b, 255);
  filledCircle(r, WIN_W - 20, midY, BALL_R);
  setCol(r, lighter(ballCol, 90));
  filledCircle(r, WIN_W - 23, midY - 3, BALL_R / 3);
}

// grid

void Render::drawGrid(SDL_Renderer *r, const Map &map) {
  Render::setCol(r, Pal::Floor);
  fill(r, 0, TOP_MARGIN, WIN_W, ROWS * TILE);

  for (int row = 0; row < ROWS; row++) {
    for (int col = 0; col < COLS; col++) {
      BrickColor cell = map.cells[row][col];
      SDL_Rect rc = map.cellRect(row, col);

      if (cell == BrickColor::EMPTY) {
        setCol(r, Pal::WallShade);
        SDL_RenderDrawRect(r, &rc);
        continue;
      }

      Col base = BrickPal::Colors[static_cast<int>(cell)];

      // Fill
      setCol(r, base);
      SDL_RenderFillRect(r, &rc);

      // Top highlight
      setCol(r, lighter(base, 70));
      fill(r, rc.x + 2, rc.y + 2, rc.w - 4, 3);
      fill(r, rc.x + 2, rc.y + 2, 3, rc.h - 4);

      // Bottom shadow
      setCol(r, darker(base, 60));
      fill(r, rc.x + 2, rc.y + rc.h - 5, rc.w - 4, 3);
      fill(r, rc.x + rc.w - 5, rc.y + 2, 3, rc.h - 4);

      // Outline
      setCol(r, Pal::WallShade);
      SDL_RenderDrawRect(r, &rc);
    }
  }

  // Divider between grid and paddle zone
  setCol(r, Pal::WallHi);
  fill(r, 0, TOP_MARGIN + ROWS * TILE, WIN_W, 2);
}

void Render::drawPaddle(SDL_Renderer *r, const Paddle &p) {
  SDL_Rect rc = p.rect();

  setCol(r, Pal::PaddleBase);
  SDL_RenderFillRect(r, &rc);

  setCol(r, Pal::PaddleHi);
  fill(r, rc.x + 3, rc.y + 2, rc.w - 6, 3);

  setCol(r, Pal::PaddleShade);
  fill(r, rc.x + 3, rc.y + rc.h - 5, rc.w - 6, 3);
}

void Render::drawBall(SDL_Renderer *r, const Ball &b) {
  Col c = BrickPal::Colors[static_cast<int>(b.color)];

  // Outer glow (dim, 1px larger)
  setCol(r, darker(c, 30), 200);
  filledCircle(r, (int)b.x, (int)b.y, BALL_R + 2);

  // Main body
  setCol(r, c);
  filledCircle(r, (int)b.x, (int)b.y, BALL_R);

  // Specular highlight
  setCol(r, lighter(c, 100));
  filledCircle(r, (int)b.x - 2, (int)b.y - 3, BALL_R / 3);
}

void Render::drawGameOver(SDL_Renderer *r) {
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r, 0, 0, 0, 200);
  SDL_Rect overlay = {0, 0, WIN_W, WIN_H};
  SDL_RenderFillRect(r, &overlay);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

  if (!font)
    return;

  SDL_Color textColor = {255, 80, 80, 255};
  SDL_Color subTextColor = {200, 200, 200, 255};

  SDL_Surface *surfMain = TTF_RenderText_Blended(font, "GAME OVER", textColor);
  SDL_Texture *texMain = SDL_CreateTextureFromSurface(r, surfMain);

  // Position it in the center
  SDL_Rect mainRect = {(WIN_W - surfMain->w) / 2, (WIN_H / 2) - surfMain->h,
                       surfMain->w, surfMain->h};
  SDL_RenderCopy(r, texMain, NULL, &mainRect);

  SDL_Surface *surfSub =
      TTF_RenderText_Blended(font, "Press R to try again", subTextColor);
  SDL_Texture *texSub = SDL_CreateTextureFromSurface(r, surfSub);

  // Position it slightly below the main text
  SDL_Rect subRect = {(WIN_W - surfSub->w) / 2, mainRect.y + mainRect.h + 20,
                      surfSub->w, surfSub->h};
  SDL_RenderCopy(r, texSub, NULL, &subRect);

  SDL_FreeSurface(surfMain);
  SDL_DestroyTexture(texMain);
  SDL_FreeSurface(surfSub);
  SDL_DestroyTexture(texSub);
}
