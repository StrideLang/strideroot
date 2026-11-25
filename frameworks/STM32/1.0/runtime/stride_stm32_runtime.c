/*
 * Stride runtime for STMicroelectronics STM32 (ARM Cortex-M) boards.
 *
 * Provides main(), the clock setup, the 1 kHz SysTick cooperative tick, per-
 * domain sleep, and the serial console, for programs compiled by Stride's
 * LLVM backend.
 *
 * Nothing here names a chip or board. stride build writes stride_board.h from
 * the board's Stride data (clocks, console, peripherals, ports) and compiles
 * this file with -include stride_board.h. The code knows the register designs
 * it drives:
 *   - RCC of the STM32F2/F4/F7: one main PLL, AHB/APB prescalers
 *   - USART "stm32.usart.v1" (F1/F2/F4: SR/DR) and "stm32.usart.v2"
 *     (F0/F3/F7/L4/H7: ISR/RDR/TDR), picked by STRIDE_CONSOLE_USART_V1
 *   - GPIO "stm32.gpio.v2" (MODER at +0x00, AFRL/AFRH at +0x20/+0x24)
 *
 * For each domain it uses, the compiled Stride object provides:
 *
 *   void <Domain>_load(void);
 *   int32_t <Domain>_init(void *state);
 *   int32_t <Domain>_process(void *state);
 *   uint8_t __stride_<Domain>_state[];
 *
 * They are declared weak here, so a program without a RootDomain,
 * STM32TickDomain or STM32LoopDomain stream still links.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifndef STRIDE_BOARD_H
#error "compile with -include stride_board.h (stride build writes it)"
#endif

#define REG(address) (*(volatile uint32_t *)(address))

/* ---- Cortex-M SysTick --------------------------------------------------- */

#define SYST_CSR REG(0xE000E010UL)
#define SYST_RVR REG(0xE000E014UL)
#define SYST_CVR REG(0xE000E018UL)

/* ---- STM32F2/F4/F7 RCC, PWR and FLASH ------------------------------------ */

#define RCC_CR REG(0x40023800UL)
#define RCC_PLLCFGR REG(0x40023804UL)
#define RCC_CFGR REG(0x40023808UL)
#define RCC_APB1ENR REG(0x40023840UL)
#define PWR_CR REG(0x40007000UL)  /* PWR_CR1 on the F7 */
#define PWR_CSR REG(0x40007004UL) /* PWR_CSR1 on the F7 */
#define FLASH_ACR REG(0x40023C00UL)

#define RCC_CR_HSEON (1UL << 16)
#define RCC_CR_HSERDY (1UL << 17)
#define RCC_CR_HSEBYP (1UL << 18)
#define RCC_CR_PLLON (1UL << 24)
#define RCC_CR_PLLRDY (1UL << 25)
#define RCC_APB1ENR_PWREN (1UL << 28)
#define PWR_CR_VOS_MASK (3UL << 14)
#define PWR_CR_ODEN (1UL << 16)
#define PWR_CR_ODSWEN (1UL << 17)
#define PWR_CSR_ODRDY (1UL << 16)
#define PWR_CSR_ODSWRDY (1UL << 17)
#define RCC_PLLCFGR_FIELDS 0x0F437FFFUL /* PLLM, PLLN, PLLP, PLLSRC, PLLQ */
#define RCC_CFGR_PRESCALERS ((0xFUL << 4) | (0x7UL << 10) | (0x7UL << 13))

static uint32_t ahb_prescaler(uint32_t divider) {
  switch (divider) {
  case 2: return 8;
  case 4: return 9;
  case 8: return 10;
  case 16: return 11;
  case 64: return 12;
  case 128: return 13;
  case 256: return 14;
  case 512: return 15;
  default: return 0;
  }
}

static uint32_t apb_prescaler(uint32_t divider) {
  switch (divider) {
  case 2: return 4;
  case 4: return 5;
  case 8: return 6;
  case 16: return 7;
  default: return 0;
  }
}

/* Runs the core at STRIDE_HCLK_HZ. stride build has checked the factors
 * against the device's clockLimits, and worked out the flash wait states and
 * the power settings that speed needs. Follows RM0090 (F4) / RM0385 (F7). */
