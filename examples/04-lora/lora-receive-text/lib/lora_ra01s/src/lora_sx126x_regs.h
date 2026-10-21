/*
 * SX126x register addresses, SPI command codes, IRQ flags and enums.
 *
 * Extracted from RadioLib (https://github.com/jgromes/RadioLib)
 * SX126x_commands.h / SX126x_registers.h, prefixed RADIOLIB_ stripped.
 */

#ifndef LORA_SX126X_REGS_H_
#define LORA_SX126X_REGS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================== */
/* Status codes (negative = error)                                     */
/* ================================================================== */
#define SX126X_ERR_NONE 0
#define SX126X_ERR_SPI_CMD_TIMEOUT -1
#define SX126X_ERR_SPI_CMD_INVALID -2
#define SX126X_ERR_SPI_CMD_FAILED -3
#define SX126X_ERR_CHIP_NOT_FOUND -4
#define SX126X_ERR_INVALID_BW -10
#define SX126X_ERR_INVALID_SF -11
#define SX126X_ERR_INVALID_CR -12
#define SX126X_ERR_INVALID_POWER -13
#define SX126X_ERR_INVALID_FREQ -14
#define SX126X_ERR_INVALID_PREAMBLE -15
#define SX126X_ERR_PACKET_TOO_LONG -16
#define SX126X_ERR_TX_TIMEOUT -17
#define SX126X_ERR_RX_TIMEOUT -18
#define SX126X_ERR_CRC_MISMATCH -19
#define SX126X_ERR_WRONG_MODEM -20
#define SX126X_ERR_UNKNOWN -99

/* ================================================================== */
/* Physical constants                                                  */
/* ================================================================== */
#define SX126X_FREQUENCY_STEP_SIZE 0.9536743164f
#define SX126X_MAX_PACKET_LENGTH 255
#define SX126X_CRYSTAL_FREQ 32.0f
#define SX126X_DIV_EXPONENT 25

/* ================================================================== */
/* SPI commands                                                        */
/* ================================================================== */
#define SX126X_CMD_NOP 0x00
#define SX126X_CMD_SET_SLEEP 0x84
#define SX126X_CMD_SET_STANDBY 0x80
#define SX126X_CMD_SET_FS 0xC1
#define SX126X_CMD_SET_TX 0x83
#define SX126X_CMD_SET_RX 0x82
#define SX126X_CMD_STOP_TIMER_ON_PREAMBLE 0x9F
#define SX126X_CMD_SET_RX_DUTY_CYCLE 0x94
#define SX126X_CMD_SET_CAD 0xC5
#define SX126X_CMD_SET_TX_CONTINUOUS_WAVE 0xD1
#define SX126X_CMD_SET_TX_INFINITE_PREAMBLE 0xD2
#define SX126X_CMD_SET_REGULATOR_MODE 0x96
#define SX126X_CMD_CALIBRATE 0x89
#define SX126X_CMD_CALIBRATE_IMAGE 0x98
#define SX126X_CMD_SET_PA_CONFIG 0x95
#define SX126X_CMD_SET_RX_TX_FALLBACK_MODE 0x93
#define SX126X_CMD_WRITE_REGISTER 0x0D
#define SX126X_CMD_READ_REGISTER 0x1D
#define SX126X_CMD_WRITE_BUFFER 0x0E
#define SX126X_CMD_READ_BUFFER 0x1E
#define SX126X_CMD_SET_DIO_IRQ_PARAMS 0x08
#define SX126X_CMD_GET_IRQ_STATUS 0x12
#define SX126X_CMD_CLEAR_IRQ_STATUS 0x02
#define SX126X_CMD_SET_DIO2_AS_RF_SWITCH_CTRL 0x9D
#define SX126X_CMD_SET_DIO3_AS_TCXO_CTRL 0x97
#define SX126X_CMD_SET_RF_FREQUENCY 0x86
#define SX126X_CMD_SET_PACKET_TYPE 0x8A
#define SX126X_CMD_GET_PACKET_TYPE 0x11
#define SX126X_CMD_SET_TX_PARAMS 0x8E
#define SX126X_CMD_SET_MODULATION_PARAMS 0x8B
#define SX126X_CMD_SET_PACKET_PARAMS 0x8C
#define SX126X_CMD_SET_CAD_PARAMS 0x88
#define SX126X_CMD_SET_BUFFER_BASE_ADDRESS 0x8F
#define SX126X_CMD_SET_LORA_SYMB_NUM_TIMEOUT 0xA0
#define SX126X_CMD_GET_STATUS 0xC0
#define SX126X_CMD_GET_RSSI_INST 0x15
#define SX126X_CMD_GET_RX_BUFFER_STATUS 0x13
#define SX126X_CMD_GET_PACKET_STATUS 0x14
#define SX126X_CMD_GET_DEVICE_ERRORS 0x17
#define SX126X_CMD_CLEAR_DEVICE_ERRORS 0x07
#define SX126X_CMD_GET_STATS 0x10
#define SX126X_CMD_RESET_STATS 0x00

