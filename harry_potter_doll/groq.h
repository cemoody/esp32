#pragma once

#include <functional>
#include <Arduino.h>
#include <WiFiClientSecure.h>

// Callback for each token as it streams in
using GroqTokenCallback = std::function<void(const char* token)>;

// Callback when generation is complete
using GroqDoneCallback = std::function<void()>;

struct ConversationTurn {
  String role;     // "user" or "assistant"
  String content;
};

class GroqClient {
public:
  GroqClient();

  void setTokenCallback(GroqTokenCallback cb);
  void setDoneCallback(GroqDoneCallback cb);
  void setPollCallback(std::function<void()> cb);

  // Send a chat completion request with streaming
  // userMessage: the latest user message
  // history: array of previous turns for context
  // historyLen: number of turns in history
  // Returns true if request was initiated successfully
  bool requestCompletion(const char* userMessage,
                         const ConversationTurn* history, size_t historyLen);

  // Cancel an in-progress request
  void cancel();

  bool isBusy() const;

private:
  GroqTokenCallback _tokenCb;
  GroqDoneCallback _doneCb;
  std::function<void()> _pollCb;
  volatile bool _busy;
  volatile bool _cancelled;

  String buildRequestBody(const char* userMessage,
                          const ConversationTurn* history, size_t historyLen);
  void processSSEStream(WiFiClientSecure& client);
};
