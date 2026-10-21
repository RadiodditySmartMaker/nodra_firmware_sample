/*
 * Layer 1 — SX126x SPI hardware abstraction for nRF52840 (SPIM3).
 *
 * Provides:
 *   - sx126x_spi_init() / sx126x_spi_deinit()
 *   - sx126x_spi_transfer() — raw full-duplex SPI transfer
 *   - sx126x_spi_assert_cs() / sx126x_spi_release_cs()
 */

#ifndef LORA_SX126X_SPI_H_
#define LORA_SX126X_SPI_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Initialisation ── */
void sx126x_spi_init(void);
void sx126x_spi_deinit(void);

/* ── Low-level SPI transfer ── */
void sx126x_spi_transfer(const uint8_t *tx, uint8_t *rx, size_t len);

/* ── CS control (for multi-transfer sequences) ── */
void sx126x_spi_assert_cs(void);
void sx126x_spi_release_cs(void);

#ifdef __cplusplus
}
#endif

#endif /* LORA_SX126X_SPI_H_ */