/* ================================================================== */
/* SetSleep parameters                                                 */
/* ================================================================== */
#define SX126X_SLEEP_START_COLD 0x00
#define SX126X_SLEEP_START_WARM 0x04
#define SX126X_SLEEP_RTC_OFF 0x00
#define SX126X_SLEEP_RTC_ON 0x01

/* ================================================================== */
/* SetStandby parameters                                               */
/* ================================================================== */
#define SX126X_STANDBY_RC 0x00
#define SX126X_STANDBY_XOSC 0x01

/* ================================================================== */
/* SetRx / SetTx timeout                                               */
/* ================================================================== */
#define SX126X_RX_TIMEOUT_NONE 0x000000
#define SX126X_RX_TIMEOUT_INF 0xFFFFFF
#define SX126X_TX_TIMEOUT_NONE 0x000000

/* ================================================================== */
/* Regulator mode                                                      */
/* ================================================================== */
#define SX126X_REGULATOR_LDO 0x00
#define SX126X_REGULATOR_DC_DC 0x01

/* ================================================================== */
/* Calibrate                                                           */
/* ================================================================== */
#define SX126X_CALIBRATE_ALL 0b01111111

/* ================================================================== */
/* Image calibration frequency bands                                   */
/* ================================================================== */
#define SX126X_CAL_IMG_430_MHZ_1 0x6B
#define SX126X_CAL_IMG_430_MHZ_2 0x6F
#define SX126X_CAL_IMG_470_MHZ_1 0x75
#define SX126X_CAL_IMG_470_MHZ_2 0x81
#define SX126X_CAL_IMG_779_MHZ_1 0xC1
#define SX126X_CAL_IMG_779_MHZ_2 0xC5
#define SX126X_CAL_IMG_863_MHZ_1 0xD7
#define SX126X_CAL_IMG_863_MHZ_2 0xDB
#define SX126X_CAL_IMG_902_MHZ_1 0xE1
#define SX126X_CAL_IMG_902_MHZ_2 0xE9
#define SX126X_CAL_IMG_FREQ_TRIG_MHZ 20.0f

/* ================================================================== */
/* PA config                                                           */
/* ================================================================== */
#define SX126X_PA_CONFIG_HP_MAX 0x07
#define SX126X_PA_CONFIG_PA_LUT 0x01
#define SX126X_PA_CONFIG_SX1262_8 0x00

/* ================================================================== */
/* Rx/Tx fallback mode                                                 */
/* ================================================================== */
#define SX126X_RX_TX_FALLBACK_FS 0x40
#define SX126X_RX_TX_FALLBACK_STDBY_XOSC 0x30
#define SX126X_RX_TX_FALLBACK_STDBY_RC 0x20

/* ================================================================== */
/* IRQ flags                                                           */
/* ================================================================== */
#define SX126X_IRQ_LR_FHSS_HOP 0x4000
#define SX126X_IRQ_TIMEOUT 0x0200
#define SX126X_IRQ_CAD_DETECTED 0x0100
#define SX126X_IRQ_CAD_DONE 0x0080
#define SX126X_IRQ_CRC_ERR 0x0040
#define SX126X_IRQ_HEADER_ERR 0x0020
#define SX126X_IRQ_HEADER_VALID 0x0010
#define SX126X_IRQ_SYNC_WORD_VALID 0x0008
#define SX126X_IRQ_PREAMBLE_DETECTED 0x0004
#define SX126X_IRQ_RX_DONE 0x0002
#define SX126X_IRQ_TX_DONE 0x0001
#define SX126X_IRQ_ALL 0x43FF
#define SX126X_IRQ_NONE 0x0000

