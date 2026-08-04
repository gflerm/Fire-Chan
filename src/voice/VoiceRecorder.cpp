#include "voice/VoiceRecorder.h"

#include <M5Unified.h>
#include <SD.h>
#include <driver/adc.h>

namespace firechan {
namespace {
constexpr char kRecordingsDirectory[] = "/recordings";
constexpr char kRecordingPath[] = "/recordings/last_prompt.wav";
constexpr char kTemporaryPath[] = "/recordings/last_prompt.tmp";

void put16(uint8_t* target, uint16_t value) {
  target[0] = value & 0xFF;
  target[1] = (value >> 8) & 0xFF;
}

void put32(uint8_t* target, uint32_t value) {
  target[0] = value & 0xFF;
  target[1] = (value >> 8) & 0xFF;
  target[2] = (value >> 16) & 0xFF;
  target[3] = (value >> 24) & 0xFF;
}
}

VoiceRecorder* VoiceRecorder::activeRecorder_ = nullptr;
portMUX_TYPE VoiceRecorder::samplerMux_ = portMUX_INITIALIZER_UNLOCKED;

void VoiceRecorder::begin(bool sdAvailable, uint8_t maxRecordingSeconds) {
  sdAvailable_ = sdAvailable;
  maxSamples_ = kSampleRate * constrain(maxRecordingSeconds, 1, 30);
  if (sdAvailable_ && !SD.exists(kRecordingsDirectory)) {
    sdAvailable_ = SD.mkdir(kRecordingsDirectory);
  }
  Serial.printf("[VOICE] recorder=%s format=16kHz/16-bit/mono max=%us block=%ums adc=gpio34\n",
                sdAvailable_ ? "ready" : "unavailable", maxRecordingSeconds,
                static_cast<unsigned>(kBlockSamples * 1000UL / kSampleRate));
}

bool VoiceRecorder::writeWavHeader(uint32_t dataBytes) {
  uint8_t header[44] = {};
  memcpy(header, "RIFF", 4);
  put32(header + 4, 36 + dataBytes);
  memcpy(header + 8, "WAVEfmt ", 8);
  put32(header + 16, 16);
  put16(header + 20, 1);
  put16(header + 22, 1);
  put32(header + 24, kSampleRate);
  put32(header + 28, kSampleRate * sizeof(int16_t));
  put16(header + 32, sizeof(int16_t));
  put16(header + 34, 16);
  memcpy(header + 36, "data", 4);
  put32(header + 40, dataBytes);
  if (!file_.seek(0)) return false;
  return file_.write(header, sizeof(header)) == sizeof(header);
}

void IRAM_ATTR VoiceRecorder::sampleTimerIsr() {
  VoiceRecorder* recorder = activeRecorder_;
  if (recorder == nullptr || !recorder->sampling_) return;

  const int32_t raw = adc1_get_raw(ADC1_CHANNEL_6);
  // Preserve headroom for the noisy analog preamp on the M5GO/Fire base.
  int32_t sample = (raw - 2048) << 3;
  if (sample < INT16_MIN) sample = INT16_MIN;
  if (sample > INT16_MAX) sample = INT16_MAX;

  portENTER_CRITICAL_ISR(&samplerMux_);
  const uint8_t buffer = recorder->writeBufferIndex_;
  const uint16_t index = recorder->writeSampleIndex_;
  if (index < kBlockSamples) {
    recorder->blocks_[buffer][index] = static_cast<int16_t>(sample);
    recorder->writeSampleIndex_ = index + 1;
  }
  if (recorder->writeSampleIndex_ >= kBlockSamples) {
    recorder->readyMask_ |= (1U << buffer);
    const uint8_t next = buffer ^ 1U;
    if (recorder->readyMask_ & (1U << next)) {
      recorder->overrun_ = true;
    } else {
      recorder->writeBufferIndex_ = next;
      recorder->writeSampleIndex_ = 0;
    }
  }
  portEXIT_CRITICAL_ISR(&samplerMux_);
}

bool VoiceRecorder::startSampler() {
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_12);
  activeRecorder_ = this;
  sampleTimer_ = timerBegin(0, 8, true);  // 10 MHz timer clock.
  if (sampleTimer_ == nullptr) return false;
  timerAttachInterrupt(sampleTimer_, &VoiceRecorder::sampleTimerIsr, false);
  timerAlarmWrite(sampleTimer_, 625, true);  // Exact 16 kHz cadence.
  sampling_ = true;
  timerAlarmEnable(sampleTimer_);
  return true;
}

void VoiceRecorder::stopSampler() {
  if (!sampling_) return;
  sampling_ = false;
  if (sampleTimer_ != nullptr) {
    timerAlarmDisable(sampleTimer_);
    timerDetachInterrupt(sampleTimer_);
    timerEnd(sampleTimer_);
    sampleTimer_ = nullptr;
  }
  portENTER_CRITICAL(&samplerMux_);
  finalPartialSamples_ = writeSampleIndex_;
  portEXIT_CRITICAL(&samplerMux_);
  activeRecorder_ = nullptr;
}

