# RFAuds2

[![CI](https://github.com/ReyFxck/RFAuds2/actions/workflows/ci.yml/badge.svg)](https://github.com/ReyFxck/RFAuds2/actions/workflows/ci.yml)

Modern low-latency PCM streaming for PlayStation 2.

RFAuds2 is a small audio backend and helper library for emulators, game
engines and other real-time PS2 software that need predictable continuous PCM
streaming without using `audsrv` at runtime. It originated as the EF2Audio
subsystem in [EF2SDK](https://github.com/ReyFxck/EF2SDK) and was split out so
the transport, resampling and diagnostics can evolve independently.

> **Status:** pre-1.0 / experimental. The code cross-builds in CI and the host
> regressions exercise the converter, mixer and EE/IOP transport contract.
> Real FAT/Slim PS2 validation and long-duration hardware stress testing are
> still required before the backend should be treated as production-ready.

Current source version: **0.1.0-alpha.2**.

## What RFAuds2 provides

- fixed **48 kHz / stereo / signed 16-bit PCM** at the SPU2 boundary;
- direct SPU2 + IOP DMA streaming with no runtime `audsrv` or `LIBSD`
  dependency;
- explicit ring-buffer occupancy instead of ambiguous read/write indices;
- blocking submission with producer backpressure;
- bounded asynchronous submission with accepted-prefix accounting;
- queue telemetry available synchronously, asynchronously or from a cached
  completed snapshot;
- nearest, linear, four-point cubic and fixed-point 8-tap sinc upsampling;
- saturating S16 mixing and reusable Game/Music/SFX/UI buses;
- pause, resume, stop, flush, volume and configurable queue latency;
- explicit underrun-to-silence behavior;
- stale-DMA protection: a missed refill deadline uses a dedicated silence
  block instead of replaying an old PCM block;
- observable queue depth, underruns, overruns, min/max occupancy, refill count,
  silent frames and missed refill deadlines;
- an explicit shutdown path that releases RFAuds2's playback thread,
  semaphores and DMA interrupt handler;
- an RPC protocol handshake so a new EE client does not silently run against
  an incompatible older IRX.

## Important hardware ownership rule

RFAuds2 is a **direct SPU2 backend**. While initialized, it expects exclusive
ownership of the SPU2 configuration and the IOP SPU2 DMA interrupt used by the
backend. Do not run `audsrv`, `LIBSD`/FreeSD streaming, or another SPU2 DMA
backend at the same time.

`rfauds2_shutdown()` stops RFAuds2 and releases its handler/resources, but it
cannot reconstruct a handler or register state that another audio stack had
before RFAuds2 initialized. If the program needs to switch to another sound
stack, shut RFAuds2 down first and then initialize that other stack again.

The RFAuds2 IRX/RPC server remains resident after shutdown. To use RFAuds2
again without reloading the IRX, call `rfauds2_bind()`.

## Architecture

```text
source/emulator audio
        |
        | native-rate PCM
        v
 optional EE converter
        |
        | 48 kHz stereo S16
        v
  EE SIF RPC client
        |
        v
     rfauds2.irx
        |
        v
  4096-frame IOP ring
        |
        v
 512-frame render/refill
        |
        v
  two 2048-byte DMA halves
        |
        +------ refill late? ------> dedicated zero block
        |
        v
   SPU2 core 1 / DMA
```

The IOP ring holds 4096 stereo frames. The hardware staging path works in
512-frame blocks. Each real DMA half has an explicit ready state. A half is
published to the interrupt handler only **after** its cache contents have been
written back. If neither real half is ready when the next DMA must start, the
interrupt schedules a dedicated zero block and records a missed refill. This
prevents stale PCM from being replayed.

More detail is in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Requirements and build

A current ps2dev toolchain plus a PS2SDK source tree are required:

```sh
export PS2DEV=/path/to/ps2dev
export PS2SDK=$PS2DEV/ps2sdk
export PS2SDKSRC=/path/to/ps2sdk-source
make
```

Outputs:

```text
build/librfauds2.a
build/rfauds2.irx
```

Useful targets:

```sh
make host-test   # deterministic host regressions
make check       # host/cross-build verification
make package     # headers, library, IRX, docs, examples, license/version
```

## Quick start

Embed `rfauds2.irx` in the EE executable (or load it yourself), then:

```c
#include <rfauds2/rfauds2.h>

rfauds2_stats stats;

if (rfauds2_init(rfauds2_irx, rfauds2_irx_size) < 0)
    return -1;

/* 43 ms maps to 2048 frames ~= 42.67 ms. */
rfauds2_set_latency_ms(43);
rfauds2_set_volume(RFAUDS2_VOLUME_MAX);

/* Prebuffer when practical. */
rfauds2_submit_s16(samples, 1536);
rfauds2_start();

rfauds2_submit_s16(more_samples, more_frames);
rfauds2_get_stats(&stats);

rfauds2_stop();
rfauds2_flush();
rfauds2_shutdown();
```

See [examples/basic_stream.c](examples/basic_stream.c).

### Asynchronous producer path

For an emulator/game loop that cannot block on queue space, use
`rfauds2_submit_s16_async()` plus `rfauds2_submit_poll()`.

The asynchronous call copies at most 960 frames into library-owned aligned
storage and returns immediately. Completion reports the **accepted prefix**.
The caller must retain and retry any unaccepted tail. Only one device RPC can
be outstanding at a time.

```c
u32 accepted;

if (rfauds2_submit_s16_async(samples, frames) == 0) {
    /* Do normal emulator/game work here. */
    if (rfauds2_submit_poll(&accepted) == 1) {
        /* Retry frames [accepted, frames) later. */
    }
}
```

Do not spin waiting for a full queue to drain. Poll at a normal scheduling
point and keep producer timing independent of SPU2 output timing. See
[examples/async_pump.c](examples/async_pump.c).

## Queue latency

`rfauds2_set_latency_ms()` converts the requested time to 48 kHz frames,
rounds to the **nearest 512-frame hardware block**, and clamps to the physical
4096-frame ring.

| Request | Capacity | Effective time |
| ---: | ---: | ---: |
| 1 ms | 512 frames | about 10.67 ms |
| 21 ms | 1024 frames | about 21.33 ms |
| 43 ms | 2048 frames | about 42.67 ms |
| 64 ms | 3072 frames | 64 ms |
| 86 ms | 4096 frames | about 85.33 ms |

The integer `latency_ms` statistic is rounded to the nearest millisecond, so
2048 frames is reported as 43 ms.

Shrinking capacity below PCM already queued returns `RFAUDS2_ERROR_BUSY`.
Drain or flush first instead of exposing `queued_frames > capacity_frames`.

For emulator experiments, 43 ms with a three-block prebuffer is a reasonable
starting point. The production default still needs real FAT/Slim measurements.

## Underrun and refill behavior

RFAuds2 distinguishes two shortage cases:

1. **Ring underrun:** fewer than 512 queued PCM frames are available for a
   render block. The missing tail is zero-filled.
2. **Refill deadline miss:** the refill thread did not publish a DMA half
   before the next transfer deadline. The interrupt transfers a dedicated
   zero block rather than replaying stale PCM.

Both contribute to `underruns` and `silent_frames`. The second case also
increments `missed_refills`.

## Rate conversion

The EE helper accepts mono or stereo S16 and uses a Q32 phase accumulator.
Modes are nearest, linear, cubic and Sinc8. Sinc8 is currently an
**upsampling** kernel and rejects downsampling because correct downsampling
needs a ratio-dependent anti-alias low-pass.

The caller must preserve the unconsumed tail returned by
`input_frames_consumed` and prepend it to the next source chunk. Host tests
compare chunked conversion against one-shot conversion on deterministic
non-constant PCM so phase/order regressions are caught.

See [docs/API.md](docs/API.md).

## Bus mixing

`rfauds2_bus_mixer` exposes Game, Music, SFX and UI buses with independent Q15
gain and mute state. The helper is allocation-free and does not own producer
lifetimes.

## Diagnostics

`rfauds2_get_stats()` returns queue/capacity, underruns, overruns, latency,
volume/state, min/max occupancy, refill count, silent frames and
`missed_refills`.

For nonblocking telemetry, use `rfauds2_get_stats_async()` /
`rfauds2_get_stats_poll()`. `rfauds2_get_cached_stats()` returns the latest
completed successful snapshot without another RPC. Call `rfauds2_reset_stats()`
before a focused reproduction.

## Protocol and client/IRX matching

The EE client checks `RFAUDS2_PROTOCOL_VERSION` before it initializes the
backend. A mismatched/older IRX is rejected with `RFAUDS2_ERROR_PROTOCOL`.
Ship `librfauds2.a`, headers and `rfauds2.irx` from the **same build**.

## CI and reproducibility

CI runs host converter/mixer regressions, chunk-equivalence tests, the delayed
EE/IOP transport fixture, EE/IOP cross-builds and package creation. The CI
container is pinned by digest and PS2SDK source rules are pinned to a known
revision instead of following moving `latest`/HEAD dependencies.

The transport fixture mocks physical SIF/DMA/SPU2 timing; it is not a
substitute for real-console validation.

## Repository layout

```text
.github/workflows/   reproducible cross-build/test CI
docs/                API, architecture, emulator notes and roadmap
examples/            lifecycle and async submission examples
include/rfauds2/     public API and RPC protocol headers
src/ee/              EE client, resampling and mixing helpers
src/iop/             rfauds2.irx and direct SPU2/DMA backend
tests/               host audio and EE/IOP transport regressions
Makefile             build, test and packaging entry points
VERSION              current pre-release source version
```

## Validation status

Covered in CI/host tests: deterministic resampling, chunk continuity,
saturating/bus mixing, async buffer lifetime, prefix retry semantics, queue
ordering, lifecycle/shutdown, protocol handshake, latency quantization,
telemetry and cross-builds.

Still required before 1.0: long emulator stress runs, FAT and Slim validation,
hardware refill/latency measurements, repeated backend/ROM transitions,
analog/digital output checks, ratio-dependent sinc downsampling low-pass,
SNESticleRevive A/B integration and the first tagged standalone release.

See [docs/ROADMAP.md](docs/ROADMAP.md).

## Why not audsrv?

RFAuds2 is not intended to reproduce every `audsrv` feature. Its focus is
continuous PCM where explicit queue accounting, bounded async admission,
predictable failure behavior and diagnostics matter. If `audsrv` already fits
an application, there is no requirement to replace it.

## License

MIT. See [LICENSE](LICENSE).