static void clock_init(void) {
  RCC_APB1ENR |= RCC_APB1ENR_PWREN;
  (void)RCC_APB1ENR;
  PWR_CR = (PWR_CR & ~PWR_CR_VOS_MASK) | STRIDE_VOLTAGE_SCALE;

#if STRIDE_CLOCK_HSE
  if (STRIDE_CLOCK_HSE_BYPASS) {
    RCC_CR |= RCC_CR_HSEBYP;
  }
  RCC_CR |= RCC_CR_HSEON;
  while ((RCC_CR & RCC_CR_HSERDY) == 0) {
  }
#endif

#if STRIDE_CLOCK_PLL
  RCC_CR &= ~RCC_CR_PLLON;
  while (RCC_CR & RCC_CR_PLLRDY) {
  }
  RCC_PLLCFGR = (RCC_PLLCFGR & ~RCC_PLLCFGR_FIELDS) |
                ((uint32_t)STRIDE_PLL_M << 0) | ((uint32_t)STRIDE_PLL_N << 6) |
                ((uint32_t)(STRIDE_PLL_P / 2 - 1) << 16) |
                ((uint32_t)STRIDE_CLOCK_HSE << 22) |
                ((uint32_t)STRIDE_PLL_Q << 24);
  RCC_CR |= RCC_CR_PLLON;
  while ((RCC_CR & RCC_CR_PLLRDY) == 0) {
  }
#endif

#if STRIDE_OVERDRIVE
  PWR_CR |= PWR_CR_ODEN;
  while ((PWR_CSR & PWR_CSR_ODRDY) == 0) {
  }
  PWR_CR |= PWR_CR_ODSWEN;
  while ((PWR_CSR & PWR_CSR_ODSWRDY) == 0) {
  }
#endif

  /* More wait states before the clock goes up. */
  FLASH_ACR = (uint32_t)STRIDE_FLASH_LATENCY | STRIDE_FLASH_FLAGS;
  while ((FLASH_ACR & 0xFUL) != (uint32_t)STRIDE_FLASH_LATENCY) {
  }

  RCC_CFGR = (RCC_CFGR & ~RCC_CFGR_PRESCALERS) |
             (ahb_prescaler(STRIDE_AHB_DIVIDER) << 4) |
             (apb_prescaler(STRIDE_APB1_DIVIDER) << 10) |
             (apb_prescaler(STRIDE_APB2_DIVIDER) << 13);

  const uint32_t source = STRIDE_CLOCK_PLL ? 2u : (STRIDE_CLOCK_HSE ? 1u : 0u);
  RCC_CFGR = (RCC_CFGR & ~3UL) | source;
  while (((RCC_CFGR >> 2) & 3UL) != source) {
  }
}

/* ---- Program entry points (weak: a domain may be unused) --------------- */

void RootDomain_load(void) __attribute__((weak));
int32_t RootDomain_init(void *state) __attribute__((weak));
int32_t RootDomain_process(void *state) __attribute__((weak));
extern uint8_t __stride_RootDomain_state[] __attribute__((weak));

void STM32TickDomain_load(void) __attribute__((weak));
int32_t STM32TickDomain_init(void *state) __attribute__((weak));
int32_t STM32TickDomain_process(void *state) __attribute__((weak));
extern uint8_t __stride_STM32TickDomain_state[] __attribute__((weak));

void STM32LoopDomain_load(void) __attribute__((weak));
int32_t STM32LoopDomain_init(void *state) __attribute__((weak));
int32_t STM32LoopDomain_process(void *state) __attribute__((weak));
extern uint8_t __stride_STM32LoopDomain_state[] __attribute__((weak));

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
  SYST_RVR = (uint32_t)((STRIDE_HCLK_HZ / 1000UL) - 1UL);
  SYST_CVR = 0;
  SYST_CSR = 0x7UL; /* CLKSOURCE = processor, TICKINT = 1, ENABLE = 1 */
}

uint32_t __stride_stm32_millis(void) { return s_millis; }

void __stride_stm32_tick_sleep(uint32_t ticks) { s_tickSleepTicks = ticks; }

static bool s_loopSleeping;
static uint32_t s_loopWakeMillis;

void __stride_stm32_loop_sleep(uint32_t ms) {
  s_loopWakeMillis = __stride_stm32_millis() + ms;
  s_loopSleeping = ms > 0;
}

void __stride_stm32_sleep_ms(uint32_t ms) {
  uint32_t start = __stride_stm32_millis();
  while ((uint32_t)(__stride_stm32_millis() - start) < ms) {
    __asm volatile("wfi");
  }
}

/* ---- Serial console ------------------------------------------------------ */

#if STRIDE_CONSOLE

#if STRIDE_CONSOLE_USART_V1
#define UART_STATUS REG(STRIDE_CONSOLE_BASE + 0x00UL) /* SR */
#define UART_RX REG(STRIDE_CONSOLE_BASE + 0x04UL)     /* DR */
#define UART_TX REG(STRIDE_CONSOLE_BASE + 0x04UL)     /* DR */
#define UART_BRR REG(STRIDE_CONSOLE_BASE + 0x08UL)
#define UART_CR1 REG(STRIDE_CONSOLE_BASE + 0x0CUL)
#define UART_CR1_UE (1UL << 13)
#else
#define UART_CR1 REG(STRIDE_CONSOLE_BASE + 0x00UL)
#define UART_BRR REG(STRIDE_CONSOLE_BASE + 0x0CUL)
#define UART_STATUS REG(STRIDE_CONSOLE_BASE + 0x1CUL) /* ISR */
#define UART_RX REG(STRIDE_CONSOLE_BASE + 0x24UL)     /* RDR */
#define UART_TX REG(STRIDE_CONSOLE_BASE + 0x28UL)     /* TDR */
#define UART_CR1_UE (1UL << 0)
#endif
#define UART_CR1_RE (1UL << 2)
#define UART_CR1_TE (1UL << 3)
#define UART_STATUS_RXNE (1UL << 5)
#define UART_STATUS_TXE (1UL << 7)

