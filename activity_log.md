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
