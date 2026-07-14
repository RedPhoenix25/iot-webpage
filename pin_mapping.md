# ESP32 Hardware Pin Mapping

This document outlines the final pin assignments for the IoT Energy Hub project. 

> [!WARNING]
> **Analog Pin Constraint**: The ESP32 disables ADC2 pins when Wi-Fi is active. Because of this, all analog energy sensors (ACS712 and ZMPT101B) must exclusively use **ADC1 pins** (32, 33, 34, 35, 36, 39). 
> 
> To accommodate this, the LDR was shifted to use its Digital Out (DO) pin instead of its Analog Out (AO) pin.

## ⚡ Relays (Digital Outputs)
These pins trigger the 4-channel relay module. They were chosen to avoid strapping pins that could cause booting issues if pulled high/low during startup.
- **Socket 1 (Fan/AC)**: `GPIO 26`
- **Socket 2**: `GPIO 27`
- **Socket 3**: `GPIO 14`
- **Socket 4**: `GPIO 13`

## 🌡️ Environment Sensors
- **DHT22 (Temp & Humidity)**: `GPIO 4` (Digital In)
- **PIR Motion Sensor**: `GPIO 5` (Digital In)
- **LDR Light Sensor**: `GPIO 15` (Digital In - using the sensor's `DO` pin)

## 📊 Energy Sensors (ADC1 Analog Inputs)
These pins read the analog voltage variations from the current and voltage sensors.
- **ZMPT101B (Voltage)**: `GPIO 36` (VP)
- **ACS712 Main Line (Total Hub Current)**: `GPIO 39` (VN)
- **ACS712 Socket 1**: `GPIO 34`
- **ACS712 Socket 2**: `GPIO 35`
- **ACS712 Socket 3**: `GPIO 32`
- **ACS712 Socket 4**: `GPIO 33`

## 📟 I2C Bus (Shared)
Both the LCD Display and the DS3231 RTC module share the standard ESP32 I2C pins. You can wire them in parallel (daisy-chain them) to these two pins.
- **SDA (Data)**: `GPIO 21`
- **SCL (Clock)**: `GPIO 22`
