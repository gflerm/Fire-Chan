#pragma once

#include <Arduino.h>

namespace firechan {

struct AppConfig {
  static constexpr uint16_t kSchemaVersion = 1;

  uint8_t displayBrightnessPercent = 70;
  uint8_t volumePercent = 65;
  uint8_t rgbBrightnessPercent = 18;
  uint8_t gestureSensitivityPercent = 100;
  uint16_t sleepyAfterSeconds = 30;
  uint16_t sleepingAfterSeconds = 60;
  uint8_t maxRecordingSeconds = 12;
  bool muted = false;
  bool demoMode = false;

  void validate() {
    displayBrightnessPercent = constrain(displayBrightnessPercent, 10, 100);
    volumePercent = constrain(volumePercent, 0, 100);
    rgbBrightnessPercent = constrain(rgbBrightnessPercent, 0, 100);
    gestureSensitivityPercent = constrain(gestureSensitivityPercent, 50, 150);
    sleepyAfterSeconds = constrain(sleepyAfterSeconds, 10, 3600);
    sleepingAfterSeconds = constrain(sleepingAfterSeconds,
                                     static_cast<uint16_t>(sleepyAfterSeconds + 5),
                                     static_cast<uint16_t>(7200));
    maxRecordingSeconds = constrain(maxRecordingSeconds, 1, 30);
  }
};

}  // namespace firechan
