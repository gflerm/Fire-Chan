#include "audio/AudioFeedback.h"

#include <M5Unified.h>

namespace firechan {

void AudioFeedback::begin(uint8_t volumePercent, bool muted) {
  M5.Mic.end();
  M5.Speaker.begin();
  // Cap expression cues below the speaker's full hardware range.
  hardwareVolume_ = static_cast<uint8_t>(volumePercent * 96U / 100U);
  // Speech needs more headroom than the deliberately quiet expression cues.
  speechVolume_ = static_cast<uint8_t>(volumePercent * 176U / 100U);
  M5.Speaker.setVolume(hardwareVolume_);
  muted_ = muted;
  Serial.printf("[AUDIO] non-blocking cues ready volume=%u%% speech_level=%u muted=%s\n",
                volumePercent, speechVolume_, muted_ ? "true" : "false");
}

void AudioFeedback::clear() {
  count_ = 0;
  index_ = 0;
  nextToneMs_ = 0;
}

void AudioFeedback::add(uint16_t frequency, uint16_t durationMs, uint16_t gapMs) {
  if (count_ >= 4) return;
  queue_[count_].frequency = frequency;
  queue_[count_].durationMs = durationMs;
  queue_[count_].gapMs = gapMs;
  ++count_;
}

void AudioFeedback::playExpression(Expression expression) {
  clear();
  if (muted_ || suspended_) return;
  switch (expression) {
    case Expression::Happy: add(784, 70); add(988, 95); break;
    case Expression::Excited: add(880, 55); add(1175, 55); add(1568, 90); break;
    case Expression::Sad: add(523, 100); add(392, 140); break;
    case Expression::Surprised: add(1047, 130); break;
    case Expression::Confused: add(440, 80); add(370, 110); break;
    case Expression::Sleepy: add(330, 110); break;
    case Expression::Sleeping: add(262, 160); break;
    case Expression::Listening: add(659, 65); break;
    case Expression::Thinking: add(523, 55); add(659, 55); break;
    case Expression::Speaking: add(740, 45); break;
    case Expression::Alarmed: add(988, 75); add(740, 75); add(988, 90); break;
    case Expression::Offline: add(494, 90); add(330, 130); break;
    case Expression::Error: add(220, 110); add(165, 180); break;
    default: add(523, 55); break;
  }
  nextToneMs_ = millis();
}

void AudioFeedback::update(uint32_t nowMs) {
  if (muted_ || suspended_ || index_ >= count_ ||
      static_cast<int32_t>(nowMs - nextToneMs_) < 0) return;
  const ToneStep& tone = queue_[index_++];
  M5.Speaker.tone(tone.frequency, tone.durationMs);
  nextToneMs_ = nowMs + tone.durationMs + tone.gapMs;
}

void AudioFeedback::toggleMute() {
  muted_ = !muted_;
  clear();
  if (muted_) M5.Speaker.stop();
  Serial.printf("[AUDIO] muted=%s\n", muted_ ? "true" : "false");
  if (!muted_) {
    add(660, 60);
    add(880, 80);
    nextToneMs_ = millis();
  }
}

void AudioFeedback::suspend() {
  clear();
  M5.Speaker.stop();
  M5.Speaker.end();
  suspended_ = true;
  Serial.println("[AUDIO] suspended for microphone capture");
}

void AudioFeedback::resume() {
  M5.Speaker.begin();
  M5.Speaker.setVolume(hardwareVolume_);
  suspended_ = false;
  Serial.println("[AUDIO] resumed after microphone capture");
}

}  // namespace firechan
