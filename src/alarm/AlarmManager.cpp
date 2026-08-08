#include "alarm/AlarmManager.h"

#include <M5Unified.h>
#include <time.h>

namespace firechan {
namespace {
// Check the current time every second; the alarm deadline is in whole
// seconds, so a one-second tick is precise enough.
constexpr uint32_t kCheckIntervalMs = 1000;
// Total alarm sound duration (~3 seconds of beeping).
constexpr uint32_t kAlarmDurationMs = 3000;

// One step in the alarm sound pattern: a tone (or silence) of a given
// duration. frequency == 0 means silence.
struct AlarmStep {
  uint16_t frequency;
  uint16_t durationMs;
};

constexpr AlarmStep kAlarmPattern[] = {
    {988, 250}, {0, 100},  // 350
    {740, 250}, {0, 100},  // 700
    {988, 250}, {0, 100},  // 1050
    {740, 250}, {0, 100},  // 1400
    {988, 300}, {0, 100},  // 1800
    {740, 300}, {0, 100},  // 2200
    {988, 400}, {0, 100},  // 2700
    {740, 300},            // 3000
};
constexpr size_t kAlarmStepCount =
    sizeof(kAlarmPattern) / sizeof(kAlarmPattern[0]);
}

void AlarmManager::setAlarm(uint32_t alarmTime, const char* label) {
  alarmTime_ = alarmTime;
  if (label) {
    strlcpy(label_, label, sizeof(label_));
  } else {
    label_[0] = '\0';
  }
  fired_ = false;
  Serial.printf("[ALARM] set for %lu label=%s\n",
                static_cast<unsigned long>(alarmTime_), label_);
}

void AlarmManager::clearAlarm() {
  if (ringing_) {
    M5.Speaker.stop();
    ringing_ = false;
    soundStarted_ = false;
    if (events_) events_->publish(AppEventType::AlarmCleared, millis());
  }
  alarmTime_ = 0;
  fired_ = false;
  label_[0] = '\0';
  Serial.println("[ALARM] cleared");
}

void AlarmManager::update(uint32_t nowMs) {
  // Phase 1: check the deadline once a second and fire when it arrives.
  if (alarmTime_ != 0 && !fired_ &&
      static_cast<int32_t>(nowMs - lastCheckMs_) >= 0) {
    lastCheckMs_ = nowMs + kCheckIntervalMs;
    const time_t now = time(nullptr);
    if (now >= 0 && static_cast<uint32_t>(now) >= alarmTime_) {
      fireAlarm(nowMs);
    }
  }

  // Phase 2: once ringing, play the 3-second alarm pattern by sequencing
  // tones from kAlarmPattern. Each step's duration is enforced by the next
  // tick check, so the main loop never blocks.
  if (!ringing_) return;
  if (!soundStarted_) {
    stepIndex_ = 0;
    stepEndMs_ = nowMs;
    soundStarted_ = true;
  }
  if (stepIndex_ < kAlarmStepCount &&
      static_cast<int32_t>(nowMs - stepEndMs_) >= 0) {
    const AlarmStep& step = kAlarmPattern[stepIndex_];
    if (step.frequency == 0) {
      M5.Speaker.stop();
    } else {
      M5.Speaker.tone(step.frequency, step.durationMs);
    }
    stepEndMs_ = nowMs + step.durationMs;
    ++stepIndex_;
  }
  if (stepIndex_ >= kAlarmStepCount &&
      static_cast<int32_t>(nowMs - soundStartMs_) >=
          static_cast<int32_t>(kAlarmDurationMs)) {
    M5.Speaker.stop();
    soundStarted_ = false;
    // Keep ringing_ = true so the Alarmed expression stays up until the
    // user presses a button to dismiss.
  }
}

void AlarmManager::fireAlarm(uint32_t nowMs) {
  fired_ = true;
  ringing_ = true;
  soundStartMs_ = nowMs;
  stepIndex_ = 0;
  stepEndMs_ = nowMs;
  soundStarted_ = true;
  Serial.println("[ALARM] firing");
  // Crank the speaker so the alarm is heard even at low user volume.
  M5.Speaker.setVolume(255);
  if (events_) events_->publish(AppEventType::AlarmStarted, nowMs);
}

}  // namespace firechan