# Raspberry Pi Pico: Features Deferred for Future Work

This document tracks features intentionally excluded from the initial Raspberry Pi Pico (RP2040 & RP2350) LLVM/JIT implementation so they can be designed and added cleanly in later phases.

---

## 1. Numeric Types & Math Acceleration

- **Fixed-Point Arithmetic Support**:
  - Language/framework support for fixed-point types (e.g., Q15/Q31 or configurable fractional formats) in `Types.stride`, `stridegen`, and `stridejit` codegen so RP2040 programs can avoid software floating-point overhead entirely.
- **RP2040 Bootrom Float & Divider Acceleration**:
  - Wiring `PicoBootromFloat` (ROM-based single-precision float routines) and RP2040's SIO hardware integer divider into `Math.stride` / `Comparisons.stride` or the linker runtime.
- **RP2040 Hardware Interpolator**:
  - Exposing `PicoHardwareInterpolator` (SIO `INTERP0`/`INTERP1`) for fast table lookups and linear interpolation.
- **Float64 / Double-Precision Option**:
  - Optional policy or type override for programs that explicitly require `Float64` despite software emulation cost.

---

## 2. Bare-Metal Runtime (Removing `pico-sdk` Dependency)

- **Self-Contained C Runtime**:
  - Replace the `pico-sdk` build dependency with a minimal bare-metal runtime (startup vector table, linker script, RP2040 boot2 stage, PLL/XOSC clock initialization, and lightweight USB CDC or UART driver), matching the self-contained model of `frameworks/AVR/1.0/runtime/stride_avr_runtime.c`.
- **Direct `arm-none-eabi-gcc` Linking**:
  - Allow `stride build` to compile and link directly with `arm-none-eabi-gcc` + `elf2uf2` without requiring a local `pico-sdk` installation or CMake invocation.
- **Custom `snprintf` / Float Formatting (`strideruntime_string.c`)**:
  - Currently, Pico compiles without `STRIDE_RT_NO_STDIO` and uses the `pico-sdk` `snprintf`. Implement a custom `snprintf` / formatting path only if it provides measurably better performance, smaller flash footprint, or stronger safety guarantees (e.g., ISR-safe / bounded stack usage) than the SDK implementation.

---

## 3. Multicore & Real-Time Execution

- **Core 1 Execution & Multicore Domains**:
  - Assigning domains to Core 1 (`pico/multicore.h`), inter-core FIFO/spinlock synchronization, and safe shared state across cores.
  - Note: Once Core 1 is enabled, `<Domain>_load` one-time initialization guards and shared domain state will require hardware spinlocks (RP2040 has no `ldrex`/`strex` atomics, while RP2350 Cortex-M33 does).
- **Interrupt-Driven / Preemptive Timer Domains**:
  - Moving `PicoTimerDomain` from cooperative main-loop dispatch to true hardware timer / alarm ISRs with safe state synchronization.

---

## 4. Additional Peripherals & Interfaces

- **Analog I/O (ADC)**: `AnalogIn` support for GP26–GP29 (and internal temperature sensor).
- **PWM**: Hardware PWM slices/channels (`AnalogOut` / `PwmOut`).
- **Serial Buses**: Hardware `I2C`, `SPI`, and multi-channel `UART` (`uart0`, `uart1`) beyond stdio `Print`/`PrintLn`.
- **PIO (Programmable I/O)**: Stride abstractions or platform modules for RP2040/RP2350 PIO state machines (e.g., WS2812/NeoPixel, I2S audio, quadrature encoders).
- **Audio & DSP**: Restoring/porting `Audio.stride`, `Buffer.stride`, and `OSC.stride` capabilities from the legacy Pico framework onto the LLVM backend.
- **DMA**: Hardware DMA channel support for zero-CPU buffer transfers.

---

## 5. Additional Boards & Wireless Variants

- **Pico W / Pico 2 W (CYW43439)**:
  - On Pico W / Pico 2 W, the on-board `LED` is connected to the CYW43 wireless chip rather than `GP25` SIO, requiring a driver-backed `gpioPin` path (`driverAccess`) instead of direct `llvm::gpio` register writes.
  - Wi-Fi / BLE networking and `OSC` support.
- **Third-Party RP2040 / RP2350 Boards**:
  - Board definitions for SparkFun Pro Micro RP2040, Adafruit Feather RP2040/RP2350, Pimoroni boards, etc.

---

## 6. Tooling & Developer Experience

- **Mass-Storage `.uf2` Fallback in `stride flash`**:
  - Automatic detection of mounted `RPI-RP2` / `RP2350` USB drives when `picotool` is not installed.
- **Serial Monitor**:
  - Integrated `stride monitor` (or USB CDC port detection) for viewing `Print`/`PrintLn` output.
- **Interactive SWD Debugging (`stride debug`)**:
  - Built-in `stride debug` command launching `openocd` as a GDB server (port `3333`) and attaching `arm-none-eabi-gdb` (or generating VS Code Cortex-Debug configurations) against `stride_firmware.elf` with DWARF debug info.
- **RP2350 Pad Isolation (`ISO`) & Security**:
  - Automatic handling of RP2350's `PADS_BANK0` `ISO` bit in startup pin initialization (`HardwareResolver::pinSetupWrites`) and TrustZone/non-secure configuration if needed.
