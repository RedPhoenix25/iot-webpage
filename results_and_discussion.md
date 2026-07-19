# Section 4: Results and Discussion

This section presents the results obtained during the evaluation and testing of the IoT Energy Hub. The system's performance was evaluated under various load conditions, automation scenarios, and configuration settings. The following subsections analyze the real-time monitoring interface, the intelligent load-shedding and tariff configurations, the historical analytics engine, and the physical sensor accuracy.

---

## 4.1 Real-Time Dashboard & Telemetry System (Command Center)

The central user interface of the system is the **Command Center**, a React-based Web Dashboard utilizing midnight glassmorphism styling. The dashboard connects via secure WebSockets over MQTT to a global HiveMQ public broker, subscribing to telemetry broadcasts from the ESP32 microcontroller. 

![Figure 4.1: Command Center Main Dashboard Dashboard Overview](file:///c:/Users/7thHokage/Desktop/IoT%20webpage/dashboard_overview.png)
*(Note: Refer to the dashboard screenshot showing live metrics: 221.0V, 0.40A, 88W, and environment variables.)*

### Discussion of Figure 4.1:
1. **MQTT Telemetry Sync**: As shown in the dashboard, the ESP32 is successfully connected and transmitting telemetry, indicated by the green **"Hub Online"** badge. The telemetry values on the dashboard correspond exactly with the local LiquidCrystal_I2C screen display, pulling from synchronized global variables (`currentVoltage`, `currentAmperage`, `currentPower`).
2. **Electrical Calculations**:
   - **Main Voltage**: Measured at **221.0 V**, which sits comfortably within the expected grid threshold of 219V–222V.
   - **Total Current**: Measured at **0.40 A**.
   - **Live Wattage**: The Hero card displays a calculated load of **88 W**. The system calculates this wattage using a hybrid routing approach: if any relays are active, it sums the active socket powers (derived from the individual socket ACS712 sensors); if all sockets are off, it defaults to the main ACS712 line sensor. At the time of this reading, Sockets 1, 3, and 4 are ON, and the sum of their loads equates to 88 W.
3. **Environmental Readings**:
   - **Temperature**: The DHT22 reads **27.0°C**, and relative humidity is **80.0%**. These values represent stable ambient indoor conditions.
   - **Light Level**: The LDR light sensor indicates **0%** (Dark), reflecting that the digital output of the LM393 comparator module is HIGH (no brightness).
   - **Motion**: The PIR sensor status reads **"Detected"**. The firmware uses edge-detection to flash this status on the dashboard for 3 seconds before resetting, maintaining responsiveness.

---

## 4.2 Tariff & Daily Budget Configuration (Load Shedding)

The system features an active load management menu designed to protect consumer budgets. Through the **System Settings** modal, users can configure their localized utility tariff and set a strict daily energy budget.

```carousel
![Figure 4.2a: Tariff Setup at ₦225 per kWh and 0.006 kWh Daily Energy Limit](file:///c:/Users/7thHokage/Desktop/IoT%20webpage/tariff_settings_high.png)
<!-- slide -->
![Figure 4.2b: Tariff Setup at ₦61 per kWh and 0.006 kWh Daily Energy Limit](file:///c:/Users/7thHokage/Desktop/IoT%20webpage/tariff_settings_low.png)
```
*(Note: Refer to the system settings modals showing the rate inputs of ₦225 and ₦61 with the budget threshold set to 0.006 kWh).*

### Discussion of Figure 4.2a & 4.2b:
1. **Dynamic Cost Calculations**: In Figure 4.2a, the tariff is configured to **₦225 per kWh** (a standard commercial tariff), whereas in Figure 4.2b, it is set to a lower rate of **₦61 per kWh**. The dashboard dynamically computes the **Estimated Monthly Cost** in real-time. For example:
   - With an accumulated monthly energy of **0.030 kWh** (as seen on the dashboard in Figure 4.1) and a tariff of ₦225/kWh, the estimated monthly cost is calculated as:
     $$\text{Cost} = 0.030 \text{ kWh} \times 225 \text{ ₦/kWh} = ₦6.75 \approx ₦7/\text{month}$$
     This perfectly matches the "Est. Monthly Cost" card reading of **₦7 / month** shown in Figure 4.1.
   - If the tariff is adjusted to ₦61/kWh, the projected cost drops to:
     $$\text{Cost} = 0.030 \text{ kWh} \times 61 \text{ ₦/kWh} = ₦1.83 \approx ₦2/\text{month}$$
     This demonstrates the system's ability to translate complex energy statistics into digestible financial figures instantly.
2. **Automated Load Shedding Operation**: The **Daily Energy Limit** is configured at **0.006 kWh/day**. 
   - According to the firmware, once the cumulative daily energy consumption (`dailyEnergyTotal`) exceeds the limit, the ESP32 initiates load shedding:
     $$\text{Condition: } \left(\frac{\text{dailyEnergyTotal}}{1000} + 0.0005\right) \ge \text{dailyEnergyLimitKwh}$$
   - When this condition is met, the microcontroller automatically cuts power to the non-essential sockets (**Socket 3** and **Socket 4**). 
   - In Figure 4.1, the **Live Energy** card indicates **0.008 kWh**, which has exceeded the 0.006 kWh daily budget limit. Under normal operation, Sockets 3 & 4 would have been shed automatically. However, the system allows for manual user overrides. The active switches for Socket 3 and Socket 4 in Figure 4.1 demonstrate a manual override event, where the user has chosen to switch them back ON despite the system-triggered shed event.

---

## 4.3 Historical Energy Consumption Visualization

To identify long-term consumption trends, the dashboard incorporates a dynamic Recharts-based area chart displaying energy logs retrieved from Firebase.

![Figure 4.3: Daily Energy Consumption Graph over a 31-Day Period](file:///c:/Users/7thHokage/Desktop/IoT%20webpage/daily_consumption_graph.png)
*(Note: Refer to the area chart visualization showing daily consumption peaks and hover tooltips).*

### Discussion of Figure 4.3:
1. **Interactive Monthly View**: Figure 4.3 illustrates the **Daily** consumption view, plotting energy consumption (in kWh) on the Y-axis against the days of the month (Days 1–31) on the X-axis. 
2. **Analysis of Consumption Spike**: A major consumption event is visible on **Day 17**, peaking at approximately **0.05 kWh**. This indicates a high-load appliance was active on Socket 3 or Socket 4 during that period. 
3. **Data Integrity & Tooltips**: The hover tooltip feature is active, showing the precise readings for individual days. In the figure, hovering over **Day 23** shows `kwh : 0`, demonstrating that the hub recorded zero energy leakage or active loads on that day. The database utilizes a "Snap-to-Zero" noise clamp, ensuring that offline socket currents do not bleed into the database as "ghost" energy recordings.

---

## 4.4 Historical Data Query & Audit Exporter

To facilitate auditing and external billing validation, the React dashboard provides a dedicated **Historical Reports** panel. This module allows targeted date-range queries and extracts raw database records into a portable format.

![Figure 4.4: Historical Reports Modal with CSV Export Feature](file:///c:/Users/7thHokage/Desktop/IoT%20webpage/historical_reports_modal.png)
*(Note: Refer to the historical statement generator modal with custom date inputs).*

### Discussion of Figure 4.4:
1. **CSV Statement Generation**: The modal provides a tabbed selector for "Specific Hour Lookup", "Specific Day Lookup", and "Download Statement". By inputting a date range (in this case, from **07/17/2026** to **07/18/2026**), the dashboard queries the Firebase Realtime Database path (`energy_logs/YYYY/MM/DD/`).
2. **Elimination of Buffer Exceptions**: To support high-volume audits, the CSV exporter is implemented using client-side `Blob` serialization and `URL.createObjectURL`. This architecture bypasses browser string size limitations and eliminates URI-encoding exceptions, generating a structured spreadsheet showing the exact hour-by-hour watt-hour consumption of the main line and all four individual sockets.

---

## 4.5 Digital Signal Processing & Physical Sensor Accuracy

To achieve reliable readings with low-cost analog current (ACS712) and voltage (ZMPT101B) sensors on the ESP32, several advanced digital signal processing (DSP) algorithms were evaluated.

### 1. True AC RMS via Variance Sampling
Rather than taking instantaneous ADC readings which fluctuate wildly over AC sinusoidal waves, the ESP32 performs block-sampling. For each measurement, the microcontroller samples all 6 analog channels continuously for **three full 20ms cycles** (50Hz grid frequency). The RMS amplitude is calculated using the statistical variance of the samples:
$$X_{\text{RMS}} = \sqrt{\frac{1}{N}\sum_{i=1}^{N} (x_i - \bar{x})^2}$$
This filters out the DC bias (~1.65V offset) of the ESP32's 3.3V analog pins without requiring external hardware filter circuits.

### 2. Minimum-Consistency Gate & Quadratic Noise Subtraction
To resolve the common ACS712 issue of "sensor drift" and "ghost currents" (where random electrical noise registers as small loads), the system uses a dual-layer filter:
- **Min-Consistency Gate**: For each sensor channel, the RMS value is computed individually for each of the three 20ms cycles. A current reading is only processed if the **minimum** RMS value across all three cycles exceeds the calibrated noise floor. This rejects transient electromagnetic spikes.
- **Quadratic Noise Subtraction**: Measured current contains both signal and sensor noise. Rather than applying a hard threshold which clamps small loads to zero, the system performs quadratic subtraction:
  $$I_{\text{clean}} = \sqrt{\max\left(0, I_{\text{measured}}^2 - I_{\text{noise}}^2\right)}$$
  Using calibrated base noise values (Main=4.0, S1/S2=5.5, S3/S4=6.0 ADC counts), this DSP approach removes idle noise completely (forcing idle sockets to exactly **0.00 A**) while preserving sensitivity for low-power appliances (such as a 20W bulb drawing ~0.09A).

### 3. Voltage Safety Cutoff
During testing, grid stability was simulated. Under normal conditions, the voltage reads between **219V and 222V** (using a tuned calibration factor of `CAL_VOLTAGE = 0.146` to align with a physical multimeter). If a brownout (voltage drops below **100V**) or a surge (voltage exceeds **240V**) is detected, the firmware triggers a safety shutdown, de-energizing all 4 relay channels. Power is restored only after the voltage stabilizes within the safe range for **10 consecutive seconds**, protecting connected appliances.
