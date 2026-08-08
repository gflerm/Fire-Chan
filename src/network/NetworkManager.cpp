#include "network/NetworkManager.h"

#include <WiFi.h>
#include <time.h>

#include "secrets.h"

namespace firechan {
namespace {
constexpr char kNtpServer[] = "pool.ntp.org";
constexpr long gmtOffsetSeconds = 2 * 3600;  // Africa/Johannesburg (SAST)
constexpr int daylightOffsetSeconds = 0;
}

void NetworkManager::begin(uint32_t nowMs) {
  configured_ = secrets::kWifiSsid[0] != '\0' &&
                strcmp(secrets::kWifiSsid, "YOUR_WIFI_NAME") != 0;
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  if (configured_) {
    connect(nowMs);
  } else {
    Serial.println("[NETWORK] Wi-Fi credentials are not configured");
  }
}

void NetworkManager::connect(uint32_t nowMs) {
  Serial.printf("[NETWORK] connecting ssid=%s\n", secrets::kWifiSsid);
  WiFi.begin(secrets::kWifiSsid, secrets::kWifiPassword);
  nextReconnectMs_ = nowMs + kReconnectDelayMs;
}

bool NetworkManager::connected() const {
  return WiFi.status() == WL_CONNECTED;
}

void NetworkManager::update(uint32_t nowMs, EventBus& events) {
  const bool isConnected = connected();
  if (isConnected != reportedConnected_) {
    reportedConnected_ = isConnected;
    if (isConnected) {
      const String ip = WiFi.localIP().toString();
      strlcpy(address_, ip.c_str(), sizeof(address_));
      Serial.printf("[NETWORK] online ip=%s rssi=%ddBm\n", address_, WiFi.RSSI());
      events.publish(AppEventType::NetworkOnline, nowMs);
      // Sync the system clock via NTP so the alarm can fire at the right time.
      // Africa/Johannesburg (SAST) is UTC+2 with no DST.
      configTime(gmtOffsetSeconds, daylightOffsetSeconds, kNtpServer);
      Serial.println("[NETWORK] NTP sync requested");
    } else {
      strlcpy(address_, "0.0.0.0", sizeof(address_));
      Serial.println("[NETWORK] offline");
      events.publish(AppEventType::NetworkOffline, nowMs);
    }
  }

  if (configured_ && !isConnected &&
      static_cast<int32_t>(nowMs - nextReconnectMs_) >= 0) {
    WiFi.disconnect();
    connect(nowMs);
  }
}

}  // namespace firechan
