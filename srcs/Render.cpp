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

// Simple HSV hue → RGB (s=1, v=1)
static Col hueToRgb(float hue) {
  hue = fmodf(hue, 6.f);
  if (hue < 0.f)
    hue += 6.f;
  int hi = (int)hue;
  float f = hue - (float)hi;
  uint8_t p = 0;
  uint8_t q = (uint8_t)((1.f - f) * 255.f);
  uint8_t t = (uint8_t)(f * 255.f);
  switch (hi % 6) {
  case 0:
    return {255, t, p};
  case 1:
    return {q, 255, p};
  case 2:
    return {p, 255, t};
  case 3:
    return {p, q, 255};
  case 4:
    return {t, p, 255};
  default:
    return {255, p, q};
  }
}

static std::string formatScore(int score) {
  struct Suffix {
    double threshold;
    const char *symbol;
  };
  static const Suffix suffixes[] = {{1e12, "T"}, {1e9, "B"}, {1e6, "M"}};
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

static void drawSpecialBrickIconBackground(SDL_Renderer *r, SDL_Rect rc,
                                           BrickColor color) {
  const int inset = 10;
  SDL_Rect smallRc = {rc.x + inset, rc.y + inset, rc.w - (inset * 2),
                      rc.h - (inset * 2)};
  const Col &bg = BrickPal::Colors[static_cast<int>(color)];
  SDL_SetRenderDrawColor(r, bg.r, bg.g, bg.b, 255);
  SDL_RenderFillRect(r, &smallRc);
}

static void drawSpecialBrickIcon(SDL_Renderer *r, BrickType type, SDL_Rect rc,
                                 BrickColor color) {
  if (type != BrickType::RAINBOW && type != BrickType::REVERSER)
    drawSpecialBrickIconBackground(r, rc, color);

  int cx = rc.x + rc.w / 2;
  int cy = rc.y + rc.h / 2;

  switch (type) {
  case BrickType::BOMB: {
    // Dark filled circle with a bright spark
    SDL_SetRenderDrawColor(r, 15, 15, 15, 255);
    filledCircle(r, cx, cy + 2, 8);
    SDL_SetRenderDrawColor(r, 255, 220, 60, 255);
    filledCircle(r, cx + 2, cy - 6, 3);
    // fuse line
    SDL_SetRenderDrawColor(r, 180, 120, 30, 255);
    SDL_RenderDrawLine(r, cx + 1, cy - 6, cx + 4, cy - 10);
    break;
  }
  case BrickType::TRANSFORMER: {
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    // Outer Diamond
    SDL_RenderDrawLine(r, cx, cy - 7, cx + 7, cy);
    SDL_RenderDrawLine(r, cx + 7, cy, cx, cy + 7);
    SDL_RenderDrawLine(r, cx, cy + 7, cx - 7, cy);
    SDL_RenderDrawLine(r, cx - 7, cy, cx, cy - 7);

    // Inner Diamond
    SDL_RenderDrawLine(r, cx, cy - 3, cx + 3, cy);
    SDL_RenderDrawLine(r, cx + 3, cy, cx, cy + 3);
    SDL_RenderDrawLine(r, cx, cy + 3, cx - 3, cy);
    SDL_RenderDrawLine(r, cx - 3, cy, cx, cy - 3);
    break;
  }
  case BrickType::RAINBOW: {
    for (int i = 1; i < static_cast<int>(BrickColor::COLOR_COUNT); i++) {
      int sy = cy - 3 + (i - 1);
      const Col &stripeColor = BrickPal::Colors[i];
      SDL_SetRenderDrawColor(r, stripeColor.r, stripeColor.g, stripeColor.b,
                             255);
      SDL_RenderDrawLine(r, cx - 9, sy, cx + 9, sy);
    }
    break;
  }
  case BrickType::REVERSER: {
    // Upward double chevron (cyan)
    SDL_SetRenderDrawColor(r, 60, 230, 255, 255);
    for (int i = 0; i <= 6; i++) {
      SDL_RenderDrawPoint(r, cx - i, cy + i - 1);
      SDL_RenderDrawPoint(r, cx + i, cy + i - 1);
      SDL_RenderDrawPoint(r, cx - i, cy + i);
      SDL_RenderDrawPoint(r, cx + i, cy + i);
    }
    for (int i = 0; i <= 6; i++) {
      SDL_RenderDrawPoint(r, cx - i, cy + i - 8);
      SDL_RenderDrawPoint(r, cx + i, cy + i - 8);
      SDL_RenderDrawPoint(r, cx - i, cy + i - 7);
      SDL_RenderDrawPoint(r, cx + i, cy + i - 7);
    }
    break;
  }
  default:
    break;
  }
}

void Render::drawScoreboard(SDL_Renderer *r, int score, float totalTime,
                            const Ball &b, float reverserTimer) {
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

  // --- Active power / reverser indicator (right-aligned) ---
  const int rightPadding = 14;
  int indicatorX = WIN_W - rightPadding; // right edge for right-aligned text

  auto drawIndicator = [&](const char *label, Col pcol, float timer) {
    SDL_Color psdl = {pcol.r, pcol.g, pcol.b, 255};
    char pbuf[24];
    SDL_snprintf(pbuf, sizeof(pbuf), "%s %ds", label, (int)timer + 1);
    int pw = 0, ph = 0;
    TTF_SizeText(font, pbuf, &pw, &ph);
    int textX = indicatorX - pw;
    renderText(r, font, pbuf, textX, midY, psdl);
    // Drain bar
    float frac = timer / POWER_DURATION;
    int barW = (int)(pw * frac);
    SDL_SetRenderDrawColor(r, pcol.r, pcol.g, pcol.b, 160);
    fill(r, textX, midY + ph / 2 + 2, barW, 3);
    indicatorX = textX - 12; // next indicator stacks to the left
  };

  if (reverserTimer > 0.f)
    drawIndicator("REVERSER", SpecialPal::Reverser, reverserTimer);
  else if (b.power == BallPower::RAINBOW)
    drawIndicator("RAINBOW", Col{220, 180, 255}, b.powerTimer);
}

void Render::drawGrid(SDL_Renderer *r, const Map &map) {
  float time = map.totalTime;

  setCol(r, Pal::Floor);
  fill(r, 0, TOP_MARGIN, WIN_W, ROWS * TILE);

  for (int row = 0; row < ROWS; row++) {
    for (int col = 0; col < COLS; col++) {
      BrickColor cell = map.cells[row][col];
      BrickType type = map.types[row][col];
      SDL_Rect rc = map.cellRect(row, col);

      if (cell == BrickColor::EMPTY) {
        setCol(r, Pal::WallShade);
        SDL_RenderDrawRect(r, &rc);
        continue;
      }

      Col base = (type != BrickType::NORMAL)
                     ? SpecialPal::Base
                     : BrickPal::Colors[static_cast<int>(cell)];

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

      if (type != BrickType::NORMAL) {
        Col borderCol;
        switch (type) {
        case BrickType::BOMB:
          borderCol = SpecialPal::Bomb;
          break;
        case BrickType::TRANSFORMER:
          borderCol = SpecialPal::Transformer;
          break;
        case BrickType::RAINBOW:
          borderCol = SpecialPal::Rainbow;
          break;
        case BrickType::REVERSER:
          borderCol = SpecialPal::Reverser;
          break;
        default:
          borderCol = Pal::WallShade;
          break;
        }
        setCol(r, borderCol);
        SDL_RenderDrawRect(r, &rc);
        // Inner border for extra visibility
        SDL_Rect inner = {rc.x + 1, rc.y + 1, rc.w - 2, rc.h - 2};
        SDL_RenderDrawRect(r, &inner);

        drawSpecialBrickIcon(r, type, rc, cell);

        // Shine sweep animation
        float phase = fmodf(time * 0.75f + row * 0.11f + col * 0.19f, 1.0f);
        float sweep = phase * 2.4f - 0.7f; // enters from left, exits right
        if (sweep > 0.f && sweep < 1.f) {
          SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
          int stripCx = rc.x + (int)(sweep * rc.w);
          uint8_t alpha = (uint8_t)(sinf(sweep * 3.14159f) * 140.f);
          SDL_SetRenderDrawColor(r, 255, 255, 255, alpha);
          for (int dx = -4; dx <= 4; dx++) {
            int sx = stripCx + dx;
            if (sx >= rc.x && sx < rc.x + rc.w)
              SDL_RenderDrawLine(r, sx, rc.y + 2, sx, rc.y + rc.h - 3);
          }
          SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        }
      } else {
        setCol(r, Pal::WallShade);
        SDL_RenderDrawRect(r, &rc);
      }
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

void Render::drawBall(SDL_Renderer *r, const Ball &b, float time) {
  Col c;
  if (b.power == BallPower::RAINBOW)
    c = hueToRgb(time * 4.f);
  else
    c = BrickPal::Colors[static_cast<int>(b.color)];

  // Outer glow (dim, 1 px larger)
  setCol(r, darker(c, 30), 200);
  filledCircle(r, (int)b.x, (int)b.y, BALL_R + 2);

  // Main body
  setCol(r, c);
  filledCircle(r, (int)b.x, (int)b.y, BALL_R);

  // Specular highlight
  setCol(r, lighter(c, 100));
  filledCircle(r, (int)b.x - 2, (int)b.y - 3, BALL_R / 3);
}

void Render::drawParticles(SDL_Renderer *r, const ParticleSystem &ps) {
  ps.draw(r);
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
  if (!surfMain) return;
  SDL_Texture *texMain = SDL_CreateTextureFromSurface(r, surfMain);
  SDL_Rect mainRect = {(WIN_W - surfMain->w) / 2, (WIN_H / 2) - surfMain->h,
                       surfMain->w, surfMain->h};
  SDL_FreeSurface(surfMain); // free surface before texture use as its no longer needed
  if (texMain) {
    SDL_RenderCopy(r, texMain, nullptr, &mainRect);
    SDL_DestroyTexture(texMain);
  }

  SDL_Surface *surfSub =
      TTF_RenderText_Blended(font, "Press R to try again", subTextColor);
  if (!surfSub) return;
  SDL_Texture *texSub = SDL_CreateTextureFromSurface(r, surfSub);
  SDL_Rect subRect = {(WIN_W - surfSub->w) / 2, mainRect.y + mainRect.h + 20,
                      surfSub->w, surfSub->h};
  SDL_FreeSurface(surfSub);
  if (texSub) {
    SDL_RenderCopy(r, texSub, nullptr, &subRect);
    SDL_DestroyTexture(texSub);
  }
}
