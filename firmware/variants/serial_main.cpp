/*
 * OpenSmell E-Nose Serial Firmware
 *
 * Reads 6 MQ sensors on ADC1 pins and prints CSV over Serial at 115200 baud.
 * Compatible with Osmograph USB detection.
 */

#include <Arduino.h>

static const int mq_pins[] = {36, 39, 34, 35, 32, 33};
static const int PIN_COUNT = 6;

void setup() {
  Serial.begin(115200);
  delay(500);
  for (int i = 0; i < PIN_COUNT; i++) {
    pinMode(mq_pins[i], INPUT);
    analogSetPinAttenuation(mq_pins[i], ADC_11db);
  }
}

void loop() {
  static unsigned long last = 0;
  unsigned long now = millis();

  if (!Serial) { delay(100); return; }
  if (now - last < 500) return;
  last = now;

  for (int i = 0; i < PIN_COUNT; i++) {
    Serial.print(analogRead(mq_pins[i]));
    if (i < PIN_COUNT - 1) Serial.print(",");
  }
  Serial.println();
}
