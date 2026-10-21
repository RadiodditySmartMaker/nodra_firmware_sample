#include "button.h"

#if defined(PIO_ENABLE_BUTTON)

#include "variant.h"
#include <Arduino.h>
#include <string.h>

#if defined(PIO_ENABLE_SHELL_MODULE)
#include <shell_module.h>
#endif /* PIO_ENABLE_SHELL_MODULE */

#if defined(PIO_ENABLE_ARDUINO_THREADS)
#include <arduino_threads.h>
#endif /* PIO_ENABLE_ARDUINO_THREADS */

/* ------------------------------------------------------------------ */
/* Timing constants                                                      */
/* ------------------------------------------------------------------ */

#define BTN_DEBOUNCE_MS 20
#define BTN_DOUBLE_CLICK_MS 300
#define BTN_LONG_PRESS_MS 1500
#define BTN_VERY_LONG_PRESS_MS 5000
#define BTN_POLL_MS 10
#define NUM_BTNS 6

/* ------------------------------------------------------------------ */
/* Pin mapping — from variant.h                                         */
/* ------------------------------------------------------------------ */

static const uint8_t btn_pins[NUM_BTNS] = {
    PIN_BTN_F1, /* P1.11 idx=0 */
    PIN_BTN_F2, /* P0.09 idx=1 [NFC1] */
    PIN_BTN_F3, /* P0.06 idx=2 */
    PIN_BTN_F4, /* P0.08 idx=3 */
    PIN_BTN_F5, /* P0.10 idx=4 [NFC2] */
    PIN_BTN_F6, /* P0.11 idx=5 */
};

/* ------------------------------------------------------------------ */
/* Event names for logging                                               */
/* ------------------------------------------------------------------ */

#if defined(PIO_ENABLE_SHELL_MODULE)
static const char *const event_str[BTN_EVENT_MAX] = {
    [BTN_EVENT_PRESS_DOWN] = "PRESS_DOWN",
    [BTN_EVENT_PRESS_UP] = "PRESS_UP",
    [BTN_EVENT_SINGLE_CLICK] = "SINGLE_CLICK",
    [BTN_EVENT_DOUBLE_CLICK] = "DOUBLE_CLICK",
    [BTN_EVENT_LONG_PRESS_START] = "LONG_PRESS_START",
    [BTN_EVENT_LONG_PRESS_UP] = "LONG_PRESS_UP",
    [BTN_EVENT_VERY_LONG_PRESS_START] = "VERY_LONG_PRESS_START",
    [BTN_EVENT_VERY_LONG_PRESS_UP] = "VERY_LONG_PRESS_UP",
};
#endif /* PIO_ENABLE_SHELL_MODULE */

/* ------------------------------------------------------------------ */
/* State machine types                                                   */
/* ------------------------------------------------------------------ */

typedef enum {
    BTN_STATE_IDLE = 0,
    BTN_STATE_PRESSED,
    BTN_STATE_WAIT_DOUBLE,
    BTN_STATE_LONG_PRESS,
    BTN_STATE_VERY_LONG_PRESS,
} btn_state_t;

typedef struct {
    btn_cb_t cb;
    void *user_data;
} btn_cb_info_t;

typedef struct {
    int pin;
    volatile bool triggered; /* set by ISR, cleared by PioThread */
    btn_state_t state;
    uint32_t debounce_start_ms; /* 0 = no debounce in progress */
    bool debounce_last_level;   /* digitalRead at debounce start */
    uint32_t press_time_ms;
    uint32_t release_time_ms;
    uint8_t click_count;
    btn_cb_info_t cbs[BTN_EVENT_MAX];
} btn_ctx_t;

static btn_ctx_t btns[NUM_BTNS];

/* ------------------------------------------------------------------ */
/* Event dispatch + logging                                              */
/* ------------------------------------------------------------------ */

static void btn_dispatch(btn_ctx_t *btn, uint8_t idx, btn_event_t event)
{
#if defined(PIO_ENABLE_SHELL_MODULE)
    dbg_log("button", "BTN %u: %s", idx, event_str[event]);
#else
    (void)idx;
#endif /* PIO_ENABLE_SHELL_MODULE */

    if (btn->cbs[event].cb) {
        btn->cbs[event].cb(idx, event, btn->cbs[event].user_data);
    }
}

/* ------------------------------------------------------------------ */
/* State machine — called after debounce confirms pin state             */
/* ------------------------------------------------------------------ */

/**
 * State machine transitions (per-button):
 *
 *   IDLE ──pressed──→ PRESSED ──released──→ WAIT_DOUBLE ──timeout──→ emit click(s)
 *    ↑                   │                                              │
 *    │                   ├──held≥1.5s──→ LONG_PRESS                     │
 *    │                   │                  │                           │
 *    │                   │                  ├──released──→ IDLE         │
 *    │                   │                  └──held≥5s──→ VERY_LONG     │
 *    │                   │                                  │           │
 *    │                   │                                  └──released │
 *    │                   │                                              │
 *    └───────────────────┴──────────────────────────────────────────────┘
 *         (WAIT_DOUBLE timeouts always return to IDLE)
 */
