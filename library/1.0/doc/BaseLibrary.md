# Stride Base Standard Library Specification & Reference Guide

This document defines the architecture, conventions, and module catalog of the Stride Base Standard Library (`strideroot/library/1.0/`).

---

## 1. Architectural Principles & Scoping Taxonomy

The Stride standard library and runtime ecosystem is structured across four architectural layers:

1. **Layer 1: Language Kernel & Meta-Model:** Core meta-types (`_Signal`, `_Constant`, `_Switch`, `_Trigger`), schema definition engine (`Schema.stride`), and execution constructs (`Callable.stride`, `Domain.stride`, `StateMachine.stride`).
2. **Layer 2: Global Prelude (Auto-Imported):** Foundational primitives loaded automatically into the root namespace of every file (`strideroot/library/1.0/*.stride`):
   - Universal scalars: `Abs()`, `Sign()`, `Inverse()`, `Floor()`, `Clamp()`, `Wrap()`, `Lerp()`, `Map()`
   - Relational comparisons: `Equal()`, `NotEqual()`, `Greater()`, `Less()`, `GreaterOrEqual()`, `LessOrEqual()`
   - Boolean logic: `Select()`, `And()`, `Or()`, `Not()`, `Xor()`, `Nand()`, `Nor()`
   - Stream adapters: `OnChange()`, `RisingEdge()`, `FallingEdge()`, `Latch()`, `Smooth()`, `Counter()`
   - Collection reductions: `Sum()`, `Average()`
   - Diagnostics: `DebugPrint()`, `Probe()`, `Error()`
3. **Layer 3: Standard Library Packages (Explicitly Imported):** Cohesive, domain-specific modules organized under namespaced packages:
   - **`package IO`:** Input/output, serial communications, OSC networking, MIDI events, and packet framing (`IO.stride`, `IO/`).
   - **`package Dsp`:** Digital signal processing, synthesis generators, audio filters, filter design, routing, and musical unit conversions (`Dsp.stride`, `Dsp/`).
   - **`package Math`:** Standard mathematical constants (`Math::Pi`, `Math::TwoPi`, `Math::E`) and modular mathematical utilities (`Math/`).
4. **Layer 4: Platform & Framework Layer:** Concrete target drivers and hardware intrinsics (`__SerialOut`, `__SerialIn`, `__OscOut`, `__OscIn`, `__Sin`, `__Cos`).

---

## 2. Package `IO` (`package IO`)

Imported via `import IO` or specific submodules:

```stride
import IO::Serial as Serial
import IO::Osc as Osc
import IO::Console as Console
import IO::Midi as Midi
import IO::Framing as Framing
```

### 2.1 `IO::Console`
* **`Print()`**: Streams values to standard console output.
* **`Println()`**: Streams values to standard console output followed by a newline.
* **`Log(tag: ...)`**: Outputs structured telemetry messages with timestamps and categorization tags.

### 2.2 `IO::Serial`
* **`Print()`**: Writes formatted signal values to the active platform serial port.
* **`Out(channel: 0)`**: Routes signals to platform serial output streams (`__SerialOut`).
* **`In(channel: 0)`**: Reads signals from platform serial input streams (`__SerialIn`).

### 2.3 `IO::Osc`
* **`Out(address: "/stride" host: "127.0.0.1" port: 9010)`**: Transmits OSC packets over UDP.
* **`In(address: "/stride" host: "127.0.0.1" port: 9011)`**: Receives OSC packets from UDP ports.

### 2.4 `IO::Midi`
* **`NoteIn(channel: 0)`**: Decodes incoming MIDI note on/off events into `[frequency, velocity, gate]`.
* **`NoteOut(pitch: 60 velocity: 100 channel: 1)`**: Transmits MIDI note on/off events triggered by an incoming trigger.
* **`CcIn(controller: 1 channel: 0)`**: Decodes Continuous Controller (CC) messages into normalized $[0.0, 1.0]$ signals.
* **`CcOut(controller: 1 channel: 1)`**: Transmits Continuous Controller (CC) messages.

### 2.5 `IO::Framing`
Lightweight framing delimiters:
* **SLIP (RFC 1055):** `SlipEnd` (192 / 0xC0), `SlipEsc` (219 / 0xDB), `SlipEscEnd` (220 / 0xDC), `SlipEscEsc` (221 / 0xDD).
* **COBS:** `CobsDelimiter` (0).

---

## 3. Package `Dsp` (`package Dsp`)

Imported via `import Dsp` or specific submodules:

```stride
import Dsp::Generators as Generators
import Dsp::Filters as Filters
import Dsp::FilterDesign as FilterDesign
import Dsp::Routing as Routing
import Dsp::Units as Units
```

