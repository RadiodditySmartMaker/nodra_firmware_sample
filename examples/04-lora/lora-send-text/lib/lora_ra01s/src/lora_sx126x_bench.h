/*
 * Layer 4b — SX126x benchmark (TX/RX throughput test).
 *
 * 255-byte packets with magic (0x4C 0x52), 4-byte sequence number,
 * and CRC8 (polynomial 0x07).
 *
 * Shell subcommands: "lora bench_tx <count>", "lora bench_rx"
 */

#ifndef LORA_SX126X_BENCH_H_
#define LORA_SX126X_BENCH_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(PIO_ENABLE_LORA_RA01S)

#ifdef __cplusplus
extern "C" {
#endif

/* ── Called from PioThread (lora_sx126x.cpp) ── */
bool sx126x_bench_is_busy(void);
bool sx126x_bench_is_rx(void);
int32_t sx126x_bench_tx_run_once(void);
void sx126x_bench_on_rx_packet(const uint8_t *data, int16_t len, int16_t rssi, int8_t snr);
void sx126x_bench_rx_idle_touch(void);
int32_t sx126x_bench_rx_check_idle(void);
void sx126x_bench_rx_print_snap(void);

/* ── Called from shell commands ── */
int sx126x_bench_reject_if_busy(char *out, size_t out_len);

/* ── Shell command handlers (registered in dispatch table) ── */
int cmd_lora_bench_tx(int argc, char *argv[], char *out, size_t out_len);
int cmd_lora_bench_rx(int argc, char *argv[], char *out, size_t out_len);

/* ── Init ── */
void sx126x_bench_init(void);

#ifdef __cplusplus
}
#endif

#endif /* PIO_ENABLE_LORA_RA01S */

#endif /* LORA_SX126X_BENCH_H_ */
