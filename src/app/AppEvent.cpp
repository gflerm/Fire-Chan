#include "app/AppEvent.h"

namespace firechan {

const char* appEventName(AppEventType type) {
  switch (type) {
    case AppEventType::PreviousExpression: return "previous-expression";
    case AppEventType::NextExpression: return "next-expression";
    case AppEventType::ToggleDemo: return "toggle-demo";
    case AppEventType::ToggleSound: return "toggle-sound";
    case AppEventType::ResetNeutral: return "reset-neutral";
    case AppEventType::ShakeDetected: return "shake-detected";
    case AppEventType::DevicePickedUp: return "device-picked-up";
    case AppEventType::DeviceFaceDown: return "device-face-down";
    case AppEventType::DeviceFaceUp: return "device-face-up";
    case AppEventType::InactivityStarted: return "inactivity-started";
    case AppEventType::DeepInactivityStarted: return "deep-inactivity-started";
    case AppEventType::DemoAdvance: return "demo-advance";
    case AppEventType::TemporaryExpressionExpired: return "temporary-expired";
    case AppEventType::AlarmStarted: return "alarm-started";
    case AppEventType::AlarmCleared: return "alarm-cleared";
    case AppEventType::ErrorRaised: return "error-raised";
    case AppEventType::ErrorCleared: return "error-cleared";
    case AppEventType::ListeningStarted: return "listening-started";
    case AppEventType::ListeningStopped: return "listening-stopped";
    case AppEventType::SpeakingStarted: return "speaking-started";
    case AppEventType::SpeakingStopped: return "speaking-stopped";
    case AppEventType::NetworkOffline: return "network-offline";
    case AppEventType::NetworkOnline: return "network-online";
    default: return "none";
  }
}

}  // namespace firechan

