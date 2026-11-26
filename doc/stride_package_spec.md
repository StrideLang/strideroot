# Stride Component & External Library Distribution Specification (Draft v4)

> [!NOTE]
> **Terminology (`component` vs. `package` / `library` / `bundle`)**: In Stride, `package` is already the AST keyword for namespaces (`package Control`), `library` refers to `strideroot/library`, `module` is a callable entity, `plugin` refers to `strideroot/plugins`, and `bundle` has historic meaning in Stride sources. To avoid keyword and mental-model collisions, this specification uses **`component`** (`Component.stride`, `componentManifest`, `.stridecomp`, `stride component ...`) for distributable external code + binary units.

> [!NOTE]
> This specification builds on Stride's existing `package` / `import Foo::Bar as Alias` AST nodes, `STRIDEROOT/library` layout, `platformModule` declarations (`@symbol`, `%%intokens:N%%`, `outTokens`, `linkTo`), LLVM ORC JIT (`StrideEnvironment`), and `stride` CLI driver.

---

## 1. Goals & Scope

This specification defines a standard format and toolchain contract for distributing **external Stride components** that:
1. Use a **Stride-native manifest (`Component.stride`)** parsed by `strideparser` (zero external TOML/YAML parser dependencies).
2. Explicitly declare typed **codegen framework requirements** (`frameworkRequirement` with `minVersion` / `maxVersion`) when providing `platformModule`s, supporting both LLVM/JIT and C/C++ text-emitter compilation engines.
3. Contain Stride source modules (`.stride`) and optional documentation/tests.
4. Optionally include **native binaries** (shared libraries for JIT, static archives/objects for AOT and embedded targets) exposed via `platformModule`, linked/loaded **by default** for the owning component without extra per-module properties.
4. Allow declaring **installed system native library dependencies** verified at compile time and gated behind an explicit CLI flag.
5. Prevent **import name clashes** and **stdlib shadowing** when importing by name rather than file path.
6. Enforce **safety guarantees**: cryptographic signing, integrity verification, symbol pollution prevention, and defense-in-depth hardening.

---

## 2. Component Distribution Format & Layout

### 2.1 Distribution Forms
A Stride component may be distributed as:
- **Directory component** (unpacked, for local development or installed cache): `<component-name>-<version>/`
- **Archive component** (`.stridecomp`, a deterministic `.tar.gz` or `.zip` containing the directory layout below, plus detached/embedded signature).

### 2.2 Standard Directory Layout

```text
acme_dsp-1.2.0/
├── Component.stride            # Stride-native component manifest (required)
├── stride.sig                  # Ed25519 signature over Component.stride + checksums.sha256
├── checksums.sha256            # SHA-256 of all .stride and native binary files
├── LICENSE
├── src/                        # Shared pure-Stride source root
│   ├── AcmeDsp.stride          # Barrel / entry file (`package AcmeDsp`)
│   ├── Filter.stride           # Submodule (`AcmeDsp::Filter`)
│   └── internal/               # Private helpers (not exported unless re-exported)
│       └── Coeffs.stride
├── frameworks/                 # Optional per-framework platformModule & header overlays
│   ├── JIT/
│   │   └── FilterNative.stride # LLVM/JIT platformModules (`processing: "@..."`)
│   └── Cpp/
│       ├── FilterNative.stride # C++ emitter platformModules (`include:`, `outTokens:`)
│       └── include/acme_dsp.h  # C/C++ headers for text-emitter targets
└── native/                     # Optional prebuilt native binaries keyed by target triple
    ├── x86_64-pc-windows-msvc/
    │   ├── jit/
    │   │   └── acme_dsp.dll    # Shared library loaded by stridejit
    │   └── aot/
    │       └── acme_dsp.lib    # Static archive linked by stride compile/build
    ├── x86_64-unknown-linux-gnu/
    │   ├── jit/libacme_dsp.so
    │   └── aot/libacme_dsp.a
    ├── aarch64-apple-darwin/
    │   ├── jit/libacme_dsp.dylib
    │   └── aot/libacme_dsp.a
    └── avr/                    # Embedded/cross-compile targets (AOT only)
        └── atmega328p/
            └── libacme_dsp.a
```

### 2.3 Stride-Native Manifest (`Component.stride`) & Typed Framework Requirements

Using Stride's existing declarative entity syntax allows `strideparser` and `CodeValidator` to parse and type-check manifests directly without adding a TOML parser or custom version-range string parser.

#### Built-in `frameworkRequirement` Entity
Define a standard entity type `frameworkRequirement` in the toolchain schema so `frameworks:` only accepts typed `frameworkRequirement` instances:

