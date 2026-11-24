# SWD Flashing & VS Code Debugging (Raspberry Pi Pico / Pico 2)

This guide covers flashing and interactive debugging for Raspberry Pi Pico (RP2040) and Pico 2 (RP2350) using a Raspberry Pi Debug Probe (or Picoprobe) over SWD.

---

## 1. Prerequisites

- **Hardware**: Raspberry Pi Debug Probe (CMSIS-DAP, USB VID:PID `2E8A:000C`) connected to the Pico's 3-pin SWD header (`SWCLK`, `SWDIO`, `GND`).
- **VS Code Extension**: [Cortex-Debug](https://marketplace.visualstudio.com/items?itemName=marus25.cortex-debug) (`marus25.cortex-debug`).
- **Tools**:
  - `arm-none-eabi-gdb` (from the Arm GNU Toolchain).
  - `openocd` (>= 0.12.0). If not in `PATH`, `stride flash --probe` will offer to download and cache it under `~/.stride/tools/openocd/`.

---

## 2. Flashing Over SWD

Build and flash directly over SWD without needing the `BOOTSEL` button:

```powershell
stride flash --probe my_program.stride
```

Or auto-preferring SWD when OpenOCD is available:

```powershell
stride flash --upload-method=auto my_program.stride
```

---

## 3. Debugging in VS Code

### Build with Debug Info
Compile with `-O0` so `stridejit` emits unoptimized code and DWARF debug info into `<program>.elf`:

```powershell
stride build -O0 my_program.stride
```

### Configure `.vscode/launch.json`
Add a Cortex-Debug configuration in your project's `.vscode/launch.json`:

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Pico SWD Debug (OpenOCD)",
      "cwd": "${workspaceFolder}",
      "executable": "${workspaceFolder}/my_program.elf",
      "request": "launch",
      "type": "cortex-debug",
      "servertype": "openocd",
      "device": "RP2040",
      "configFiles": [
        "interface/cmsis-dap.cfg",
        "target/rp2040.cfg"
      ],
      "openOCDPreConfigLaunchCommands": [
        "adapter speed 5000"
      ],
      "svdFile": "${env:PICO_SDK_PATH}/src/rp2040/hardware_regs/rp2040.svd"
    }
  ]
}
```

- **Pico 2 (RP2350)**: Set `"device": "RP2350"`, `"target/rp2350.cfg"`, and `"svdFile": "${env:PICO_SDK_PATH}/src/rp2350/hardware_regs/rp2350.svd"`.
- **OpenOCD Path**: If `openocd` is not in `PATH`, set `"serverpath"` to the cached binary under `~/.stride/tools/openocd/<version>/bin/openocd.exe` (or `openocd` on macOS/Linux).

### Start Debugging
Press **F5** in VS Code. Cortex-Debug starts OpenOCD, flashes `<program>.elf` over SWD, and halts at `main` (or your breakpoints) with full variable and peripheral register inspection.

---

## 4. UART Serial Output Over the Debug Probe (Optional)

By default, `frameworks/Pico/1.0/runtime/CMakeLists.txt` enables USB CDC stdio (`pico_enable_stdio_usb(stride_firmware 1)`). To also route `Print` / `PrintLn` over the Debug Probe's 3-pin UART (`GP0`/`GP1`), enable `pico_enable_stdio_uart(stride_firmware 1)` in `CMakeLists.txt`.
