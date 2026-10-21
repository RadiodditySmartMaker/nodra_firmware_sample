# E-ink icon movement

This example draws an icon on the e-ink panel. The up, left, right, and down buttons move the icon in that direction. The icon stays inside the panel.

## Refresh strategy

To limit ghosting, the example mixes partial refreshes with a periodic full refresh:

- The first image uses a full refresh.
- Each real move uses a partial refresh.
- Every fourth real move uses a full refresh, then the count starts over.
- A button press that does not change the position does not refresh the panel and does not count.

Partial refresh uses the driver's fast waveform. Full refresh uses the complete waveform and clears ghosting left by partial updates.

## Build and upload

```powershell
pio run -e nodara
pio run -e nodara -t upload --upload-port COMx
```

## Files

| Path | Purpose |
| --- | --- |
| `platformio.ini` | Build and upload entry. Enables the button and e-ink options for this example. |
| `src/main.cpp` | Program entry. Defines the icon bitmap, handles direction buttons, and redraws the icon. |
| `lib/button/library.json` | Button library build metadata. |
| `lib/button/src/button.h` | Button interface: events and callbacks. |
| `lib/button/src/button.cpp` | Scan, debounce, state, and callback dispatch. |
| `lib/button/src/GpioteOverride.cpp` | nRF52 GPIOTE interrupt adapter used by the button module. |
| `lib/elink_gdem0213b74/library.json` | E-ink library build metadata. |
| `lib/elink_gdem0213b74/src/elink_gdem0213b74.h` | High-level e-ink drawing interface. |
| `lib/elink_gdem0213b74/src/elink_gdem0213b74.cpp` | Drawing, framebuffer, and refresh. |
| `lib/elink_gdem0213b74/src/elink_driver.h` | Low-level panel driver interface. |
| `lib/elink_gdem0213b74/src/elink_driver.cpp` | SPI, reset, power, and panel refresh. |
| `lib/elink_gdem0213b74/src/font5x7.h` | ASCII font interface. |
| `lib/elink_gdem0213b74/src/font5x7.cpp` | Glyph data. |
| `board/nodara.json` | PlatformIO board definition for Nodara. |
| `board/nodara/variant.h` | Pin names for the buttons, e-ink panel, and other peripherals. |
| `board/nodara/variant.cpp` | Maps Arduino pin numbers to GPIOs. |
| `board/nodara/nrf52840_s140_v6_nodara.ld` | Linker script for the memory layout. |
