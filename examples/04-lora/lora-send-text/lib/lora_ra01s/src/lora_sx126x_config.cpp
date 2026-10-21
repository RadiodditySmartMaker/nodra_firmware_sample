/*
 * Layer 3 — SX126x LoRa modem configuration API implementation.
 *
 * Ported from RadioLib SX126x.cpp / SX126x_config.cpp / SX1262.cpp.
 * All RADIOLIB_ prefix stripped; class nesting flattened to functions.
 * Modem state held in global g_sx126x_cfg.
 */

#include "lora_sx126x_config.h"
#include "lora_sx126x_hal.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

#if defined(PIO_ENABLE_LORA_RA01S)

/* ================================================================== */
/* Global modem state                                                  */
/* ================================================================== */
sx126x_modem_config_t g_sx126x_cfg = {
    .frequency = 906.875f,
    .sf = 11,
    .bw_khz = 250.0f,
    .cr = 5,
    .tx_power = 22,
    .preamble_len = 8,
    .sync_word = 0x12,
    .crc_enabled = true,
    .ldro_auto = true,
    .header_implicit = false,
    .iq_inverted = false,
};

/* ── Internal state not exposed in config struct ── */
static uint8_t s_bandwidth_reg = SX126X_LORA_BW_500_0; /* default 500kHz (RadioLib init) */
static uint8_t s_coding_rate;                          /* 0..4 or 5..7 for long interleaver */
static uint8_t s_ldro;                                 /* low data rate optimize */
static uint8_t s_pwr;                                  /* raw power register value (0..22) */
static uint8_t s_packet_type;                          /* cached packet type */

/* ================================================================== */
/* Frequency step size helper                                          */
/* ================================================================== */
#define FREQ_STEP SX126X_FREQUENCY_STEP_SIZE

static uint32_t freq_to_frf(float freq_mhz)
{
    /* RadioLib formula: frf = freq_MHz * (1<<25) / 32.0 */
    return (uint32_t)(freq_mhz * 1048576.0f);
}

/* ================================================================== */
/* SX1262 PA optimal table (from RadioLib)                             */
/* ================================================================== */
static const sx126x_pa_table_entry_t pa_opt_table[32] = {
    {2, 2, -5}, {2, 1, 0},  {1, 1, 3},  {1, 2, 0},  {1, 1, 6},  {1, 2, 3},  {2, 2, 2},  {4, 1, 6},
    {1, 1, 11}, {2, 1, 11}, {1, 1, 14}, {2, 1, 14}, {1, 1, 20}, {1, 1, 22}, {2, 2, 11}, {3, 1, 21},
    {1, 2, 17}, {4, 2, 13}, {1, 2, 20}, {1, 2, 22}, {2, 2, 21}, {3, 2, 21}, {1, 4, 19}, {1, 4, 20},
    {3, 3, 20}, {2, 5, 19}, {1, 6, 22}, {2, 5, 22}, {3, 5, 22}, {3, 6, 22}, {4, 6, 22}, {4, 7, 22},
};

/* ================================================================== */
/* Lifecycle                                                           */
/* ================================================================== */

