/*
 * Stride AVR runtime.
 *
 * Provides main(), the domain tick, digital pins and the serial port for
 * Stride programs compiled for AVR. stridec compiles this file with avr-gcc
 * once for each board configuration and links it with the program:
 *
 *   avr-gcc -mmcu=<device> -DF_CPU=<clock> -Os -include stride_board.h ...
 *
 * stride_board.h is generated from the resolved board (platforms/AVR/boards)
 * and defines:
 *
 *   STRIDE_PIN_COUNT            number of Arduino pin numbers
 *   STRIDE_PIN_OUTPUT_REGS      { PORTx data-space address per pin }
 *   STRIDE_PIN_DIRECTION_REGS   { DDRx  data-space address per pin }
 *   STRIDE_PIN_INPUT_REGS       { PINx  data-space address per pin }
 *   STRIDE_PIN_BITS             { bit per pin }
 *   STRIDE_SERIAL_BAUD          baud rate of the USB serial port
 *
 * For each domain it uses, the program provides
 *
 *   int32_t <Domain>_init(void *state);
 *   int32_t <Domain>_process(void *state);
 *   uint8_t __stride_<Domain>_state[];   the domain's state, sized by stridec
 *                                         with the AVR data layout
 *
 * They are declared weak here, so a program without a RootDomain,
 * AvrTickDomain, or AvrLoopDomain stream still links.
 */

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef F_CPU
#error "F_CPU must be defined (the board's clock in Hz)"
#endif

#ifndef STRIDE_PIN_COUNT
#error "STRIDE_PIN_COUNT is not defined: build with the generated stride_board.h"
#endif

#ifndef STRIDE_SERIAL_BAUD
#define STRIDE_SERIAL_BAUD 115200UL
#endif

#ifndef STRIDE_SERIAL_TX_SIZE
#define STRIDE_SERIAL_TX_SIZE 64 /* power of two */
#endif

#ifndef STRIDE_SERIAL_RX_SIZE
#define STRIDE_SERIAL_RX_SIZE 32 /* power of two */
#endif

#define STRIDE_TICK_HZ 1000UL

/* ---- Program entry points (weak: a domain may be unused) --------------- */

int32_t RootDomain_init(void *state) __attribute__((weak));
int32_t RootDomain_process(void *state) __attribute__((weak));
extern uint8_t __stride_RootDomain_state[] __attribute__((weak));
int32_t AvrTickDomain_init(void *state) __attribute__((weak));
int32_t AvrTickDomain_process(void *state) __attribute__((weak));
extern uint8_t __stride_AvrTickDomain_state[] __attribute__((weak));
int32_t AvrLoopDomain_init(void *state) __attribute__((weak));
int32_t AvrLoopDomain_process(void *state) __attribute__((weak));
extern uint8_t __stride_AvrLoopDomain_state[] __attribute__((weak));

#ifdef STRIDE_ISR_DECLARATIONS
STRIDE_ISR_DECLARATIONS
#endif

/* USART0's vectors: chips with one USART (ATmega328P) name them USART_*,
 * chips with several (ATmega2560) USART0_*. */
#if defined(USART0_RX_vect)
#define STRIDE_USART_RX_vect USART0_RX_vect
#define STRIDE_USART_UDRE_vect USART0_UDRE_vect
#else
#define STRIDE_USART_RX_vect USART_RX_vect
#define STRIDE_USART_UDRE_vect USART_UDRE_vect
#endif

/* ---- Tick (Timer0) ---------------------------------------------------- */

static volatile uint32_t s_millis;
static volatile uint8_t s_pendingTicks;
static volatile uint32_t s_avrTickSleepTicks;

ISR(TIMER0_COMPA_vect) {
  s_millis++;
  if (s_pendingTicks != 0xFF) {
    s_pendingTicks++;
  }
}

static void tick_start(void) {
  /* CTC mode, prescaler 64: F_CPU / 64 / 1000 counts per tick (250 at
   * 16 MHz). */
  TCCR0A = (1 << WGM01);
  TCCR0B = (1 << CS01) | (1 << CS00);
  OCR0A = (uint8_t)(F_CPU / 64UL / STRIDE_TICK_HZ - 1UL);
  TIMSK0 = (1 << OCIE0A);
}

