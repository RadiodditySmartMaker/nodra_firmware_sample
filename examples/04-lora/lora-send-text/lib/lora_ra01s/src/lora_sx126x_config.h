/*
 * Layer 3 — SX126x LoRa modem configuration API.
 *
 * Provides high-level functions to configure the SX126x radio
 * and perform packet TX/RX in LoRa mode.
 *
 * All functions return int16_t status code (SX126X_ERR_NONE = 0,
 * negative on error).  Functions that return a value (RSSI, SNR, …)
 * return the value on success or a negative error code on failure.
 */

#ifndef LORA_SX126X_CONFIG_H_
#define LORA_SX126X_CONFIG_H_

#include "lora_sx126x_regs.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Modem state (persistent across calls) ── */
extern sx126x_modem_config_t g_sx126x_cfg;

/* ── Lifecycle ── */
int16_t sx126x_config_begin(void);
int16_t sx126x_config_begin_lora(float freq_mhz, float bw_khz, uint8_t sf, uint8_t cr, int8_t power, uint16_t preamble);
int16_t sx126x_config_radio_setup(void); /* RadioLib-style config() */
int16_t sx126x_config_standby(void);
int16_t sx126x_config_sleep(bool retain_config);

/* ── Frequency ── */
int16_t sx126x_config_set_frequency(float freq_mhz);

/* ── Modulation parameters (LoRa) ── */
int16_t sx126x_config_set_spreading_factor(uint8_t sf);
int16_t sx126x_config_set_bandwidth(float bw_khz);
int16_t sx126x_config_set_coding_rate(uint8_t cr);
int16_t sx126x_config_set_modulation_params(void);
int16_t sx126x_config_set_output_power(int8_t power_dbm);
int16_t sx126x_config_set_preamble_length(uint16_t symbols);
int16_t sx126x_config_set_sync_word(uint8_t sync_word);

/* ── Packet parameters ── */
int16_t sx126x_config_set_packet_params(bool crc_on, bool implicit_header, uint8_t payload_len, bool iq_inverted);

/* ── IQ config ── */
int16_t sx126x_config_set_iq_inversion(bool invert);

/* ── IRQ / DIO ── */
int16_t sx126x_config_set_dio_irq_params(uint16_t irq_mask, uint16_t dio1_mask, uint16_t dio2_mask, uint16_t dio3_mask);
int16_t sx126x_config_set_dio2_as_rf_switch(bool enable);
int16_t sx126x_config_set_rx_boosted_gain(bool enable);

/* ── Regulator / Current / PA ── */
int16_t sx126x_config_set_regulator_dcdc(void);
int16_t sx126x_config_set_regulator_ldo(void);
int16_t sx126x_config_set_current_limit(float ma);
int16_t sx126x_config_set_pa_config(int8_t power_dbm);
int16_t sx126x_config_calibrate_image(float freq_mhz);

/* ── TX / RX ── */
int16_t sx126x_config_transmit(const uint8_t *data, size_t len);
int16_t sx126x_config_receive(uint8_t *buf, size_t buf_len, uint32_t timeout_ms, int16_t *rssi, int8_t *snr);
int16_t sx126x_config_start_receive(void);

/* ── Buffer management ── */
int16_t sx126x_config_set_buffer_base_address(void);
int16_t sx126x_config_read_data(uint8_t *buf, size_t buf_len, int16_t *rssi, int8_t *snr);

/* ── Status queries ── */
float sx126x_config_get_rssi(void);
float sx126x_config_get_snr(void);
uint16_t sx126x_config_get_irq_flags(void);
int16_t sx126x_config_clear_irq_flags(void);
size_t sx126x_config_get_packet_length(void);
uint8_t sx126x_config_get_packet_type(void);
int16_t sx126x_config_get_device_errors(void);

/* ── Airtime calculation ── */
uint32_t sx126x_config_calc_airtime_us(uint16_t payload_len, const sx126x_modem_config_t *cfg);

/* ── Hardware workarounds (datasheet known limitations) ── */
int16_t sx126x_config_fix_sensitivity(void);
int16_t sx126x_config_fix_pa_clamping(bool enable);

#ifdef __cplusplus
}
#endif

#endif /* LORA_SX126X_CONFIG_H_ */
