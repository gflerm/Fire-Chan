#include <Arduino.h>
#include <M5Unified.h>

#include "app/Application.h"

firechan::Application application;

void setup() {
  auto config = M5.config();
  config.serial_baudrate = 115200;
  config.clear_display = true;
  config.internal_imu = true;
  config.internal_mic = true;
  config.internal_spk = true;
  M5.begin(config);
  application.begin();
}

void loop() {
  M5.update();
  application.update();
  delay(1);
}

