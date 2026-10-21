/*
 * Layer 5 — SX126x module entry point.
 *
 * Provides the project-standard lora_ra01s_setup() / lora_ra01s_loop()
 * interface with PioThread integration.
 *
 * This is the only header that main.cpp includes:
 *   #include <lora_ra01s.h>
 *   lora_ra01s_setup();
 *   lora_ra01s_loop();
 */

#ifndef LORA_SX126X_H_
#define LORA_SX126X_H_

#include <stdint.h>
#include <stddef.h>

#if defined(PIO_ENABLE_LORA_RA01S)

void lora_ra01s_setup(void);
int32_t lora_ra01s_loop(void);

/* Minimal public API for standalone LoRa applications. */
int lora_ra01s_start(void);
int lora_ra01s_send(const uint8_t *data, size_t len);

#endif /* PIO_ENABLE_LORA_RA01S */

#endif /* LORA_SX126X_H_ */
