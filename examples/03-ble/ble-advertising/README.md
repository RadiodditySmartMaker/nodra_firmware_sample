# BLE advertising

This example starts BLE advertising under the name `Nodara-BLE-Demo`. The e-ink panel and the serial port show the current advertising state. A phone BLE scanner can find that name.

## Build, upload, and view the log

```powershell
pio run -e nodara
pio run -e nodara -t upload --upload-port COMx
pio device monitor -p COMx -b 115200
```

## Files

| Path | Purpose |
| --- | --- |
| `platformio.ini` | Build and upload entry. Enables BLE, the USB CDC serial port, and the e-ink panel. |
| `src/main.cpp` | Program entry. Initializes Bluefruit, sets the device name and advertising data, and shows the status on the serial port and the panel. |
| `lib/elink_gdem0213b74/library.json` | E-ink library build metadata. |
| `lib/elink_gdem0213b74/src/elink_gdem0213b74.h` | High-level e-ink drawing interface. |
| `lib/elink_gdem0213b74/src/elink_gdem0213b74.cpp` | Text, framebuffer, and refresh. |
| `lib/elink_gdem0213b74/src/elink_driver.h` | Low-level panel driver interface. |
| `lib/elink_gdem0213b74/src/elink_driver.cpp` | SPI, reset, power, and panel refresh. |
| `lib/elink_gdem0213b74/src/font5x7.h` | ASCII font interface. |
| `lib/elink_gdem0213b74/src/font5x7.cpp` | Glyph data. |
| `board/nodara.json` | PlatformIO board definition for Nodara. |
| `board/nodara/variant.h` | Pin names for the board peripherals. |
| `board/nodara/variant.cpp` | Maps Arduino pin numbers to GPIOs. |
| `board/nodara/nrf52840_s140_v6_nodara.ld` | Linker script for the memory layout. |
| `board/nodara/tusb_config_nodara.h` | USB CDC serial configuration used for the runtime log. |

The Bluefruit BLE library comes from the Arduino nRF52 framework, so it is not copied into this example's `lib/` directory.
