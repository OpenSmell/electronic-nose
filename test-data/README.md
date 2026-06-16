# Test Recordings

Sensor recordings made with the reference 3-sensor hardware (MQ-135, MQ-3, MQ-7).

| File | Substance | Sessions |
|------|-----------|----------|
| `garlic_session1.csv` | Garlic | 1 |
| `garlic_session2.csv` | Garlic | 2 |
| `garlic_session3.csv` | Garlic | 3 |
| `ginger_session1.csv` | Ginger | 1 |
| `ginger_session2.csv` | Ginger | 2 |

## Format

Raw ADC readings only (no column headers). 3 columns: MQ-135, MQ-3, MQ-7. These are loaded as "fw" format by Osmograph's training pipeline.

## Usage in Osmograph

1. Launch Osmograph
2. Open the **Train** tab
3. Click **Discover Recordings** — these files will be auto-detected
4. Assign labels, click **Train Classifier**

To record additional substances (lime, cinnamon, room air):
1. Connect your ESP32 board
2. Enter a label in the toolbar (e.g. "lime")
3. Click **Record** — the CSV saves to `~/Osmograph_Recordings/`
4. Repeat for each substance
5. Open the **Train** tab, discover all recordings, train a combined classifier
