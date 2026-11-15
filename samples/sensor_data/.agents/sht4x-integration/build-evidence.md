# Build Evidence — SHT4x Integration

## Build command

```bash
TC=/opt/nordic/ncs/toolchains/97226bad62
WEST=$TC/Cellar/python@3.12/3.12.4/Frameworks/Python.framework/Versions/3.12/bin/west
export PATH="$TC/bin:$TC/usr/bin:$TC/usr/local/bin:$TC/opt/bin:$TC/opt/nanopb/generator-bin:$TC/nrfutil/bin:$TC/opt/zephyr-sdk/gnu/arm-zephyr-eabi/bin:$TC/opt/zephyr-sdk/gnu/riscv64-zephyr-elf/bin:$PATH"
export ZEPHYR_TOOLCHAIN_VARIANT="zephyr"
export ZEPHYR_SDK_INSTALL_DIR="$TC/opt/zephyr-sdk"
export ZEPHYR_BASE=/opt/nordic/ncs/v3.4.1/zephyr
export BOARD_ROOT=/opt/nordic/ncs/v3.4.1/conexio-firmware-sdk
cd /opt/nordic/ncs/v3.4.1/modules/lib/conexio-cloud-sdk/samples/sensor_data
$WEST build -b conexio_stratus_pro/nrf9151/ns --pristine
```

> Note: `BOARD_ROOT` must point to `conexio-firmware-sdk` because the
> `conexio_stratus_pro` board definition lives there, not in the Zephyr tree.

## Result: **0 errors**

Total steps: 409/409 — all completed.

Key evidence that SHT4x driver was compiled in:

```
[381/409] Linking C static library zephyr/drivers/sensor/sensirion/sht4x/libdrivers__sensor__sensirion__sht4x.a
```

Final memory report:

```
Memory region         Used Size  Region Size  %age Used
           FLASH:      180340 B     417642 B     43.18%
             RAM:      100008 B     211608 B     47.26%
        IDT_LIST:           0 B        32 KB      0.00%
```

Signed images generated:
```
Generating files from .../zephyr.elf for board: conexio_stratus_pro/nrf9151/ns
image.py: sign the payload
image.py: sign the payload
[18/18] Generating ../dfu_application.zip
```