/* ================================================================== */
/* DIO2 config                                                         */
/* ================================================================== */
#define SX126X_DIO2_AS_IRQ 0x00
#define SX126X_DIO2_AS_RF_SWITCH 0x01

/* ================================================================== */
/* DIO3 TCXO voltage                                                   */
/* ================================================================== */
#define SX126X_DIO3_OUTPUT_1_6V 0x00
#define SX126X_DIO3_OUTPUT_1_7V 0x01
#define SX126X_DIO3_OUTPUT_1_8V 0x02
#define SX126X_DIO3_OUTPUT_2_2V 0x03
#define SX126X_DIO3_OUTPUT_2_4V 0x04
#define SX126X_DIO3_OUTPUT_2_7V 0x05
#define SX126X_DIO3_OUTPUT_3_0V 0x06
#define SX126X_DIO3_OUTPUT_3_3V 0x07

/* ================================================================== */
/* Packet types                                                        */
/* ================================================================== */
#define SX126X_PACKET_TYPE_GFSK 0x00
#define SX126X_PACKET_TYPE_LORA 0x01
#define SX126X_PACKET_TYPE_BPSK 0x02
#define SX126X_PACKET_TYPE_LR_FHSS 0x03

/* ================================================================== */
/* TX params (ramp time)                                               */
/* ================================================================== */
#define SX126X_PA_RAMP_10U 0x00
#define SX126X_PA_RAMP_20U 0x01
#define SX126X_PA_RAMP_40U 0x02
#define SX126X_PA_RAMP_80U 0x03
#define SX126X_PA_RAMP_200U 0x04
#define SX126X_PA_RAMP_800U 0x05
#define SX126X_PA_RAMP_1700U 0x06
#define SX126X_PA_RAMP_3400U 0x07

/* ================================================================== */
/* LoRa modulation parameters                                          */
/* ================================================================== */
#define SX126X_LORA_BW_7_8 0x00
#define SX126X_LORA_BW_10_4 0x08
#define SX126X_LORA_BW_15_6 0x01
#define SX126X_LORA_BW_20_8 0x09
#define SX126X_LORA_BW_31_25 0x02
#define SX126X_LORA_BW_41_7 0x0A
#define SX126X_LORA_BW_62_5 0x03
#define SX126X_LORA_BW_125_0 0x04
#define SX126X_LORA_BW_250_0 0x05
#define SX126X_LORA_BW_500_0 0x06

#define SX126X_LORA_CR_4_5 0x01
#define SX126X_LORA_CR_4_6 0x02
#define SX126X_LORA_CR_4_7 0x03
#define SX126X_LORA_CR_4_8 0x04
#define SX126X_LORA_CR_4_5_LI 0x05
#define SX126X_LORA_CR_4_6_LI 0x06
#define SX126X_LORA_CR_4_8_LI 0x07

#define SX126X_LORA_LDRO_OFF 0x00
#define SX126X_LORA_LDRO_ON 0x01

/* ================================================================== */
/* LoRa packet parameters                                              */
/* ================================================================== */
#define SX126X_LORA_HEADER_EXPLICIT 0x00
#define SX126X_LORA_HEADER_IMPLICIT 0x01
#define SX126X_LORA_CRC_OFF 0x00
#define SX126X_LORA_CRC_ON 0x01
#define SX126X_LORA_IQ_STANDARD 0x00
#define SX126X_LORA_IQ_INVERTED 0x01

/* ================================================================== */
/* GFSK modulation/packet parameters                                   */
/* ================================================================== */
#define SX126X_GFSK_FILTER_NONE 0x00
#define SX126X_GFSK_FILTER_GAUSS_0_3 0x08
#define SX126X_GFSK_FILTER_GAUSS_0_5 0x09
#define SX126X_GFSK_FILTER_GAUSS_0_7 0x0A
#define SX126X_GFSK_FILTER_GAUSS_1 0x0B

