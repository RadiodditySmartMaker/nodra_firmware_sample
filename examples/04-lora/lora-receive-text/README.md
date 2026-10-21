# LoRa receive text

This example makes Nodara a LoRa receiver. Received text is printed on the serial port together with RSSI and SNR, and the first payload is drawn on the e-ink panel.

The default radio settings come from `lib/lora_ra01s/src/lora_sx126x_config.cpp`. The transmitter and receiver must use the same frequency, spreading factor, bandwidth, coding rate, sync word, and CRC setting.

## Build, upload, and view the log

```powershell
pio run -e nodara
pio run -e nodara -t upload --upload-port COMx
pio device monitor -p COMx -b 115200
```

## Files

| Path | Purpose |
| --- | --- |
| `platformio.ini` | Build and upload entry. Enables the USB CDC serial port, e-ink panel, and LoRa radio. |
| `src/main.cpp` | Program entry. Starts LoRa receive, prints the payload and signal quality, and updates the panel. |
| `lib/elink_gdem0213b74/library.json` | E-ink library build metadata. |
| `lib/elink_gdem0213b74/src/elink_gdem0213b74.h` | High-level e-ink drawing interface. |
| `lib/elink_gdem0213b74/src/elink_gdem0213b74.cpp` | Text, framebuffer, and refresh. |
| `lib/elink_gdem0213b74/src/elink_driver.h` | Low-level panel driver interface. |
| `lib/elink_gdem0213b74/src/elink_driver.cpp` | SPI, reset, power, and panel refresh. |
| `lib/elink_gdem0213b74/src/font5x7.h` | ASCII font interface. |
| `lib/elink_gdem0213b74/src/font5x7.cpp` | Glyph data. |
| `lib/lora_ra01s/library.json` | LoRa library build metadata. |
| `lib/lora_ra01s/src/lora_ra01s.h` | Application-facing start, send, and receive interface. |
| `lib/lora_ra01s/src/lora_sx126x.h` | Main SX126x driver interface. |
| `lib/lora_ra01s/src/lora_sx126x.cpp` | SX126x init, state, and send/receive flow. |
| `lib/lora_ra01s/src/lora_sx126x_config.h` | Radio parameter types and interface. |
| `lib/lora_ra01s/src/lora_sx126x_config.cpp` | Default frequency, spreading factor, bandwidth, coding rate, and transmit power. |
| `lib/lora_ra01s/src/lora_sx126x_hal.h` | Hardware adaptation interface. |
| `lib/lora_ra01s/src/lora_sx126x_hal.cpp` | Reset, power, GPIO, and interrupt handling on the nRF52840. |
| `lib/lora_ra01s/src/lora_sx126x_spi.h` | SX126x SPI interface. |
| `lib/lora_ra01s/src/lora_sx126x_spi.cpp` | SX126x SPI read and write. |
| `lib/lora_ra01s/src/lora_sx126x_regs.h` | SX126x commands, registers, and error codes. |
| `lib/lora_ra01s/src/lora_sx126x_shell.h` | Helper interface used by the application. |
| `lib/lora_ra01s/src/lora_sx126x_shell.cpp` | Send/receive helpers and an optional serial command wrapper. |
| `lib/lora_ra01s/src/lora_sx126x_bench.h` | Link-test helper interface. |
| `lib/lora_ra01s/src/lora_sx126x_bench.cpp` | Optional link-test statistics. |
| `board/nodara.json` | PlatformIO board definition for Nodara. |
| `board/nodara/variant.h` | Pin names for the LoRa radio, e-ink panel, and other peripherals. |
| `board/nodara/variant.cpp` | Maps Arduino pin numbers to GPIOs. |
| `board/nodara/nrf52840_s140_v6_nodara.ld` | Linker script for the memory layout. |
| `board/nodara/tusb_config_nodara.h` | USB CDC serial configuration used for the receive log. |
