# ESP32-S3 external sensor injection

This note describes how the ESP32-S3 reads sensors, encodes the values as Meshtastic protobuf, and injects them into Nodara over USB CDC, and how Nodara accepts that packet and broadcasts it to the mesh. The later sections record the earlier plan, the failures, and the fixes.

Status as of 2026-09: a BME280 (temperature, humidity, pressure) and a VEML7700 (light) are on I2C. One environment telemetry packet is sent every 60 seconds. The S3 only injects packets. It no longer performs the PhoneAPI `want_config_id` handshake.

---

## 1. Roles

```text
[BME280 / VEML7700 / later sensors]
        I2C (SDA=GPIO6, SCL=GPIO7, 3.3V, 100kHz)
                |
        ESP32-S3 USB Host  (this project)
                |  USB OTG  <->  Nodara USB device CDC
                |  frame: 0x94 0xC3 + big-endian length + ToRadio protobuf
                v
        Nodara (nRF52840, Meshtastic)
                |  PhoneAPI / SerialConsole
                |  MeshService::handleToRadio
                v
        LoRa mesh broadcast  (portnum = TELEMETRY_APP = 67)
                +
        local e-ink environment page
```

- The S3 is not a Meshtastic node. It acts as the phone app: it uses the official serial PhoneAPI and hands the packet to Nodara.
- Other nodes see `from` as Nodara's own node number. Nodara rewrites `p.from = 0` to its local node ID.
- Nodara has no onboard environment I2C sensor. Its firmware defines `MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR_EXTERNAL` so it does not scan a local bus and only accepts the USB injection.

---

## 2. Libraries

Dependencies are listed in `lib_deps`. PlatformIO fetches them during `pio run`.

| Library | Requirement | Role |
|---|---|---|
| EspUsbHost | `https://github.com/tanakamasayuki/EspUsbHost.git` | ESP32-S3 USB host and CDC ACM |
| Adafruit BME280 | `adafruit/Adafruit BME280 Library@^2.3.0` | Temperature, humidity, pressure |
| Adafruit VEML7700 | `adafruit/Adafruit VEML7700 Library@^2.1.6` | Ambient and white light |
| Adafruit Unified Sensor | `adafruit/Adafruit Unified Sensor@^1.1.14` | Adafruit sensor base types |
| Adafruit BusIO | `adafruit/Adafruit BusIO@^1.17.4` | I2C bus helpers |

Keep these versions aligned with `[environmental_base]` and `[environmental_extra_common]` in `master_firmware/platformio.ini` so the readings match the official drivers.

### 2.1 Adding another environment sensor

1. Add the Adafruit or Sensirion library to `lib_deps`.
2. Add a driver under `src/sensors/`, following `BME280Sensor`, with `initDevice()` and `getMetrics()`.
3. Register it from `src/main.cpp`:

```cpp
sensors.addSensor<YourSensor>(&Wire);
```

`SensorRegistry` initializes the sensor and drops it if initialization fails. Other sensors that are already online keep running. Values of the same environment kind are merged into one `EnvironmentMetrics` and sent in one USB packet.

### 2.2 Board build options

```ini
build_unflags =
  -DARDUINO_USB_MODE
  -DARDUINO_USB_CDC_ON_BOOT

build_flags =
  -DBOARD_HAS_PSRAM
  -DARDUINO_USB_MODE=0          ; USB is the host/OTG port, not a device
  -DARDUINO_USB_CDC_ON_BOOT=0   ; logs do not use USB CDC

monitor_rts = 0
monitor_dtr = 0                 ; the UART monitor must not reset the S3
```

Logs use UART (`Serial`). Nodara is plugged into the OTG port. Those two ports are separate.

### 2.3 One telemetry variant per packet

Meshtastic `Telemetry` is a oneof. One packet carries one variant:

