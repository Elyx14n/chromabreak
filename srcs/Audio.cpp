#include "Audio.h"

Audio::Audio() {
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    SDL_Log("SDL audio init failed: %s", SDL_GetError());
    return;
  }
  if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
    SDL_Log("Mix_OpenAudio failed: %s", Mix_GetError());
    return;
  }
  ok_ = true;
}

Audio::~Audio() {
  for (auto &[path, chunk] : chunks_)
    Mix_FreeChunk(chunk);
  if (music_)
    Mix_FreeMusic(music_);
  Mix_CloseAudio();
  Mix_Quit();
}

void Audio::playSound(const SFX &sfx) {
  if (!ok_)
    return;
  if (chunks_.find(sfx.path) == chunks_.end()) {
    chunks_[sfx.path] = Mix_LoadWAV(sfx.path);
    if (!chunks_[sfx.path]) {
      SDL_Log("Failed to load SFX: %s | Error: %s", sfx.path, Mix_GetError());
      return;
    }
  }
  Mix_VolumeChunk(chunks_[sfx.path], sfx.volume);
  Mix_PlayChannel(-1, chunks_[sfx.path], 0);
}

void Audio::playRandomBreakSingle() {
  const SFX sounds[] = {SFXLib::Break1, SFXLib::Break2, SFXLib::Break3,
                        SFXLib::Break4};
  playSound(sounds[rand() % 4]);
}

void Audio::playRandomBreakMultiple() {
  const SFX sounds[] = {SFXLib::BreakMultiple1, SFXLib::BreakMultiple2};
  playSound(sounds[rand() % 2]);
}
