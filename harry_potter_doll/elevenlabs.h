#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>

// Callback when audio chunk is received (raw PCM 16-bit 16kHz mono)
using ElevenLabsAudioCallback = std::function<void(const uint8_t* data, size_t len)>;

// Callback when TTS generation is complete
using ElevenLabsDoneCallback = std::function<void()>;

class ElevenLabsClient {
public:
  ElevenLabsClient();

  void setAudioCallback(ElevenLabsAudioCallback cb);
  void setDoneCallback(ElevenLabsDoneCallback cb);

  // Connect to ElevenLabs streaming TTS WebSocket
  bool connect();

  // Disconnect
  void disconnect();

  // Send text to be spoken. Can be called multiple times to stream tokens.
  bool sendText(const char* text);

  // Signal that all text has been sent (flush remaining audio)
  bool flush();

  // Process incoming WebSocket messages
  void poll();

  bool isConnected() const;

private:
  ElevenLabsAudioCallback _audioCb;
  ElevenLabsDoneCallback _doneCb;
  bool _connected;

  void handleMessage(const char* data, size_t len);
};
