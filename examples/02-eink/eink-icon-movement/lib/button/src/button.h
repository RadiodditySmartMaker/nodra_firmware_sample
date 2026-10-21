#ifndef BUTTON_H_
#define BUTTON_H_

#if defined(PIO_ENABLE_BUTTON)

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Button event types                                                     */
/* ------------------------------------------------------------------ */

typedef enum {
    BTN_EVENT_PRESS_DOWN = 0,
    BTN_EVENT_PRESS_UP,
    BTN_EVENT_SINGLE_CLICK,
    BTN_EVENT_DOUBLE_CLICK,
    BTN_EVENT_LONG_PRESS_START,
    BTN_EVENT_LONG_PRESS_UP,
    BTN_EVENT_VERY_LONG_PRESS_START,
    BTN_EVENT_VERY_LONG_PRESS_UP,
    BTN_EVENT_MAX,
} btn_event_t;

/* Button event callback signature */
typedef void (*btn_cb_t)(uint8_t btn_idx, btn_event_t event, void *user_data);

/* ------------------------------------------------------------------ */
/* Public API                                                             */
/* ------------------------------------------------------------------ */

/**
 * Register a callback for a specific button event.
 * Only one callback per (button, event) pair. Pass NULL to unregister.
 *
 * @param btn_idx   Button index (0..5).
 * @param event     Event type.
 * @param cb        Callback function, or NULL.
 * @param user_data Optional user data passed to callback.
 * @return 0 on success, -1 if parameters out of range.
 */
int btn_register_cb(uint8_t btn_idx, btn_event_t event, btn_cb_t cb, void *user_data);

/**
 * Read current logical pressed state of a button.
 *
 * @param btn_idx Button index (0..5).
 * @return true if pressed, false if released or invalid index.
 */
bool btn_is_pressed(uint8_t btn_idx);

/* ── Module lifecycle ── */
void button_setup(void);
int32_t button_loop(void);

#ifdef __cplusplus
}
#endif

#endif /* PIO_ENABLE_BUTTON */

#endif /* BUTTON_H_ */
