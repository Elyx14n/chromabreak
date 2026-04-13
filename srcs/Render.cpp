#include "Render.h"
#include "Audio.h"
#include "Ball.h"
#include "Map.h"
#include "Paddle.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

// ── file-local helpers

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

static void fillRect(SDL_Renderer *r, int x, int y, int w, int h) {
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

// Hue ∈ [0, 6) maps to the colour wheel split into 6 sectors of 60° each:
//   0 Red→Yellow  1 Yellow→Green  2 Green→Cyan  3 Cyan→Blue  4 Blue→Magenta  5
//   Magenta→Red
// hi   = which sector,  f = how far through it (0..1).
// f  = fractional position within the sector (0.0 at entry, 1.0 at exit)
// t  = tint:   the incoming channel, rising  0→255 across the sector (t =
// f*255) q  = quench: the outgoing channel, falling 255→0 across the sector (q
// = (1-f)*255) p  = always 0 — the channel that plays no role in this sector
// One channel stays at 255, one rises (t), one falls (q), one is zero (p) —
// three moving parts are all you need to sweep the full spectrum cleanly.
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
  int cx = rc.x + rc.w / 2;
  int cy = rc.y + rc.h / 2;

  switch (type) {
  case BrickType::BOMB: {
    SDL_SetRenderDrawColor(r, 15, 15, 15, 255);
    filledCircle(r, cx, cy + 2, 8);
    SDL_SetRenderDrawColor(r, 255, 220, 60, 255);
    filledCircle(r, cx + 2, cy - 6, 3);
    SDL_SetRenderDrawColor(r, 180, 120, 30, 255);
    SDL_RenderDrawLine(r, cx + 1, cy - 6, cx + 4, cy - 10);
    break;
  }
  case BrickType::TRANSFORMER: {
    drawSpecialBrickIconBackground(r, rc, color);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
    SDL_RenderDrawLine(r, cx, cy - 7, cx + 7, cy);
    SDL_RenderDrawLine(r, cx + 7, cy, cx, cy + 7);
    SDL_RenderDrawLine(r, cx, cy + 7, cx - 7, cy);
    SDL_RenderDrawLine(r, cx - 7, cy, cx, cy - 7);
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

// ── Render members

Render::Render(SDL_Renderer *r) : r_(r), font_(nullptr) {
  font_ = TTF_OpenFont(FONT_PATH, FONT_SIZE);
  if (!font_)
    SDL_Log("TTF_OpenFont failed: %s", TTF_GetError());
}

Render::~Render() { TTF_CloseFont(font_); }

void Render::setCol(Col c, uint8_t a) {
  SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, a);
}

void Render::drawScoreboard(int score, float totalTime, const Ball &b,
                            float reverserTimer) {
  setCol(Pal::Outside);
  fillRect(r_, 0, 0, WIN_W, TOP_MARGIN);

  setCol(Pal::WallHi);
  fillRect(r_, 0, TOP_MARGIN - 1, WIN_W, 2);

  if (!font_)
    return;

  const int midY = TOP_MARGIN / 2;
  const SDL_Color white = {220, 220, 220, 255};
  const SDL_Color dim = {140, 140, 140, 255};

  std::string scoreStr = formatScore(score);
  renderText(r_, font_, "SCORE", 14, midY, dim);
  int labelW = 0, labelH = 0;
  TTF_SizeText(font_, "SCORE", &labelW, &labelH);
  renderText(r_, font_, scoreStr.c_str(), 14 + labelW + 10, midY, white);

  int secs = (int)totalTime;
  char timeBuf[8];
  SDL_snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", secs / 60, secs % 60);
  int tw = 0, th = 0;
  TTF_SizeText(font_, timeBuf, &tw, &th);
  renderText(r_, font_, timeBuf, WIN_W / 2 - tw / 2, midY, white);

  const int rightPadding = 14;
  int indicatorX = WIN_W - rightPadding;

  auto drawIndicator = [&](const char *label, Col pcol, float timer) {
    SDL_Color psdl = {pcol.r, pcol.g, pcol.b, 255};
    char pbuf[24];
    SDL_snprintf(pbuf, sizeof(pbuf), "%s %ds", label, (int)timer + 1);
    int pw = 0, ph = 0;
    TTF_SizeText(font_, pbuf, &pw, &ph);
    int textX = indicatorX - pw;
    renderText(r_, font_, pbuf, textX, midY, psdl);
    float frac = timer / POWER_DURATION;
    int barW = (int)(pw * frac);
    SDL_SetRenderDrawColor(r_, pcol.r, pcol.g, pcol.b, 160);
    fillRect(r_, textX, midY + ph / 2 + 2, barW, 3);
    indicatorX = textX - 12;
  };

  if (reverserTimer > 0.f)
    drawIndicator("REVERSER", SpecialPal::Reverser, reverserTimer);
  else if (b.getPower() == BallPower::RAINBOW)
    drawIndicator("RAINBOW", Col{220, 180, 255}, b.getPowerTimer());
}

void Render::drawVisualizer(const Audio &audio) {
  SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);

  const int gridH = ROWS * TILE;
  const int gridBottom = TOP_MARGIN + gridH;
  constexpr int kSegs = 8;

  // Linear interpolation blends between two values by a factor t ∈ [0,1]:
  //     result = A + t × (B − A)
  // At t=0 you get A exactly, at t=1 you get B exactly, anything in between is
  // a proportional mix. Here it's applied per channel so the colour transitions
  // smoothly from BarLow (orange) to BarHigh (pink) as t goes bottom→top.
  // The int casts are necessary because uint8_t subtraction wraps on underflow
  auto lerpCol = [&](float t) -> Col {
    return {
        (uint8_t)((int)VisPal::BarLow.r +
                  (int)(t * ((int)VisPal::BarHigh.r - (int)VisPal::BarLow.r))),
        (uint8_t)((int)VisPal::BarLow.g +
                  (int)(t * ((int)VisPal::BarHigh.g - (int)VisPal::BarLow.g))),
        (uint8_t)((int)VisPal::BarLow.b +
                  (int)(t * ((int)VisPal::BarHigh.b - (int)VisPal::BarLow.b))),
    };
  };

  for (int col = 0; col < Audio::NUM_VIS_BANDS; col++) {
    float band = audio.getVisBand(col);
    if (band < 0.002f)
      continue;

    int barH = (int)(band * gridH);
    int x = col * TILE + 1;
    int w = TILE - 2;

    // Divide the bar into kSegs stacked rects, each lerped from BarLow
    // (orange, t=0 at bottom) to BarHigh (pink, t=1 at top).
    // pixBot and pixTop both use (int)(t * barH), so sY + sH lands exactly
    // on the next segment's sY. Computing sY and sH independently would floor
    // them separately and leave 1-px gaps at every transition.
    for (int seg = 0; seg < kSegs; seg++) {
      float t0 = (float)seg / kSegs;
      float t1 = (float)(seg + 1) / kSegs;
      float tMid = (t0 + t1) * 0.5f;

      Col c = lerpCol(tMid);
      int pixBot = (int)(t0 * barH);
      int pixTop = (int)(t1 * barH);
      int sH = pixTop - pixBot;
      if (sH <= 0)
        continue;
      int sY = gridBottom - pixTop;

      SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, 200);
      SDL_Rect rc = {x, sY, w, sH};
      SDL_RenderFillRect(r_, &rc);
    }

    // Peak line at bar top
    int yTop = gridBottom - barH;
    SDL_SetRenderDrawColor(r_, VisPal::BarPeak.r, VisPal::BarPeak.g,
                           VisPal::BarPeak.b, 240);
    SDL_RenderDrawLine(r_, x, yTop, x + w - 1, yTop);
    if (yTop + 1 < gridBottom) {
      SDL_SetRenderDrawColor(r_, VisPal::BarPeak.r, VisPal::BarPeak.g,
                             VisPal::BarPeak.b, 110);
      SDL_RenderDrawLine(r_, x, yTop + 1, x + w - 1, yTop + 1);
    }
  }

  SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_NONE);
}