int16_t sx126x_config_begin(void)
{
    int16_t state;

    /* Note: sx126x_hal_init() must have been called before this function.
     * The caller (e.g. shell layer) is responsible for power/GPIO/SPI init. */

    /* ======== RadioLib modSetup + config() equivalent ======== */

    /* Standby RC (with retry like RadioLib modSetup) */
    state = sx126x_config_standby();
    if (state != SX126X_ERR_NONE)
        return state;

    /* --- config() --- */

    /* Set buffer base address (0, 0) */
    state = sx126x_config_set_buffer_base_address();
    if (state != SX126X_ERR_NONE)
        return state;

    /* Set packet type to LoRa */
    s_packet_type = SX126X_PACKET_TYPE_LORA;
    uint8_t pt = SX126X_PACKET_TYPE_LORA;
    state = sx126x_hal_cmd(SX126X_CMD_SET_PACKET_TYPE, &pt, 1);
    if (state != SX126X_ERR_NONE)
        return state;

    /* Set Rx/Tx fallback mode to STDBY_RC */
    {
        uint8_t fb = SX126X_RX_TX_FALLBACK_STDBY_RC;
        state = sx126x_hal_cmd(SX126X_CMD_SET_RX_TX_FALLBACK_MODE, &fb, 1);
        if (state != SX126X_ERR_NONE)
            return state;
    }

    /* Set CAD params (RadioLib defaults) */
    {
        uint8_t cad[7] = {
            SX126X_CAD_ON_8_SYMB,            /* cadSymbolNum */
            (uint8_t)(g_sx126x_cfg.sf + 13), /* cadDetPeak */
            0x0A,                            /* cadDetMin (RADIOLIB_SX126X_CAD_PARAM_DET_MIN) */
            SX126X_CAD_GOTO_STDBY,           /* cadExitMode */
            0x00,
            0x00,
            0x00 /* cadTimeout */
        };
        state = sx126x_hal_cmd(SX126X_CMD_SET_CAD_PARAMS, cad, 7);
        if (state != SX126X_ERR_NONE)
            return state;
    }

    /* Clear IRQ and disable all DIO IRQs */
    state = sx126x_hal_clear_irq_flags(SX126X_IRQ_ALL);
    if (state != SX126X_ERR_NONE)
        return state;
    state = sx126x_config_set_dio_irq_params(SX126X_IRQ_NONE, SX126X_IRQ_NONE, 0, 0);
    if (state != SX126X_ERR_NONE)
        return state;

    /* Calibrate all blocks */
    {
        uint8_t cal = SX126X_CALIBRATE_ALL;
        state = sx126x_hal_cmd(SX126X_CMD_CALIBRATE, &cal, 1);
        if (state != SX126X_ERR_NONE)
            return state;
        /* Wait for calibration to complete (BUSY pin) */
        sx126x_hal_wait_on_busy();
        delay(5);
    }

    /* --- end of config() --- */

    /* Set regulator to DC-DC (RA-01S uses DCDC) */
    state = sx126x_config_set_regulator_dcdc();
    if (state != SX126X_ERR_NONE)
        return state;

    /* Set coding rate from default config */
    state = sx126x_config_set_coding_rate(g_sx126x_cfg.cr);
    if (state != SX126X_ERR_NONE)
        return state;

    /* Set sync word (private) */
    state = sx126x_config_set_sync_word(SX126X_SYNC_WORD_PRIVATE);
    if (state != SX126X_ERR_NONE)
        return state;

    /* Set preamble length */
    state = sx126x_config_set_preamble_length(g_sx126x_cfg.preamble_len);
    if (state != SX126X_ERR_NONE)
        return state;

    /* Set current limit 60 mA */
    state = sx126x_config_set_current_limit(60.0f);
    if (state != SX126X_ERR_NONE)
        return state;

    /* DIO2 as RF switch (RA-01S uses DIO2 for TX/RX switching) */
    state = sx126x_config_set_dio2_as_rf_switch(true);
    if (state != SX126X_ERR_NONE)
        return state;

    /* CRC on (2 bytes) */
    state = sx126x_config_set_packet_params(true, false, 0xFF, false);
    if (state != SX126X_ERR_NONE)
        return state;

    /* IQ config: non-inverted (RadioLib invertIQ(false)) */
    state = sx126x_config_set_iq_inversion(false);
    if (state != SX126X_ERR_NONE)
        return state;

    return SX126X_ERR_NONE;
}

int16_t sx126x_config_begin_lora(float freq_mhz, float bw_khz, uint8_t sf, uint8_t cr, int8_t power, uint16_t preamble)
{
    int16_t state = sx126x_config_begin();
    if (state != SX126X_ERR_NONE)
        return state;

    state = sx126x_config_set_spreading_factor(sf);
    if (state != SX126X_ERR_NONE)
        return state;

    state = sx126x_config_set_bandwidth(bw_khz);
    if (state != SX126X_ERR_NONE)
        return state;

    state = sx126x_config_set_frequency(freq_mhz);
    if (state != SX126X_ERR_NONE)
        return state;

    state = sx126x_config_fix_pa_clamping(true);
    if (state != SX126X_ERR_NONE)
        return state;

    state = sx126x_config_set_preamble_length(preamble);
    if (state != SX126X_ERR_NONE)
        return state;

    state = sx126x_config_set_output_power(power);
    if (state != SX126X_ERR_NONE)
        return state;

    return SX126X_ERR_NONE;
}

int16_t sx126x_config_standby(void)
{
    return sx126x_hal_cmd(SX126X_CMD_SET_STANDBY, (const uint8_t[]){SX126X_STANDBY_RC}, 1);
}

int16_t sx126x_config_sleep(bool retain_config)
{
    uint8_t mode =
        retain_config ? (SX126X_SLEEP_START_WARM | SX126X_SLEEP_RTC_OFF) : (SX126X_SLEEP_START_COLD | SX126X_SLEEP_RTC_OFF);
    int16_t state = sx126x_hal_cmd(SX126X_CMD_SET_SLEEP, &mode, 1);
    delay(1);
    return state;
}

/* ================================================================== */
/* Frequency                                                           */
/* ================================================================== */

