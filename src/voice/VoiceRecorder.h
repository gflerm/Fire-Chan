#pragma once

#include <Arduino.h>
#include <FS.h>
#include <freertos/FreeRTOS.h>

namespace firechan {

enum class VoiceRecorderEvent : uint8_t {
  None,
  RecordingReady,
  RecordingFailed
};

class VoiceRecorder {
 public:
  void begin(bool sdAvailable, uint8_t maxRecordingSeconds);
  bool start(uint32_t nowMs);
  void requestStop();
  VoiceRecorderEvent update(uint32_t nowMs);

  bool recording() const { return recording_; }
  uint32_t lastDurationMs() const { return lastDurationMs_; }
  uint32_t lastDataBytes() const { return lastDataBytes_; }
  uint16_t lastPeak() const { return lastPeak_; }
  const char* recordingPath() const { return "/recordings/last_prompt.wav"; }

 private:
  static void IRAM_ATTR sampleTimerIsr();
  bool startSampler();
  void stopSampler();
  bool writeBuffer(uint8_t index, size_t sampleCount);
  VoiceRecorderEvent finish();
  VoiceRecorderEvent fail(const char* reason);
  bool writeWavHeader(uint32_t dataBytes);

  static constexpr uint32_t kSampleRate = 16000;
  static constexpr size_t kBlockSamples = 4096;

  static VoiceRecorder* activeRecorder_;
  static portMUX_TYPE samplerMux_;

  File file_;
  int16_t blocks_[2][kBlockSamples] = {};
  uint32_t startedMs_ = 0;
  uint32_t samplesWritten_ = 0;
  uint32_t maxSamples_ = kSampleRate * 12;
  uint32_t lastDurationMs_ = 0;
  uint32_t lastDataBytes_ = 0;
  uint16_t lastPeak_ = 0;
  hw_timer_t* sampleTimer_ = nullptr;
  bool sdAvailable_ = false;
  bool recording_ = false;
  bool stopRequested_ = false;
  volatile bool sampling_ = false;
  volatile bool overrun_ = false;
  bool partialWritten_ = false;
  volatile uint8_t writeBufferIndex_ = 0;
  volatile uint16_t writeSampleIndex_ = 0;
  volatile uint8_t readyMask_ = 0;
  uint8_t readBufferIndex_ = 0;
  uint16_t finalPartialSamples_ = 0;
};

}  // namespace firechan
