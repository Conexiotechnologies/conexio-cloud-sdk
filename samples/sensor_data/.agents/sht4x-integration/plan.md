# Implementation Plan — SHT4x Real Sensor Integration into `sensor_data` Sample

## Findings from code exploration

| File | State |
|---|---|
| `samples/sensor_data/src/main.c` | Already includes `<zephyr/drivers/sensor.h>` and `<math.h>`. Has `CONFIG_CONEXIO_SAMPLE_SIMULATED_SENSORS` guard around the existing `read_temperature` / `read_humidity` callbacks. Non-simulated stubs return `NAN`. Registers two metrics: `"temperature"` and `"humidity"`. |
| `samples/sensor_data/prj.conf` | Has `CONFIG_CONEXIO_SAMPLE_SIMULATED_SENSORS=y`. No I2C / sensor / SHT4x entries. |
| `samples/sensor_data/boards/conexio_stratus_pro_nrf9151_ns.overlay` | **Already exists** with both partition `#include` lines. Does NOT have the `&i2c2` SHT4x node yet. |
| `samples/sensor_data/CMakeLists.txt` | Lists `src/main.c` only. No changes needed — sensor drivers are Kconfig/DTS-driven, not CMake source additions. |
| `samples/sensor_data/Kconfig.sysbuild` | No changes needed. |
| `zephyr/drivers/sensor/sensirion/sht4x/Kconfig` | `CONFIG_SHT4X` has `default y` when `DT_HAS_SENSIRION_SHT4X_ENABLED`, `select I2C`, `select CRC`. The driver auto-enables once the DT node is status-okay. Explicit entries in `prj.conf` are still added for documentation clarity. |

Key Zephyr sensor API note: `sensor_sample_fetch()` triggers a measurement and stores the result in the driver's internal buffer. `sensor_channel_get()` reads from that buffer without triggering another measurement. This means we can fetch once in `read_temperature_real()` and call `sensor_channel_get()` for humidity in `read_humidity_real()` — one I2C transaction per publish cycle. Registration order matters: `temperature_real` must be registered before `humidity_real` so the fetch always precedes the humidity channel read.

---

## Implementation Plan

- [ ] 1. Update the board overlay to add the SHT4x I2C node.

      The existing overlay at
      `samples/sensor_data/boards/conexio_stratus_pro_nrf9151_ns.overlay`
      already contains both partition `#include` lines. Append the `&i2c2` block
      after those includes, following the reference overlay exactly.

      **Final file content:**
      ```c
      /*
       * Copyright (c) 2024 Conexio Technologies, Inc
       *
       * SPDX-License-Identifier: Apache-2.0
       */

      #include <samples/cellular/nrf91_bootloader_partitions.dtsi>
      #include <samples/cellular/nrf91_sram_partitions.dtsi>

      &i2c2 {
          status = "okay";
          clock-frequency = <I2C_BITRATE_FAST>;
          sht4x@44 {
              status = "okay";
              compatible = "sensirion,sht4x";
              reg = <0x44>;
              repeatability = <2>;
          };
      };
      ```

      Files: `samples/sensor_data/boards/conexio_stratus_pro_nrf9151_ns.overlay`

      Verify: Build completes with no `devicetree error` or `i2c2 not found` messages.

---

- [ ] 2. Add SHT4x Kconfig entries to `prj.conf`.

      Append a new `# Real SHT4x sensor` section to the end of the existing
      `prj.conf`. Keep all existing lines untouched.

      ```
      # Real SHT4x sensor (QWIIC port)
      # CONFIG_SHT4X is auto-selected when the DT node is status="okay";
      # explicit entries here document the intent.
      CONFIG_SENSOR=y
      CONFIG_I2C=y
      CONFIG_SHT4X=y
      ```

      Files: `samples/sensor_data/prj.conf`

      Verify: `west build` with the target board produces no Kconfig warnings about
      SHT4x, I2C, or SENSOR being undefined or unmet.

---