void Render::drawGrid(const Map &map) {
  float time = map.getTotalTime();

  for (int row = 0; row < ROWS; row++) {
    for (int col = 0; col < COLS; col++) {
      BrickColor cell = map.colorAt(row, col);
      BrickType type = map.typeAt(row, col);
      SDL_Rect rc = map.cellRect(row, col);

      if (cell == BrickColor::EMPTY && type == BrickType::NORMAL) {
        // BLEND mode: result = src*srcA + dst*(1-srcA).
        // At alpha=80 (~31%) this fill is mostly transparent, so the visualizer
        // bars drawn earlier in drawVisualizer bleed through.
        SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r_, Pal::Floor.r, Pal::Floor.g, Pal::Floor.b,
                               80);
        SDL_RenderFillRect(r_, &rc);
        continue;
      }

      Col base = (type != BrickType::NORMAL)
                     ? SpecialPal::Base
                     : BrickPal::Colors[static_cast<int>(cell)];

      // alpha=200 lets a little visualizer colour tint through the brick face.
      // BLENDMODE_NONE below restores full-overwrite for the opaque bevel
      // lines.
      SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(r_, base.r, base.g, base.b, 200);
      SDL_RenderFillRect(r_, &rc);
      SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_NONE);

      setCol(lighter(base, 70));
      fillRect(r_, rc.x + 2, rc.y + 2, rc.w - 4, 3);
      fillRect(r_, rc.x + 2, rc.y + 2, 3, rc.h - 4);

      setCol(darker(base, 60));
      fillRect(r_, rc.x + 2, rc.y + rc.h - 5, rc.w - 4, 3);
      fillRect(r_, rc.x + rc.w - 5, rc.y + 2, 3, rc.h - 4);

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
        setCol(borderCol);
        SDL_RenderDrawRect(r_, &rc);
        SDL_Rect inner = {rc.x + 1, rc.y + 1, rc.w - 2, rc.h - 2};
        SDL_RenderDrawRect(r_, &inner);

        drawSpecialBrickIcon(r_, type, rc, cell);

        float phase = fmodf(time * 0.75f + row * 0.11f + col * 0.19f, 1.0f);
        float sweep = phase * 2.4f - 0.7f;
        if (sweep > 0.f && sweep < 1.f) {
          SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
          int stripCx = rc.x + (int)(sweep * rc.w);
          uint8_t alpha = (uint8_t)(sinf(sweep * 3.14159f) * 140.f);
          SDL_SetRenderDrawColor(r_, 255, 255, 255, alpha);
          for (int dx = -4; dx <= 4; dx++) {
            int sx = stripCx + dx;
            if (sx >= rc.x && sx < rc.x + rc.w)
              SDL_RenderDrawLine(r_, sx, rc.y + 2, sx, rc.y + rc.h - 3);
          }
          SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_NONE);
        }
      } else {
        setCol(Pal::WallShade);
        SDL_RenderDrawRect(r_, &rc);
      }
    }
  }

  setCol(Pal::WallHi);
  fillRect(r_, 0, TOP_MARGIN + ROWS * TILE, WIN_W, 2); // bottom border
  fillRect(r_, 0, TOP_MARGIN, 2, ROWS * TILE);         // left border
  fillRect(r_, WIN_W - 2, TOP_MARGIN, 2, ROWS * TILE); // right border
}

