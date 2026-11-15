# Implementation Plan — Multi-Board LED Support for full_app_template

## Exploration findings

All six overlay boards were checked against their base DTS files. Every board already ships
a `led0` alias in its upstream DTSI — no DTS additions are needed in any overlay file.

### Per-board LED alias map

| Overlay file | Board DTSI | led0 target node | GPIO pin | Flag |
|---|---|---|---|---|
| `conexio_stratus_pro_nrf9151_ns.overlay` | `conexio_stratus_pro_common.dtsi` | `led0: led_0` | `gpio0 25` | `GPIO_ACTIVE_HIGH` |
| `nrf9151dk_nrf9151_ns.overlay` | `nrf9151dk_nrf9151_common.dtsi` | `led0: led_0` | `gpio0 0` | `GPIO_ACTIVE_HIGH` |
| `nrf9160dk_nrf9160_ns.overlay` | `nrf9160dk_nrf9160_common.dtsi` | `led0: led_0` | `gpio0 2` | `0` (active-high, no polarity flag) |
| `nrf9161dk_nrf9161_ns.overlay` | `nrf9161dk_nrf9161_common.dtsi` | `led0: led_0` | `gpio0 0` | `GPIO_ACTIVE_HIGH` |
| `thingy91_nrf9160_ns.overlay` | `thingy91_nrf9160_common.dtsi` | `led0 = &red_led` (rgb red) | `gpio0 29` | `0` (active-high, no polarity flag) |
| `thingy91x_nrf9151_ns.overlay` | `thingy91x_nrf9151_common.dtsi` | `led0 = &red_led` (rgb red) | `gpio0 29` | `0` (active-high, no polarity flag) |

**Conclusion:** The `led0` alias resolves on every target board. The `DT_HAS_ALIAS(led0)` guard
in the updated `led.c` will always take the `#if` branch for all six boards. The `#else`
no-op branch exists purely for forward compatibility with any future board that has no
user-accessible LED.

### Overlay file integrity issue found

`thingy91x_nrf9151_ns.overlay` is missing the closing `>` and trailing newline on its last
`#include` directive. This is a DTS pre-processor syntax error that will cause the build to
fail for Thingy91x regardless of any LED changes.  Fix this as item 1 below.

---

## Implementation Plan

- [ ] 1. Fix the DTS syntax error in `thingy91x_nrf9151_ns.overlay`.

      The file ends with `#include <samples/cellular/nrf91_sram_partitions.dtsi` — the
      closing `>` and trailing newline are absent (confirmed via hex dump). Without the `>`,
      the DTS pre-processor treats everything after it as part of the filename and the
      Thingy91x build fails immediately.

      Change the last line from:
      ```
      #include <samples/cellular/nrf91_sram_partitions.dtsi
      ```
      to:
      ```
      #include <samples/cellular/nrf91_sram_partitions.dtsi>
      ```
      and ensure the file ends with a single trailing newline.

      **Files:** `boards/thingy91x_nrf9151_ns.overlay`

      **Verify:** `west build -b thingy91x/nrf9151/ns ...` no longer errors with a DTS
      pre-processor failure on that include line (build may still fail for Kconfig reasons
      unrelated to LED — see the caveat in item 3).

---

- [ ] 2. Refactor `src/led.c` to use a `DT_HAS_ALIAS(led0)` compile-time guard.

      **Why:** The current implementation uses `DT_ALIAS(led0)` unconditionally at file scope.
      This is fine today because every current board has `led0`, but it will cause a hard
      build error on any future board that lacks the alias. Wrapping the GPIO-specific code
      in `#if DT_HAS_ALIAS(led0)` isolates the hardware dependency and makes the module
      safe to include in any Zephyr application.

      **Exact changes to `src/led.c`:**

      1. Add `#include <zephyr/devicetree.h>` (provides `DT_HAS_ALIAS`). Place it after
         `<zephyr/drivers/gpio.h>` for clarity.

      2. Replace the unconditional `#define LED0_NODE` and `static const struct
         gpio_dt_spec g_led` lines with a `#if DT_HAS_ALIAS(led0)` block that wraps the
         full active implementation:

         ```c
         #if DT_HAS_ALIAS(led0)

         static const struct gpio_dt_spec g_led =
                 GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

         int led_init(void)
         {
                 if (!gpio_is_ready_dt(&g_led)) {
                         LOG_ERR("LED GPIO device not ready");
                         return -ENODEV;
                 }
                 int ret = gpio_pin_configure_dt(&g_led, GPIO_OUTPUT_INACTIVE);
                 if (ret != 0) {
                         LOG_ERR("LED GPIO configure failed (%d)", ret);
                         return ret;
                 }
                 LOG_INF("LED GPIO ready (pin %d)", g_led.pin);
                 /* Boot indicator: 2 s blink */
                 gpio_pin_set_dt(&g_led, 1);
                 k_sleep(K_SECONDS(2));
                 gpio_pin_set_dt(&g_led, 0);
                 return 0;
         }

         void led_on(int led_num)
         {
                 ARG_UNUSED(led_num);
                 int ret = gpio_pin_set_dt(&g_led, 1);
                 if (ret != 0) {
                         LOG_ERR("led_on: gpio_pin_set_dt failed (%d)", ret);
                 } else {
                         LOG_INF("LED is ON");
                 }
         }

         void led_off(int led_num)
         {
                 ARG_UNUSED(led_num);
                 int ret = gpio_pin_set_dt(&g_led, 0);
                 if (ret != 0) {
                         LOG_ERR("led_off: gpio_pin_set_dt failed (%d)", ret);
                 } else {
                         LOG_INF("LED is OFF");
                 }
         }

         #else /* !DT_HAS_ALIAS(led0) */

         int led_init(void)
         {
                 LOG_DBG("No led0 alias — LED init is a no-op");
                 return 0;
         }

         void led_off(int led_num)
         {
                 ARG_UNUSED(led_num);
                 LOG_DBG("No led0 alias — led_off is a no-op");
         }

         void led_on(int led_num)
         {
                 ARG_UNUSED(led_num);
                 LOG_DBG("No led0 alias — led_on is a no-op");
         }

         #endif /* DT_HAS_ALIAS(led0) */
         ```

      3. Update the block comment at the top of the file to document the multi-board
         pattern (replace the Stratus-Pro-only paragraph with a note that all six
         supported boards provide `led0` in their base DTSI, so the active `#if` branch
         is always taken for those boards).

      4. **`src/led.h` must NOT be modified.** `led_init`, `led_on`, and `led_off` keep
         their existing signatures. `main.c` calls them without any `#ifdef`.

      **Files:** `src/led.c`  (read-only: `src/led.h`, `src/main.c`)

      **Verify:** The file must compile without warnings for both the `DT_HAS_ALIAS(led0)=1`
      path (Stratus Pro build) and, conceptually, the `=0` path. The Stratus Pro build in
      item 3 confirms the active path; the no-op path is verified by inspection — it
      contains no GPIO calls, so it has no hardware dependency.

