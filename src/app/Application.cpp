#include "app/Application.h"

#include <M5Unified.h>

namespace firechan {
namespace {
constexpr uint32_t kDemoPeriodMs = 3200;
}

void Application::begin() {
  Serial.println();
  Serial.println("========================================");
  Serial.println(" Fire-chan face and gesture test");
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
  nextDemoMs_ = now + kDemoPeriodMs;
  rgb_.setExpression(Expression::Neutral);
  audio_.playExpression(Expression::Neutral);
}

void Application::setExpression(Expression expression, uint32_t nowMs,
                                uint32_t durationMs) {
  face_.setExpression(expression);
  rgb_.setExpression(expression);
  audio_.playExpression(expression);
  temporaryUntilMs_ = durationMs ? nowMs + durationMs : 0;
  if (!durationMs) baseExpression_ = expression;
}

void Application::handleInput(InputEvent event, uint32_t nowMs) {
  if (event == InputEvent::None) return;
  Serial.printf("[INPUT] %s\n", inputEventName(event));
  switch (event) {
    case InputEvent::PreviousExpression:
      demoMode_ = false;
      setExpression(previousExpression(face_.expression()), nowMs);
      break;
    case InputEvent::NextExpression:
      demoMode_ = false;
      setExpression(nextExpression(face_.expression()), nowMs);
      break;
    case InputEvent::ToggleDemo:
      demoMode_ = !demoMode_;
      nextDemoMs_ = nowMs + kDemoPeriodMs;
      Serial.printf("[FACE] demo=%s\n", demoMode_ ? "on" : "off");
      break;
    case InputEvent::ToggleSound:
      audio_.toggleMute();
      break;
    case InputEvent::ResetNeutral:
      demoMode_ = false;
      setExpression(Expression::Neutral, nowMs);
      break;
    case InputEvent::Shake:
      setExpression(Expression::Confused, nowMs, 1800);
      break;
    case InputEvent::PickedUp:
      setExpression(Expression::Surprised, nowMs, 1800);
      break;
    case InputEvent::FaceDown:
      demoMode_ = false;
      setExpression(Expression::Sleeping, nowMs);
      break;
    case InputEvent::FaceUp:
      setExpression(Expression::Neutral, nowMs, 1200);
      break;
    case InputEvent::Inactive:
      if (!demoMode_) setExpression(Expression::Sleepy, nowMs);
      break;
    case InputEvent::DeepSleepy:
      if (!demoMode_) setExpression(Expression::Sleeping, nowMs);
      break;
    default:
      break;
  }
}

void Application::update() {
  const uint32_t now = millis();
  const InputState input = input_.update(now);
  face_.setTilt(input.tiltX, input.tiltY);
  handleInput(input.event, now);

  if (temporaryUntilMs_ && static_cast<int32_t>(now - temporaryUntilMs_) >= 0) {
    temporaryUntilMs_ = 0;
    setExpression(baseExpression_, now);
  }

  if (demoMode_ && temporaryUntilMs_ == 0 &&
      static_cast<int32_t>(now - nextDemoMs_) >= 0) {
    setExpression(nextExpression(face_.expression()), now);
    nextDemoMs_ = now + kDemoPeriodMs;
  }

  if (faceReady_) face_.update(now, demoMode_);
  rgb_.update(now);
  audio_.update(now);
}

}  // namespace firechan
