#include <Adafruit_NeoPixel.h>

#define RGB_PIN    38
#define NUM_LEDS   7
#define BRIGHTNESS 50

Adafruit_NeoPixel strip(NUM_LEDS, RGB_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32-S3 Audio Board - RGB Demo");
  Serial.println("================================");

  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.show();

  Serial.println("RGB LEDs initialized on GPIO 38");
  Serial.println("Running rainbow cycle...");
}

void loop() {
  // Cycle through colors on all 7 LEDs with a rainbow chase effect
  for (long firstPixelHue = 0; firstPixelHue < 5 * 65536; firstPixelHue += 256) {
    for (int i = 0; i < strip.numPixels(); i++) {
      int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
    }
    strip.show();
    delay(10);
  }
}
