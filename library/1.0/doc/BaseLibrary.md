# Stride Base Standard Library Specification & Reference Guide

This document defines the architecture, conventions, and module catalog of the Stride Base Standard Library (`strideroot/library/1.0/`).

---

## 1. Architectural Principles & Scoping Taxonomy

The Stride standard library and runtime ecosystem is structured across four architectural layers:

1. **Layer 1: Language Kernel & Meta-Model:** Core meta-types (`_Signal`, `_Constant`, `_Switch`, `_Trigger`), schema definition engine (`Schema.stride`), and execution constructs (`Callable.stride`, `Domain.stride`, `StateMachine.stride`). The platform schema (`_PlatformType`, `_DeviceType`, `_PolicyBinding`, ...) is in `Framework.stride`.
2. **Layer 2: Global Prelude (Auto-Imported):** Foundational primitives loaded automatically into the root namespace of every file (`strideroot/library/1.0/*.stride`):
   - Universal scalars (`MathPrelude.stride`): `Abs()`, `Sign()`, `Negate()`, `Floor()`, `Clamp()`, `Wrap()`, `Lerp()`, `Map()`, `Sin()`, `Cos()`, `Pow()`, ...
   - Relational comparisons: `Equal()`, `NotEqual()`, `Greater()`, `Less()`, `GreaterOrEqual()`, `LessOrEqual()`
   - Boolean logic: `Select()`, `And()`, `Or()`, `Not()`, `Xor()`, `Nand()`, `Nor()`
   - Stream adapters (`ControlPrelude.stride`, `Counter.stride`): `OnChange()`, `RisingEdge()`, `FallingEdge()`, `Latch()`, `Smooth()`, `Counter()`
   - Collection reductions: `Sum()`, `Average()`
   - Diagnostics: `DebugPrint()`, `Probe()`, `Error()`
3. **Layer 3: Standard Library Packages (Explicitly Imported):** Cohesive, domain-specific modules organized under namespaced packages:
   - **`package IO`:** Input/output, serial communications, OSC networking, MIDI events, and packet framing (`IO.stride`, `IO/`).
   - **`package Dsp`:** Digital signal processing, audio filters, filter design, synthesis generators, spectral tone detection, delay lines, dynamics, waveshaping, and musical units (`Dsp.stride`, `Dsp/`).
   - **`package Math`:** Standard mathematical constants (`Math::Pi`, `Math::TwoPi`, `Math::E`) (`Math.stride`, `Math/`).
   - **`package Control`:** Industrial & embedded control systems: PID controllers, Schmitt triggers, slew rate limiters, and motor control Clarke/Park transforms (`Control.stride`, `Control/`).
   - **`package Sensors`:** Signal conditioning & IMU fusion: debouncing, deadband scaling, linear ADC calibration, and complementary filters (`Sensors.stride`, `Sensors/`).
   - **`package Estimation`:** Real-time state estimation: 1D recursive Kalman filters and local peak detection (`Estimation.stride`, `Estimation/`).
   - **`package Window`:** Standard spectral & FIR window functions: Hann, Hamming, Blackman, and Flat-Top (`Window.stride`, `Window/`).
   - **`package LinAlg`:** Linear algebra of any dimension: vectors (dot, norm, normalize, scale, cross), matrices (matrix-vector product, matrix product, transpose) and unit quaternions (`LinAlg.stride`, `LinAlg/`).
   - **`package Test`:** Real-time stream assertions and boundary guards (`Test.stride`, `Test/`).
4. **Layer 4: Platform & Framework Layer:** Concrete target drivers and hardware intrinsics (`__SerialOut`, `__SerialIn`, `__OscOut`, `__OscIn`, `__Sin`, `__Cos`, `__Select`, ...). Library modules call intrinsics (names starting with `__`) only to implement the primitives of the prelude and of the I/O package.

### 1.1 File naming

A package `Name` is a barrel file `Name.stride` next to a directory `Name/`. Files of the global prelude must not share a name with a package, so the prelude files that would collide carry the suffix `Prelude`:

| Prelude file | Package it must not be confused with |
|---|---|
| `MathPrelude.stride` (scalar functions) | `package Math` (`Math.stride`, `Math/Constants.stride`) |
| `ControlPrelude.stride` (edges, `Latch`, `Smooth`) | `package Control` (`Control.stride`, `Control/`) |

