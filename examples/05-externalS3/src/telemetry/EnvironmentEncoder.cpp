#include "telemetry/EnvironmentEncoder.h"

#include <Arduino.h>
#include <cstring>

static void pbPutVarint(uint8_t *buf, size_t &n, size_t cap, uint32_t value)
{
  do {
    if (n >= cap) {
      return;
    }
    uint8_t byte = static_cast<uint8_t>(value & 0x7F);
    value >>= 7;
    if (value != 0) {
      byte |= 0x80;
    }
    buf[n++] = byte;
  } while (value != 0);
}

static void pbPutTag(uint8_t *buf, size_t &n, size_t cap, uint8_t field, uint8_t wireType)
{
  pbPutVarint(buf, n, cap, (static_cast<uint32_t>(field) << 3) | wireType);
}

static void pbPutFloat(uint8_t *buf, size_t &n, size_t cap, uint8_t field, bool has, float value)
{
  if (!has || n + 6 > cap) {
    return;
  }
  pbPutTag(buf, n, cap, field, 5);
  uint32_t bits = 0;
  memcpy(&bits, &value, sizeof(bits));
  buf[n++] = static_cast<uint8_t>(bits);
  buf[n++] = static_cast<uint8_t>(bits >> 8);
  buf[n++] = static_cast<uint8_t>(bits >> 16);
  buf[n++] = static_cast<uint8_t>(bits >> 24);
}

static void pbPutUInt32(uint8_t *buf, size_t &n, size_t cap, uint8_t field, bool has, uint32_t value)
{
  if (!has) {
    return;
  }
  pbPutTag(buf, n, cap, field, 0);
  pbPutVarint(buf, n, cap, value);
}

static size_t encodeEnvironmentMetrics(uint8_t *buf, size_t cap, const EnvironmentMetrics &m)
{
  size_t n = 0;
  pbPutFloat(buf, n, cap, 1, m.has_temperature, m.temperature);
  pbPutFloat(buf, n, cap, 2, m.has_relative_humidity, m.relative_humidity);
  pbPutFloat(buf, n, cap, 3, m.has_barometric_pressure, m.barometric_pressure);
  pbPutFloat(buf, n, cap, 4, m.has_gas_resistance, m.gas_resistance);
  pbPutFloat(buf, n, cap, 5, m.has_voltage, m.voltage);
  pbPutFloat(buf, n, cap, 6, m.has_current, m.current);
  pbPutUInt32(buf, n, cap, 7, m.has_iaq, m.iaq);
  pbPutFloat(buf, n, cap, 8, m.has_distance, m.distance);
  pbPutFloat(buf, n, cap, 9, m.has_lux, m.lux);
  pbPutFloat(buf, n, cap, 10, m.has_white_lux, m.white_lux);
  pbPutFloat(buf, n, cap, 11, m.has_ir_lux, m.ir_lux);
  pbPutFloat(buf, n, cap, 12, m.has_uv_lux, m.uv_lux);
  pbPutUInt32(buf, n, cap, 13, m.has_wind_direction, m.wind_direction);
  pbPutFloat(buf, n, cap, 14, m.has_wind_speed, m.wind_speed);
  pbPutFloat(buf, n, cap, 15, m.has_weight, m.weight);
  pbPutFloat(buf, n, cap, 16, m.has_wind_gust, m.wind_gust);
  pbPutFloat(buf, n, cap, 17, m.has_wind_lull, m.wind_lull);
  pbPutFloat(buf, n, cap, 18, m.has_radiation, m.radiation);
  pbPutFloat(buf, n, cap, 19, m.has_rainfall_1h, m.rainfall_1h);
  pbPutFloat(buf, n, cap, 20, m.has_rainfall_24h, m.rainfall_24h);
  pbPutUInt32(buf, n, cap, 21, m.has_soil_moisture, m.soil_moisture);
  pbPutFloat(buf, n, cap, 22, m.has_soil_temperature, m.soil_temperature);
  return n;
}

size_t encodeEnvironmentTelemetry(uint8_t *out, size_t outCap, const EnvironmentMetrics &metrics)
{
  uint8_t env[192];
  const size_t envLen = encodeEnvironmentMetrics(env, sizeof(env), metrics);

  size_t n = 0;
  pbPutTag(out, n, outCap, 3, 2);  // Telemetry.environment_metrics
  pbPutVarint(out, n, outCap, static_cast<uint32_t>(envLen));
  if (n + envLen > outCap) {
    return n;
  }
  memcpy(out + n, env, envLen);
  n += envLen;
  return n;
}

void logEnvironmentMetrics(const EnvironmentMetrics &m)
{
  Serial.print("TX telemetry env:");
  if (m.has_temperature) {
    Serial.printf(" T=%.1fC", m.temperature);
  }
  if (m.has_relative_humidity) {
    Serial.printf(" H=%.1f%%", m.relative_humidity);
  }
  if (m.has_barometric_pressure) {
    Serial.printf(" P=%.1fhPa", m.barometric_pressure);
  }
  if (m.has_gas_resistance) {
    Serial.printf(" gas=%.1f", m.gas_resistance);
  }
  if (m.has_iaq) {
    Serial.printf(" iaq=%u", m.iaq);
  }
  if (m.has_lux) {
    Serial.printf(" lux=%.1f", m.lux);
  }
  if (m.has_white_lux) {
    Serial.printf(" white=%.1f", m.white_lux);
  }
  if (m.has_ir_lux) {
    Serial.printf(" ir=%.1f", m.ir_lux);
  }
  if (m.has_uv_lux) {
    Serial.printf(" uv=%.1f", m.uv_lux);
  }
  if (m.has_distance) {
    Serial.printf(" dist=%.1f", m.distance);
  }
  if (m.has_voltage) {
    Serial.printf(" V=%.2f", m.voltage);
  }
  if (m.has_current) {
    Serial.printf(" I=%.2f", m.current);
  }
  if (m.has_wind_speed) {
    Serial.printf(" wind=%.1f", m.wind_speed);
  }
  if (m.has_weight) {
    Serial.printf(" kg=%.2f", m.weight);
  }
  if (m.has_radiation) {
    Serial.printf(" rad=%.2f", m.radiation);
  }
  if (m.has_soil_temperature) {
    Serial.printf(" soilT=%.1fC", m.soil_temperature);
  }
  if (m.has_soil_moisture) {
    Serial.printf(" soilM=%u", m.soil_moisture);
  }
  Serial.println();
}
