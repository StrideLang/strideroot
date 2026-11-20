# Stride Examples

Complete projects that show what Stride can do on each system. Every project
here builds with the current toolchain, and a test builds all of them
(`StridecDriverTest.ExamplesBuild` in stridelang), so they can't silently go
stale.

## Layout

```
examples/
  <System>/            the system named in `use <System> version 1.0`
    <Project>/
      <Project>.stride  the program: the file to build
      *.stride          the project's other files, such as its hardware file
      README.md         what it shows, how to wire, build and run it
```

The program is the file named after its folder. Its other files are found
because imports look next to the program first. A project can carry hardware
files for several boards (`BlinkRig.stride`, `BlinkRigMega.stride`); the
examples test builds the program once for each.

## Examples

| System | Project | Shows |
|---|---|---|
| AVR (Arduino Uno, Mega 2560) | [PortableBlink](AVR/PortableBlink) | Pins through a hardware file (`gpioPin`) and `DigitalIO`, one program on two boards, the standard loop and timer domains, a cooperative `Sleep`, open drain and active-low pins, with wiring diagrams |

## Systems

What each system in `systems/` can build today, and where its examples are:

| System | Framework | Code generation | Examples |
|---|---|---|---|
| AVR | AVR | LLVM; `stride build` makes firmware | here |
| JIT | JIT | LLVM, in the host process; used by libgame and the tests, with simulated GPIO | none yet |
| Pico | Pico | C++ templates (older generator) | `systems/Pico/examples`, not built by this toolchain yet |
| Wiring | Wiring | C++ templates (older generator) | `systems/Wiring/examples`, `frameworks/Wiring/examples`, not built yet |
| Desktop, DesktopAudio, DesktopFunctions | Wiring with Cpp, RtAudio or DesktopTemplate | C++ templates (older generator) | `systems/DesktopFunctions/1.0/examples`, not built yet |
| SaturnM7 | STM32F7 | C++ templates (older generator) | `systems/SaturnM7/examples`, not built yet |

An example moves here when its system builds with the current toolchain, and
then the examples test builds it too.

## Adding an example

1. Make `examples/<System>/<Project>/<Project>.stride` and its other files.
2. Write its README: what it shows, wiring for hardware, and the commands to
   build and run it.
3. Add it to the table above.
4. Build it with the examples test. For AVR that needs avr-gcc; the test
   skips firmware builds without it.
