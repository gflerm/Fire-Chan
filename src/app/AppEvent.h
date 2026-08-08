#pragma once

#include <Arduino.h>

namespace firechan {

enum class AppEventType : uint8_t {
  None,
  VoiceCaptureRequested,
  VoiceCaptureStopRequested,
  VoiceRecordingReady,
  VoiceRecordingFailed,
  AssistantResponseReady,
  AssistantRequestFailed,
  PreviousExpression,
  NextExpression,
  ToggleDemo,
  ToggleSound,
  ResetNeutral,
  ShakeDetected,
  DevicePickedUp,
  DeviceFaceDown,
  DeviceFaceUp,
  InactivityStarted,
  DeepInactivityStarted,
  DemoAdvance,
  TemporaryExpressionExpired,
  AlarmStarted,
  AlarmCleared,
  AlarmDismissRequested,
  ErrorRaised,
  ErrorCleared,
  ListeningStarted,
  ListeningStopped,
  SpeakingStarted,
  SpeakingStopped,
  NetworkOffline,
  NetworkOnline
};

struct AppEvent {
  AppEventType type = AppEventType::None;
  uint32_t timestampMs = 0;
};

const char* appEventName(AppEventType type);

}  // namespace firechan
