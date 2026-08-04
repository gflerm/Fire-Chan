#pragma once

#include <Arduino.h>

namespace firechan::secrets {

// Copy this file to include/secrets.h and fill in the private values.
constexpr char kWifiSsid[] = "YOUR_WIFI_NAME";
constexpr char kWifiPassword[] = "YOUR_WIFI_PASSWORD";
constexpr char kEmberHost[] = "192.168.8.107";
constexpr uint16_t kEmberPort = 8088;
constexpr char kEmberToken[] = "PASTE_THE_PI_TOKEN_HERE";

}  // namespace firechan::secrets
