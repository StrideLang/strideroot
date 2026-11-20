# Shared cores

A `core` declares what every chip built on one processor architecture has in
common: its LLVM triple, pointer width, default types, FPU, cache line, codegen
and frameworks. A device names its core with `overlays:`.

Put a core here when devices of several platforms use it, such as an Arm
Cortex-M or RISC-V core that more than one vendor builds on. A core that only
one platform uses goes in that platform's folder instead, as
`platforms/<Platform>/cores/<Name>.stride` (for example
`platforms/AVR/cores/AVR5.stride`).

Stride looks for `<Name>.stride` in the program's folder first, then in the
active platform's `boards/`, `devices/` and `cores/`, and last here.
