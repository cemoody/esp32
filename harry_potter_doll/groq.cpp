#include "groq.h"
#include "config.h"

#include <string>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

GroqClient::GroqClient()
  : _tokenCb(nullptr), _doneCb(nullptr), _pollCb(nullptr), _busy(false), _cancelled(false) {}

void GroqClient::setPollCallback(std::function<void()> cb) {
  _pollCb = cb;
}

void GroqClient::setTokenCallback(GroqTokenCallback cb) {
  _tokenCb = cb;
}

void GroqClient::setDoneCallback(GroqDoneCallback cb) {
  _doneCb = cb;
}

bool GroqClient::isBusy() const {
  return _busy;
}

void GroqClient::cancel() {
  _cancelled = true;
}

String GroqClient::buildRequestBody(const char* userMessage,
                                     const ConversationTurn* history, size_t historyLen) {
  JsonDocument doc;

  doc["model"] = GROQ_MODEL;
  doc["max_tokens"] = GROQ_MAX_TOKENS;
  doc["stream"] = true;
  doc["temperature"] = 0.7;

  JsonArray messages = doc["messages"].to<JsonArray>();

  // System prompt
  JsonObject sys = messages.add<JsonObject>();
  sys["role"] = "system";
  sys["content"] = SYSTEM_PROMPT;

  // Conversation history
  for (size_t i = 0; i < historyLen; i++) {
    JsonObject msg = messages.add<JsonObject>();
    msg["role"] = history[i].role;
    msg["content"] = history[i].content;
  }

  // Current user message
  JsonObject user = messages.add<JsonObject>();
  user["role"] = "user";
  user["content"] = userMessage;

  String body;
  serializeJson(doc, body);
  return body;
}

bool GroqClient::requestCompletion(const char* userMessage,
                                    const ConversationTurn* history, size_t historyLen) {
  if (_busy) return false;

  _busy = true;
  _cancelled = false;

  String body = buildRequestBody(userMessage, history, historyLen);

  WiFiClientSecure client;
  client.setInsecure();  // Skip cert validation (saves RAM)
  client.setTimeout(10);

  Serial.println("[GROQ] Connecting...");
  if (!client.connect(GROQ_HOST, 443)) {
    Serial.println("[GROQ] Connection failed");
    _busy = false;
    return false;
  }
  Serial.println("[GROQ] Connected to Groq");

  // Send HTTP request
  String request = String("POST /openai/v1/chat/completions HTTP/1.1\r\n") +
    "Host: " + GROQ_HOST + "\r\n" +
    "Authorization: Bearer " + GROQ_API_KEY + "\r\n" +
    "Content-Type: application/json\r\n" +
    "Content-Length: " + String(body.length()) + "\r\n" +
    "Connection: close\r\n\r\n" +
    body;

  client.print(request);
  Serial.println("[GROQ] Request sent, waiting for response...");

  // Skip HTTP headers
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
    if (_cancelled) {
      client.stop();
      _busy = false;
      return false;
    }
  }

  // Process SSE stream
  processSSEStream(client);

  client.stop();
  _busy = false;

  if (!_cancelled && _doneCb) {
    _doneCb();
  }

  return true;
}

void GroqClient::processSSEStream(WiFiClientSecure& client) {
  String buffer;
  int lineCount = 0;

  while (client.connected() || client.available()) {
    if (_cancelled) return;

    if (!client.available()) {
      if (_pollCb) _pollCb();
      delay(1);
      continue;
    }

    char c = client.read();
    buffer += c;

    if (c == '\n') {
      buffer.trim();
      lineCount++;

      // Debug: show first 10 lines received
      if (lineCount <= 10) {
        Serial.printf("[GROQ] line %d: '%s'\n", lineCount, buffer.c_str());
      }

      if (buffer.startsWith("data: ")) {
        String data = buffer.substring(6);

        if (data == "[DONE]") {
          Serial.println("\n[GROQ] Stream complete");
          buffer = "";
          return;
        }

        // Extract content using string search (faster than full JSON parse)
        // Handles escaped quotes: find "content":" then scan for unescaped "
        int idx = data.indexOf("\"content\":\"");
        if (idx >= 0) {
          idx += 11; // skip past "content":"
          // Find closing quote, skipping escaped ones
          int end = idx;
          const char* s = data.c_str();
          while (s[end] && !(s[end] == '"' && (end == idx || s[end - 1] != '\\'))) {
            end++;
          }
          if (end > idx && s[end] == '"') {
            String content = data.substring(idx, end);
            // Unescape common JSON sequences
            // Note: Arduino String doesn't have replace(), so we rebuild
            std::string unescaped;
            const char* c = content.c_str();
            for (size_t i = 0; i < content.length(); i++) {
              if (c[i] == '\\' && i + 1 < content.length()) {
                switch (c[i + 1]) {
                  case '"': unescaped += '"'; i++; break;
                  case '\\': unescaped += '\\'; i++; break;
                  case 'n': unescaped += '\n'; i++; break;
                  case 't': unescaped += '\t'; i++; break;
                  default: unescaped += c[i]; break;
                }
              } else {
                unescaped += c[i];
              }
            }
            if (!unescaped.empty()) {
              if (_tokenCb) {
                _tokenCb(unescaped.c_str());
              }
            }
          }
        }
      }
      buffer = "";
    }
  }
  Serial.printf("[GROQ] SSE loop ended, lines=%d\n", lineCount);
}