| Kind | Protobuf field | Tag | Example sensors |
|---|---|---|---|
| Environment | `environment_metrics` | field 3, `0x1A` | BME280, VEML7700, LTR390 |
| Air quality | `air_quality_metrics` | field 4, `0x22` | SCD41, PMSA003I |
| Power | `power_metrics` | field 5, `0x2A` | INA219 |
| Health | `health_metrics` | field 7, `0x3A` | MAX30102 (excluded by the Nodara firmware) |

BME280 and VEML7700 share one environment packet. An SCD41 needs a separate air-quality packet.

---

## 3. Reading the sensors

Startup in `src/main.cpp`:

1. `Wire.begin(GPIO6, GPIO7)` at 100 kHz with a 20 ms timeout.
2. `sensors.addSensor<BME280Sensor>(&Wire)`
3. `sensors.addSensor<VEML7700Sensor>(&Wire)`

`src/sensors/SensorRegistry.h` follows the firmware `AddI2CSensorTemplate` pattern: allocate the sensor, then keep it only when `initDevice` succeeds.

### 3.1 BME280

- Try `0x76`, then `0x77`. The SDO pin selects the address (GND or 3.3 V).
- Forced mode, X1 oversampling, no filter. This matches `BME280Sensor.cpp` in `master_firmware`.
- Each report calls `takeForcedMeasurement()`, then reads temperature, humidity, and pressure (Pa converted to hPa).

| Quantity | EnvironmentMetrics field |
|---|---|
| temperature | 1 |
| relative_humidity | 2 |
| barometric_pressure | 3 |

### 3.2 VEML7700

- Fixed address `0x10`. It does not collide with the BME280.
- `lux` uses `VEML_LUX_AUTO`. `white_lux` uses the same polynomial correction as the firmware.

| Quantity | Field |
|---|---|
| lux | 9 |
| white_lux | 10 |

### 3.3 Collection interval

`loop()` sends when all of these are true:

- CDC is connected and `DTR=1`
- CDC has been open for more than 500 ms
- at least 60 s have passed since the previous send, or no packet has been sent yet
- `sensors.collectMetrics()` got a reading from at least one sensor

The 60 s interval is intentionally longer than the PhoneAPI 10 s rate limit for `TELEMETRY_APP`. A 10 s interval was dropped by that limit.

---

## 4. Building the ToRadio packet

Encoding has two layers: the telemetry payload, then the MeshPacket / ToRadio wrapper, then the serial frame header.

### 4.1 Telemetry / EnvironmentMetrics

`src/telemetry/EnvironmentEncoder.cpp` writes protobuf by hand from `telemetry.proto`. nanopb is not linked, which keeps the S3 project smaller.

- float: wire type 5 (32-bit little-endian)
- uint32: wire type 0 (varint)
- only fields with `has_xxx == true` are written
- the outer field is `Telemetry.environment_metrics`, field 3, wire type 2, tag `0x1A`, followed by a varint length and the inner bytes

Nested lengths must be varints. A one-byte length breaks the packet once the payload reaches 128 bytes. That was fixed when lux was added.

### 4.2 Data / MeshPacket / ToRadio

`sendDecodedPacket()` encodes these fields from `mesh.proto`:

```text
Data {
  portnum = 67          // field 1 varint, tag 0x08
  payload = Telemetry   // field 2 bytes, tag 0x12 + varint length
}

MeshPacket {
  to = 0xFFFFFFFF       // field 2 fixed32, tag 0x15
  decoded = Data        // field 4 bytes, tag 0x22 + varint length
}

ToRadio {
  packet = MeshPacket   // field 1 bytes, tag 0x0A + varint length
}
```

`from` and `id` are left unset. Nodara replaces `from` with its local node ID and generates a packet id when `id` is 0.

### 4.3 USB frame (StreamAPI)

```text
[0x94] [0xC3] [len_hi] [len_lo] [ToRadio bytes...]
```

This matches `START1` / `START2` in `src/mesh/StreamAPI.cpp`. The length is a 16-bit big-endian value and must stay within `MAX_TO_FROM_RADIO_SIZE`.

---

## 5. Sending to Nodara

