/*
 * OpenSmell E-Nose WiFi Firmware
 *
 * Creates WiFi AP (SSID: Osmograph-XXXXXX), TCP server on port 8080,
 * reads 6 MQ sensors (ADC1 pins only — safe with WiFi), streams CSV.
 * Advertises via mDNS as _osmograph._tcp for auto-discovery.
 *
 * ADC1 pins used (safe with WiFi): 32, 33, 34, 35, 36 (VN), 39 (VP)
 *
 * Build:  pio run -e esp32-wifi
 * Flash:  pio run -e esp32-wifi -t upload
 * Binary: .pio/build/esp32-wifi/firmware.bin → Osmograph/firmware/
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>

// ADC1 pins only (WiFi-safe — ADC2 conflicts with WiFi)
static const int mq_pins[] = {36, 39, 34, 35, 32, 33};
static const int PIN_COUNT = 6;

static const char* AP_PASSWORD = "osmograph";
static const int   TCP_PORT    = 8080;

static const char* STA_SSID = "";   // set for station mode
static const char* STA_PASS = "";

static WiFiServer tcp_server(TCP_PORT);
static WiFiClient tcp_client;
static char ap_ssid[32];

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n--- OpenSmell WiFi Firmware ---");

  uint32_t id = (uint32_t)ESP.getEfuseMac() & 0xFFFFFF;
  snprintf(ap_ssid, sizeof(ap_ssid), "Osmograph-%06X", id);

  // Configure all ADC1 pins
  for (int i = 0; i < PIN_COUNT; i++) {
    pinMode(mq_pins[i], INPUT);
    analogSetPinAttenuation(mq_pins[i], ADC_11db);
  }

  // Start AP
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap_ssid, AP_PASSWORD);
  Serial.printf("AP: %s  IP: %s\n", ap_ssid, WiFi.softAPIP().toString().c_str());

  // Optional station connection
  if (strlen(STA_SSID) > 0) {
    WiFi.begin(STA_SSID, STA_PASS);
    Serial.printf("Connecting to %s", STA_SSID);
    for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) {
      delay(500); Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED)
      Serial.printf("\nStation IP: %s\n", WiFi.localIP().toString().c_str());
    else
      Serial.println("\nStation fail (AP only)");
  }

  // mDNS
  if (MDNS.begin("osmograph")) {
    MDNS.addService("_osmograph", "_tcp", TCP_PORT);
    Serial.println("mDNS: _osmograph._tcp");
  } else {
    Serial.println("mDNS fail");
  }

  tcp_server.begin();
  tcp_server.setNoDelay(true);
  Serial.printf("TCP ready on port %d\n", TCP_PORT);
}

void loop() {
  static unsigned long last = 0;
  unsigned long now = millis();

  if (!tcp_client || !tcp_client.connected()) {
    if (tcp_client) tcp_client.stop();
    tcp_client = tcp_server.available();
    if (tcp_client) Serial.println("Client connected");
  }

  if (now - last < 50) return;
  last = now;

  char buf[128];
  int pos = 0;
  for (int i = 0; i < PIN_COUNT; i++) {
    int raw = analogRead(mq_pins[i]);
    pos += snprintf(buf + pos, sizeof(buf) - pos, "%s%d", i > 0 ? "," : "", raw);
  }
  buf[pos++] = '\n';
  buf[pos] = '\0';

  Serial.print(buf);

  if (tcp_client && tcp_client.connected())
    tcp_client.write((uint8_t*)buf, pos);
}
