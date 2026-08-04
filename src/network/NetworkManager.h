#pragma once

#include <Arduino.h>

#include "app/EventBus.h"

namespace firechan {

class NetworkManager {
 public:
  void begin(uint32_t nowMs);
  void update(uint32_t nowMs, EventBus& events);

  bool connected() const;
  const char* address() const { return address_; }

 private:
  void connect(uint32_t nowMs);

  static constexpr uint32_t kReconnectDelayMs = 10000;
  uint32_t nextReconnectMs_ = 0;
  // Start opposite the actual boot state so the first update emits Offline.
  bool reportedConnected_ = true;
  bool configured_ = false;
  char address_[16] = "0.0.0.0";
};

}  // namespace firechan
