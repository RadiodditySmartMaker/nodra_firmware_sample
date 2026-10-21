/*
 * Layer 4a — SX126x shell command interface.
 *
 * Provides "lora" shell command with subcommands:
 *   start, stop, send, recv, recv_async, recv_async_stop,
 *   status, config, setpower, cw, airtime
 */

#ifndef LORA_SX126X_SHELL_H_
#define LORA_SX126X_SHELL_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(PIO_ENABLE_LORA_RA01S)

#ifdef __cplusplus
extern "C" {
#endif

/* ── Initialisation ── */
void sx126x_shell_init(void);

/* ── State query (shared with bench) ── */
bool sx126x_shell_is_started(void);
bool sx126x_shell_is_async_rx(void);

/* ── Public helpers (shared with bench & PioThread) ── */
int sx126x_shell_start(void);
int sx126x_shell_stop(void);
int sx126x_shell_send(const uint8_t *data, size_t len);
int sx126x_shell_recv_data(uint8_t *buf, size_t buf_len, uint32_t timeout_ms, int16_t *rssi, int8_t *snr);
int sx126x_shell_start_async_rx(void);
int sx126x_shell_stop_async_rx(void);

/* ── PioThread polling interface ── */
bool sx126x_shell_is_rx_pending(void);
int16_t sx126x_shell_consume_rx(uint8_t *buf, size_t buf_len, int16_t *rssi, int8_t *snr);

/* ── Airtime ── */
uint32_t sx126x_shell_calc_airtime_us(uint16_t payload_len);

/* ── Internal helpers (shared with bench) ── */
int sx126x_shell_apply_config(bool tx);
void sx126x_shell_hw_apply_pio_init(void);

#ifdef __cplusplus
}
#endif

#endif /* PIO_ENABLE_LORA_RA01S */

#endif /* LORA_SX126X_SHELL_H_ */
