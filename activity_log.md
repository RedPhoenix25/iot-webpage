# IoT Webpage - Activity Log

## Initialization & Setup
- **Dev Server**: Started the Vite development server for the `dashboard` folder.
- **Dependency Fixes**: Updated `lucide-react` to the latest version to resolve missing icon import errors.
- **Git Integration**: Created a root `.gitignore` to exclude `node_modules` and `.pio` build folders, initialized a local Git repository, and successfully pushed the project to GitHub (`RedPhoenix25/iot-webpage`).

## ESP32 Configuration & Flashing
- **PlatformIO Setup**: Configured `platformio.ini` to automatically target `COM7` for both uploading and the serial monitor.
- **Wi-Fi Credentials**: Injected the user's mobile hotspot credentials (`DESKTOP-JUC9IKT 5571`) into `src/main.cpp`.
- **Compilation Fixes**: 
  - Fixed an error where `dayOfTheYear()` was missing from the `RTClib` version by using `unixtime() / 86400` for day rollovers.
  - Fixed a deprecated `ArduinoJson` v7 warning by switching from `containsKey()` to direct property evaluation.
  - Fixed an ambiguous C++ overload error by explicitly casting `DateTime(0)` to `DateTime((uint32_t)0)`.
- **I2C Bug Fix**: The DS3231 RTC module being disconnected caused an infinite `i2cWriteReadNonStop` error loop that flooded the serial monitor and blocked the Wi-Fi connected message. Added a boolean flag `rtcFound` to completely bypass polling the RTC if it fails to initialize.
- **Flashing**: Erased the ESP32 flash to fix a corrupted NVS (`nvs_open failed: NOT_FOUND`) and successfully uploaded the stable firmware over `COM7`.

## Dashboard Integration & Refinement
- **IP Address Targeting**: Updated `dashboard/src/App.jsx` to dynamically target the ESP32's assigned IP address on the local hotspot (`ws://192.168.137.240:81`).
- **Offline States Implementation**: 
  - Modified `src/main.cpp` to send `null` instead of `0.0` when a sensor read fails (e.g., `isnan` for DHT22).
  - Modified `dashboard/src/App.jsx` default states and JSX rendering to gracefully detect `null` values and display a sleek "Offline" badge instead of false readings.
  - **Manual Sensor Override**: Hardcoded the analog/digital sensors (PIR, LDR, Voltage) to `null` in `src/main.cpp` because disconnected GPIO pins act as antennas and "float" (pick up random electrical noise), making software-only auto-detection impossible. They will display as "Offline" until physical sensors are connected.
- **Continuous Deployment**: Pushed the UI and logic updates to GitHub so the Vercel deployment automatically rebuilds.

## Dynamic Networking & Captive Portal
- **WiFiManager**: Replaced hardcoded Wi-Fi credentials in `src/main.cpp` with `tzapu/WiFiManager`. The ESP32 now automatically spins up an Access Point ("IoT-Hub-AP") with a captive portal if it cannot connect to a known network, allowing users to enter new Wi-Fi credentials dynamically.
  - **Custom Retry Logic**: Programmed the ESP32 to only launch the Captive Portal after explicitly attempting to connect to a known network 5 times, with exactly a 5-second interval between each attempt.
- **mDNS Support**: Added `ESPmDNS.h` to broadcast the ESP32 as `iot-hub.local`.
- **Dashboard Network Settings**: Updated `App.jsx` to default to `ws://iot-hub.local:81` and added a clickable connection status that reveals a fallback Settings modal. Users can now manually enter an IP address which is persisted in `localStorage`.
   
## True IoT (Global MQTT Migration)
- **Architecture Shift**: Completely removed Local WebSockets in favor of a Global Cloud Broker architecture using MQTT. This bypasses local network restrictions (like smartphone AP Isolation).
- **ESP32 Firmware**: Added PubSubClient to securely publish sensor data and subscribe to commands via the free broker.hivemq.com public broker.
- **React Dashboard**: Removed local IP settings and WebSocket logic. Installed the mqtt npm package to connect the dashboard securely to the cloud broker and stream data instantly.

## Hardware Pin Mapping
- **Analog Constraint Resolved**: Assigned 6 available ADC1 pins to the energy sensors (5 Current, 1 Voltage). Shifted the LDR to use its Digital Out pin on GPIO 15 to preserve ADC pins.
- **LCD Display**: Added LiquidCrystal_I2C and initialized it on standard I2C pins (SDA: 21, SCL: 22) shared with the RTC.
- **Relays**: Assigned 4-channel relays to safe GPIOs (26, 27, 14, 13) to avoid bootstrapping issues.

