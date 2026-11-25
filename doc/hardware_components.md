# Hardware components: distribution and lifecycle

Status: plan. It extends `stride_package_spec.md` (the component spec, draft
v4) to platforms, devices, boards and frameworks, so users download only
what their projects use, from git, with no server.

## What a hardware component is

The component spec defines a component as a `Component.stride` manifest plus
`src/`, `frameworks/` and `native/`. A hardware component uses the same
manifest, signing, checksums and CLI. Its tree **mirrors strideroot**, so the
resolver mounts it as one more root and its files need no changes:

```text
stm32f4-1.2.0/
├── Component.stride
├── checksums.sha256
├── stride.sig
├── platforms/STM32F4/{Platform.stride, devices/, boards/, resources/}
├── systems/STM32F4/1.0/System.stride
└── cores/CortexM4.stride            # only when no other component provides it
```

Frameworks with runtimes (`frameworks/STM32/1.0/{platformlib,runtime}`) are
components of their own, because several hardware components share them.

The spec's `frameworks/<Name>/` directories hold *overlay modules* for an
existing framework. A hardware component's `frameworks/<Name>/<version>/`
holds a *whole framework*. The manifest's `kind` (below) says which layout
applies, so the two meanings never mix.

### Manifest

These are additions to the spec's `componentManifest`. Everything else is
unchanged.

```stride
componentManifest Stm32F4 {
    name: "stm32f4"
    version: "1.2.0"
    kind: ComponentKind::Hardware          # Library (the spec's default) | Hardware | Framework | Toolchain
    namespace: "STM32F4"
    minStrideVersion: "0.3.0"
    license: "BSD-3-Clause"

    # What it provides, so the index can answer "who provides board X?"
    # without downloading anything.
    provides: {
        platforms: ["STM32F4"]
        systems: ["STM32F4"]
        devices: ["STM32F4xx", "STM32F407VG"]
        boards: ["STM32F4Discovery"]
    }

    dependencies: [
        componentRequirement { component: "stm32"  minVersion: "1.0" },    # the framework
        componentRequirement { component: "cortex-m"  minVersion: "1.0" }  # cores
    ]
    frameworks: [ frameworkRequirement { framework: "STM32" minVersion: "1.0" } ]
    toolchains: [ componentRequirement { component: "arm-none-eabi-gcc" minVersion: "13" } ]

    lifecycle: { support: Support::Current }
}
```

- `componentRequirement` fills the spec's empty `dependencies: []`. It has
  the same `minVersion`, `maxVersion` and `maxInclusive` fields as
  `frameworkRequirement`, so the validator and the version-range logic are
  shared.
- A framework component's runtime is C source built per board, not a
  prebuilt binary. Its sources are listed in `checksums.sha256`, and it is
  treated as native code for trust: it needs a valid signature, or
  `--trust-unsigned`, as `native: on` does in spec section 6.1.

## Getting components with only git

### Repositories

- **Curated components** stay in one monorepo, `StrideLang/strideroot`, each
  in its own folder. Each component is versioned with prefixed tags:
  `stm32f4/v1.2.0`.
- **Third-party and user components** are any git repository with a
  `Component.stride` at the root, or in a folder of the repository.

### The index

One git repository, `StrideLang/stride-index`, holds `Index.stride`:

```stride
componentEntry Stm32F4 {
    name: "stm32f4"
    git: "https://github.com/StrideLang/strideroot.git"
    path: "platforms/STM32F4"          # sparse-checkout paths, for monorepo components
    versions: [
        componentVersion { version: "1.2.0" tag: "stm32f4/v1.2.0" commit: "<sha>" manifestSha256: "<hash>" }
    ]
    provides: { platforms: ["STM32F4"] boards: ["STM32F4Discovery"] devices: ["STM32F407VG"] }
    lifecycle: { support: Support::Current }
}
```

- `tools/index/update.py` writes it from the components' manifests and tags.
- The index is signed like a component. A user can add their own index
  repositories, and a project can name a component's git URL directly.
- Being Stride, it is parsed by `strideparser`, so no TOML or JSON parser is
  needed (spec section 1).

### Fetching

To fetch a component, `stride` runs a partial, sparse clone of exactly one
tag:

```text
git clone --filter=blob:none --sparse --depth 1 --branch stm32f4/v1.2.0 <git> <cache>
git -C <cache> sparse-checkout set platforms/STM32F4 systems/STM32F4
```

