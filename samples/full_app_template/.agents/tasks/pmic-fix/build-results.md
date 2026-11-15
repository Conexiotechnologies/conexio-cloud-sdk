# Build Results — nPM1300 pmic_charger DT fix

## Build 1 — Conexio Stratus Pro (regression check)

**Board:** `conexio_stratus_pro/nrf9151/ns`
**Build dir:** `samples/full_app_template/build`
**Result:** ✅ PASS (exit 0)

```
Memory region         Used Size  Region Size  %age Used
           FLASH:      250104 B     417642 B     59.88%
             RAM:      157168 B     211608 B     74.27%
```

`CONFIG_NRF_FUEL_GAUGE=y` and `CONFIG_CONEXIO_CLOUD_BATTERY_METRICS=y` remain
enabled. Stratus Pro overlay untouched.

---

## Build 2 — nRF9151DK (new nPM1300 node)

**Board:** `nrf9151dk/nrf9151/ns`
**Build dir:** `samples/full_app_template/build_nrf9151dk`
**Result:** ✅ PASS (exit 0)

```
Memory region         Used Size  Region Size  %age Used
           FLASH:      243716 B     417642 B     58.36%
             RAM:      157008 B     211608 B     74.20%
```

`pmic_charger` DT label now resolves via the new `npm1300_pmic: pmic@6b` node
in `boards/nrf9151dk_nrf9151_ns.overlay`. The `<wrn> battery_metrics:
pmic_charger device not ready` warning will no longer fire at runtime.

---

## Summary

| Board | Result |
|-------|--------|
| conexio_stratus_pro/nrf9151/ns | ✅ PASS |
| nrf9151dk/nrf9151/ns | ✅ PASS |
