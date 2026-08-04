#pragma once

#include <Arduino.h>

#include "face/Expression.h"

namespace firechan {

class AudioFeedback {
 public:
  void begin(uint8_t volumePercent, bool muted);
  void playExpression(Expression expression);
  void update(uint32_t nowMs);
  void toggleMute();
  bool muted() const { return muted_; }

 private:
  struct ToneStep {
    uint16_t frequency = 0;
    uint16_t durationMs = 0;
    uint16_t gapMs = 0;
  };

  void clear();
  void add(uint16_t frequency, uint16_t durationMs, uint16_t gapMs = 25);

  ToneStep queue_[4];
  uint8_t count_ = 0;
  uint8_t index_ = 0;
  uint32_t nextToneMs_ = 0;
  bool muted_ = false;
};

}  // namespace firechan
