#include "assistant/VoiceGatewayClient.h"

#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <WiFi.h>

#include "secrets.h"

namespace firechan {
namespace {
constexpr char kBoundary[] = "----FireChanVoiceBoundary";
constexpr char kVoicePath[] = "/v1/voice";
constexpr uint32_t kRequestTimeoutMs = 180000;
constexpr uint32_t kDownloadTimeoutMs = 60000;
constexpr size_t kMaximumAudioBytes = 4 * 1024 * 1024;
constexpr char kResponseAudioPath[] = "/cache/ember_response.wav";
constexpr char kTemporaryAudioPath[] = "/cache/ember_response.tmp";
}

void VoiceGatewayClient::begin() {
  const BaseType_t created = xTaskCreatePinnedToCore(
      taskEntry, "ember-gateway", 8192, this, 1, &task_, 0);
  if (created != pdPASS) {
    task_ = nullptr;
    setError("gateway task allocation failed");
    state_ = State::Failed;
  } else {
    Serial.printf("[ASSISTANT] gateway=http://%s:%u task=ready\n",
                  secrets::kEmberHost, secrets::kEmberPort);
  }
}

bool VoiceGatewayClient::submit(const char* recordingPath) {
  if (task_ == nullptr || state_ != State::Idle || WiFi.status() != WL_CONNECTED) {
    return false;
  }
  if (secrets::kEmberToken[0] == '\0' ||
      strcmp(secrets::kEmberToken, "PASTE_THE_PI_TOKEN_HERE") == 0) {
    setError("Ember token is not configured");
    state_ = State::Failed;
    return false;
  }
  strlcpy(recordingPath_, recordingPath, sizeof(recordingPath_));
  transcript_[0] = reply_[0] = expression_[0] = action_[0] = error_[0] = '\0';
  streamedThisTurn_ = false;
  state_ = State::Pending;
  xTaskNotifyGive(task_);
  return true;
}

void VoiceGatewayClient::setDeviceId(const char* id) {
  strlcpy(deviceId_, id ? id : "", sizeof(deviceId_));
}

void VoiceGatewayClient::setStreamSink(StreamSink* sink) {
  streamSink_ = sink;
}

VoiceGatewayEvent VoiceGatewayClient::update() {
  if (state_ == State::Ready) {
    state_ = State::Idle;
    return VoiceGatewayEvent::ResponseReady;
  }
  if (state_ == State::Failed) {
    state_ = State::Idle;
    return VoiceGatewayEvent::RequestFailed;
  }
  return VoiceGatewayEvent::None;
}

void VoiceGatewayClient::taskEntry(void* context) {
  static_cast<VoiceGatewayClient*>(context)->taskLoop();
}

void VoiceGatewayClient::taskLoop() {
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (state_ != State::Pending) continue;
    state_ = State::Working;
    state_ = performRequest() ? State::Ready : State::Failed;
  }
}

void VoiceGatewayClient::setError(const char* message) {
  strlcpy(error_, message, sizeof(error_));
  Serial.printf("[ASSISTANT] ERROR: %s\n", error_);
}