## UI Design Overhaul
- **Midnight Glassmorphism**: Swapped generic dark theme for a premium #090E17 background with frosted glass cards and neon accents (Cyan/Emerald).
- **Mobile-First Layout**: Reordered the hierarchy so Total Power Load is a Hero Card at the top. Placed environment stats in a tight 2x2 grid.
- **Typography & Micro-animations**: Introduced 'Outfit' font for sharp numerical displays. Added spring animations to toggles and scale effects on card clicks.

## True Online Status (LWT)
- **MQTT Last Will & Testament**: Configured the ESP32 to register a "Last Will" with HiveMQ. If the ESP loses power/WiFi, the broker automatically publishes 'offline' to the status topic.
- **Dashboard UI Update**: Updated the React dashboard to subscribe to the status topic, accurately reflecting the ESP32's physical state rather than just the browser's connection.

## Security & Authentication
- **Login Screen**: Added a full-screen, glassmorphism login gateway to the React dashboard.
- **Environment Variable Protection**: Secured the dashboard with a master password (`VITE_APP_PASSWORD`). Session persistence is intentionally disabled per user request, requiring the password on every fresh load to prevent unauthorized physical access.

## Historical Data & Cloud Database
- **Firebase Migration**: Replaced the ESP32's internal flash memory logging with a direct HTTP connection to Firebase Realtime Database. The ESP32 now patches an accumulated "Watt-hour" tally for the current hour directly to the cloud every 60 seconds.
- **Analog Meter**: Removed the static Daily/Weekly text cards in favor of a sleek SVG `AnalogGauge` component to display live wattage instantly.
- **Multi-View Interactive Graph**: Integrated `recharts` to build a `HistoricalGraph` component. It dynamically queries the Firebase JSON tree and plots Hourly, Daily, Weekly, and Monthly aggregations.
- **Historical Lookup Module**: Built a targeted querying tool allowing users to select a specific Date and Hour from a calendar to check exact power usage in the past.
- **UI Bug Fix**: Wrapped the `HistoryLookup` modal in a React portal (`createPortal`) to attach it directly to the document body. This fixed an issue where the modal was trapped inside a new stacking context created by a parent element's `backdrop-filter`, making the calendar picker and close buttons unclickable.
- **CSV Data Generator Bug Fix**: Patched a fatal error during CSV report generation caused by Firebase inserting `null` array indices for missing hourly data. Rewrote the CSV download implementation using `Blob` and `URL.createObjectURL` to remove string length limits and prevent `encodeURI` exceptions on large datasets.
- **Themed Notifications**: Replaced browser-native `alert()` dialogs in the data fetching modules with a custom glassmorphism `Toast` notification system that renders at the root level via portals, providing a seamless premium UX.
- **Query Hardening**: Wrapped the Firebase `get()` calls in comprehensive `try/catch` blocks and enforced strict base-10 radix on all date parsing functions (`parseInt(val, 10)`) to resolve silent failures and prevent octal interpretation issues in older browsers during specific hour lookups.

