#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace firechan {

enum class ResponseAudioEvent : uint8_t {
  None,
  PlaybackFinished,
  PlaybackFailed
};

class ResponseAudioPlayer {
 public:
  void begin();
  bool play(const char* wavPath, uint8_t volume);
  ResponseAudioEvent update();

  bool busy() const { return state_ == State::Pending || state_ == State::Playing; }
  const char* error() const { return error_; }

 private:
  enum class State : uint8_t { Idle, Pending, Playing, Finished, Failed };

  static void taskEntry(void* context);
  void taskLoop();
  bool performPlayback();
  void setError(const char* message);

  static constexpr size_t kPathCapacity = 64;
  static constexpr size_t kErrorCapacity = 96;
  static constexpr size_t kBufferSamples = 768;

  TaskHandle_t task_ = nullptr;
  volatile State state_ = State::Idle;
  char wavPath_[kPathCapacity] = {};
  char error_[kErrorCapacity] = {};
  uint8_t volume_ = 96;
  int16_t buffers_[3][kBufferSamples] = {};
};

}  // namespace firechan
