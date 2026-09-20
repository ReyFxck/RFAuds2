# Architecture

RFAuds2 separates source audio policy from the PlayStation 2 hardware backend.

## Data path

```text
application/core
    |
    | source-rate PCM
    v
optional EE rate converter
    |
    | 48 kHz stereo S16
    v
EE RPC client
    |
    v
rfauds2.irx
    |
    v
4096-frame IOP ring
    |
    v
512-frame refill staging
    |
    v
4096-byte double DMA buffer
    |
    v
SPU2 core 1
```

The hardware side always consumes 48 kHz stereo signed 16-bit PCM.

## Ring-buffer rules

The IOP ring tracks read frame, write frame and queued-frame count separately.
The queued count is authoritative; equal read/write indices are never used to
guess whether the ring is empty or full.

When the producer reaches the configured queue limit it waits for space. PCM is
not discarded. When the consumer has fewer than 512 frames available, the
remainder of that hardware block is explicitly zero-filled.

## DMA

RFAuds2 owns SPU2 block-DMA setup and the DMA interrupt. The 4096-byte DMA
buffer contains two 2048-byte hardware blocks. Each callback wakes the IOP
refill thread, which prepares the idle block.

## Runtime dependencies

The hardware path does not use audsrv or LIBSD. The IRX still uses normal IOP
kernel services for threads, semaphores, interrupt registration, cache
maintenance and SIFRPC.

## Diagnostics

Stats expose current/capacity queue depth, underruns, producer backpressure,
configured latency, min/max queue depth, refill count and frames replaced with
silence. This is intended to make emulator audio bugs measurable instead of
requiring diagnosis from sound alone.
