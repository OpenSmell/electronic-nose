# Firmware — ESP32 E‑Nose

PlatformIO project for the ESP32-based electronic nose.

## Hardware Targets

- ESP32 Dev Kit V1 (ESP-WROOM-32)
- MQ series sensors: MQ-135, MQ-3, MQ-7 (3-sensor config)
- Optional: MQ-6, MQ-4, MQ-8 (6-sensor config)

## Build & Flash

```bash
# Install PlatformIO CLI
pip install platformio

# Build and upload
cd firmware
pio run -t upload

# Monitor serial output
pio device monitor
```

## Sensor Configurations

Edit `src/main.cpp` and set `ACTIVE_SENSORS` to match your hardware:

- `3` — MQ-135, MQ-3, MQ-7 (standard)
- `4` — MQ-135, MQ-3, MQ-6, MQ-7
- `6` — all six MQ sensors

## Data Format

Outputs CSV over serial at 115200 baud:
```
timestamp_ms, MQ135, MQ3, MQ6, MQ7, MQ4, MQ8
```
