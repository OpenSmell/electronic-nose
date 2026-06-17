# OpenSmell Electronic Nose — Reference Hardware

Build an interoperable electronic nose from locally available parts.

## Quick Start

1. **Buy parts** — See [BOM.csv](BOM.csv) for a full shopping list.
2. **Wire it** — Follow [WIRING.md](WIRING.md) for pin connections. For a step-by-step build guide with photos, see [BUILD.md](BUILD.md).
3. **Flash firmware** — Use [Osmograph](../Osmograph/) for one-click flashing:
   ```bash
   pip install -r ../Osmograph/requirements.txt
   python -m Osmograph
   ```
   Connect your board, click **Detect Board**, select your sensor configuration, and Osmograph flashes the ESP32 via esptool.
4. **Record** — In Osmograph, enter a substance label and press Record. Or use `screen /dev/ttyUSB0 115200` to log raw CSV data.
5. **Train** — Record 30+ seconds per substance, open the Train tab, and train a classifier.

## Sensor Configurations

The same firmware pattern works for any number of MQ sensors (1–6) and even I2C sensors like the BME688.

| Sensors | Possible use case |
|---------|-------------------|
| 1 (MQ-135) | General VOC detection |
| 2 (MQ-135, MQ-3) | Food vs alcohol distinction |
| 3 (MQ-135, MQ-3, MQ-7) | Multi-gas food identification |
| 4 (add MQ-6) | Food spoilage + LPG detection |
| 6 (add MQ-4, MQ-8) | Full-spectrum gas fingerprinting |

The 6-sensor configuration matches the reference [SmellNet](https://github.com/opensmell/SmellNet) dataset. Fewer sensors means less information per sample, which may reduce classification accuracy for fine-grained substance distinctions.

### Adding more sensors — the pattern

Each sensor is three lines in the firmware:

```cpp
#define SENSOR1_PIN 34   // 1. Define pin
int val1 = analogRead(SENSOR1_PIN);  // 2. Read ADC
Serial.print(val1); Serial.print(",");  // 3. Print CSV
```

Or better: edit the `SENSOR_PINS[]` array in Osmograph's `board/compiler.py` and recompile through the app — no manual code editing needed.

### Mapping N sensors to the 6-channel encoder

The OpenSmell encoder expects 6 input channels. If your device has fewer, you need a channel mapping. For example, a common 3-sensor mapping (MQ-135, MQ-3, MQ-7 → 6 channels) is:

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
```

Osmograph handles this mapping automatically during training and prediction — see `viz/train_tab.py` and `viz/realtime_classifier.py`.

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
  archive/
    visualizer.py     — Archived real-time sensor dashboard (superseded by Osmograph)
  firmware/      — PlatformIO firmware source
```

## Related Resources

- [OpenSmell SDK](https://github.com/opensmell/opensmell) — `pip install opensmell`
- [Osmograph GUI](https://github.com/opensmell/osmograph) — Desktop app for recording and training
- [Universal Encoder](https://github.com/opensmell/universal-encoder) — Training code
- [Chemoprint Optimization](https://github.com/opensmell/chemoprint-optimization) — RDKit descriptor selection
- [Data Commons](https://github.com/opensmell/data-commons) — Contribute your recordings
- [Session-Invariance Proof](https://github.com/opensmell/session-invariance)
- [Discord Community](https://discord.gg/CGER3tHxbH)
