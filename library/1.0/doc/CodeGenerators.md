# Modules, Reactions and Loops

Modules, reactions and loops are the three kinds of callable code written in
Stride. Each one is a declaration with ports, blocks and streams, and is called
from a stream like a function:

```stride
Input >> Smooth(amount: 0.5) >> Output;
```

They differ in when they run, whether they keep state between calls, and
whether their streams repeat:

| | Runs | State between calls | Streams |
| :--- | :--- | :--- | :--- |
| `module` | every time the stream that calls it runs | yes, in blocks it reads before writing | once per call |
| `reaction` | when its trigger is on | no | once per call |
| `loop` | every time the stream that calls it runs | no | repeated until `terminateWhen` is on |

The schema types are `_Module`, `_Reaction` and `_Loop` in
`CodeGenerators.stride`. Ports and their attributes are in `Callable.stride`.

---

## 1. Parts of a declaration

```stride
module Scale {
    ports: [
        mainInputPort InputPort { block: Input }
        mainOutputPort OutputPort { block: Output }
        propertyInputPort GainPort { name: "gain" block: Gain }
    ]
    blocks: [
        signal Input { type: Real }
        signal Output { type: Real }
        signal Gain { type: Real default: 1.0 }
    ]
    streams: [
        Input * Gain >> Output;
    ]
}

Signal >> Scale(gain: 2.0) >> Louder;
```

| Part | Meaning |
| :--- | :--- |
| `ports` | How the call connects to its caller. Each port names the block it is bound to. |
| `blocks` | Signals, switches, triggers and nested callables private to the declaration. |
| `streams` | The code. Streams run in the order they are written. |

There are four kinds of port:

| Port | Connects to |
| :--- | :--- |
| `mainInputPort` | What comes before the call in the stream: `Input >> F()`. Modules and loops only: a reaction's input is its trigger (§3). |
| `mainOutputPort` | What comes after the call in the stream: `F() >> Output`. |
| `propertyInputPort` | A named property of the call: `F(gain: 2.0)`. The property is the port's `name`, or the port's own name when it has none. Properties are matched by name, so their order in the call does not matter. |
| `propertyOutputPort` | A named property the call writes to. |

### Port attributes

A port has read-only attributes, filled in by the compiler from what is
connected to it: `size`, `rate`, `domain` and `type`. Blocks use them to adapt
to the call:

```stride
signal Input[InputPort.size] { type: Int }     # as many values as feed the call
signal Output[OutputPort.size] { type: Int }   # as many values as the call writes to
signal Sum { domain: OutputPort.domain }       # runs in the caller's domain
```

