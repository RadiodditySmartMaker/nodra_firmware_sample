/*
 * Layer 2 — SX126x register / buffer / command SPI access.
 *
 * Depends on lora_sx126x_regs.h (command/register constants) and
 * lora_sx126x_spi.h (low-level SPI transfer).
 *
 * All functions return int16_t status code:
 *   SX126X_ERR_NONE (0) on success,
 *   negative on error (see lora_sx126x_regs.h).
 */

#ifndef LORA_SX126X_HAL_H_
#define LORA_SX126X_HAL_H_

#include "lora_sx126x_regs.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Initialisation / termination (all module pins + SPI + DIO1 IRQ) ── */
/* If dio1_cb is non-NULL, attachInterrupt(DIO1, dio1_cb, RISING) is called. */
int16_t sx126x_hal_init(const char *chip_type, void (*dio1_cb)(void));
void sx126x_hal_term(void);

/* ── Busy pin wait ── */
void sx126x_hal_wait_on_busy(void);

/* ── Register access (16-bit address) ── */
int16_t sx126x_hal_read_reg(uint16_t addr, uint8_t *buf, uint8_t num_bytes);
int16_t sx126x_hal_write_reg(uint16_t addr, const uint8_t *buf, uint8_t num_bytes);

/* ── Buffer (FIFO) access ── */
int16_t sx126x_hal_read_buffer(uint8_t offset, uint8_t *buf, uint8_t num_bytes);
int16_t sx126x_hal_write_buffer(uint8_t offset, const uint8_t *buf, uint8_t num_bytes);

/* ── Generic command: write command + optional data bytes ── */
int16_t sx126x_hal_cmd(uint8_t cmd, const uint8_t *data, uint8_t data_len);

/* ── Generic command: write command, read back response bytes ── */
int16_t sx126x_hal_cmd_read(uint8_t cmd, uint8_t *resp, uint8_t resp_len);

/* ── IRQ flag read / clear ── */
uint16_t sx126x_hal_get_irq_flags(void);
int16_t sx126x_hal_clear_irq_flags(uint16_t mask);

/* ── Packet status helpers ── */
int16_t sx126x_hal_get_rx_buffer_status(uint8_t *payload_len, uint8_t *offset);
uint32_t sx126x_hal_get_packet_status(void);
int16_t sx126x_hal_get_device_errors(void);

/* ── Status parsing ── */
int16_t sx126x_hal_parse_status(uint8_t spi_status);

#ifdef __cplusplus
}
#endif

#endif /* LORA_SX126X_HAL_H_ */
