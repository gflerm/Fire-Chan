#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace firechan {

enum class VoiceGatewayEvent : uint8_t {
  None,
  ResponseReady,
  RequestFailed
};

class VoiceGatewayClient {
 public:
  void begin();
  bool submit(const char* recordingPath);
  VoiceGatewayEvent update();

  bool busy() const { return state_ == State::Pending || state_ == State::Working; }
  const char* transcript() const { return transcript_; }
  const char* reply() const { return reply_; }
  const char* expression() const { return expression_; }
  const char* action() const { return action_; }
  const char* error() const { return error_; }

 private:
  enum class State : uint8_t { Idle, Pending, Working, Ready, Failed };

  static void taskEntry(void* context);
  void taskLoop();
  bool performRequest();
  void setError(const char* message);

  static constexpr size_t kPathCapacity = 64;
  static constexpr size_t kTranscriptCapacity = 384;
  static constexpr size_t kReplyCapacity = 768;
  static constexpr size_t kHintCapacity = 24;
  static constexpr size_t kErrorCapacity = 96;

  TaskHandle_t task_ = nullptr;
  volatile State state_ = State::Idle;
  char recordingPath_[kPathCapacity] = {};
  char transcript_[kTranscriptCapacity] = {};
  char reply_[kReplyCapacity] = {};
  char expression_[kHintCapacity] = {};
  char action_[kHintCapacity] = {};
  char error_[kErrorCapacity] = {};
};

}  // namespace firechan
