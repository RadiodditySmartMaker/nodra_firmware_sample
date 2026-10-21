#pragma once

#include <stdint.h>

using button_release_callback_t = void (*)(uint8_t button_index);

void button_setup();
void button_loop();
bool button_register_release_callback(uint8_t button_index, button_release_callback_t callback);
