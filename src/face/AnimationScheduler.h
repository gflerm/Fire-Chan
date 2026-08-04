#pragma once

#include <Arduino.h>

#include "face/Expression.h"

namespace firechan {

struct AnimationFrame {
  float blink = 0.0f;
  float idleGazeX = 0.0f;
  float idleGazeY = 0.0f;
  float mouthPhase = 0.0f;
};

class AnimationScheduler {
 public:
  void begin(uint32_t nowMs);
  AnimationFrame update(uint32_t nowMs, Expression expression);

 private:
  void scheduleBlink(uint32_t nowMs);
  void scheduleGaze(uint32_t nowMs);

  uint32_t blinkStartMs_ = 0;
  uint32_t nextBlinkMs_ = 0;
  uint32_t nextGazeMs_ = 0;
  bool blinking_ = false;
  float gazeX_ = 0.0f;
  float gazeY_ = 0.0f;
  float targetGazeX_ = 0.0f;
  float targetGazeY_ = 0.0f;
};

}  // namespace firechan

