/*
 * Stride STMicroelectronics STM32F7 (ARM Cortex-M7) runtime.
 *
 * Provides main(), the 1 kHz SysTick cooperative tick timer, per-domain sleep,
 * and serial I/O for Stride programs compiled with Stride's LLVM backend.
 *
 * For each domain it uses, the compiled Stride object provides:
 *
 *   void <Domain>_load(void);
 *   int32_t <Domain>_init(void *state);
 *   int32_t <Domain>_process(void *state);
 *   uint8_t __stride_<Domain>_state[];
 *
 * They are declared weak here, so a program without a RootDomain,
 * STM32F7TickDomain, or STM32F7LoopDomain stream still links.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifndef STRIDE_STM32F7_SYSCLK_HZ
#define STRIDE_STM32F7_SYSCLK_HZ 16000000UL
#endif

#ifndef STRIDE_STM32F7_UART
#define STRIDE_STM32F7_UART 3
#endif

/* ---- Cortex-M7 SysTick & NVIC registers -------------------------------- */

#define SYST_CSR (*(volatile uint32_t *)0xE000E010UL)
#define SYST_RVR (*(volatile uint32_t *)0xE000E014UL)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018UL)

/* ---- STM32F7 RCC & Console USART --------------------------------------- */

#define RCC_AHB1ENR (*(volatile uint32_t *)0x40023830UL)
#define RCC_APB1ENR (*(volatile uint32_t *)0x40023840UL)
#define RCC_APB2ENR (*(volatile uint32_t *)0x40023844UL)

#if STRIDE_STM32F7_UART == 1
/* USART1: PA9 (TX, AF7) / PB7 (RX, AF7) - ST-Link VCP on STM32F746G-DISCO */
#define GPIOA_MODER (*(volatile uint32_t *)0x40020000UL)
#define GPIOA_AFRH  (*(volatile uint32_t *)0x40020024UL)
#define GPIOB_MODER (*(volatile uint32_t *)0x40020400UL)
#define GPIOB_AFRL  (*(volatile uint32_t *)0x40020420UL)

#define UART_CR1    (*(volatile uint32_t *)0x40011000UL)
#define UART_BRR    (*(volatile uint32_t *)0x4001100CUL)
#define UART_ISR    (*(volatile uint32_t *)0x4001101CUL)
#define UART_RDR    (*(volatile uint32_t *)0x40011024UL)
#define UART_TDR    (*(volatile uint32_t *)0x40011028UL)
#else
/* USART3: PD8 (TX, AF7) / PD9 (RX, AF7) - ST-Link VCP on NUCLEO-F767ZI */
#define GPIOD_MODER (*(volatile uint32_t *)0x40020C00UL)
#define GPIOD_AFRH  (*(volatile uint32_t *)0x40020C24UL)

#define UART_CR1    (*(volatile uint32_t *)0x40004800UL)
#define UART_BRR    (*(volatile uint32_t *)0x4000480CUL)
#define UART_ISR    (*(volatile uint32_t *)0x4000481CUL)
#define UART_RDR    (*(volatile uint32_t *)0x40004824UL)
#define UART_TDR    (*(volatile uint32_t *)0x40004828UL)
#endif

/* ---- Program entry points (weak: a domain may be unused) --------------- */

void RootDomain_load(void) __attribute__((weak));
int32_t RootDomain_init(void *state) __attribute__((weak));
int32_t RootDomain_process(void *state) __attribute__((weak));
extern uint8_t __stride_RootDomain_state[] __attribute__((weak));

void STM32F7TickDomain_load(void) __attribute__((weak));
int32_t STM32F7TickDomain_init(void *state) __attribute__((weak));
int32_t STM32F7TickDomain_process(void *state) __attribute__((weak));
extern uint8_t __stride_STM32F7TickDomain_state[] __attribute__((weak));

void STM32F7LoopDomain_load(void) __attribute__((weak));
int32_t STM32F7LoopDomain_init(void *state) __attribute__((weak));
int32_t STM32F7LoopDomain_process(void *state) __attribute__((weak));
extern uint8_t __stride_STM32F7LoopDomain_state[] __attribute__((weak));

/* ---- Tick (1 kHz SysTick) ---------------------------------------------- */

static volatile uint32_t s_millis;
static volatile uint32_t s_pendingTicks;
static volatile uint32_t s_tickSleepTicks;

void SysTick_Handler(void) {
  s_millis++;
  if (s_pendingTicks < 0xFFFFFFFFu) {
    s_pendingTicks++;
  }
}

static void systick_init_1khz(void) {
  SYST_RVR = (uint32_t)((STRIDE_STM32F7_SYSCLK_HZ / 1000UL) - 1UL);
  SYST_CVR = 0;
  SYST_CSR = 0x7UL; /* CLKSOURCE = processor, TICKINT = 1, ENABLE = 1 */
}

uint32_t __stride_stm32f7_millis(void) { return s_millis; }

void __stride_stm32f7_tick_sleep(uint32_t ticks) {
  s_tickSleepTicks = ticks;
}

static bool s_loopSleeping;
static uint32_t s_loopWakeMillis;

void __stride_stm32f7_loop_sleep(uint32_t ms) {
  s_loopWakeMillis = __stride_stm32f7_millis() + ms;
  s_loopSleeping = ms > 0;
}

void __stride_stm32f7_sleep_ms(uint32_t ms) {
  uint32_t start = __stride_stm32f7_millis();
  while ((uint32_t)(__stride_stm32f7_millis() - start) < ms) {
    __asm volatile("wfi");
  }
}

/* ---- Serial (USART @ 115200 baud) -------------------------------------- */

