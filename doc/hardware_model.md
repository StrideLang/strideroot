# The hardware model

Status: the peripheral entity, `@` addresses, the family layer, `gpioLayout`
and the console are implemented for STM32F4/F7. Generated device files,
per-kind drivers and multi-core devices are plans.

How to add a device or board: `platforms/AGENTS.md`. Clocks:
`clocking.md`. Distribution and lifecycle: `hardware_components.md`.

## Goals

- Thousands of chips and boards, with no chip names in C++ or runtimes.
- Each fact written once: per register design, per family, per chip, per
  board.
- One description that serves validation, LLVM/JIT code generation, C++ code
  generation and tools (flashers, debuggers, documentation).
- A user can describe their own board in one small file.

## Layers

`core -> family -> chip -> board -> project rig`, joined with `overlays`. Each
layer states what it adds. Records merge key by key, named lists merge by
name (an entry of the same name and type merges key by key, so a device's
`openocd OpenOCD { target: }` and its board's `openocd OpenOCD { interface: }`
become one), and other values are replaced. The family layer is an ordinary `device`
that chips overlay. The resolver already allows a device to overlay a device.

The chain is linear. Two facts don't fit it:

- A register design shared by families on different cores. STM32 GPIO v2 is
  the same on the M4-based F4 and the M7-based F7, so its `gpioLayout` is
  written twice.
- Board settings shared by all boards of a family: `toolchain`, `image` and
  the OpenOCD transport repeat in every STM32 board.

The plan: allow **mixins**, `overlays: [STM32F4xx, Stm32GpioV2]`. Layers
after the first add properties without changing the chain's kind order.
Until then, keep the duplication small and identical.

## Peripherals

```stride
import Peripheral

peripherals: [
    uart USART2 @ 0x40004400 { ip: "stm32.usart.v1" bus: "apb1" enable: { register: 0x40023840 bit: 17 } },
    spi SPI2 @ 0x40003800 { ip: "stm32.spi.v1" bus: "apb1" enable: { register: 0x40023840 bit: 14 } alsoAs: ["i2s"] },
    i2c I2C1 @ 0x40005400 { ip: "stm32.i2c.v1" bus: "apb1" enable: { register: 0x40023840 bit: 21 } }
]
```

### Why these parts

- **`@` is the base address.** The parser stores it as the declaration's
  `_at`, where `@` already means "where this lives": `@ Domain` for streams,
  `Sin@Poly5` for variants. A hex literal after `@` can't be mistaken for
  either, because variant tags are names. `gpioPort GPIOA @ 0x40020000`
  uses the same form.
- **The entity type is the function** (`uart`, `spi`, `i2c`, `i2s`, `sai`,
  `timer`, `adc`, `dac`, `dma`, `can`, `usb`). Portable libraries ask for
  "a uart", whatever the vendor calls it (USART, UART, UARTE, PL011).
- **`ip` is the register design and version**, written
  `<vendor>.<block>.v<n>`, for example `stm32.usart.v1`, `nrf.uarte.v1`,
  `rp.pl011.v1`. Drivers, register layouts and code generators are keyed by
  `ip`, never by chip. A new chip that reuses known designs needs only data.
- **One base type, small kinds.** `_PeripheralType` holds what every
  instance has: `ip`, `bus` (its clock domain), `enable` (clock gate),
  `reset` and `alsoAs`. A kind adds only what code generation needs to
  choose or check something: `timer { width channels }`,
  `adc { bits channels }`, `dma { channels }`. Register-level detail belongs
  to the `ip`, not to each instance.
- **Multi-function blocks** are declared once as their main kind, with
  `alsoAs: ["i2s"]`. One instance, one address, one clock gate, and a
  resource conflict if two roles claim it.

### What uses them

- **Boards assign roles.** `console: { peripheral: USART2 tx: PA2 rx: PA3 select: 7 }`
  picks an instance and its pins. The validator reserves those pins, and
  later the instance too: a program that opens USART2 itself on a board
  whose console is USART2 gets an error.
- **Drivers per ip.** An `ip` description gives register offsets and field
  encodings, as `gpioLayout` does for GPIO:
  ```stride
  peripheralIp Stm32UsartV1 {
      ip: "stm32.usart.v1"
      registers: { status: 0x00 data: 0x04 baud: 0x08 control: 0x0C }
      fields: [ ... ]
  }
  ```
  - The LLVM backend emits register access from it, as it does for GPIO
    today.
  - The C++ generator emits the same accesses, or calls a vendor HAL.
  - Today the STM32 C runtime has one console driver for each of the two
    USART designs. It's the first instance of this pattern, written in C.
- **Pins.** A pin's alternate functions (`pinFeatures` with
  `pinRoute { function: uartTx select: 7 }`, which the GPIO setup already
  applies for `mode: PinFunction`) will name the instance:
  `pinRoute { function: uartTx peripheral: USART2 select: 7 }`. Then the
  validator checks the board's `console.select`, and the console's pin setup
  moves out of the runtime into the same register plan that GPIO pins use.

## Generated device files

Hand-writing hundreds of chips doesn't scale, and their addresses must be
right. `tools/devicegen` (planned) writes the family and chip files from
vendor data:

| Source | Gives |
|---|---|
| CMSIS-SVD (vendor packs, `cmsis-svd-data`) | peripheral instances and base addresses, register layouts, interrupt numbers |
| Pin databases (`STM32_open_pin_data`, NXP/Nordic equivalents) | packages, pins, each pin's alternate functions with their instance |
| Vendor device lists | memory sizes, FPU, maximum clock, per-part names (probe-rs chip) |
| Hand-written overrides | clock limits, flash wait-state tables, `ip` assignments where SVD names are ambiguous, errata |

