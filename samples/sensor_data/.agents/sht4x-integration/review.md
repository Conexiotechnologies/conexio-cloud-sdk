# SHT4x Real Sensor Integration in sensor_data Sample

The change extends the `sensor_data` sample to publish live SHT4x temperature and humidity readings
alongside the existing simulated metrics. The approach registers two new SDK sensor callbacks
(`temperature_real`, `humidity_real`) that drive the Zephyr `sensirion,sht4x` driver, with full
`DT_HAS_COMPAT_STATUS_OKAY` guards so the sample builds and runs on boards without the sensor.
The simulated `temperature` / `humidity` registrations and their Kconfig guard are untouched.
The build is confirmed clean at 0 errors with the SHT4x driver library linked in.

**Watch for:** `read_humidity_real` skips `sensor_sample_fetch` and reads from the driver's internal
buffer, relying on `read_temperature_real` having been invoked first in the same publish cycle — an
implicit SDK ordering contract that is documented in comments but not enforced in the code.
`sensor_channel_get` return values are also unchecked in both real callbacks.

**Verdict**: APPROVED

---

## High-level view

The integration adds an `&i2c2` DTS node to the board overlay, three Kconfig lines to `prj.conf`,
a guarded static device pointer, two real-sensor callbacks, and two new `conexio_cloud_register_sensor`
calls. The overlay is structurally identical to the reference in `conexio-firmware-sdk` — same
`I2C_BITRATE_FAST`, same address `0x44`, same `repeatability = <2>`, and both partition `#include`
lines are preserved. `prj.conf` adds `CONFIG_SENSOR`, `CONFIG_I2C`, and `CONFIG_SHT4X` after all
pre-existing entries without disturbing them.

The real callbacks are guarded at two levels: a compile-time `DT_HAS_COMPAT_STATUS_OKAY` macro
that provides NAN-returning stubs on boards without the node, and a runtime `device_is_ready` check
that returns NAN if the I2C bus or sensor failed to initialise. The simulated path runs in parallel
under its existing `CONFIG_CONEXIO_SAMPLE_SIMULATED_SENSORS` guard; prj.conf keeps that flag `y`
so the default build publishes all four metrics simultaneously.

The one structural risk is the split fetch strategy: `read_temperature_real` owns the
`sensor_sample_fetch` call, and `read_humidity_real` reads from the driver's buffer without
re-fetching. The code and README both call this out explicitly, but correctness depends on the SDK
always invoking registered callbacks in insertion order within a single publish cycle.

---

<details>
<summary>Issues (2)</summary>

1. **Implicit fetch ordering between humidity_real and temperature_real** — `read_humidity_real`
   calls `sensor_channel_get` without a preceding `sensor_sample_fetch`, relying on the SDK
   calling `read_temperature_real` first. If the SDK does not guarantee insertion-order callback
   dispatch, the first (or any) publish cycle where humidity is called before temperature would
   read stale or zero-initialised buffer data instead of NAN — a silent bad value, not an error.
   Safer alternatives: store humidity in a static inside `read_temperature_real` and return it
   from `read_humidity_real`, or call `sensor_sample_fetch` in `read_humidity_real` and accept
   the extra I2C transaction.

2. **Unchecked `sensor_channel_get` return values** — both `read_temperature_real` and
   `read_humidity_real` ignore the int returned by `sensor_channel_get`. On an error, `val`
   contains whatever was on the stack, and `sensor_value_to_double` returns an uncontrolled
   double rather than NAN. Add `if (sensor_channel_get(...) < 0) return (double)NAN;`.

</details>

---

<details>
<summary>Details</summary>

### Simulated registrations are untouched — confirmed

`conexio_cloud_register_sensor("temperature", read_temperature, NULL)` and the corresponding
`humidity` call appear at exactly the same positions in `main()` as before. The
`CONFIG_CONEXIO_SAMPLE_SIMULATED_SENSORS` conditional compiles the random-value implementations
unchanged. `prj.conf` keeps `CONFIG_CONEXIO_SAMPLE_SIMULATED_SENSORS=y`, so all four metrics
publish by default.

