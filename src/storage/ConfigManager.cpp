#include "storage/ConfigManager.h"

#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <esp_random.h>

namespace firechan {
namespace {
constexpr char kPreferencesNamespace[] = "firechan";
constexpr char kConfigDirectory[] = "/config";
constexpr char kBackupPath[] = "/config/device.json";
constexpr char kTemporaryPath[] = "/config/device.tmp";
constexpr uint8_t kSdChipSelect = 4;
constexpr uint32_t kSaveDelayMs = 1500;
constexpr char kHexDigits[] = "0123456789abcdef";

bool ensureDeviceId(AppConfig& config) {
  if (config.deviceId[0] != '\0') return true;
  uint8_t bytes[16];
  for (uint8_t& byte : bytes) byte = static_cast<uint8_t>(esp_random() & 0xFF);
  size_t index = 0;
  for (uint8_t byte : bytes) {
    config.deviceId[index++] = kHexDigits[byte >> 4];
    config.deviceId[index++] = kHexDigits[byte & 0x0F];
  }
  config.deviceId[index] = '\0';
  return config.deviceId[0] != '\0';
}
}

bool ConfigManager::loadNvs(AppConfig& config) {
  Preferences preferences;
  if (!preferences.begin(kPreferencesNamespace, true)) return false;
  const uint16_t schema = preferences.getUShort("schema", 0);
  if (schema != AppConfig::kSchemaVersion) {
    preferences.end();
    return false;
  }

  config.displayBrightnessPercent = preferences.getUChar("display", 70);
  config.volumePercent = preferences.getUChar("volume", 65);
  config.rgbBrightnessPercent = preferences.getUChar("rgb", 18);
  config.gestureSensitivityPercent = preferences.getUChar("gesture", 100);
  config.sleepyAfterSeconds = preferences.getUShort("sleepy", 30);
  config.sleepingAfterSeconds = preferences.getUShort("sleeping", 60);
  config.maxRecordingSeconds = preferences.getUChar("maxvoice", 12);
  config.muted = preferences.getBool("muted", false);
  config.demoMode = preferences.getBool("demo", false);
  preferences.getString("deviceid", config.deviceId, sizeof(config.deviceId));
  preferences.end();
  config.validate();
  return true;
}

bool ConfigManager::saveNvs(const AppConfig& config) {
  Preferences preferences;
  if (!preferences.begin(kPreferencesNamespace, false)) return false;
  bool ok = true;
  ok &= preferences.putUShort("schema", AppConfig::kSchemaVersion) > 0;
  ok &= preferences.putUChar("display", config.displayBrightnessPercent) > 0;
  ok &= preferences.putUChar("volume", config.volumePercent) > 0;
  ok &= preferences.putUChar("rgb", config.rgbBrightnessPercent) > 0;
  ok &= preferences.putUChar("gesture", config.gestureSensitivityPercent) > 0;
  ok &= preferences.putUShort("sleepy", config.sleepyAfterSeconds) > 0;
  ok &= preferences.putUShort("sleeping", config.sleepingAfterSeconds) > 0;
  ok &= preferences.putUChar("maxvoice", config.maxRecordingSeconds) > 0;
  ok &= preferences.putBool("muted", config.muted) > 0;
  ok &= preferences.putBool("demo", config.demoMode) > 0;
  ok &= preferences.putString("deviceid", config.deviceId) > 0;
  preferences.end();
  return ok;
}

bool ConfigManager::mountSd() {
  if (!SD.begin(kSdChipSelect, SPI, 25000000)) return false;
  if (!SD.exists(kConfigDirectory) && !SD.mkdir(kConfigDirectory)) return false;
  return true;
}

uint64_t ConfigManager::sdFreeBytes() const {
  if (!sdAvailable_) return 0;
  return SD.totalBytes() > SD.usedBytes() ? SD.totalBytes() - SD.usedBytes() : 0;
}

uint64_t ConfigManager::sdTotalBytes() const {
  return sdAvailable_ ? SD.totalBytes() : 0;
}

bool ConfigManager::writeSdBackup(const AppConfig& config) {
  if (!sdAvailable_) return false;
  if (SD.exists(kTemporaryPath) && !SD.remove(kTemporaryPath)) return false;
  File file = SD.open(kTemporaryPath, FILE_WRITE);
  if (!file) return false;

  file.printf("{\n");
  file.printf("  \"schema_version\": %u,\n", AppConfig::kSchemaVersion);
  file.printf("  \"device\": {\n");
  file.printf("    \"name\": \"Fire-chan\",\n");
  file.printf("    \"deviceId\": \"%s\",\n", config.deviceId[0] ? config.deviceId : "unset");
  file.printf("    \"timezone\": \"Africa/Johannesburg\",\n");
  file.printf("    \"brightness\": %u,\n", config.displayBrightnessPercent);
  file.printf("    \"volume\": %u,\n", config.volumePercent);
  file.printf("    \"mute\": %s\n", config.muted ? "true" : "false");
  file.printf("  },\n");
  file.printf("  \"face\": {\n");
  file.printf("    \"demo_mode\": %s,\n", config.demoMode ? "true" : "false");
  file.printf("    \"rgb_brightness\": %u\n", config.rgbBrightnessPercent);
  file.printf("  },\n");
  file.printf("  \"gestures\": {\n");
  file.printf("    \"sensitivity\": %u,\n", config.gestureSensitivityPercent);
  file.printf("    \"sleepy_after_seconds\": %u,\n", config.sleepyAfterSeconds);
  file.printf("    \"sleeping_after_seconds\": %u\n", config.sleepingAfterSeconds);
  file.printf("  },\n");
  file.printf("  \"assistant\": {\n");
  file.printf("    \"push_to_talk\": true,\n");
  file.printf("    \"max_recording_seconds\": %u\n", config.maxRecordingSeconds);
  file.printf("  }\n");
  file.printf("}\n");
  file.flush();
  const bool writeOk = file.getWriteError() == 0;
  file.close();
  if (!writeOk) return false;

  if (SD.exists(kBackupPath) && !SD.remove(kBackupPath)) return false;
  return SD.rename(kTemporaryPath, kBackupPath);
}

void ConfigManager::begin(AppConfig& config) {
  const bool loaded = loadNvs(config);
  if (!loaded) {
    config = AppConfig{};
    config.validate();
    Serial.printf("[CONFIG] NVS defaults save=%s\n", saveNvs(config) ? "ok" : "failed");
  } else {
    Serial.println("[CONFIG] NVS settings loaded");
  }

  if (config.deviceId[0] == '\0') {
    if (ensureDeviceId(config)) {
      Serial.printf("[CONFIG] deviceId generated, persistence=%s\n",
                    saveNvs(config) ? "ok" : "failed");
    } else {
      Serial.println("[CONFIG] deviceId generation failed");
    }
  }
  Serial.printf("[CONFIG] deviceId=%s\n", config.deviceId[0]
                  ? config.deviceId : "unset");

  sdAvailable_ = mountSd();
  Serial.printf("[CONFIG] microSD=%s backup=%s\n",
                sdAvailable_ ? "ready" : "unavailable",
                writeSdBackup(config) ? "ok" : "skipped/failed");
  Serial.printf("[CONFIG] display=%u%% volume=%u%% muted=%s demo=%s rgb=%u%% gesture=%u%% idle=%us/%us voice=%us\n",
                config.displayBrightnessPercent, config.volumePercent,
                config.muted ? "true" : "false",
                config.demoMode ? "true" : "false",
                config.rgbBrightnessPercent, config.gestureSensitivityPercent,
                config.sleepyAfterSeconds, config.sleepingAfterSeconds,
                config.maxRecordingSeconds);
}

void ConfigManager::markDirty(uint32_t nowMs) {
  dirty_ = true;
  saveAfterMs_ = nowMs + kSaveDelayMs;
}

void ConfigManager::update(uint32_t nowMs, const AppConfig& config) {
  if (!dirty_ || static_cast<int32_t>(nowMs - saveAfterMs_) < 0) return;
  const bool nvsOk = saveNvs(config);
  const bool sdOk = writeSdBackup(config);
  dirty_ = !nvsOk;
  if (dirty_) saveAfterMs_ = nowMs + kSaveDelayMs;
  Serial.printf("[CONFIG] persisted nvs=%s sd=%s\n", nvsOk ? "ok" : "failed",
                sdAvailable_ ? (sdOk ? "ok" : "failed") : "unavailable");
}

}  // namespace firechan