Its rules:

- Output is committed `.stride` files that start with
  `# Generated by tools/devicegen from <sources>. Do not edit.`
- Fixes go in the overrides file.
- A test regenerates the files and fails on any difference.
- One family file per register-compatible family, and one small chip file
  per part.
- `ip` is assigned in the overrides: "every STM32F4 USART is
  stm32.usart.v1". The SVD only checks it, by register layout.

## Scaling to other platforms

The layers and the peripheral entity stay. What changes is how many cores a
device has, and whether peripherals are registers or OS resources.

### Microcontrollers from other vendors

These are the same model with new `ip` strings and a clock tree:

- RP2040/RP2350: `rp.pl011.v1`, PIO as `pio` with `ip: "rp.pio.v1"`.
- nRF: `uarte`-style easyDMA peripherals, with a `routing` property for
  PSEL.
- ESP32: GPIO matrix routing, which `pinRoute { register: }` already models.

### Heterogeneous microcontrollers (NXP i.MX RT595)

The RT595 has a Cortex-M33 and a Cadence HiFi4 DSP, shared SRAM seen at
different addresses by each core, a messaging unit (MU), TrustZone, and one
clock controller with per-core roots. Model it as follows:

- **One device, several cores:**
  ```stride
  device MIMXRT595 {
      cores: [
          coreInstance Main { core: CortexM33  role: "main" },
          coreInstance Dsp  { core: XtensaHiFi4  bootedBy: Main }
      ]
      memory: [
          region Sram @ 0x20000000 { size: 5242880  views: { Dsp: 0x00200000 } }
      ]
      peripherals: [
          uart Flexcomm0 @ 0x40106000 { ip: "nxp.flexcomm.usart.v1" bus: "fc0" reach: [Main] },
          mailbox Mu @ 0x40110000 { ip: "nxp.mu.v1" reach: [Main, Dsp] }
      ]
  }
  ```
  - Each core has its own triple, codegen and toolchain. The HiFi4 uses
    `Codegen::Cpp` with Cadence's compiler until LLVM's Xtensa backend is
    usable.
  - `reach` lists which cores can use a peripheral, and the validator
    checks it.
  - A memory region has one address per core view.
- **A system with one platform per core**, joined by a `connection` over the
  MU and shared memory. Stride `system`s already join platforms with
  connections, so one program can place domains on each core, and the
  compiler generates both images and the transport between them.
- **TrustZone**: `secure` and `nonSecure` views of the same peripheral
  (aliased addresses), and a `world` property on domains. Later.

### Application processors (Qualcomm Snapdragon)

On a QCS6490-class SoC, Stride runs on Linux on the Kryo CPU cores (JIT or
C++ frameworks). The Hexagon DSP is reached through FastRPC, and the GPU
through OpenCL or Vulkan. Registers belong to the kernel. Model it as
follows:

- **Peripherals are OS resources**, with an OS `ip`: `ip: "linux.i2c-dev"`,
  `"linux.gpio-cdev"`, `"linux.spidev"`, `"linux.alsa"`. The address is the
  OS path: `i2c Bus1 @ "/dev/i2c-1"`. This needs the parser to accept a
  string after `@`, a small rule like the hex one.
- **Drivers per ip** are thin wrappers over the OS API, so the same Stride
  program uses "a uart" on a microcontroller or on Linux.
- **The board** (a dev kit such as the RB3 Gen 2) maps header pins to
  `gpiochip` lines and buses. Clocks are read-only facts the OS reports, with
  no tree to program.
- **Compute units** (Hexagon, GPU) are platforms in the system, with
  `connection`s for FastRPC or shared buffers. The vendor SDKs are
  `systemDependencies` of their components (`hardware_components.md`),
  installed by the user because of their licenses.

## Floating point ABI

STM32 cores with an FPU (Cortex-M4F, M7) use the hard-float ABI. Their
triple is `thumbv7em-none-eabihf`, so floats are passed in FPU registers, and
the runtime is built `-mfloat-abi=hard` against newlib's hard multilib.

- The rule: every object linked into the firmware must use the same ABI.
  That covers the runtime, the C library and a component's prebuilt
  archives (`hardware_components.md`), and GNU ld rejects a mix. So the ABI
  is fixed per framework, never a user option, and it is part of the triple
  that `nativeTargets` keys archives by.
- Hard-float saves moving each float between core and FPU registers at
  calls, such as calls into libm. Variadic calls (`printf`) still use core
  registers, as the AAPCS requires.
- The Cortex-M33 (Pico 2) stays `eabi` (softfp) to match the Pico SDK, which
  builds RP2350 code softfp unless `PICO_HARD_FLOAT_ABI` is set. Switching it
  is a Pico framework decision.
- Before November 2026, every `...-eabihf` triple silently produced softfp
  code: `llvm::Triple` read the third part of `thumbv7em-none-eabihf` as the
  OS, so the environment was unknown. stridejit now normalizes triples, and
  `CrossCompileTest` checks the object's `Tag_ABI_VFP_args`.

## Later

- **Interrupts.** The startup file has only the 16 core vectors, so devices
  declare no interrupts. The plan:
  - an `irq` number per peripheral, from SVD;
  - a generated full vector table;
  - domains triggered by interrupts, as AVR's `gpioInterrupt` does today.
- **Mixins** for shared register designs and board defaults (see "Layers").
- **Resource claims.** Peripherals become resources that domains and roles
  claim, so conflicts are compile-time errors.
