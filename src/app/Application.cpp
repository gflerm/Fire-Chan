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
    case InputEvent::DismissAlarm: return AppEventType::AlarmDismissRequested;
    case InputEvent::Shake: return AppEventType::ShakeDetected;
    case InputEvent::PickedUp: return AppEventType::DevicePickedUp;
    case InputEvent::FaceDown: return AppEventType::DeviceFaceDown;
    case InputEvent::FaceUp: return AppEventType::DeviceFaceUp;
    case InputEvent::Inactive: return AppEventType::InactivityStarted;
    case InputEvent::DeepSleepy: return AppEventType::DeepInactivityStarted;
    default: return AppEventType::None;
  }
}

void Application::buildDeviceStatus(char* buffer, size_t size) const {
  char wifi[8];
  snprintf(wifi, sizeof(wifi), "%d", network_.connected() ? 1 : 0);
  char sdMb[16];
  snprintf(sdMb, sizeof(sdMb), "%llu",
           static_cast<unsigned long long>(configManager_.sdFreeBytes() / (1024 * 1024)));
  int batteryPercent = -1;
  if (M5.Power.getBatteryLevel() >= 0) {
    batteryPercent = M5.Power.getBatteryLevel();
  }
  char battery[16];
  if (batteryPercent >= 0) {
    snprintf(battery, sizeof(battery), "%d", batteryPercent);
  } else {
    snprintf(battery, sizeof(battery), "na");
  }
  snprintf(buffer, size, "fw=%s;wifi=%s;sd_free_mb=%s;battery=%s",
           FIRECHAN_VERSION, wifi, sdMb, battery);
  Serial.printf("[ASSISTANT] device status: %s\n", buffer);
}

void Application::handleCommandEvent(const AppEvent& event) {
  if (event.type == AppEventType::VoiceCaptureRequested && !voice_.recording() &&
      !voiceGateway_.busy() && !responsePlayer_.busy()) {
    // If the alarm is ringing, the user pressing the talk button means they
    // want to dismiss the alarm, not start a new capture.
    if (alarm_.isRinging()) {
      alarm_.clearAlarm();
      return;
    }
    audio_.suspend();
    if (voice_.start(event.timestampMs)) {
      events_.publish(AppEventType::ListeningStarted, event.timestampMs);
    } else {
      audio_.resume();
      events_.publish(AppEventType::VoiceRecordingFailed, event.timestampMs);
    }
  } else if (event.type == AppEventType::VoiceCaptureStopRequested) {
    voice_.requestStop();
  } else if (event.type == AppEventType::AlarmDismissRequested) {
    if (alarm_.isRinging()) {
      alarm_.clearAlarm();
    } else {
      // The alarm is not ringing; treat the button as the regular
      // "next expression" gesture so the existing demo/manual flow is
      // preserved when no alarm is active.
      events_.publish(AppEventType::NextExpression, event.timestampMs);
    }
  }
}