- Components go to `~/.stride/components/<name>-<version>/`. They are
  verified against `checksums.sha256`, `stride.sig` and the index's
  `manifestSha256`, and then mounted.
- `stride` uses only the `git` executable, not libgit2 or a server. A tag is
  never moved or deleted, so old projects keep building.

### Automatic download at build time

1. `use STM32F4 version 1.0 on BlinkRigSTM32F4Discovery` names the system
   and board.
2. When the resolver can't find platform `STM32F4` or board
   `STM32F4Discovery` locally, it asks the index which component `provides`
   it.
3. It resolves dependencies by version range, choosing the newest
   compatible versions, and fetches what's missing.
4. It writes the exact commits to `stride.lock` next to the project:
   `componentLock { name: "stm32f4" version: "1.2.0" commit: "<sha>" manifestSha256: "<hash>" }`.

A build with a lock file installs exactly the locked commits. `--offline`
fails instead of fetching, and `--no-fetch` disables fetching for
reproducible CI that pre-populates the cache.

### Commands

These extend the spec's `stride component init | check | pack | sign | verify`:

| Command | Does |
|---|---|
| `stride component search <text>` | Searches the index: names, `provides` and lifecycle |
| `stride component add <name>[@<range>]` | Adds a component to the project and the lock file |
| `stride component install` | Fetches what the lock file names |
| `stride component update [<name>]` | Moves to the newest versions in range, and rewrites the lock |
| `stride component outdated` | Lists newer versions and lifecycle changes |
| `stride component remove <name>` | Removes a component from the project |
| `stride component list` | Lists installed components and their versions |
| `stride component init --hardware` | Scaffolds a board component for a user's own board, with `Component.stride`, `platforms/<P>/boards/<Board>.stride` and a test rig |

### User-designed boards

There are three tiers, and none needs infrastructure:

1. A board file in the project folder. The resolver already searches it
   first.
2. A board component in the user's own git repository, used by URL.
3. The same component listed in a shared or community index.

## Lifecycle

A board's commercial status and its support in Stride change independently:
a discontinued board can still be fully supported. So there are two axes:

```stride
lifecycle: {
    production: Production::Active   # Active | NotRecommended | EndOfLife | Unknown (vendor status)
    support: Support::Current        # Current | Deprecated | Archived (Stride's support)
    since: "2026-11"                 # when the status last changed
    replacedBy: STM32F407GDisc1      # a board or device to suggest instead
    note: "MB997B/C: no USB serial port; MB997D adds it"
}
```

- `lifecycle` is a property of devices, boards and component manifests.
  Devices and boards inherit it upward through `overlays`, so a chip marked
  `EndOfLife` marks its boards too, unless a board says otherwise.
- The vendor's status comes from its data. ST, for example, publishes
  Active, NRND (not recommended for new design) and Obsolete. `devicegen`
  fills it in, and `stride component outdated` reports changes.

What each value does:

| Status | Build | `stride boards` / `search` |
|---|---|---|
| `production: NotRecommended` | A note | Shown, marked |
| `production: EndOfLife` | A note naming `replacedBy` | Hidden unless `--all` |
| `support: Deprecated` | A warning, with the version support ends in | Shown, marked |
| `support: Archived` | An error unless `--allow-archived`; the component stays fetchable | Hidden unless `--all` |

The index carries the same lifecycle, so listings need no download. An
archived component moves to the index's `archive` section, but its tags stay,
so locked projects still build.

## Mapping onto today's strideroot

| Today | Component |
|---|---|
| `frameworks/STM32/1.0` | `stm32` (Framework) |
| `platforms/STM32F4` and `systems/STM32F4` | `stm32f4` (Hardware) |
| `platforms/STM32F7` and `systems/STM32F7`, `systems/SaturnM7` | `stm32f7` (Hardware); SaturnM7 could be its own board component |
| `cores/*.stride` | `cortex-m` and `avr` (Hardware, cores only) |
| `library/` | stays in strideroot: the language's standard library |
| arm-none-eabi-gcc, OpenOCD, picotool | Toolchain components that point at vendor downloads, extending `ToolManager` |

The base install shrinks to `library/`, the JIT framework and the component
tooling. Everything else is fetched on first use.