/* ================================================================== */
/* CAD parameters                                                      */
/* ================================================================== */
#define SX126X_CAD_ON_1_SYMB 0x00
#define SX126X_CAD_ON_2_SYMB 0x01
#define SX126X_CAD_ON_4_SYMB 0x02
#define SX126X_CAD_ON_8_SYMB 0x03
#define SX126X_CAD_ON_16_SYMB 0x04
#define SX126X_CAD_GOTO_STDBY 0x00
#define SX126X_CAD_GOTO_RX 0x01

/* ================================================================== */
/* Status (GetStatus command response)                                 */
/* ================================================================== */
#define SX126X_STATUS_MODE_STDBY_RC 0b00100000
#define SX126X_STATUS_MODE_STDBY_XOSC 0b00110000
#define SX126X_STATUS_MODE_FS 0b01000000
#define SX126X_STATUS_MODE_RX 0b01010000
#define SX126X_STATUS_MODE_TX 0b01100000
#define SX126X_STATUS_DATA_AVAILABLE 0b00000100
#define SX126X_STATUS_CMD_TIMEOUT 0b00000110
#define SX126X_STATUS_CMD_INVALID 0b00001000
#define SX126X_STATUS_CMD_FAILED 0b00001010
#define SX126X_STATUS_TX_DONE 0b00001100

/* ================================================================== */
/* Register addresses                                                  */
/* ================================================================== */
#define SX126X_REG_RX_GAIN_RETENTION_0 0x029F
#define SX126X_REG_RX_GAIN_RETENTION_1 0x02A0
#define SX126X_REG_RX_GAIN_RETENTION_2 0x02A1
#define SX126X_REG_VERSION_STRING 0x0320
#define SX126X_REG_HOPPING_ENABLE 0x0385
#define SX126X_REG_DIOX_OUT_ENABLE 0x0580
#define SX126X_REG_DIOX_DRIVE_STRENGTH 0x0582
#define SX126X_REG_DIOX_IN_ENABLE 0x0583
#define SX126X_REG_DIOX_PULL_UP_CTRL 0x0584
#define SX126X_REG_DIOX_PULL_DOWN_CTRL 0x0585
#define SX126X_REG_TX_BITBANG_ENABLE_0 0x0587
#define SX126X_REG_PATCH_UPDATE_ENABLE 0x0610
#define SX126X_REG_TX_BITBANG_ENABLE_1 0x0680
#define SX126X_REG_WHITENING_INITIAL_MSB 0x06B8
#define SX126X_REG_WHITENING_INITIAL_LSB 0x06B9
#define SX126X_REG_RX_TX_PLD_LEN 0x06BB
#define SX126X_REG_CRC_INITIAL_MSB 0x06BC
#define SX126X_REG_CRC_INITIAL_LSB 0x06BD
#define SX126X_REG_CRC_POLYNOMIAL_MSB 0x06BE
#define SX126X_REG_CRC_POLYNOMIAL_LSB 0x06BF
#define SX126X_REG_SYNC_WORD_0 0x06C0
#define SX126X_REG_SYNC_WORD_1 0x06C1
#define SX126X_REG_SYNC_WORD_2 0x06C2
#define SX126X_REG_SYNC_WORD_3 0x06C3
#define SX126X_REG_SYNC_WORD_4 0x06C4
#define SX126X_REG_SYNC_WORD_5 0x06C5
#define SX126X_REG_SYNC_WORD_6 0x06C6
#define SX126X_REG_SYNC_WORD_7 0x06C7
#define SX126X_REG_NODE_ADDRESS 0x06CD
#define SX126X_REG_BROADCAST_ADDRESS 0x06CE
#define SX126X_REG_PAYLOAD_LENGTH 0x0702
#define SX126X_REG_PACKET_PARAMS 0x0704
#define SX126X_REG_LORA_SYNC_TIMEOUT 0x0706
#define SX126X_REG_IQ_CONFIG 0x0736
#define SX126X_REG_LORA_SYNC_WORD_MSB 0x0740
#define SX126X_REG_LORA_SYNC_WORD_LSB 0x0741
#define SX126X_REG_LORA_RX_CODING_RATE 0x0749
#define SX126X_REG_FREQ_ERROR_RX_CRC 0x076B
#define SX126X_REG_SPECTRAL_SCAN_STATUS 0x07CD
#define SX126X_REG_RX_ADDR_PTR 0x0803
#define SX126X_REG_RANDOM_NUMBER_0 0x0819
#define SX126X_REG_RANDOM_NUMBER_1 0x081A
#define SX126X_REG_RANDOM_NUMBER_2 0x081B
#define SX126X_REG_RANDOM_NUMBER_3 0x081C
#define SX126X_REG_SENSITIVITY_CONFIG 0x0889
#define SX126X_REG_RF_FREQUENCY_0 0x088B
#define SX126X_REG_RF_FREQUENCY_1 0x088C
#define SX126X_REG_RF_FREQUENCY_2 0x088D
#define SX126X_REG_RF_FREQUENCY_3 0x088E
#define SX126X_REG_RSSI_AVG_WINDOW 0x089B
#define SX126X_REG_RX_GAIN 0x08AC
#define SX126X_REG_TX_CLAMP_CONFIG 0x08D8
#define SX126X_REG_ANA_LNA 0x08E2
#define SX126X_REG_LNA_CAP_TUNE_N 0x08E3
#define SX126X_REG_LNA_CAP_TUNE_P 0x08E4
#define SX126X_REG_ANA_MIXER 0x08E5
#define SX126X_REG_OCP_CONFIGURATION 0x08E7
#define SX126X_REG_RTC_CTRL 0x0902
#define SX126X_REG_XTA_TRIM 0x0911
#define SX126X_REG_XTB_TRIM 0x0912
#define SX126X_REG_DIO3_OUT_VOLTAGE_CTRL 0x0920
#define SX126X_REG_EVENT_MASK 0x0944