---

- [ ] 3. Build verification.

      Run the following commands in order. Each must complete without errors.

      **Environment setup (run once before any west command):**
      ```sh
      export PATH="/opt/nordic/ncs/toolchains/97226bad62/Cellar/python@3.12/3.12.4/Frameworks/Python.framework/Versions/3.12/bin:/opt/nordic/ncs/toolchains/97226bad62/bin:$PATH"
      export ZEPHYR_SDK_INSTALL_DIR=/opt/nordic/ncs/toolchains/97226bad62/opt/zephyr-sdk
      ```

      **Build 1 — Conexio Stratus Pro (primary board, must pass):**
      ```sh
      cd /opt/nordic/ncs/v3.4.1
      west build -b conexio_stratus_pro/nrf9151/ns \
          modules/lib/conexio-cloud-sdk/samples/full_app_template \
          --build-dir /tmp/build_stratus \
          -p always
      ```
      Expected: build succeeds, `led.c` compiled with `DT_HAS_ALIAS(led0)=1` (gpio0
      pin 25, `GPIO_ACTIVE_HIGH`).

      **Build 2 — nRF9160-DK (multi-board confirmation):**
      ```sh
      west build -b nrf9160dk/nrf9160/ns \
          modules/lib/conexio-cloud-sdk/samples/full_app_template \
          --build-dir /tmp/build_nrf9160dk \
          -p always
      ```
      Expected: build succeeds, `led.c` compiled with `DT_HAS_ALIAS(led0)=1` (gpio0
      pin 2, no polarity flag).

      > **Kconfig caveat for DK builds:** `prj.conf` enables `CONFIG_NRF_FUEL_GAUGE=y`
      > and `CONFIG_REGULATOR=y` which target the nPM1300 PMIC present only on the
      > Stratus Pro. For DK boards these drivers will find no matching DTS node.
      > Depending on how strictly the SDK Kconfig dependency tree is enforced, the DK
      > build may emit a warning or error about missing PMIC hardware. If Build 2 fails
      > for this reason, add a board-specific Kconfig overlay at
      > `boards/nrf9160dk_nrf9160_ns.conf` containing:
      > ```
      > CONFIG_NRF_FUEL_GAUGE=n
      > CONFIG_CONEXIO_CLOUD_BATTERY_METRICS=n
      > CONFIG_CONEXIO_CLOUD_AUTO_BATTERY=y
      > ```
      > Apply the same pattern to `boards/nrf9151dk_nrf9151_ns.conf`,
      > `boards/nrf9161dk_nrf9161_ns.conf`, `boards/thingy91_nrf9160_ns.conf`, and
      > `boards/thingy91x_nrf9151_ns.conf`. This is a Kconfig portability concern
      > independent of LED; it is listed here so the implementer is not surprised.

      **Files:** No file changes — this is a verification-only step.

---

## Files touched summary

| File | Change |
|---|---|
| `boards/thingy91x_nrf9151_ns.overlay` | Add missing `>` and trailing newline on last `#include` |
| `src/led.c` | Wrap GPIO implementation in `#if DT_HAS_ALIAS(led0)` / `#else` no-op |
| `src/led.h` | **No change** |
| `src/main.c` | **No change** |
| All other `boards/*.overlay` | **No change** — `led0` alias already present in each board's base DTSI |
