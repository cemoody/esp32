#pragma once

#include <string>
#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>

// Arduino String wrapper around std::string
class String {
  std::string _s;
public:
  String() {}
  String(const char* s) : _s(s ? s : "") {}
  String(const std::string& s) : _s(s) {}
  String(int val) : _s(std::to_string(val)) {}
  String(unsigned int val) : _s(std::to_string(val)) {}
  String(long val) : _s(std::to_string(val)) {}
  String(unsigned long val) : _s(std::to_string(val)) {}

  const char* c_str() const { return _s.c_str(); }
  size_t length() const { return _s.length(); }

  String& operator+=(const String& rhs) { _s += rhs._s; return *this; }
  String& operator+=(const char* rhs) { if (rhs) _s += rhs; return *this; }
  String& operator+=(char c) { _s += c; return *this; }

  String operator+(const String& rhs) const { return String((_s + rhs._s).c_str()); }
  String operator+(const char* rhs) const { return String((_s + (rhs ? rhs : "")).c_str()); }
  friend String operator+(const char* lhs, const String& rhs) { return String((std::string(lhs) + rhs._s).c_str()); }

  bool operator==(const char* rhs) const { return _s == (rhs ? rhs : ""); }
  bool operator==(const String& rhs) const { return _s == rhs._s; }
  bool operator!=(const char* rhs) const { return !(*this == rhs); }

  bool startsWith(const char* prefix) const {
    return _s.compare(0, strlen(prefix), prefix) == 0;
  }
  bool startsWith(const String& prefix) const { return startsWith(prefix.c_str()); }

  String substring(unsigned int from) const { return String(_s.substr(from).c_str()); }
  String substring(unsigned int from, unsigned int to) const {
    return String(_s.substr(from, to - from).c_str());
  }

  int indexOf(const char* str) const {
    auto pos = _s.find(str);
    return pos == std::string::npos ? -1 : (int)pos;
  }
  int indexOf(const char* str, int from) const {
    auto pos = _s.find(str, from);
    return pos == std::string::npos ? -1 : (int)pos;
  }
  int indexOf(char c) const {
    auto pos = _s.find(c);
    return pos == std::string::npos ? -1 : (int)pos;
  }

  void trim() {
    size_t start = _s.find_first_not_of(" \t\r\n");
    size_t end = _s.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) { _s.clear(); return; }
    _s = _s.substr(start, end - start + 1);
  }

  bool contains(const char* s) const { return _s.find(s) != std::string::npos; }

  // ArduinoJson serialization support
  size_t write(uint8_t c) { _s += (char)c; return 1; }
  size_t write(const uint8_t* buf, size_t n) { _s.append((const char*)buf, n); return n; }
};

// Serial mock
struct SerialMock {
  void begin(unsigned long) {}
  void println(const char* s = "") { printf("%s\n", s); }
  void print(const char* s) { printf("%s", s); }
  void printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
  }
};
static SerialMock Serial;

// Arduino timing
inline void delay(unsigned long) {}
inline unsigned long millis() { return 0; }

// Arduino constants
#ifndef HIGH
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
#endif

#ifndef PI
#define PI 3.14159265358979323846
#endif