uint32_t __stride_avr_millis(void) {
  uint8_t sreg = SREG;
  cli();
  uint32_t value = s_millis;
  SREG = sreg;
  return value;
}

void __stride_avr_tick_sleep(uint32_t ticks) {
  s_avrTickSleepTicks = ticks;
}

/* Loop-domain sleep: AvrLoopDomain is skipped until s_loopWakeMillis, while
 * main() keeps processing ticks. Set by Sleep(domain: AvrLoopDomain). */
static bool s_loopSleeping;
static uint32_t s_loopWakeMillis;

void __stride_avr_loop_sleep(uint32_t ms) {
  s_loopWakeMillis = __stride_avr_millis() + ms;
  s_loopSleeping = ms > 0;
}

void __stride_avr_sleep_ms(uint32_t ms) {
  uint32_t start = __stride_avr_millis();
  while ((uint32_t)(__stride_avr_millis() - start) < ms) {
  }
}

/* ---- Digital pins ------------------------------------------------------ */

/* Registers above the 8-bit data space (ATmega2560's ports H to L) need
 * 16-bit tables; stride_board.h says so with STRIDE_PIN_REG_WIDE. */
#ifdef STRIDE_PIN_REG_WIDE
typedef uint16_t stride_pin_reg_t;
#else
typedef uint8_t stride_pin_reg_t;
#endif

static const stride_pin_reg_t s_outputRegs[STRIDE_PIN_COUNT] =
    STRIDE_PIN_OUTPUT_REGS;
static const stride_pin_reg_t s_directionRegs[STRIDE_PIN_COUNT] =
    STRIDE_PIN_DIRECTION_REGS;
static const stride_pin_reg_t s_inputRegs[STRIDE_PIN_COUNT] =
    STRIDE_PIN_INPUT_REGS;
static const uint8_t s_bits[STRIDE_PIN_COUNT] = STRIDE_PIN_BITS;

#define REG8(address) (*(volatile uint8_t *)(uint16_t)(address))

void __stride_avr_pin_mode(int16_t pin, int16_t mode) {
  if (pin < 0 || pin >= STRIDE_PIN_COUNT) {
    return;
  }
  uint8_t mask = (uint8_t)(1u << s_bits[pin]);
  uint8_t sreg = SREG;
  cli();
  if (mode == 1) { /* output */
    REG8(s_directionRegs[pin]) |= mask;
  } else {
    REG8(s_directionRegs[pin]) &= (uint8_t)~mask;
    if (mode == 2) { /* input with pull-up */
      REG8(s_outputRegs[pin]) |= mask;
    } else {
      REG8(s_outputRegs[pin]) &= (uint8_t)~mask;
    }
  }
  SREG = sreg;
}

/* value is a Stride Switch: only its lowest bit is meaningful. */
void __stride_avr_pin_write(int16_t pin, uint8_t value) {
  if (pin < 0 || pin >= STRIDE_PIN_COUNT) {
    return;
  }
  uint8_t mask = (uint8_t)(1u << s_bits[pin]);
  uint8_t sreg = SREG;
  cli();
  if (value & 1) {
    REG8(s_outputRegs[pin]) |= mask;
  } else {
    REG8(s_outputRegs[pin]) &= (uint8_t)~mask;
  }
  SREG = sreg;
}

bool __stride_avr_pin_read(int16_t pin) {
  if (pin < 0 || pin >= STRIDE_PIN_COUNT) {
    return false;
  }
  return (REG8(s_inputRegs[pin]) & (uint8_t)(1u << s_bits[pin])) != 0;
}

/* ---- Serial (USART0) --------------------------------------------------- */

static volatile uint8_t s_tx[STRIDE_SERIAL_TX_SIZE];
static volatile uint8_t s_txHead;
static volatile uint8_t s_txTail;
static volatile uint8_t s_rx[STRIDE_SERIAL_RX_SIZE];
static volatile uint8_t s_rxHead;
static volatile uint8_t s_rxTail;

static void serial_start(void) {
  /* Double speed mode: much smaller baud error at 115200 on 16 MHz. */
  uint16_t ubrr = (uint16_t)((F_CPU + 4UL * STRIDE_SERIAL_BAUD) /
                                 (8UL * STRIDE_SERIAL_BAUD) -
                             1UL);
  UCSR0A = (1 << U2X0);
  UBRR0H = (uint8_t)(ubrr >> 8);
  UBRR0L = (uint8_t)ubrr;
  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); /* 8N1 */
  UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
}