`import Math` and `import Control` therefore always load the package, never the prelude.

---

## 2. Conventions of the Library

All packages follow the same four conventions. A module that departs from one of them says so in its `meta`.

### 2.1 Time and frequency are converted against the rate

The library never takes a sample rate as a property. A module that works with time, frequency or a rate of change takes it in the units a user thinks in (seconds, Hz, units per second) and converts it with the **rate of its output**, `OutputPort.rate` (the rate of the signal the module writes to, which the compiler resolves from the stream connections and the domain).

| Quantity | Unit | Converted with | Modules |
|---|---|---|---|
| Frequency | Hz | `2π · f / rate` | `Oscillator`, `Impulse`, `TableOscillator`, `BandLimitedSaw`, `FilterDesign::Coefficients`, `BiQuad` and the filters, `Goertzel::Detector` |
| Duration, time constant | seconds | `time · rate` (samples), `exp(-1 / (time · rate))` (one-pole coefficient) | `Smooth`, `AD`, `EnvelopeFollower::Follower`, `Compressor::Device`, `FractionalDelay::Line`, `Debounce`, `ComplementaryFilter` |
| Rate of change | units per second | `value / rate` | `SlewRateLimiter` (`up`, `down`), `Pid` (`ki`; `kd` is multiplied by `rate`), `Kalman::Filter` (`q`) |