```stride
# Schema / platform-level definition
entity frameworkRequirement {
    framework: ""               # Framework name, e.g., "JIT", "Cpp", "AVR"
    minVersion: "1.0"           # Minimum supported framework version (inclusive)
    maxVersion: ""              # Optional maximum version (empty = no upper bound)
    maxInclusive: off           # off => [minVersion, maxVersion), on => [minVersion, maxVersion]
}
```

#### Example `Component.stride`

```stride
componentManifest AcmeDsp {
    name: "acme_dsp"
    version: "1.2.0"
    namespace: "AcmeDsp"                # Root Stride namespace (must match `package AcmeDsp` in entry)
    minStrideVersion: "0.1.0"
    maxStrideVersion: "0.2.0"
    license: "BSD-3-Clause"
    entry: "src/AcmeDsp.stride"

    exports: ["AcmeDsp", "AcmeDsp::Filter"]
    strictExports: on                   # Files outside `exports` cannot be imported directly

    dependencies: [
        # Other Stride components
    ]

    # Only accepts `frameworkRequirement` entities (enforced by schema/validator).
    # Checked against the active System's framework and its `inherits` chain (e.g., AVR -> JIT).
    frameworks: [
        frameworkRequirement {
            framework: "JIT"
            minVersion: "1.0"
            maxVersion: "2.0"
        },
        frameworkRequirement {
            framework: "Cpp"
            minVersion: "1.0"
        }
    ]

    # Internal/installed native system libraries (verified at compile time; gated by --allow-system-libs)
    systemDependencies: [
        # e.g., "fftw3", "blas", "sndfile"
    ]

    features: ["simd"]
    defaultFeatures: ["simd"]

    native: on                          # Package ships native binaries in native/<triple>/
    nativeAbi: "c"

    nativeTargets: [
        target "x86_64-pc-windows-msvc" {
            jit: "native/x86_64-pc-windows-msvc/jit/acme_dsp.dll"
            aot: "native/x86_64-pc-windows-msvc/aot/acme_dsp.lib"
            systemLibs: []
        },
        target "x86_64-unknown-linux-gnu" {
            jit: "native/x86_64-unknown-linux-gnu/jit/libacme_dsp.so"
            aot: "native/x86_64-unknown-linux-gnu/aot/libacme_dsp.a"
            systemLibs: ["m"]
        },
        target "avr-unknown-unknown" {
            mcu: "atmega328p"
            aot: "native/avr/atmega328p/libacme_dsp.a"
        }
    ]
}
```

---

## 3. Native Binaries & `platformModule` Integration

Stride already models native bindings via `platformModule` (e.g., `strideroot/frameworks/JIT/1.0/platformlib/Math.stride` and `Cpp/1.0/platformlib/Math.stride`). No `platformFunction` keyword or new `nativeTarget` property is needed.

### 3.1 Framework Compatibility & Reusing `platformModule` Properties
- **Framework Dependency Rule**:
  - **Pure-Stride components** (no `platformModule`s) are codegen-agnostic and may omit `frameworks`.
  - **Components with `platformModule`s** **must** declare `frameworks` using `frameworkRequirement` entities in `Component.stride` (and any non-`frameworkRequirement` element in `frameworks:` is a schema validation error). At compile time, the toolchain verifies that the active system's framework and version (or an ancestor in its `inherits` chain, such as `DesktopTemplate -> Cpp`) satisfy `[minVersion, maxVersion)` / `[minVersion, maxVersion]` for at least one declared `frameworkRequirement`.
  - When a package supports multiple frameworks, the loader resolves `frameworks/<FrameworkName>/*.stride` matching the active framework (or its inherited base framework) and exposes its C/C++ `include/` directory to text-emitter targets.
- **JIT / LLVM IR targets (`JIT` / `Codegen::Llvm`)**: Use `@symbol` in `processing:` (matching existing JIT `platformModule`s like `processing: "@fabs"`), assuming the external symbol is provided by the package's own bundled binary by default:
  ```stride
  package AcmeDsp::Filter

  platformModule BiquadStep {
      inputs: [Real, Real, Real]
      outputs: [Real]
      processing: "@acme_dsp_biquad_step"
      inherits: [_Signal]
  }
  ```
- **Code-generating / C++ targets**: Continue using `%%intokens:N%%` / `outTokens` / `include:` as in `Cpp` / `DesktopTemplate` frameworks:
  ```stride
  platformModule BiquadStep {
      inputs: [Real, Real, Real]
      outputs: [Real]
      include: ["acme_dsp.h"]
      processing: ""
      outTokens: ["acme_dsp_biquad_step(%%intokens:0%%, %%intokens:1%%, %%intokens:2%%)"]
      inherits: [_Signal]
  }
  ```
