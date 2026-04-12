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
  shuffle = true;
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

void Audio::playRandomMusic() {
  if (!ok_ || !shuffle)
    return;

  const int count = static_cast<int>(std::size(playlist));

  // Refill the unplayed pool when exhausted.
  // Exclude the last played track so two cycles never meet on the same song.
  if (unplayed_.empty()) {
    for (int i = 0; i < count; i++)
      if (i != lastMusicIndex_)
        unplayed_.push_back(i);
    // Fisher-Yates shuffle
    for (int i = (int)unplayed_.size() - 1; i > 0; i--) {
      int j = rand() % (i + 1);
      std::swap(unplayed_[i], unplayed_[j]);
    }
  }

  const int nextIndex = unplayed_.back();
  unplayed_.pop_back();
  lastMusicIndex_ = nextIndex;

  if (music_) {
    Mix_HaltMusic();
    Mix_FreeMusic(music_);
    music_ = nullptr;
  }

  const SFX &selected = playlist[nextIndex];
  music_ = Mix_LoadMUS(selected.path);
  if (!music_) {
    SDL_Log("Failed to load Music: %s | Error: %s", selected.path, Mix_GetError());
    return;
  }

  Mix_VolumeMusic(selected.volume);
  Mix_PlayMusic(music_, 1);
}

void Audio::stopMusic(const int ms) {
  if (!ok_)
    return;
  Mix_FadeOutMusic(ms);
  shuffle = false;
}