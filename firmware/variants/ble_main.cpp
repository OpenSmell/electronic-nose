/*
 * OpenSmell E-Nose BLE Firmware
 * Advertises sensor data via BLE. Use with Osmograph's BleReader.
 * Pins: 32,33,34,35,36,39 (ADC1, safe with BLE)
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

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
static const int PIN_COUNT = 6;

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

static BLEServer* server = nullptr;
static BLECharacteristic* characteristic = nullptr;
static bool connected = false;

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override { connected = true; }
  void onDisconnect(BLEServer* s) override { connected = false; }
};

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("OSM:boot");

  for (int i = 0; i < PIN_COUNT; i++) {
    pinMode(mq_pins[i], INPUT);
    analogSetPinAttenuation(mq_pins[i], ADC_11db);
  }

  // Declare the cadence on the wire so the host never has to assume it.
  Serial.printf("INFO,universal-esp32-ble,1.0.0,%d,interval_ms=%d\n",
                PIN_COUNT, OSMOGRAPH_SAMPLE_INTERVAL_MS);

  BLEDevice::init("Osmograph-BLE");
  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService* service = server->createService(SERVICE_UUID);
  characteristic = service->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  characteristic->addDescriptor(new BLE2902());

  service->start();
  BLEAdvertising* adv = server->getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->start();

  Serial.println("OSM:ble ready");
}

void loop() {
  static unsigned long last = 0;
  unsigned long now = millis();
  if (now - last < OSMOGRAPH_SAMPLE_INTERVAL_MS) return;
  last = now;

  char buf[96];
  // The OSM prefix is part of the wire format, not decoration: a reader that
  // dispatches on message type cannot tell a sample from a control line without
  // it, and a spec-conforming reader drops unprefixed CSV entirely.
  int pos = snprintf(buf, sizeof(buf), "OSM");
  for (int i = 0; i < PIN_COUNT; i++) {
    pos += snprintf(buf + pos, sizeof(buf) - pos, ",%d", analogRead(mq_pins[i]));
  }
  buf[pos++] = '\n';
  buf[pos] = '\0';

  Serial.print(buf);

  if (connected) {
    characteristic->setValue((uint8_t*)buf, pos);
    characteristic->notify();
  }
}
