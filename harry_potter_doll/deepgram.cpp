#include "deepgram.h"
#include "config.h"

#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

using namespace websockets;

static WebsocketsClient* wsClient = nullptr;

DeepgramClient::DeepgramClient()
  : _transcriptCb(nullptr), _partialCb(nullptr), _connected(false) {}

void DeepgramClient::setTranscriptCallback(DeepgramTranscriptCallback cb) {
  _transcriptCb = cb;
}

void DeepgramClient::setPartialCallback(DeepgramPartialCallback cb) {
  _partialCb = cb;
}

bool DeepgramClient::connect() {
  if (_connected) return true;

  if (!wsClient) {
    wsClient = new WebsocketsClient();
    wsClient->setInsecure();
  }

  // Build WSS URL
  String url = "wss://" DEEPGRAM_HOST DEEPGRAM_PATH;

  // Set authorization header
  wsClient->addHeader("Authorization", "Token " DEEPGRAM_API_KEY);

  // Set up message handler
  wsClient->onMessage([this](WebsocketsMessage msg) {
    if (msg.isText()) {
      handleMessage(msg.c_str(), msg.length());
    }
  });

  wsClient->onEvent([this](WebsocketsEvent event, String data) {
    switch (event) {
      case WebsocketsEvent::ConnectionOpened:
        Serial.println("[DG] WebSocket connected");
        _connected = true;
        break;
      case WebsocketsEvent::ConnectionClosed:
        Serial.println("[DG] WebSocket disconnected");
        _connected = false;
        break;
      case WebsocketsEvent::GotPing:
        wsClient->pong();
        break;
      default:
        break;
    }
  });

  Serial.println("[DG] Connecting to Deepgram...");
  bool ok = wsClient->connect(url);
  if (ok) {
    _connected = true;
    Serial.println("[DG] Connected!");
  } else {
    Serial.println("[DG] Connection failed");
  }
  return ok;
}

void DeepgramClient::disconnect() {
  if (_connected && wsClient) {
    // Send close message per Deepgram protocol
    wsClient->send("{\"type\": \"CloseStream\"}");
    delay(100);
    wsClient->close();
    _connected = false;
  }
}

bool DeepgramClient::sendAudio(const uint8_t* data, size_t len) {
  if (!_connected || !wsClient) return false;
  return wsClient->sendBinary((const char*)data, len);
}

void DeepgramClient::poll() {
  if (_connected && wsClient) {
    wsClient->poll();
  }
}

bool DeepgramClient::isConnected() const {
  return _connected;
}

void DeepgramClient::handleMessage(const char* data, size_t len) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) {
    Serial.printf("[DG] JSON parse error: %s\n", err.c_str());
    return;
  }

  // Check for speech_final or is_final in the response
  const char* type = doc["type"] | "";

  if (strcmp(type, "Results") == 0) {
    bool isFinal = doc["is_final"] | false;
    bool speechFinal = doc["speech_final"] | false;

    // Extract transcript from channel.alternatives[0].transcript
    const char* transcript = doc["channel"]["alternatives"][0]["transcript"] | "";

    if (strlen(transcript) == 0) return;

    if (speechFinal || isFinal) {
      Serial.printf("[DG] Final: %s\n", transcript);
      if (_transcriptCb) {
        _transcriptCb(transcript);
      }
    } else {
      if (_partialCb) {
        _partialCb(transcript);
      }
    }
  } else if (strcmp(type, "UtteranceEnd") == 0) {
    // Deepgram detected end of utterance
    Serial.println("[DG] Utterance end detected");
  }
}
