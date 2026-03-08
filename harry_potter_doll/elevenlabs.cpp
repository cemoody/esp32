// TTS client — uses Deepgram Aura via WebSocket streaming
// (Originally ElevenLabs, swapped to Deepgram for cost/reliability)

#include "elevenlabs.h"
#include "config.h"

#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

using namespace websockets;

static WebsocketsClient* ttsWsClient = nullptr;

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

  if (!ttsWsClient) {
    ttsWsClient = new WebsocketsClient();
    ttsWsClient->setInsecure();
  }

  // Deepgram TTS WebSocket endpoint
  String url = String("wss://api.deepgram.com/v1/speak?model=") +
    TTS_MODEL + "&encoding=linear16&sample_rate=16000";

  // Handle binary messages (raw PCM audio from Deepgram)
  ttsWsClient->onMessage([this](WebsocketsMessage msg) {
    if (msg.isBinary()) {
      // Binary frame = raw PCM audio, pass directly to callback
      const uint8_t* data = (const uint8_t*)msg.c_str();
      size_t len = msg.length();
      if (len > 0 && _audioCb) {
        _audioCb(data, len);
      }
    } else if (msg.isText()) {
      // Text frame = metadata/control message
      handleMessage(msg.c_str(), msg.length());
    }
  });

  ttsWsClient->onEvent([this](WebsocketsEvent event, String data) {
    switch (event) {
      case WebsocketsEvent::ConnectionOpened:
        Serial.println("[TTS] WebSocket connected");
        _connected = true;
        break;
      case WebsocketsEvent::ConnectionClosed:
        Serial.println("[TTS] WebSocket disconnected");
        _connected = false;
        break;
      case WebsocketsEvent::GotPing:
        ttsWsClient->pong();
        break;
      default:
        break;
    }
  });

  // Set auth header
  ttsWsClient->addHeader("Authorization", "Token " DEEPGRAM_API_KEY);

  Serial.println("[TTS] Connecting to Deepgram TTS...");
  bool ok = ttsWsClient->connect(url);
  if (ok) {
    _connected = true;
    Serial.println("[TTS] Connected!");
  } else {
    Serial.println("[TTS] Connection failed");
  }
  return ok;
}

void ElevenLabsClient::disconnect() {
  if (_connected && ttsWsClient) {
    // Send close message
    ttsWsClient->send("{\"type\":\"Close\"}");
    ttsWsClient->close();
    _connected = false;
  }
}

bool ElevenLabsClient::sendText(const char* text) {
  if (!_connected || !ttsWsClient || !text) return false;

  // Deepgram TTS text message format
  JsonDocument doc;
  doc["type"] = "Speak";
  doc["text"] = text;

  String msg;
  serializeJson(doc, msg);
  return ttsWsClient->send(msg);
}

bool ElevenLabsClient::flush() {
  if (!_connected || !ttsWsClient) return false;

  // Deepgram flush command
  return ttsWsClient->send("{\"type\":\"Flush\"}");
}

void ElevenLabsClient::poll() {
  if (_connected && ttsWsClient) {
    ttsWsClient->poll();
  }
}

bool ElevenLabsClient::isConnected() const {
  return _connected;
}

void ElevenLabsClient::handleMessage(const char* data, size_t len) {
  // Deepgram sends JSON metadata messages
  // Flushed = all audio for queued text has been sent
  if (strstr(data, "\"type\":\"Flushed\"")) {
    Serial.println("[TTS] Flushed — audio complete");
    if (_doneCb) {
      _doneCb();
    }
  }

  // Log any warnings/errors
  if (strstr(data, "\"type\":\"Warning\"") || strstr(data, "\"type\":\"Error\"")) {
    Serial.printf("[TTS] %.*s\n", (int)(len > 200 ? 200 : len), data);
  }
}