void Render::drawPaddle(const Paddle &p) {
  SDL_Rect rc = p.rect();

  float vx = p.getVX(), vy = p.getVY();
  float speed = sqrtf(vx * vx + vy * vy);
  if (speed > 1.f) {
    // Normalise speed against the paddle's max so t ∈ [0,1] regardless of
    // direction. Diagonal movement produces a slightly higher raw speed but
    // the min() clamp keeps shift from overshooting.
    float norm = speed / PADDLE_X_SPEED;
    float t = std::min(norm, 1.f);
    int shift = (int)(t * 12.f); // max 12 px separation between ghosts

    // (nx, ny) is the unit direction of travel.
    // The ghosts sit behind and ahead of the real paddle along that vector —
    // same idea as lens chromatic aberration where different wavelengths focus
    // at slightly different distances.
    float nx = vx / speed, ny = vy / speed;
    int ox = (int)(nx * shift), oy = (int)(ny * shift);

    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);

    SDL_Rect trail = {rc.x - ox, rc.y - oy, rc.w, rc.h}; // behind
    SDL_SetRenderDrawColor(r_, 255, 60, 160, (uint8_t)(t * 70.f));
    SDL_RenderFillRect(r_, &trail);

    SDL_Rect lead = {rc.x + ox, rc.y + oy, rc.w, rc.h}; // ahead
    SDL_SetRenderDrawColor(r_, 60, 220, 255, (uint8_t)(t * 50.f));
    SDL_RenderFillRect(r_, &lead);

    SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_NONE);
  }

  setCol(Pal::PaddleBase);
  SDL_RenderFillRect(r_, &rc);

  setCol(Pal::PaddleHi);
  fillRect(r_, rc.x + 3, rc.y + 2, rc.w - 6, 3);

  setCol(Pal::PaddleShade);
  fillRect(r_, rc.x + 3, rc.y + rc.h - 5, rc.w - 6, 3);
}

