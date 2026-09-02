/*
 * OpenSmell E-Nose Universal Firmware
 * Supports USB Serial AND WiFi AP + TCP on port 8080.
 * Falls back to Serial-only if WiFi fails.
 * ADC1 pins only (WiFi-safe): 32, 33, 34, 35, 36, 39
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>

static const int mq_pins[] = {32, 33, 34, 35, 36, 39};
#ifndef OSMOGRAPH_PIN_COUNT
#define OSMOGRAPH_PIN_COUNT 6
#endif
static const int PIN_COUNT = OSMOGRAPH_PIN_COUNT;
static_assert(PIN_COUNT >= 1 && PIN_COUNT <= 6, "OSMOGRAPH_PIN_COUNT must be 1..6");

static WiFiServer tcp_server(8080);
static WiFiClient tcp_client;
static bool wifi_ok = false;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("OSM:boot");

  for (int i = 0; i < PIN_COUNT; i++) {
    pinMode(mq_pins[i], INPUT);
    analogSetPinAttenuation(mq_pins[i], ADC_11db);
  }

  // WiFi AP - non-blocking, errors don't halt setup
  uint32_t id = (uint32_t)ESP.getEfuseMac() & 0xFFFFFF;
  char ssid[32];
  snprintf(ssid, sizeof(ssid), "Osmograph-%06X", id);

  WiFi.mode(WIFI_AP);
  wifi_ok = WiFi.softAP(ssid, "osmograph");
  if (wifi_ok) {
    Serial.printf("OSM:wifi %s %s\n", ssid, WiFi.softAPIP().toString().c_str());

    if (MDNS.begin("osmograph")) {
      MDNS.addService("_osmograph", "_tcp", 8080);
    }

    tcp_server.begin();
    tcp_server.setNoDelay(true);
    Serial.println("OSM:tcp 8080");
  } else {
    Serial.println("OSM:wifi fail");
  }

  Serial.println("OSM:ready");
  Serial.printf("INFO,universal-esp32,1.0.0,%d\n", PIN_COUNT);
}

void loop() {
  static unsigned long last = 0;
  unsigned long now = millis();

  // TCP accept
  if (wifi_ok) {
    if (!tcp_client || !tcp_client.connected()) {
      if (tcp_client) tcp_client.stop();
      tcp_client = tcp_server.available();
    }
  }

  if (now - last < 100) return;  // 10 Hz
  last = now;

  char buf[96];
  int pos = 0;
  for (int i = 0; i < PIN_COUNT; i++) {
    pos += snprintf(buf + pos, sizeof(buf) - pos, "%s%d", i > 0 ? "," : "", analogRead(mq_pins[i]));
  }
  buf[pos++] = '\n';
  buf[pos] = '\0';

  Serial.print(buf);

  if (wifi_ok && tcp_client && tcp_client.connected()) {
    tcp_client.write((uint8_t*)buf, pos);
  }
}