`InputPort.size` is the size of what feeds the call. `OutputPort.size` is the
size of what the call writes to. Both are known at compile time. A port sized
this way must be connected. See [§5](#5-loops-driven-by-their-input-or-their-output).

---

## 2. Modules

A module runs its streams once each time the stream that calls it runs. Each
call in the source is a separate instance with its own state.

A block that the module reads before writing keeps its value from one call to
the next. This is how a module accumulates:

```stride
module Accumulator {
    ports: [
        mainInputPort InputPort { block: Input }
        mainOutputPort OutputPort { block: Output }
    ]
    blocks: [
        signal Input { type: Int domain: OutputPort.domain }
        signal Output { type: Int domain: OutputPort.domain }
        signal Acc { default: 10 type: Int reset: Reset domain: OutputPort.domain }
    ]
    streams: [
        Acc + Input >> Acc;   # Acc is read before it is written: it is state
        Acc >> Output;
    ]
}
```

A block that is always written before it is read is temporary and costs
nothing between calls. `reset: Trigger` returns a block to its `default` when
the trigger is on. A module with no state passes no state at all.

---

## 3. Reactions

A reaction runs in response to something. It has no state between calls: its
blocks start from their defaults every time it runs.

A reaction has no main input port. Its input is automatic: a trigger, which
decides whether it runs and sets the domain it runs in. A `trigger` is a
switch that turns off by itself at the end of its domain's processing, so a
reaction runs once each time its trigger is set. A buffer's `ready` is a
trigger (see `Buffer.md`).

A switch can be given where a trigger is expected: it is converted to a
trigger automatically, which takes the switch's value. The reaction then runs
on every tick the switch is on. (Other conversions, such as running only when
the switch turns on, may be added later.)

```stride
trigger Restart {}
switch Paused {}

Restart >> Reset();               # runs once each time Restart is set
Paused >> ShowPauseMenu();        # Paused converts to a trigger: runs on every
                                  # tick Paused is on
on >> Report(value: Count);       # on: runs every time
```

Values reach a reaction through property ports.

Declaring a `mainInputPort` in a reaction is a compile error
(`ReactionMainInput`). Types the compiler treats as reactions, such as the
game library's `action`, follow the same rule. Code that should run every time its stream does, with
a value coming in, is a module:

```stride
module EntityTypeAt {
    ports: [
        mainInputPort LocationPort { block: Location }
        mainOutputPort EntityTypePort { block: EntityType }
    ]
    ...
}

Square >> EntityTypeAt() >> Occupant;
```

A reaction whose trigger is off writes nothing: its outputs keep their
previous values.

---

## 4. Loops

A loop repeats its streams until its `terminateWhen` switch is on. It has no
state between calls: every call starts again.

```stride
loop Sum {
    ports: [
        mainInputPort InputPort { block: Input }
        mainOutputPort OutputPort { block: Output }
    ]
    blocks: [
        signal Input[InputPort.size] { type: Int }
        signal Output { default: 0 type: Int }
        signal Index { default: 0 type: Int reset: Reset }
        switch Done { default: off }
        trigger Reset {}
    ]
    onExecution: Reset
    terminateWhen: Done
    streams: [
        Output + Input[Index] >> Output;
        Index + 1 >> Index;
        [Index, InputPort.size] >> Equal() >> Done;
    ]
}

Values >> Sum() >> Total;
```

| Property | Meaning |
| :--- | :--- |
| `terminateWhen` | The switch that ends the loop. The loop ends right after a stream that turns it on, without running the rest of that pass. Otherwise it is checked at the end of each pass. |
| `onExecution` | A trigger turned on at the start of each call, usually the `reset` of the loop's counters. |
| `onTermination` | Declared in the schema, not implemented yet. |

**Every call starts again.** The loop's own blocks start from their
`default`, and an output that holds one value is set to its `default` before
the first pass. That is why `Output` above starts at 0 on every call.
`onExecution: Reset` makes this explicit; it is how the library writes loops.

**An array output is not set first.** An output declared as an array
(`Output[OutputPort.size]`) is the caller's array, written in place. Entries
the loop does not write keep their values, even if the block has a `default`.

A loop is fast: its blocks become registers or stack values, and nothing is
kept between calls. Use a loop instead of a broadcast (`F[]()`) over a large
board or table: a broadcast makes one instance per element at compile time.

---

## 5. Loops driven by their input or their output

A loop that works on an array takes the array's size from what it is
connected to, so the same loop works for any size.

**Driven by its input.** `Input[InputPort.size]` has as many elements as what
feeds the call. The library's reductions work this way:

```stride
[A, B, C] >> AllOn() >> AllSet;     # InputPort.size is 3
Squares[0:63] >> AllOff() >> Empty; # InputPort.size is 64
```

**Driven by its output.** `Output[OutputPort.size]` has as many elements as
what the call writes to. The loop fills it in place, so a loop can fill a
table it doesn't know the size of:

```stride
# GameFunctions.stride: one entry per ray, as many as the table
loop GridRayAheadTable {
    ports: [ mainOutputPort OutputPort { block: Output } ]
    blocks: [
        signal Output[OutputPort.size] { type: Int }
        signal Index { default: 0 type: Int reset: Reset }
        switch Done { default: off }
        trigger Reset {}
    ]
    onExecution: Reset
    terminateWhen: Done
    streams: [
        Index >> GridRayAhead() >> Output[Index];
        Index + 1 >> Index;
        [Index, OutputPort.size] >> Equal() >> Done;
    ]
}

constant RayAhead[513] { type: Int }
GridRayAheadTable() >> RayAhead;    # OutputPort.size is 513
```

What a sized output can write to:

| Destination | `OutputPort.size` | How it is written |
| :--- | :--- | :--- |
| An array: `>> Table` | the array's size | in place |
| A slice or bundle: `>> Table[2:4]`, `>> [A, B]` | the number of members | into a temporary array, then copied to each member |
| One element or signal: `>> Table[3]`, `>> X` | 1 | in place |

**Driven by both.** A loop can size both sides. It decides which size to count
to. This one stops at the output's size:

```stride
loop Double {
    ...
    blocks: [
        signal Input[InputPort.size] { type: Int }
        signal Output[OutputPort.size] { type: Int }
        ...
    ]
    streams: [
        Input[Index] * 2 >> Output[Index];
        Index + 1 >> Index;
        [Index, OutputPort.size] >> Equal() >> Done;
    ]
}

Table >> Double() >> Doubled;
```

The compiler does not compare the two sizes. Indexing past the end of either
array is an error in the loop that the compiler cannot see, so count to the
smaller one when they can differ.

Modules use port sizes the same way.

### When there is no size

A port sized from its connection must have one. These are compile errors
(`UnsizedPort`):

```stride
Fill();                        # the output goes nowhere
Fill() >> Double() >> Out;     # Double sizes its input from Fill, Fill sizes
                               # its output from Double: neither is known
Add();                         # Add sizes its input from what feeds it
```

The message names the call and the port:

```
'Fill()' takes the size of its output from what it writes to (OutputPort.size), so it must write to a declared array, element or bundle
```

Write to a declared array first and read from it in the next stream:
`Fill() >> Table; Table >> Double() >> Out;`.

### Filling a constant

A constant declared with a type and no value is written once, by the first
stream that writes it, and is read-only from then on. With an output-driven
loop, a table is declared by its size and filled from Stride code:

```stride
constant RayAhead[513] { type: Int }

gameDefinition Othello {
    setupActions: [
        GridRayAheadTable() >> RayAhead;
        ...
    ]
}
```

A second stream that writes the constant is a compile error
(`ConstantWrittenTwice`). A constant with a value is written by its value, so
writing it from a stream is the same error. A constant is a global, not part
of any domain's state, so it costs nothing when a domain's state is copied or
reset.

---

## 6. Calling

| Call | Meaning |
| :--- | :--- |
| `In >> F() >> Out;` | Main input and main output. |
| `F(name: value)` | A property. Values can be literals, constants or blocks. |
| `Trigger >> R();` | A reaction run when its trigger is on. A switch converts to a trigger automatically. |
| `[A, B] >> F()` | A bundle into the main input. A loop or module receives it as one array. |
| `Values[0:8] >> F[]() >> Out[0:8];` | One call per element (broadcast). The compiler also broadcasts on its own when a bundle goes into a call that takes fewer values. |

Calls of the same function in different places are separate instances. A
module instance keeps its own state.

---

## 7. Current limitations

- A loop or module with no ports cannot be called on its own (`Loop();`): the
  call passes an argument it has no port for. Give it an input or an output.
- `onTermination` is in the schema but not implemented.
- `iterator` blocks (an index with `increment`, `limit` and `done`) appear in
  a stridejit fixture but are not tested. Count with a signal, as above.
- `OutputPort.size` is tested in loops and modules, not in reactions.
- `terminateWhen` also ends a reaction early in stridejit
  (`reaction_terminate_when.stride`), but `_Reaction` does not declare it.

The stridejit tests that cover this document are `JITControlFlowTest.*` in
`tests/jit_control_flow_tests.cpp`, with the fixtures `loop.stride`,
`loop_output_size.stride`, `loop_output_cases.stride`,
`loop_output_chained.stride`, `loop_output_unwritten.stride`,
`loop_input_unfed.stride` and `constant_fill.stride`.
