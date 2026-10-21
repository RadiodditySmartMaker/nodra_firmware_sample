/*
 * nodara TinyUSB config wrapper.
 *
 * Adafruit nrf tusb_config_nrf.h enables USB Host (MAX3421) by default;
 * this board uses CDC Device only (shell_module).  Include the stock nrf
 * config, then override Host macros before tusb_option.h consumes them.
 */

#ifndef TUSB_CONFIG_NODARA_H_
#define TUSB_CONFIG_NODARA_H_

#include "arduino/ports/nrf/tusb_config_nrf.h"

#undef CFG_TUH_ENABLED
#define CFG_TUH_ENABLED 0

#undef CFG_TUH_MAX3421
#define CFG_TUH_MAX3421 0

#endif /* TUSB_CONFIG_NODARA_H_ */
