#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>

// Callback when a final transcript is received (end of utterance)
using DeepgramTranscriptCallback = std::function<void(const char* transcript)>;

// Callback for partial/interim transcripts
using DeepgramPartialCallback = std::function<void(const char* transcript)>;

class DeepgramClient {
public:
  DeepgramClient();

  void setTranscriptCallback(DeepgramTranscriptCallback cb);
  void setPartialCallback(DeepgramPartialCallback cb);

  // Connect to Deepgram WSS
  bool connect();

  // Disconnect
  void disconnect();

  // Send raw audio data (16-bit mono PCM at 16kHz)
  bool sendAudio(const uint8_t* data, size_t len);

  // Process incoming WebSocket messages (call from loop/task)
  void poll();

  bool isConnected() const;

private:
  DeepgramTranscriptCallback _transcriptCb;
  DeepgramPartialCallback _partialCb;
  bool _connected;

  void handleMessage(const char* data, size_t len);
};
