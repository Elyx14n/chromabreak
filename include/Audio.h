#pragma once
#include "Constants.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <map>
#include <string>
#include <vector>

class Audio {
public:
  Audio();
  ~Audio();

  Audio(const Audio &) = delete;
  Audio &operator=(const Audio &) = delete;
  bool shuffle = false;

  bool ok() const { return ok_; }

  void playSound(const SFX &sfx);
  void playRandomBreakSingle();
  void playRandomBreakMultiple();
  void playRandomMusic();
  void stopMusic(const int ms = 2000);

  // Visualizer — call updateVisBands(dt) each frame, read via getVisBand(i)
  static constexpr int NUM_VIS_BANDS = 12;
  void updateVisBands(float dt);
  float getVisBand(int i) const { return smoothBands_[i]; }

private:
  bool ok_ = false;
  std::map<std::string, Mix_Chunk *> chunks_;
  Mix_Music *music_ = nullptr;
  int lastMusicIndex_ = -1;
  std::vector<int> unplayed_;
  static inline const SFX playlist[] = {
      MusicLib::ForsakenOne, MusicLib::NeonTwilight, MusicLib::LosingYou};

  // Visualizer internals
  static void postMixCallback(void *udata, Uint8 *stream, int len);
  SDL_SpinLock visLock_ = 0;
  float rawBands_[NUM_VIS_BANDS]    = {};
  float smoothBands_[NUM_VIS_BANDS] = {};
};
