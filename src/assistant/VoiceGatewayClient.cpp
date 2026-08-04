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
  state_ = State::Pending;
  xTaskNotifyGive(task_);
  return true;
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

bool VoiceGatewayClient::performRequest() {
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
  request.sendHeader("Content-Type", String("multipart/form-data; boundary=") + kBoundary);
  request.sendHeader("Content-Length", contentLength);
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
  if (transcript_[0] == '\0' || reply_[0] == '\0') {
    setError("gateway response was incomplete");
    return false;
  }
  Serial.printf("[ASSISTANT] heard: %s\n", transcript_);
  Serial.printf("[ASSISTANT] Ember: %s\n", reply_);
  Serial.printf("[ASSISTANT] expression=%s action=%s\n", expression_,
                action_[0] ? action_ : "none");
  return true;
}

}  // namespace firechan
