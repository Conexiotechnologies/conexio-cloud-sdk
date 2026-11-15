<div align="center">

[![Twitter: conexiotech](https://img.shields.io/twitter/follow/conexiotech.svg?style=social)](https://twitter.com/conexiotech)
[![NCS](https://img.shields.io/badge/NCS-v3.4.1-blue?logo=nordicsemiconductor)](https://docs.nordicsemi.com/bundle/ncs-3.4.1/page/nrf/index.html)
[![License](https://img.shields.io/badge/License-Apache%202.0-green.svg)](LICENSE)
[![Target](https://img.shields.io/badge/Target-nRF9151-orange)](https://www.nordicsemi.com/Products/nRF9151)

<img src="Design/banner.png" style="margin-bottom:10px" />

# Conexio Cloud SDK — Samples

Standalone sample applications for the Conexio Cloud.
Each sample demonstrates one specific feature of the Conexio Cloud SDK.

</div>

## Installing the Conexio Cloud SDK

Follow the [Conexio Documentation](https://docs.conexiotech.com/), Section: **Getting Started with Conexio Console** on how to install the Conexio Cloud SDK and setup the environment.

## nRF Connect SDK Integration

The SDK integrates with nRF Connect SDK v3.4.1 as a west module. If you are integrating it into your own west workspace, add the following entry to the west.yml file here:

```
nordic/ncs/v3.4.1/nrf/west.yml
```
```
    # Conexio Cloud SDK repository.
    - name: conexio-cloud-sdk
      url: https://github.com/Conexiotechnologies/conexio-cloud-sdk.git
      revision: main
      path: modules/lib/conexio-cloud-sdk
```
and clone all repositories including `conexio-cloud-sdk` by running:

```
west update
```

## Where the Conexio Cloud SDK Resides

After successful west update, the Conexio Cloud SDK can be located at:
```
nordic/ncs/v3.4.1/modules/lib/conexio-cloud-sdk/
```

> You do not need to clone anything separately — the SDK is installed as a Zephyr west module.


## Sample Index

The SDK includes multiple sample applications to get you started easily.

| Sample | Feature | Complexity |
|--------|---------|------------|
| [`hello/`](samples/hello/) | Minimal cloud connection + single metric | ⭐ Beginner |
| [`sensor_data/`](samples/sensor_data/) | Streaming temperature & humidity | ⭐ Beginner |
| [`device_commands/`](samples/device_commands/) | Handling cloud commands (LED, REBOOT) | ⭐⭐ Intermediate |
| [`device_settings/`](samples/device_settings/) | OTA Config JSON settings from Console | ⭐⭐ Intermediate |
| [`device_schedules/`](samples/device_schedules/) | Timed commands with NVS watchdog | ⭐⭐ Intermediate |
| [`cellular_location/`](samples/cellular_location/) | AT%NCELLMEAS cell-based positioning | ⭐⭐ Intermediate |
| [`offline_buffer/`](samples/offline_buffer/) | Flash-backed buffering during outages | ⭐⭐ Intermediate |
| [`fleet_provisioning/`](samples/fleet_provisioning/) | Zero-touch fleet provisioning for Conexio Cloud | ⭐⭐ Intermediate |
| [`fota/`](samples/fota/) | Over-the-air firmware updates | ⭐⭐⭐ Advanced |
| [`full_app_template/`](samples/full_app_template/) | All features combined — production template | ⭐⭐⭐ Advanced |


## Recommended learning path

```
hello → sensor_data → device_commands → device_settings
      ↓
device_schedules → cellular_location → offline_buffer → fota
      ↓
full_app_template  (combine everything into your own application)
```

---
*Copyright (c) 2026 Conexio Technologies, Inc. — Apache-2.0 License*
