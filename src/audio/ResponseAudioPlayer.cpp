#include "audio/ResponseAudioPlayer.h"

#include <M5Unified.h>
#include <SD.h>

namespace firechan {
namespace {
constexpr uint8_t kSpeakerChannel = 0;
constexpr size_t kPrebufferBytes = 4096;
constexpr uint32_t kStreamWaitTimeoutMs = 2000;

uint16_t readLe16(const uint8_t* bytes) {
  return static_cast<uint16_t>(bytes[0]) |
         (static_cast<uint16_t>(bytes[1]) << 8);
}

uint32_t readLe32(const uint8_t* bytes) {
  return static_cast<uint32_t>(bytes[0]) |
         (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) |
         (static_cast<uint32_t>(bytes[3]) << 24);
}

bool readExact(File& file, void* target, size_t length) {
  return file.read(static_cast<uint8_t*>(target), length) == length;
}
}

void ResponseAudioPlayer::begin() {
  streamMutex_ = xSemaphoreCreateMutex();
  const BaseType_t created = xTaskCreatePinnedToCore(
      taskEntry, "ember-playback", 4096, this, 2, &task_, 0);
  if (created != pdPASS) {
    task_ = nullptr;
    setError("playback task allocation failed");
    state_ = State::Failed;
  } else {
    Serial.printf("[PLAYBACK] streamer=ready buffers=%ux%u bytes ring=%u\n",
                  kBufferCount, static_cast<unsigned>(kBufferBytes),
                  static_cast<unsigned>(kStreamCapacity));
  }
}

bool ResponseAudioPlayer::play(const char* wavPath, uint8_t volume) {
  if (task_ == nullptr || state_ != State::Idle) return false;
  strlcpy(wavPath_, wavPath, sizeof(wavPath_));
  volume_ = volume;
  error_[0] = '\0';
  state_ = State::Pending;
  xTaskNotifyGive(task_);
  return true;
}

ResponseAudioEvent ResponseAudioPlayer::update() {
  if (streamStarted_) {
    streamStarted_ = false;
    return ResponseAudioEvent::StreamStarted;
  }
  if (state_ == State::Finished) {
    state_ = State::Idle;
    return ResponseAudioEvent::PlaybackFinished;
  }
  if (state_ == State::Failed) {
    state_ = State::Idle;
    return ResponseAudioEvent::PlaybackFailed;
  }
  return ResponseAudioEvent::None;
}

void ResponseAudioPlayer::setError(const char* message) {
  strlcpy(error_, message, sizeof(error_));
  Serial.printf("[PLAYBACK] ERROR: %s\n", error_);
}

// ---- StreamSink producer side (runs in the gateway task) ----

bool ResponseAudioPlayer::beginStream(uint32_t expectedBytes) {
  (void)expectedBytes;
  if (streamMutex_ == nullptr || task_ == nullptr) {
    setError("stream requested but player not ready");
    state_ = State::Failed;
    return false;
  }
  if (state_ != State::Idle) {
    setError("stream refused while player busy");
    return false;
  }
  streamEnded_ = false;
  streamAborted_ = false;
  dataBytes_ = 0;
  streamStarted_ = false;
  ringHead_ = 0;
  ringTail_ = 0;
  state_ = State::StreamPending;
  xTaskNotifyGive(task_);
  return true;
}

bool ResponseAudioPlayer::streamWrite(const uint8_t* data, size_t length) {
  return streamWriteImpl(data, length);
}

void ResponseAudioPlayer::endStream() {
  streamEnded_ = true;
  xTaskNotifyGive(task_);
}

void ResponseAudioPlayer::abortStream() {
  streamAborted_ = true;
  xTaskNotifyGive(task_);
}

// ---- Ring helpers ----

size_t ResponseAudioPlayer::ringAvailable() const {
  if (ringHead_ >= ringTail_) {
    return ringHead_ - ringTail_;
  }
  return kStreamCapacity - (ringTail_ - ringHead_);
}

bool ResponseAudioPlayer::waitForBytes(size_t wanted, uint32_t timeoutMs) {
  const uint32_t deadline = millis() + timeoutMs;
  while (ringAvailable() < wanted) {
    if (streamEnded_ || streamAborted_) break;
    if (millis() >= deadline) break;
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
  }
  return ringAvailable() >= wanted;
}

bool ResponseAudioPlayer::streamWriteImpl(const uint8_t* data, size_t length) {
  if (streamMutex_ == nullptr || task_ == nullptr) return false;
  if (streamAborted_) return false;
  xSemaphoreTake(streamMutex_, portMAX_DELAY);
  const size_t capacity = kStreamCapacity;
  size_t written = 0;
  while (written < length) {
    size_t available = ringAvailable();
    size_t freeSpace = capacity - 1 - available;
    if (freeSpace == 0) {
      xSemaphoreGive(streamMutex_);
      if (streamAborted_) return false;
      vTaskDelay(pdMS_TO_TICKS(2));
      xSemaphoreTake(streamMutex_, portMAX_DELAY);
      continue;
    }
    size_t count = length - written;
    if (count > freeSpace) count = freeSpace;
    for (size_t i = 0; i < count; ++i) {
      ring_[(ringHead_ + i) % capacity] = data[written + i];
    }
    ringHead_ = (ringHead_ + count) % capacity;
    written += count;
  }
  xSemaphoreGive(streamMutex_);
  xTaskNotifyGive(task_);
  return true;
}

// ---- Streaming playback ----

bool ResponseAudioPlayer::performStreamPlayback() {
  uint8_t header[kHeaderBytes];

  if (!waitForBytes(kHeaderBytes, kStreamWaitTimeoutMs)) {
    setError(streamAborted_ ? "stream aborted before header"
                            : streamEnded_ ? "stream ended before header"
                                           : "stream header timeout");
    return false;
  }

  // Parse the canonical 44-byte PCM header from the ring head.
  xSemaphoreTake(streamMutex_, portMAX_DELAY);
  for (size_t i = 0; i < sizeof(header); ++i) {
    header[i] = ring_[(ringTail_ + i) % kStreamCapacity];
  }
  xSemaphoreGive(streamMutex_);

  if (memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) {
    setError("stream was not RIFF/WAVE");
    return false;
  }

  const uint16_t format = readLe16(header + 20);
  const uint16_t channels = readLe16(header + 22);
  const uint32_t sampleRate = readLe32(header + 24);
  const uint16_t blockAlign = readLe16(header + 32);
  const uint16_t bitsPerSample = readLe16(header + 34);
  dataBytes_ = readLe32(header + 40);

  if (format != 1 || bitsPerSample != 16 || (channels != 1 && channels != 2) ||
      sampleRate < 8000 || sampleRate > 48000 ||
      blockAlign != channels * sizeof(int16_t) || dataBytes_ == 0 ||
      dataBytes_ % blockAlign != 0) {
    setError("stream header validation failed");
    return false;
  }

  xSemaphoreTake(streamMutex_, portMAX_DELAY);
  ringTail_ = (ringTail_ + kHeaderBytes) % kStreamCapacity;
  xSemaphoreGive(streamMutex_);

  // Wait for at least a small prebuffer so early output is gap-free; short
  // replies will simply progress as soon as data arrives.
  waitForBytes(min<size_t>(kPrebufferBytes, dataBytes_), kStreamWaitTimeoutMs);

  M5.Speaker.begin();
  M5.Speaker.setVolume(volume_);
  Serial.printf("[PLAYBACK] stream started rate=%luHz channels=%u bytes=%lu volume=%u\n",
                static_cast<unsigned long>(sampleRate), channels,
                static_cast<unsigned long>(dataBytes_), volume_);
  streamStarted_ = true;

  size_t remaining = dataBytes_;
  uint8_t bufferIndex = 0;
  bool first = true;
  uint16_t peak = 0;

  while (remaining > 0) {
    const size_t wantedBytes = min<size_t>(remaining, kBufferBytes);
    const size_t wantedSamples = wantedBytes / sizeof(int16_t);

    // Read wantedBytes from the ring into the current playback buffer. When
    // the producer has not yet delivered them, block; the M5.Speaker feed
    // below paces this loop and leaves CPU to the gateway downloader.
    size_t copied = 0;
    while (copied < wantedBytes) {
      xSemaphoreTake(streamMutex_, portMAX_DELAY);
      const size_t available = ringAvailable();
      const size_t piece = min<size_t>(available, wantedBytes - copied);
      if (piece > 0) {
        uint8_t* const target =
            reinterpret_cast<uint8_t*>(buffers_[bufferIndex]) + copied;
        const size_t capacity = kStreamCapacity;
        const size_t firstLen =
            min<size_t>(piece, capacity - ringTail_);
        memcpy(target, ring_ + ringTail_, firstLen);
        if (piece > firstLen) {
          memcpy(target + firstLen, ring_, piece - firstLen);
        }
        ringTail_ = (ringTail_ + piece) % capacity;
        copied += piece;
      }
      xSemaphoreGive(streamMutex_);

      if (copied < wantedBytes) {
        if (streamAborted_) break;
        if (streamEnded_ && ringAvailable() == 0) break;
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
      }
    }

    if (copied != wantedBytes) {
      M5.Speaker.stop(kSpeakerChannel);
      setError(streamAborted_ ? "stream aborted during playback"
                              : "stream ended during playback");
      return false;
    }

    for (size_t index = 0; index < wantedSamples; ++index) {
      const int32_t sample = buffers_[bufferIndex][index];
      const uint16_t amplitude =
          static_cast<uint16_t>(sample < 0 ? -sample : sample);
      if (amplitude > peak) peak = amplitude;
    }

    if (!M5.Speaker.playRaw(
            buffers_[bufferIndex], wantedSamples, sampleRate, channels == 2,
            1, kSpeakerChannel, first)) {
      M5.Speaker.stop(kSpeakerChannel);
      setError("stream audio feed failed");
      return false;
    }
    first = false;
    remaining -= wantedBytes;
    bufferIndex = (bufferIndex + 1U) % kBufferCount;
  }

  while (M5.Speaker.isPlaying(kSpeakerChannel)) {
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  M5.Speaker.stop(kSpeakerChannel);
  Serial.printf("[PLAYBACK] stream finished peak=%u\n", peak);
  return true;
}

// ---- Task loop and file fallback ----

void ResponseAudioPlayer::taskEntry(void* context) {
  static_cast<ResponseAudioPlayer*>(context)->taskLoop();
}

void ResponseAudioPlayer::taskLoop() {
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (state_ == State::Pending) {
      state_ = State::Playing;
      state_ = performPlayback() ? State::Finished : State::Failed;
    } else if (state_ == State::StreamPending ||
               state_ == State::StreamPlaying) {
      state_ = State::StreamPlaying;
      const bool ok = performStreamPlayback();
      if (ok) {
        state_ = State::Finished;
      } else {
        streamAborted_ = true;
        M5.Speaker.stop(kSpeakerChannel);
        state_ = State::Failed;
      }
    }
  }
}

bool ResponseAudioPlayer::performPlayback() {
  File wav = SD.open(wavPath_, FILE_READ);
  if (!wav) {
    setError("response WAV could not be opened");
    return false;
  }

  uint8_t riff[12];
  if (!readExact(wav, riff, sizeof(riff)) || memcmp(riff, "RIFF", 4) != 0 ||
      memcmp(riff + 8, "WAVE", 4) != 0) {
    wav.close();
    setError("response was not a RIFF/WAVE file");
    return false;
  }

  uint16_t format = 0;
  uint16_t channels = 0;
  uint16_t bitsPerSample = 0;
  uint16_t blockAlign = 0;
  uint32_t sampleRate = 0;
  uint32_t dataBytes = 0;
  bool foundFormat = false;
  bool foundData = false;

  while (wav.available() >= 8) {
    uint8_t chunkHeader[8];
    if (!readExact(wav, chunkHeader, sizeof(chunkHeader))) break;
    const uint32_t chunkBytes = readLe32(chunkHeader + 4);
    const size_t chunkStart = wav.position();
    if (memcmp(chunkHeader, "fmt ", 4) == 0 && chunkBytes >= 16) {
      uint8_t fmt[16];
      if (!readExact(wav, fmt, sizeof(fmt))) break;
      format = readLe16(fmt);
      channels = readLe16(fmt + 2);
      sampleRate = readLe32(fmt + 4);
      blockAlign = readLe16(fmt + 12);
      bitsPerSample = readLe16(fmt + 14);
      foundFormat = true;
    } else if (memcmp(chunkHeader, "data", 4) == 0) {
      dataBytes = chunkBytes;
      foundData = true;
      break;
    }
    const size_t nextChunk = chunkStart + chunkBytes + (chunkBytes & 1U);
    if (nextChunk > wav.size() || !wav.seek(nextChunk)) break;
  }

  if (!foundFormat || !foundData || format != 1 || bitsPerSample != 16 ||
      (channels != 1 && channels != 2) || sampleRate < 8000 || sampleRate > 48000 ||
      blockAlign != channels * sizeof(int16_t) || dataBytes == 0 ||
      dataBytes % blockAlign != 0 || wav.position() + dataBytes > wav.size()) {
    wav.close();
    setError("unsupported or damaged response WAV");
    return false;
  }

  M5.Speaker.begin();
  M5.Speaker.setVolume(volume_);
  Serial.printf("[PLAYBACK] started rate=%luHz channels=%u bytes=%lu volume=%u\n",
                static_cast<unsigned long>(sampleRate), channels,
                static_cast<unsigned long>(dataBytes), volume_);
  size_t remaining = dataBytes;
  uint8_t bufferIndex = 0;
  bool first = true;
  uint16_t peak = 0;
  while (remaining > 0) {
    size_t wanted = min(kBufferBytes, remaining);
    wanted -= wanted % blockAlign;
    const size_t count =
        wav.read(reinterpret_cast<uint8_t*>(buffers_[bufferIndex]), wanted);
    for (size_t index = 0; index < count / sizeof(int16_t); ++index) {
      const int32_t sample = buffers_[bufferIndex][index];
      const uint16_t amplitude = static_cast<uint16_t>(sample < 0 ? -sample : sample);
      if (amplitude > peak) peak = amplitude;
    }
    if (count != wanted || !M5.Speaker.playRaw(
        buffers_[bufferIndex], count / sizeof(int16_t), sampleRate, channels == 2,
        1, kSpeakerChannel, first)) {
      wav.close();
      M5.Speaker.stop(kSpeakerChannel);
      setError("response audio stream failed");
      return false;
    }
    first = false;
    remaining -= count;
    bufferIndex = (bufferIndex + 1U) % kBufferCount;
    taskYIELD();
  }
  wav.close();
  while (M5.Speaker.isPlaying(kSpeakerChannel)) {
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  Serial.printf("[PLAYBACK] finished peak=%u\n", peak);
  return true;
}

}  // namespace firechan