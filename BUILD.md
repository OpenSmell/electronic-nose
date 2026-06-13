# BUILD.md — The OpenSmell Hardware Guide

This document takes you from a pile of parts to a working chemical sensing
instrument. It covers sensor selection, wiring, firmware, burn‑in, first
test, and calibration. No prior electronics experience is assumed.

---

## 0. What You're Building

You are building a device that detects volatile organic compounds in the air.
It uses heated metal‑oxide sensors whose resistance changes when gas molecules
contact their surface. A microcontroller reads those resistance changes and
streams them to a computer. Software then converts those raw streams into a
stable, device‑independent representation that any application can consume.

The same garlic clove produces different raw voltages on two different devices.
The OpenSmell pipeline cancels those hardware differences so that garlic is
always recognised as garlic, regardless of which device recorded it.

---

## 1. Choosing Sensors

Sensor count determines what your instrument can distinguish. Each sensor
responds to a broad range of gases, but with different sensitivity profiles.
Two sensors provide two independent chemical dimensions. Four sensors provide
four. More dimensions mean finer discrimination between similar smells.

There is no universal "best" sensor count. The right number depends entirely
on what you want to detect.

### What you can do with N sensors

| Sensors | Reliable applications |
|---------|----------------------|
| **1–2** | Detect presence vs. absence of a known substance. Gas leak alarms. Smoke detection. Basic alcohol sensing. |
| **3** | Distinguish chemically different foods (garlic vs. coffee, citrus vs. herbs). Track spoilage over days. Detect anomalies against a clean‑air baseline. |
| **4** | Distinguish chemically similar foods (garlic vs. ginger, onion vs. shallot). Higher accuracy on all classification tasks. |
| **5–6** | Maximum chemical resolution with current MOX technology. Complex mixture analysis. Research‑grade applications. |

Start with three sensors if you are unsure. You can add more later—the firmware
and software handle any sensor count automatically.

### Common metal‑oxide sensors

| Sensor | Primary sensitivity | Typical use |
|--------|--------------------|-------------|
| MQ‑135 | NH₃, benzene, broad VOCs | General‑purpose food and air quality |
| MQ‑3 | Ethanol, alcohols | Breath analysis, fermentation |
| MQ‑7 | Carbon monoxide | Air quality, combustion detection |
| MQ‑6 | LPG, propane, butane | Gas leak detection, cooking safety |
| MQ‑4 | Methane, natural gas | Gas leak detection |
| MQ‑8 | Hydrogen | Industrial, research |
| MQ‑2 | Smoke, general combustibles | Fire detection |
| MQ‑5 | LPG, natural gas | Gas leak detection |
| MiCS‑6814 | CO, NO₂, NH₃ (three channels) | Compact multi‑purpose |
| BME688 | Broad VOCs (digital, I²C) | Compact environmental sensing |
| TGS‑series | Various (Figaro) | Higher stability, research |

Any metal‑oxide sensor that produces a time‑varying signal—analogue voltage or
digital reading—can be used with this guide. The calibration pipeline does not
care how the data arrived. It only cares that the sensor responds to the
chemicals you want to detect.

### Using a sensor not listed here

If you have a sensor that is not in the table above, you need to know two
things: its target gas(es) and its operating voltage. Nearly all MQ‑family
sensors run at 5 V and use the same voltage‑divider circuit described in
Section 3. Digital sensors (BME688, SGP30) connect via I²C or SPI and require
slightly different firmware—see Section 5.

---

## 2. Bill of Materials

The following components are needed for a 3‑sensor device. Multiply the
sensor and resistor rows for larger builds. Prices are approximate in USD
and will vary by region. Source locally where possible.

| Item | Qty | Notes |
|------|-----|-------|
| ESP32 dev board (38‑pin) | 1 | Any ESP32 development board. Other microcontrollers with ADC and serial work, but ESP32 is the reference platform. |
| MQ‑135 sensor module | 1 | Or equivalent broad‑spectrum VOC sensor. |
| MQ‑3 sensor module | 1 | Or equivalent alcohol‑selective sensor. |
| MQ‑7 sensor module | 1 | Or equivalent CO‑selective sensor. |
| Breadboard (any size) | 1 | For prototyping. Can be replaced by direct wiring later. |
| Jumper wires (male‑male) | 10–15 | For connections. |
| 10 kΩ resistors | 2 per sensor | For the voltage‑divider circuit. |
| Micro USB data cable | 1 | Must support data transfer, not just charging. |
| Computer (Linux, Windows, macOS) | 1 | For running the Osmograph app. |

**Optional but recommended:**

| Item | Qty | Notes |
|------|-----|-------|
| DHT22 temperature/humidity sensor | 1 | Environmental logging. Improves drift tracking. |
| Container for samples | 1 | Any cup, bowl, jar, or beaker to hold samples near the sensors. |
| Airtight container | 1 | For storing the device when not in use. |
| Silica gel packets or dry rice | a few | Desiccant for storage. |

---

## 3. Wiring

### The circuit

Each MQ‑family sensor module has four pins: **VCC** (power), **GND** (ground),
**DO** (digital output—ignore), and **AO** (analogue output). Only VCC, GND,
and AO are used.

The AO pin produces a voltage that varies with gas concentration. To protect
the ESP32's 3.3 V input pins, each AO line needs a voltage divider made of
two equal resistors. For any resistance R (10 kΩ recommended):

```
Sensor AO pin ─── R ───┬─── ESP32 ADC pin
                        │
                        R
                        │
                       GND
```

The ADC pin sees approximately half the sensor's output voltage, which keeps
it safely within the 0–3.3 V range.

