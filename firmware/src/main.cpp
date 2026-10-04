/*
 * OpenSmell E-Nose Universal Firmware
 * Supports USB Serial AND WiFi AP + TCP on port 8080.
 * Falls back to Serial-only if WiFi fails.
 * ADC1 pins only (WiFi-safe): 32, 33, 34, 35, 36, 39
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <stdlib.h>
#include <string.h>

// Nominal sample period in milliseconds. 500 ms (2 Hz) is the default because
// that is the cadence measured on all 13 reference recordings; see
// SAMPLING_CONTRACT.md. Override at build time with
// -DOSMOGRAPH_SAMPLE_INTERVAL_MS=100 for a 10 Hz build.
#ifndef OSMOGRAPH_SAMPLE_INTERVAL_MS
#define OSMOGRAPH_SAMPLE_INTERVAL_MS 500
#endif
static_assert(OSMOGRAPH_SAMPLE_INTERVAL_MS >= 10,
              "sample interval below 10 ms exceeds what sequential ADC reads can sustain");

static const int mq_pins[] = {32, 33, 34, 35, 36, 39};
#ifndef OSMOGRAPH_PIN_COUNT
#define OSMOGRAPH_PIN_COUNT 6
#endif
static const int PIN_COUNT = OSMOGRAPH_PIN_COUNT;
static_assert(PIN_COUNT >= 1 && PIN_COUNT <= 6, "OSMOGRAPH_PIN_COUNT must be 1..6");

static WiFiServer tcp_server(8080);
static WiFiClient tcp_client;
static bool wifi_ok = false;

// Phase boundaries. A boundary is announced on every output immediately before
// the first OSM line of the new phase, so a reader knows which sample the label
// belongs to without counting. A host injects one with
//   EVENT,<label>,<sample_index>[,t_ms=<MS>]
// where a sample_index of -1 asks the device to assign its own next index.
// See docs/protocols/wire-protocol.md, EVENT message.
#define EVENT_LABEL_MAX 32
static bool pending_event = false;
static char pending_label[EVENT_LABEL_MAX];
static long pending_index = -1;
static long pending_t_ms = -1;
static long sample_index = 0;

static void announce_to_all(const char *line) {
  Serial.println(line);
  if (wifi_ok && tcp_client && tcp_client.connected()) {
    tcp_client.println(line);
  }
}

static void info_line(char *out, size_t cap) {
  snprintf(out, cap, "INFO,universal-esp32,1.0.0,%d,interval_ms=%d", PIN_COUNT,
           OSMOGRAPH_SAMPLE_INTERVAL_MS);
}

// Emit a queued boundary, if any, immediately before the sample it annotates.
// Flushing it after the OSM line instead would attach the label to the previous
// sample, which is the one mistake a phase-aware reader cannot detect.
static void flush_pending_event() {
  if (!pending_event) return;
  char line[96];
  int n = snprintf(line, sizeof(line), "EVENT,%s,%ld", pending_label, pending_index);
  if (pending_t_ms >= 0 && n > 0 && (size_t)n < sizeof(line)) {
    snprintf(line + n, sizeof(line) - n, ",t_ms=%ld", pending_t_ms);
  }
  announce_to_all(line);
  pending_event = false;
}

// Only EVENT is accepted: this is a streaming sensor, so there is nothing else to
// configure at runtime. Reading over USB as well as TCP means a host attached to
// the serial port can inject a boundary; the sketch only ever writes to that
// port, so reading from it cannot eat a byte of our own output.
static void read_commands(Stream &in, Print &out) {
  while (in.available()) {
    String cmd = in.readStringUntil('\n');
    cmd.trim();
    if (!cmd.startsWith("EVENT,")) continue;

    // The label is quoted between the first two commas rather than split
    // blindly: a comma inside it would shift every following field, which is
    // exactly what ERR,8 exists to report.
    int c1 = cmd.indexOf(',');
    int c2 = cmd.indexOf(',', c1 + 1);
    if (c1 < 0 || c2 < 0) {
      out.println("ERR,8,EVENT needs <label> and <sample_index>");
      continue;
    }
    String label = cmd.substring(c1 + 1, c2);
    String rest = cmd.substring(c2 + 1);
    long t_ms = -1;
    int t_at = rest.indexOf("t_ms=");
    if (t_at >= 0) {
      t_ms = strtol(rest.substring(t_at + 5).c_str(), NULL, 10);
      rest = rest.substring(0, t_at);
    }
    if (rest.length() == 0) {
      out.println("ERR,8,EVENT needs a sample_index");
      continue;
    }
    if (label.length() == 0 || label.length() >= EVENT_LABEL_MAX) {
      out.printf("ERR,8,EVENT label must be 1..%d characters\n", EVENT_LABEL_MAX - 1);
      continue;
    }

    long index = strtol(rest.c_str(), NULL, 10);
    strncpy(pending_label, label.c_str(), EVENT_LABEL_MAX - 1);
    pending_label[EVENT_LABEL_MAX - 1] = '\0';
    // A negative index means "device assigns": the host is not counting the
    // stream, so only the device knows which sample this boundary lands on.
    pending_index = (index < 0) ? sample_index : index;
    pending_t_ms = t_ms;
    pending_event = true;
  }
}

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
  // Declare the cadence on the wire, over USB as well as TCP. A host that has to
  // assume a sample rate will eventually assume the wrong one, and every
  // count-based temporal feature (rise time, decay time, latency) then scales by
  // that error.
  char info[80];
  info_line(info, sizeof(info));
  announce_to_all(info);
}

void loop() {
  static unsigned long last = 0;
  unsigned long now = millis();

  // TCP accept
  if (wifi_ok) {
    if (!tcp_client || !tcp_client.connected()) {
      if (tcp_client) tcp_client.stop();
      tcp_client = tcp_server.available();
      if (tcp_client && tcp_client.connected()) {
        // Every client is told the cadence, not just the first one. A client that
        // joins an already-running stream has no other way to learn it, and that
        // is the case that silently rescales every temporal feature when the
        // interval is missing.
        char info[80];
        info_line(info, sizeof(info));
        tcp_client.println(info);
      }
    }
  }

  if (Serial) {
    read_commands(Serial, Serial);
  }
  if (wifi_ok && tcp_client && tcp_client.connected()) {
    read_commands(tcp_client, tcp_client);
  }

  if (now - last < OSMOGRAPH_SAMPLE_INTERVAL_MS) return;
  last = now;

  flush_pending_event();

  char buf[96];
  // The OSM prefix is part of the wire format, not decoration: a reader that
  // dispatches on message type cannot tell a sample from an EVENT line without
  // it, and a spec-conforming reader drops unprefixed CSV entirely.
  int pos = snprintf(buf, sizeof(buf), "OSM");
  for (int i = 0; i < PIN_COUNT; i++) {
    pos += snprintf(buf + pos, sizeof(buf) - pos, ",%d", analogRead(mq_pins[i]));
  }
  buf[pos++] = '\n';
  buf[pos] = '\0';

  Serial.print(buf);

  if (wifi_ok && tcp_client && tcp_client.connected()) {
    tcp_client.write((uint8_t*)buf, pos);
  }

  sample_index++;
}