/* ================================================================== */
/* Register field values                                               */
/* ================================================================== */
#define SX126X_SYNC_WORD_PUBLIC 0x34
#define SX126X_SYNC_WORD_PRIVATE 0x12

#define SX126X_RX_GAIN_BOOSTED 0x96
#define SX126X_RX_GAIN_POWER_SAVING 0x94

#define SX126X_LNA_RNG_DISABLED 0x01
#define SX126X_LNA_RNG_ENABLED 0x00
#define SX126X_MIXER_RNG_DISABLED 0x01
#define SX126X_MIXER_RNG_ENABLED 0x00

/* ================================================================== */
/* SX1262 chip version string                                          */
/* ================================================================== */
#define SX1262_CHIP_TYPE "SX1261"

/* ================================================================== */
/* Error/device error flags                                            */
/* ================================================================== */
#define SX126X_DEVICE_ERROR_PA_RAMP 0x100
#define SX126X_DEVICE_ERROR_PLL_LOCK 0x040
#define SX126X_DEVICE_ERROR_XOSC_START 0x020
#define SX126X_DEVICE_ERROR_IMG_CALIB 0x010
#define SX126X_DEVICE_ERROR_ADC_CALIB 0x008
#define SX126X_DEVICE_ERROR_PLL_CALIB 0x004
#define SX126X_DEVICE_ERROR_RC13M_CALIB 0x002
#define SX126X_DEVICE_ERROR_RC64K_CALIB 0x001

/* ================================================================== */
/* PA table entry for SX1262                                           */
/* ================================================================== */
typedef struct {
    uint8_t paDutyCycle : 4;
    uint8_t hpMax : 4;
    int8_t paVal;
} sx126x_pa_table_entry_t;

/* ================================================================== */
/* Modem configuration state (LoRa)                                    */
/* ================================================================== */
typedef struct {
    float frequency;       /* MHz */
    uint8_t sf;            /* 5..12 */
    float bw_khz;          /* 7.8..500 */
    uint8_t cr;            /* 5=4/5..8=4/8 */
    int8_t tx_power;       /* dBm, -17..22 */
    uint16_t preamble_len; /* symbols */
    uint8_t sync_word;     /* 1 byte */
    bool crc_enabled;      /* LoRa CRC */
    bool ldro_auto;        /* auto-calculate low data rate optimize */
    bool header_implicit;  /* implicit header mode */
    bool iq_inverted;
} sx126x_modem_config_t;

#ifdef __cplusplus
}
#endif

#endif /* LORA_SX126X_REGS_H_ */
