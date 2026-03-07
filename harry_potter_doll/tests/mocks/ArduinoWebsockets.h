#pragma once
#include "Arduino.h"
#include <functional>

namespace websockets {

class WebsocketsMessage {
  std::string _data;
  bool _isText;
public:
  WebsocketsMessage() : _isText(true) {}
  WebsocketsMessage(const std::string& d, bool t) : _data(d), _isText(t) {}
  bool isText() const { return _isText; }
  const char* c_str() const { return _data.c_str(); }
  size_t length() const { return _data.size(); }
};

enum class WebsocketsEvent {
  ConnectionOpened,
  ConnectionClosed,
  GotPing
};

class WebsocketsClient {
public:
  void setInsecure() {}
  void addHeader(const String&, const String&) {}
  bool connect(const String&) { return true; }
  void close() {}
  bool send(const String&) { return true; }
  bool sendBinary(const char*, size_t) { return true; }
  void pong() {}
  void poll() {}
  void onMessage(std::function<void(WebsocketsMessage)>) {}
  void onEvent(std::function<void(WebsocketsEvent, String)>) {}
};

} // namespace websockets