static void btn_process(btn_ctx_t *btn, uint8_t idx, bool pressed, uint32_t now)
{
    switch (btn->state) {

    case BTN_STATE_IDLE:
        if (pressed) {
            btn->state = BTN_STATE_PRESSED;
            btn->press_time_ms = now;
            btn->click_count = 0;
            btn_dispatch(btn, idx, BTN_EVENT_PRESS_DOWN);
        }
        break;

    case BTN_STATE_PRESSED:
        if (!pressed) {
            /* Released before long press threshold */
            btn->release_time_ms = now;
            btn->click_count++;
            btn_dispatch(btn, idx, BTN_EVENT_PRESS_UP);
            btn->state = BTN_STATE_WAIT_DOUBLE;
        } else {
            /* Check for long press timeout */
            uint32_t held = now - btn->press_time_ms;
            if (held >= BTN_LONG_PRESS_MS) {
                btn->state = BTN_STATE_LONG_PRESS;
                btn_dispatch(btn, idx, BTN_EVENT_LONG_PRESS_START);
            }
        }
        break;

    case BTN_STATE_WAIT_DOUBLE:
        if (pressed) {
            /* Second press within double-click window */
            btn->press_time_ms = now;
            btn->state = BTN_STATE_PRESSED;
            btn_dispatch(btn, idx, BTN_EVENT_PRESS_DOWN);
        } else {
            /* Either window expired, or bounce resolved back to released.
             * Use elapsed time since last release to decide. */
            uint32_t elapsed = now - btn->release_time_ms;
            if (elapsed >= BTN_DOUBLE_CLICK_MS) {
                if (btn->click_count == 1) {
                    btn_dispatch(btn, idx, BTN_EVENT_SINGLE_CLICK);
                } else if (btn->click_count >= 2) {
                    btn_dispatch(btn, idx, BTN_EVENT_DOUBLE_CLICK);
                }
                btn->click_count = 0;
                btn->state = BTN_STATE_IDLE;
            }
        }
        break;

    case BTN_STATE_LONG_PRESS:
        if (!pressed) {
            btn_dispatch(btn, idx, BTN_EVENT_PRESS_UP);
            btn_dispatch(btn, idx, BTN_EVENT_LONG_PRESS_UP);
            btn->state = BTN_STATE_IDLE;
        } else {
            uint32_t held = now - btn->press_time_ms;
            if (held >= BTN_VERY_LONG_PRESS_MS) {
                btn->state = BTN_STATE_VERY_LONG_PRESS;
                btn_dispatch(btn, idx, BTN_EVENT_VERY_LONG_PRESS_START);
            }
        }
        break;

    case BTN_STATE_VERY_LONG_PRESS:
        if (!pressed) {
            btn_dispatch(btn, idx, BTN_EVENT_PRESS_UP);
            btn_dispatch(btn, idx, BTN_EVENT_LONG_PRESS_UP);
            btn_dispatch(btn, idx, BTN_EVENT_VERY_LONG_PRESS_UP);
            btn->state = BTN_STATE_IDLE;
        }
        break;
    }
}

/* ------------------------------------------------------------------ */
/* PioThread — periodic state machine driver                             */
/* ------------------------------------------------------------------ */

#if defined(PIO_ENABLE_ARDUINO_THREADS)

class ButtonThread : public PioThread
{
  public:
    ButtonThread() : PioThread("btn", BTN_POLL_MS) { traceLog = false; }

  protected:
    int32_t runOnce() override { return button_loop(); }
};

static ButtonThread *button_thread;

#endif /* PIO_ENABLE_ARDUINO_THREADS */

/* ------------------------------------------------------------------ */
/* PORT event callback — shared by all buttons                         */
/* ------------------------------------------------------------------ */

/* registerGpiotePortPin / enableGpiotePortCallback are defined in
 * GpioteOverride.cpp (extern "C"). */
extern "C" {
void registerGpiotePortPin(uint32_t physPin);
void enableGpiotePortCallback(void (*cb)(void));
}

static uint32_t s_btn_prev_state;

static void btn_port_isr(void)
{
    /* Read current state of all buttons */
    uint32_t curr = 0;
    for (uint8_t i = 0; i < NUM_BTNS; i++) {
        if (digitalRead(btn_pins[i]) == LOW) {
            curr |= (1u << i);
        }
    }

    /* Compare with previous state to find which pin(s) changed */
    uint32_t changed = curr ^ s_btn_prev_state;
    s_btn_prev_state = curr;

    for (uint8_t i = 0; i < NUM_BTNS; i++) {
        if (changed & (1u << i)) {
            btns[i].triggered = true;
        }
    }

#if defined(PIO_ENABLE_ARDUINO_THREADS)
    if (button_thread) {
        button_thread->notifyFromISR();
    }
#endif /* PIO_ENABLE_ARDUINO_THREADS */
}

