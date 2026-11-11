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
   - **`package Dsp`:** Digital signal processing, audio filters, filter design, synthesis generators, spectral tone detection, delay lines, dynamics, waveshaping, and musical units (`Dsp.stride`, `Dsp/`).
   - **`package Math`:** Standard mathematical constants (`Math::Pi`, `Math::TwoPi`, `Math::E`) and modular mathematical utilities (`Math/`).
   - **`package Control`:** Industrial & embedded control systems: PID controllers, Schmitt triggers, slew rate limiters, and motor control Clarke/Park transforms (`Control/`).
   - **`package Sensors`:** Signal conditioning & IMU fusion: debouncing, deadband scaling, linear ADC calibration, and complementary filters (`Sensors.stride`, `Sensors/`).
   - **`package Estimation`:** Real-time state estimation: 1D recursive Kalman filters and local peak detection (`Estimation.stride`, `Estimation/`).
   - **`package Window`:** Standard spectral & FIR window functions: Hann, Hamming, Blackman, and Flat-Top (`Window.stride`, `Window/`).
   - **`package LinAlg`:** Embedded linear algebra: 2D/3D vectors (dot, cross, norm, normalize), matrix transforms (2x2, 3x3), matrix multiplication, and unit quaternions (`LinAlg.stride`, `LinAlg/`).
   - **`package Test`:** Real-time stream assertions and boundary guards (`Test.stride`, `Test/`).
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
* **`Pan(position: 0.0)`**: Stereo panner with position in $[-1.0, 1.0]$, outputting stereo array `[left, right]`.
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

### 3.6 `Dsp::Spectral`
* **`Goertzel::Detector(frequency: 440.0 sampleRate: 44100.0)`**: Computes instantaneous power and tone presence at a target frequency using a 2nd-order IIR recursion ($O(N)$ efficiency vs $O(N \log N)$ FFT).

### 3.7 `Dsp::Delay`
* **`FractionalDelay::Line(delay: 100.0)`**: Circular ring-buffer fractional delay line with linear interpolation for variable-delay effects (chorus, flanger, physical modeling).

### 3.8 `Dsp::Dynamics`
* **`EnvelopeFollower::Follower(attack: 0.1 release: 0.01)`**: Peak envelope detector with decoupled attack and release rates.
* **`Compressor::Device(threshold: 0.5 ratio: 4.0 makeup: 1.0)`**: Feedforward dynamic range compressor.
* **`Limiter::Fast(threshold: 0.95)`**: Hard peak clamping ceiling limiter.

### 3.9 `Dsp::Nonlinear`
* **`Saturate::SoftClip()`**: Cubic polynomial soft clipper ($1.5(x - x^3 / 3)$) constrained to $[-1.0, 1.0]$.
* **`Saturate::Tanh()`**: Padé rational [3/2] approximation of hyperbolic tangent saturation with zero transcendentals.

### 3.10 Multi-Rate Filters (`Dsp::Filters`)
* **`Halfband::Fir7()`**: 7-tap symmetric halfband anti-aliasing FIR filter (50% zero taps for optimal decimation/interpolation).
* **`Cic::Decimator2()`**: 2nd-order multiplierless Cascaded Integrator-Comb decimation filter stage.

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

## 5. Package `Control` (`package Control`)

Imported via `import Control`:

```stride
import Control as Control
```

### 5.1 Modules
* **`Pid(kp: 1.0 ki: 0.0 kd: 0.0 min: -1.0 max: 1.0)`**: Proportional-Integral-Derivative controller with anti-windup clamping on the integrator and output saturation.
* **`Hysteresis(low: 0.2 high: 0.8)`**: Dual-threshold Schmitt trigger comparator preventing chatter on noisy analog thresholds.
* **`SlewRateLimiter(up: 0.01 down: 0.01)`**: Limits maximum rate of change ($dx/dt$) per sample step.
* **`Clarke()`**: Transforms 3-phase currents $[I_a, I_b, I_c]$ to stationary frame $[I_\alpha, I_\beta]$.
* **`Park(angle: Theta)`**: Transforms stationary $[I_\alpha, I_\beta]$ to rotating direct-quadrature $[I_d, I_q]$.
* **`InversePark(angle: Theta)`**: Converts rotating $[V_d, V_q]$ back to stationary $[V_\alpha, V_\beta]$.
* **`InverseClarke()`**: Converts stationary $[V_\alpha, V_\beta]$ back to 3-phase voltages $[V_a, V_b, V_c]$.

---

## 6. Package `Sensors` (`package Sensors`)

Imported via `import Sensors` or `import Sensors::Conditioning`:

```stride
import Sensors::Conditioning as Sensors
```

