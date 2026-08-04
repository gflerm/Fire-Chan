#include "audio/ResponseAudioPlayer.h"

#include <M5Unified.h>
#include <SD.h>

namespace firechan {
namespace {
constexpr uint8_t kSpeakerChannel = 0;

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
  const BaseType_t created = xTaskCreatePinnedToCore(
      taskEntry, "ember-playback", 4096, this, 2, &task_, 0);
  if (created != pdPASS) {
    task_ = nullptr;
    setError("playback task allocation failed");
    state_ = State::Failed;
  } else {
    Serial.printf("[PLAYBACK] streamer=ready buffers=%ux%u bytes\n", 3U,
                  static_cast<unsigned>(sizeof(buffers_[0])));
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

void ResponseAudioPlayer::taskEntry(void* context) {
  static_cast<ResponseAudioPlayer*>(context)->taskLoop();
}

void ResponseAudioPlayer::taskLoop() {
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (state_ != State::Pending) continue;
    state_ = State::Playing;
    state_ = performPlayback() ? State::Finished : State::Failed;
  }
}

void ResponseAudioPlayer::setError(const char* message) {
  strlcpy(error_, message, sizeof(error_));
  Serial.printf("[PLAYBACK] ERROR: %s\n", error_);
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
                sampleRate, channels, dataBytes, volume_);
  size_t remaining = dataBytes;
  uint8_t bufferIndex = 0;
  bool first = true;
  uint16_t peak = 0;
  while (remaining > 0) {
    size_t wanted = min(sizeof(buffers_[0]), remaining);
    wanted -= wanted % blockAlign;
    const size_t count = wav.read(reinterpret_cast<uint8_t*>(buffers_[bufferIndex]), wanted);
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
    bufferIndex = (bufferIndex + 1U) % 3U;
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
