# PortableBlink

Blinks an Arduino Uno's on-board LED from the main loop, and drives a relay
module from a button polled by a 1 kHz timer. It shows how a Stride program
uses pins without naming a board.

| File | What it is |
|---|---|
| `PortableBlink.stride` | The program. It uses `BlinkRig::Status`, `BlinkRig::Button` and `BlinkRig::Relay` through `DigitalIO::Led`, `DigitalIn` and `DigitalOut`, in the standard `PlatformLoopDomain` and `PlatformTimerDomain`. |
| `BlinkRig.stride` | The project's hardware file. It overlays `ArduinoUno` and declares how each pin is used (`gpioPin`). This is the only file that names the board. |

## Wiring

| Signal | Uno pin | Wiring |
|---|---|---|
| `Status` | `LED` (D13) | The on-board LED |
| `Button` | D2 | A push button to GND. The internal pull-up holds D2 high. |
| `Relay` | D7 | The input of an active-low relay module. It is driven low while the button is pressed and released otherwise. |

## Build and flash

```sh
stride build PortableBlink.stride            # PortableBlink.hex and .elf
stride flash PortableBlink.stride --port=COM3
```

`stride build` finds `BlinkRig` next to the program first, then the board it
overlays in `platforms/AVR/boards`. It needs avr-gcc, which the Arduino IDE
installs (`stride doctor` shows where it was found).

## What the compiler does with it

- **Checks the wiring against the chip.** Writing `pull: Gpio::PullDown` on
  the button fails with `pull PullDown is not supported by ATmega328P pin
  PortD.2 (supported: PullNone, PullUp)`. Using D0 or D1 fails too, because
  the runtime reserves them for the USB serial port.
- **Sets every pin up once at startup**, before any domain runs. It writes
  the levels first, then the pull-up, then the directions, so the relay
  never clicks at reset.
- **Turns each pin access into direct register access** at constant
  addresses, with no runtime calls. The startup writes are single
  `sbi`/`cbi` instructions. A blink is a read-modify-write of PORTB.
- **Sleeps cooperatively.** `Sleep(domain: PlatformLoopDomain)` suspends
  only the loop, so the timer keeps polling the button while the LED waits.

## Porting it

Move the signals to other pins, or to another AVR board, by editing
`BlinkRig.stride`: change `overlays:` and the `pin:` labels. The program does
not change. For a board on another platform, also change the `use` line of
`PortableBlink.stride`.