static void serial_init(void) {
#if STRIDE_STM32F7_UART == 1
  RCC_AHB1ENR |= (1UL << 0) | (1UL << 1); /* GPIOAEN | GPIOBEN */
  RCC_APB2ENR |= (1UL << 4);              /* USART1EN */

  /* PA9 (TX) and PB7 (RX) -> Alternate Function 7 (USART1). */
  GPIOA_MODER &= ~(3UL << 18);
  GPIOA_MODER |= (2UL << 18);
  GPIOA_AFRH &= ~(0xFUL << 4);
  GPIOA_AFRH |= (7UL << 4);

  GPIOB_MODER &= ~(3UL << 14);
  GPIOB_MODER |= (2UL << 14);
  GPIOB_AFRL &= ~(0xFUL << 28);
  GPIOB_AFRL |= (7UL << 28);
#else
  RCC_AHB1ENR |= (1UL << 3);  /* GPIODEN */
  RCC_APB1ENR |= (1UL << 18); /* USART3EN */

  /* PD8 (TX) and PD9 (RX) -> Alternate Function 7 (USART3). */
  GPIOD_MODER &= ~((3UL << 16) | (3UL << 18));
  GPIOD_MODER |= (2UL << 16) | (2UL << 18);
  GPIOD_AFRH &= ~((0xFUL << 0) | (0xFUL << 4));
  GPIOD_AFRH |= (7UL << 0) | (7UL << 4);
#endif

  UART_CR1 = 0;
  UART_BRR = (uint32_t)(STRIDE_STM32F7_SYSCLK_HZ / 115200UL);
  UART_CR1 = (1UL << 0) | (1UL << 2) | (1UL << 3); /* UE | RE | TE */
}

void __stride_stm32f7_serial_write_byte(uint8_t byte) {
  while ((UART_ISR & (1UL << 7)) == 0) { /* TXE */
  }
  UART_TDR = byte;
}

static void serial_write_str(const char *s) {
  while (*s) {
    __stride_stm32f7_serial_write_byte((uint8_t)*s++);
  }
}

void __stride_stm32f7_serial_print_int(int32_t value) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%ld", (long)value);
  serial_write_str(buf);
}

void __stride_stm32f7_serial_print_real(float value) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%.3f", (double)value);
  serial_write_str(buf);
}

void __stride_stm32f7_serial_newline(void) { serial_write_str("\r\n"); }

int32_t __stride_stm32f7_serial_read_byte(void) {
  if ((UART_ISR & (1UL << 5)) == 0) { /* RXNE */
    return -1;
  }
  return (int32_t)(UART_RDR & 0xFFu);
}

/* ---- Newlib stdio / POSIX syscall stubs ------------------------------- */

#include <sys/stat.h>

int _write(int file, char *ptr, int len) {
  if (file != 1 && file != 2) {
    return -1;
  }
  for (int i = 0; i < len; ++i) {
    if (ptr[i] == '\n') {
      __stride_stm32f7_serial_write_byte('\r');
    }
    __stride_stm32f7_serial_write_byte((uint8_t)ptr[i]);
  }
  return len;
}

int _read(int file, char *ptr, int len) {
  if (file != 0 || len <= 0) {
    return 0;
  }
  int32_t ch = __stride_stm32f7_serial_read_byte();
  if (ch < 0) {
    return 0;
  }
  ptr[0] = (char)ch;
  return 1;
}

int _close(int file) { (void)file; return -1; }
int _fstat(int file, struct stat *st) { (void)file; st->st_mode = S_IFCHR; return 0; }
int _isatty(int file) { (void)file; return 1; }
int _lseek(int file, int ptr, int dir) { (void)file; (void)ptr; (void)dir; return 0; }
int _getpid(void) { return 1; }
int _kill(int pid, int sig) { (void)pid; (void)sig; return -1; }

/* ---- Helpers the comparison modules call ------------------------------- */

float __stride_Greater_d_dd(float a, float b) { return a > b ? 1.0f : 0.0f; }

/* ---- main --------------------------------------------------------------- */

int main(void) {
  serial_init();
  systick_init_1khz();

  if (RootDomain_load) {
    RootDomain_load();
  }
  if (STM32F7TickDomain_load) {
    STM32F7TickDomain_load();
  }
  if (STM32F7LoopDomain_load) {
    STM32F7LoopDomain_load();
  }

  if (RootDomain_init) {
    RootDomain_init(__stride_RootDomain_state);
  }
  if (STM32F7TickDomain_init) {
    STM32F7TickDomain_init(__stride_STM32F7TickDomain_state);
  }
  if (STM32F7LoopDomain_init) {
    STM32F7LoopDomain_init(__stride_STM32F7LoopDomain_state);
  }

  if (RootDomain_process) {
    RootDomain_process(__stride_RootDomain_state);
  }

  for (;;) {
    __asm volatile("cpsid i");
    uint32_t ticks = s_pendingTicks;
    s_pendingTicks = 0;
    __asm volatile("cpsie i");

    while (ticks--) {
      if (s_tickSleepTicks > 0) {
        s_tickSleepTicks--;
      } else if (STM32F7TickDomain_process) {
        STM32F7TickDomain_process(__stride_STM32F7TickDomain_state);
      }
    }

    if (s_loopSleeping &&
        (int32_t)(__stride_stm32f7_millis() - s_loopWakeMillis) >= 0) {
      s_loopSleeping = false;
    }
    if (!s_loopSleeping && STM32F7LoopDomain_process) {
      STM32F7LoopDomain_process(__stride_STM32F7LoopDomain_state);
    }
  }
}
