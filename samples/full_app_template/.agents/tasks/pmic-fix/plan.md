# Implementation Plan — PMIC / battery-metrics board overlay fix

## Context verified during exploration

| File | Finding |
|---|---|
| `boards/conexio_stratus_pro_nrf9151_ns.overlay` | Correct. `&pmic_charger { thermistor-ohms = <0>; thermistor-beta = <0>; }` and `&i2c2_default { group1 { bias-pull-up; }; }` already present. **Do not touch.** |
| `boards/nrf9151dk_nrf9151_ns.overlay` | Only includes partition dtsi files. No nPM1300 node. |
| `boards/nrf9160dk_nrf9160_ns.overlay` | Only includes partition dtsi files. No nPM1300 node. |
| `boards/thingy91x_nrf9151_ns.overlay` | Only includes partition dtsi files. No `.conf` counterpart exists. |
| `boards/thingy91_nrf9160_ns.overlay` | Only includes partition dtsi files. No `.conf` counterpart exists. |
| `nrf9151dk_nrf9151_common-pinctrl.dtsi` | i2c2: SDA P0.30, SCL P0.31. `arduino_i2c: &i2c2` alias confirmed in `nrf9151dk_nrf9151_common.dtsi`. |
| `nrf9160dk_nrf9160_common-pinctrl.dtsi` | i2c2: SDA P0.30, SCL P0.31 (identical layout). `arduino_i2c: &i2c2` alias confirmed in `nrf9160dk_nrf9160_common.dtsi`. |
| `thingy91x_common.dtsi` | nPM1300 charger child is labeled `npm1300_charger:`, **not** `pmic_charger`. Battery is LP803448. SDK `fuel_gauge.c` is built for LP963450. |
| `thingy91_nrf9160_common.dtsi` | ADP5360 PMIC — no nPM1300 present. No `pmic_charger` DT node anywhere. |
| `prj.conf` | `CONFIG_CONEXIO_CLOUD_BATTERY_METRICS=y`, `CONFIG_NRF_FUEL_GAUGE=y`, `CONFIG_NRF_FUEL_GAUGE_VARIANT_SECONDARY_CELL=y`, `CONFIG_REGULATOR=y`, `CONFIG_SENSOR=y`, `CONFIG_I2C=y` all set globally. |
| `src/conexio_cloud.c:246` | `DEVICE_DT_GET_OR_NULL(DT_NODELABEL(pmic_charger))` — silently returns NULL when label absent, emitting the `<wrn>` seen at runtime. |

---

## Steps

- [ ] 1. Add nPM1300 node to the nRF9151DK overlay.

  The DK ships with an nPM1300-EK expansion board on the Arduino header.
  `arduino_i2c` aliases to `&i2c2` (SDA P0.30 / SCL P0.31 per pinctrl dtsi).
  Add an I2C pull-up group to `i2c2_default`, then define the nPM1300 parent
  node and the `pmic_charger:` child with NTC disabled (`thermistor-ohms = <0>`,
  `thermistor-beta = <0>`) because the nPM1300-EK evaluation board does not
  populate a thermistor by default.

  **File:** `boards/nrf9151dk_nrf9151_ns.overlay`

  Replace the file body (after the existing partition includes) with:

  ```dts
  #include <zephyr/dt-bindings/regulator/npm13xx.h>

  /* Enable I2C pull-ups — nPM1300-EK uses open-drain lines */
  &i2c2_default {
      group1 {
          bias-pull-up;
      };
  };

  /* nPM1300-EK expansion board on Arduino header (arduino_i2c = &i2c2) */
  &arduino_i2c {
      pmic_main: npm1300@6b {
          compatible = "nordic,npm1300";
          reg = <0x6b>;

          pmic_charger: charger {
              compatible = "nordic,npm1300-charger";
              term-microvolt = <4200000>;
              term-warm-microvolt = <4000000>;
              current-microamp = <800000>;
              dischg-limit-microamp = <1000000>;
              vbus-limit-microamp = <500000>;
              /* NTC not connected on nPM1300-EK — disable comparator */
              thermistor-ohms = <0>;
              thermistor-beta = <0>;
              charging-enable;
          };
      };
  };
  ```

  **Verify:**
  ```
  west build -b nrf9151dk/nrf9151/ns \
    /opt/nordic/ncs/v3.4.1/modules/lib/conexio-cloud-sdk/samples/full_app_template \
    --pristine
  ```
  Expected: build succeeds with no DTS errors, no `pmic_charger` undefined
  warnings. The `<wrn> battery_metrics: pmic_charger device not ready` log line
  must NOT appear on a board running this firmware (flash and verify at runtime).

