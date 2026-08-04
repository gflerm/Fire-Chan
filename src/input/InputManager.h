#pragma once

#include <Arduino.h>

#include "input/GestureDetector.h"
#include "config/AppConfig.h"

namespace firechan {

enum class InputEvent : uint8_t {
  None,
  PreviousExpression,
  NextExpression,
  ToggleDemo,
  ToggleSound,
  ResetNeutral,
  Shake,
  PickedUp,
  FaceDown,
  FaceUp,
  Inactive,
  DeepSleepy
};

struct InputState {
  InputEvent event = InputEvent::None;
  float tiltX = 0.0f;
  float tiltY = 0.0f;
};

class InputManager {
 public:
  void begin(const AppConfig& config);
  InputState update(uint32_t nowMs);

 private:
  GestureDetector gestures_;
  uint32_t lastImuReadMs_ = 0;
  float tiltX_ = 0.0f;
  float tiltY_ = 0.0f;
};

const char* inputEventName(InputEvent event);

}  // namespace firechan