bool VoiceGatewayClient::downloadAudio(const char* audioUrl) {
  if (audioUrl == nullptr || strncmp(audioUrl, "/v1/audio/", 10) != 0) {
    setError("gateway audio URL was invalid");
    return false;
  }
  if (!SD.exists("/cache") && !SD.mkdir("/cache")) {
    setError("audio cache directory failed");
    return false;
  }
  if (SD.exists(kTemporaryAudioPath) && !SD.remove(kTemporaryAudioPath)) {
    setError("old audio cache could not be cleared");
    return false;
  }

  WiFiClient transport;
  HttpClient request(transport, secrets::kEmberHost, secrets::kEmberPort);
  request.setHttpResponseTimeout(kDownloadTimeoutMs);
  request.beginRequest();
  request.get(audioUrl);
  request.sendHeader("X-Ember-Token", secrets::kEmberToken);
  request.sendHeader("Connection", "close");
  request.endRequest();

  const int status = request.responseStatusCode();
  const long contentLength = request.contentLength();
  const bool chunked = request.isResponseChunked();
  const bool streamed = chunked || contentLength <= 0;
  Serial.printf("[ASSISTANT] audio response status=%d length=%ld chunked=%s\n",
                status, contentLength, chunked ? "true" : "false");
  if (status != 200 || (!streamed && (contentLength < 44 ||
      contentLength > static_cast<long>(kMaximumAudioBytes)))) {
    request.stop();
    setError(status == 200 ? "response audio size was invalid" : "response audio download failed");
    return false;
  }

  File output = SD.open(kTemporaryAudioPath, FILE_WRITE);
  if (!output) {
    request.stop();
    setError("response audio cache could not be opened");
    return false;
  }

  // The player can begin playback while the download is still in flight if a
  // sink is attached (unmuted path). The canonical header leads the stream.
  bool feeding = false;
  if (streamSink_ != nullptr) {
    feeding = streamSink_->beginStream(
        streamed ? 0u : static_cast<uint32_t>(contentLength));
  }
  streamedThisTurn_ = feeding;

  // Batch network fragments into SD-friendly blocks. Writing every small TCP
  // fragment directly to the card made response downloads the dominant delay.
  size_t buffered = 0;
  size_t received = 0;
  bool transferOk = true;
  uint32_t lastDataMs = millis();
  while (streamed || received < static_cast<size_t>(contentLength)) {
    const int available = request.available();
    if (available > 0) {
      const size_t freeSpace = kDownloadBufferSize - buffered;
      size_t wanted = min(freeSpace, static_cast<size_t>(available));
      if (!streamed) {
        wanted = min(wanted, static_cast<size_t>(contentLength) - received);
      }
      int count = 0;
      if (chunked) {
        // ArduinoHttpClient only updates its chunk decoder on single-byte reads.
        while (count < static_cast<int>(wanted)) {
          const int value = request.read();
          if (value < 0) break;
          downloadBuffer_[buffered + count++] = static_cast<uint8_t>(value);
        }
      } else {
        count = request.read(downloadBuffer_ + buffered, wanted);
      }
      if (count <= 0 || received + count > kMaximumAudioBytes) {
        transferOk = false;
        break;
      }
      buffered += static_cast<size_t>(count);
      received += count;
      lastDataMs = millis();
      if (buffered == kDownloadBufferSize) {
        if (output.write(downloadBuffer_, buffered) != buffered) {
          transferOk = false;
          break;
        }
        if (feeding && !streamSink_->streamWrite(downloadBuffer_, buffered)) {
          feeding = false;
        }
        buffered = 0;
      }
    } else {
      if (!request.connected() || millis() - lastDataMs > kDownloadTimeoutMs) break;
      vTaskDelay(pdMS_TO_TICKS(2));
    }
  }
  if (transferOk && buffered > 0 &&
      output.write(downloadBuffer_, buffered) != buffered) {
    transferOk = false;
  }
  if (feeding && buffered > 0 &&
      !streamSink_->streamWrite(downloadBuffer_, buffered)) {
    feeding = false;
  }
  output.flush();
  const bool writeOk = transferOk && output.getWriteError() == 0;
  output.close();
  request.stop();
  if (!writeOk || received < 44 ||
      (!streamed && received != static_cast<size_t>(contentLength))) {
    if (feeding) streamSink_->abortStream();
    SD.remove(kTemporaryAudioPath);
    setError("response audio was incomplete");
    return false;
  }
  if (SD.exists(kResponseAudioPath) && !SD.remove(kResponseAudioPath)) {
    if (feeding) streamSink_->abortStream();
    SD.remove(kTemporaryAudioPath);
    setError("previous response audio could not be replaced");
    return false;
  }
  if (!SD.rename(kTemporaryAudioPath, kResponseAudioPath)) {
    if (feeding) streamSink_->abortStream();
    SD.remove(kTemporaryAudioPath);
    setError("response audio rename failed");
    return false;
  }
  if (feeding) streamSink_->endStream();
  Serial.printf("[ASSISTANT] audio=%s bytes=%u\n", kResponseAudioPath,
                static_cast<unsigned>(received));
  return true;
}