The link is S3 USB-OTG host to Nodara nRF52840 USB device CDC (Adafruit TinyUSB, VID `0x239A`).

In `loop()`:

1. After `cdcSerial.connected()`, call `setDtr(true)`. The nRF52 CDC port usually stays closed until the host asserts DTR.
2. Wait 500 ms so enumeration finishes before the first protobuf.
3. `cdcSerial.write(header + ToRadio)` and `flush()`.

There is no `want_config_id` handshake. Nodara replies are not read by default (`drainNodaraRx()` is commented out). The S3 is a one-way injector, not a full client.

---

## 6. How Nodara receives and broadcasts

### 6.1 Serial framing

`SerialConsole` extends `StreamAPI`. It reads the USB CDC `Serial` port, finds `0x94 0xC3`, cuts out the payload by length, and passes it to `handleToRadio()`.

`config.security.serial_enabled` must be true. That is the factory default. When it is false, injected packets are dropped silently.

After a valid protobuf:

- `canWrite = true`, so the device may send FromRadio back over USB
- `usingProtobufs = true`, so logs switch to protobuf and a UART monitor shows binary

That behavior is one reason the handshake was removed.

### 6.2 PhoneAPI decode

`PhoneAPI::handleToRadio()` decodes `ToRadio` with nanopb:

| Variant | Behavior |
|---|---|
| `packet` | `handleToRadioPacket`, then `MeshService::handleToRadio` |
| `want_config_id` | full config dump and a serial client session |
| other | heartbeat, disconnect, MQTT proxy; unused here |

`TELEMETRY_APP` is rate-limited to one packet every 10 seconds. A faster packet gets a QueueStatus reply and is not sent to the mesh.

### 6.3 Mesh send

`MeshService::handleToRadio()`:

1. sets `p.from = 0`, so a client cannot impersonate another node
2. calls `generatePacketId()` when `id` is 0
3. calls `router->sendLocal()`, which encrypts the packet and sends it over LoRa. The mesh sees it as Nodara's own telemetry

Nodara-specific behavior from commit `9f5caa7ad`:

```cpp
sendToMesh(a, RX_SRC_USER, p.decoded.portnum == meshtastic_PortNum_TELEMETRY_APP);
```

When the third argument `ccToPhone` is true, the packet is also queued for the phone:

- LoRa still broadcasts it to other nodes
- the local `EnvironmentTelemetryModule::handleReceivedProtobuf()` updates `lastMeasurementPacket`
- the e-ink environment page can show the temperature, humidity, pressure, and light injected over USB

Without that copy, the local screen does not show the sensor values the node itself transmitted.

### 6.4 Firmware switches

In `variants/nrf52840/nodara/variant.h`:

```cpp
// Keep the EnvironmentTelemetry module for the screen, but do not scan an onboard I2C sensor.
#define MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR_EXTERNAL 1
#define MESHTASTIC_EXCLUDE_POWER_TELEMETRY             1
#define MESHTASTIC_EXCLUDE_AIR_QUALITY_SENSOR          1
#define MESHTASTIC_EXCLUDE_HEALTH_TELEMETRY            1
```

Also:

- `NodeDB::installDefaultModuleConfig()` sets `environment_screen_enabled = true`
- `Modules.cpp` always creates `EnvironmentTelemetryModule` on Nodara, without waiting for the app measurement switch
- `runOnce()` does not transmit its own telemetry when no onboard sensor is present; it only displays received packets

---

## 7. Layout

```text
examples/05-externalS3/
├── platformio.ini                 libraries, USB host options, monitor port
├── src/main.cpp                   USB, DTR, periodic report, ToRadio assembly
├── src/sensors/
│   ├── TelemetrySensor.h          sensor base class
│   ├── SensorRegistry.h           registry
│   ├── BME280Sensor.*
│   └── VEML7700Sensor.*
└── src/telemetry/
    ├── EnvironmentMetrics.h       field numbers aligned with telemetry.proto
    └── EnvironmentEncoder.*       hand-written protobuf encoder
```

