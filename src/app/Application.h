#pragma once

#include <Arduino.h>

#include "face/FaceEngine.h"
#include "input/InputManager.h"
#include "audio/AudioFeedback.h"
#include "hardware/RgbFeedback.h"
#include "app/BehaviorEngine.h"
#include "app/EventBus.h"
#include "config/AppConfig.h"
#include "storage/ConfigManager.h"
#include "voice/VoiceRecorder.h"

namespace firechan {

class Application {
 public:
  void begin();
  void update();

 private:
  AppEventType mapInputEvent(InputEvent event) const;
  void applyAction(const BehaviorAction& action);
  void handleCommandEvent(const AppEvent& event);

  FaceEngine face_;
  InputManager input_;
  AudioFeedback audio_;
  RgbFeedback rgb_;
  EventBus events_;
  BehaviorEngine behavior_;
  AppConfig config_;
  ConfigManager configManager_;
  VoiceRecorder voice_;
  bool faceReady_ = false;
};

}  // namespace firechan
