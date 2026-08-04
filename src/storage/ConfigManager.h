#pragma once

#include <Arduino.h>

#include "config/AppConfig.h"

namespace firechan {

class ConfigManager {
 public:
  void begin(AppConfig& config);
  void markDirty(uint32_t nowMs);
  void update(uint32_t nowMs, const AppConfig& config);

  bool sdAvailable() const { return sdAvailable_; }
  bool dirty() const { return dirty_; }

 private:
  bool loadNvs(AppConfig& config);
  bool saveNvs(const AppConfig& config);
  bool mountSd();
  bool writeSdBackup(const AppConfig& config);

  uint32_t saveAfterMs_ = 0;
  bool sdAvailable_ = false;
  bool dirty_ = false;
};

}  // namespace firechan
