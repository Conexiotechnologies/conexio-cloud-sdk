# PMIC-fix Review

## Evidence examined

| File | Finding |
|------|---------|
| `boards/nrf9151dk_nrf9151_ns.overlay` | Reviewed in full |
| `boards/nrf9160dk_nrf9160_ns.overlay` | Reviewed in full |
| `boards/thingy91_nrf9160_ns.conf` | Reviewed in full |
| `boards/thingy91x_nrf9151_ns.conf` | Reviewed in full |
| `boards/conexio_stratus_pro_nrf9151_ns.overlay` | Confirmed unmodified |
| `src/conexio_cloud.c` | Confirmed `DEVICE_DT_GET_OR_NULL(DT_NODELABEL(pmic_charger))` at line 246 |
| `nrf/boards/nordic/thingy91x/thingy91x_common.dtsi` | Confirmed charger label is `npm1300_charger` (line 86), not `pmic_charger` |
| `nrf/boards/nordic/thingy91/thingy91_nrf9160_common.dtsi` | Confirmed ADP5360 PMIC (line 158), no nPM1300 |
| `zephyr/boards/nordic/nrf9151dk/nrf9151dk_nrf9151_common.dtsi` | Confirmed `arduino_i2c: &i2c2`, pinctrl `i2c2_default` with `group1` having only `psels` |
| `zephyr/boards/nordic/nrf9160dk/nrf9160dk_nrf9160_common.dtsi` | Confirmed `arduino_i2c: &i2c2`, pinctrl `i2c2_default` with `group1` having only `psels` |
| `build-results.md` | Both required builds (Stratus Pro, nRF9151DK) PASS |

---

## Checklist

### 1. DK overlays — nPM1300 node correctness

**`pmic_charger:` label on charger child node, not parent?**
Both overlays: `npm1300_pmic: pmic@6b { ... pmic_charger: charger { ... } }`. Label is on the child. ✓

**I2C address `0x6b`?**
`reg = <0x6b>;` in both overlays, matching the Thingy91x board DTS which uses the same address. ✓

**`compatible = "nordic,npm1300"` on parent; `compatible = "nordic,npm1300-charger"` on child?**
Both overlays set exactly those strings. ✓

**`thermistor-ohms = <0>` set?**
Both overlays: `thermistor-ohms = <0>; thermistor-beta = <0>;` ✓

**`#include <zephyr/dt-bindings/regulator/npm13xx.h>` present?**
Both overlays include it. No macros from that header are used in the overlay itself, but the include is harmless and does not cause a compile failure. ✓

**`&i2c2_default { group1 { bias-pull-up; }; }` correctly extends existing pinctrl group?**
Base board pinctrl for both DKs defines `i2c2_default: i2c2_default { group1 { psels = <...>; }; }`.
The overlay extends that group1, resulting in merged `{ psels = <...>; bias-pull-up; }`. This is the standard DTS extension pattern, also used without issue by the Stratus Pro overlay (same construct, build passes). ✓

**`&arduino_i2c` correct bus for both DK boards?**
nRF9151DK: `arduino_i2c: &i2c2` confirmed in `nrf9151dk_nrf9151_common.dtsi` line 171. ✓
nRF9160DK: `arduino_i2c: &i2c2` confirmed in `nrf9160dk_nrf9160_common.dtsi` line 204. ✓

---

### 2. Thingy .conf files — Kconfig correctness

**thingy91_nrf9160_ns.conf**
- Thingy:91 board DTS uses ADP5360 PMIC (`compatible = "adi,adp5360"`) — no nPM1300, no `pmic_charger` label. Disabling fuel gauge metrics is correct.
- `CONFIG_CONEXIO_CLOUD_BATTERY_METRICS=n` ✓
- `CONFIG_NRF_FUEL_GAUGE=n` ✓
- `CONFIG_NRF_FUEL_GAUGE_VARIANT_SECONDARY_CELL=n` ✓
- `CONFIG_REGULATOR`, `CONFIG_SENSOR`, `CONFIG_I2C` not mentioned — not disabled. ✓

**thingy91x_nrf9151_ns.conf**
- Thingy:91X board DTS uses nPM1300 but labels the charger `npm1300_charger` (confirmed `thingy91x_common.dtsi` line 86). SDK calls `DT_NODELABEL(pmic_charger)` — label mismatch → device NULL → warning. Disabling metrics is correct.
- `CONFIG_CONEXIO_CLOUD_BATTERY_METRICS=n` ✓
- `CONFIG_NRF_FUEL_GAUGE=n` ✓
- `CONFIG_NRF_FUEL_GAUGE_VARIANT_SECONDARY_CELL=n` ✓
- `CONFIG_REGULATOR`, `CONFIG_SENSOR`, `CONFIG_I2C` not mentioned — not disabled. ✓

---

### 3. Stratus Pro overlay — unchanged

File read and confirmed identical to original. No modifications. ✓

---

### 4. Build results

| Board | Result |
|-------|--------|
| `conexio_stratus_pro/nrf9151/ns` | ✅ PASS — fuel gauge enabled, no regressions |
| `nrf9151dk/nrf9151/ns` | ✅ PASS — `pmic_charger` DT label resolves |

Both required builds pass. ✓

---

## Verdict

**APPROVED** — No blockers. All DT label, I2C bus, pinctrl, compatible, address, Kconfig, and build-result checks pass.