int16_t sx126x_config_set_frequency(float freq_mhz)
{
    if (freq_mhz < 150.0f || freq_mhz > 960.0f) {
        return SX126X_ERR_INVALID_FREQ;
    }
    g_sx126x_cfg.frequency = freq_mhz;

    uint32_t frf = freq_to_frf(freq_mhz);
    uint8_t data[4] = {(uint8_t)(frf >> 24), (uint8_t)(frf >> 16), (uint8_t)(frf >> 8), (uint8_t)(frf)};

    /* Set frequency first, then auto-calibrate image.
     * RadioLib does calibrate → set, but empirically set → calibrate
     * avoids CMD_FAILED on the SetRfFrequency command. */
    int16_t state = sx126x_hal_cmd(SX126X_CMD_SET_RF_FREQUENCY, data, 4);
    if (state != SX126X_ERR_NONE)
        return state;

    state = sx126x_config_calibrate_image(freq_mhz);
    return state;
}

/* ================================================================== */
/* Modulation parameters                                               */
/* ================================================================== */

int16_t sx126x_config_set_spreading_factor(uint8_t sf)
{
    if (sf < 5 || sf > 12)
        return SX126X_ERR_INVALID_SF;

    g_sx126x_cfg.sf = sf;
    return sx126x_config_set_modulation_params();
}

int16_t sx126x_config_set_bandwidth(float bw_khz)
{
    if (bw_khz < 0.0f || bw_khz > 510.0f)
        return SX126X_ERR_INVALID_BW;

    uint8_t bw_div2 = (uint8_t)(bw_khz / 2.0f + 0.01f);
    switch (bw_div2) {
    case 3:
        s_bandwidth_reg = SX126X_LORA_BW_7_8;
        break;
    case 5:
        s_bandwidth_reg = SX126X_LORA_BW_10_4;
        break;
    case 7:
        s_bandwidth_reg = SX126X_LORA_BW_15_6;
        break;
    case 10:
        s_bandwidth_reg = SX126X_LORA_BW_20_8;
        break;
    case 15:
        s_bandwidth_reg = SX126X_LORA_BW_31_25;
        break;
    case 20:
        s_bandwidth_reg = SX126X_LORA_BW_41_7;
        break;
    case 31:
        s_bandwidth_reg = SX126X_LORA_BW_62_5;
        break;
    case 62:
        s_bandwidth_reg = SX126X_LORA_BW_125_0;
        break;
    case 125:
        s_bandwidth_reg = SX126X_LORA_BW_250_0;
        break;
    case 250:
        s_bandwidth_reg = SX126X_LORA_BW_500_0;
        break;
    default:
        return SX126X_ERR_INVALID_BW;
    }

    g_sx126x_cfg.bw_khz = bw_khz;
    return sx126x_config_set_modulation_params();
}

int16_t sx126x_config_set_coding_rate(uint8_t cr)
{
    /* cr: 4..8 (4/4 .. 4/8).  4/5 = cr5, 4/8 = cr8 */
    if (cr < 4 || cr > 8)
        return SX126X_ERR_INVALID_CR;

    /* RadioLib maps: cr=4->0, cr=5->1, cr=6->2, cr=7->3, cr=8->4 */
    s_coding_rate = cr - 4;
    g_sx126x_cfg.cr = cr;
    return sx126x_config_set_modulation_params();
}

int16_t sx126x_config_set_modulation_params(void)
{
    /* Calculate LDRO if auto */
    if (g_sx126x_cfg.ldro_auto) {
        float symbol_len = (float)(1UL << g_sx126x_cfg.sf) / g_sx126x_cfg.bw_khz;
        s_ldro = (symbol_len >= 16.0f) ? SX126X_LORA_LDRO_ON : SX126X_LORA_LDRO_OFF;
    }

    /* Convert sf to register value:
     *   SF5 = 0x05, SF6 = 0x06, ..., SF12 = 0x0C */
    uint8_t sf_reg = g_sx126x_cfg.sf;

    uint8_t data[4] = {sf_reg, s_bandwidth_reg, s_coding_rate, s_ldro};
    return sx126x_hal_cmd(SX126X_CMD_SET_MODULATION_PARAMS, data, 4);
}

/* ================================================================== */
/* Output power                                                        */
/* ================================================================== */

