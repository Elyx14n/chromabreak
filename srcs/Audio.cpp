/*  Audio visualizer
    ─────────────────────────────────────────────────────────────────────────
    GOAL: read the music being played and produce 12 height values (0..1),
    one per column, that rise and fall with the energy at different frequencies.

    STEP 1 — Capture audio (postMixCallback)
    SDL_mixer calls postMixCallback on a background audio thread every time it
    fills a buffer (~2048 stereo frames, so roughly every 46 ms). That buffer
    is the final mixed output — music plus SFX — as raw 16-bit signed integers.
    We convert it to mono floats in [-1, 1] by averaging left + right channels
    and dividing by 32768 (the max value of a signed 16-bit integer).

    STEP 2 — Measure energy per frequency band (Goertzel algorithm)
    We want to know "how loud is the bass?", "how loud is the midrange?", etc.
    The textbook answer is a Fast Fourier Transform (FFT), which decomposes the
    signal into all frequencies at once. We only need 12 specific frequencies,
    so we use Goertzel instead — it computes one DFT bin at a time and is far
    cheaper for a small, fixed set of targets.

    Goertzel works like a resonator tuned to one frequency. Feed it N samples
    and it accumulates energy only at that frequency, ignoring everything else.
    The recurrence is:
        s0 = x[n]  +  2·cos(2π·k/N)·s1  −  s2
    where k = freq·N/sampleRate (which bin we want), and s1/s2 are the two
    previous state values. After all N samples, the power at that frequency is:
        power = s1² + s2² − coeff·s1·s2
    Taking √power and dividing by N gives a normalised magnitude where a
    full-scale sine at exactly that frequency returns ~0.5.

    Because a single bin is very narrow — music rarely sits precisely on our
    12 chosen centre frequencies — we sample each band at three points
    (f×0.75, f, f×1.35) and take the RMS of all three. RMS (root-mean-square)
    is just √((m0²+m1²+m2²)/3), the standard way to average magnitudes without
    positive and negative values cancelling out. This widens each band's window
    so it catches nearby energy too.

    The 12 centre frequencies are spaced logarithmically (60 Hz → 16 kHz)
    because that matches how humans perceive pitch — equal ratios sound equally
    spaced, not equal Hz steps.

    Per-band gain factors amplify the result before clamping to [0,1]. Low bands
    need less gain because bass carries a lot of energy in most music; high bands
    need much more because their energy is naturally quieter.

    STEP 3 — Hand data to the main thread safely (spinlock)
    The callback runs on the audio thread; the game loop runs on the main thread.
    Both touch rawBands_ — without coordination that's a data race. We use
    SDL_AtomicLock (a spinlock) to guard the 12-float write/read. A spinlock
    busy-loops until the other thread releases it, which is fine here because
    the critical section is trivially short.

    STEP 4 — Smooth the values each frame (updateVisBands)
    Raw Goertzel values jump around a lot frame-to-frame. Two smoothing passes
    are applied on the main thread before the values reach the renderer:

      a) Lateral smoothing — each band borrows 20% from each neighbour
         (weighted average: 0.2·left + 0.6·self + 0.2·right). This prevents
         a single column from spiking alone while its neighbours stay flat.

      b) Temporal smoothing — exponential moving average each frame:
             s += (target − s) · alpha,   alpha = 1 − e^(−dt/tau)
         Think of s as a value that chases the target. When tau is small,
         alpha is close to 1 and s jumps fast (the bar rises quickly on a beat).
         When tau is large, alpha is close to 0 and s moves slowly (the bar
         decays gradually after the beat ends). We use a short tau (0.08 s)
         on the way up and a longer one (0.30 s) on the way down.

    STEP 5 — Render
    Render::drawVisualizer() reads the 12 smoothed values and maps each one
    to a bar height (value × grid height in pixels). That's all — the rest is
    just drawing coloured rectangles with alpha blending.

    FORMULAS
      Mono conversion:    mono[i]   = (L + R) / 65536
      Goertzel bin:       k         = freq × N / sampleRate
      Goertzel coeff:     coeff     = 2 × cos(2π × k / N)
      Goertzel recurrence:s0        = x[n] + coeff×s1 − s2
      Goertzel power:     power     = s1² + s2² − coeff×s1×s2
      Normalised magnitude:         = √power / N
      3-point RMS:        rms       = √((m0² + m1² + m2²) / 3)
      Lateral blend:      out[b]    = 0.2×in[b-1] + 0.6×in[b] + 0.2×in[b+1]
      Smoothing alpha:    alpha     = 1 − e^(−dt / tau)
      Smoothing step:     s        += (target − s) × alpha
      Bar height (px):    barH      = smoothBand × gridHeightPixels
    ─────────────────────────────────────────────────────────────────────────*/

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

