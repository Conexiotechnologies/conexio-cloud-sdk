# SHT4x Real Sensor Integration

This sample now publishes **both** simulated sensor data (existing) and real SHT4x
temperature and humidity readings via the QWIIC port.

## New metrics published

| Metric name | Source | Unit |
|---|---|---|
| `temperature` | Simulated (random 20–40 °C) | °C |
| `humidity` | Simulated (random 40–80 %) | % |
| `temperature_real` | SHT4x hardware sensor | °C |
| `humidity_real` | SHT4x hardware sensor | % |

`temperature_real` and `humidity_real` are omitted from the CBOR payload if the
SHT4x is not present or not ready (the SDK silently skips NaN-valued metrics).

## Board Overlay

### `boards/conexio_stratus_pro_nrf9151_ns.overlay`

Add the `&i2c2` node with the SHT4x child at address `0x44` on I2C_BITRATE_FAST.

```dts
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

### `prj.conf`

```kconfig
CONFIG_SENSOR=y
CONFIG_I2C=y
CONFIG_SHT4X=y
```

`CONFIG_SHT4X` auto-selects `CONFIG_I2C` and `CONFIG_CRC` when the DT node is
`status = "okay"`.

## Runtime behaviour

- **Sensor present and ready**: Both `temperature_real` and `humidity_real` are
  included in every publish with actual measured values.
- **Sensor absent or I2C error**: Callbacks return `NaN`; the SDK omits those
  keys from the payload. Simulated `temperature`/`humidity` continue publishing
  normally.
- Serial log on boot shows `SHT4x ready — real sensor metrics enabled` or
  `SHT4x not ready — real sensor metrics will be skipped`.