int16_t sx126x_config_set_output_power(int8_t power_dbm)
{
    if (power_dbm < -17 || power_dbm > 22)
        return SX126X_ERR_INVALID_POWER;

    g_sx126x_cfg.tx_power = power_dbm;

    /* Save OCP (RadioLib: save before PA config, restore after) */
    uint8_t ocp_saved = 0;
    sx126x_hal_read_reg(SX126X_REG_OCP_CONFIGURATION, &ocp_saved, 1);

    /* Set PA config first */
    int16_t state = sx126x_config_set_pa_config(power_dbm);
    if (state != SX126X_ERR_NONE)
        return state;

    /* Set TX params (power index 0..22, ramp time 200 us) */
    /* SX1262: output power = -17 + pwr_idx (0 = -17 dBm, 39 = +22 dBm) */
    uint8_t pwr_idx = (uint8_t)(power_dbm + 17);
    if (pwr_idx > 39)
        pwr_idx = 39;
    s_pwr = pwr_idx;

    uint8_t data[2] = {pwr_idx, SX126X_PA_RAMP_200U};
    state = sx126x_hal_cmd(SX126X_CMD_SET_TX_PARAMS, data, 2);

    /* Restore OCP */
    sx126x_hal_write_reg(SX126X_REG_OCP_CONFIGURATION, &ocp_saved, 1);

    return state;
}

int16_t sx126x_config_set_pa_config(int8_t power_dbm)
{
    /* Use the SX1262 optimal PA table */
    int idx = power_dbm + 17;
    if (idx < 0)
        idx = 0;
    if (idx > 31)
        idx = 31;

    const sx126x_pa_table_entry_t *entry = &pa_opt_table[idx];
    uint8_t data[4] = {entry->paDutyCycle, entry->hpMax, SX126X_PA_CONFIG_SX1262_8, SX126X_PA_CONFIG_PA_LUT};
    return sx126x_hal_cmd(SX126X_CMD_SET_PA_CONFIG, data, 4);
}

/* ================================================================== */
/* Preamble / sync word                                                */
/* ================================================================== */

int16_t sx126x_config_set_preamble_length(uint16_t symbols)
{
    g_sx126x_cfg.preamble_len = symbols;

    /* Packet params in LoRa mode */
    uint8_t crc = g_sx126x_cfg.crc_enabled ? SX126X_LORA_CRC_ON : SX126X_LORA_CRC_OFF;
    uint8_t header = g_sx126x_cfg.header_implicit ? SX126X_LORA_HEADER_IMPLICIT : SX126X_LORA_HEADER_EXPLICIT;
    uint8_t iq = g_sx126x_cfg.iq_inverted ? SX126X_LORA_IQ_INVERTED : SX126X_LORA_IQ_STANDARD;

    uint8_t data[8] = {(uint8_t)(symbols >> 8),
                       (uint8_t)(symbols & 0xFF),
                       header,
                       0xFF, /* payload_len = 0xFF default */
                       crc,
                       iq,
                       0x00,
                       0x00};
    return sx126x_hal_cmd(SX126X_CMD_SET_PACKET_PARAMS, data, 8);
}

int16_t sx126x_config_set_sync_word(uint8_t sync_word)
{
    g_sx126x_cfg.sync_word = sync_word;

    /* SX126x stores sync word differently:
     * MSB register: 0x0740 = (sync_word & 0xF0) | (control_bits >> 4)
     * LSB register: 0x0741 = ((sync_word & 0x0F) << 4) | (control_bits & 0x0F)
     * We keep control_bits = 0x44 (matching RadioLib default pattern: 0x1424) */
    uint8_t data[2] = {(uint8_t)((sync_word & 0xF0) | 0x04), (uint8_t)(((sync_word & 0x0F) << 4) | 0x04)};
    return sx126x_hal_write_reg(SX126X_REG_LORA_SYNC_WORD_MSB, data, 2);
}

/* ================================================================== */
/* Packet params                                                       */
/* ================================================================== */

int16_t sx126x_config_set_packet_params(bool crc_on, bool implicit_header, uint8_t payload_len, bool iq_inverted)
{
    g_sx126x_cfg.crc_enabled = crc_on;
    g_sx126x_cfg.header_implicit = implicit_header;
    g_sx126x_cfg.iq_inverted = iq_inverted;

    uint8_t crc = crc_on ? SX126X_LORA_CRC_ON : SX126X_LORA_CRC_OFF;
    uint8_t header = implicit_header ? SX126X_LORA_HEADER_IMPLICIT : SX126X_LORA_HEADER_EXPLICIT;
    uint8_t iq = iq_inverted ? SX126X_LORA_IQ_INVERTED : SX126X_LORA_IQ_STANDARD;

    uint8_t data[8] = {(uint8_t)(g_sx126x_cfg.preamble_len >> 8),
                       (uint8_t)(g_sx126x_cfg.preamble_len & 0xFF),
                       header,
                       payload_len,
                       crc,
                       iq,
                       0x00,
                       0x00};
    return sx126x_hal_cmd(SX126X_CMD_SET_PACKET_PARAMS, data, 8);
}

/* ================================================================== */
/* IRQ / DIO                                                           */
/* ================================================================== */

