# BLE LED control

This example exposes a BLE peripheral named `Nodara-LED-Control`. After a phone connects, writing `on` or `off` to the BLE UART turns the onboard blue LED on or off. The e-ink panel and the serial port show the current state.

## Build, upload, and use

```powershell
pio run -e nodara
pio run -e nodara -t upload --upload-port COMx
pio device monitor -p COMx -b 115200
```

Connect to `Nodara-LED-Control` with a BLE tool that supports the Nordic UART Service. Write `on` or `off` to the UART and send it.

## Files

| Path | Purpose |
| --- | --- |
| `platformio.ini` | Build and upload entry. Enables BLE, the USB CDC serial port, and the e-ink panel. |
| `src/main.cpp` | Program entry. Creates the BLE UART service, parses `on` and `off`, controls the LED, and updates the panel and the log. |
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

The Bluefruit BLE library comes from the Arduino nRF52 framework and does not need to be copied into `lib/`.
