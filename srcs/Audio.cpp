#include "Audio.h"
#include <algorithm>
#include <cmath>

Audio::Audio() {
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    SDL_Log("SDL audio init failed: %s", SDL_GetError());
    return;
  }
  if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
    SDL_Log("Mix_OpenAudio failed: %s", Mix_GetError());
    return;
  }
  Mix_SetPostMix(postMixCallback, this);
  ok_ = true;
  shuffle = true;
}

Audio::~Audio() {
  Mix_SetPostMix(nullptr, nullptr);
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
    SDL_Log("Failed to load Music: %s | Error: %s", selected.path,
            Mix_GetError());
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

// ── Visualizer

// Compute Goertzel magnitude for a single frequency bin.
// Returns |X[k]| / N, so a full-scale sine at exactly freq gives ~0.5.
static float goertzelMag(const float *samples, int N, float freq,
                         float sampleRate) {
  float k = freq * (float)N / sampleRate;
  float omega = 2.f * 3.14159265f * k / (float)N;
  float coeff = 2.f * cosf(omega);
  float s1 = 0.f, s2 = 0.f;
  for (int i = 0; i < N; i++) {
    float s0 = samples[i] + coeff * s1 - s2;
    s2 = s1;
    s1 = s0;
  }
  float power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
  return sqrtf(fmaxf(0.f, power)) / (float)N;
}

// Center frequencies for the 12 visualizer bands (logarithmic, ~60 Hz–16 kHz)
static constexpr float kVisBandFreqs[Audio::NUM_VIS_BANDS] = {
    60.f,   120.f,  200.f,  350.f,  600.f,   1000.f,
    1700.f, 2800.f, 4600.f, 7000.f, 11000.f, 16000.f};

// Per-band gain — progressively higher for upper bands to compensate
// for music's natural bass-heavy energy distribution and the narrower
// effective bandwidth of low-frequency Goertzel bins.
static constexpr float kVisBandGains[Audio::NUM_VIS_BANDS] = {
    15.f, 15.f,  18.f,  21.f,  38.f,  55.f,
    75.f, 100.f, 160.f, 200.f, 250.f, 300.f};

void Audio::postMixCallback(void *udata, Uint8 *stream, int len) {
  Audio *self = static_cast<Audio *>(udata);

  // Convert Sint16 stereo to float mono
  const Sint16 *src = reinterpret_cast<const Sint16 *>(stream);
  int frames = len / (2 * (int)sizeof(Sint16)); // stereo frames
  int count = std::min(frames, 2048);

  float mono[2048];
  for (int i = 0; i < count; i++)
    mono[i] = (src[i * 2] + src[i * 2 + 1]) * (1.f / 65536.f);

  float bands[NUM_VIS_BANDS];
  for (int b = 0; b < NUM_VIS_BANDS; b++) {
    // 3-point integration: sample at f×0.75, f, f×1.35 and take RMS.
    // Widens each band's capture window, smoothing out single-bin spikiness.
    float f = kVisBandFreqs[b];
    float m0 = goertzelMag(mono, count, f * 0.75f, 44100.f);
    float m1 = goertzelMag(mono, count, f, 44100.f);
    float m2 = goertzelMag(mono, count, f * 1.35f, 44100.f);
    float rms = sqrtf((m0 * m0 + m1 * m1 + m2 * m2) * (1.f / 3.f));
    bands[b] = std::min(rms * kVisBandGains[b], 1.f);
  }

  SDL_AtomicLock(&self->visLock_);
  for (int b = 0; b < NUM_VIS_BANDS; b++)
    self->rawBands_[b] = bands[b];
  SDL_AtomicUnlock(&self->visLock_);
}

void Audio::updateVisBands(float dt) {
  float raw[NUM_VIS_BANDS];
  SDL_AtomicLock(&visLock_);
  for (int b = 0; b < NUM_VIS_BANDS; b++)
    raw[b] = rawBands_[b];
  SDL_AtomicUnlock(&visLock_);

  // Lateral blend across adjacent bands to smooth out isolated spikes
  float lateraled[NUM_VIS_BANDS];
  lateraled[0] = raw[0] * 0.65f + raw[1] * 0.35f;
  for (int b = 1; b < NUM_VIS_BANDS - 1; b++)
    lateraled[b] = raw[b - 1] * 0.2f + raw[b] * 0.6f + raw[b + 1] * 0.2f;
  lateraled[NUM_VIS_BANDS - 1] =
      raw[NUM_VIS_BANDS - 2] * 0.35f + raw[NUM_VIS_BANDS - 1] * 0.65f;

  for (int b = 0; b < NUM_VIS_BANDS; b++) {
    float &s = smoothBands_[b];
    float tgt = lateraled[b];
    // Slower attack (80 ms) reduces spikiness; moderate decay (300 ms)
    float tau = tgt > s ? 0.08f : 0.30f;
    float alpha = 1.f - expf(-dt / tau);
    s += (tgt - s) * alpha;
  }
}