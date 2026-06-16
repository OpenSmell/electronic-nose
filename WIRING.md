# Wiring Guide

For the standard 3-sensor configuration:

| Sensor | GPIO | ADC Channel |
|--------|------|-------------|
| MQ-135 | GPIO34 | ADC1_CH0 |
| MQ-3   | GPIO35 | ADC1_CH1 |
| MQ-7   | GPIO32 | ADC1_CH4 |

Power: All MQ sensors require 5V Vcc and the heater is always-on when powered. The logic/ADC output pins are 3.3V compatible. Connect sensor DOUT pins through a voltage divider if using 5V sensors.

Ground: Common ground between ESP32 and all sensors.
