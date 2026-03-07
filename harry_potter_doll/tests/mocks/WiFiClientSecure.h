#pragma once
#include "Arduino.h"
#include <string>
#include <sstream>

class WiFiClientSecure {
  std::string _data;
  size_t _pos = 0;
  bool _connected = false;
public:
  void setInsecure() {}
  void setTimeout(int) {}

  bool connect(const char*, int) { _connected = true; return true; }
  void stop() { _connected = false; }
  bool connected() { return _connected && _pos < _data.size(); }
  int available() { return _connected ? (int)(_data.size() - _pos) : 0; }

  void print(const String&) {}

  char read() {
    if (_pos < _data.size()) return _data[_pos++];
    return -1;
  }

  String readStringUntil(char delim) {
    std::string result;
    while (_pos < _data.size()) {
      char c = _data[_pos++];
      if (c == delim) break;
      result += c;
    }
    return String(result.c_str());
  }

  // Test helper: preload response data
  void _setResponseData(const std::string& data) {
    _data = data;
    _pos = 0;
    _connected = true;
  }
};
