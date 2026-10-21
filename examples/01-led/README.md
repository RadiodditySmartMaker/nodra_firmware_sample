# LED control

This example uses the onboard RGB LED together with the direction buttons: up turns on blue, left turns on red, and right turns on green. Only one LED is lit at a time, and the e-ink display shows the current state.

## Build and upload

```powershell
pio run -e nodara
pio run -e nodara -t upload --upload-port COMx
```

Replace `COMx` with the board's serial port.

## Files

| Path | Purpose |
| --- | --- |
| `platformio.ini` | Build and upload entry for this example. Selects the Nodara board and build options. |
| `src/` | Application code for this example. |
| `src/main.cpp` | Program entry. Initializes the buttons and e-ink display, handles button events, switches the RGB LED, and updates the on-screen text. |
| `src/button.h` | Button module interface: button indexes, the callback type, and setup. |
| `src/button.cpp` | Button scan, debounce, and release-to-trigger logic. |
| `src/display.h` | E-ink display module interface. |
| `src/display.cpp` | E-ink driver, text drawing, and the status page for this example. |
| `board/` | Nodara board files shipped with this example so it can build on its own. |
| `board/nodara.json` | PlatformIO board definition for Nodara. |
| `board/nodara/variant.h` | Pin names for the onboard LEDs, buttons, and peripherals. |
| `board/nodara/variant.cpp` | Maps Arduino pin numbers to nRF52840 GPIOs. |
| `board/nodara/nrf52840_s140_v6_nodara.ld` | Linker script for the application, RAM, and SoftDevice memory layout. |

`.pio/` is the PlatformIO build cache. It is not example source and should not be committed.
