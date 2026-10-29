# Emulator integration

RFAuds2 is intended for emulators and other real-time producers whose audio
clock belongs to the emulated system, not to the PS2 output device.

## Recommended pipeline

```text
emulated audio core
        |
        | native-rate PCM
        v
rate conversion (if required)
        |
        | 48 kHz stereo S16
        v
RFAuds2 submit/poll adapter
        |
        v
IOP queue -> render -> SPU2 DMA
```

Do not change an emulated DSP/APU clock merely to match 48 kHz output timing.
During an A/B backend comparison, keep the emulator's existing conversion and
mixing unchanged so only transport changes.

## Producer policy

The blocking API is useful for simple applications that can wait for queue
space. Emulators should normally use async submit/poll so a full audio queue
does not stall useful EE work.

An adapter must:

- preserve producer blocks larger than 960 frames;
- submit at most 960 frames per async RPC;
- retain the exact unaccepted tail;
- poll during normal emulator work instead of spinning;
- keep emulated timing independent of queue occupancy;
- drain/flush safely at ROM, pause and backend transitions.

## Startup and latency

A practical starting point is `rfauds2_set_latency_ms(43)`, which selects
2048 queue frames (~42.67 ms). When practical, prebuffer three 512-frame blocks
before `start()`. The production default still needs FAT/Slim measurements.

## Diagnostics

Record at least:

- `queued_frames` / `capacity_frames`;
- `min_queued_frames` / `max_queued_frames`;
- `underruns`;
- `overruns`;
- `refill_count`;
- `silent_frames`;
- `missed_refills`.

If `missed_refills` rises, the IOP refill thread missed a DMA deadline and the
backend substituted a dedicated silence block. If underruns rise while missed
refills do not, the render queue itself ran short. That distinction helps avoid
blaming SPU2 transport for producer/resampler timing problems.

Use `rfauds2_reset_stats()` immediately before a focused reproduction.

## Backend ownership and transitions

RFAuds2 requires exclusive direct-SPU2/DMA ownership while initialized. Do not
leave audsrv/LIBSD streaming active concurrently.

For a backend switch:

1. stop the producer;
2. collect any outstanding async RPC result;
3. `rfauds2_stop()`;
4. `rfauds2_flush()`;
5. `rfauds2_shutdown()`;
6. initialize the next audio stack.

If returning to RFAuds2 while its IRX is still resident, call `rfauds2_bind()`.

## SNESticleRevive A/B

The intended comparison keeps this side identical:

```text
SNES S-DSP -> existing 32 kHz -> 48 kHz conversion -> backend
```

Only the backend changes:

```text
A: audsrv
B: RFAuds2
```

Before claiming a runtime improvement, validate long continuous playback,
scene/ROM transitions, CPU-heavy sections and focused reproductions while
recording queue/underrun/missed-refill stats.

## Hardware validation checklist

Before 1.0, validate at minimum:

- one FAT PS2;
- one Slim PS2;
- long continuous playback;
- repeated pause/resume/flush/backend transitions;
- low/high queue latency;
- sustained CPU-heavy emulator scenes;
- stable queue diagnostics during clean playback;
- analog and digital output where practical.

Emulators are useful development targets, but successful compilation or host
transport tests do not establish real SPU2 timing correctness.
