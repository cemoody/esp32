#include "elevenlabs.h"
#include "config.h"

#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include "mbedtls/base64.h"

using namespace websockets;

static WebsocketsClient* elWsClient = nullptr;

ElevenLabsClient::ElevenLabsClient()
  : _audioCb(nullptr), _doneCb(nullptr), _connected(false) {}

void ElevenLabsClient::setAudioCallback(ElevenLabsAudioCallback cb) {
  _audioCb = cb;
}

void ElevenLabsClient::setDoneCallback(ElevenLabsDoneCallback cb) {
  _doneCb = cb;
}

bool ElevenLabsClient::connect() {
  if (_connected) return true;

  if (!elWsClient) {
    elWsClient = new WebsocketsClient();
    elWsClient->setInsecure();
  }

  // Build WSS URL for streaming TTS
  String url = String("wss://") + ELEVENLABS_HOST +
    "/v1/text-to-speech/" + ELEVENLABS_VOICE_ID +
    "/stream-input?model_id=" + ELEVENLABS_MODEL +
    "&output_format=pcm_16000";

  elWsClient->onMessage([this](WebsocketsMessage msg) {
    if (msg.isText()) {
      handleMessage(msg.c_str(), msg.length());
    }
  });

  elWsClient->onEvent([this](WebsocketsEvent event, String data) {
    switch (event) {
      case WebsocketsEvent::ConnectionOpened:
        Serial.println("[EL] WebSocket connected");
        _connected = true;
        break;
      case WebsocketsEvent::ConnectionClosed:
        Serial.println("[EL] WebSocket disconnected");
        _connected = false;
        break;
      case WebsocketsEvent::GotPing:
        elWsClient->pong();
        break;
      default:
        break;
    }
  });

  Serial.println("[EL] Connecting to ElevenLabs...");
  bool ok = elWsClient->connect(url);
  if (ok) {
    _connected = true;
    Serial.println("[EL] Connected!");

    // Send initial BOS (beginning of stream) message
    JsonDocument doc;
    doc["text"] = " ";
    doc["voice_settings"]["stability"] = 0.5;
    doc["voice_settings"]["similarity_boost"] = 0.75;
    doc["xi_api_key"] = ELEVENLABS_API_KEY;

    String msg;
    serializeJson(doc, msg);
    elWsClient->send(msg);
  } else {
    Serial.println("[EL] Connection failed");
  }
  return ok;
}

void ElevenLabsClient::disconnect() {
  if (_connected && elWsClient) {
    elWsClient->close();
    _connected = false;
  }
}

bool ElevenLabsClient::sendText(const char* text) {
  if (!_connected || !elWsClient || !text) return false;

  JsonDocument doc;
  doc["text"] = text;

  String msg;
  serializeJson(doc, msg);
  return elWsClient->send(msg);
}

bool ElevenLabsClient::flush() {
  if (!_connected || !elWsClient) return false;

  // Send empty text to signal end of input
  JsonDocument doc;
  doc["text"] = "";

  String msg;
  serializeJson(doc, msg);
  return elWsClient->send(msg);
}

void ElevenLabsClient::poll() {
  if (_connected && elWsClient) {
    elWsClient->poll();
  }
}

bool ElevenLabsClient::isConnected() const {
  return _connected;
}

void ElevenLabsClient::handleMessage(const char* data, size_t len) {
  // Check for isFinal
  if (strstr(data, "\"isFinal\":true")) {
    Serial.println("[EL] TTS complete");
    if (_doneCb) {
      _doneCb();
    }
  }

  // Extract audio using string search (messages are too large for JSON parsing)
  const char* audioStart = strstr(data, "\"audio\":\"");
  if (!audioStart) return;
  audioStart += 9; // skip past "audio":"

  const char* audioEnd = strchr(audioStart, '"');
  if (!audioEnd || audioEnd <= audioStart) return;

  size_t b64Len = audioEnd - audioStart;

  // Check for null audio
  if (b64Len == 4 && strncmp(audioStart, "null", 4) == 0) return;

  Serial.printf("[EL] audio chunk b64=%d\n", b64Len);

  // Decode base64 audio
  size_t outLen = 0;
  mbedtls_base64_decode(nullptr, 0, &outLen, (const uint8_t*)audioStart, b64Len);

  if (outLen > 0) {
    uint8_t* pcmData = (uint8_t*)heap_caps_malloc(outLen, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pcmData) pcmData = (uint8_t*)malloc(outLen);
    if (pcmData) {
      size_t actualLen = 0;
      int ret = mbedtls_base64_decode(pcmData, outLen, &actualLen,
                                       (const uint8_t*)audioStart, b64Len);
      if (ret == 0 && actualLen > 0 && _audioCb) {
        _audioCb(pcmData, actualLen);
      }
      free(pcmData);
    } else {
      Serial.println("[EL] malloc failed for audio decode");
    }
  }
}
