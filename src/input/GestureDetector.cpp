#include "input/GestureDetector.h"

#include <math.h>

namespace firechan {

const char* gestureName(GestureEvent event) {
  switch (event) {
    case GestureEvent::Shake: return "shake";
    case GestureEvent::PickedUp: return "picked-up";
    case GestureEvent::FaceDown: return "face-down";
    case GestureEvent::FaceUp: return "face-up";
    case GestureEvent::Inactive: return "inactive";
    case GestureEvent::DeepSleepy: return "deep-sleepy";
    default: return "none";
  }
}

void GestureDetector::begin(uint32_t nowMs, uint8_t sensitivityPercent,
                            uint16_t sleepyAfterSeconds,
                            uint16_t sleepingAfterSeconds) {
  lastMotionMs_ = nowMs;
  thresholdScale_ = 100.0f / sensitivityPercent;
  sleepyAfterMs_ = sleepyAfterSeconds * 1000UL;
  sleepingAfterMs_ = sleepingAfterSeconds * 1000UL;
}

void GestureDetector::noteInteraction(uint32_t nowMs) {
  lastMotionMs_ = nowMs;
  sleepySent_ = false;
  sleepingSent_ = false;
}

GestureReading GestureDetector::update(uint32_t nowMs, float ax, float ay, float az) {
  GestureReading result;
  if (!initialized_) {
    previousAx_ = filteredAx_ = ax;
    previousAy_ = filteredAy_ = ay;
    previousAz_ = filteredAz_ = az;
    initialized_ = true;
  }

  filteredAx_ += (ax - filteredAx_) * 0.18f;
  filteredAy_ += (ay - filteredAy_) * 0.18f;
  filteredAz_ += (az - filteredAz_) * 0.18f;
  result.tiltX = constrain(filteredAx_ * 1.8f, -1.0f, 1.0f);
  result.tiltY = constrain(filteredAy_ * 1.8f, -1.0f, 1.0f);

  const float jerk = fabsf(ax - previousAx_) + fabsf(ay - previousAy_) +
                     fabsf(az - previousAz_);
  const float magnitude = sqrtf(ax * ax + ay * ay + az * az);
  previousAx_ = ax;
  previousAy_ = ay;
  previousAz_ = az;

  if (jerk > 0.12f * thresholdScale_ ||
      fabsf(magnitude - 1.0f) > 0.12f * thresholdScale_) noteInteraction(nowMs);

  if (filteredAz_ < -0.72f) {
    if (faceDownSinceMs_ == 0) faceDownSinceMs_ = nowMs;
    if (!faceDown_ && nowMs - faceDownSinceMs_ > 500) {
      faceDown_ = true;
      result.event = GestureEvent::FaceDown;
    }
  } else {
    faceDownSinceMs_ = 0;
    if (faceDown_ && filteredAz_ > -0.25f) {
      faceDown_ = false;
      result.event = GestureEvent::FaceUp;
      noteInteraction(nowMs);
    }
  }

  if (result.event == GestureEvent::None && static_cast<int32_t>(nowMs - cooldownUntilMs_) >= 0) {
    if (jerk > 1.35f * thresholdScale_ || magnitude > 2.2f) {
      result.event = GestureEvent::Shake;
      cooldownUntilMs_ = nowMs + 1600;
      noteInteraction(nowMs);
    } else if (jerk > 0.38f * thresholdScale_ && magnitude > 0.55f &&
               magnitude < 1.65f) {
      result.event = GestureEvent::PickedUp;
      cooldownUntilMs_ = nowMs + 1600;
      noteInteraction(nowMs);
    }
  }

  const uint32_t idleMs = nowMs - lastMotionMs_;
  if (!faceDown_ && result.event == GestureEvent::None) {
    if (!sleepingSent_ && idleMs > sleepingAfterMs_) {
      sleepingSent_ = true;
      result.event = GestureEvent::DeepSleepy;
    } else if (!sleepySent_ && idleMs > sleepyAfterMs_) {
      sleepySent_ = true;
      result.event = GestureEvent::Inactive;
    }
  }
  return result;
}

}  // namespace firechan