### 3.1 `Dsp::Routing`
* **`Level(gain: 1.0 offset: 0.0)`**: Scales an audio stream and applies an offset:
  $$\text{output} = \text{input} \times \text{gain} + \text{offset}$$
* **`Pan(position: 0.0)`**: Stereo panner with position in $[-1.0, 1.0]$, outputting stereo bundle `[left, right]`.
* **`Crossfade(mix: 0.5 with: InputB)`**: Linear interpolation blending two streams.

### 3.2 `Dsp::Generators`
* **`Oscillator(frequency: 440.0 amplitude: 1.0)`**: Continuous sine wave oscillator.
* **`Impulse(frequency: 1.0 amplitude: 1.0)`**: Periodic single-sample impulse train.
* **`TableOscillator(frequency: 440.0 amplitude: 1.0)`**: Wavetable lookup oscillator reading pre-computed tables.
* **`BandLimitedSaw(frequency: 220.0)`**: Differentiated parabolic wave (DPW) band-limited sawtooth oscillator.
* **`AD(attack: 0.2 decay: 3.0)`**: Two-stage Attack-Decay linear envelope generator triggered by an input trigger.
* **`Sequencer(values: [ ... ])`**: Step sequencer advancing on input triggers and wrapping around port size.

### 3.3 `Dsp::Filters`
* **`BiQuad(centerFrequency: 1000.0 resonance: 50.0)`**: 2nd-order IIR biquad filter.
* **`LowPass(frequency: 1000.0 qFactor: 0.7071 gain: 1.0)`**: Standard lowpass filter.
* **`BandPass(centerFrequency: 1000.0 qFactor: 1.0 gain: 1.0)`**: Bandpass resonant filter.
* **`Rectify()`**: Full-wave audio rectifier.

### 3.4 `Dsp::FilterDesign`
Robert Bristow-Johnson (RBJ) Audio EQ Cookbook normalized biquad coefficient calculations:
* **`LowPassCoefficients(frequency: 1000.0 q: 0.7071 sampleRate: 44100.0)`**: Generates `b[3]` numerator and `a[2]` denominator coefficients.
* **`HighPassCoefficients(frequency: 1000.0 q: 0.7071 sampleRate: 44100.0)`**: Generates highpass biquad coefficients.

### 3.5 `Dsp::Units`
* **Concert Pitch Constants:** Note frequencies in Hz from `C0` through `B6`, including `C4` (261.6256 Hz), `A4` (440.0000 Hz), `C5` (523.2511 Hz).
* **`MidiToFreq()`**: Converts MIDI note number to frequency in Hz:
  $$f = 440.0 \times 2^{(note - 69) / 12}$$
* **`DbToLinear()`**: Converts decibels to linear amplitude:
  $$A = 10^{dB / 20}$$

---

## 4. Package `Math` (`package Math`)

Imported via `import Math` or specific submodules:

```stride
import Math
# or
import Math::Constants as MathConstants
```

### 4.1 `Math::Constants`
* **`Pi`**: Ratio of a circle's circumference to its diameter:
  $$\pi \approx 3.14159265358979323846$$
* **`TwoPi`**: Tau constant ($2\pi$):
  $$2\pi \approx 6.28318530717958647693$$
* **`E`**: Euler's number (base of natural logarithm):
  $$e \approx 2.71828182845904523536$$

---

## 5. Global Prelude Reference

### 5.1 Elementary & Scalar Math (`Math.stride`)
* `Abs()`, `Sign()`, `Inverse()`, `Floor()`, `Sin()`, `Cos()`, `Tan()`, `Exp()`
* `Pow()`: `[ Base, Exponent ] >> Pow() >> Result;`
* `Multiply()`, `Divide()`
* `Max()`, `Min()`
* `Clamp(min: ... max: ...)`, `Wrap(min: ... max: ...)`
* `Lerp(target: ... factor: ...)`
* `Map(inMin: ... inMax: ... outMin: ... outMax: ...)`

### 5.2 Relational Comparisons (`Comparisons.stride`)
`Greater()`, `GreaterOrEqual()`, `Less()`, `LessOrEqual()`, `Equal()`, `NotEqual()`.

### 5.3 Logic & Multiplexing (`Logic.stride`)
`Select()`, `And()`, `Or()`, `Not()`, `Xor()`, `Nand()`, `Nor()`.

### 5.4 Stream Adapters (`Control.stride`, `Counter.stride`)
`OnChange()`, `RisingEdge()`, `FallingEdge()`, `Latch()`, `Smooth()`, `Counter()`.