- [ ] 3. Extend `main.c` with the SHT4x device pointer, real-sensor callbacks,
         and metric registrations.

      All additions go inside a `#if DT_HAS_COMPAT_STATUS_OKAY(sensirion_sht4x)`
      guard so the file builds cleanly on any board that lacks the DT node.
      The `#else` branch provides NAN-returning stubs identical in signature.

      **3a — Add static device pointer** (after the existing `g_alert_threshold`
      declaration, before the existing `/* ── Sensor callbacks ──` comment):

      ```c
      /* ── Real SHT4x hardware device ─────────────────────────────────────────
       * Initialised in main(); NULL on boards without the DT node.
       * Both real-sensor callbacks guard against NULL / not-ready before use.
       */
      #if DT_HAS_COMPAT_STATUS_OKAY(sensirion_sht4x)
      static const struct device *s_sht4x;
      #endif
      ```

      **3b — Add real-sensor callbacks** (insert as a new block immediately after
      the closing `#endif` of the existing `CONFIG_CONEXIO_SAMPLE_SIMULATED_SENSORS`
      block, before `on_alert_threshold`):

      ```c
      /* ── Real SHT4x sensor callbacks ────────────────────────────────────────
       * read_temperature_real: fetches a fresh sample from the SHT4x, returns
       *   temperature in °C as a double.
       * read_humidity_real: reads the humidity channel from the same buffered
       *   sample WITHOUT triggering a second I2C transaction (Zephyr sensor API
       *   stores the last fetch result until the next call to sensor_sample_fetch).
       *
       * IMPORTANT: register temperature_real BEFORE humidity_real so that the
       *   SDK always calls the fetch before the channel-only read in the same
       *   publish cycle.
       *
       * Returns NAN when the sensor is absent, not ready, or fetch fails —
       * the SDK skips NAN values silently so the sample works on boards
       * without the sensor attached.
       */
      #if DT_HAS_COMPAT_STATUS_OKAY(sensirion_sht4x)

      static double read_temperature_real(void *arg)
      {
          ARG_UNUSED(arg);
          if (s_sht4x == NULL || !device_is_ready(s_sht4x)) {
              return (double)NAN;
          }
          struct sensor_value val;
          if (sensor_sample_fetch(s_sht4x) < 0) {
              LOG_WRN("SHT4x: sample fetch failed");
              return (double)NAN;
          }
          sensor_channel_get(s_sht4x, SENSOR_CHAN_AMBIENT_TEMP, &val);
          return sensor_value_to_double(&val);
      }

      static double read_humidity_real(void *arg)
      {
          ARG_UNUSED(arg);
          if (s_sht4x == NULL || !device_is_ready(s_sht4x)) {
              return (double)NAN;
          }
          /* No sensor_sample_fetch here — read_temperature_real (registered and
           * called first) already fetched the sample this cycle.
           * sensor_channel_get reads from the driver's internal buffer. */
          struct sensor_value val;
          sensor_channel_get(s_sht4x, SENSOR_CHAN_HUMIDITY, &val);
          return sensor_value_to_double(&val);
      }

      #else  /* !DT_HAS_COMPAT_STATUS_OKAY(sensirion_sht4x) */

      static double read_temperature_real(void *arg) { ARG_UNUSED(arg); return (double)NAN; }
      static double read_humidity_real(void *arg)    { ARG_UNUSED(arg); return (double)NAN; }

      #endif /* DT_HAS_COMPAT_STATUS_OKAY(sensirion_sht4x) */
      ```

      **3c — Initialise the device pointer in `main()`** (add immediately after
      the opening `LOG_INF("=== Conexio Sensor Data Sample ===");` log line,
      before the `conexio_cloud_register_sensor` calls):

      ```c
      /* Initialise real SHT4x sensor — guarded; no-op on boards without the node */
      #if DT_HAS_COMPAT_STATUS_OKAY(sensirion_sht4x)
          s_sht4x = DEVICE_DT_GET_ANY(sensirion_sht4x);
          if (!device_is_ready(s_sht4x)) {
              LOG_WRN("SHT4x not ready — real sensor metrics will be skipped");
              s_sht4x = NULL;
          } else {
              LOG_INF("SHT4x ready — real sensor metrics enabled");
          }
      #endif
      ```

      **3d — Register the new metrics in `main()`** (add immediately after the
      existing two `conexio_cloud_register_sensor` calls for `"temperature"` and
      `"humidity"`, so that `temperature_real` is always registered before
      `humidity_real`):

      ```c
          /* Real SHT4x metrics — registered after simulated; NAN returned when absent */
          conexio_cloud_register_sensor("temperature_real", read_temperature_real, NULL);
          conexio_cloud_register_sensor("humidity_real",    read_humidity_real,    NULL);
      ```

      **3e — Update the startup log** to reflect that real metrics are now also active:

      Replace the existing `LOG_INF("Interval: ...")` call with:
      ```c
      LOG_INF("Interval: %ds | Simulated: %s | SHT4x: %s",
              CONFIG_CONEXIO_CLOUD_INTERVAL_SEC,
              IS_ENABLED(CONFIG_CONEXIO_SAMPLE_SIMULATED_SENSORS) ? "yes" : "no",
              IS_ENABLED(CONFIG_SHT4X) ? "enabled" : "disabled");
      ```

      Files: `samples/sensor_data/src/main.c`

      Verify: Build completes without errors. Serial log shows both the simulated
      and real metric names in the SDK publish log when connected.

---

- [ ] 4. Confirm CMakeLists.txt and Kconfig.sysbuild need no changes.

      `CMakeLists.txt` references only `src/main.c`; sensor drivers are enabled
      via Kconfig and DTS — no CMake source additions are required.
      `Kconfig.sysbuild` has no partition-manager or sysbuild settings that
      interact with I2C or sensor drivers.

      Files: none (read-only confirmation)

      Verify: `west build -b conexio_stratus_pro/nrf9151/ns --pristine` succeeds
      with 0 errors and 0 warnings about missing symbols or undefined nodes.

---

## Build command

```bash
cd /opt/nordic/ncs/v3.4.1/modules/lib/conexio-cloud-sdk/samples/sensor_data
west build -b conexio_stratus_pro/nrf9151/ns --pristine
```

## Expected publish output (serial console, once connected)

```
[INF] app: === Conexio Sensor Data Sample ===
[INF] app: Interval: 30s | Simulated: yes | SHT4x: enabled
[INF] app: SHT4x ready — real sensor metrics enabled
...
[INF] conexio_cloud: Publishing: temperature=<sim_val>, humidity=<sim_val>,
                                 temperature_real=<real_val>, humidity_real=<real_val>
```

If the SHT4x is wired but the I2C address is wrong, the callbacks return NAN
and the SDK omits those keys from the payload — the simulated metrics continue
publishing normally.

## What is NOT changed

- Simulated `read_temperature()` / `read_humidity()` and their `CONFIG_CONEXIO_SAMPLE_SIMULATED_SENSORS` guard — untouched.
- `conexio_cloud_register_sensor("temperature", ...)` and `"humidity"` registrations — untouched.
- `CMakeLists.txt`, `Kconfig.sysbuild` — untouched.
- All existing `prj.conf` entries — preserved; new entries only appended.
- Overlay partition `#include` lines — preserved; `&i2c2` block appended after.