void Application::applyAction(const BehaviorAction& action) {
  const uint32_t now = millis();
  if (action.toggleSound) {
    setAudioMuted(!audio_.muted(), now);
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

void Application::setAudioMuted(bool muted, uint32_t nowMs) {
  if (audio_.muted() == muted) return;
  audio_.setMuted(muted);
  config_.muted = muted;
  configManager_.markDirty(nowMs);
}

void Application::setAudioVolume(uint8_t volumePercent, uint32_t nowMs) {
  volumePercent = constrain(volumePercent, 0, 100);
  if (config_.volumePercent == volumePercent) return;
  audio_.setVolumePercent(volumePercent);
  config_.volumePercent = volumePercent;
  configManager_.markDirty(nowMs);
}

void Application::applyPendingAssistantDirective(uint32_t nowMs) {
  if (!hasPendingDirective_) return;
  if (pendingDirective_.action == AssistantAction::Mute) {
    setAudioMuted(true, nowMs);
  } else if (pendingDirective_.action == AssistantAction::Unmute) {
    setAudioMuted(false, nowMs);
  } else if (pendingDirective_.action == AssistantAction::Volume) {
    if (pendingDirective_.volumeAbsolute) {
      setAudioVolume(pendingDirective_.volumeTarget, nowMs);
    } else {
      int32_t target = static_cast<int32_t>(config_.volumePercent) +
                       pendingDirective_.volumeDelta;
      setAudioVolume(static_cast<uint8_t>(constrain(target, 0, 100)), nowMs);
    }
  } else if (pendingDirective_.action == AssistantAction::SetAlarm) {
    alarm_.setAlarm(pendingDirective_.alarmTime, pendingDirective_.alarmLabel);
    config_.alarmTime = pendingDirective_.alarmTime;
    strlcpy(config_.alarmLabel, pendingDirective_.alarmLabel,
            sizeof(config_.alarmLabel));
    configManager_.markDirty(nowMs);
  }
  Serial.printf("[ASSISTANT] apply expression=%s action=%s\n",
                pendingDirective_.hasExpression
                    ? expressionName(pendingDirective_.expression) : "none",
                AssistantDirectiveParser::actionName(pendingDirective_.action));
  if (pendingDirective_.action == AssistantAction::SetAlarm) {
    Serial.printf("[ASSISTANT] SetAlarm time=%lu label=%s\n",
                  static_cast<unsigned long>(pendingDirective_.alarmTime),
                  pendingDirective_.alarmLabel);
  }
  applyAction(behavior_.applyAssistantDirective(pendingDirective_, nowMs));
  hasPendingDirective_ = false;
}

void Application::begin() {
  Serial.println();
  Serial.println("========================================");
  Serial.println(" Fire-chan event-driven personality test");
  Serial.printf(" Firmware: %s\n", FIRECHAN_VERSION);
  Serial.printf(" PSRAM: %u bytes (optional)\n", ESP.getPsramSize());
  Serial.println(" Hold A=push-to-talk, B=next or dismiss-alarm, C=auto/manual");
  Serial.println(" Hold B=neutral, hold C=mute");
  Serial.println(" Tilt=gaze, pickup=surprised, shake=confused, face-down=sleep");
  Serial.println("========================================");

  configManager_.begin(config_);
  voiceGateway_.setDeviceId(config_.deviceId);
  voice_.begin(configManager_.sdAvailable(), config_.maxRecordingSeconds);
  network_.begin(millis());
  voiceGateway_.begin();
  responsePlayer_.begin();
  input_.begin(config_);
  faceReady_ = face_.begin(config_.displayBrightnessPercent);
  rgb_.begin(config_.rgbBrightnessPercent);
  audio_.begin(config_.volumePercent, config_.muted);
  const uint32_t now = millis();
  behavior_.begin(now, config_.demoMode);
  rgb_.setExpression(behavior_.expression());
  audio_.playExpression(behavior_.expression());
  // Restore any alarm that was scheduled before the last reboot.
  if (config_.alarmTime != 0) {
    alarm_.setAlarm(config_.alarmTime, config_.alarmLabel);
  }
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
    char deviceStatus[192];
    buildDeviceStatus(deviceStatus, sizeof(deviceStatus));
    if (!network_.connected() ||
        !voiceGateway_.submit(voice_.recordingPath(), deviceStatus)) {
      Serial.println("[ASSISTANT] prompt not submitted; gateway unavailable");
      events_.publish(AppEventType::AssistantRequestFailed, now);
    }  } else if (voiceEvent == VoiceRecorderEvent::RecordingFailed) {
    audio_.resume();
    events_.publish(AppEventType::VoiceRecordingFailed, now);
  }

  const VoiceGatewayEvent gatewayEvent = voiceGateway_.update();
  if (gatewayEvent == VoiceGatewayEvent::ResponseReady) {
    pendingDirective_ = AssistantDirectiveParser::parse(
        voiceGateway_.expression(), voiceGateway_.action());
    // If the gateway asked the device to set a local alarm, populate the
    // directive with the timestamp and label so applyPendingAssistantDirective
    // can persist it and hand it to the AlarmManager.
    if (voiceGateway_.pendingAlarmTime() != 0) {
      pendingDirective_.action = AssistantAction::SetAlarm;
      pendingDirective_.alarmTime = voiceGateway_.pendingAlarmTime();
      strlcpy(pendingDirective_.alarmLabel, voiceGateway_.pendingAlarmLabel(),
              sizeof(pendingDirective_.alarmLabel));
      Serial.printf("[ASSISTANT] routing alarm directive time=%lu label=%s\n",
                    static_cast<unsigned long>(pendingDirective_.alarmTime),
                    pendingDirective_.alarmLabel);
    }
    hasPendingDirective_ = true;
    // Unmute must happen before deciding whether Ember may speak. Mute is
    // deliberately deferred until her acknowledgement has finished.
    if (pendingDirective_.action == AssistantAction::Unmute) {
      setAudioMuted(false, now);
    }
    events_.publish(AppEventType::AssistantResponseReady, now);
    if (!audio_.muted()) {
      audio_.suspend();
      if (responsePlayer_.play(voiceGateway_.audioPath(), audio_.speechVolume())) {
        events_.publish(AppEventType::SpeakingStarted, now);
      } else {
        audio_.resume();
        applyPendingAssistantDirective(now);
        events_.publish(AppEventType::AssistantRequestFailed, now);
      }
    } else {
      applyPendingAssistantDirective(now);
    }
  } else if (gatewayEvent == VoiceGatewayEvent::RequestFailed) {
    Serial.printf("[ASSISTANT] request failed: %s\n", voiceGateway_.error());
    events_.publish(AppEventType::AssistantRequestFailed, now);
  }

  const ResponseAudioEvent playbackEvent = responsePlayer_.update();
  if (playbackEvent == ResponseAudioEvent::PlaybackFinished) {
    audio_.resume();
    events_.publish(AppEventType::SpeakingStopped, now);
    applyPendingAssistantDirective(now);
  } else if (playbackEvent == ResponseAudioEvent::PlaybackFailed) {
    audio_.resume();
    Serial.printf("[PLAYBACK] failed: %s\n", responsePlayer_.error());
    events_.publish(AppEventType::SpeakingStopped, now);
    applyPendingAssistantDirective(now);
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
  alarm_.update(now);
  if (alarm_.wasCleared()) {
    // The AlarmManager cleared a stale deadline; persist the cleared state
    // so the next reboot doesn't reload it from NVS.
    config_.alarmTime = 0;
    config_.alarmLabel[0] = '\0';
    configManager_.markDirty(now);
  }
  configManager_.update(now, config_);
}

}  // namespace firechan
