#include <Arduino.h>
#include <Wire.h>
#include <cstring>
#include "EspUsbHost.h"
#include "sensors/BME280Sensor.h"
#include "sensors/SensorRegistry.h"
#include "sensors/VEML7700Sensor.h"
#include "telemetry/EnvironmentEncoder.h"

EspUsbHost usbHost;
EspUsbHostCdcSerial cdcSerial(usbHost);
SensorRegistry sensors;

static bool dtrAsserted = false;
static uint32_t cdcOpenAtMs = 0;
static uint32_t lastTelemetryAtMs = 0;

static constexpr uint8_t kI2cSdaPin = 6;
static constexpr uint8_t kI2cSclPin = 7;
static constexpr uint32_t kTelemetryIntervalMs = 60000;
static constexpr uint32_t kCdcSettleMs = 500;
static constexpr uint8_t kPortNumTelemetryApp = 67;

static void sendToRadio(const uint8_t *proto, uint16_t len)
{
  const uint8_t hdr[4] = {0x94, 0xC3, (uint8_t)(len >> 8), (uint8_t)(len & 0xFF)};
  cdcSerial.write(hdr, sizeof(hdr));
  cdcSerial.write(proto, len);
  cdcSerial.flush();
}

static void pbPutVarint(uint8_t *buf, size_t &n, uint32_t value)
{
  do {
    uint8_t byte = static_cast<uint8_t>(value & 0x7F);
    value >>= 7;
    if (value != 0) {
      byte |= 0x80;
    }
    buf[n++] = byte;
  } while (value != 0);
}

static void sendDecodedPacket(uint8_t portnum, const uint8_t *payload, uint8_t payloadLen)
{
  uint8_t data[8 + 256];
  size_t i = 0;
  data[i++] = 0x08;
  data[i++] = portnum;
  data[i++] = 0x12;
  pbPutVarint(data, i, payloadLen);
  memcpy(data + i, payload, payloadLen);
  i += payloadLen;

  uint8_t pkt[16 + 256];
  size_t j = 0;
  pkt[j++] = 0x15;  // to = broadcast 0xFFFFFFFF
  pkt[j++] = 0xFF;
  pkt[j++] = 0xFF;
  pkt[j++] = 0xFF;
  pkt[j++] = 0xFF;
  pkt[j++] = 0x22;
  pbPutVarint(pkt, j, static_cast<uint32_t>(i));
  memcpy(pkt + j, data, i);
  j += i;

  uint8_t proto[20 + 256];
  size_t k = 0;
  proto[k++] = 0x0A;
  pbPutVarint(proto, k, static_cast<uint32_t>(j));
  memcpy(proto + k, pkt, j);
  k += j;

  sendToRadio(proto, static_cast<uint16_t>(k));
}

static void sendEnvironmentTelemetry(const EnvironmentMetrics &metrics)
{
  uint8_t telemetry[192];
  const size_t tLen = encodeEnvironmentTelemetry(telemetry, sizeof(telemetry), metrics);
  sendDecodedPacket(kPortNumTelemetryApp, telemetry, static_cast<uint8_t>(tLen));
  logEnvironmentMetrics(metrics);
}

static void initI2c()
{
  Wire.begin(kI2cSdaPin, kI2cSclPin);
  Wire.setClock(100000);
  Wire.setTimeOut(20);
}

static void drainNodaraRx()
{
  while (cdcSerial.available() > 0) {
    const int value = cdcSerial.read();
    if (value < 0) {
      break;
    }
    if (value == '\n' || value == '\r' || (value >= 32 && value < 127)) {
      Serial.write((uint8_t)value);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("ESP32-S3 USB Host starting (sensor inject only, no PhoneAPI handshake)");
  Serial.println("UART: this log    OTG: plug Nodara here");
  Serial.printf("I2C: VCC=3V3 GND=GND SDA=GPIO%u SCL=GPIO%u\n", kI2cSdaPin, kI2cSclPin);

  initI2c();
  sensors.addSensor<BME280Sensor>(&Wire);
  sensors.addSensor<VEML7700Sensor>(&Wire);

  usbHost.onDeviceConnected([](const EspUsbHostDeviceInfo &device) {
    Serial.printf("USB device connected  addr=%u  VID=0x%04X PID=0x%04X  %s %s\n", device.address, device.vid,
                  device.pid, device.manufacturer, device.product);
    cdcSerial.setAddress(device.address);
  });

  usbHost.onDeviceDisconnected([](const EspUsbHostDeviceInfo &device) {
    Serial.printf("USB device disconnected addr=%u\n", device.address);
    cdcSerial.clearAddress();
    dtrAsserted = false;
    cdcOpenAtMs = 0;
    lastTelemetryAtMs = 0;
  });

  cdcSerial.setRxBufferSize(8192);
  cdcSerial.begin(115200);

  if (!usbHost.begin()) {
    Serial.printf("usbHost.begin() failed: %s\n", usbHost.lastErrorName());
    return;
  }

  Serial.printf("USB Host ready, %u sensor(s) online\n", static_cast<unsigned>(sensors.count()));
}

void loop()
{
  if (cdcSerial.connected() && !dtrAsserted) {
    if (cdcSerial.setDtr(true)) {
      dtrAsserted = true;
      cdcOpenAtMs = millis();
      Serial.println("CDC ACM open, DTR=1");
    }
  }

  if (dtrAsserted && sensors.hasSensors() && (millis() - cdcOpenAtMs) > kCdcSettleMs &&
      (lastTelemetryAtMs == 0 || (millis() - lastTelemetryAtMs) >= kTelemetryIntervalMs)) {
    EnvironmentMetrics metrics;
    if (sensors.collectMetrics(&metrics)) {
      sendEnvironmentTelemetry(metrics);
      lastTelemetryAtMs = millis();
    }
  }

  // drainNodaraRx();
  delay(1);
}
