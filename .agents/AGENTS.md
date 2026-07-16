# Activity Logging

Whenever significant changes or progress are made in this project, you must append a summary of the actions taken to the `activity_log.md` file located at the root of the workspace. 

Maintain the chronological structure of the log, use clear bullet points, and categorize updates sensibly (e.g., "UI Updates", "Backend Changes", "Hardware Configuration").

---

# IoT Energy Hub — System Behaviour Specification

This section documents all agreed system behaviours for the IoT Energy Hub project. Always refer to these rules when making changes to `src/main.cpp` or the React dashboard.

## Hardware Layout
- **Socket 1** → Bulb (Light)
- **Socket 2** → Fan
- **Socket 3** → General purpose
- **Socket 4** → General purpose
- **Main voltage sensor** → ZMPT101B on GPIO 36 (ADC1)
- **Main current sensor** → ACS712 on GPIO 39 (ADC1)
- **Socket current sensors** → ACS712 on GPIO 34 (S1), 35 (S2), 32 (S3), 33 (S4)
- **PIR motion sensor** → GPIO 5
- **LDR light sensor** → GPIO 15 (Digital Output pin of LM393 module)
- **DHT22** → GPIO 4
- **Relay module** → Active-LOW on GPIOs 26 (S1), 27 (S2), 14 (S3), 13 (S4)

## Voltage & Current Measurement
- Use **True AC RMS** via the variance method: sample for **3 full 20ms cycles** (one 50Hz cycle each).
- Store each cycle's RMS value **individually** per sensor channel — do not just accumulate a sum.
- Apply a **min-consistency gate**: a reading is only accepted if the **minimum** value across all 3 cycles exceeds the noise floor. This rejects erratic noise while accepting consistent real loads.
- Apply **Exponential Moving Average (EMA)** with **85% old / 15% new** weight to smooth stable readings without masking sudden load changes.
- **Noise floor**: 0.10A for all current channels; 10V for voltage.
- **Voltage calibration factor**: `CAL_VOLTAGE = 0.437` (tuned to match 219–222V multimeter reading).
- **Current calibration factors**: separate constants `CAL_CURRENT_MAIN`, `CAL_CURRENT_S1`–`S4`, all starting at `0.017` but individually adjustable.
- The **LCD top row** and the **dashboard Main Voltage card** must always read from the same global variables (`currentVoltage`, `currentAmperage`, `currentPower`) to stay in sync.

## Power Reporting
- If a **socket relay is OFF**, its current and power must be forced to **exactly 0** — never use the raw sensor value.
- The **Live Wattage gauge** on the dashboard derives total power from the **sum of active socket powers** whenever any socket is ON and reporting. It falls back to `mainCurrent × voltage` only when all sockets are off.
- With all sockets off, the system's own 15W adapter draw (~0.068A) is below the sensor threshold and correctly reports as **0W**.
- The **voltage safety cutoff** trips if voltage goes below 180V or above 240V, cutting all relays. It restores after 10 seconds of stable voltage.

## Automation Rules
### Rule 1 — Temperature → Fan (Socket 2)
- If temperature exceeds **30°C** and Socket 2 (Fan) is off, turn Socket 2 ON automatically.
- This rule only fires if the DHT22 reading is valid (not NaN).

### Rule 2 — Motion + LDR Smart Bulb (Socket 1)
The bulb follows a state machine to prevent feedback loops:
1. **Motion detected + bulb OFF + no override** → Turn bulb **ON**.
2. **Bulb ON + LDR state changes to light detected** (LDR went from dark to bright) → Turn bulb **OFF**, set `bulbOverride = true`. This prevents the bulb's own light from re-triggering motion in a loop.
3. **Bulb OFF + override active + LDR state changes to dark** → Turn bulb back **ON**, clear `bulbOverride`.
4. **No motion detected** → Turn bulb **OFF**, clear `bulbOverride`.

The LDR module (LM393) pulls LOW when **bright** and HIGH when **dark**. `ldrLight = (ldrState == LOW)`.

## Dashboard Behaviour
- Socket names must match hardware: **"Socket 1 (Bulb)"**, **"Socket 2 (Fan)"**, **"Socket 3"**, **"Socket 4"**.
- Sensor cards show `--` only when the value is `null` (e.g., DHT22 returning NaN).
- Motion shows **"Detected"** / **"Clear"** based on PIR state.
- Light Level shows percentage (0% = dark, 100% = bright).
- All MQTT communication uses the topic namespace `iot-hub/redphoenix25-v1-x8f9a2/`.

## Flashing Workflow
- Always use `pio run -t upload --upload-port COM7` to flash.
- After any firmware change, run `pio run` first to verify compilation before flashing.
- After a successful flash and verification, push all changes to GitHub with a descriptive commit message.
- Always update `activity_log.md` after significant changes.
