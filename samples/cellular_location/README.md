# Device Location using Cellular Positioning

Demonstrates gathering neighboring cell measurements and
sending them to Conexio Cloud for server-side positioning.

No positioning solver runs on the device. The firmware gathers raw cell data;
the Conexio Cloud writes the resolved coordinates to the Location Tracker. 
The device location result appears as a pin on the Conexio Console Fleet Map.

---

## What this sample does

1. Initialises the cellular location module after LTE connects
2. Triggers `AT%NCELLMEAS` every `CONFIG_CELL_LOCATION_INTERVAL_SEC` seconds
3. Collects serving cell + up to 17 neighbor cells
4. Queues `location` metrics — they are published to the location topic
5. Conexio Cloud resolves the cell location data to `latitude` and `longitude`

---

## prj.conf settings

| Option | Value | Description |
|--------|-------|-------------|
| `CONFIG_CELL_LOCATION` | `y` | Enable AT%NCELLMEAS module |
| `CONFIG_CELL_LOCATION_INTERVAL_SEC` | `3600` | Fix every hour |
| `CONFIG_CELL_LOCATION_PUBLISH_ON_FIX` | `y` | Publish immediately after fix |
| `CONFIG_CELL_LOCATION_SEARCH_TYPE` | `1` | EXTENDED_LIGHT — 3–8 neighbors |

**Search types:**
- `0` = DEFAULT — fast (1–3s), 0–2 neighbors, ±1–5 km accuracy
- `1` = EXTENDED_LIGHT — moderate (3–8s), 3–8 neighbors, ±300m–1km *(default)*
- `2` = EXTENDED — thorough (5–15s), up to 17 neighbors, best accuracy

---

## Build and flash

```bash
west build -b conexio_stratus_pro/nrf9151/ns --pristine
newtmgr -c serial image upload build/cellular_location/zephyr/zephyr.signed.bin
```

---

## Expected serial output

```
[00:00:00.257] <inf> app: === Conexio Cellular Location Sample ===
[00:00:00.257] <inf> app: Fix interval: 3600s | Publish on fix: yes
[00:00:19.533] <inf> mqtt_transport: MQTT connected
[00:00:19.787] <inf> app: Connected — 3550259349809xx | App v1.0.0
[00:00:21.900] <inf> app: Cell location active — first fix in 3600s
...
[01:00:21.900] <inf> cell_location: AT%NCELLMEAS result: serving + 5 neighbors
[01:00:22.100] <inf> cell_location: Location metrics queued — publishing
```
---

## Production guidance

| Use case | Recommended interval |
|----------|---------------------|
| Development / testing | `180s` |
| Active field tracking | `14400s` (4 hours) |
| Asset management | `28800s` (8 hours) |

---
*Copyright (c) 2026 Conexio Technologies, Inc. — Apache-2.0 License*