// Goertzel computes a single DFT bin — the energy at one specific frequency.
// A full FFT computes every bin at once; here we only need 12, so Goertzel is
// cheaper and has no power-of-two constraint on the input length.
//
// The recurrence  s0 = x[n] + 2·cos(2π·k/N)·s1 − s2  is a second-order IIR
// filter tuned to bin k. It resonates at that frequency, accumulating energy
// as samples are fed in. After N samples, s1²+s2²−coeff·s1·s2 equals |X[k]|².
// Dividing by N normalises so a full-scale sine at exactly freq returns ~0.5.
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
    // A single Goertzel bin is very narrow — music rarely sits exactly on
    // our centre frequency, so we'd get spiky near-zero reads most of the time.
    // Sampling at f×0.75, f, and f×1.35 and taking the RMS widens the effective
    // window without needing a full band-pass filter.
    float f = kVisBandFreqs[b];
    float m0 = goertzelMag(mono, count, f * 0.75f, 44100.f);
    float m1 = goertzelMag(mono, count, f, 44100.f);
    float m2 = goertzelMag(mono, count, f * 1.35f, 44100.f);
    float rms = sqrtf((m0 * m0 + m1 * m1 + m2 * m2) * (1.f / 3.f));
    bands[b] = std::min(rms * kVisBandGains[b], 1.f);
  }

  // SDL_mixer fires this callback on a dedicated audio thread, not the main
  // thread. rawBands_ is written here and read in updateVisBands() — without a
  // lock that's a data race (torn reads, undefined behaviour). SDL_AtomicLock
  // is a spinlock: it busy-waits until the lock is free. The critical section
  // is 12 float copies so the spin time is negligible compared to an audio
  // buffer period (~46 ms).
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

  // Each band borrows a little from its neighbours before temporal smoothing.
  // This stops one column from spiking while the ones beside it stay flat,
  // which looks more like a continuous spectrum and less like a noisy meter.
  float lateraled[NUM_VIS_BANDS];
  lateraled[0] = raw[0] * 0.65f + raw[1] * 0.35f;
  for (int b = 1; b < NUM_VIS_BANDS - 1; b++)
    lateraled[b] = raw[b - 1] * 0.2f + raw[b] * 0.6f + raw[b + 1] * 0.2f;
  lateraled[NUM_VIS_BANDS - 1] =
      raw[NUM_VIS_BANDS - 2] * 0.35f + raw[NUM_VIS_BANDS - 1] * 0.65f;

  for (int b = 0; b < NUM_VIS_BANDS; b++) {
    float &s = smoothBands_[b];
    float tgt = lateraled[b];
    // Exponential moving average: alpha = 1 − e^(−dt/tau).
    // Small tau → alpha near 1 → s jumps quickly to target (fast attack).
    // Large tau → alpha near 0 → s drifts down slowly (slow decay).
    float tau = tgt > s ? 0.08f : 0.30f;
    float alpha = 1.f - expf(-dt / tau);
    s += (tgt - s) * alpha;
  }
}