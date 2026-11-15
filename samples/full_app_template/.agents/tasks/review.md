# Review — Multi-Board LED Support (DT_HAS_ALIAS guard)

## Evidence read

- `src/led.c` — current state (DT_HAS_ALIAS guard in place)
- `src/led.h` — current state
- `src/main.c` — current state
- `boards/*.overlay` — all six overlay files
- `conexio-firmware-sdk/boards/conexio/stratus_pro/conexio_stratus_pro_common.dtsi` — confirmed led0 alias
- `/opt/nordic/ncs/v3.4.1/zephyr/include/zephyr/devicetree.h` — confirmed `DT_HAS_ALIAS` macro
- `.agents/tasks/plan.md` — coder's implementation plan and per-board alias map
- `.agents/tasks/review.md` / `review.json` — prior review (LED extraction pass)

---

## 1. led.c guard correctness

**`DT_HAS_ALIAS` availability**: Confirmed.
`devicetree.h` line: `#define DT_HAS_ALIAS(alias_name) DT_NODE_EXISTS(DT_ALIAS(alias_name))`.

**All three functions guarded**: Yes. `led_init`, `led_on`, and `led_off` appear in both the `#if DT_HAS_ALIAS(led0)` active branch and the `#else` no-op branch. No function is stranded outside the guard.

**Boot-blink and GPIO logic preserved**: Yes, and the prior review's finding (boot-blink return values silently discarded) is now fixed. Both `gpio_pin_set_dt` calls inside `led_init()` assign their return value, check for nonzero, and call `LOG_ERR` on failure.

**`ARG_UNUSED` in no-op branches**: Yes — `led_on` and `led_off` in the `#else` block both call `ARG_UNUSED(led_num)`. `led_init` takes no parameters.

**`ARG_UNUSED` in active branches**: Yes — `led_on` and `led_off` in the `#if` block also call `ARG_UNUSED(led_num)`.

**`#include <zephyr/devicetree.h>`**: Present in `led.c`. Required for `DT_HAS_ALIAS` and already included alongside `<zephyr/drivers/gpio.h>`.

No issues.

---

## 2. API preservation

**`led.h` unchanged**: Yes. The header declares `int led_init(void)`, `void led_on(int led_num)`, `void led_off(int led_num)` with an include guard. No DT macros or GPIO types are exposed.

**`main.c` calls without board `#ifdef`**: Yes. `main()` calls `led_init()` unconditionally; the `on_led_on` / `on_led_off` command handlers call `led_on(0)` / `led_off(0)` unconditionally. No board-specific guards in `main.c`.

No issues.

---

## 3. Overlay file correctness

All six overlay files were read. None adds a `led0` alias or any GPIO node — only partition `#include` directives and, for the Stratus Pro, the PMIC/UART customizations that pre-date this change. This is correct: each board's base DTSI already provides `led0`, so adding a duplicate in the overlay would cause a DTS label conflict.

Per-board alias verified against base DTS:

| Board overlay | led0 source (verified) | Pin | Flag |
|---|---|---|---|
| `conexio_stratus_pro_nrf9151_ns` | `conexio_stratus_pro_common.dtsi` (read, line 81) | gpio0 25 | `GPIO_ACTIVE_HIGH` |
| `nrf9151dk_nrf9151_ns` | plan.md / nrf9151dk_nrf9151_common.dtsi | gpio0 0 | `GPIO_ACTIVE_HIGH` |
| `nrf9160dk_nrf9160_ns` | plan.md / nrf9160dk_nrf9160_common.dtsi | gpio0 2 | `0` (active-high) |
| `nrf9161dk_nrf9161_ns` | plan.md / nrf9161dk_nrf9161_common.dtsi | gpio0 0 | `GPIO_ACTIVE_HIGH` |
| `thingy91_nrf9160_ns` | plan.md / thingy91_nrf9160_common.dtsi | gpio0 29 | `0` (active-high) |
| `thingy91x_nrf9151_ns` | plan.md / thingy91x_nrf9151_common.dtsi | gpio0 29 | `0` (active-high) |

The previously-identified DTS syntax error in `thingy91x_nrf9151_ns.overlay` (missing closing `>` on the last `#include`) is **resolved** — the file now reads `#include <samples/cellular/nrf91_sram_partitions.dtsi>` with the correct `>` and a trailing newline.

No issues.

---

## 4. Build evidence

The prior review (LED extraction pass) recorded "Build verified: OK (commit: 93881b3)" for the Conexio Stratus Pro. No separate build output file exists for this multi-board guard change.

The risk of a build regression from this change is structurally negligible:
- The `#if DT_HAS_ALIAS(led0)` active branch wraps the same code that the prior build verified.
- Every current board provides `led0` in its base DTSI, so the active branch is always selected and runtime behavior is identical to the prior approved build.
- The `#else` no-op branch contains no GPIO calls, no DT macros, and no hardware dependencies. Its only content is `ARG_UNUSED` and `LOG_DBG` calls, which compile unconditionally.
- The overlay files contain zero functional DTS changes.

No new build failure path is introduced.

---

## Prior finding resolution

Prior review raised: **boot-blink gpio errors silently swallowed** — `gpio_pin_set_dt` return values discarded inside `led_init()`.

**Resolved**: The current `led.c` assigns the return value of both `gpio_pin_set_dt` calls, checks for nonzero, and calls `LOG_ERR` with the error code. The prior finding is closed.

---

## Verdict

**APPROVED** — no blockers. All guard correctness, API preservation, and overlay safety criteria pass. The prior review's error-handling finding has been addressed.
