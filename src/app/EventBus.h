#pragma once

#include <Arduino.h>

#include "app/AppEvent.h"

namespace firechan {

class EventBus {
 public:
  bool publish(AppEventType type, uint32_t timestampMs);
  bool next(AppEvent& event);
  uint8_t pending() const { return count_; }
  uint32_t dropped() const { return dropped_; }

 private:
  static constexpr uint8_t kCapacity = 16;
  AppEvent events_[kCapacity];
  uint8_t readIndex_ = 0;
  uint8_t writeIndex_ = 0;
  uint8_t count_ = 0;
  uint32_t dropped_ = 0;
};

}  // namespace firechan

