# Adding devices and boards

Read this before you add or change anything under `platforms/` or `cores/`.
The design behind it is in `doc/hardware_model.md` (peripherals, generated
device files), `doc/clocking.md` (clocks) and `doc/hardware_components.md`
(distribution, lifecycle).

## The rule

**Facts about hardware live in Stride files, never in C++ or in a runtime.**
The compiler, `stride build` and the runtimes read the data and act on it.
They may know a *register design* (an `ip`, such as `stm32.usart.v1`), but
never a chip, family or board name. If you need `if (chip == ...)` anywhere,
the data is missing a property: add the property to the schema
(`library/1.0/Framework.stride`, `Gpio.stride`, `Peripheral.stride`,
`Flasher.stride`) instead.

## Layers

Each layer `overlays` the one below. Properties merge upward: a record merges
key by key, a list of named entries merges by name (an entry of the same
name and type merges key by key; another type replaces it), and anything else is
replaced. So each layer states only what is new or different.

| Layer | Example | Holds |
|---|---|---|
| core | `cores/CortexM4.stride` | triple, pointer width, types, FPU present, codegen |
| family | `platforms/STM32F4/devices/STM32F4xx.stride` | cpu, fpu, clocks and clockLimits, gpioLayout, ports, peripherals, flashers (OpenOCD's target), resources |
| chip | `platforms/STM32F4/devices/STM32F407VG.stride` | memory, what differs from the family (fpu, clock, clockLimits), flashers (probe-rs' chip) |
| board | `platforms/STM32F4/boards/STM32F4Discovery.stride` | pins and their names, console, crystal (clocks.hse), flashers (probe, speed), defaultFlasher, usb ids, image, toolchain |
| project rig | `examples/PortableBlink/BlinkRigSTM32F4Discovery.stride` | gpioPins: how the project uses the board's pins; clock changes |

A family layer is a `device` that other devices overlay. List it in the
platform file's imports, but not in `supportedDevices` (it is not a chip you
can buy).

## Writing a family

- **Ports**: give each port its base address with `@`, and put the register
  offsets every port shares in one `gpioLayout` record:
  ```stride
  gpioLayout: { width: 32 input: 0x10 output: 0x14 outputSet: 0x18 ... fields: [...] }
  ports: [
      gpioPort GPIOA @ 0x40020000 { enable: { register: 0x40023830 bit: 0 } },
      ...
  ]
  ```
  A port that states a register itself overrides the layout.
- **Peripherals**: one entry per instance, at its base address, in the kind's
  vocabulary from `library/1.0/Peripheral.stride` (`uart`, `spi`, `i2c`,
  `i2s`, `sai`, `timer`, `adc`, `dac`, `dma`, `can`, `usb`):
  ```stride
  uart USART2 @ 0x40004400 { ip: "stm32.usart.v1" bus: "apb1" enable: { register: 0x40023840 bit: 17 } }
  ```
  - The kind says what the peripheral does.
  - `ip` names its register design, so drivers are written per `ip`. Two
    instances share an `ip` only if one driver runs both unchanged.
  - `bus` names the clock that feeds it, using a name from the clocks.
  - If an instance can do a second job, list that kind in `alsoAs`
    (for example STM32 `SPI2` with `alsoAs: ["i2s"]`).
- **Clocks**: `clock` is the core clock the runtime sets up, `clocks` says how
  it is made, and `clockLimits` says what the silicon allows. `stride build`
  checks that `clocks` stays within `clockLimits` and that the result equals
  `clock`. Default to the internal oscillator, so the family works on any
  board. See `doc/clocking.md`.
- **No interrupts yet**: don't declare interrupt capabilities until the
  runtime has vectors for them (`doc/hardware_model.md`, "Later").

## Writing a chip

Overlay the family, and add only `memory`, the chip's probe-rs name
(`flashers: [probeRs ProbeRs { chip: }]`), its resources (for example the FPU precision) and anything that
differs from the family. Name the package in the header comment.

## Writing a board

- Overlay the chip.
- List `pins`, using both the silkscreen names and the chip names.
- Add `console: { peripheral: tx: rx: select: baud: }` when the board has a
  serial port wired to USB or a header. `select` is the pins' alternate
  function number. The console's pins are reserved: a project's `gpioPin`
  on them is an error.
- Give the crystal as `clocks: { hse: <Hz> }`, even when the default clocks
  don't use it.
- Say how it is flashed in `flashers`, using the same entry names as the
  device so they merge: `openocd OpenOCD { interface: "interface/stlink.cfg"
  speed: 2000 }`. Leave out `transport` unless the probe needs one; OpenOCD's
  interface script picks it. When the device lists another tool first, name
  the one `stride flash` uses: `defaultFlasher: Picotool`. The kinds and
  their options are in `library/1.0/Flasher.stride`; a new kind needs a type
  there and a runner in stridec (`target_runner.cpp`).
- Say in the header comment which board revisions the file covers and what
  differs between them (for example, which revisions have a USB serial port).

A user's own board is the same file in their project folder (it is searched
first) or in their own component (`doc/hardware_components.md`).

## Where values come from

Copy addresses and limits from the vendor's reference manual and datasheet,
and cite the section in a comment. The goal is to generate family and chip
files instead of writing them by hand (`doc/hardware_model.md`, "Generated
device files"):

- CMSIS-SVD gives peripheral base addresses, registers and interrupts.
- The vendor's pin database gives packages, pins and each pin's alternate
  functions. For STM32 that is `STM32_open_pin_data`;
  `platforms/ST/stm32f4.stride` is an older hand-made table of the same data.
- The datasheet's electrical characteristics give clock limits, flash wait
  states and voltage ranges. No machine-readable source exists for these, so
  they stay hand-written in a small overrides file that the generator merges.

When the generator exists, a generated file starts with
`# Generated by tools/devicegen from <sources>. Do not edit.`, and fixes go in
the overrides file.

## Checks before you finish

1. `HardwareResolver` resolves the board, and the values you added come out
   right. Add a test to `stridegen/tests/hardwareresolver_tests.cpp`.
2. For an STM32 board, `resolveStm32Board` accepts it and the clocks are the
   ones you expect. Add a test to `stridelang/tests/cli_test.cpp`.
3. `stride build examples/PortableBlink/PortableBlink.stride -D Board::<Board>`
   links. Then check the ELF with `arm-none-eabi-readelf -A` (CPU and FPU),
   look at the vector table (`objdump -s -j .isr_vector`: the initial stack
   pointer is at the top of RAM), and disassemble the console code to confirm
   its register addresses.
4. Write down what has not been tried on hardware.