### 6.1 Modules
* **`Debounce(samples: 10)`**: Digital switch bounce suppression requiring $N$ consecutive stable samples before accepting state transitions.
* **`Deadband(threshold: 0.05)`**: Zeroes sensor drift around origin and rescales remaining range smoothly to $[-1.0, 1.0]$.
* **`LinearCalibration(rawMin: 0 rawMax: 1023 calMin: 0.0 calMax: 3.3)`**: Rescales raw ADC integer counts into physical engineering units.
* **`ComplementaryFilter(accelAngle: ... alpha: 0.98 dt: 0.01)`**: Low-overhead sensor fusion blending high-rate gyro integration with accelerometer tilt.

---

## 7. Package `Estimation` (`package Estimation`)

Imported via `import Estimation`:

```stride
import Estimation::Kalman as Kalman
import Estimation::PeakDetector as PeakDetector
```

### 7.1 Modules
* **`Kalman::Filter(q: 0.01 r: 0.1)`**: 1-dimensional recursive discrete Kalman filter providing optimal state estimation from noisy measurements.
* **`PeakDetector::Detector(threshold: 0.5)`**: Real-time local maximum peak detector with amplitude thresholding and sample history evaluation.

---

## 8. Package `Window` (`package Window`)

Imported via `import Window::Functions`:

```stride
import Window::Functions as Window
```

### 8.1 Modules
* **`Hann(size: 512)`**: $w[n] = 0.5 \cdot (1 - \cos(\frac{2\pi n}{N-1}))$
* **`Hamming(size: 512)`**: $w[n] = 0.54 - 0.46 \cos(\frac{2\pi n}{N-1})$
* **`Blackman(size: 512)`**: $w[n] = 0.42 - 0.5 \cos(\frac{2\pi n}{N-1}) + 0.08 \cos(\frac{4\pi n}{N-1})$
* **`FlatTop(size: 512)`**: Precision calibration window for accurate amplitude metering.

---

## 9. Package `LinAlg` (`package LinAlg`)

Imported via `import LinAlg`:

```stride
import LinAlg::Vectors as Vec
import LinAlg::Matrices as Mat
import LinAlg::Quaternion as Quat
```

### 9.1 Vectors (`LinAlg::Vectors`)
* **`Dot2D(with: ...)`**, **`Dot3D(with: ...)`**: Inner scalar product.
* **`Cross3D(with: ...)`**: Orthogonal 3D vector cross product.
* **`Norm2D()`**, **`Norm3D()`**: Euclidean vector magnitude.
* **`Normalize2D()`**, **`Normalize3D()`**: Unit vector normalization ($v / \|v\|$).

### 9.2 Matrices (`LinAlg::Matrices`)
* **`Transform2D(matrix: ...)`**: $2\times 2$ matrix-vector multiplication.
* **`Transform3D(matrix: ...)`**: $3\times 3$ matrix-vector multiplication.
* **`MatMul2x2(with: ...)`**: $2\times 2$ matrix multiplication ($C = A \cdot B$).
* **`Transpose2x2()`**: Matrix transposition.

### 9.3 Quaternions (`LinAlg::Quaternion`)
* **`Multiply(with: ...)`**: Hamilton product ($q_1 \cdot q_2$) for orientation concatenation.
* **`Conjugate()`**: Inverse orientation for unit quaternions ($[w, -x, -y, -z]$).
* **`Norm()`**: Quaternion Euclidean magnitude.
* **`RotateVector3D(rotation: ...)`**: Rotates a 3D vector by unit orientation quaternion.

---

## 10. Package `Test` (`package Test`)

Imported via `import Test::Assertions`:

```stride
import Test::Assertions as Assert
```

### 10.1 Modules
* **`AssertInRange(min: -1.0 max: 1.0)`**: Passthrough signal guard triggering a runtime error reaction if signal values breach specified limits.

---

## 11. Global Prelude Reference

### 11.1 Elementary & Scalar Math (`Math.stride`)
* `Abs()`, `Sign()`, `Inverse()`, `Floor()`, `Sin()`, `Cos()`, `Tan()`, `Exp()`
* `Pow()`: `[ Base, Exponent ] >> Pow() >> Result;`
* `Multiply()`, `Divide()`
* `Max()`, `Min()`
* `Clamp(min: ... max: ...)`, `Wrap(min: ... max: ...)`
* `Lerp(target: ... factor: ...)`
* `Map(inMin: ... inMax: ... outMin: ... outMax: ...)`

### 11.2 Relational Comparisons (`Comparisons.stride`)
`Greater()`, `GreaterOrEqual()`, `Less()`, `LessOrEqual()`, `Equal()`, `NotEqual()`.

### 11.3 Logic & Multiplexing (`Logic.stride`)
`Select()`, `And()`, `Or()`, `Not()`, `Xor()`, `Nand()`, `Nor()`.

### 11.4 Stream Adapters (`Control.stride`, `Counter.stride`)
`OnChange()`, `RisingEdge()`, `FallingEdge()`, `Latch()`, `Smooth()`, `Counter()`.
