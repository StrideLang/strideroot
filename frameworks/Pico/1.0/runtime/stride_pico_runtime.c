/*
 * Stride Raspberry Pi Pico (RP2040 & RP2350) runtime.
 *
 * Built on pico-sdk. Provides main(), the 1 kHz cooperative tick timer,
 * per-domain sleep, and stdio serial I/O for Stride programs compiled with
 * Stride's LLVM backend.
 *
 * For each domain it uses, the compiled Stride object provides:
 *
 *   void <Domain>_load(void);
 *   int32_t <Domain>_init(void *state);
 *   int32_t <Domain>_process(void *state);
 *   uint8_t __stride_<Domain>_state[];
 *
 * They are declared weak here, so a program without a RootDomain,
 * PicoTickDomain, or PicoLoopDomain stream still links.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/sync.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"

/* ---- Program entry points (weak: a domain may be unused) --------------- */

void RootDomain_load(void) __attribute__((weak));
int32_t RootDomain_init(void *state) __attribute__((weak));
int32_t RootDomain_process(void *state) __attribute__((weak));
extern uint8_t __stride_RootDomain_state[] __attribute__((weak));

void PicoTickDomain_load(void) __attribute__((weak));
int32_t PicoTickDomain_init(void *state) __attribute__((weak));
int32_t PicoTickDomain_process(void *state) __attribute__((weak));
extern uint8_t __stride_PicoTickDomain_state[] __attribute__((weak));

void PicoLoopDomain_load(void) __attribute__((weak));
int32_t PicoLoopDomain_init(void *state) __attribute__((weak));
int32_t PicoLoopDomain_process(void *state) __attribute__((weak));
extern uint8_t __stride_PicoLoopDomain_state[] __attribute__((weak));

/* ---- Tick (1 kHz hardware repeating timer) ----------------------------- */

static volatile uint32_t s_pendingTicks;
static volatile uint32_t s_picoTickSleepTicks;
static struct repeating_timer s_tickTimer;

static bool tick_callback(struct repeating_timer *rt) {
  (void)rt;
  if (s_pendingTicks < 0xFFFFFFFFu) {
    s_pendingTicks++;
  }
  return true;
}

uint32_t __stride_pico_millis(void) {
  return to_ms_since_boot(get_absolute_time());
}

void __stride_pico_tick_sleep(uint32_t ticks) {
  s_picoTickSleepTicks = ticks;
}

static bool s_loopSleeping;
static uint32_t s_loopWakeMillis;

void __stride_pico_loop_sleep(uint32_t ms) {
  s_loopWakeMillis = __stride_pico_millis() + ms;
  s_loopSleeping = ms > 0;
}

void __stride_pico_sleep_ms(uint32_t ms) { sleep_ms(ms); }

/* ---- Digital pins (fallback pin-number helpers) ------------------------ */

void __stride_pico_pin_mode(int32_t pin, int32_t mode) {
  if (pin < 0 || pin >= 30) {
    return;
  }
  gpio_init((uint)pin);
  if (mode == 1) {
    gpio_set_dir((uint)pin, GPIO_OUT);
  } else {
    gpio_set_dir((uint)pin, GPIO_IN);
    if (mode == 2) {
      gpio_pull_up((uint)pin);
    } else if (mode == 3) {
      gpio_pull_down((uint)pin);
    } else {
      gpio_disable_pulls((uint)pin);
    }
  }
}

void __stride_pico_pin_write(int32_t pin, uint8_t value) {
  if (pin < 0 || pin >= 30) {
    return;
  }
  gpio_put((uint)pin, (value & 1) != 0);
}

bool __stride_pico_pin_read(int32_t pin) {
  if (pin < 0 || pin >= 30) {
    return false;
  }
  return gpio_get((uint)pin);
}

/* ---- Serial (pico-sdk stdio: USB CDC / UART) --------------------------- */

void __stride_pico_serial_write_byte(uint8_t byte) { putchar_raw((int)byte); }

void __stride_pico_serial_print_int(int32_t value) {
  printf("%ld", (long)value);
}

void __stride_pico_serial_print_real(float value) {
  printf("%.3f", (double)value);
}

void __stride_pico_serial_newline(void) { printf("\r\n"); }

int32_t __stride_pico_serial_read_byte(void) {
  int ch = getchar_timeout_us(0);
  return (ch == PICO_ERROR_TIMEOUT) ? -1 : ch;
}

/* ---- Helpers the comparison modules call ------------------------------- */

float __stride_Greater_d_dd(float a, float b) { return a > b ? 1.0f : 0.0f; }

/* ---- main --------------------------------------------------------------- */

int main(void) {
  stdio_init_all();

  if (RootDomain_load) {
    RootDomain_load();
  }
  if (PicoTickDomain_load) {
    PicoTickDomain_load();
  }
  if (PicoLoopDomain_load) {
    PicoLoopDomain_load();
  }

  if (RootDomain_init) {
    RootDomain_init(__stride_RootDomain_state);
  }
  if (PicoTickDomain_init) {
    PicoTickDomain_init(__stride_PicoTickDomain_state);
  }
  if (PicoLoopDomain_init) {
    PicoLoopDomain_init(__stride_PicoLoopDomain_state);
  }

  if (RootDomain_process) {
    RootDomain_process(__stride_RootDomain_state);
  }

  add_repeating_timer_ms(-1, tick_callback, NULL, &s_tickTimer);

  for (;;) {
    uint32_t irqStatus = save_and_disable_interrupts();
    uint32_t ticks = s_pendingTicks;
    s_pendingTicks = 0;
    restore_interrupts(irqStatus);

    while (ticks--) {
      if (s_picoTickSleepTicks > 0) {
        s_picoTickSleepTicks--;
      } else if (PicoTickDomain_process) {
        PicoTickDomain_process(__stride_PicoTickDomain_state);
      }
    }

    if (s_loopSleeping &&
        (int32_t)(__stride_pico_millis() - s_loopWakeMillis) >= 0) {
      s_loopSleeping = false;
    }
    if (!s_loopSleeping && PicoLoopDomain_process) {
      PicoLoopDomain_process(__stride_PicoLoopDomain_state);
    }

    tight_loop_contents();
  }
}