int16_t sx126x_config_set_dio_irq_params(uint16_t irq_mask, uint16_t dio1_mask, uint16_t dio2_mask, uint16_t dio3_mask)
{
    uint8_t data[8] = {(uint8_t)(irq_mask >> 8),    (uint8_t)(irq_mask & 0xFF), (uint8_t)(dio1_mask >> 8),
                       (uint8_t)(dio1_mask & 0xFF), (uint8_t)(dio2_mask >> 8),  (uint8_t)(dio2_mask & 0xFF),
                       (uint8_t)(dio3_mask >> 8),   (uint8_t)(dio3_mask & 0xFF)};
    return sx126x_hal_cmd(SX126X_CMD_SET_DIO_IRQ_PARAMS, data, 8);
}

int16_t sx126x_config_set_dio2_as_rf_switch(bool enable)
{
    uint8_t val = enable ? SX126X_DIO2_AS_RF_SWITCH : SX126X_DIO2_AS_IRQ;
    return sx126x_hal_cmd(SX126X_CMD_SET_DIO2_AS_RF_SWITCH_CTRL, &val, 1);
}

int16_t sx126x_config_set_rx_boosted_gain(bool enable)
{
    uint8_t val = enable ? SX126X_RX_GAIN_BOOSTED : SX126X_RX_GAIN_POWER_SAVING;
    int16_t state = sx126x_hal_write_reg(SX126X_REG_RX_GAIN, &val, 1);
    if (state != SX126X_ERR_NONE)
        return state;

    /* Persist RX gain in retention memory (RadioLib: setRxBoostedGainMode) */
    /* Reg 0x029F: 0x01 = enable retention for register at address 0x08AC */
    uint8_t ret[3] = {0x01, 0x08, 0xAC};
    state = sx126x_hal_write_reg(SX126X_REG_RX_GAIN_RETENTION_0, ret, 3);
    return state;
}

/* ================================================================== */
/* Regulator / Current                                                 */
/* ================================================================== */

int16_t sx126x_config_set_regulator_dcdc(void)
{
    uint8_t val = SX126X_REGULATOR_DC_DC;
    return sx126x_hal_cmd(SX126X_CMD_SET_REGULATOR_MODE, &val, 1);
}

int16_t sx126x_config_set_regulator_ldo(void)
{
    uint8_t val = SX126X_REGULATOR_LDO;
    return sx126x_hal_cmd(SX126X_CMD_SET_REGULATOR_MODE, &val, 1);
}

int16_t sx126x_config_set_current_limit(float ma)
{
    if (ma < 0.0f || ma > 140.0f)
        return SX126X_ERR_INVALID_POWER;
    uint8_t raw = (uint8_t)(ma / 2.5f);
    return sx126x_hal_write_reg(SX126X_REG_OCP_CONFIGURATION, &raw, 1);
}

/* ================================================================== */
/* Image calibration                                                   */
/* ================================================================== */

int16_t sx126x_config_calibrate_image(float freq_mhz)
{
    uint8_t data[2] = {0, 0};
    int freq_band = (int)freq_mhz;

    if (freq_band >= 902 && freq_band <= 928) {
        data[0] = SX126X_CAL_IMG_902_MHZ_1;
        data[1] = SX126X_CAL_IMG_902_MHZ_2;
    } else if (freq_band >= 863 && freq_band <= 870) {
        data[0] = SX126X_CAL_IMG_863_MHZ_1;
        data[1] = SX126X_CAL_IMG_863_MHZ_2;
    } else if (freq_band >= 779 && freq_band <= 787) {
        data[0] = SX126X_CAL_IMG_779_MHZ_1;
        data[1] = SX126X_CAL_IMG_779_MHZ_2;
    } else if (freq_band >= 470 && freq_band <= 510) {
        data[0] = SX126X_CAL_IMG_470_MHZ_1;
        data[1] = SX126X_CAL_IMG_470_MHZ_2;
    } else if (freq_band >= 430 && freq_band <= 440) {
        data[0] = SX126X_CAL_IMG_430_MHZ_1;
        data[1] = SX126X_CAL_IMG_430_MHZ_2;
    } else {
        return SX126X_ERR_NONE; /* not critical */
    }

    return sx126x_hal_cmd(SX126X_CMD_CALIBRATE_IMAGE, data, 2);
}

/* ================================================================== */
/* TX / RX                                                             */
/* ================================================================== */

