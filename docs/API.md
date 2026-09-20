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
