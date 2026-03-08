#include <Wire.h>
#include "TCA9555.h"

#define TCA9555_ADDR 0x20
#define I2C_SDA 11
#define I2C_SCL 10

TCA9555 tca(TCA9555_ADDR);

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== TCA9555 Button Scanner ===");
  Serial.println("Press buttons to see which pins change.\n");

  Wire.begin(I2C_SDA, I2C_SCL, 100000);

  // Set all pins as INPUT except pin 8 (PA amp, keep as output HIGH)
  for (int i = 0; i < 16; i++) {
    if (i == 8) {
      tca.pinMode1(i, OUTPUT);
      tca.write1(i, HIGH);
    } else {
      tca.pinMode1(i, INPUT);
    }
  }

  Serial.println("Pin states (1=HIGH, 0=LOW):");
}

uint16_t lastState = 0xFFFF;

void loop() {
  uint16_t state = tca.read16();

  if (state != lastState) {
    Serial.printf("\nChange detected! Raw=0x%04X\n", state);
    for (int i = 0; i < 16; i++) {
      int val = (state >> i) & 1;
      int prev = (lastState >> i) & 1;
      if (val != prev) {
        Serial.printf("  >>> Pin %d (P%d%d): %d -> %d %s\n",
          i, i / 8, i % 8, prev, val,
          val == 0 ? "PRESSED" : "RELEASED");
      }
    }
    lastState = state;
  }

  // Also print periodic status every 5 seconds
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint > 5000) {
    lastPrint = millis();
    Serial.printf("[%lus] All pins: ", millis() / 1000);
    for (int i = 0; i < 16; i++) {
      Serial.printf("P%d%d=%d ", i / 8, i % 8, (state >> i) & 1);
    }
    Serial.println();
  }

  delay(50);
}