Consequences:
- The same program behaves the same at 8 kHz and at 48 kHz: `Smooth(time: 0.1)` has a time constant of 0.1 s at any rate, a `LowPass(frequency: 1000.0)` cuts at 1000 Hz.
- A module that needs a rate raises an `Error` constraint when its output has no rate (rate `none` or `<= 0`, as in one-shot domains).
- Where a quantity is genuinely "per tick" (`Counter`'s `increment`), it stays per tick and is documented as such.

### 2.2 Data types propagate from the ports

Signals inside a module take their type from a port, never from a property or a hard-coded `Real`:

- **From the output:** `type: OutputPort.type` for the input, the properties, the internal state and the output. This is the rule for almost every module.
- **From the input,** when the output is not a number: modules whose output is a `switch` (`Hysteresis`, `PeakDetector`, comparisons) type their numeric entities with `InputPort.type`.
- **Fixed types** are used only where a different type is part of the meaning: array indices and counters are `Int`; a filter `mode` is an `Int` selector; `Debounce`'s time is a `Real` because its main ports carry switches.

Properties never decide the type of the module. A property that used to carry the type (for example `Level`'s `gain`) now takes it from the main output port, so the type of a module is chosen where the module is connected.

Modules that use fractional coefficients (filters, `Smooth`, `Kalman`, `Pid`, ...) are only meaningful for floating point types; their `meta` says so.

### 2.3 Switches for boolean results

Whenever a result is a yes/no condition the module uses a `switch`, not a 0.0/1.0 signal: `Hysteresis`, `Debounce` and `PeakDetector` take and/or produce switches. The comparisons, `Select`'s condition and the logic modules already did.

### 2.4 Dimensions come from port sizes

Modules that work on arrays never come in one version per size. The dimension is the size of the connected port (`InputPort.size`, `OutputPort.size`), as in `Sum`:

- `Dot`, `Norm`, `Normalize` and `Scale` work for 2, 3, 4 or any number of components.
- `Transform` takes `columns` from the size of the input vector and `rows` from the size of the output.
- Only operations that are defined for one dimension keep it: the quaternion modules (4 components). The cross product has no fixed-size version: `Wedge` is its generalisation to any dimension.

A matrix is a flat row-major array, so its size alone does not say how it is split into rows and columns. `MatMul` and `Transpose` therefore take the shape as properties (`inner`, `columns`, `rows`).

Loops (`loop`) keep their entities in the scope of the domain, so the entities of a loop are prefixed with the loop's name (`Dot_VectorA`, `Transform_Matrix`), as in `Sum_Input`. Modules do not need the prefix.

---

## 3. Package `IO` (`package IO`)

Imported via `import IO` or specific submodules:

```stride
import IO::Serial as Serial
import IO::Osc as Osc
import IO::Console as Console
import IO::Midi as Midi
import IO::Framing as Framing
```

### 3.1 `IO::Console`
* **`Print()`**: Streams values to standard console output.
* **`Println()`**: Streams values to standard console output followed by a newline.
* **`Log(tag: ...)`**: Outputs structured telemetry messages with timestamps and categorization tags.

### 3.2 `IO::Serial`
* **`Print()`**: Writes formatted signal values to the active platform serial port.
* **`Out(channel: 0)`**: Routes signals to platform serial output streams (`__SerialOut`).
* **`In(channel: 0)`**: Reads signals from platform serial input streams (`__SerialIn`).

### 3.3 `IO::Osc`
* **`Out(address: "/stride" host: "127.0.0.1" port: 9010)`**: Transmits OSC packets over UDP.
* **`In(address: "/stride" host: "127.0.0.1" port: 9011)`**: Receives OSC packets from UDP ports.

### 3.4 `IO::Midi`
* **`NoteIn(channel: 0)`**: Decodes incoming MIDI note on/off events into `[frequency, velocity, gate]`.
* **`NoteOut(pitch: 60 velocity: 100 channel: 1)`**: Transmits MIDI note on/off events triggered by an incoming trigger.
* **`CcIn(controller: 1 channel: 0)`**: Decodes Continuous Controller (CC) messages into normalized $[0.0, 1.0]$ signals.
* **`CcOut(controller: 1 channel: 1)`**: Transmits Continuous Controller (CC) messages.

### 3.5 `IO::Framing`
Lightweight framing delimiters:
* **SLIP (RFC 1055):** `SlipEnd` (192 / 0xC0), `SlipEsc` (219 / 0xDB), `SlipEscEnd` (220 / 0xDC), `SlipEscEsc` (221 / 0xDD).
* **COBS:** `CobsDelimiter` (0).

---

## 4. Package `Dsp` (`package Dsp`)

Imported via `import Dsp` or specific submodules:

```stride
import Dsp::Generators as Generators
import Dsp::Filters as Filters
import Dsp::FilterDesign as FilterDesign
import Dsp::Routing as Routing
import Dsp::Units as Units
```

All frequencies are in Hz and all times in seconds; they are converted against the rate of the output (section 2.1). The audio type is the type of the output (section 2.2) and must be a floating point type.

### 4.1 `Dsp::Routing`
`Level`, `Pan` and `Crossfade` are defined once, here (the `Dsp` barrel does not redefine them).
* **`Level(gain: 1.0 offset: 0.0)`**: Scales an audio stream and applies an offset:
  $$\text{output} = \text{input} \times \text{gain} + \text{offset}$$
* **`Pan(position: 0.0)`**: Linear stereo panner with position in $[-1.0, 1.0]$, outputting stereo array `[left, right]`.
* **`Crossfade(mix: 0.5 with: InputB)`**: Linear interpolation blending two streams.

### 4.2 `Dsp::Generators`
* **`Oscillator(frequency: 440.0 amplitude: 1.0)`**: Continuous sine wave oscillator. `frequency` in Hz.
* **`Impulse(frequency: 1.0 amplitude: 1.0)`**: Periodic single-sample impulse train. `frequency` in Hz.
* **`TableOscillator(frequency: 440.0 amplitude: 1.0)`**: Wavetable lookup oscillator reading pre-computed tables.
* **`BandLimitedSaw(frequency: 220.0)`**: Differentiated parabolic wave (DPW) band-limited sawtooth oscillator: the squared naive saw is differentiated and scaled by `rate / (4 · frequency)`.
* **`AD(attack: 0.2 decay: 3.0)`**: Two-stage Attack-Decay linear envelope generator triggered by an input trigger. `attack` and `decay` in seconds.
* **`Sequencer(values: [ ... ])`**: Step sequencer advancing on input triggers and wrapping around port size.

### 4.3 `Dsp::Filters`

Second-order (biquad) filters designed with the RBJ Audio EQ Cookbook. The response is a **property choice** of `BiQuad` (`mode`); `LowPass`, `HighPass`, `BandPass`, `Notch`, `AllPass` and `PeakingEq` are shortcuts for the common modes. They all share the same properties, in the same units:

| Property | Unit | Meaning |
|---|---|---|
| `frequency` | Hz | Cutoff (`LowPass`, `HighPass`), centre (`BandPass`, `Notch`, `PeakingEq`) or 180° phase frequency (`AllPass`). Converted against the rate of the output. Default 1000.0 |
| `q` | — | Quality factor. Default 0.7071 for `LowPass`, `HighPass`, `AllPass`, 1.0 for the others. Must be greater than 0 |
| `gain` | dB | `PeakingEq` and `BiQuad(mode: ModePeak)` only: boost (positive) or cut (negative). Default 6.0 for `PeakingEq`, 0.0 for `BiQuad` |

| Module | Response |
|---|---|
| `LowPass(frequency q)` | Passes below `frequency`, −12 dB/octave above. DC gain 1 |
| `HighPass(frequency q)` | Passes above `frequency`, −12 dB/octave below. DC gain 0 |
| `BandPass(frequency q)` | Constant 0 dB peak gain at `frequency`; bandwidth ≈ 1.44 / q octaves |
| `Notch(frequency q)` | Rejects a narrow band around `frequency`. DC gain 1 |
| `AllPass(frequency q)` | Unity magnitude; phase shift of 180° at `frequency` |
| `PeakingEq(frequency q gain)` | Boosts or cuts a band around `frequency` by `gain` dB |
| `BiQuad(mode frequency q gain)` | Any of the above, chosen with `mode` |
| `Section(coefficients)` | The filter itself: a second-order section (direct form II transposed) with explicit normalised coefficients `[b0, b1, b2, a1, a2]` |
| `Rectify()` | Full-wave rectifier |

```stride
import Dsp::Filters as Filters
import Dsp::FilterDesign as FilterDesign

AudioIn >> Filters::LowPass(frequency: 800.0) >> Smoothed;
AudioIn >> Filters::BiQuad(mode: FilterDesign::ModeNotch frequency: 50.0 q: 10.0) >> NoHum;
```

`BiQuad` redesigns its coefficients on every tick (`FilterDesign::Coefficients` followed by `Section`), so `frequency`, `q` and `gain` can be modulated.

*Changes from the previous version:* `LowPass` and `BandPass` were placeholders that only applied a gain, and `BiQuad` neither updated its state nor used `B[0]`. They now implement the filters above. The properties `qFactor`, `resonance`, `centerFrequency` and the passband `gain` of `LowPass` were replaced by `q` and `frequency` (use `Level` for gain), and the hard-coded 44100 Hz sample rate is gone.

### 4.4 `Dsp::FilterDesign`
RBJ cookbook coefficients:
* **Modes:** the integer constants `ModeLowPass` (0), `ModeHighPass` (1), `ModeBandPass` (2), `ModeNotch` (3), `ModeAllPass` (4), `ModePeak` (5).
* **`Coefficients(mode: ModeLowPass q: 0.7071 gain: 0.0)`**: `Frequency >> Coefficients(...) >> Coeffs[5]`. Returns the coefficients normalised by `a0` as `[b0, b1, b2, a1, a2]` for $y[n] = b_0 x[n] + b_1 x[n-1] + b_2 x[n-2] - a_1 y[n-1] - a_2 y[n-2]$. The sample rate is the rate of the output.

(`LowPassCoefficients` and `HighPassCoefficients` were replaced by `Coefficients`.)

### 4.5 `Dsp::Units`
* **Concert Pitch Constants:** Note frequencies in Hz from `C0` through `B6`, including `C4` (261.6256 Hz), `A4` (440.0000 Hz), `C5` (523.2511 Hz).
* **`MidiToFreq()`**: Converts MIDI note number to frequency in Hz:
  $$f = 440.0 \times 2^{(note - 69) / 12}$$
* **`DbToLinear()`**: Converts decibels to linear amplitude:
  $$A = 10^{dB / 20}$$

### 4.6 `Dsp::Spectral`
* **`Goertzel::Detector(frequency: 440.0)`**: Computes instantaneous power at a target frequency in Hz using a 2nd-order IIR recursion ($O(N)$ efficiency vs $O(N \log N)$ FFT). The sample rate is the rate of the output (it was the `sampleRate` property, default 44100).

### 4.7 `Dsp::Delay`
* **`FractionalDelay::Line(delay: 0.002)`**: Circular ring-buffer fractional delay line with linear interpolation for variable-delay effects (chorus, flanger, physical modeling). `delay` is in **seconds** (it was in samples), converted against the rate of the output; the buffer holds 4096 samples, so the maximum delay is `4096 / rate` seconds.

### 4.8 `Dsp::Dynamics`
* **`EnvelopeFollower::Follower(attack: 0.01 release: 0.1)`**: Peak envelope detector with decoupled attack and release. Both are time constants in **seconds** (they were per-sample coefficients); the one-pole coefficient is $1 - e^{-1 / (t \cdot rate)}$.
* **`Compressor::Device(threshold: 0.5 ratio: 4.0 makeup: 1.0 attack: 0.01 release: 0.1)`**: Feedforward dynamic range compressor. `attack` and `release` (seconds) are those of its level detector.
* **`Limiter::Fast(threshold: 0.95)`**: Hard peak clamping ceiling limiter.

### 4.9 `Dsp::Nonlinear`
* **`Saturate::SoftClip()`**: Cubic polynomial soft clipper ($1.5(x - x^3 / 3)$) constrained to $[-1.0, 1.0]$.
* **`Saturate::Tanh()`**: Padé rational [3/2] approximation of hyperbolic tangent saturation with zero transcendentals: $x (27 + x^2) / (27 + 9 x^2)$ on $[-3, 3]$.

### 4.10 Multi-Rate Filters (`Dsp::Filters`)
* **`Halfband::Fir7()`**: 7-tap symmetric halfband anti-aliasing FIR filter (50% zero taps for optimal decimation/interpolation).
* **`Cic::Decimator2()`**: 2nd-order multiplierless Cascaded Integrator-Comb decimation filter stage.

---

## 5. Package `Math` (`package Math`)

Imported via `import Math` or specific submodules:

```stride
import Math
# or
import Math::Constants as MathConstants
```

The package contains only constants. The scalar functions (`Abs`, `Sin`, `Pow`, ...) are in the prelude (section 12.1).

### 5.1 `Math::Constants`
* **`Pi`**: Ratio of a circle's circumference to its diameter:
  $$\pi \approx 3.14159265358979323846$$
* **`TwoPi`**: Tau constant ($2\pi$):
  $$2\pi \approx 6.28318530717958647693$$
* **`E`**: Euler's number (base of natural logarithm):
  $$e \approx 2.71828182845904523536$$

---

## 6. Package `Control` (`package Control`)

Imported via `import Control`:

```stride
import Control as Control
```

The package is `Control`; the stream adapters `OnChange`, `RisingEdge`, `FallingEdge`, `Latch` and `Smooth` are prelude functions (section 12.4).

### 6.1 Modules
* **`Pid(kp: 1.0 ki: 0.0 kd: 0.0 min: -1.0 max: 1.0)`**: Proportional-Integral-Derivative controller with anti-windup clamping on the integrator and output saturation. `ki` is per second ($I \mathrel{+}= e \cdot k_i / rate$) and `kd` is a time in seconds ($D = \Delta e \cdot k_d \cdot rate$), so the tuning does not depend on the sample rate.
* **`Hysteresis(low: 0.2 high: 0.8)`**: Dual-threshold Schmitt trigger comparator preventing chatter on noisy analog thresholds. The output is a **switch**.
* **`SlewRateLimiter(up: 1.0 down: 1.0)`**: Limits the rate of change ($dx/dt$). `up` and `down` are in signal units per **second**, converted against the rate of the output.
* **`Clarke()`**: Transforms 3-phase currents $[I_a, I_b, I_c]$ to stationary frame $[I_\alpha, I_\beta]$.
* **`Park(angle: Theta)`**: Transforms stationary $[I_\alpha, I_\beta]$ to rotating direct-quadrature $[I_d, I_q]$.
* **`InversePark(angle: Theta)`**: Converts rotating $[V_d, V_q]$ back to stationary $[V_\alpha, V_\beta]$.
* **`InverseClarke()`**: Converts stationary $[V_\alpha, V_\beta]$ back to 3-phase voltages $[V_a, V_b, V_c]$.

---

## 7. Package `Sensors` (`package Sensors`)

Imported via `import Sensors` or `import Sensors::Conditioning`:

```stride
import Sensors::Conditioning as Sensors
```

### 7.1 Modules
* **`Debounce(time: 0.01)`**: Digital switch bounce suppression. The input and output are **switches**; the output follows the input once it has been different for `time` seconds (it was `samples`, a number of ticks), converted against the rate of the output.
* **`Deadband(threshold: 0.05)`**: Zeroes sensor drift around origin and rescales remaining range smoothly to $[-1.0, 1.0]$.
* **`LinearCalibration(rawMin: 0 rawMax: 1023 calMin: 0.0 calMax: 3.3)`**: Rescales raw ADC integer counts into physical engineering units.
* **`ComplementaryFilter(accelAngle: ... timeConstant: 0.5)`**: Low-overhead sensor fusion blending high-rate gyro integration with accelerometer tilt. The gyro rate is in angle units per second. The sample interval is $1 / rate$ and the blend factor $\alpha = \tau / (\tau + dt)$, so `timeConstant` $\tau$ (seconds) replaces the former `alpha` and `dt` properties.

---

## 8. Package `Estimation` (`package Estimation`)

Imported via `import Estimation`:

```stride
import Estimation::Kalman as Kalman
import Estimation::PeakDetector as PeakDetector
```

### 8.1 Modules
* **`Kalman::Filter(q: 0.01 r: 0.1)`**: 1-dimensional recursive discrete Kalman filter providing optimal state estimation from noisy measurements. `q` is the process noise variance gained **per second** (the covariance grows by $q / rate$ per tick); `r` is the measurement noise variance of each sample.
* **`PeakDetector::Detector(threshold: 0.5)`**: Real-time local maximum peak detector with amplitude thresholding and sample history evaluation. The output is a **switch**, on for one tick (one tick after the peak, which needs the following sample to be confirmed).

---

## 9. Package `Window` (`package Window`)

Imported via `import Window::Functions`:

```stride
import Window::Functions as Window
```

### 9.1 Modules
* **`Hann(size: 512)`**: $w[n] = 0.5 \cdot (1 - \cos(\frac{2\pi n}{N-1}))$
* **`Hamming(size: 512)`**: $w[n] = 0.54 - 0.46 \cos(\frac{2\pi n}{N-1})$
* **`Blackman(size: 512)`**: $w[n] = 0.42 - 0.5 \cos(\frac{2\pi n}{N-1}) + 0.08 \cos(\frac{4\pi n}{N-1})$
* **`FlatTop(size: 512)`**: Precision calibration window for accurate amplitude metering.

---

## 10. Package `LinAlg` (`package LinAlg`)

Imported via `import LinAlg`:

```stride
import LinAlg::Vectors as Vec
import LinAlg::Matrices as Mat
import LinAlg::Quaternion as Quat
```

Dimensions come from port sizes (section 2.4): there is one `Dot`, one `Norm`, one `Transform`, for every size. Matrices are flat, row-major arrays.

### 10.1 Vectors (`LinAlg::Vectors`)
* **`Dot(with: ...)`**: Inner scalar product of two vectors of the same size.
* **`Scale(factor: ...)`**: Multiplies every component by a scalar.
* **`Norm()`**: Euclidean vector magnitude. Also the norm of a quaternion (4 components).
* **`Normalize()`**: Unit vector normalization ($v / \|v\|$). The zero vector stays zero.
* **`Wedge(with: ...)`**: Wedge (exterior) product of two vectors of $n$ components, the generalisation of the cross product. The output has one component per pair of axes $i < j$, in lexicographic order, so its size must be $n(n-1)/2$ (an `Error` constraint checks it): $w_{ij} = a_i b_j - a_j b_i$.
  * $n = 2$: one component, the 2D cross product $a_0 b_1 - a_1 b_0$.
  * $n = 3$: three components $[w_{01}, w_{02}, w_{12}]$. The 3D cross product is $[w_{12}, -w_{02}, w_{01}]$.
  * $n = 4$: six components.

*Replaces:* `Dot2D`, `Dot3D`, `Norm2D`, `Norm3D`, `Normalize2D`, `Normalize3D` (→ `Dot`, `Norm`, `Normalize`) and `Cross3D` (→ `Cross`).

### 10.2 Matrices (`LinAlg::Matrices`)
* **`Transform(matrix: ...)`**: Matrix-vector product $y = M v$. $M$ has `size(output)` rows and `size(input)` columns, i.e. `size(output) · size(input)` elements.
* **`MatMul(with: ... inner: k columns: n)`**: Matrix product $C = A B$ with $A$ of `size(output) / n` rows and $k$ columns and $B$ of $k$ rows and $n$ columns.
* **`Transpose(rows: m columns: n)`**: Transposes an $m \times n$ matrix.

*Replaces:* `Transform2D`, `Transform3D`, `MatMul2x2` and `Transpose2x2`.

```stride
# 2 x 3 matrix times a 3-vector: Out has 2 components
Vec3 >> Mat::Transform(matrix: M2x3) >> Out2;

# (2 x 3) * (3 x 2) = (2 x 2)
A >> Mat::MatMul(with: B inner: 3 columns: 2) >> C;
```

### 10.3 Quaternions (`LinAlg::Quaternion`)
A quaternion has exactly four components `[w, x, y, z]` by definition, so these modules have fixed sizes.
* **`Multiply(with: ...)`**: Hamilton product ($q_1 \cdot q_2$) for orientation concatenation.
* **`Conjugate()`**: Inverse orientation for unit quaternions ($[w, -x, -y, -z]$).
* **`Rotate(rotation: ...)`**: Rotates a 3-component vector by a unit orientation quaternion (was `RotateVector3D`; rotation in space is three dimensional).

(`Quaternion::Norm` was removed: it was `Vectors::Norm` of 4 components.)

---

## 11. Package `Test` (`package Test`)

Imported via `import Test::Assertions`:

```stride
import Test::Assertions as Assert
```

### 11.1 Modules
* **`AssertInRange(min: -1.0 max: 1.0)`**: Passthrough signal guard triggering a runtime error reaction if signal values breach specified limits.

---

## 12. Global Prelude Reference

### 12.1 Elementary & Scalar Math (`MathPrelude.stride`)
* `Abs()`, `Sign()`, `Negate()`, `Floor()`, `Sin()`, `Cos()`, `Tan()`, `Exp()`
* `Pow()`: `[ Base, Exponent ] >> Pow() >> Result;`
* `Multiply()`, `Divide()`
* `Max()`, `Min()`
* `Clamp(min: ... max: ...)`, `Wrap(min: ... max: ...)`
* `Lerp(target: ... factor: ...)`
* `Map(inMin: ... inMax: ... outMin: ... outMax: ...)`

`Negate()` was named `Inverse()`, which suggests the multiplicative inverse; use `1.0 / x` for that.

### 12.2 Relational Comparisons (`Comparisons.stride`)
`Greater()`, `GreaterOrEqual()`, `Less()`, `LessOrEqual()`, `Equal()`, `NotEqual()`; reductions `AllOn()`, `AllOff()`.

### 12.3 Logic & Multiplexing (`Logic.stride`)
`Select()`, `And()`, `Or()`, `Not()`, `Xor()`, `Nand()`, `Nor()`.

### 12.4 Stream Adapters (`ControlPrelude.stride`, `Counter.stride`)
`OnChange()`, `RisingEdge()`, `FallingEdge()`, `Latch(trigger: ...)`, `Smooth(time: ...)`, `Counter()`.

* **`Smooth(time: 0.01)`**: Exponential smoothing (one-pole low-pass). `time` is the time constant in **seconds** (it was the per-sample `coefficient`; the coefficient is now $e^{-1 / (time \cdot rate)}$).

### 12.5 Collections (`BasicOperations.stride`)
`Sum()`, `Average()`, `Clip(minimum: ... maximum: ...)` (a shortcut for `Clamp`).

### 12.6 Diagnostics (`Debug.stride`)
`DebugPrint()`, `Probe(label: ...)` (passes the signal through and prints it), `Error(message: ...)` (handled specially by the compiler).

---

## 13. Tests

Library tests are `functionTest`s in `library/1.0/tests/`, one file per area (see `UnitTesting.md`). Modules that convert time or frequency are tested in a domain with a known rate; the expected values of the filters, dynamics, controllers and estimators come from a plain-Python model of the same difference equations.