---

## 8. What to buy next

The firmware `platformio.ini` groups sensor libraries by telemetry kind. The S3 implements only the environment path.

The BME280 already covers temperature, humidity, and pressure. Later parts should add a new quantity, share the same 3.3 V I2C bus, and use a free address:

| Priority | Kind | Part | I2C | Packet sent to Nodara |
|---|---|---|---|---|
| Present | Temperature, humidity, pressure | BME280 | 0x76/0x77 | environment fields 1/2/3 |
| Present | Light | VEML7700 | 0x10 | environment fields 9/10 |
| Next | CO2 | SCD41 (SCD40 is an alternative) | 0x62 | air-quality `co2` |
| Next | Voltage and current | INA219 | 0x40 | power channel 1 |
| Optional | UV | LTR390 | 0x53 | environment `uv_lux` |
| Later | PM2.5 | PMSA003I (I2C is required) | 0x12 | air-quality packet |

Skip SHT31, BMP280, and AHT20 (they duplicate the BME280), the UART PMS5003 (the firmware expects the I2C PMSA003I), SGP30 and CCS811 (they are not true CO2 sensors), and the MAX30102 (Nodara excludes health telemetry).

Wiring is `VCC=3V3`, `GND=GND`, `SDA=GPIO6`, `SCL=GPIO7`. Tie the BME280 CSB pin to 3.3 V to keep I2C mode. Tie SDO to GND for address 0x76.

---

## 9. Problems already fixed

These follow the commits in the original S3 repository.

### 9.1 The USB port cannot be a device and a host at the same time

The OTG port did not enumerate Nodara, or the log CDC and the host controller fought over the same USB peripheral.

The ESP32-S3 Arduino core enables `ARDUINO_USB_CDC_ON_BOOT` by default, so USB is occupied by the device CDC port.

The fix is `ARDUINO_USB_MODE=0` and `ARDUINO_USB_CDC_ON_BOOT=0`, UART logs, and `monitor_rts` / `monitor_dtr` set to 0. The project uses the pioarduino ESP32 platform and EspUsbHost. Commit `cde1ffb`.

### 9.2 The nRF52 CDC port stayed closed

The device enumerated, but writes did not go through. Adafruit TinyUSB opens ACM only after the host asserts DTR.

The fix is `cdcSerial.setDtr(true)`, then a 500 ms wait. The expected VID is `0x239A`.

### 9.3 The `want_config_id` handshake made Nodara act as a phone client

After the handshake, Nodara's Bluetooth behavior changed, `toPhoneQueue` filled with the config dump, and logs switched to binary protobuf.

A full PhoneAPI session calls `handleStartConfig()`. The device treats the peer as an app: it turns off text debug output, occupies the queue, and can affect BLE. The S3 only needs to send telemetry. It does not need NodeDB or the config.

The handshake was removed in commit `0477002`. After CDC is stable, the sketch sends `ToRadio.packet` directly. `SerialConsole` still sets `usingProtobufs` after the first protobuf, so the S3 also leaves `drainNodaraRx()` disabled and does not print those replies as ASCII.

### 9.4 Reports were rate-limited

A packet every 10 seconds produced `Rate limit portnum 67` on Nodara, and the packet never reached the mesh. `PhoneAPI::handleToRadioPacket` rate-limits `TELEMETRY_APP` and position packets to 10 seconds.

The interval is now 60 seconds, which matches a normal environment telemetry period.

### 9.5 I2C did not find the BME280

The log was `BME280 not found`.

The pins are GPIO6 for SDA and GPIO7 for SCL (commit `0477002`). The bus is 100 kHz with a 20 ms timeout so a stuck transaction cannot hang the loop. Both `0x76` and `0x77` are tried. Power is 3.3 V with a common ground, and CSB is tied to 3.3 V for I2C mode.

### 9.6 A fixed packet was used to prove the link