- **Default Linking**: Any `platformModule` declared inside a package with `native: on` automatically binds to that package's `nativeTargets` artifact (`jit` or `aot`) for the active target triple—no `nativeTarget` property is required.
- **`linkTo` for System Libraries**: Keep `linkTo: [...]` only when a `platformModule` specifically requires an external system library (which must also be declared in `systemDependencies` in `Package.stride` and enabled via `--allow-system-libs`).

### 3.2 System Library Dependencies (Compile-Time Verification)
Packages may declare `systemDependencies` (e.g., `["fftw3", "sndfile"]`) for native libraries installed on the host/target system:
1. **Explicit Opt-In Flag**: Disabled by default. Compiling or JIT-running a package with non-empty `systemDependencies` requires `--allow-system-libs` (or `--allow-system-lib=<name>`).
2. **Compile-Time Verification**: Before codegen/JIT, the driver verifies each declared system library exists in standard system/SDK paths (or `-L` paths) using platform library lookup (`lib<name>.so` / `lib<name>.a` / `<name>.lib` / `.dylib` or `pkg-config` / linker probe) and fails early with a clear missing-dependency diagnostic if absent.

### 3.3 JIT Execution (`stridejit`)
In `StrideEnvironment`:
1. Match the host LLVM target triple to `nativeTargets` -> `jit` binary.
2. Load into a **dedicated per-package `llvm::orc::JITDylib`** (or a symbol-filtered `DynamicLibrarySearchGenerator`) rather than polluting the global process symbol table.
3. Only expose symbols explicitly named by `@<symbol>` in that package's `platformModule` declarations.
4. Load any verified `systemLibs` / `systemDependencies` (when `--allow-system-libs` is active).

### 3.4 AOT Compilation & Linking (`stride compile` / `stride build`)
In `stridelang` (`driver.cpp` / `board_builder.cpp`):
1. Match `--target` / `--mcpu` / `--boardOverride` to `nativeTargets` -> `aot` static archive (`.a` / `.lib`).
2. Append the package's `aot` archive and verified `systemLibs` to the linker invocation.
3. Never fall back to host binaries when cross-compiling; emit an error if the target triple/MCU is absent from `nativeTargets`.

---

## 4. Namespacing, Shadowing & How Modern Languages Handle It

Because Stride imports by module/package name (`import Control::Pid as Pid`) rather than file path, name clashes must be prevented structurally.

### 4.1 How Modern Languages (e.g., Rust) Handle This
- **Distinct Package Roots**: In Rust, every external crate is rooted at its own top-level namespace (`crate_name::...`) defined in `Cargo.toml`, separate from `std::`.
- **Two Packages with the Same Name**: Cannot coexist in the same scope unless explicitly renamed in the manifest (`my_alias = { package = "dsp", ... }`).
- **Ambiguity vs. Shadowing**:
  - `std` / core primitives cannot be silently replaced by third-party crates.
  - Bringing two identical names into the same scope via imports is a **hard ambiguity error** (`E0252` / `E0659`) unless aliased (`use a::Foo as AFoo`).
  - Local bindings shadowing outer/prelude items are allowed but trigger compiler lints/warnings (`unused` / `shadowing` lints) when unintentional.

### 4.2 Stride Namespacing & Shadowing Rules

1. **Mandatory Root Namespace per Package**:
   - Every external package must declare `namespace: "<Name>"` in `Package.stride`, matching `package <Name>` in its barrel file.
   - All submodules must live under that root (`AcmeDsp::Filter`).
2. **Qualified Imports by Default**:
   - `import AcmeDsp` loads only `entry` (`AcmeDsp.stride`), keeping symbols under `AcmeDsp::` (or `import AcmeDsp as Dsp`).
   - `strictExports: on` prevents importing private internal files (`AcmeDsp::internal::Coeffs`).
3. **Never Shadow Standard Library or Framework Functions (Hard Error)**:
   - Reserve `STRIDEROOT/library` top-level namespaces (`Math`, `Control`, `Dsp`, `IO`, `LinAlg`, `Buffer`, etc.), framework `platformlib` entities, and `Std::*` / `Stride::*`.
   - Any external package attempting to declare or export a symbol/module that shadows a standard library or framework function/entity **must fail with a compile-time error**.
4. **Other Shadowing -> Compiler Warning**:
   - If a user-defined entity or non-stdlib import shadows an earlier non-stdlib declaration, emit a **compiler warning** (upgradeable to an error via `--deny-warnings` / `-Werror`).
   - If two external packages export the exact same unqualified module name on the search path, fail with an **ambiguity error** requiring `import ... as ...` or explicit `--pkg` disambiguation.

---

## 5. Toolchain & Compiler Flags