### Power

MQ sensors are designed for 5 V. Connect the ESP32's **VIN** (5 V) pin to the
red (+) power rail and the ESP32's **GND** pin to the blue (–) rail. Every
sensor's VCC pin connects to the red rail. Every sensor's GND pin connects to
the blue rail.

### Pin mapping

Each sensor's AO line must connect to a different ADC‑capable GPIO pin on the
ESP32. The following GPIOs are recommended (all belong to ADC1, which works
even when WiFi is active):

| GPIO | Recommended for |
|------|----------------|
| 34 | First sensor |
| 35 | Second sensor |
| 32 | Third sensor |
| 33 | Fourth sensor |
| 25 | Fifth sensor |
| 26 | Sixth sensor |

You can use any ADC1 pin (32–39). Just ensure each sensor has a unique pin and
update the firmware accordingly.

### Wiring checklist

- [ ] ESP32 VIN (5 V) → red rail
- [ ] ESP32 GND → blue rail
- [ ] Each sensor VCC → red rail
- [ ] Each sensor GND → blue rail
- [ ] Each sensor AO → voltage divider (two equal resistors) → unique GPIO
- [ ] No bare metal touching between the red and blue rails

---

## 4. Enclosure

During prototyping, the device can sit on a desk in open air. For regular use,
store it in an airtight container to keep dust and humidity out.

When recording a sample, any container that holds the smell near the sensors
works—a cup, bowl, jar, or bag. The goal is simply to expose the sensors to
the volatile compounds.

---

## 5. Firmware

### Reference firmware (MQ‑family, analogue)

The reference firmware reads each sensor's ADC pin and prints comma‑separated
values to the serial port at 115200 baud. It is sensor‑count‑agnostic: add or
remove sensor definitions as needed.

If you have installed PlatformIO (VS Code extension), open the `firmware/`
folder in the `electronic‑nose` repository, edit `src/main.cpp` to match your
pin selections, and click Upload. If you do not have PlatformIO, the Osmograph
desktop app can flash pre‑compiled firmware for common sensor configurations
with one click.

### Digital sensors (BME688, SGP30, etc.)

Digital sensors communicate via I²C or SPI rather than analogue voltage. You
need different firmware to initialise the sensor and read its registers. The
output—a stream of comma‑separated values over serial—remains the same.
Osmograph and the OpenSmell pipeline do not care whether the numbers came from
an ADC or an I²C transaction. If you write firmware for a digital sensor,
follow the same CSV format and baud rate, and everything downstream will work.

---

## 6. Burn‑In

New metal‑oxide sensors need approximately 24 cumulative hours powered on
before their readings stabilise. This is a one‑time requirement. The clock
does not reset if power is interrupted—it is the total powered‑on time that
matters.

1. Plug the ESP32 into any USB power source (wall charger, laptop, power bank).
2. Leave it powered. The sensors will feel warm—that is the internal heater.
3. Note the start time. After 24 cumulative hours, the sensors are ready.
4. Before every recording session, let the sensors warm up for 5 minutes.

The burn‑in does not need to be continuous. If the power goes out, resume
when it returns. The Osmograph app includes a persistent burn‑in timer.

---

## 7. First Test

After burn‑in:

1. Flash the firmware (PlatformIO or Osmograph).
2. Open the Serial Monitor (115200 baud) or launch Osmograph.
3. Confirm you see comma‑separated numbers streaming steadily.
4. Crush a clove of garlic. Bring it near the sensors. The numbers should
   rise or fall noticeably.
5. Remove the garlic. The numbers should drift back toward their original
   baseline over 30–60 seconds.

If the numbers move, your instrument is working.

---

## 8. Calibration (in active development)

A new device produces voltages in its own coordinate system. The calibration
pipeline translates those voltages into the shared representation that
OpenSmell apps understand. The Osmograph Calibration
Wizard is under active development and will guide you through this process.

In the meantime, you can use your device for within‑session experiments:
record a substance, record another substance, and observe the differences
in the sensor traces and latent vectors.

---

## 9. Using Apps

Once calibrated, you can run OpenSmell apps that read your sensor data and
produce results—substance labels, spoilage alerts, or whatever the app was
designed to do. Calibration is required once per device; you do not need to
recalibrate unless you change your hardware (add sensors, replace the ESP32,
modify the circuit). Day‑to‑day use requires only a 5‑minute warm‑up before
each session.

---

## 10. Contributing Data

When you record a substance and label it, you can upload that recording to the
OpenSmell Data Commons. Your contribution helps train the next version of the
encoder and improves the standard for everyone. The uploader is built into
Osmograph. Recordings are stored on HuggingFace under the `opensmell/community`
dataset.

---

## 11. Troubleshooting

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| No numbers in serial monitor | Wrong baud rate | Set to 115200 |
| All values are zero | Loose VCC or GND connection | Press each wire firmly into the breadboard |
| One sensor always reads zero | Loose voltage‑divider connection | Check both resistors on that sensor's AO line |
| Values drift wildly | Insufficient burn‑in | Continue burn‑in until 24 cumulative hours |
| Values drift during a session | Sensors still warming up | Wait 5 minutes after power‑on before recording |
| ESP32 not recognised by computer | Charge‑only USB cable | Use a data‑sync cable |
| Permission denied on serial port | Linux group membership | Run `sudo usermod -a -G dialout $USER` and log out/in |

---

## 12. Next Steps

- Record a variety of substances and build your own smell library.
- Train a classifier on your recordings for a custom application.
- Upload labelled data to the Data Commons.
- Join the OpenSmell Discord to share results and get help.
