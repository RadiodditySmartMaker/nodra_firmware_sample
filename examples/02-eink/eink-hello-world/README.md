# E-ink hello world

This example draws `HELLO,WORLD!` in the center of the 2.13-inch e-ink panel. Use it to check the panel wiring and refresh.

## Build and upload

```powershell
pio run -e nodara
pio run -e nodara -t upload --upload-port COMx
```

## Files

| Path | Purpose |
| --- | --- |
| `platformio.ini` | Build and upload entry. Selects the Nodara board and the e-ink driver for this example. |
| `src/` | Application code for this example. |
| `src/main.cpp` | Program entry. Initializes the panel, then draws and refreshes `HELLO,WORLD!`. |
| `lib/` | Local libraries for this example. PlatformIO builds them automatically. |
| `lib/elink_gdem0213b74/library.json` | Library name, version, and build metadata. |
| `lib/elink_gdem0213b74/src/elink_gdem0213b74.h` | High-level e-ink drawing interface. |
| `lib/elink_gdem0213b74/src/elink_gdem0213b74.cpp` | Drawing, text, framebuffer, and refresh flow. |
| `lib/elink_gdem0213b74/src/elink_driver.h` | Low-level panel driver interface. |
| `lib/elink_gdem0213b74/src/elink_driver.cpp` | SPI, reset, power, and panel refresh. |
| `lib/elink_gdem0213b74/src/font5x7.h` | 5×7 ASCII font interface. |
| `lib/elink_gdem0213b74/src/font5x7.cpp` | 5×7 glyph data. |
| `board/` | Nodara board files so this example can build on its own. |
| `board/nodara.json` | PlatformIO board definition for Nodara. |
| `board/nodara/variant.h` | Pin names for the board peripherals. |
| `board/nodara/variant.cpp` | Maps Arduino pin numbers to nRF52840 GPIOs. |
| `board/nodara/nrf52840_s140_v6_nodara.ld` | Linker script for the memory layout. |
