# Advanced Historical Logging & Voltage Protection Tasks

- `[x]` Update ESP32 `main.cpp`:
  - `[x]` Implement 5-channel energy tracking.
  - `[x]` Implement voltage analytics tracking (min, max, avg).
  - `[x]` Implement Voltage Safety Cutoff (180V-240V) with 10-second stability recovery.
  - `[x]` Update Firebase REST API payload format.
- `[x]` Update React Dashboard:
  - `[x]` Update `HistoricalGraph` to parse `total_wh` instead of `wh`.
  - `[x]` Build Glassmorphism Modal for Detailed Hour Lookup.
  - `[x]` Add CSV Statement Download functionality (Bank Statement style).
