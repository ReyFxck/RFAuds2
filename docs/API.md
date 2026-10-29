# API

Include:

```c
#include <rfauds2/rfauds2.h>
```

RFAuds2's device boundary is always **48 kHz, stereo, signed 16-bit PCM**.
The helper converter can be used before submission when a producer runs at a
different native rate.

## Initialization and protocol matching

`rfauds2_init(irx, size)` loads an embedded `rfauds2.irx`, binds the RPC
server, checks the protocol version, then initializes the hardware backend.

`rfauds2_bind()` binds to an already-loaded IRX and performs the same protocol
check/initialization.

The EE client and IRX must come from the same build. A mismatched/older IRX is
rejected with `RFAUDS2_ERROR_PROTOCOL` before SPU2 initialization.

## Shutdown and ownership

`rfauds2_shutdown()` stops playback and releases RFAuds2's playback thread,
semaphores and SPU2 DMA interrupt handler. The RPC server remains resident, so
`rfauds2_bind()` can initialize it again later.

RFAuds2 is a direct backend and expects exclusive SPU2/DMA ownership while it
is initialized. `shutdown()` releases RFAuds2's ownership, but cannot restore
a previous audio stack's register state or interrupt handler. Reinitialize the
other stack after shutting RFAuds2 down.

## Blocking submission

`rfauds2_submit_s16(samples, frames)` accepts interleaved 48 kHz stereo S16.
It is the compatibility blocking API. Large submissions are split into RPC
blocks internally. If the configured queue is full, the IOP side waits for
space, so do not use this path while playback is stopped/paused with a full
queue.

## Asynchronous PCM transport

`rfauds2_submit_s16_async(samples, frames)` launches one NOWAIT SIF RPC. It
accepts 1..`RFAUDS2_ASYNC_MAX_FRAMES` (960) frames and copies them into
library-owned aligned storage before returning.

`rfauds2_submit_poll(&accepted_frames)` returns:

- `0` while pending;
- `1` when complete;
- a negative error on failure.

On completion, `accepted_frames` is the prefix admitted to the IOP ring. The
caller must preserve and retry the unaccepted tail. A zero acceptance means the
queue was full; PCM was not silently dropped.

Only one device RPC may be outstanding. Device controls, blocking submission,
bind/shutdown and synchronous stats return `RFAUDS2_ERROR_BUSY` until the
pending result is collected.

## Playback controls

`rfauds2_start()` starts DMA after priming both real 512-frame halves from the
queue. If hardware start fails, the priming step is rolled back so queued PCM
is not consumed accidentally.

`rfauds2_pause()` mutes/pauses consumption while retaining queued PCM.

`rfauds2_resume()` resumes normal consumption.

`rfauds2_stop()` stops DMA but does not discard the IOP ring.

`rfauds2_flush()` clears queued ring PCM. To discard the block currently in
DMA as well, stop first, then flush, then restart/prebuffer as needed.

## Queue latency

`rfauds2_set_latency_ms(ms)` converts the requested time to 48 kHz frames,
rounds to the nearest 512-frame block and clamps to the 4096-frame ring.
Examples: 43 ms -> 2048 frames (~42.67 ms), 64 ms -> 3072 frames, 86 ms ->
4096 frames.

Shrinking below current queue occupancy returns `RFAUDS2_ERROR_BUSY`. Drain or
flush first.

## Volume

`rfauds2_set_volume()` accepts native SPU2 values from 0 through
`RFAUDS2_VOLUME_MAX` (`0x3fff`).

## Diagnostics

`rfauds2_get_stats()` returns:

- queue depth/capacity;
- underruns and overruns;
- effective latency and volume;
- started/paused state;
- min/max observed queue depth;
- render refill count;
- zero-filled frames;
- `missed_refills`.

A ring underrun means the render stage had fewer than 512 frames and zero-filled
the missing tail. A missed refill means the IOP refill thread did not publish a
real DMA half before the hardware deadline; the DMA interrupt used a dedicated
silence block instead of replaying stale PCM. Missed refills also count toward
`underruns` and `silent_frames`.

`rfauds2_reset_stats()` resets the diagnostic window without changing queued
PCM.

`rfauds2_get_stats_async()` launches a NOWAIT stats query in the same single
RPC slot used by async PCM. Collect with `rfauds2_get_stats_poll()`.

`rfauds2_get_cached_stats()` returns the last successful completed snapshot
without issuing another RPC. It is a snapshot, not live occupancy.

## Rate conversion

`rfauds2_rate_converter_init()` accepts non-zero source/output rates, mono or
stereo S16 and one of:

- `RFAUDS2_RESAMPLE_NEAREST`
- `RFAUDS2_RESAMPLE_LINEAR`
- `RFAUDS2_RESAMPLE_CUBIC`
- `RFAUDS2_RESAMPLE_SINC8`

The converter uses a Q32 phase accumulator. The caller must preserve the
unconsumed input tail reported through `input_frames_consumed` and prepend it
to the next source chunk.

Nearest/linear retain one source frame; cubic retains three; Sinc8 retains its
history window through the same tail-preservation contract.

Sinc8 is currently for upsampling only. It rejects downsampling with
`RFAUDS2_RATE_ERROR_SINC_DOWNSAMPLE` because correct sinc downsampling also
needs a ratio-dependent anti-alias low-pass.

## Mixing

`rfauds2_mix_s16()` mixes one S16 source into a destination with Q15 gain and
saturates to S16.

`rfauds2_bus_mixer` provides Game, Music, SFX and UI buses. Initialize with
`rfauds2_bus_mixer_init()`, clear a destination with
`rfauds2_bus_mixer_clear_s16()`, then mix sources with
`rfauds2_bus_mixer_mix_s16()`.

## Public error constants

Common stable errors are:

- `RFAUDS2_ERROR_INVALID_ARGUMENT`
- `RFAUDS2_ERROR_BUSY`
- `RFAUDS2_ERROR_PROTOCOL`

Some lower-level SIF/module-load failures can still surface as other negative
values. Treat any negative return as an error.
