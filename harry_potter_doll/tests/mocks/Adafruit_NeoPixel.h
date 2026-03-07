#pragma once
#include <cstdint>

#define NEO_GRB 0
#define NEO_RGB 1
#define NEO_KHZ800 0

class Adafruit_NeoPixel {
public:
  Adafruit_NeoPixel(int, int, int) {}
  void begin() {}
  void show() {}
  void setBrightness(uint8_t) {}
  void setPixelColor(int, uint32_t) {}
  static uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
  }
};
