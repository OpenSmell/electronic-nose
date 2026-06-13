# OpenSmell Firmware

PlatformIO project for the ESP32-based electronic nose.

## Prerequisites

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- USB cable (data-sync, not charge-only)

## Flash

```bash
cd firmware
platformio run -t upload
```

## Monitor

```bash
platformio device monitor -b 115200
```

## Add a sensor

1. Add pin definition: `#define MQ6_PIN 33`
2. Add `analogRead()` in `loop()`
3. Append to `Serial.print()` chain
4. Update the channel mapping in the SDK (see main repo README)

## Customize

Edit `platformio.ini` to change board, upload speed, or monitor baud rate.