bool VoiceGatewayClient::performRequest() {
  const uint32_t turnStartedMs = millis();
  File recording = SD.open(recordingPath_, FILE_READ);
  if (!recording) {
    setError("recording could not be opened");
    return false;
  }

  const String prefix = String("--") + kBoundary +
      "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"last_prompt.wav\""
      "\r\nContent-Type: audio/wav\r\n\r\n";
  const String suffix = String("\r\n--") + kBoundary + "--\r\n";
  const size_t contentLength = prefix.length() + recording.size() + suffix.length();

  WiFiClient transport;
  HttpClient request(transport, secrets::kEmberHost, secrets::kEmberPort);
  request.setHttpResponseTimeout(kRequestTimeoutMs);
  request.beginRequest();
  request.post(kVoicePath);
  request.sendHeader("X-Ember-Token", secrets::kEmberToken);
  request.sendHeader("X-Ember-Device", deviceId_[0] ? deviceId_ : "local");
  request.sendHeader("Content-Type", String("multipart/form-data; boundary=") + kBoundary);
  request.sendHeader("Content-Length", contentLength);
  request.sendHeader("Connection", "close");
  request.beginBody();
  request.print(prefix);

  uint8_t buffer[1024];
  while (recording.available()) {
    const size_t read = recording.read(buffer, sizeof(buffer));
    if (read == 0 || request.write(buffer, read) != read) {
      recording.close();
      request.stop();
      setError("recording upload failed");
      return false;
    }
    taskYIELD();
  }
  recording.close();
  request.print(suffix);
  request.endRequest();

  const int status = request.responseStatusCode();
  const String body = request.responseBody();
  const uint32_t gatewayResponseMs = millis();
  request.stop();
  if (status != 200) {
    snprintf(error_, sizeof(error_), "gateway HTTP %d", status);
    Serial.printf("[ASSISTANT] ERROR: %s body=%.120s\n", error_, body.c_str());
    return false;
  }

  StaticJsonDocument<1536> document;
  const DeserializationError jsonError = deserializeJson(document, body);
  if (jsonError) {
    setError("gateway returned invalid JSON");
    return false;
  }
  strlcpy(transcript_, document["transcript"] | "", sizeof(transcript_));
  strlcpy(reply_, document["reply"] | "", sizeof(reply_));
  strlcpy(expression_, document["expression"] | "happy", sizeof(expression_));
  strlcpy(action_, document["action"] | "", sizeof(action_));
  const char* audioUrl = document["audio_url"] | "";
  const char* provider = document["conversation_provider"] | "unknown";
  const JsonObject timings = document["timings_ms"];
  if (transcript_[0] == '\0' || reply_[0] == '\0') {
    setError("gateway response was incomplete");
    return false;
  }
  if (!downloadAudio(audioUrl)) return false;
  const uint32_t audioReadyMs = millis();
  Serial.printf("[ASSISTANT] heard: %s\n", transcript_);
  Serial.printf("[ASSISTANT] Ember: %s\n", reply_);
  Serial.printf("[ASSISTANT] expression=%s action=%s\n", expression_,
                action_[0] ? action_ : "none");
  Serial.printf(
      "[LATENCY] provider=%s upload_validation=%lums transcription=%lums "
      "conversation=%lums synthesis=%lums gateway_total=%lums\n",
      provider,
      static_cast<unsigned long>(timings["upload_validation"] | 0),
      static_cast<unsigned long>(timings["transcription"] | 0),
      static_cast<unsigned long>(timings["conversation"] | 0),
      static_cast<unsigned long>(timings["synthesis"] | 0),
      static_cast<unsigned long>(timings["gateway_total"] | 0));
  Serial.printf(
      "[LATENCY] fire_request=%lums audio_download=%lums ready_total=%lums\n",
      static_cast<unsigned long>(gatewayResponseMs - turnStartedMs),
      static_cast<unsigned long>(audioReadyMs - gatewayResponseMs),
      static_cast<unsigned long>(audioReadyMs - turnStartedMs));
  return true;
}

}  // namespace firechan
