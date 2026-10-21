/*
 * Layer 5 — SX126x module entry point.
 *
 * Provides lora_ra01s_setup() / lora_ra01s_loop() with PioThread.
 * Following CLAUDE.md 2.4 / 2.5 conventions.
 */

#include "lora_sx126x.h"

#if defined(PIO_ENABLE_LORA_RA01S)

#include "lora_sx126x_bench.h"
#include "lora_sx126x_config.h"
#include "lora_sx126x_hal.h"
#include "lora_sx126x_shell.h"
#include "variant.h"

#include <Arduino.h>

#if defined(PIO_ENABLE_SHELL_MODULE)
#include <shell_module.h>
#endif /* PIO_ENABLE_SHELL_MODULE */

#if defined(PIO_ENABLE_ARDUINO_THREADS)
#include <arduino_threads.h>
#endif /* PIO_ENABLE_ARDUINO_THREADS */

/* ================================================================== */
/* PioThread                                                            */
/* ================================================================== */

#if defined(PIO_ENABLE_ARDUINO_THREADS)

class LoraThread : public PioThread
{
  public:
    LoraThread() : PioThread("lora", 10) { traceLog = false; }

  protected:
    int32_t runOnce() override;
};

static LoraThread *s_lora_thread;

/* Thread wake functions (called from shell/bench) */
void sx126x_wake_thread(void)
{
    if (s_lora_thread)
        s_lora_thread->notify();
}

void sx126x_wake_thread_from_isr(void)
{
    if (s_lora_thread)
        s_lora_thread->notifyFromISR();
}

/* ── Poll interval when async RX is active ── */
#define ASYNC_RX_POLL_MS 1000

int32_t LoraThread::runOnce(void)
{
    /* 1. Check pending RX packet (DIO1 IRQ) */
    if (sx126x_shell_is_rx_pending()) {
        uint8_t buf[256];
        int16_t rssi;
        int8_t snr;

        int16_t len = sx126x_shell_consume_rx(buf, sizeof(buf), &rssi, &snr);
        if (len > 0) {
            if (sx126x_bench_is_rx()) {
                /* Route to bench RX handler */
                sx126x_bench_on_rx_packet(buf, len, rssi, snr);
                sx126x_bench_rx_idle_touch();
            } else if (sx126x_shell_is_async_rx()) {
#if defined(PIO_ENABLE_SHELL_MODULE)
                /* Log to serial */
                char hex[128];
                size_t pos = 0;
                for (int16_t i = 0; i < len && pos < sizeof(hex) - 3; i++) {
                    pos += (size_t)snprintf(hex + pos, sizeof(hex) - pos, "%02x ", buf[i]);
                }
                hex[pos] = '\0';
                dbg_log("lora", "RX %d bytes [%s] RSSI=%d SNR=%d", len, hex, rssi, snr);
#endif /* PIO_ENABLE_SHELL_MODULE */
            }
        }
    }

    /* 2. Bench TX progress (one packet per runOnce call) */
    if (sx126x_bench_is_busy() && !sx126x_bench_is_rx()) {
        int32_t ret = sx126x_bench_tx_run_once();
        if (ret < 0) {
            return 10; /* Bench TX finished */
        }
        return 0; /* Run again ASAP for back-to-back TX */
    }

    /* 3. Bench RX idle check & progress snap */
    if (sx126x_bench_is_rx()) {
        int32_t ret = sx126x_bench_rx_check_idle();
        if (ret < 0) {
            sx126x_shell_stop_async_rx();
            return 10;
        }
        sx126x_bench_rx_print_snap();
    }

    /* 4. Low-rate polling when async RX active */
    if (sx126x_shell_is_async_rx()) {
        return ASYNC_RX_POLL_MS;
    }

    /* 5. Nothing to do — disable until notify() from command */
    disable();
    return 10;
}

#else /* !PIO_ENABLE_ARDUINO_THREADS */

void sx126x_wake_thread(void) {}
void sx126x_wake_thread_from_isr(void) {}

#endif /* PIO_ENABLE_ARDUINO_THREADS */

/* ================================================================== */
/* Public API                                                          */
/* ================================================================== */

void lora_ra01s_setup(void)
{
    /* Init bench state */
    sx126x_bench_init();

    /* Init shell commands */
#if defined(PIO_ENABLE_SHELL_MODULE)
    sx126x_shell_init();
#endif /* PIO_ENABLE_SHELL_MODULE */

    /* Create PioThread */
#if defined(PIO_ENABLE_ARDUINO_THREADS)
    if (!s_lora_thread) {
        s_lora_thread = new LoraThread();
    }
#endif /* PIO_ENABLE_ARDUINO_THREADS */
}

int32_t lora_ra01s_loop(void)
{
    /* Nothing to do in loop() when using PioThread;
     * PioThread::runOnce() handles everything.
     * Return value is only used in non-thread mode. */
    return 100;
}

int lora_ra01s_start(void)
{
    return sx126x_shell_start();
}

int lora_ra01s_send(const uint8_t *data, size_t len)
{
    if (data == NULL || len == 0) {
        return -1;
    }

    return sx126x_shell_send(data, len);
}

#endif /* PIO_ENABLE_LORA_RA01S */
