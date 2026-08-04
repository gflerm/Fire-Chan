#include "app/EventBus.h"

namespace firechan {

bool EventBus::publish(AppEventType type, uint32_t timestampMs) {
  if (type == AppEventType::None) return false;
  if (count_ >= kCapacity) {
    ++dropped_;
    Serial.printf("[EVENT] DROP type=%s total=%u\n", appEventName(type), dropped_);
    return false;
  }
  events_[writeIndex_].type = type;
  events_[writeIndex_].timestampMs = timestampMs;
  writeIndex_ = (writeIndex_ + 1) % kCapacity;
  ++count_;
  return true;
}

bool EventBus::next(AppEvent& event) {
  if (count_ == 0) return false;
  event = events_[readIndex_];
  readIndex_ = (readIndex_ + 1) % kCapacity;
  --count_;
  return true;
}

}  // namespace firechan

