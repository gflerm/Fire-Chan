#pragma once

#include <Arduino.h>

namespace firechan {

enum class GestureEvent : uint8_t {
  None,
  Shake,
  PickedUp,
  FaceDown,
  FaceUp,
  Inactive,
  DeepSleepy
};

struct GestureReading {
  GestureEvent event = GestureEvent::None;
  float tiltX = 0.0f;
  float tiltY = 0.0f;
};

class GestureDetector {
 public:
  void begin(uint32_t nowMs);
  GestureReading update(uint32_t nowMs, float ax, float ay, float az);
  void noteInteraction(uint32_t nowMs);

 private:
  float previousAx_ = 0.0f;
  float previousAy_ = 0.0f;
  float previousAz_ = 1.0f;
  float filteredAx_ = 0.0f;
  float filteredAy_ = 0.0f;
  float filteredAz_ = 1.0f;
  uint32_t lastMotionMs_ = 0;
  uint32_t cooldownUntilMs_ = 0;
  uint32_t faceDownSinceMs_ = 0;
  bool faceDown_ = false;
  bool sleepySent_ = false;
  bool sleepingSent_ = false;
  bool initialized_ = false;
};

const char* gestureName(GestureEvent event);

}  // namespace firechan

