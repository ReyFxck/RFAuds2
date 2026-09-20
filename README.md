# RFAuds2

Modern low-latency PCM streaming for PlayStation 2.

RFAuds2 is a small audio library aimed at emulators, game engines and other
real-time PS2 software that need predictable continuous PCM streaming without
using `audsrv`.

It started as the EF2Audio subsystem inside
[EF2SDK](https://github.com/ReyFxck/EF2SDK) and was split into its own project
so the backend can evolve and be reused independently.

## Design goals

- native producer rates are allowed; the PS2 output rate is a backend detail;
- nearest, linear and fixed-point four-point cubic resampling;
- reusable Game/Music/SFX/UI bus mixing helpers;
- fixed 48 kHz / stereo / signed 16-bit PCM at the SPU2 boundary;
- explicit ring-buffer accounting instead of ambiguous read/write state;
- backpressure instead of silently dropping the tail of a block;
- underrun produces silence, never stale PCM;
- real flush, pause, resume and stop operations;
- configurable queue latency;
- observable queue/underrun/overrun/refill statistics;
- direct SPU2 + IOP DMA streaming;
- no runtime dependency on `audsrv` or `LIBSD`.

## Current status

**Pre-1.0 / experimental.**

The original EF2Audio path has been audibly validated in NetherSX2 with a
continuous generated 32 kHz -> 48 kHz melody stream. The direct SPU2/DMA path,
ring buffering, pause/resume, stop, flush, volume and latency controls are
implemented.

Cross-build CI and deterministic host regressions cover the resampler and bus
mixer. Real FAT/Slim PS2 validation and long-duration emulator stress testing
are still required before calling the backend production-ready.

## Architecture

```text
producer (e.g. emulator @ 32 kHz)
              |
              v
       rate conversion
              |
              v
     48 kHz stereo S16
              |
              v
       EE SIF RPC client
              |
              v
        rfauds2.irx
              |
              v
       IOP PCM ring buffer
              |
              v
      double-buffer refill
              |
              v
        SPU2 DMA / SPU2
```

The IOP ring is 4096 stereo frames. Hardware refills are performed in 512-frame
blocks. Queue latency can be reduced at runtime in 512-frame increments.

## Why not audsrv?

RFAuds2 is not intended as a drop-in reimplementation of every `audsrv`
feature. It focuses on continuous PCM streams where the application needs
control over buffering and failure behavior.

In particular, RFAuds2 makes underrun, overrun, queue occupancy and flush
semantics part of the public contract.

## Repository layout

```text
include/rfauds2/     public and RPC headers
src/ee/              EE client + rate conversion/mixing helpers
src/iop/             rfauds2.irx + direct SPU2/DMA backend
docs/                architecture/API/roadmap
examples/            small usage examples
```

## Build

A current ps2dev toolchain and PS2SDK source tree are used for compiler,
headers and IOP build rules.

```sh
export PS2DEV=/path/to/ps2dev
export PS2SDK=$PS2DEV/ps2sdk
export PS2SDKSRC=/path/to/ps2sdk-source

make
```

The target outputs are:

```text
build/librfauds2.a
build/rfauds2.irx
```

RFAuds2 uses PS2SDK/ps2dev as a **build environment**. Its runtime audio path
does not call `audsrv` or `LIBSD`.

## Basic API

```c
#include <rfauds2/rfauds2.h>

rfauds2_init(rfauds2_irx, rfauds2_irx_size);
rfauds2_set_latency_ms(43);
rfauds2_set_volume(RFAUDS2_VOLUME_MAX);

/* submit interleaved 48 kHz stereo S16 */
rfauds2_submit_s16(samples, frames);
rfauds2_start();

rfauds2_get_stats(&stats);
```

Applications that generate another sample rate can use the included fixed-point
rate converter before submitting to the device backend.

## Origin and license

The initial implementation was developed as EF2Audio in EF2SDK by ReyFxck and
contributors, then separated into RFAuds2.

MIT licensed. See [LICENSE](LICENSE).
