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