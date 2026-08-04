#include "app/Application.h"

#include <M5Unified.h>

namespace firechan {

AppEventType Application::mapInputEvent(InputEvent event) const {
  switch (event) {
    case InputEvent::StartVoiceCapture: return AppEventType::VoiceCaptureRequested;
    case InputEvent::StopVoiceCapture: return AppEventType::VoiceCaptureStopRequested;
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

void Application::handleCommandEvent(const AppEvent& event) {
  if (event.type == AppEventType::VoiceCaptureRequested && !voice_.recording() &&
      !voiceGateway_.busy()) {
    audio_.suspend();
    if (voice_.start(event.timestampMs)) {
      events_.publish(AppEventType::ListeningStarted, event.timestampMs);
    } else {
      audio_.resume();
      events_.publish(AppEventType::VoiceRecordingFailed, event.timestampMs);
    }
  } else if (event.type == AppEventType::VoiceCaptureStopRequested) {
    voice_.requestStop();
  }
}

void Application::applyAction(const BehaviorAction& action) {
  const uint32_t now = millis();
  if (action.toggleSound) {
    audio_.toggleMute();
    config_.muted = audio_.muted();
    configManager_.markDirty(now);
  }
  if (action.demoModeChanged) {
    config_.demoMode = action.demoMode;
    configManager_.markDirty(now);
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
  Serial.println(" Hold A=push-to-talk, B=next, C=auto/manual");
  Serial.println(" Hold B=neutral, hold C=mute");
  Serial.println(" Tilt=gaze, pickup=surprised, shake=confused, face-down=sleep");
  Serial.println("========================================");

  configManager_.begin(config_);
  voice_.begin(configManager_.sdAvailable(), config_.maxRecordingSeconds);
  network_.begin(millis());
  voiceGateway_.begin();
  input_.begin(config_);
  faceReady_ = face_.begin(config_.displayBrightnessPercent);
  rgb_.begin(config_.rgbBrightnessPercent);
  audio_.begin(config_.volumePercent, config_.muted);
  const uint32_t now = millis();
  behavior_.begin(now, config_.demoMode);
  rgb_.setExpression(behavior_.expression());
  audio_.playExpression(behavior_.expression());
  Serial.println("[EVENT] fixed queue capacity=16 ready");
}

void Application::update() {
  const uint32_t now = millis();
  network_.update(now, events_);
  const InputState input = input_.update(now);
  face_.setTilt(input.tiltX, input.tiltY);

  const AppEventType inputEvent = mapInputEvent(input.event);
  if (inputEvent != AppEventType::None) events_.publish(inputEvent, now);

  const VoiceRecorderEvent voiceEvent = voice_.update(now);
  if (voiceEvent == VoiceRecorderEvent::RecordingReady) {
    audio_.resume();
    events_.publish(AppEventType::VoiceRecordingReady, now);
    if (!network_.connected() || !voiceGateway_.submit(voice_.recordingPath())) {
      Serial.println("[ASSISTANT] prompt not submitted; gateway unavailable");
      events_.publish(AppEventType::AssistantRequestFailed, now);
    }
  } else if (voiceEvent == VoiceRecorderEvent::RecordingFailed) {
    audio_.resume();
    events_.publish(AppEventType::VoiceRecordingFailed, now);
  }

  const VoiceGatewayEvent gatewayEvent = voiceGateway_.update();
  if (gatewayEvent == VoiceGatewayEvent::ResponseReady) {
    events_.publish(AppEventType::AssistantResponseReady, now);
  } else if (gatewayEvent == VoiceGatewayEvent::RequestFailed) {
    Serial.printf("[ASSISTANT] request failed: %s\n", voiceGateway_.error());
    events_.publish(AppEventType::AssistantRequestFailed, now);
  }
  behavior_.tick(now, events_);

  AppEvent event;
  while (events_.next(event)) {
    Serial.printf("[EVENT] dispatch=%s pending=%u\n", appEventName(event.type),
                  events_.pending());
    handleCommandEvent(event);
    applyAction(behavior_.handle(event));
  }

  if (faceReady_) face_.update(now, behavior_.demoMode());
  rgb_.update(now);
  audio_.update(now);
  configManager_.update(now, config_);
}

}  // namespace firechan
