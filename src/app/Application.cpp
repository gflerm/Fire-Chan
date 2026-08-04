#include "app/Application.h"

#include <M5Unified.h>

namespace firechan {

AppEventType Application::mapInputEvent(InputEvent event) const {
  switch (event) {
    case InputEvent::PreviousExpression: return AppEventType::PreviousExpression;
    case InputEvent::NextExpression: return AppEventType::NextExpression;
    case InputEvent::ToggleDemo: return AppEventType::ToggleDemo;
    case InputEvent::ToggleSound: return AppEventType::ToggleSound;
    case InputEvent::ResetNeutral: return AppEventType::ResetNeutral;
    case InputEvent::Shake: return AppEventType::ShakeDetected;
    case InputEvent::PickedUp: return AppEventType::DevicePickedUp;
    case InputEvent::FaceDown: return AppEventType::DeviceFaceDown;
    case InputEvent::FaceUp: return AppEventType::DeviceFaceUp;
    case InputEvent::Inactive: return AppEventType::InactivityStarted;
    case InputEvent::DeepSleepy: return AppEventType::DeepInactivityStarted;
    default: return AppEventType::None;
  }
}

void Application::applyAction(const BehaviorAction& action) {
  if (action.toggleSound) audio_.toggleMute();
  if (action.demoModeChanged) {
    Serial.printf("[BEHAVIOR] demo=%s\n", action.demoMode ? "on" : "off");
  }
  if (action.expressionChanged) {
    face_.setExpression(action.expression);
    rgb_.setExpression(action.expression);
    audio_.playExpression(action.expression);
    Serial.printf("[BEHAVIOR] resolved=%s\n", expressionName(action.expression));
  }
}

void Application::begin() {
  Serial.println();
  Serial.println("========================================");
  Serial.println(" Fire-chan event-driven personality test");
  Serial.printf(" Firmware: %s\n", FIRECHAN_VERSION);
  Serial.printf(" PSRAM: %u bytes (optional)\n", ESP.getPsramSize());
  Serial.println(" A=previous, B=next, C=auto/manual, hold B=neutral, hold C=mute");
  Serial.println(" Tilt=gaze, pickup=surprised, shake=confused, face-down=sleep");
  Serial.println("========================================");

  input_.begin();
  faceReady_ = face_.begin();
  rgb_.begin();
  audio_.begin();
  const uint32_t now = millis();
  behavior_.begin(now);
  rgb_.setExpression(behavior_.expression());
  audio_.playExpression(behavior_.expression());
  Serial.println("[EVENT] fixed queue capacity=16 ready");
}

void Application::update() {
  const uint32_t now = millis();
  const InputState input = input_.update(now);
  face_.setTilt(input.tiltX, input.tiltY);

  const AppEventType inputEvent = mapInputEvent(input.event);
  if (inputEvent != AppEventType::None) events_.publish(inputEvent, now);
  behavior_.tick(now, events_);

  AppEvent event;
  while (events_.next(event)) {
    Serial.printf("[EVENT] dispatch=%s pending=%u\n", appEventName(event.type),
                  events_.pending());
    applyAction(behavior_.handle(event));
  }

  if (faceReady_) face_.update(now, behavior_.demoMode());
  rgb_.update(now);
  audio_.update(now);
}

}  // namespace firechan
