#pragma once

#include <Arduino.h>

#include "face/FaceEngine.h"
#include "input/InputManager.h"
#include "audio/AudioFeedback.h"
#include "audio/ResponseAudioPlayer.h"
#include "hardware/RgbFeedback.h"
#include "app/BehaviorEngine.h"
#include "app/EventBus.h"
#include "config/AppConfig.h"
#include "storage/ConfigManager.h"
#include "voice/VoiceRecorder.h"
#include "network/NetworkManager.h"
#include "assistant/AssistantDirective.h"
#include "assistant/VoiceGatewayClient.h"

namespace firechan {

class Application {
 public:
  void begin();
  void update();

 private:
  AppEventType mapInputEvent(InputEvent event) const;
  void applyAction(const BehaviorAction& action);
  void applyPendingAssistantDirective(uint32_t nowMs);
  void setAudioMuted(bool muted, uint32_t nowMs);
  void setAudioVolume(uint8_t volumePercent, uint32_t nowMs);
  void handleCommandEvent(const AppEvent& event);
  void buildDeviceStatus(char* buffer, size_t size) const;

  FaceEngine face_;
  InputManager input_;
  AudioFeedback audio_;
  RgbFeedback rgb_;
  EventBus events_;
  BehaviorEngine behavior_;
  AppConfig config_;
  ConfigManager configManager_;
  VoiceRecorder voice_;
  NetworkManager network_;
  VoiceGatewayClient voiceGateway_;
  ResponseAudioPlayer responsePlayer_;
  AssistantDirective pendingDirective_;
  bool hasPendingDirective_ = false;
  bool faceReady_ = false;
};

}  // namespace firechan