### Hardware Configuration
- **Wi-Fi Resilience Logic**: Refactored the `setup()` Wi-Fi configuration sequence in `src/main.cpp`. Wrapped the `WiFi.begin()` and `WiFiManager` captive portal in an infinite retry loop. Implemented a strict 3-minute timeout (`setConfigPortalTimeout(180)`) so the ESP32 will automatically tear down the config portal and retry connecting to a known hotspot if the router temporarily vanishes, preventing the device from getting permanently stuck in AP mode.
- **Hardware UI Overhaul (LCD)**: Refactored the `sendUpdate()` loop to maximize the 16x2 LCD screen space. Row 1 now displays real-time `[Voltage]V [Current]A [Wattage]W`. Row 2 displays the real-time clock from the RTC module (`Time: HH:MM:SS`) statically, with the IP address broadcast entirely removed to save space and improve privacy. 
- **Sensor Telemetry**: Updated the primary power calculation logic to bypass mocked values and read directly from the `ACS712_MAIN` and `ZMPT101B_PIN` analog pins. When sensors are missing, the LCD and web UI will correctly display the random floating noise generated by the ungrounded pins, allowing for bare-board testing.
- **LCD Flickering Bug Fix**: Resolved a visual bug where the LCD flickered wildly by removing duplicate layout updates from the MQTT `sendUpdate` function and isolating all UI refresh logic into a single non-blocking `loop()` timer.
- **RTC (DS1307 / DS3231) Compatibility**: Rewrote the I2C boot sequence so the RTC chip boots before the LCD to prevent bus monopolization. Refactored the `RTClib` initialization to dynamically probe for both `DS3231` and `DS1307` variants (often labeled "AT24C32") and automatically bind to whichever one is detected.
- **NTP Time Syncing**: Added `configTime` logic to periodically sync the ESP32's internal clock via SNTP over the hotspot. The LCD now gracefully displays the SNTP time when available, falls back to the RTC module if offline, and accurately flashes the real-time back to the physical RTC chip in the background.
- **Non-Blocking Wi-Fi & Scrolling UI**: Re-engineered the `WiFiManager` to run in non-blocking mode. The ESP32 no longer freezes on "Starting..." when disconnected from the internet. Instead, it continuously streams the live electrical measurements while scrolling `"   No Internet Connection   "` across the bottom row. When connected, the bottom row scrolls `"Time: HH:MM:SS   Status: Online   "` smoothly at 350ms per frame.
- **MQTT Telemetry Fixes**: Increased the internal `PubSubClient` buffer size to `1024` bytes to prevent the MQTT broker from quietly dropping large JSON payloads (which previously caused the dashboard to report "Hub Offline"). Also removed the temporary `nullptr` override on the motion and light sensors now that they are physically wired up.
- **True RMS Sampling**: Rewrote the voltage and current sensor logic to perform a proper 20ms block sample (one full 50Hz cycle). The code now correctly calculates the True RMS values for the connected ZMPT101B and ACS712 sensors instead of relying on a single instantaneous ADC read (which resulted in wildly jumping noise).
- **Socket Sensor Sampling**: Replaced mocked current values for the 4 individual sockets with true analog reads from the 4 connected ACS712 sensors (`ACS712_PIN1` - `ACS712_PIN4`). Extracted calibration factors into individual constants to allow independent calibration for each socket, and integrated them into the existing 20ms RMS block sampling loop for accurate power measurement on all channels.
- **Voltage Calibration**: Updated the ZMPT101B voltage calibration factor from `0.396` to `0.437` to accurately align the ESP32 readings (previously ~199.7V) with the true multimeter readings (~220.5V).
- **Stable Current Sampling**: Replaced single 20ms cycle ADC sampling with a **3-cycle averaged** approach. Each 2s measurement window now takes 3 consecutive 20ms RMS samples and averages them, significantly reducing the chance of a single bad ADC read dragging readings to zero under stable load.
- **EMA Smoothing Improvement**: Reduced the Exponential Moving Average new-sample weight from `0.30` to `0.15` (85% old / 15% new), making readings far more resistant to single-cycle noise spikes.
- **Noise Floor Adjustment**: Lowered the current noise floor threshold from `0.15A` to `0.10A` to prevent false zero-clamping under lighter loads.
- **LCD ↔ Dashboard Sync**: Refactored the LCD top-row display to read directly from the `currentVoltage`, `currentAmperage`, and `currentPower` global variables (same source as the MQTT telemetry), guaranteeing perfect sync between the LCD and dashboard.
- **Smart Bulb Automation (Socket 1)**: Replaced the old temperature-based `checkThresholds()` function with a full state machine `runAutomation()`. Implements motion-triggered bulb control with LDR feedback loop prevention: motion ON → bulb ON; LDR detects light → bulb OFF + override; LDR goes dark → bulb back ON; no motion → bulb OFF.
- **Socket Naming**: Updated `socket1` label to `"Socket 1 (Bulb)"` and `socket2` to `"Socket 2 (Fan)"` across the firmware.
- **Temperature Rule**: Moved the >30°C fan trigger from Socket 1 to **Socket 2 (Fan)** to match the updated hardware layout.
- **No-Load Ghost Reading Fix**: Replaced the simple 3-cycle average for socket sensors with a **min-consistency gate**. Each cycle's RMS is stored individually; a socket only reports current if the *minimum* across all 3 cycles clears the noise floor. This prevents inductive/capacitive coupling from a relay-closed-but-unloaded socket from registering as a real load, while still accurately measuring true current draw when a load is connected.
- **Idle Power Fix (Main Line)**: Extended the min-consistency gate to the **main line ACS712** sensor as well. The 15W system adapter's draw (~0.068A) is below the ACS712-30A's reliable threshold, so all channels now correctly report 0W with all sockets off. Total Live Wattage now sources from the sum of active socket powers when any socket is on, falling back to the main line reading otherwise.
- **Energy Calculation Leak Fix**: Clamped the loop-level socket currents (`currentS1`–`S4`) to `0` whenever their respective relays are OFF. This completely prevents noise floor leakage from bleeding into `calculateEnergy()` and causing ghost energy logs on Firebase when sockets are inactive.
- **Dashboard Compliance Tuning**: Updated the live wattage gauge logic in the React app (`App.jsx`) to correctly sum active socket powers when any socket is active, falling back to the main line reading only when all sockets are turned OFF. Updated dashboard's initial state names to match hardware: `"Socket 1 (Bulb)"` and `"Socket 2 (Fan)"`.
- **RMS Accumulator Hardening**: Changed raw voltage and current sum-of-squares accumulator variables from 32-bit signed `long` to 64-bit float `double` in the 3-cycle True RMS calculation loop, successfully preventing potential integer overflows that lead to distorted standard deviation calculations.
- **Voltage Calibration**: Added a temporary rate-limited `CALIBRATION DEBUG` print to monitor the raw `zRmsAvg` standard deviation values over the serial port. Based on serial telemetry under active grid conditions, re-tuned the `CAL_VOLTAGE` multiplier from `0.437` to `0.146` to scale the raw ADC values down to match the physical multimeter grid average (219V - 222V), resolving the issue where the dashboard and LCD showed 500V to 650V.
- **Fan Temperature Hysteresis**: Re-engineered Rule 1 in `runAutomation()` for Socket 2 (Fan). The fan now turns ON automatically when the DHT22 temperature reading reaches 30°C or higher and remains active until the temperature cools down below 26°C, preventing the fan from rapidly cycling on/off around a single point.
- **Lowered Current Noise Floor**: Reduced the current measurement noise floor threshold from `0.10A` to `0.05A` for all channels, enabling the system to reliably measure and display lighter loads (such as a 20W / 0.09A load). Added a safety clamp to ignore the main current sensor and force total current to `0.00A` when all relays are OFF, preventing the system's own 15W adapter draw from leaking onto the dashboard.
- **Bulb Automation Corrected**: Restored the original state machine structure for Rule 2 to match the active-LOW, NC-wired relay layout. When ambient light goes to 0%, Socket 1 will turn ON (relay triggered, writing LOW to GPIO 26) to physically turn the bulb OFF.
- **DHT22 Read Resiliency**: Implemented a retry loop in `sendUpdate()` that attempts to read the DHT22 sensor up to 3 times with a 200ms delay between retries if a `NaN` reading occurs, resolving transient single-wire bus failures. Added diagnostic Serial prints to track DHT22 read attempts.
- **Individual Channel Noise Floors**: Fine-tuned the current thresholds per channel: Socket 1 & 2 = `0.10A` (fully clamps Socket 2's `0.08A` no-load noise to `0.00A` when ON); Socket 3 & 4 = `0.12A` (clamps their `0.11A` no-load noise to `0.00A` but detects a 20W load which reads $\sim 0.15\text{A}$ under load due to noise addition). Main current threshold remains at `0.08A`.
- **Bulb Automation Lockout & Timer**: Re-engineered Rule 2 to prevent the bulb's own light from feeding back into the LDR. The LDR is now only queried to *turn ON* the bulb when the bulb is OFF. Once ON, the LDR is ignored, and the bulb is kept ON steadily for a 15-second timeout.
- **PIR Level Automation & Auto-Clear Telemetry**: Decoupled automation from telemetry. Automation now uses the raw physical PIR pin state (`digitalRead(PIR_PIN) == HIGH`) to reset the stay-on timer continuously while a user is present, ensuring the bulb stays ON. Telemetry uses software edge-detection to flash "Detected" on the dashboard for 3 seconds and then auto-clear to "Clear" to keep the dashboard responsive. Manual dashboard toggles reset the timer.
- **Quadratic Noise Subtraction**: Replaced threshold-based noise floor clamping with proper DSP quadratic subtraction (`sqrt(signal² - noise²)`). Each ACS712 channel has a calibrated raw RMS noise offset (Main=4.0, S1/S2=4.5, S3/S4=6.0 ADC counts) that is subtracted in quadrature from the measured RMS, yielding a clean signal that reads exactly 0A with no load but accurately detects real loads on any socket. This eliminates ghost readings on all channels without sacrificing sensitivity.

## Dashboard Authentication Bug Fix
- **Fallback Password & Input Trimming**: Added a code-level fallback to `'iothub26'` when `import.meta.env.VITE_APP_PASSWORD` is undefined (such as on Vercel deployment builds where `.env` is excluded). Also added `.trim()` to the user password input to prevent spacing issues caused by auto-correct or keyboard suggestions.

## UI Bug Fixes
- **Number Input Styling**: Fixed an issue where the Electricity Rate placeholder (`225`) wasn't inheriting the correct font size and weight by explicitly adding `::placeholder` styling to `.history-input`. Also removed the default WebKit spin buttons (up/down arrows) from number inputs to prevent them from creating an awkward dark box inside the input field and breaking the layout.
- **Bleeding Input Fix**: Added `minWidth: 0` and `width: '100%'` inline styles to the electricity rate input inside its flex container in `EnergyStats.jsx` to prevent it from bleeding out of the modal boundaries.