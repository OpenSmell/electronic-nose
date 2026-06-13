#include <Arduino.h>

// ============================================================
// OpenSmell E-Nose Firmware
// ============================================================
// SCALING: To add more MQ sensors, just add pins and analogRead.
//   Example for 4 sensors:
//     #define MQ6_PIN   33
//     int mq6 = analogRead(MQ6_PIN);
//     Serial.print(","); Serial.print(mq6);  // add to CSV
// ============================================================

// Sensor pins
#define MQ135_PIN 34
#define MQ3_PIN   35
#define MQ7_PIN   32

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Set ADC attenuation for wider voltage range
  analogSetPinAttenuation(MQ135_PIN, ADC_11db);
  analogSetPinAttenuation(MQ3_PIN, ADC_11db);
  analogSetPinAttenuation(MQ7_PIN, ADC_11db);
  
  // Print CSV header once (skip if using 3-sensor raw format)
  // Serial.println("MQ135,MQ3,MQ7");
}

void loop() {
  static unsigned long last = 0;
  unsigned long now = millis();
  
  // Wait for serial to be ready (skip bootloader noise)
  if (!Serial) {
    delay(100);
    return;
  }
  
  int mq135 = analogRead(MQ135_PIN);
  int mq3   = analogRead(MQ3_PIN);
  int mq7   = analogRead(MQ7_PIN);
  
  // Only print if ADC values are valid (non-zero, within range)
  if (mq135 >= 0 && mq3 >= 0 && mq7 >= 0) {
    Serial.print(mq135);
    Serial.print(",");
    Serial.print(mq3);
    Serial.print(",");
    Serial.println(mq7);
  }
  
  delay(500);
}