| Flag | Scope | Purpose |
| :--- | :--- | :--- |
| `-L <dir>`, `--lib-path <dir>` | `compile`, `build`, JIT | Add external package search directory (distinct from `-I` for raw `.stride` includes). |
| `--pkg <name>[@<ver>]` or `--pkg <path>` | `compile`, `build`, JIT | Explicitly enable/pin an external package without a project `Package.stride`. |
| `--no-external-libs` | `compile`, `build`, JIT | Disable external package discovery (stdlib + framework only). |
| `--allow-native` / `--no-native` | `compile`, `build`, JIT | Gate loading/linking packages with `native: on`. |
| `--allow-system-libs` / `--allow-system-lib=<name>` | `compile`, `build`, JIT | Explicitly permit packages to link against installed `systemDependencies` (verified at compile time). |
| `--verify-sig` / `--trust-unsigned` | `compile`, `build`, JIT | Require Ed25519 signature verification vs. allow unsigned local dev packages. |
| `--keyring <path>` | `compile`, `build`, JIT | Trusted public keys directory (`~/.stride/trusted_keys/`, `STRIDEROOT/keys/`). |
| `--deny-warnings` | `compile`, `build` | Treat shadowing and other warnings as errors. |

### Proposed `stride component` Subcommands
- `stride component init` — scaffold `Component.stride` and `src/<Namespace>.stride`.
- `stride component check` — validate `Component.stride`, `frameworkRequirement` entries, namespace/stdlib non-shadowing, exports, and `platformModule` symbols.
- `stride component pack` — create a deterministic `.stridecomp` archive + `checksums.sha256`.
- `stride component sign --key <privkey>` — sign `Component.stride` + `checksums.sha256` into `stride.sig`.
- `stride component verify <component>` — verify checksums, signature, and binary import tables.

---

## 6. Safety: Signing, Symbol Pollution & Further Hardening

### 6.1 Component Signing & Integrity
- **`checksums.sha256`**: Covers `Component.stride`, all `src/**/*.stride`, and all `native/**/*` binaries; verified before parsing or loading.
- **`stride.sig`**: Ed25519 signature over `Component.stride` + `checksums.sha256`, checked against `~/.stride/trusted_keys/` and `STRIDEROOT/keys/`.
- **Default Policy**:
  - Pure Stride components in local `-L` paths: checksum-verified; signature optional unless `--verify-sig` is passed.
  - Components with `native: on`: require a valid trusted signature **or** explicit `--trust-unsigned --allow-native`.

### 6.2 Preventing Symbol Pollution
- **Stride AST**: Enforce `exports` visibility; do not leak transitive imports (`A` imports `B` -> user importing `A` does not get `B` in global scope).
- **AOT Linker**: Mangle Stride-generated symbols with `<pkg>_<verhash>_<module>_<symbol>`. Require package native binaries to prefix exported C symbols (`<pkg>_*`) and compile with `-fvisibility=hidden` / explicit `.def` export lists. Detect duplicate exported symbols across `.a` archives before final linking.
- **JIT (`stridejit`)**: Isolate each package's `.dll`/`.so`/`.dylib` in its own `JITDylib` with a symbol allowlist restricted strictly to `@<symbol>` names declared in that package's `platformModule`s.

### 6.3 Further Security Hardening Steps (Defense-in-Depth)
Beyond cryptographic signing and `--allow-native` / `--allow-system-libs` flags, the following layers can progressively harden untrusted or third-party native code:
1. **Binary Import / PLT Auditing (`stride bundle verify` & pre-load)**:
   - Inspect native binaries with `llvm-readobj` / `nm` / `dumpbin` before loading/linking to reject binaries that import high-risk OS APIs (e.g., `system`, `execve`, `dlopen`, `LoadLibrary`, `VirtualAlloc`, `socket`) unless explicitly declared and approved.
2. **OS-Level JIT Sandboxing (Out-of-Process Execution)**:
   - Run untrusted JIT code in a separate worker process confined with OS sandboxes (Linux `seccomp-bpf` / Landlock, Windows AppContainer / Job Objects, macOS Seatbelt) communicating audio/control buffers over shared memory, so a buggy or malicious native binary cannot compromise the host IDE/compiler process.
3. **WebAssembly / WASI Target for Pure-Compute Plugins**:
   - Allow bundles to ship a `.wasm` binary under `nativeTargets` for pure DSP/math `platformModule`s, executed in an in-process Wasm sandbox with deterministic memory bounds and zero OS syscall access.
4. **Capability Declarations & OS Code Signing**:
   - Require `Bundle.stride` to declare capabilities (e.g., `capabilities: ["realtime-audio"]` vs `["filesystem", "network", "hardware-io"]`) and optionally verify OS-native signatures (Windows Authenticode / macOS `codesign`) on `.dll`/`.dylib` files.
5. **Reproducible Builds & Provenance**:
   - Pair `.stridebundle` releases with SLSA-style build provenance / SBOM metadata so users can verify a binary artifact was built from a specific tagged source commit.
