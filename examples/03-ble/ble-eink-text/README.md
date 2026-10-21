# BLE e-ink text

This example exposes a BLE peripheral named `Nodara-Eink-Text`. Text sent from a phone over the BLE UART service is drawn on the e-ink panel, and the board replies with a confirmation.

## Build, upload, and use

```powershell
pio run -e nodara
pio run -e nodara -t upload --upload-port COMx
pio device monitor -p COMx -b 115200
```

Connect to `Nodara-Eink-Text` with a phone BLE tool that supports the Nordic UART Service, then write ASCII text to the UART. The panel refreshes when a newline is sent, or about 300 ms after input stops.

## Files

| Path | Purpose |
| --- | --- |
| `platformio.ini` | Build and upload entry. Enables BLE, the USB CDC serial port, and the e-ink panel. |
| `src/main.cpp` | Program entry. Creates the BLE UART service, receives text, decides when to draw it, refreshes the panel, and sends a confirmation. |
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
| `board/nodara/tusb_config_nodara.h` | USB CDC serial configuration used for the debug log. |

The Bluefruit BLE library comes from the Arduino nRF52 framework and does not need to be copied into `lib/`.