/* Gives a pin to its alternate function (the function select first, so the
 * pin never drives the wrong signal). */
static void pin_alternate(uint32_t port, uint32_t pin, uint32_t function) {
  volatile uint32_t *afr = &REG(port + 0x20UL + (pin >= 8 ? 4UL : 0UL));
  const uint32_t shift = (pin & 7u) * 4u;
  *afr = (*afr & ~(0xFUL << shift)) | (function << shift);
  REG(port) = (REG(port) & ~(3UL << (pin * 2u))) | (2UL << (pin * 2u));
}

static void serial_init(void) {
  REG(STRIDE_CONSOLE_TX_ENABLE_REG) |= 1UL << STRIDE_CONSOLE_TX_ENABLE_BIT;
  REG(STRIDE_CONSOLE_RX_ENABLE_REG) |= 1UL << STRIDE_CONSOLE_RX_ENABLE_BIT;
  REG(STRIDE_CONSOLE_ENABLE_REG) |= 1UL << STRIDE_CONSOLE_ENABLE_BIT;
  (void)REG(STRIDE_CONSOLE_ENABLE_REG); /* let the clock reach it */

  pin_alternate(STRIDE_CONSOLE_TX_PORT, STRIDE_CONSOLE_TX_PIN, STRIDE_CONSOLE_AF);
  pin_alternate(STRIDE_CONSOLE_RX_PORT, STRIDE_CONSOLE_RX_PIN, STRIDE_CONSOLE_AF);

  UART_CR1 = 0;
  UART_BRR = (uint32_t)((STRIDE_CONSOLE_CLOCK_HZ + STRIDE_CONSOLE_BAUD / 2UL) /
                        STRIDE_CONSOLE_BAUD);
  UART_CR1 = UART_CR1_UE | UART_CR1_RE | UART_CR1_TE;
}

void __stride_stm32_serial_write_byte(uint8_t byte) {
  while ((UART_STATUS & UART_STATUS_TXE) == 0) {
  }
  UART_TX = byte;
}

int32_t __stride_stm32_serial_read_byte(void) {
  if ((UART_STATUS & UART_STATUS_RXNE) == 0) {
    return -1;
  }
  return (int32_t)(UART_RX & 0xFFu);
}

#else /* The board has no console: Print goes nowhere. */

static void serial_init(void) {}
void __stride_stm32_serial_write_byte(uint8_t byte) { (void)byte; }
int32_t __stride_stm32_serial_read_byte(void) { return -1; }

#endif

static void serial_write_str(const char *s) {
  while (*s) {
    __stride_stm32_serial_write_byte((uint8_t)*s++);
  }
}

void __stride_stm32_serial_print_int(int32_t value) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%ld", (long)value);
  serial_write_str(buf);
}

void __stride_stm32_serial_print_real(float value) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%g", (double)value);
  serial_write_str(buf);
}

void __stride_stm32_serial_newline(void) { serial_write_str("\r\n"); }

/* ---- Newlib stdio / POSIX syscall stubs ------------------------------- */

#include <sys/stat.h>

int _write(int file, char *ptr, int len) {
  if (file != 1 && file != 2) {
    return -1;
  }
  for (int i = 0; i < len; ++i) {
    if (ptr[i] == '\n') {
      __stride_stm32_serial_write_byte('\r');
    }
    __stride_stm32_serial_write_byte((uint8_t)ptr[i]);
  }
  return len;
}

int _read(int file, char *ptr, int len) {
  if (file != 0 || len <= 0) {
    return 0;
  }
  int32_t ch = __stride_stm32_serial_read_byte();
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
  clock_init();
  serial_init();
  systick_init_1khz();

  if (RootDomain_load) {
    RootDomain_load();
  }
  if (STM32TickDomain_load) {
    STM32TickDomain_load();
  }
  if (STM32LoopDomain_load) {
    STM32LoopDomain_load();
  }

  if (RootDomain_init) {
    RootDomain_init(__stride_RootDomain_state);
  }
  if (STM32TickDomain_init) {
    STM32TickDomain_init(__stride_STM32TickDomain_state);
  }
  if (STM32LoopDomain_init) {
    STM32LoopDomain_init(__stride_STM32LoopDomain_state);
  }

  if (RootDomain_process) {
    RootDomain_process(__stride_RootDomain_state);
  }

  for (;;) {
    __asm volatile("cpsid i" ::: "memory");
    uint32_t ticks = s_pendingTicks;
    s_pendingTicks = 0;
    __asm volatile("cpsie i" ::: "memory");

    while (ticks--) {
      if (s_tickSleepTicks > 0) {
        s_tickSleepTicks--;
      } else if (STM32TickDomain_process) {
        STM32TickDomain_process(__stride_STM32TickDomain_state);
      }
    }

    if (s_loopSleeping &&
        (int32_t)(__stride_stm32_millis() - s_loopWakeMillis) >= 0) {
      s_loopSleeping = false;
    }
    if (!s_loopSleeping && STM32LoopDomain_process) {
      STM32LoopDomain_process(__stride_STM32LoopDomain_state);
    }
  }
}
