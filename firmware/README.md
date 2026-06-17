# Firmware — ESP32 E‑Nose

PlatformIO project for the ESP32-based electronic nose.

**Note:** This firmware source is the original serial-only version. The current universal firmware (USB Serial + WiFi AP) is maintained in the [Osmograph](https://github.com/opensmell/osmograph) app under `board/compiler.py`. Use Osmograph for one-click firmware flashing — it handles both compilation and upload via esptool.

## Hardware Targets

- ESP32 Dev Kit V1 (ESP-WROOM-32)
- MQ series sensors: MQ-135, MQ-3, MQ-7 (3-sensor config)
- Optional: MQ-6, MQ-4, MQ-8 (6-sensor config)

## Build & Flash (Legacy)

```bash
pip install platformio
cd firmware
pio run -t upload
pio device monitor
```

## Data Format

The universal Osmograph firmware outputs lines in this format over both Serial (115200 baud) and WiFi TCP (port 8080):

```
OSM,<adc0>,<adc1>,...
```

The `OSM` prefix allows the Osmograph app to distinguish sensor data from bootloader output. Readings arrive every 500ms.