int16_t sx126x_config_transmit(const uint8_t *data, size_t len)
{
    int16_t state;

    if (len > SX126X_MAX_PACKET_LENGTH)
        return SX126X_ERR_PACKET_TOO_LONG;

    /* Standby */
    state = sx126x_config_standby();
    if (state != SX126X_ERR_NONE)
        return state;

    /* Set packet params (update payload length) */
    state = sx126x_config_set_packet_params(g_sx126x_cfg.crc_enabled, g_sx126x_cfg.header_implicit, (uint8_t)len,
                                            g_sx126x_cfg.iq_inverted);
    if (state != SX126X_ERR_NONE)
        return state;

    /* IRQ: TX_DONE + TIMEOUT on DIO1 */
    state = sx126x_config_set_dio_irq_params(SX126X_IRQ_TX_DONE | SX126X_IRQ_TIMEOUT, SX126X_IRQ_TX_DONE, 0, 0);
    if (state != SX126X_ERR_NONE)
        return state;

    /* Buffer base address */
    state = sx126x_config_set_buffer_base_address();
    if (state != SX126X_ERR_NONE)
        return state;

    /* Write data to buffer */
    state = sx126x_hal_write_buffer(0, data, (uint8_t)len);
    if (state != SX126X_ERR_NONE)
        return state;

    /* Clear IRQ */
    state = sx126x_hal_clear_irq_flags(SX126X_IRQ_ALL);
    if (state != SX126X_ERR_NONE)
        return state;

    /* Fix sensitivity */
    state = sx126x_config_fix_sensitivity();
    if (state != SX126X_ERR_NONE)
        return state;

    /* Set TX with no timeout */
    state = sx126x_hal_cmd(SX126X_CMD_SET_TX, (const uint8_t[]){0x00, 0x00, 0x00}, 3);
    if (state != SX126X_ERR_NONE)
        return state;

    /* Wait for BUSY to go low (= PA ramp up done) */
    sx126x_hal_wait_on_busy();

    /* Blocking wait for TX_DONE IRQ */
    uint32_t start = millis();
    while (true) {
        if (digitalRead(PIN_LORA_DIO1)) {
            break;
        }
        if ((millis() - start) > 5000) {
            sx126x_config_standby();
            return SX126X_ERR_TX_TIMEOUT;
        }
    }

    /* Finish */
    state = sx126x_hal_clear_irq_flags(SX126X_IRQ_ALL);
    if (state != SX126X_ERR_NONE)
        return state;
    state = sx126x_config_standby();

    return state;
}

int16_t sx126x_config_receive(uint8_t *buf, size_t buf_len, uint32_t timeout_ms, int16_t *rssi, int8_t *snr)
{
    int16_t state;

    if (buf_len > SX126X_MAX_PACKET_LENGTH)
        buf_len = SX126X_MAX_PACKET_LENGTH;

    /* Standby */
    state = sx126x_config_standby();
    if (state != SX126X_ERR_NONE)
        return state;

    /* Set packet params with explicit header length for implicit mode */
    state = sx126x_config_set_packet_params(g_sx126x_cfg.crc_enabled, g_sx126x_cfg.header_implicit, (uint8_t)buf_len,
                                            g_sx126x_cfg.iq_inverted);
    if (state != SX126X_ERR_NONE)
        return state;

    /* IRQ: RX_DONE + CRC_ERR + TIMEOUT on DIO1 */
    state =
        sx126x_config_set_dio_irq_params(SX126X_IRQ_RX_DONE | SX126X_IRQ_CRC_ERR | SX126X_IRQ_TIMEOUT, SX126X_IRQ_RX_DONE, 0, 0);
    if (state != SX126X_ERR_NONE)
        return state;

    /* Buffer base address */
    state = sx126x_config_set_buffer_base_address();
    if (state != SX126X_ERR_NONE)
        return state;

    /* Clear IRQ */
    state = sx126x_hal_clear_irq_flags(SX126X_IRQ_ALL);
    if (state != SX126X_ERR_NONE)
        return state;

    /* Fix sensitivity */
    state = sx126x_config_fix_sensitivity();
    if (state != SX126X_ERR_NONE)
        return state;

    /* Calculate timeout register value (units of 15.625 us) */
    uint32_t timeout_raw = 0;
    if (timeout_ms > 0) {
        timeout_raw = (uint32_t)((float)timeout_ms * 1000.0f / 15.625f);
    } else {
        timeout_raw = SX126X_RX_TIMEOUT_INF;
    }

    /* Set RX */
    uint8_t rx_data[3] = {(uint8_t)((timeout_raw >> 16) & 0xFF), (uint8_t)((timeout_raw >> 8) & 0xFF),
                          (uint8_t)(timeout_raw & 0xFF)};
    state = sx126x_hal_cmd(SX126X_CMD_SET_RX, rx_data, 3);
    if (state != SX126X_ERR_NONE)
        return state;

    /* Blocking wait for RX_DONE IRQ or timeout */
    bool soft_timeout = false;
    uint32_t start = millis();
    while (true) {
        if (digitalRead(PIN_LORA_DIO1)) {
            break;
        }
        if ((millis() - start) > (timeout_ms > 0 ? timeout_ms + 100 : 10000)) {
            soft_timeout = true;
            break;
        }
    }

    /* Standby */
    state = sx126x_config_standby();

    /* Check timeout */
    uint16_t irq = sx126x_config_get_irq_flags();
    if (soft_timeout || (irq & SX126X_IRQ_TIMEOUT)) {
        sx126x_hal_clear_irq_flags(SX126X_IRQ_ALL);
        return SX126X_ERR_RX_TIMEOUT;
    }

    /* Read data */
    return sx126x_config_read_data(buf, buf_len, rssi, snr);
}

