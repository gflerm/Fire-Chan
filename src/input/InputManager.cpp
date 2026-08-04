#include "input/InputManager.h"

#include <M5Unified.h>

namespace firechan {

const char* inputEventName(InputEvent event) {
  switch (event) {
    case InputEvent::PreviousExpression: return "button-a/previous";
    case InputEvent::NextExpression: return "button-b/next";
    case InputEvent::ToggleDemo: return "button-c/toggle-demo";
    case InputEvent::ToggleSound: return "button-c-long/toggle-sound";
    case InputEvent::ResetNeutral: return "button-b-long/neutral";
    case InputEvent::Shake: return "shake";
    case InputEvent::PickedUp: return "picked-up";
    case InputEvent::FaceDown: return "face-down";
    case InputEvent::FaceUp: return "face-up";
    case InputEvent::Inactive: return "inactive";
    case InputEvent::DeepSleepy: return "deep-sleepy";
    default: return "none";
  }
}

void InputManager::begin() { gestures_.begin(millis()); }

InputState InputManager::update(uint32_t nowMs) {
  InputState state;
  if (M5.BtnC.wasHold()) {
    state.event = InputEvent::ToggleSound;
    gestures_.noteInteraction(nowMs);
  } else if (M5.BtnB.wasHold()) {
    state.event = InputEvent::ResetNeutral;
    gestures_.noteInteraction(nowMs);
  } else if (M5.BtnA.wasClicked()) {
    state.event = InputEvent::PreviousExpression;
    gestures_.noteInteraction(nowMs);
  } else if (M5.BtnB.wasClicked()) {
    state.event = InputEvent::NextExpression;
    gestures_.noteInteraction(nowMs);
  } else if (M5.BtnC.wasClicked()) {
    state.event = InputEvent::ToggleDemo;
    gestures_.noteInteraction(nowMs);
  }

  if (M5.Imu.isEnabled() && nowMs - lastImuReadMs_ >= 20) {
    lastImuReadMs_ = nowMs;
    float ax = 0, ay = 0, az = 0;
    if (M5.Imu.getAccelData(&ax, &ay, &az)) {
      const GestureReading reading = gestures_.update(nowMs, ax, ay, az);
      tiltX_ = reading.tiltX;
      tiltY_ = reading.tiltY;
      if (state.event == InputEvent::None) {
        switch (reading.event) {
          case GestureEvent::Shake: state.event = InputEvent::Shake; break;
          case GestureEvent::PickedUp: state.event = InputEvent::PickedUp; break;
          case GestureEvent::FaceDown: state.event = InputEvent::FaceDown; break;
          case GestureEvent::FaceUp: state.event = InputEvent::FaceUp; break;
          case GestureEvent::Inactive: state.event = InputEvent::Inactive; break;
          case GestureEvent::DeepSleepy: state.event = InputEvent::DeepSleepy; break;
          default: break;
        }
      }
    }
  }
  state.tiltX = tiltX_;
  state.tiltY = tiltY_;
  return state;
}

}  // namespace firechan
