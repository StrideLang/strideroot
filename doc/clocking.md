# Clocking in Stride

Status: plan. Step 1 is implemented for STM32F2/F4/F7, and the rest is
design.

## Why

A program's timing, baud rates, timer periods, audio sample rates and flash
wait states all follow from the clocks. Today a device states `clock: <Hz>`
and the runtime has to make that true. Until November 2026 the STM32 runtime
didn't: it ran at the 16 MHz reset clock while the program was told 168 MHz.

The goal is that clocks are:

1. **Described** as data: what the chip can do, what the board adds
   (crystals, external clocks), and what the project asks for.
2. **Validated** at compile time against the chip's limits, with errors that
   name the clock and the limit.
3. **Solved**: a project asks for frequencies ("core as fast as possible, USB
   at 48 MHz, I2S at 48 kHz × 256"), and the compiler finds the settings.
4. **Generated** as a configuration that each backend emits in its own form:
   - the LLVM/JIT backend emits the register sequence into the program;
   - the C++ generator writes a C init function and `constexpr` frequencies;
   - a C runtime includes a generated header.
5. **Known to programs and to other passes**: the frequencies become
   compile-time constants for timers, baud rates and sample rates.

## Step 1 (done): STM32 records, validation, header

The device (family) gives `clocks` and `clockLimits`, and the board may change
`clocks`, for example to set its crystal:

```stride
clock: 168000000
clocks: {
    source: "hsi"  hsi: 16000000
    pll: on  pllM: 16  pllN: 336  pllP: 2  pllQ: 7
    ahbDivider: 1  apb1Divider: 4  apb2Divider: 2
    flashFlags: 0x0700  voltageScale: 0x4000
}
clockLimits: {
    sysclk: { max: 168000000 }  apb1: { max: 42000000 }  apb2: { max: 84000000 }
    vcoIn: { min: 950000 max: 2100000 }  vcoOut: { min: 100000000 max: 432000000 }
    pllM: { min: 2 max: 63 }  pllN: { min: 50 max: 432 }  pllP: { values: [2, 4, 6, 8] }
    pllQ: { min: 2 max: 15 }
    flashLatency: { stepHz: 30000000 max: 7 }
}
```

`stride build` (`stridelang/src/stm32_board.cpp`) does four things:

1. It computes every frequency.
2. It checks each frequency and factor against `clockLimits`.
3. It works out the flash wait states and whether over-drive is needed.
4. It checks that the result equals `clock`.

It then writes `stride_board.h`, and the runtime's `clock_init()` programs
the RCC from it. This step is STM32-RCC-specific C++ code. Everything after
it removes that.

## The model

### Clock tree

A clock tree is a graph of nodes, declared in the family (or generated, see
`hardware_model.md`). The node kinds:

| Kind | What it is | Its data |
|---|---|---|
| `oscillator` | Fixed internal source (HSI, LSI, RC) | frequency, accuracy |
| `crystalInput` | Pins a board can put a crystal or a clock on (HSE, LSE) | allowed range; `bypass` for an external clock |
| `pll` | Input divider, multiplier and output dividers | ranges for each factor; VCO input and output ranges; fractional part |
| `divider` | Prescaler | allowed values |
| `mux` | Picks one of its inputs | the inputs |
| `gate` | A clock enable feeding one peripheral | (what `enable:` is today) |
| `clockDomain` | A named clock that consumers refer to (`sysclk`, `hclk`, `apb1`, `pll48`, `i2s`) | max frequency, which may depend on power mode |

Every adjustable node maps to a register field and an encoding:

```stride
pll MainPll {
    input: PllSource            # a mux node
    m: { field: { register: 0x40023804 shift: 0 width: 6 }  range: [2, 63] }
    n: { field: { register: 0x40023804 shift: 6 width: 9 }  range: [50, 432] }
    p: { field: { register: 0x40023804 shift: 16 width: 2 } values: [2, 4, 6, 8] encode: [0, 1, 2, 3] }
    q: { field: { register: 0x40023804 shift: 24 width: 4 } range: [2, 15] }
    vcoIn: { min: 950000 max: 2100000 }
    vcoOut: { min: 100000000 max: 432000000 }
    enable: { register: 0x40023800 bit: 24 }
    ready: { register: 0x40023800 bit: 25 }
}
divider Apb1 { input: Hclk  field: { register: 0x40023808 shift: 10 width: 3 }
               values: [1, 2, 4, 8, 16]  encode: [0, 4, 5, 6, 7] }
clockDomain Apb1Clock { from: Apb1  max: 42000000 }
```

The values-and-encodings pattern is the one GPIO already uses
(`gpioField { options: codes: }`), so one evaluator can serve both.

### Constraints that aren't a node

- **Flash wait states** are a table indexed by HCLK and supply voltage:
  `flashLatency: { table: [[30000000, 0], [60000000, 1], ...] voltage: "2.7-3.6" }`.
- **Power**: voltage scaling and over-drive thresholds, as rules such as
  "above 144 MHz needs scale 1" or "above 180 MHz needs over-drive". Each
  rule has the register fields it sets and the ready bits to wait for.
- **Peripheral needs**: a peripheral names the domain that clocks it
  (`bus: "apb1"` today, a node reference later), and its kind may add
  requirements. For example USB needs 48 MHz ± 0.25%, and SDMMC at most
  50 MHz. They are checked like limits.
- **Board facts**: the crystal frequency, whether it is bypassed, and an
  external clock on a pin (I2S_CKIN, or MCO from another chip).

### Sequencing

The order of the writes depends on the clock controller's design, not on the
chip:

1. Raise the voltage scale.
2. Start the source and wait until it's ready.
3. Program the PLL while it's off, start it, and wait.
4. Turn on over-drive.
5. Set flash latency before raising the clock.
6. Set the prescalers.
7. Switch the mux and wait for its status.

That order is written once per clock-controller `ip` (`stm32.rcc.f4`,
`rp.clocks.v1`, `nxp.ccm.rt`), as a list of steps over the node names:

```stride
clockSequence Stm32RccF4 {
    steps: [
        set { node: VoltageScale },
        start { node: Hse }, wait { node: Hse ready: on },
        stop { node: MainPll }, configure { node: MainPll }, start { node: MainPll }, wait { node: MainPll ready: on },
        set { node: OverDrive }, set { node: FlashLatency }, set { node: [Ahb, Apb1, Apb2] },
        select { node: SysclkMux }, wait { node: SysclkMux status: on }
    ]
}
```

A step that a node doesn't need does nothing, for example `start Hse` when
running from HSI.

### What the project writes

A project can ask for clocks in either of two ways.

**By request** (the usual way). It names frequencies and lets the compiler
solve for the settings:

```stride
clocks: {
    want: [
        clockRequest { domain: Hclk  frequency: max },
        clockRequest { domain: Pll48  frequency: 48000000  tolerance: 0.0025 },
        clockRequest { domain: I2s  frequency: 12288000  tolerance: 0.0001 }
    ]
}
```

The solver searches the PLL factors (about 10^5 to 10^6 combinations per PLL,
which is trivial at compile time). It scores the candidates by how far each
request misses, then by lower VCO jitter (higher VCO input), then by lower
power. It reports the frequencies it achieved.

**By setting** (what step 1 does). It gives the factors directly, and the
compiler only validates them.

`clock` stops being written by hand. It becomes the solved HCLK, so it can't
disagree with the clocks.

## Output: the clock plan

Solving produces a `ClockPlan`:

- the frequency of every domain;
- an ordered list of operations: `write { register, clearMask, setBits }`
  (`RegisterWrite` from the GPIO setup) and
  `waitUntil { register, mask, value }`;
- the board facts it relied on.

Each backend emits the same plan:

| Backend | Emits |
|---|---|
| LLVM / JIT (`Codegen::Llvm`) | `RootDomain_load` starts with the operations as volatile stores and polling loops, like the GPIO setup's `llvm::gpio modify` with an added `llvm::gpio wait`. The C runtime then has no clock code. |
| C++ generator (`Codegen::Cpp`) | `stride_clock_init()` in the generated C++, and `constexpr` frequencies in a generated header. |
| C runtime (interim, step 1) | `stride_board.h` macros |
| Hosted targets (Linux, macOS) | Nothing to program: the frequencies are read-only facts, or absent |

Programs see the result as constants: `PlatformClocks::Core`, plus one per
domain. The timer solver, baud-rate checks and audio sample-rate checks read
those constants, so they can't go stale.

## Steps

1. **(done)** STM32 `clocks` and `clockLimits` records, validation, the
   `stride_board.h` header, and runtime `clock_init()`.
2. Typed clock-tree schema (`library/1.0/Clock.stride`), a generic evaluator
   (frequencies from tree plus settings), and the STM32F4/F7 trees. The
   evaluator must reproduce step 1's numbers exactly; that is the test.
3. Register encodings and `clockSequence`, producing the operation list. The
   LLVM backend emits it into `RootDomain_load`, and the runtime's
   `clock_init()` goes away.
4. The solver and `clockRequest`. Peripherals' `bus` becomes a node
   reference, with per-kind needs checked.
5. Other designs: RP2040/RP2350 (XOSC, PLL_SYS, PLL_USB, clk_sys/clk_peri
   muxes), nRF (fixed HFCLK), i.MX RT (CCM: several PLLs with PFDs, per-core
   roots), ESP32. Multi-core chips get a domain per core (`hardware_model.md`,
   "Scaling").
6. Low-power modes: alternative plans per power state, switched at run time.
   Out of scope until domains can change rate.
