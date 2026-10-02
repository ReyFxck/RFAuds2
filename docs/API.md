# API

Include:

```c
#include <rfauds2/rfauds2.h>
```

## Initialization

`rfauds2_init(irx, size)` loads an embedded `rfauds2.irx`, binds RPC and
initializes the device.

`rfauds2_bind()` binds to an IRX that the application already loaded.

## Playback

`rfauds2_submit_s16()` accepts interleaved **48 kHz stereo S16** frames.
It is the compatibility blocking API; use it only when playback can make
space, or when the entire prebuffer fits in the configured queue.

### Asynchronous PCM transport

`rfauds2_submit_s16_async(samples, frames)` launches one SIF RPC with
`SIF_RPC_M_NOWAIT`. It copies 1..960 frames to its own aligned EE staging
buffer before returning 0. The caller may immediately reuse the source.
It does not wait for the IOP to execute the request or consume PCM.

`rfauds2_submit_poll(&accepted_frames)` returns 0 while the RPC is pending,
1 when complete, or a negative transport/protocol error. On success,
`accepted_frames` is the prefix admitted to the IOP ring. Keep the
unaccepted tail and submit it later. Zero accepted frames means the queue
was full; it does not discard or replace queued PCM. Poll from normal EE
code between useful work; do not spin until a full queue drains.

Only one request can be in flight. Collect its result before launching
another request or calling a device control, stats, bind or blocking submit.
Those calls return `RFAUDS2_ERROR_BUSY` while a result is outstanding, even
if DMA has already finished. This keeps both the submit and reply buffers
alive without an allocation, EE worker thread, or audio work in an interrupt.
The device API has one EE owner and is not thread-safe.

The new `TRY_SUBMIT` IOP command never waits for queue space. This keeps
start/resume/flush reachable when playback is paused or stopped and the ring
is full. EE poll collects the accepted prefix, then a control call can run.
Both the client library and IRX must come from this version; an older IRX
returns an error for the new opcode.

This is a bounded transport primitive. An emulator adapter must preserve
blocks larger than 960 frames, keep their unaccepted tails, and schedule
poll/retry without changing emulated audio timing. It is not an automatic
background producer queue. Initialization and device controls remain
synchronous; the existing blocking submit API remains available.

`rfauds2_start()`, `pause()`, `resume()` and `stop()` control the stream.

`rfauds2_flush()` discards queued ring-buffer PCM. If an application needs
the currently active DMA block discarded immediately, stop, flush and start.

## Queue and volume

`rfauds2_set_latency_ms()` rounds the requested queue size up to 512-frame
hardware blocks and clamps it to the physical 4096-frame ring.

`rfauds2_set_volume()` uses native 0..0x3fff SPU2 units.

## Diagnostics

`rfauds2_get_stats()` reports queue depth, underruns, producer backpressure,
latency, volume, state, min/max queue depth, refill count and silent frames.

`rfauds2_reset_stats()` starts a new diagnostic window without changing the
queue.

## Rate conversion

The helper converter accepts arbitrary non-zero input/output rates, mono or
stereo S16 and nearest, linear, four-point cubic Lagrange or optional
8-tap Lanczos-windowed sinc interpolation.
It uses a Q32 phase accumulator.

The caller preserves the unconsumed source tail reported by
`input_frames_consumed` and prepends it to the next chunk. Nearest/linear
retain one source frame; cubic retains three so its four-point window stays
continuous across chunk boundaries. Exact-rate streams use a copy path.


## Bus mixer

`rfauds2_bus_mixer` provides four named buses:

- Game
- Music
- SFX
- UI

Each bus has an independent Q15 gain and mute flag. Initialize the state with
`rfauds2_bus_mixer_init()`, clear an output block with
`rfauds2_bus_mixer_clear_s16()`, then mix any number of sources into the
appropriate bus with `rfauds2_bus_mixer_mix_s16()`.

The helper is intentionally allocation-free and does not own producer
lifetimes; an emulator or engine can keep one rate converter per producer and
mix the resulting PCM into these buses.


### Sinc8 scope

`RFAUDS2_RESAMPLE_SINC8` is a 256-phase, 8-tap fixed-point
Lanczos-windowed sinc kernel intended for **upsampling**, including common
emulator paths such as 32 kHz -> 48 kHz and 44.1 kHz -> 48 kHz. The phase
rows are normalized to unity gain.

Sinc8 deliberately rejects downsampling for now because doing that correctly
also requires a ratio-dependent anti-alias low-pass. Use linear/cubic for
current downsampling needs rather than silently accepting an aliased sinc
configuration.
