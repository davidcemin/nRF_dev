===============================
Nordic BLE DS18B20 Temperature Sensor
===============================

Overview
--------

This project demonstrates how to use the **Nordic nRF7002 DK (nRF5340 SoC)** with the **Zephyr RTOS** to read temperature from a **DS18B20 1-Wire sensor** and make it available over Bluetooth Low Energy (BLE).

The device:
- Reads temperature from the DS18B20 using Zephyr's `w1_thermo` driver.
- Publishes the temperature via the **Environmental Sensing Service (ESS)** using the standard GATT Temperature Characteristic (UUID 0x2A6E).
- Optionally includes the latest reading in BLE advertising packets (Manufacturer Data).
- Can be read by any BLE-compatible device (smartphone, tablet, PC).
- Updates continuously at 1 Hz.

Hardware Requirements
---------------------

- `Nordic nRF7002 DK` (nRF5340)
- DS18B20 temperature sensor (1-Wire)
- 4.7 kΩ pull-up resistor between data and VDD (required for 1-Wire)
- Jumper wires for connecting DS18B20 to the DK

Pin Connections
~~~~~~~~~~~~~~~

Example wiring (change pins in your overlay as needed):

+------------+---------------------+
| DS18B20    | nRF7002 DK          |
+============+=====================+
| VDD (3.0V) | 3V0 pin             |
+------------+---------------------+
| GND        | GND                 |
+------------+---------------------+
| DQ         | P0.xx (configured)  |
+------------+---------------------+
| 4.7k pull-up resistor between DQ and VDD |
+----------------------------------+

Firmware Features
-----------------

- **BLE Peripheral** role
- **Environmental Sensing Service (ESS)**
  - Temperature characteristic, 16-bit signed integer in 0.01 °C units
  - Supports GATT read and notify
- **Advertising**
  - Flags: LE General Discoverable, BR/EDR Not Supported
  - UUIDs: ESS (0x181A)
  - Manufacturer Data: Nordic Semiconductor Company ID (0x0059) + temperature in 0.01 °C
- **Update rate**: ~1 Hz
- LED blinking while advertising; solid ON when connected

Building and Flashing
---------------------

1. Install the Nordic nRF Connect SDK (NCS) v2.9.2 or newer.
2. Fetch this repository into your NCS workspace.
3. Connect your nRF7002 DK to your PC.
4. Build and flash:

.. code-block:: bash

   west build -b nrf7002dk/nrf5340/cpuapp --sysbuild
   west flash

Running
-------

- On boot, the device will start advertising as ``Nordic_TEMP`` (or your configured name).
- The temperature is updated in both:
  - The **GATT characteristic** (0x2A6E) under the Environmental Sensing Service (0x181A)
  - The **Manufacturer Data** field in advertisements

BLE Client Usage
----------------

- Using **nRF Connect Mobile**:
  1. Scan for the device.
  2. Connect and open the Environmental Sensing Service.
  3. Enable notifications on the Temperature characteristic.
  4. The value is a signed 16-bit integer in 0.01 °C (divide by 100 to get °C).

Example:

Raw value: 0x0A47 (little-endian: 0x470A) = 2631
Temperature: 26.31 °C

License
-------

SPDX-License-Identifier: Apache-2.0

This example is based on Nordic Semiconductor and Zephyr Project sample code.

