# Firmware — ESP32 E‑Nose (Archived)

Original serial-only firmware for the ESP32-based electronic nose. Retained for reference.

**The current firmware lives in the Osmograph app** (`board/compiler.py`) and supports USB Serial + WiFi AP simultaneously with one-click flashing via esptool. See the [Osmograph README](../../Osmograph/README.md) for how to flash your board.

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

The Osmograph firmware outputs lines in this format over both Serial (115200 baud) and WiFi TCP (port 8080):

```
OSM,<adc0>,<adc1>,...
```

The `OSM` prefix allows the Osmograph app to distinguish sensor data from bootloader output. Readings arrive every 500ms.
