#pragma once
#include "Arduino.h"

class TwoWire {
public:
  void begin(int, int, uint32_t = 100000) {}
  void beginTransmission(uint8_t) {}
  uint8_t endTransmission(bool = true) { return 0; }
  size_t write(uint8_t) { return 1; }
  uint8_t requestFrom(uint8_t, uint8_t) { return 0; }
  int read() { return 0; }
};

static TwoWire Wire;
