#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "audio/StreamSink.h"

namespace firechan {

enum class ResponseAudioEvent : uint8_t {
  None,
  StreamStarted,
  PlaybackFinished,
  PlaybackFailed
};

class ResponseAudioPlayer : public StreamSink {
 public:
  void begin();
  bool play(const char* wavPath, uint8_t volume);
  ResponseAudioEvent update();

  bool busy() const {
    return state_ == State::Pending || state_ == State::Playing ||
           state_ == State::StreamPending || state_ == State::StreamPlaying;
  }
  bool streaming() const {
    return state_ == State::StreamPending || state_ == State::StreamPlaying;
  }
  const char* error() const { return error_; }

  // StreamSink (producer side, called from the gateway task).
  bool beginStream(uint32_t expectedBytes) override;
  bool streamWrite(const uint8_t* data, size_t length) override;
  void endStream() override;
  void abortStream() override;

 private:
  enum class State : uint8_t {
    Idle, Pending, Playing, StreamPending, StreamPlaying, Finished, Failed
  };

  static void taskEntry(void* context);
  void taskLoop();
  bool performPlayback();
  bool performStreamPlayback();
  void setError(const char* message);

  size_t ringAvailable() const;
  bool waitForBytes(size_t wanted, uint32_t timeoutMs);
  bool streamWriteImpl(const uint8_t* data, size_t length);

  static constexpr size_t kPathCapacity = 64;
  static constexpr size_t kErrorCapacity = 96;
  static constexpr size_t kBufferSamples = 768;
  static constexpr size_t kBufferBytes = kBufferSamples * sizeof(int16_t);
  static constexpr uint8_t kBufferCount = 3;
  static constexpr size_t kStreamCapacity = 16384;
  static constexpr size_t kHeaderBytes = 44;

  TaskHandle_t task_ = nullptr;
  SemaphoreHandle_t streamMutex_ = nullptr;
  volatile State state_ = State::Idle;
  volatile bool streamStarted_ = false;
  char wavPath_[kPathCapacity] = {};
  char error_[kErrorCapacity] = {};
  uint8_t volume_ = 96;
  int16_t buffers_[kBufferCount][kBufferSamples] = {};

  uint8_t ring_[kStreamCapacity] = {};
  volatile size_t ringHead_ = 0;
  volatile size_t ringTail_ = 0;
  volatile bool streamEnded_ = false;
  volatile bool streamAborted_ = false;
  uint32_t dataBytes_ = 0;
};

}  // namespace firechan