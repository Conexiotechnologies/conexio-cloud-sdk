/*
 * Copyright (c) 2026 Conexio Technologies, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * cellular_location/src/main.c — Cell-based location sample.
 *
 * Demonstrates gathering neighboring cell measurements and
 * streaming them to the Conexio Cloud for server-side positioning.
 */

#include <conexio_cloud/conexio_cloud.h>
#include <conexio_cloud/cell_location.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#if __has_include(<app_version.h>)
#  include <app_version.h>
#else
#  define APP_VERSION_STRING "1.0.0"
#endif

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

/* ── Cloud event handler ────────────────────────────────────────────────── */
static void on_cloud_event(const struct conexio_cloud_event *evt)
{
    switch (evt->type) {
    case CONEXIO_CLOUD_EVT_CONNECTED:
        LOG_INF("Connected — %s | App v%s",
                conexio_cloud_device_id(), APP_VERSION_STRING);
        break;
    case CONEXIO_CLOUD_EVT_DISCONNECTED:
        LOG_WRN("Disconnected");
        break;
    case CONEXIO_CLOUD_EVT_ERROR:
        LOG_ERR("Cloud error: %d", evt->data.error);
        break;
    default:
        break;
    }
}

/* ── main ─────────────────────────────────────────────────────────────── */
int main(void)
{
    LOG_INF("=== Conexio Cellular Location Sample ===");
    LOG_INF("Fix interval: %ds | Publish on fix: %s",
            CONFIG_CELL_LOCATION_INTERVAL_SEC,
            IS_ENABLED(CONFIG_CELL_LOCATION_PUBLISH_ON_FIX) ? "yes" : "no");

    int ret = conexio_cloud_init(on_cloud_event);
    if (ret) { LOG_ERR("init failed (%d)", ret); return -1; }

    LOG_INF("Connecting...");
    if (conexio_cloud_wait_connected(60000) == 0) {
        k_sleep(K_SECONDS(2));
        conexio_cloud_publish();
    }

    /*
     * Initialise the cell location module AFTER LTE is connected.
     * cell_location_init() registers the AT%NCELLMEAS event handler.
     * The first measurement fires automatically after
     * CONFIG_CELL_LOCATION_INTERVAL_SEC seconds via cell_location_tick().
     */
    cell_location_init();
    LOG_INF("Cell location active — first fix in %ds",
            CONFIG_CELL_LOCATION_INTERVAL_SEC);

    /* Main loop — advance the location countdown every 5 seconds */
    while (1) {
        k_sleep(K_SECONDS(5));
        for (int i = 0; i < 5; i++) {
            cell_location_tick();
        }
    }
    return 0;
}