The first packet was hard-coded as `23.5°C / 61% / 1013.2 hPa` and only proved the USB-to-mesh path. The BME280 was connected in `688f4b9`.

### 9.7 Protobuf lengths must be varints

After lux and other fields pushed the payload to 128 bytes or more, Nodara reported `malformed toradio` or decoded garbage. The early encoder wrote `payloadLen` as one byte. A nested protobuf length is a varint: 0–127 is one byte, and 128–16383 takes two bytes (the low 7 bits plus `0x80`).

`pbPutVarint()` now writes the length of `Data.payload`, `MeshPacket.decoded`, `ToRadio.packet`, and `Telemetry.environment_metrics`. Commit `c58479a`.

### 9.8 The local screen did not show the injected telemetry

Other nodes saw the packet, but Nodara's e-ink environment page stayed blank. The default `sendToMesh(..., ccToPhone=false)` sends a USB packet out over LoRa and does not deliver it to local modules. `MESHTASTIC_EXCLUDE_ENVIRONMENTAL_SENSOR` also removes the whole environment module from the build.

Firmware commit `9f5caa7ad` excludes only `ENVIRONMENTAL_SENSOR_EXTERNAL`, keeps the module and the screen, defaults `environment_screen_enabled` to true, and sends `TELEMETRY_APP` on Nodara with `ccToPhone=true` so the local screen can display it.

### 9.9 Reading Nodara's USB replies interferes

`drainNodaraRx()` prints CDC bytes to the UART as ASCII. After the handshake was removed, Nodara can still reply with binary FromRadio. Printing that looks like garbage and can stall `loop()`. The call stays commented out. Enable it only when capturing a log.

### 9.10 Multiple sensors

Putting the BME280 directly in `main.cpp` did not leave room for more sensors. The sketch now follows the firmware `TelemetrySensor` and `addSensor<>` registry. A new environment sensor is a class plus one `addSensor` line. The encoder already covers the common `EnvironmentMetrics` fields.

---

## 10. Checks

| Check | Expected result |
|---|---|
| S3 UART | `USB device connected VID=0x239A`, `CDC ACM open, DTR=1`, `Opened BME280/VEML7700` |
| S3 periodic log | `TX telemetry env: T=..C H=..% P=..hPa lux=.. white=..` |
| Nodara log | `PACKET FROM PHONE` with portnum 67, and no `malformed toradio` or `Rate limit portnum 67` |
| Mesh / app | The node shows temperature, humidity, pressure, and lux |
| Nodara screen | The environment page shows values on firmware `9f5caa7ad` or later |
| `serial_enabled` | Must be true |

On the S3, the first TX should appear about 0.5 s after Nodara is plugged in, then once every 60 s.

---

## 11. Related commits

S3 project `Nodara_EXesp32s3`:

| Commit | Change |
|---|---|
| `cde1ffb` | USB host and PhoneAPI link, one hard-coded packet |
| `688f4b9` | BME280 connected |
| `0477002` | Handshake removed, 60 s period, I2C on GPIO6/7 |
| `c58479a` | Multi-sensor registry, VEML7700, varint lengths |

Nodara firmware `master_firmware`:

| Commit | Change |
|---|---|
| `9f5caa7ad` | Local display of USB-injected environment telemetry, also copied back to the client |

---

## 12. Adding the next sensor

An SCD41 (CO2, air-quality packet) cannot be added inside the current `encodeEnvironmentTelemetry()`.

1. Add the Sensirion SCD4x library to `lib_deps`.
2. Read CO2 in a new driver and encode `Telemetry.air_quality_metrics` (field 4).
3. Keep `sendDecodedPacket(67, ...)`, but use the air-quality variant as the payload.
4. Stagger it from the environment packet. Each kind has its own 10 s rate limit, so 60 s for both is the safe interval.
5. Nodara currently excludes `AIR_QUALITY_SENSOR`, so the local air-quality page will not update. Other nodes that include the air-quality module can still decode the LoRa packet. Showing it locally requires another Nodara firmware change, following the environment telemetry change.
