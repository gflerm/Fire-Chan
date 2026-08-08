#pragma once

#include <Arduino.h>

#include "app/EventBus.h"

namespace firechan {

class AlarmManager {
 public:
  // Set a one-shot alarm to fire at ``alarmTime`` (Unix seconds, UTC). The
  // label is shown in serial logs only. ``alarmTime == 0`` clears the alarm.
  void setAlarm(uint32_t alarmTime, const char* label);

  // Cancel any scheduled or ringing alarm.
  void clearAlarm();

  // Called from the main loop every tick. Checks the current time against
  // the stored alarm deadline and plays the 3-second alarm tone pattern
  // when it fires. The pattern is sequenced by the tick counter so the main
  // loop never blocks.
  void update(uint32_t nowMs);

  bool hasAlarm() const { return alarmTime_ != 0; }
  bool isRinging() const { return ringing_; }
  // True for one tick after a stale deadline was cleared, so the caller
  // can persist the cleared state to NVS.
  bool wasCleared() const { return cleared_; }
  uint32_t alarmTime() const { return alarmTime_; }

 private:
  void fireAlarm(uint32_t nowMs);
  void startTone(uint16_t frequency, uint16_t durationMs);
  void silence(uint16_t durationMs);

  EventBus* events_ = nullptr;
  uint32_t alarmTime_ = 0;  // Unix seconds, UTC. 0 = no alarm.
  uint32_t lastCheckMs_ = 0;
  uint32_t soundStartMs_ = 0;
  uint32_t stepEndMs_ = 0;
  uint8_t stepIndex_ = 0;
  bool ringing_ = false;
  bool fired_ = false;
  bool soundStarted_ = false;
  bool cleared_ = false;
  char label_[32] = {};
};

}  // namespace firechan