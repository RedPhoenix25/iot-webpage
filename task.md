# Advanced Historical Logging & Voltage Protection Tasks

- `[/]` Update ESP32 `main.cpp`:
  - `[ ]` Implement 5-channel energy tracking.
  - `[ ]` Implement voltage analytics tracking (min, max, avg).
  - `[ ]` Implement Voltage Safety Cutoff (180V-240V) with 10-second stability recovery.
  - `[ ]` Update Firebase REST API payload format.
- `[ ]` Update React Dashboard:
  - `[ ]` Update `HistoricalGraph` to parse `total_wh` instead of `wh`.
  - `[ ]` Build Glassmorphism Modal for Detailed Hour Lookup.
  - `[ ]` Add CSV Statement Download functionality (Bank Statement style).
