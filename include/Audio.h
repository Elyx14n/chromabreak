#pragma once
#include "Constants.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <map>
#include <string>

class Audio {
public:
  Audio();
  ~Audio();

  Audio(const Audio &) = delete;
  Audio &operator=(const Audio &) = delete;

  bool ok() const { return ok_; }

  void playSound(const SFX &sfx);
  void playRandomBreakSingle();
  void playRandomBreakMultiple();

private:
  bool ok_ = false;
  std::map<std::string, Mix_Chunk *> chunks_;
  Mix_Music *music_ = nullptr;
};