bool VoiceRecorder::start(uint32_t nowMs) {
  if (!sdAvailable_ || recording_) return false;
  if (SD.exists(kTemporaryPath) && !SD.remove(kTemporaryPath)) return false;
  file_ = SD.open(kTemporaryPath, FILE_WRITE);
  if (!file_ || !writeWavHeader(0)) {
    if (file_) file_.close();
    return false;
  }

  samplesWritten_ = 0;
  lastPeak_ = 0;
  stopRequested_ = false;
  sampling_ = false;
  overrun_ = false;
  partialWritten_ = false;
  writeBufferIndex_ = 0;
  writeSampleIndex_ = 0;
  readyMask_ = 0;
  readBufferIndex_ = 0;
  finalPartialSamples_ = 0;
  startedMs_ = nowMs;
  M5.Mic.end();
  if (!startSampler()) {
    file_.close();
    SD.remove(kTemporaryPath);
    return false;
  }
  recording_ = true;
  Serial.println("[VOICE] recording started; release A to stop");
  return true;
}

void VoiceRecorder::requestStop() {
  if (recording_) stopRequested_ = true;
}

bool VoiceRecorder::writeBuffer(uint8_t index, size_t sampleCount) {
  uint16_t blockPeak = 0;
  for (size_t i = 0; i < sampleCount; ++i) {
    const int32_t sample = blocks_[index][i];
    const uint16_t amplitude = static_cast<uint16_t>(sample < 0 ? -sample : sample);
    if (amplitude > blockPeak) blockPeak = amplitude;
  }
  if (blockPeak > lastPeak_) lastPeak_ = blockPeak;
  const size_t bytes = sampleCount * sizeof(int16_t);
  if (file_.write(reinterpret_cast<const uint8_t*>(blocks_[index]), bytes) != bytes) {
    return false;
  }
  samplesWritten_ += sampleCount;
  return true;
}

VoiceRecorderEvent VoiceRecorder::finish() {
  const uint32_t dataBytes = samplesWritten_ * sizeof(int16_t);
  const bool headerOk = writeWavHeader(dataBytes);
  file_.flush();
  const bool writeOk = file_.getWriteError() == 0;
  file_.close();
  stopSampler();
  recording_ = false;

  if (!headerOk || !writeOk) return fail("WAV finalization failed");
  if (SD.exists(kRecordingPath) && !SD.remove(kRecordingPath)) {
    return fail("previous recording could not be replaced");
  }
  if (!SD.rename(kTemporaryPath, kRecordingPath)) return fail("recording rename failed");

  lastDataBytes_ = dataBytes;
  lastDurationMs_ = samplesWritten_ * 1000UL / kSampleRate;
  Serial.printf("[VOICE] ready path=%s duration=%lums bytes=%lu peak=%u\n",
                kRecordingPath, lastDurationMs_, lastDataBytes_, lastPeak_);
  return VoiceRecorderEvent::RecordingReady;
}

VoiceRecorderEvent VoiceRecorder::fail(const char* reason) {
  if (file_) file_.close();
  stopSampler();
  recording_ = false;
  if (SD.exists(kTemporaryPath)) SD.remove(kTemporaryPath);
  Serial.printf("[VOICE] ERROR: %s\n", reason);
  return VoiceRecorderEvent::RecordingFailed;
}

VoiceRecorderEvent VoiceRecorder::update(uint32_t nowMs) {
  if (!recording_) return VoiceRecorderEvent::None;
  if (nowMs - startedMs_ >= maxSamples_ * 1000UL / kSampleRate || overrun_) {
    stopRequested_ = true;
  }
  if (stopRequested_ && sampling_) stopSampler();

  uint8_t readyMask;
  portENTER_CRITICAL(&samplerMux_);
  readyMask = readyMask_;
  portEXIT_CRITICAL(&samplerMux_);

  if (readyMask & (1U << readBufferIndex_)) {
    const uint8_t completedBuffer = readBufferIndex_;
    portENTER_CRITICAL(&samplerMux_);
    readyMask_ &= ~(1U << completedBuffer);
    portEXIT_CRITICAL(&samplerMux_);
    readBufferIndex_ ^= 1U;
    if (!writeBuffer(completedBuffer, kBlockSamples)) return fail("microSD write failed");
    return VoiceRecorderEvent::None;
  }

  if (!sampling_ && !partialWritten_) {
    partialWritten_ = true;
    if (finalPartialSamples_ > 0 &&
        !writeBuffer(writeBufferIndex_, finalPartialSamples_)) {
      return fail("microSD partial write failed");
    }
  }
  if (!sampling_ && readyMask_ == 0 && partialWritten_) {
    if (overrun_) return fail("capture buffer overrun");
    return finish();
  }
  return VoiceRecorderEvent::None;
}

}  // namespace firechan