/* ------------------------------------------------------------------ */
/* Public API                                                             */
/* ------------------------------------------------------------------ */

int btn_register_cb(uint8_t btn_idx, btn_event_t event, btn_cb_t cb, void *user_data)
{
    if (btn_idx >= NUM_BTNS || event >= BTN_EVENT_MAX) {
        return -1;
    }
    btns[btn_idx].cbs[event].cb = cb;
    btns[btn_idx].cbs[event].user_data = user_data;
    return 0;
}

bool btn_is_pressed(uint8_t btn_idx)
{
    if (btn_idx >= NUM_BTNS) {
        return false;
    }
    /* ACTIVE_LOW: LOW = pressed */
    return digitalRead(btn_pins[btn_idx]) == LOW;
}

/* ------------------------------------------------------------------ */
/* Periodic poll — called from PioThread or directly from main loop     */
/* ------------------------------------------------------------------ */

int32_t button_loop(void)
{
    uint32_t now = millis();
    bool any_active = false;

    for (uint8_t i = 0; i < NUM_BTNS; i++) {
        btn_ctx_t *btn = &btns[i];
        bool current_pressed = (digitalRead(btn->pin) == LOW);

        /* ---- New trigger from interrupt ---- */
        if (btn->triggered) {
            btn->triggered = false;
            btn->debounce_start_ms = now;
            btn->debounce_last_level = current_pressed;
        }

        /* ---- Debounce in progress ---- */
        if (btn->debounce_start_ms != 0) {
            uint32_t elapsed = now - btn->debounce_start_ms;
            if (elapsed >= BTN_DEBOUNCE_MS) {
                if (current_pressed == btn->debounce_last_level) {
                    /* Debounce confirmed — feed into state machine */
                    btn_process(btn, i, current_pressed, now);
                    btn->debounce_start_ms = 0;
                } else {
                    /* State changed during debounce — restart timer */
                    btn->debounce_start_ms = now;
                    btn->debounce_last_level = current_pressed;
                }
            }
        }

        /* ---- Timeout checks for non-idle states ---- */
        if (btn->debounce_start_ms == 0 && btn->state != BTN_STATE_IDLE) {
            switch (btn->state) {
            case BTN_STATE_PRESSED:
                if (!current_pressed) {
                    /* Release may be detected by polling (backup for
                     * PORT event SENSE toggle race). */
                    btn_process(btn, i, false, now);
                } else if (now - btn->press_time_ms >= BTN_LONG_PRESS_MS) {
                    btn_process(btn, i, true, now);
                }
                break;
            case BTN_STATE_LONG_PRESS:
                if (!current_pressed) {
                    btn_process(btn, i, false, now);
                } else if (now - btn->press_time_ms >= BTN_VERY_LONG_PRESS_MS) {
                    btn_process(btn, i, true, now);
                }
                break;
            case BTN_STATE_WAIT_DOUBLE:
                if (now - btn->release_time_ms >= BTN_DOUBLE_CLICK_MS) {
                    btn_process(btn, i, false, now);
                }
                break;
            default:
                break;
            }
        }

        /* Track whether this button needs polling */
        if (btn->state != BTN_STATE_IDLE || btn->debounce_start_ms != 0) {
            any_active = true;
        }
    }

    if (any_active) {
        return BTN_POLL_MS;
    }

    return INT32_MAX;
}

/* ------------------------------------------------------------------ */
/* Module init                                                           */
/* ------------------------------------------------------------------ */

void button_setup(void)
{
    for (uint8_t i = 0; i < NUM_BTNS; i++) {
        btn_ctx_t *btn = &btns[i];

        btn->pin = btn_pins[i];
        btn->state = BTN_STATE_IDLE;
        btn->triggered = false;
        btn->debounce_start_ms = 0;
        btn->click_count = 0;

        /* PORT event (GPIO SENSE) — ~2.4 µA total vs ~17 µA per pin via
         * attachInterrupt.  registerGpiotePortPin internally calls
         * nrf_gpio_cfg_sense_input(pin, PULLUP, SENSE_LOW). */
        registerGpiotePortPin(g_ADigitalPinMap[btn_pins[i]]);
    }
    enableGpiotePortCallback(btn_port_isr);

#if defined(PIO_ENABLE_ARDUINO_THREADS)
    button_thread = new ButtonThread();
#endif /* PIO_ENABLE_ARDUINO_THREADS */
}

#endif /* PIO_ENABLE_BUTTON */
