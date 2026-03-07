#include "rgb_feedback.h"
#include "config.h"
#include <Adafruit_NeoPixel.h>

static Adafruit_NeoPixel strip(RGB_LED_COUNT, RGB_LED_PIN, NEO_RGB + NEO_KHZ800);
static DollState currentState = STATE_CONNECTING;
static uint32_t animTick = 0;

void rgb_init() {
  strip.begin();
  strip.setBrightness(RGB_BRIGHTNESS);
  strip.show();
}

// Helper: set all LEDs to one color
static void setAll(uint32_t color) {
  for (int i = 0; i < RGB_LED_COUNT; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

// Helper: breathing effect — returns brightness 0-255
static uint8_t breathe(uint32_t tick, uint16_t period) {
  float phase = (float)(tick % period) / period;
  float val = (sin(phase * 2.0 * PI) + 1.0) * 0.5;
  return (uint8_t)(val * 255);
}

// Helper: pulse effect with gamma
static uint32_t pulseColor(uint8_t r, uint8_t g, uint8_t b, uint32_t tick, uint16_t period) {
  uint8_t br = breathe(tick, period);
  return strip.Color((r * br) >> 8, (g * br) >> 8, (b * br) >> 8);
}

void rgb_set_state(DollState state) {
  if (state != currentState) {
    currentState = state;
    animTick = 0;
  }
}

void rgb_update() {
  animTick++;

  switch (currentState) {
    case STATE_IDLE: {
      // Slow golden pulse (Gryffindor)
      uint32_t color = pulseColor(255, 180, 0, animTick, 120);
      setAll(color);
      break;
    }

    case STATE_LISTENING: {
      // Blue breathing
      uint32_t color = pulseColor(0, 100, 255, animTick, 60);
      setAll(color);
      break;
    }

    case STATE_THINKING: {
      // Purple spinning chase
      for (int i = 0; i < RGB_LED_COUNT; i++) {
        int pos = (animTick / 4 + i) % RGB_LED_COUNT;
        uint8_t br = (pos == 0) ? 255 : (pos == 1) ? 100 : 20;
        strip.setPixelColor(i, strip.Color((180 * br) >> 8, 0, (255 * br) >> 8));
      }
      strip.show();
      break;
    }

    case STATE_SPEAKING: {
      // Green pulse
      uint32_t color = pulseColor(0, 255, 80, animTick, 40);
      setAll(color);
      break;
    }

    case STATE_ERROR: {
      // Red flash
      bool on = (animTick % 30) < 15;
      setAll(on ? strip.Color(255, 0, 0) : 0);
      break;
    }

    case STATE_CONNECTING: {
      // White fade in/out
      uint32_t color = pulseColor(255, 255, 255, animTick, 80);
      setAll(color);
      break;
    }
  }
}
