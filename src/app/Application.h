#pragma once

#include <Arduino.h>

#include "face/FaceEngine.h"
#include "input/InputManager.h"
#include "audio/AudioFeedback.h"
#include "hardware/RgbFeedback.h"

namespace firechan {

class Application {
 public:
  void begin();
  void update();

 private:
  void handleInput(InputEvent event, uint32_t nowMs);
  void setExpression(Expression expression, uint32_t nowMs, uint32_t durationMs = 0);

  FaceEngine face_;
  InputManager input_;
  AudioFeedback audio_;
  RgbFeedback rgb_;
  bool demoMode_ = true;
  bool faceReady_ = false;
  uint32_t nextDemoMs_ = 0;
  uint32_t temporaryUntilMs_ = 0;
  Expression baseExpression_ = Expression::Neutral;
};

}  // namespace firechan
