# Buffer Entities

A `buffer` is a fixed-capacity store that collects a stream of values over time and makes the collected window available as a block. It turns a sample-by-sample stream into frames: values go in one at a time, and the buffer signals when a full window is available to process.

A buffer differs from the other storage entities in these ways:

* A `signal` holds one value, and each write replaces it.
* A `table` is a fixed array that is indexed explicitly.
* A `buffer` accumulates. Each value written to it is appended, and the oldest value is discarded once the buffer is full.

---

## 1. Declaration

```stride
buffer InBuffer {
    type: Real
    capacity: 4
    triggerMode: Buffer::TriggerFull
}
```

| Property | Meaning | Default |
| :--- | :--- | :--- |
| `type` | Element data type (`Real`, `Int`, ...). Every slot holds one value of this type. | `Real` |
| `capacity` | Number of values the buffer holds. Fixed at declaration. Must be a positive integer literal. | required |
| `triggerMode` | When the buffer reports that a window is ready (see §4). | `Buffer::TriggerFull` |
| `hop` | Number of values between triggers for `Buffer::TriggerPeriodic`. | `capacity` |
| `default` | Initial value of every slot. | zero of `type` |
| `domain` | Domain that owns the buffer's storage and processes its writes. | enclosing domain |
| `reset` | Restores the initial state (§2) when triggered. | none |

> **Schema note:** `_Buffer` in `CoreTypes.stride` declares `capacity` (required), `frameSize`, `hop`, `mode`, `triggerMode` and `trigger`. Buffers have no `size` property. The JIT validates the values (see stridejit `doc/buffers_and_frequency_domain.md`).

---

## 2. State

A buffer has the following state:

* **Contents:** `capacity` slots of `type`.
* **Write position:** the slot the next value will go into.
* **Fill count:** how many values have been written since the last reset. It stops increasing at `capacity`.

Initial state, and the state after a reset:

* All slots hold `default`.
* The write position is slot 0.
* The fill count is 0, so the buffer is empty.

---

## 3. Writing: Streaming Into a Buffer

A buffer is written by streaming into it:

```stride
AudioIn >> InBuffer;
```

Each write appends one value:

1. The incoming value is converted to the buffer's `type` if needed (for example, `Int` to `Real`).
2. The value is stored in the slot at the write position.
3. The write position advances by one. After the last slot it wraps back to slot 0.
4. The fill count increases by one, up to `capacity`.
5. The trigger condition is evaluated (§4).

A buffer takes exactly **one value per write**. On the input side it always has size 1, whatever its capacity. So a stream into a buffer is never expanded or duplicated to match the capacity. `capacity` consecutive writes fill the buffer.

Once the buffer is full, each further write overwrites the oldest value. The buffer always holds the most recent `capacity` values.

Writes happen at the rate of the stream that feeds the buffer. In a domain with `rate: R`, a buffer fed by a native-rate stream receives `R` values per second and fills in `capacity / R` seconds.

---

## 4. Triggering: The `ready` Switch

Every buffer has a `ready` member, which is a `switch`. It is evaluated on every write and turns `on` on the write that completes a window.

`triggerMode` decides which writes complete a window:

| `triggerMode` | `ready` turns `on` when | Windows per `capacity` writes |
| :--- | :--- | :--- |
| `Buffer::TriggerFull` | the write fills the last slot | 1 |
| `Buffer::TriggerHalf` | the write fills the middle slot or the last slot | 2 |
| `Buffer::TriggerQuarter` | the write fills the last slot of each quarter of the buffer | 4 |
| `Buffer::TriggerPeriodic` | the write is the `hop`-th value since the previous trigger | `capacity / hop` |

Trigger points are counted from the start of the buffer's storage, so they repeat with every wrap. With `TriggerFull` and `capacity: 4`, `ready` turns on at writes 4, 8, 12, and so on. With `TriggerHalf` it turns on at writes 2, 4, 6, and so on.

On a write that does not complete a window, `ready` is `off`.

Streaming `ready` into a reaction runs that reaction once for each completed window:

```stride
InBuffer.ready >> IncrementReaction(buffer: InBuffer) >> Result;
```

---

## 5. Reading: The Current Frame