---

- [ ] 2. Add nPM1300 node to the nRF9160DK overlay.

  Identical hardware situation to the nRF9151DK. `arduino_i2c` aliases to
  `&i2c2` (SDA P0.30 / SCL P0.31 confirmed in
  `nrf9160dk_nrf9160_common-pinctrl.dtsi`). Use the same DTS block — the
  nPM1300-EK runs on both DKs.

  **File:** `boards/nrf9160dk_nrf9160_ns.overlay`

  Replace the file body (after the existing partition includes) with:

  ```dts
  #include <zephyr/dt-bindings/regulator/npm13xx.h>

  /* Enable I2C pull-ups — nPM1300-EK uses open-drain lines */
  &i2c2_default {
      group1 {
          bias-pull-up;
      };
  };

  /* nPM1300-EK expansion board on Arduino header (arduino_i2c = &i2c2) */
  &arduino_i2c {
      pmic_main: npm1300@6b {
          compatible = "nordic,npm1300";
          reg = <0x6b>;

          pmic_charger: charger {
              compatible = "nordic,npm1300-charger";
              term-microvolt = <4200000>;
              term-warm-microvolt = <4000000>;
              current-microamp = <800000>;
              dischg-limit-microamp = <1000000>;
              vbus-limit-microamp = <500000>;
              /* NTC not connected on nPM1300-EK — disable comparator */
              thermistor-ohms = <0>;
              thermistor-beta = <0>;
              charging-enable;
          };
      };
  };
  ```

  **Verify:**
  ```
  west build -b nrf9160dk/nrf9160/ns \
    /opt/nordic/ncs/v3.4.1/modules/lib/conexio-cloud-sdk/samples/full_app_template \
    --pristine
  ```
  Expected: build succeeds, no DTS errors.

---

- [ ] 3. Create `boards/thingy91x_nrf9151_ns.conf` to disable battery metrics.

  The Thingy:91X `thingy91x_common.dtsi` defines an nPM1300 charger node
  labeled `npm1300_charger:` — **not** `pmic_charger:`. The SDK's
  `DEVICE_DT_GET_OR_NULL(DT_NODELABEL(pmic_charger))` therefore still returns
  NULL. Renaming the label in an overlay would conflict with Thingy91X's own
  charger configuration and fuel_gauge.c is hardcoded for the LP963450 model
  while Thingy91X ships an LP803448 — mixing models produces wrong SOC readings.
  The correct fix is to disable battery metrics for this board at build time.
  `CONFIG_REGULATOR`, `CONFIG_SENSOR`, and `CONFIG_I2C` must stay enabled
  because they are required for other Thingy91X hardware.

  **File to create:** `boards/thingy91x_nrf9151_ns.conf`

  ```kconfig
  # Thingy:91X uses an LP803448 battery (not LP963450) and its nPM1300
  # charger node is labeled npm1300_charger, not pmic_charger.
  # The SDK fuel gauge is built for LP963450 only; using it with a different
  # model produces incorrect SOC readings. Disable battery metrics for this
  # board until a Thingy91X-specific fuel gauge profile is available.
  # CONFIG_REGULATOR, CONFIG_SENSOR, and CONFIG_I2C are intentionally left
  # enabled — they are required for other Thingy:91X hardware.
  CONFIG_CONEXIO_CLOUD_BATTERY_METRICS=n
  CONFIG_NRF_FUEL_GAUGE=n
  CONFIG_NRF_FUEL_GAUGE_VARIANT_SECONDARY_CELL=n
  ```

  **Verify:**
  ```
  west build -b thingy91x/nrf9151/ns \
    /opt/nordic/ncs/v3.4.1/modules/lib/conexio-cloud-sdk/samples/full_app_template \
    --pristine
  ```
  Expected: build succeeds. `CONFIG_NRF_FUEL_GAUGE` must be `n` in
  `build/zephyr/.config`. The `<wrn> battery_metrics: pmic_charger device not
  ready` log line must NOT appear at runtime.

---