ISR(STRIDE_USART_UDRE_vect) {
  if (s_txHead == s_txTail) {
    UCSR0B &= (uint8_t)~(1 << UDRIE0);
    return;
  }
  UDR0 = s_tx[s_txTail];
  s_txTail = (uint8_t)((s_txTail + 1) & (STRIDE_SERIAL_TX_SIZE - 1));
}

ISR(STRIDE_USART_RX_vect) {
  uint8_t byte = UDR0;
  uint8_t next = (uint8_t)((s_rxHead + 1) & (STRIDE_SERIAL_RX_SIZE - 1));
  if (next != s_rxTail) { /* drop the byte when the buffer is full */
    s_rx[s_rxHead] = byte;
    s_rxHead = next;
  }
}

void __stride_avr_serial_write_byte(uint8_t byte) {
  uint8_t next = (uint8_t)((s_txHead + 1) & (STRIDE_SERIAL_TX_SIZE - 1));
  /* Wait for room. The transmit interrupt drains the buffer, so this only
   * blocks when a tick writes more than the buffer holds. */
  while (next == s_txTail) {
  }
  s_tx[s_txHead] = byte;
  s_txHead = next;
  UCSR0B |= (1 << UDRIE0);
}

int16_t __stride_avr_serial_read_byte(void) {
  if (s_rxHead == s_rxTail) {
    return -1;
  }
  uint8_t byte = s_rx[s_rxTail];
  s_rxTail = (uint8_t)((s_rxTail + 1) & (STRIDE_SERIAL_RX_SIZE - 1));
  return byte;
}

static void serial_write_text(const char *text) {
  while (*text) {
    __stride_avr_serial_write_byte((uint8_t)*text++);
  }
}

void __stride_avr_serial_print_int(int16_t value) {
  char text[8];
  itoa(value, text, 10);
  serial_write_text(text);
}

void __stride_avr_serial_print_real(float value) {
  char text[16];
  dtostrf(value, 0, 3, text);
  serial_write_text(text);
}

void __stride_avr_serial_newline(void) { serial_write_text("\r\n"); }

/* Print and PrintLn output from the string runtime (strideruntime_string.c,
 * built with STRIDE_RT_NO_STDIO) goes to the serial port. */
void stride_rt_platform_write(const char *data, int32_t length) {
  for (int32_t i = 0; i < length; ++i) {
    __stride_avr_serial_write_byte((uint8_t)data[i]);
  }
}

/* ---- Helpers the comparison modules call ------------------------------- */

/* Greater with a Real result (1 or 0). The name matches the JIT's helper;
 * on AVR Real is float. */
float __stride_Greater_d_dd(float a, float b) { return a > b ? 1.0f : 0.0f; }

/* ---- main --------------------------------------------------------------- */

int main(void) {
  serial_start();
  tick_start();

  if (RootDomain_init) {
    RootDomain_init(__stride_RootDomain_state);
  }
  if (AvrTickDomain_init) {
    AvrTickDomain_init(__stride_AvrTickDomain_state);
  }
  if (AvrLoopDomain_init) {
    AvrLoopDomain_init(__stride_AvrLoopDomain_state);
  }
#ifdef STRIDE_ISR_INIT
  STRIDE_ISR_INIT
#endif
  sei();

  if (RootDomain_process) {
    RootDomain_process(__stride_RootDomain_state);
  }

  for (;;) {
    uint8_t ticks;
    uint8_t sreg = SREG;
    cli();
    ticks = s_pendingTicks;
    s_pendingTicks = 0;
    SREG = sreg;

    /* Run one process call per elapsed tick, so time stays right when a
     * tick takes longer than 1 ms now and then. Skip ticks while sleeping. */
    while (ticks--) {
      if (s_avrTickSleepTicks > 0) {
        s_avrTickSleepTicks--;
      } else if (AvrTickDomain_process) {
        AvrTickDomain_process(__stride_AvrTickDomain_state);
      }
    }

    if (s_loopSleeping &&
        (int32_t)(__stride_avr_millis() - s_loopWakeMillis) >= 0) {
      s_loopSleeping = false;
    }
    if (!s_loopSleeping && AvrLoopDomain_process) {
      AvrLoopDomain_process(__stride_AvrLoopDomain_state);
    }
  }
}
