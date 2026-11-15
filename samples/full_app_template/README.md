# Conexio Advanced Sample App

Production-grade firmware built on
`conexio-cloud-sdk` with nRF Connect SDK v3.4.1.

The SDK handles all infrastructure — PSM, offline buffering, FOTA, retry/watchdog,
NTP sync, certificate management, MQTT command delivery, and battery metrics.
The application provides sensor callbacks, command handlers, settings handlers,
and the cellular location module.

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [Application Structure](#application-structure)
3. [Payload Encoding — CBOR vs JSON](#payload-encoding--cbor-vs-json)
4. [Telemetry Data Packet](#telemetry-data-packet)
5. [Cellular Location Feature](#cellular-location-feature)
6. [HERE Positioning API — Pricing](#here-positioning-api--pricing)
7. [Firmware Configuration Reference](#firmware-configuration-reference)
8. [Build Variants](#build-variants)
9. [Commands and Settings](#commands-and-settings)
10. [Battery Metrics (nPM1300)](#battery-metrics-npm1300)
11. [FOTA — Firmware Over-the-Air](#fota--firmware-over-the-air)
12. [Power Management](#power-management)
13. [Offline Buffering](#offline-buffering)

---

## Quick Start

```bash
# Build for Conexio Stratus Pro
west build -b conexio_stratus_pro/nrf9151/ns -- \
  -DEXTRA_CONF_FILE=low_power.conf   # optional: disable UART for field builds

# Flash
newtmgr -c serial image upload build/app/zephyr/zephyr.signed.bin

# Monitor serial output (development)
screen /dev/tty.usbmodem* 115200
```

---

## Application Structure

```
src/
  main.c            — SDK init, sensor/command/settings registration, main loop
prj.conf            — All Kconfig options with inline documentation
low_power.conf      — Production overlay: disables UART/logging
debug.conf          — Development overlay: DBG-level logging + assertions
VERSION             — App firmware version (published as _app_fw_version)
```

The application uses a single SDK include:

```c
#include <conexio_cloud/conexio_cloud.h>
```

Everything else — MQTT, TLS, modem, NTP, PSM, FOTA, retry — is auto-selected
by the SDK Kconfig.

---

## Payload Encoding — CBOR vs JSON

The SDK supports two payload encodings, selectable via a single Kconfig option.
The **application code is 100% encoding-agnostic** — no changes to sensor callbacks,
metric names, or any `main.c` code are needed to switch between them.

### CBOR (default, recommended)

```kconfig
CONFIG_CONEXIO_CLOUD_CBOR=y   # in prj.conf
```

CBOR (Concise Binary Object Representation, RFC 8949) encodes the same telemetry
data as compact binary instead of JSON text.

**Why CBOR?**

| | JSON | CBOR |
|---|---|---|
| Typical payload size | ~150–200 bytes | ~60–100 bytes |
| Field names | Repeated in full as strings | Embedded as compact text strings |
| Numbers | ASCII text (e.g. `29.6` = 4 bytes) | Typed binary (float16 = 2 bytes) |
| Cloud decoding | Native | Auto-detected and decoded by Conexio Cloud |
| App code changes | None | None |
| Firmware deps | cJSON (already in SDK) | zcbor (already in NCS) |

For a device publishing every 60 seconds, switching from JSON to CBOR saves
approximately **50–70 KB/day per device** in SIM data. At scale this is significant.

### JSON (backwards-compatible fallback)

```kconfig
CONFIG_CONEXIO_CLOUD_CBOR=n   # in prj.conf
```

Set to `=n` to revert to JSON text encoding. Useful for:
- Debugging with a MQTT client that shows human-readable JSON
- Comparing payload sizes during development

---

## Firmware Configuration Reference

Key options in `prj.conf`:

```kconfig
# Core SDK
CONFIG_CONEXIO_CLOUD=y
CONFIG_CONEXIO_CLOUD_INTERVAL_SEC=30      # Telemetry interval (s)

# CBOR binary encoding (50-70% smaller payloads vs JSON)
CONFIG_CONEXIO_CLOUD_CBOR=y              # Enable CBOR (default, recommended)
CONFIG_CONEXIO_CLOUD_CBOR_BUFFER_SIZE=512 # Static encode buffer (bytes)
# Set CONFIG_CONEXIO_CLOUD_CBOR=n to revert to JSON text encoding

# PSM — keep modem in µA sleep between publishes
CONFIG_CONEXIO_CLOUD_PSM=y
CONFIG_CONEXIO_CLOUD_PSM_TAU_SEC=7200     # Network keepalive every 2h
CONFIG_CONEXIO_CLOUD_PSM_ACTIVE_TIME_SEC=30

# Offline buffer — 100 samples × 60s = ~100 min coverage
CONFIG_CONEXIO_CLOUD_OFFLINE_BUFFER=y
CONFIG_CONEXIO_CLOUD_OFFLINE_BUFFER_SIZE=100

# FOTA
CONFIG_CONEXIO_CLOUD_FOTA=y

# Retry with exponential backoff (5s → 300s, reboot after 10 failures)
CONFIG_CONEXIO_CLOUD_RETRY=y
CONFIG_CONEXIO_CLOUD_RETRY_BASE_SEC=5
CONFIG_CONEXIO_CLOUD_RETRY_MAX_SEC=300
CONFIG_CONEXIO_CLOUD_RETRY_MAX_ATTEMPTS=10

# Battery (nPM1300 fuel gauge)
CONFIG_CONEXIO_CLOUD_AUTO_BATTERY=n      # Disable modem ADC path
CONFIG_CONEXIO_CLOUD_BATTERY_METRICS=y   # Enable SOC + drain rate metrics

# Cellular location
CONFIG_CELL_LOCATION=y                   # Enable AT%NCELLMEAS module
```

---

## Build Variants

### Development (default)

Uses `prj.conf` only. UART console enabled, log level INF. CBOR encoding enabled.

```bash
west build -b conexio_stratus_pro/nrf9151/ns
```

### Ingest key quick-connect (no fleet provisioning)

Connect a pre-registered device without running the provisioning firmware.
Create a separate overlay file (never commit a real key to git):

```kconfig
# prj_ingest_key.conf — keep this file out of version control
CONFIG_CONEXIO_CLOUD_INGEST_KEY="wks_64c8a418_..."        # ← key from Ingest Keys page
```

```bash
west build -b conexio_stratus_pro/nrf9151/ns --pristine -- \
  -DEXTRA_CONF_FILE=prj_ingest_key.conf
```

> The device IMEI must be pre-registered in Conexio Console → All Devices.

### Debug

Adds DBG-level logging and assertions.

```bash
west build -b conexio_stratus_pro/nrf9151/ns -- \
  -DEXTRA_CONF_FILE=debug.conf
```

### Production / Low-Power

Disables UART console and logging. Reduces current draw. CBOR encoding enabled.

```bash
west build -b conexio_stratus_pro/nrf9151/ns -- \
  -DEXTRA_CONF_FILE=low_power.conf
```

> **Simulated sensors warning:** `CONFIG_CONEXIO_SAMPLE_SIMULATED_SENSORS=y`
> is on by default. The build emits a `#warning` as a reminder. Set it to `n`
> and implement real sensor reads before any production deployment.

---

## Commands and Settings

### Built-in SDK Commands (automatic)

| Command | Payload | Effect |
|---------|---------|--------|
| `REBOOT` | `{}` | Immediate device reboot |
| `SET_INTERVAL` | `{"intervalSec": 60}` | Change telemetry interval |
| `FIRMWARE_UPDATE` | `{"url": "..."}` | Trigger FOTA download |

### Application Commands

| Command | Payload | Effect |
|---------|---------|--------|
| `FAN_ON` | `{"speed": 80}` (optional) | Turn on fan / actuator |
| `FAN_OFF` | `{}` | Turn off fan / actuator |
| `LED_ON` | `{}` | Turn on board LED (gpio0 pin 25) |
| `LED_OFF` | `{}` | Turn off board LED |

`LED_ON` / `LED_OFF` are wired to the Device Schedules feature — use them
to test timed command delivery from the Conexio Console. The SDK stores the
`stopAt` time in NVS so `LED_OFF` fires even if the device loses connectivity
after receiving `LED_ON`.

### Settings (OTA Config)

| Key | Type | Range | Default | Effect |
|-----|------|-------|---------|--------|
| `telemetryIntervalSec` | int | 10–604800 | 30 | Publish interval (built-in) |
| `alertThreshold` | int | 0–200 | 80 | App-level alert trigger |
| `debugMode` | bool | — | false | App-level debug flag |

---

## FOTA — Firmware Over-the-Air

FOTA is enabled via `CONFIG_CONEXIO_CLOUD_FOTA=y`. The SDK handles:

1. Cloud delivers `FIRMWARE_UPDATE` command with a signed download URL
2. SDK downloads the binary over HTTPS using the nRF9151 modem
3. MCUboot applies the update on next reboot
4. `_app_fw_version` in the next telemetry confirms the new version

---

## Power Management

### PSM (Power Saving Mode)

The modem enters µA-level sleep between transmissions.

```
TAU timer  (T3412) = 3600s = 1h  → network keepalive interval (prj.conf default)
Active time (T3324) = 10s        → window to connect + publish + sleep
```

The actual negotiated values are granted by the network and published as
`_psm_tau_sec` and `_psm_active_sec` on boot. Carriers impose their own
minimums — AT&T LTE-M typically grants T3324 ≥ 30s and TAU ≥ 3.5h on Band 13
regardless of what the device requests.

### Measured PSM Sleep Current — Conexio Stratus Pro (nRF9151)

Measured with Nordic nRF PPK2 power profiler on production firmware built with
`low_power.conf`. Conditions: PSM sleep active, UART disabled, all peripherals idle.

| Component | Measured current |
|---|---|
| nRF9151 modem (PSM sleep) | ~2.5 µA |
| nPM1300 PMIC (quiescent) | ~0.8 µA |
| **Board total** | **~3.3 µA** |

The `CONFIG_CONEXIO_FUEL_GAUGE_PSM_SLEEP_CURRENT_UA=4` setting in `prj.conf` is
calibrated from this measurement. The nRF Fuel Gauge library uses this value to
integrate coulombs during PSM sleep without any CPU wake-ups, keeping `_batt_soc`
and `_batt_drain_hr` accurate across long sleep windows.

> Using 4 µA (vs the measured 3.3 µA) provides a conservative margin for
> unit-to-unit variation and temperature drift while remaining close to the real value.

### Typical Current Profile (3600s interval, PSM enabled)

| Phase | Typical duration | Typical current | Notes |
|---|---|---|---|
| LTE attach + MQTT connect | ~8–15 s | ~50–110 mA | Peaks at attach |
| Telemetry publish (MQTT) | ~1–3 s | ~50–100 mA | QoS 1 PUBACK wait |
| AT%NCELLMEAS (if enabled) | ~1–5 s | ~30–50 mA | Extended Light scan |
| T3324 active window | ~30–60 s | ~5–15 mA | Network-granted; RRC idle until PSM |
| **PSM sleep** | remainder of interval | **~3.3 µA** | Measured on Stratus Pro |

### Main Loop Sleep

In low-power builds (`low_power.conf`, `CONFIG_SERIAL=n`) the main loop sleeps for
the full `CONFIG_CONEXIO_CLOUD_INTERVAL_SEC` between wakes. In debug builds
(`CONFIG_SERIAL=y`) it sleeps in 5-second increments so `SET_INTERVAL` and
`telemetryIntervalSec` OTA Config changes take effect quickly.

### Estimated Battery Life

> Actual life depends on network attach time, carrier-granted PSM timers, and
> temperature. 
---

## Offline Buffering

When the device loses LTE connectivity, telemetry is stored in NVS flash:

- Buffer size: 100 entries (`CONFIG_CONEXIO_CLOUD_OFFLINE_BUFFER_SIZE`)
- On reconnect: replays in batches of 10 (`CONFIG_CONEXIO_CLOUD_OFFLINE_REPLAY_BATCH`)
- At 60s interval: ~100 minutes of offline coverage before oldest entries
  are overwritten
- Each buffered entry includes the original timestamp so the cloud can
  reconstruct the correct timeline

---

*Copyright (c) 2026 Conexio Technologies, Inc. — Apache-2.0 License*
