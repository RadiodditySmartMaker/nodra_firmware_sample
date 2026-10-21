/*
 * Layer 1 — SX126x SPI hardware abstraction for nRF52840 (SPIM3).
 *
 * Pin assignment (from variant.h):
 *   SCK  = P0.19, MOSI = P0.22, MISO = P0.23, CS   = P0.24
 *
 * Uses Arduino SPI class (default SPI instance).
 * On Adafruit nRF52 core, SPI.begin() initialises SPIM3
 * with the default pins defined by PIN_SPI_MISO/MOSI/SCK.
 */

#include "lora_sx126x_spi.h"

#include "variant.h"
#include <Arduino.h>
#include <SPI.h>

#if defined(PIO_ENABLE_LORA_RA01S)

/* ── SPI settings ── */
/* SX126x max SPI clock = 16 MHz (datasheet). RadioLib default = 2 MHz. */
#define SX126X_SPI_CLOCK 2000000UL
#define SX126X_SPI_ORDER MSBFIRST
#define SX126X_SPI_MODE SPI_MODE0

static SPISettings spi_settings(SX126X_SPI_CLOCK, SX126X_SPI_ORDER, SX126X_SPI_MODE);

/* ================================================================== */
/* Public API                                                          */
/* ================================================================== */

void sx126x_spi_init(void)
{
    /* CS: owned by spi layer */
    pinMode(PIN_LORA_CS, OUTPUT);
    digitalWrite(PIN_LORA_CS, HIGH);

    SPI.begin();
}

void sx126x_spi_deinit(void)
{
    SPI.end();

    /* De-assert all SPI pins to OUTPUT LOW (module off, prevent leakage) */
    pinMode(PIN_LORA_SCK, OUTPUT);
    digitalWrite(PIN_LORA_SCK, LOW);
    pinMode(PIN_LORA_MOSI, OUTPUT);
    digitalWrite(PIN_LORA_MOSI, LOW);
    pinMode(PIN_LORA_MISO, OUTPUT);
    digitalWrite(PIN_LORA_MISO, LOW);
    pinMode(PIN_LORA_CS, OUTPUT);
    digitalWrite(PIN_LORA_CS, LOW);
}

void sx126x_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    if (rx) {
        /* Full-duplex: write tx, read into rx */
        for (size_t i = 0; i < len; i++) {
            rx[i] = SPI.transfer(tx ? tx[i] : 0x00);
        }
    } else {
        /* Write-only */
        for (size_t i = 0; i < len; i++) {
            (void)SPI.transfer(tx ? tx[i] : 0x00);
        }
    }
}

void sx126x_spi_assert_cs(void)
{
    SPI.beginTransaction(spi_settings);
    digitalWrite(PIN_LORA_CS, LOW);
}

void sx126x_spi_release_cs(void)
{
    digitalWrite(PIN_LORA_CS, HIGH);
    SPI.endTransaction();
}

#endif /* PIO_ENABLE_LORA_RA01S */