void Render::drawBall(const Ball &b, float time) {
  Col c;
  if (b.getPower() == BallPower::RAINBOW)
    c = hueToRgb(time * 4.f);
  else
    c = BrickPal::Colors[static_cast<int>(b.getColor())];

  setCol(darker(c, 30), 200);
  filledCircle(r_, (int)b.getX(), (int)b.getY(), BALL_R + 2);

  setCol(c);
  filledCircle(r_, (int)b.getX(), (int)b.getY(), BALL_R);

  setCol(lighter(c, 100));
  filledCircle(r_, (int)b.getX() - 2, (int)b.getY() - 3, BALL_R / 3);
}

void Render::drawParticles(const ParticleSystem &ps) { ps.draw(r_); }

void Render::drawGameOver() {
  SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(r_, 0, 0, 0, 200);
  SDL_Rect overlay = {0, 0, WIN_W, WIN_H};
  SDL_RenderFillRect(r_, &overlay);
  SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_NONE);

  if (!font_)
    return;

  SDL_Color textColor = {255, 80, 80, 255};
  SDL_Color subTextColor = {200, 200, 200, 255};

  SDL_Surface *surfMain = TTF_RenderText_Blended(font_, "GAME OVER", textColor);
  if (!surfMain)
    return;
  SDL_Texture *texMain = SDL_CreateTextureFromSurface(r_, surfMain);
  SDL_Rect mainRect = {(WIN_W - surfMain->w) / 2, (WIN_H / 2) - surfMain->h,
                       surfMain->w, surfMain->h};
  SDL_FreeSurface(surfMain);
  if (texMain) {
    SDL_RenderCopy(r_, texMain, nullptr, &mainRect);
    SDL_DestroyTexture(texMain);
  }

  SDL_Surface *surfSub =
      TTF_RenderText_Blended(font_, "Press R to try again", subTextColor);
  if (!surfSub)
    return;
  SDL_Texture *texSub = SDL_CreateTextureFromSurface(r_, surfSub);
  SDL_Rect subRect = {(WIN_W - surfSub->w) / 2, mainRect.y + mainRect.h + 20,
                      surfSub->w, surfSub->h};
  SDL_FreeSurface(surfSub);
  if (texSub) {
    SDL_RenderCopy(r_, texSub, nullptr, &subRect);
    SDL_DestroyTexture(texSub);
  }
}