- [ ] 4. Create `boards/thingy91_nrf9160_ns.conf` to disable battery metrics.

  The Thingy:91 uses an ADP5360 PMIC (confirmed in
  `thingy91_nrf9160_common.dtsi`). There is no nPM1300, no `pmic_charger`
  DT node anywhere in its board files, and the ADP5360 does not expose a
  sensor interface compatible with the SDK's fuel gauge. Battery metrics must
  be disabled for this board entirely.

  **File to create:** `boards/thingy91_nrf9160_ns.conf`

  ```kconfig
  # Thingy:91 uses an ADP5360 PMIC — no nPM1300, no pmic_charger DT node.
  # The SDK fuel gauge (LP963450 model, npm1300-charger driver) is
  # incompatible with this hardware. Disable battery metrics entirely.
  # CONFIG_REGULATOR, CONFIG_SENSOR, and CONFIG_I2C are intentionally left
  # enabled — they are required for other Thingy:91 hardware.
  CONFIG_CONEXIO_CLOUD_BATTERY_METRICS=n
  CONFIG_NRF_FUEL_GAUGE=n
  CONFIG_NRF_FUEL_GAUGE_VARIANT_SECONDARY_CELL=n
  ```

  **Verify:**
  ```
  west build -b thingy91/nrf9160/ns \
    /opt/nordic/ncs/v3.4.1/modules/lib/conexio-cloud-sdk/samples/full_app_template \
    --pristine
  ```
  Expected: build succeeds. `CONFIG_NRF_FUEL_GAUGE` must be `n` in
  `build/zephyr/.config`.

---

- [ ] 5. Regression-check the Stratus Pro overlay (DO NOT MODIFY).

  Run the Stratus Pro build to confirm no regressions were introduced by
  changes to shared files (prj.conf is unchanged, but the partition dtsi
  includes in all overlays could indirectly affect the build).

  **Verify:**
  ```
  west build -b conexio_stratus_pro/nrf9151/ns \
    /opt/nordic/ncs/v3.4.1/modules/lib/conexio-cloud-sdk/samples/full_app_template \
    --pristine
  ```
  Expected: build succeeds. `CONFIG_NRF_FUEL_GAUGE=y` and
  `CONFIG_CONEXIO_CLOUD_BATTERY_METRICS=y` must remain `y` in
  `build/zephyr/.config`.

---

- [ ] 6. Commit the changes.

  Stage only the four files modified/created by steps 1–4:
  ```
  git -C /opt/nordic/ncs/v3.4.1/modules/lib/conexio-cloud-sdk \
    add samples/full_app_template/boards/nrf9151dk_nrf9151_ns.overlay \
        samples/full_app_template/boards/nrf9160dk_nrf9160_ns.overlay \
        samples/full_app_template/boards/thingy91x_nrf9151_ns.conf \
        samples/full_app_template/boards/thingy91_nrf9160_ns.conf
  ```

  Commit message:
  ```
  samples: full_app_template: fix pmic_charger for nRF91xx boards

  Add nPM1300 DT nodes to nRF9151DK and nRF9160DK overlays so that
  battery_metrics_init() resolves pmic_charger via DEVICE_DT_GET_OR_NULL
  instead of returning NULL and emitting the "pmic_charger device not
  ready" warning.

  Add per-board Kconfig fragments for Thingy:91X and Thingy:91 that
  disable CONFIG_CONEXIO_CLOUD_BATTERY_METRICS and CONFIG_NRF_FUEL_GAUGE:
  - Thingy:91X: nPM1300 present but labeled npm1300_charger, not
    pmic_charger; battery model is LP803448, not LP963450 (SDK default).
  - Thingy:91: ADP5360 PMIC, no nPM1300, no compatible charger node.

  CONFIG_REGULATOR/SENSOR/I2C are left enabled on both Thingy boards
  because other on-board hardware depends on them.

  Conexio Stratus Pro overlay is unchanged.
  ```

---

## Summary of files changed

| File | Action |
|---|---|
| `boards/nrf9151dk_nrf9151_ns.overlay` | Modified — add nPM1300 + pmic_charger node |
| `boards/nrf9160dk_nrf9160_ns.overlay` | Modified — add nPM1300 + pmic_charger node |
| `boards/thingy91x_nrf9151_ns.conf` | Created — disable battery metrics |
| `boards/thingy91_nrf9160_ns.conf` | Created — disable battery metrics |
| `boards/conexio_stratus_pro_nrf9151_ns.overlay` | **Not touched** |
| `prj.conf` | **Not touched** |