A buffer named on its own is only a push target (`AudioIn >> InBuffer`). Reading it is explicit, through members declared in the `_Buffer` schema. `Buf.frame` is the **current frame**: the newest `frameSize` values, oldest first (`frameSize` defaults to `capacity`). Reading the frame is a **peek**: it doesn't consume values or change `Buf.count`.

The usual way to read a frame is to pass the buffer to a reaction through a property port. The reaction gets a pointer to the frame, without a copy:

```stride
reaction IncrementReaction {
    ports: [
        propertyInputPort InBufPort { name: "buffer" block: InBuf }
        mainOutputPort OutPort { block: OutVal }
    ]
    blocks: [
        signal InBuf[InBufPort.size] { type: Real }
        signal OutVal { type: Real }
    ]
    streams: [
        InBuf[0] + 10.0 >> OutVal;
    ]
}

InBuffer.ready >> IncrementReaction(buffer: InBuffer) >> Result;
```

`InBufPort.size` is the buffer's `frameSize`. A loop takes the frame through its main input, `InBuffer.frame >> FrameSum() >> Total;`. Streaming the frame into a block, `InBuffer.frame >> Snapshot;`, copies it; the block must have exactly `frameSize` elements.

**Element order.** `InBuf[0]` is the oldest value of the frame and `InBuf[frameSize - 1]` the newest, for every trigger mode and also across the end of the ring. For example, after writing 1, 2, 3, 4 to a buffer with capacity 4, `InBuf[0]` is 1 and `InBuf[3]` is 4.

`Buf.frame[i]` is not supported yet. Index the copied block, or index inside the reaction or loop.

---

## 6. Example: Framing a Stream

```stride
_domainDefinition BufferDom {
    rate: -1
    inputs:  [ signal AudioIn { domain: BufferDom type: Real } ]
    outputs: [ signal Result  { domain: BufferDom type: Real } ]
    domainFunctionCode: ''
}

buffer InBuffer {
    type: Real
    capacity: 4
    triggerMode: Buffer::TriggerFull
}

AudioIn >> InBuffer;
InBuffer.ready >> IncrementReaction(buffer: InBuffer) >> Result;
```

| Tick | `AudioIn` | Buffer contents | `ready` | `Result` |
| :---: | :---: | :--- | :---: | :---: |
| 1 | 1.0 | `[1, 0, 0, 0]` | off | 0.0 |
| 2 | 2.0 | `[1, 2, 0, 0]` | off | 0.0 |
| 3 | 3.0 | `[1, 2, 3, 0]` | off | 0.0 |
| 4 | 4.0 | `[1, 2, 3, 4]` | **on** | 11.0 |

`Result` keeps its previous value on ticks where the reaction does not run.

---

## 7. Reserved and Planned Features

These names are reserved, but their behavior is not defined yet:

* **Status members:** `halfReady`, `full` and `empty` are recognized as switch members of a buffer, the same way `ready` is. `full` and `empty` are expected to report whether the fill count is `capacity` or 0. `halfReady` is expected to mark the half-way point independently of `triggerMode`.
* **Operational modes:** `mode` selects `Buffer::ModeSliding` (the default, described above). `Buffer::ModeFifo` and `Buffer::ModeOverlapAdd` are reserved and rejected for now. There is no ping-pong mode: ping-pong is `frameSize: capacity / 2` with `Buffer::TriggerHalf`, and the compiler picks the storage layout.

## 8. Open Questions

* **How long `ready` stays on.** `ready` is evaluated on writes, so on a tick with no write it keeps its last value. It is not yet decided whether `ready` should drop back to `off` on the next tick even when nothing is written.
* **`hop` that does not divide `capacity`.** Periodic triggers are counted relative to storage slots. If `hop` does not divide `capacity`, the spacing between triggers changes at each wrap. The options are to require `hop` to divide `capacity`, or to count triggers independently of the write position.
* **Element order outside `TriggerFull`.** With `TriggerHalf`, `TriggerQuarter` or `TriggerPeriodic`, the window can fire before the last slot is written. Slot 0 is then not necessarily the oldest value. Whether a read should present the values oldest-first, rotated to the write position, is not yet decided.
