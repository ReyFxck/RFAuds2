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
stereo S16 and nearest, linear or four-point cubic Lagrange interpolation.
It uses a Q32 phase accumulator.

The caller preserves the unconsumed source tail reported by
`input_frames_consumed` and prepends it to the next chunk. Nearest/linear
retain one source frame; cubic retains three so its four-point window stays
continuous across chunk boundaries. Exact-rate streams use a copy path.