int16_t sx126x_config_start_receive(void)
{
    int16_t state;

    /* Standby */
    state = sx126x_config_standby();
    if (state != SX126X_ERR_NONE)
        return state;

    /* IRQ: RX_DONE + CRC_ERR + TIMEOUT on DIO1 */
    state = sx126x_config_set_dio_irq_params(SX126X_IRQ_RX_DONE | SX126X_IRQ_CRC_ERR, SX126X_IRQ_RX_DONE, 0, 0);
    if (state != SX126X_ERR_NONE)
        return state;

    /* Buffer base address */
    state = sx126x_config_set_buffer_base_address();
    if (state != SX126X_ERR_NONE)
        return state;

    /* Clear IRQ */
    state = sx126x_hal_clear_irq_flags(SX126X_IRQ_ALL);
    if (state != SX126X_ERR_NONE)
        return state;

    /* Fix sensitivity */
    state = sx126x_config_fix_sensitivity();
    if (state != SX126X_ERR_NONE)
        return state;

    /* Set RX continuous (infinite timeout) */
    uint8_t rx_data[3] = {(uint8_t)(SX126X_RX_TIMEOUT_INF >> 16), (uint8_t)((SX126X_RX_TIMEOUT_INF >> 8) & 0xFF),
                          (uint8_t)(SX126X_RX_TIMEOUT_INF & 0xFF)};
    state = sx126x_hal_cmd(SX126X_CMD_SET_RX, rx_data, 3);
    return state;
}

/* ================================================================== */
/* Buffer management                                                   */
/* ================================================================== */

int16_t sx126x_config_set_buffer_base_address(void)
{
    uint8_t data[2] = {0x00, 0x00};
    return sx126x_hal_cmd(SX126X_CMD_SET_BUFFER_BASE_ADDRESS, data, 2);
}

int16_t sx126x_config_read_data(uint8_t *buf, size_t buf_len, int16_t *rssi, int8_t *snr)
{
    int16_t state;

    /* Check IRQ for CRC error */
    uint16_t irq = sx126x_config_get_irq_flags();
    bool crc_err = (irq & SX126X_IRQ_CRC_ERR) ? true : false;
    bool header_err = (irq & SX126X_IRQ_HEADER_ERR) ? true : false;
    bool header_valid = (irq & SX126X_IRQ_HEADER_VALID) ? true : false;

    if (crc_err || (header_err && !header_valid)) {
        /* CRC mismatch or header error - read data anyway, but report error */
        state = SX126X_ERR_CRC_MISMATCH;
    } else {
        state = SX126X_ERR_NONE;
    }

    /* Get packet length and offset */
    uint8_t pkt_len = 0, offset = 0;
    int16_t ret = sx126x_hal_get_rx_buffer_status(&pkt_len, &offset);
    if (ret != SX126X_ERR_NONE)
        return ret;

    if (pkt_len > buf_len)
        pkt_len = (uint8_t)buf_len;

    /* Read payload from buffer */
    ret = sx126x_hal_read_buffer(offset, buf, pkt_len);
    if (ret != SX126X_ERR_NONE)
        return ret;

    /* Clear IRQ */
    sx126x_hal_clear_irq_flags(SX126X_IRQ_ALL);

    if (rssi)
        *rssi = (int16_t)sx126x_config_get_rssi();
    if (snr)
        *snr = (int8_t)sx126x_config_get_snr();

    /* Return CRC error if detected */
    if (state != SX126X_ERR_NONE)
        return state;

    return (int16_t)pkt_len;
}

/* ================================================================== */
/* IQ config                                                           */
/* ================================================================== */

int16_t sx126x_config_set_iq_inversion(bool invert)
{
    g_sx126x_cfg.iq_inverted = invert;
    /* Reg 0x0736: 0x0D = non-inverted, 0x1D = inverted (RadioLib) */
    uint8_t val = invert ? 0x1D : 0x0D;
    return sx126x_hal_write_reg(SX126X_REG_IQ_CONFIG, &val, 1);
}

/* ================================================================== */
/* Status queries                                                      */
/* ================================================================== */

