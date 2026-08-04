#include "app/BehaviorEngine.h"

namespace firechan {
namespace {
constexpr uint32_t kDemoPeriodMs = 3200;
}

void BehaviorEngine::begin(uint32_t nowMs, bool demoMode) {
  demoMode_ = demoMode;
  nextDemoMs_ = nowMs + kDemoPeriodMs;
  activeExpression_ = resolveExpression();
  Serial.println("[BEHAVIOR] priority engine ready");
}

void BehaviorEngine::tick(uint32_t nowMs, EventBus& events) {
  if (demoMode_ && static_cast<int32_t>(nowMs - nextDemoMs_) >= 0) {
    events.publish(AppEventType::DemoAdvance, nowMs);
    nextDemoMs_ = nowMs + kDemoPeriodMs;
  }
  if (temporaryUntilMs_ && !temporaryExpiryQueued_ &&
      static_cast<int32_t>(nowMs - temporaryUntilMs_) >= 0) {
    temporaryExpiryQueued_ = events.publish(AppEventType::TemporaryExpressionExpired,
                                             nowMs);
  }
}

void BehaviorEngine::setDemoMode(bool enabled, uint32_t nowMs,
                                 BehaviorAction& action) {
  if (demoMode_ == enabled) return;
  demoMode_ = enabled;
  if (enabled) nextDemoMs_ = nowMs + kDemoPeriodMs;
  action.demoModeChanged = true;
  action.demoMode = demoMode_;
}

void BehaviorEngine::setBaseExpression(Expression expression) {
  baseExpression_ = expression;
}

void BehaviorEngine::setTemporaryExpression(Expression expression, uint32_t nowMs,
                                            uint32_t durationMs) {
  temporaryExpression_ = expression;
  temporaryUntilMs_ = nowMs + durationMs;
  temporaryExpiryQueued_ = false;
}

Expression BehaviorEngine::resolveExpression() const {
  // Priority follows the project specification. Higher-priority system states
  // always override user, temporary, network, and idle expressions.
  if (errorActive_) return Expression::Error;
  if (alarmActive_) return Expression::Alarmed;
  if (faceDown_) return Expression::Sleeping;
  if (speaking_) return Expression::Speaking;
  if (listening_) return Expression::Listening;
  if (temporaryUntilMs_) return temporaryExpression_;
  if (offline_) return Expression::Offline;
  return baseExpression_;
}

void BehaviorEngine::resolveAction(BehaviorAction& action) {
  const Expression resolved = resolveExpression();
  if (resolved != activeExpression_) {
    activeExpression_ = resolved;
    action.expressionChanged = true;
    action.expression = resolved;
  }
}

BehaviorAction BehaviorEngine::handle(const AppEvent& event) {
  BehaviorAction action;
  action.demoMode = demoMode_;
  switch (event.type) {
    case AppEventType::VoiceCaptureRequested:
      // A real interaction takes precedence over the expression showcase.
      setDemoMode(false, event.timestampMs, action);
      break;
    case AppEventType::VoiceRecordingReady:
      listening_ = false;
      setTemporaryExpression(Expression::Thinking, event.timestampMs, 1800);
      break;
    case AppEventType::VoiceRecordingFailed:
      listening_ = false;
      setTemporaryExpression(Expression::Error, event.timestampMs, 2500);
      break;
    case AppEventType::AssistantResponseReady:
      setTemporaryExpression(Expression::Happy, event.timestampMs, 2200);
      break;
    case AppEventType::AssistantRequestFailed:
      setTemporaryExpression(Expression::Error, event.timestampMs, 3000);
      break;
    case AppEventType::PreviousExpression:
      setDemoMode(false, event.timestampMs, action);
      setBaseExpression(previousExpression(activeExpression_));
      temporaryUntilMs_ = 0;
      break;
    case AppEventType::NextExpression:
      setDemoMode(false, event.timestampMs, action);
      setBaseExpression(nextExpression(activeExpression_));
      temporaryUntilMs_ = 0;
      break;
    case AppEventType::ToggleDemo:
      setDemoMode(!demoMode_, event.timestampMs, action);
      break;
    case AppEventType::ToggleSound:
      action.toggleSound = true;
      break;
    case AppEventType::ResetNeutral:
      setDemoMode(false, event.timestampMs, action);
      setBaseExpression(Expression::Neutral);
      temporaryUntilMs_ = 0;
      break;
    case AppEventType::ShakeDetected:
      setTemporaryExpression(Expression::Confused, event.timestampMs, 1800);
      break;
    case AppEventType::DevicePickedUp:
      setTemporaryExpression(Expression::Surprised, event.timestampMs, 1800);
      break;
    case AppEventType::DeviceFaceDown:
      faceDown_ = true;
      setDemoMode(false, event.timestampMs, action);
      break;
    case AppEventType::DeviceFaceUp:
      faceDown_ = false;
      setTemporaryExpression(Expression::Neutral, event.timestampMs, 1200);
      break;
    case AppEventType::InactivityStarted:
      if (!demoMode_) setBaseExpression(Expression::Sleepy);
      break;
    case AppEventType::DeepInactivityStarted:
      if (!demoMode_) setBaseExpression(Expression::Sleeping);
      break;
    case AppEventType::DemoAdvance:
      if (demoMode_) setBaseExpression(nextExpression(baseExpression_));
      break;
    case AppEventType::TemporaryExpressionExpired:
      temporaryUntilMs_ = 0;
      temporaryExpiryQueued_ = false;
      break;
    case AppEventType::AlarmStarted: alarmActive_ = true; break;
    case AppEventType::AlarmCleared: alarmActive_ = false; break;
    case AppEventType::ErrorRaised: errorActive_ = true; break;
    case AppEventType::ErrorCleared: errorActive_ = false; break;
    case AppEventType::ListeningStarted: listening_ = true; break;
    case AppEventType::ListeningStopped: listening_ = false; break;
    case AppEventType::SpeakingStarted: speaking_ = true; break;
    case AppEventType::SpeakingStopped: speaking_ = false; break;
    case AppEventType::NetworkOffline: offline_ = true; break;
    case AppEventType::NetworkOnline: offline_ = false; break;
    default: break;
  }
  resolveAction(action);
  return action;
}

}  // namespace firechan
