# OpenSmell Electronic Nose — Reference Hardware

Build an interoperable electronic nose from locally available parts.

## Quick Start

1. **Buy parts** — See [BOM.csv](BOM.csv) for a full shopping list.
2. **Wire it** — Follow [WIRING.md](WIRING.md) for pin connections. For a step-by-step build guide with photos, see [BUILD.md](BUILD.md).
3. **Flash firmware** — Install [PlatformIO](https://platformio.org/), open `firmware/`, upload:
   ```bash
   cd firmware
   platformio run -t upload
   ```
   Or use Osmograph for simplicity, details in [BUILD.md](BUILD.md).
4. **Record** — Run `python src/visualizer.py` (or `screen /dev/ttyUSB0 115200`), expose sensor to a substance, save CSV.
5. **Test** — Run the interoperability test:
   ```bash
   python src/interop_test.py my_recording.csv
   ```

## Sensor Tiers — Scale Up or Down

The same firmware pattern works for **any number of MQ sensors** (1–6) and even I2C sensors like the BME688.

| Tier | Sensors | Use case |
|------|---------|----------|
| Bare Minimum | 1 (MQ-135) | General VOC detection |
| Better | 2 (MQ-135, MQ-3) | Food vs alcohol |
| Standard | **3 (MQ-135, MQ-3, MQ-7)** | Food identification (this build) |
| Advanced | 4 (MQ-135, MQ-3, MQ-6, MQ-7) + DHT22 | Food spoilage, substance ID |
| Professional | 6 (add MQ-4, MQ-8) + BME680 | Lab-grade fingerprinting |

### Adding more sensors — the pattern

The Osmograph GUI handles this as well, but the firmware is minimal by design. Each sensor is three lines:

```cpp
#define SENSOR1_PIN 34   // 1. Define pin
int val1 = analogRead(SENSOR1_PIN);  // 2. Read ADC
Serial.print(val1); Serial.print(",");  // 3. Print CSV
```

To add a 4th MQ sensor (e.g., MQ-6), add to `firmware/src/main.cpp`:
```cpp
#define MQ6_PIN   33       // new pin
```
And in `loop()`:
```cpp
int mq6 = analogRead(MQ6_PIN);
```
Append to `Serial.print` chain. That's it.

To add the BME688 (I2C environmental sensor), you need the `Adafruit_BME680` library and a separate read path — see the Advanced tier branch.

### Mapping N sensors to the 6-channel encoder

The OpenSmell encoder expects 6 input channels. If your device has fewer, you need a channel mapping. The default 3-to-6 mapping is:

| Your sensor | Your channel | Encoder channel |
|------------|-------------|-----------------|
| MQ-135 | 0 | 0 (NO2) |
| MQ-3 | 1 | 1 (C2H5OH) |
| MQ-135 (replicate) | 0 | 2 (VOC) |
| MQ-7 | 2 | 3 (CO) |
| MQ-3 (replicate) | 1 | 4 (Alcohol) |
| — | — | 5 = constant (LPG mean) |

In Python (using the SDK):
```python
from opensmell.preprocessing import expand_channels
expanded = expand_channels(your_3ch_array, mapping=[(0,0), (1,1), (0,2), (2,3), (1,4)])
# Now pass expanded to the encoder
```

For 4 sensors (e.g., MQ-135, MQ-3, MQ-6, MQ-7), provide a 4-to-6 mapping:
```python
expanded = expand_channels(arr_4ch, mapping=[(0,0), (1,1), (2,2), (3,3), (1,4)])
```

For 6 sensors, no mapping needed — each channel maps to itself.

## Sensor Care

Full care guide at [CARE.md](CARE.md).


## Project Structure

```
electronic-nose/
  BUILD.md       — Step-by-step build guide (start here)
  BOM.csv        — Full bill of materials with prices
  WIRING.md      — Pinout reference
  CARE.md        — Sensor burn-in and maintenance
  ENCLOSURE.md   — Enclosure design notes
  EXPERIMENT.md  — Interoperability protocol
  src/
    visualizer.py     — Real-time sensor dashboard
    interop_test.py   — CLI interoperability test
  firmware/      — PlatformIO firmware (N-sensor pattern)
```

## Related Resources

- [OpenSmell SDK](https://github.com/opensmell/opensmell) — `pip install opensmell`
- [Osmograph GUI](https://github.com/opensmell/osmograph) — Desktop app for recording
- [Universal Encoder](https://github.com/opensmell/universal-encoder) — Training code
- [Chemoprint Optimization](https://github.com/opensmell/chemoprint-optimization) — RDKit descriptor selection
- [Data Commons](https://github.com/opensmell/data-commons) — Contribute your recordings
- [Session-Invariance Proof](https://github.com/opensmell/session-invariance)
- [Discord Community](https://discord.gg/CGER3tHxbH)