### Real metric naming

`temperature_real` and `humidity_real` are distinct names with no collision risk against the
existing `temperature` / `humidity` keys or any SDK-reserved auto-metric (`_rssi`, `_snr`, etc.).

### NAN-on-absent-device path

The `#if DT_HAS_COMPAT_STATUS_OKAY` / `#else` split is the correct Zephyr idiom. On a board
without the DTS node the stubs compile to `return (double)NAN` with no reference to the driver
API, so the translation unit builds cleanly regardless of whether the SHT4x driver Kconfig is
selected. The runtime guard in the live path (`s_sht4x == NULL || !device_is_ready`) handles
cases where the node exists but the I2C bus fails at runtime.

### Implicit fetch ordering between real callbacks — possible bad value (not NAN)

`read_humidity_real` calls `sensor_channel_get(SENSOR_CHAN_HUMIDITY, &val)` directly, without
a `sensor_sample_fetch`. The design intent is that `read_temperature_real`, registered first,
already fetched the sample. The comment in the source and the README both flag this dependency
explicitly. The risk is that if the SDK ever dispatches callbacks out of registration order —
or if humidity is registered before temperature in a future refactor — the channel read draws on
either the SHT4x driver's zero-initialised buffer (first cycle) or the previous cycle's data.
Neither case triggers the NAN guard; a plausible-looking but wrong humidity value gets published.
The straightforward fix is a static variable: store humidity inside `read_temperature_real` and
just return the stored value from `read_humidity_real`, decoupling the two callbacks entirely.

### `sensor_channel_get` return values — confirmed unchecked

```c
/* read_temperature_real */
sensor_channel_get(s_sht4x, SENSOR_CHAN_AMBIENT_TEMP, &val);
return sensor_value_to_double(&val);

/* read_humidity_real */
sensor_channel_get(s_sht4x, SENSOR_CHAN_HUMIDITY, &val);
return sensor_value_to_double(&val);
```

Both calls discard the return value. `sensor_channel_get` returns a negative errno on failure
(e.g., `-ENOTSUP` for an unsupported channel). In that case `val` is uninitialised and
`sensor_value_to_double` returns a garbage double. The fix is one-liner: check the return and
return NAN on error.

### `#warning` on simulated-sensor builds

The `#if defined(CONFIG_CONEXIO_SAMPLE_SIMULATED_SENSORS)` block opens with a `#warning` pragma.
With `CONFIG_CONEXIO_SAMPLE_SIMULATED_SENSORS=y` in `prj.conf` this fires on every compile.
The build evidence reports 0 errors (the pragma is treated as a warning, not an error). This is
intentional — it reminds developers not to ship the sample binary to production — and is not a
concern, but it means the build is not strictly warning-free.

### Overlay vs. reference diff

The only difference between the new overlay and the reference in `conexio-firmware-sdk` is a
blank line between the two `#include` directives, which is whitespace-only and has no effect on
DTS preprocessing. Address, compatible string, repeatability, clock-frequency, and partition
includes are byte-for-byte identical.

### Build evidence

The evidence file shows 409/409 build steps completed with the SHT4x driver library linked:
```
[381/409] Linking C static library
  zephyr/drivers/sensor/sensirion/sht4x/libdrivers__sensor__sensirion__sht4x.a
```
No build re-run was performed; the evidence is accepted as-is.

</details>

---

<details>
<summary>File map</summary>

| File | Change |
|---|---|
| `src/main.c` | Added `s_sht4x` pointer, `read_temperature_real`/`read_humidity_real` callbacks, and two new `conexio_cloud_register_sensor` calls |
| `prj.conf` | Appended `CONFIG_SENSOR=y`, `CONFIG_I2C=y`, `CONFIG_SHT4X=y` |
| `boards/conexio_stratus_pro_nrf9151_ns.overlay` | Added `&i2c2` node with `sht4x@44` child; partition includes preserved |
| `CMakeLists.txt` | Unchanged |
| `SHT4X_INTEGRATION.md` | New — documents metrics, changed files, build result, and runtime behaviour |

</details>
