# StrideRoot

**StrideRoot** (`strideroot`) provides the core standard library, target system definitions, runtime frameworks, platform descriptions, and toolchain configurations for the [Stride](https://github.com/StrideLang/Stride) programming language.

---

## Repository Structure

| Directory | Description |
| :--- | :--- |
| **`library/`** | Standard library modules (`1.0/`), including core types, math, DSP, control, linear algebra, estimation, I/O, sensors, state machines, windowing, and testing utilities. |
| **`systems/`** | Target system definitions (`Desktop`, `DesktopAudio`, `DesktopFunctions`, `JIT`, `AVR`, `Pico`, `SaturnM7`, `Wiring`, `TestSystem`). |
| **`frameworks/`** | Runtime and build frameworks (`JIT`, `Cpp`, `RtAudio`, `DesktopTemplate`, `AVR`, `Pico`, `STM32F7`, `Wiring`, `TestFramework`). |
| **`platforms/`** | Hardware and platform abstractions (`AVR`, `JIT`, `Pico`, `ST`, `Test`, `generic`). |
| **`cores/`** | Processor core definitions shared by several platforms. A core only one platform uses lives in `platforms/<P>/cores/` (e.g., `platforms/AVR/cores/AVR5.stride`), which is searched first. |
| **`tools/`** | External toolchain and firmware definitions (`Arduino`, `ArmCrosscompiler`, `CMake`, `Cpp`, `Git`, `OpenOCD`, `STM32F7_Firmware`). |

---

## Usage

`strideroot` is typically used as a submodule within the main [Stride](https://github.com/StrideLang/Stride) repository and serves as the root directory where the Stride compiler and toolchain resolve standard library imports, target systems, frameworks, and platform definitions.

---

## License

Stride is licensed under the terms of the **GNU Affero General Public License version 3** (AGPLv3).

It is based on code previously released under the **Modified BSD License** (3-Clause BSD), Copyright © 2017 The Regents of the University of California, developed by Andres Cabrera and Joseph Tilbian.

See [`License.txt`](License.txt) for the complete license terms and notices.
