# Base library roadmap

Plan derived from the library review. Each item says what it is, why, and its state.
Status: **done** (committed, not yet compiled), **open**, **decision** (needs your call).

## Phase 0 - Verify what exists (blocking)

Nothing from the cleanup has been compiled or run.

1. Build `stridetest` and run `stridetest strideroot/library/1.0/tests/`.
2. Rebuild `stridec` (the one in `build/bin` fails on the current `Callable.stride`).
3. Fix what the guesses got wrong: array-valued assertions, qualified targets in `functionTest`, `Wedge` loop counters, `Transform`/`MatMul` indexing.
4. Regenerate `stridedoc` output (it still uses `Inverse`, `Cross`, the old filter stubs).

Exit: every test file passes or has a documented, intentional failure (see Phase 1).

## Phase 1 - Defects from the review

| Item | State |
|---|---|
| Stub `LowPass`/`BandPass`; add `HighPass`, `Notch`, `AllPass`, `PeakingEq` | done |
| `BiQuad` fixed (RBJ coefficients, DF2T section, rate from port) | done |
| Duplicates: `Level`/`Pan`, `_CodeGenerator`, `Watch`, `testing/1.0` | done |
| `Clip` vs `Clamp` | open: kept because `tests/data/platform/06_Clip.stride` uses it. Migrate that test data, then remove |
| Empty `Error` module | accepted (special compiler treatment) |
| Intrinsics with no framework definition: `__Pow`, `__Exp`, `__Console*`, `__Midi*`, `__Serial*`, `__ExitDomain` | open, deferred by you. `Pow`/`Exp` are used by `Norm`, `Smooth`, `Debounce` tests, so those tests fail until defined |
| Framework coverage: `__Abs`, `__Floor`, `_RandomInt` only in JIT; `__And` only in JIT and Cpp | open, deferred |
| Tests for every package | mostly done. Missing: IO (Console, Midi), Generators other than `Oscillator` (needs timing spec for triggers), Framework/QualityPolicy schema |

## Phase 2 - Structural consistency

| Item | State |
|---|---|
| Prelude/package name collisions (`Control`, `Math`) | done: `ControlPrelude`, `MathPrelude`, new barrels |
| Time in seconds, converted with port rate | done (Smooth, Pid, SlewRateLimiter, Debounce, ComplementaryFilter, Kalman, dynamics, FractionalDelay, Goertzel) |
| Size from ports | done for LinAlg; `Wedge` replaces `Cross`, quaternion modules stay fixed-size |
| Types from output port, else input | done |
| Switches for booleans | done (Hysteresis, Debounce, PeakDetector) |
| `Inverse` -> `Negate`; `_DeviceType`/`_PolicyBinding` -> `Framework` | done |
| Stuttering names (`Kalman::Filter`, `Compressor::Device`, `Limiter::Fast`, `PeakDetector::Detector`, `FractionalDelay::Line`) | **decision**: flatten to `Estimation::Kalman`, `Dsp::Compressor`, ... or keep one-module packages. Needs a compiler check that `package` can export a module of the same name |
| Move schema types into `core/` | **decision**: breaks every import path; do in one pass with the compiler-repo test data |
| Rename prelude `Control.stride`, move prelude math under `Math` | partly done (Prelude suffix). Full move depends on the `core/` decision |
| Fold `Window` into `Dsp` | **decision** |
| Replace the 49 MIDI note constants with `MidiToFreq` | open (low risk, mostly deletion) |

## Phase 3 - Expansion, in order of value

1. **Scalar math**: `Sqrt`, `Log`, `Atan2`, `Asin`, `Acos`, `Ceil`, `Round`, `Mod`, integer divide. Needs intrinsics (Phase 1), so do them together.
2. **Timers and latches** (IEC 61131-3 style, in seconds via rate): `OnDelay`, `OffDelay`, `Pulse`, `Timeout`, `Elapsed`, `SetReset`/`ResetSet`, `Toggle`, `UpDownCounter`. Most useful for embedded and game timers.
3. **Collections**: `AnyOn`, `Count`, array `Min`/`Max`, `ArgMax`, `IndexOf`, `Contains`, `Sort`, `Shuffle`. Follow `Sum`/`Dot` (loops over port size) with unrolled `@` variants where wanted.
4. **Random**: seeded, reproducible, available on every framework (`Random.stride` exists in the working tree from the boardgame chance work; reconcile with it, do not duplicate), plus `RandomReal` and weighted choice.
5. **Bits and fixed point**: and/or/xor/shift/mask, saturating add, Q formats.
6. **Abstract hardware I/O**: `IO::Digital`, `Analog`, `Pwm`, `I2c`, `Spi`, `Timer`, implemented per framework (embedded-hal model). Fold the JIT-only `Serial` into this.
7. **Framing**: SLIP/COBS encode/decode and CRC.
8. **Strings**: move `Substring`, `Format`, ... out of the JIT platformlib into the library with per-framework implementations.
9. **Faults**: working `Error`/`AssertInRange`, watchdog, safe state, range monitors.

## Phase 4 - Later (language level)

- Units/dimensions on signals.
- Fixed-point types.
- Timed transitions in state machines (`after`).
- Verification back end (assume/guarantee over streams and state machines).
- Generic `Topology` package: move `GridNeighbour` and `GridRay*` up from `libgame/games/GameFunctions.stride` (also serves LED matrices, keypads, cellular automata).

## Process rules going forward

- A new package ships with its `functionTest` file.
- Time properties are in seconds and converted with the port rate; types come from the output port; booleans are switches; sizes come from ports.
- `doc/BaseLibrary.md` is updated in the same change as the code.
- Renames are checked against `Stride/tests/data` and `libgame/games` before landing.

## Decisions needed

1. Stuttering names: flatten or keep?
2. `core/` directory and the Window-into-Dsp fold: do now, or after Phase 0 passes?
3. Order within Phase 3: I would start with timers and latches, then collections and Random.
