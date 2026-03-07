#include <Adafruit_NeoPixel.h>

#define RGB_LED_PIN   38
#define RGB_LED_COUNT 7

Adafruit_NeoPixel strip(RGB_LED_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Board test starting...");

  strip.begin();
  strip.setBrightness(40);

  for (int i = 0; i < RGB_LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(0, 255, 0));
  }
  strip.show();
  Serial.println("LEDs should be green now.");
}

void loop() {
  Serial.printf("Alive - millis=%lu, heap=%u\n", millis(), esp_get_free_heap_size());
  delay(2000);
}