float sx126x_config_get_rssi(void)
{
    /* Try packet RSSI first, fall back to instant RSSI */
    uint32_t pkt_status = sx126x_hal_get_packet_status();
    if (pkt_status == 0) {
        /* Instant RSSI */
        uint8_t raw = 0;
        sx126x_hal_cmd_read(SX126X_CMD_GET_RSSI_INST, &raw, 1);
        return (float)raw / (-2.0f);
    }
    /* Packet RSSI: lowest byte */
    return -1.0f * (float)(pkt_status & 0xFF) / 2.0f;
}

float sx126x_config_get_snr(void)
{
    /* Only valid in LoRa mode */
    uint32_t pkt_status = sx126x_hal_get_packet_status();
    if (pkt_status == 0)
        return 0.0f;

    /* SNR is second byte */
    uint8_t snr_raw = (uint8_t)(pkt_status >> 8) & 0xFF;
    if (snr_raw < 128) {
        return snr_raw / 4.0f;
    } else {
        return (snr_raw - 256) / 4.0f;
    }
}

uint16_t sx126x_config_get_irq_flags(void)
{
    return sx126x_hal_get_irq_flags();
}

int16_t sx126x_config_clear_irq_flags(void)
{
    return sx126x_hal_clear_irq_flags(SX126X_IRQ_ALL);
}

size_t sx126x_config_get_packet_length(void)
{
    uint8_t len = 0, offset = 0;
    sx126x_hal_get_rx_buffer_status(&len, &offset);
    return (size_t)len;
}

uint8_t sx126x_config_get_packet_type(void)
{
    uint8_t pt = 0xFF;
    sx126x_hal_cmd_read(SX126X_CMD_GET_PACKET_TYPE, &pt, 1);
    s_packet_type = pt;
    return pt;
}

int16_t sx126x_config_get_device_errors(void)
{
    return sx126x_hal_get_device_errors();
}

/* ================================================================== */
/* Airtime calculation (standard LoRa formula)                        */
/* ================================================================== */

uint32_t sx126x_config_calc_airtime_us(uint16_t payload_len, const sx126x_modem_config_t *cfg)
{
    float bw_hz = cfg->bw_khz * 1000.0f;
    uint32_t sf = cfg->sf;
    uint8_t cr = cfg->cr;
    bool crc_enabled = cfg->crc_enabled;
    bool de = ((uint32_t)(1UL << sf) / (uint32_t)bw_hz) >= 16;
    uint16_t preamble = cfg->preamble_len;

    float tsym = (float)(1UL << sf) / bw_hz;
    uint32_t tpreamble_us = (uint32_t)((preamble + 4.25f) * tsym * 1e6f);

    int16_t sf_bits = (int16_t)sf;
    float tmp = (float)(8 * (int16_t)payload_len - 4 * sf_bits + 28 + (crc_enabled ? 16 : 0));
    tmp /= (float)(4 * (sf_bits - (de ? 2 : 0)));
    int16_t payload_sym = 8 + (int16_t)(tmp + 0.999f);
    payload_sym *= (int16_t)(cr + 4);
    uint32_t tpayload_us = (uint32_t)(payload_sym * tsym * 1e6f);

    return tpreamble_us + tpayload_us;
}

/* ================================================================== */
/* Hardware workarounds                                                */
/* ================================================================== */

int16_t sx126x_config_fix_sensitivity(void)
{
    /* SX1262/SX1268 datasheet, chapter 15, section 15.1 */
    uint8_t sens_cfg = 0;
    int16_t state = sx126x_hal_read_reg(SX126X_REG_SENSITIVITY_CONFIG, &sens_cfg, 1);
    if (state != SX126X_ERR_NONE)
        return state;

    if (g_sx126x_cfg.bw_khz >= 500.0f - 0.001f) {
        sens_cfg &= 0xFB;
    } else {
        sens_cfg |= 0x04;
    }

    return sx126x_hal_write_reg(SX126X_REG_SENSITIVITY_CONFIG, &sens_cfg, 1);
}

int16_t sx126x_config_fix_pa_clamping(bool enable)
{
    /* SX1262/SX1268 datasheet, chapter 15, section 15.2.
     *
     * Read register (status may be CMD_FAILED but data is valid),
     * then set bits 4-1 to enable the workaround.
     * RadioLib uses the same read-modify-write approach. */
    uint8_t clamp = 0;
    (void)sx126x_hal_read_reg(SX126X_REG_TX_CLAMP_CONFIG, &clamp, 1);

    if (enable) {
        clamp |= 0x1E;
    } else {
        clamp = (clamp & ~0x1E) | 0x08;
    }

    return sx126x_hal_write_reg(SX126X_REG_TX_CLAMP_CONFIG, &clamp, 1);
}

#endif /* PIO_ENABLE_LORA_RA01S */
