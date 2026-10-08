# Architecture

RFAuds2 separates producer policy from the PlayStation 2 hardware backend.
The application owns source timing; the backend owns only transport to the
fixed 48 kHz stereo S16 SPU2 boundary.

## Data path

```text
application / emulator
        |
        | native-rate PCM
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
512-frame render blocks
        |
        v
real DMA half 0 / real DMA half 1
        |
        +---- missed refill deadline ---> dedicated silence DMA block
        |
        v
SPU2 core 1
```

## Ring-buffer rules

The ring tracks read index, write index and `queued_frames` independently.
`queued_frames` is authoritative; equal indices are never used to infer full
versus empty.

Blocking submission waits for space. Asynchronous `TRY_SUBMIT` admits only the
available prefix and returns immediately, leaving the caller responsible for
retrying the exact tail.

The configured queue capacity is quantized to 512-frame blocks. A request to
shrink below current occupancy is rejected instead of creating an impossible
`queued_frames > capacity_frames` state.

## Render and DMA staging

Each render operation produces exactly 512 stereo frames. If fewer source
frames are available, the remainder is explicitly zero-filled and counted as
an underrun.

The 4096-byte staging buffer contains two 2048-byte real DMA halves. Each half
has an explicit ready state. The refill thread writes a completed half, calls
`FlushDcache()`, then publishes that half as ready. The interrupt handler never
starts DMA from a half before publication.

This ordering closes the old cache race where interrupts were re-enabled before
cache writeback and DMA could observe stale memory.

## Missed refill deadlines

The DMA interrupt must keep the SPU2 stream moving even if the refill thread is
late. If the next real half is not ready at the deadline, the interrupt starts
DMA from a dedicated, cache-flushed silence block rather than replaying an old
half.

Each such event increments `missed_refills`; it also contributes one underrun
and 512 silent frames to the public stats. The completed real half is still
queued for refill, so normal double-buffer operation resumes when the thread
catches up.

## Start/stop lifecycle

`start()` primes both real halves from the ring before DMA begins. Priming is
transactional: if hardware start fails, queue indices and diagnostic counters
are restored.

`stop()` disables DMA and clears pending refill ownership before the public
started state is dropped. A semaphore token queued just before stop therefore
cannot consume additional PCM after playback has stopped.

`shutdown()` stops playback, terminates/deletes the playback thread, releases
its semaphores and releases RFAuds2's SPU2 DMA interrupt handler. The RPC server
remains resident so the backend can be initialized again with `bind()`.

## SPU2 ownership

RFAuds2 directly resets/configures SPU2 state and owns the SPU2 DMA interrupt
while initialized. This is intentionally exclusive: do not run another direct
SPU2/audsrv/LIBSD streaming backend concurrently.

Shutdown releases RFAuds2's handler but does not reconstruct another backend's
previous handler/register state. Initialize the other backend again after
RFAuds2 shutdown.

## RPC protocol

The EE client performs a protocol-version query before `INIT`. This rejects an
older/incompatible IRX before hardware initialization instead of discovering a
wire mismatch on a later opcode.

The async EE client owns one aligned submit buffer and one full cache-line reply
buffer until poll collects completion. Device RPCs are serialized around that
slot.

## Diagnostics

Stats separate producer starvation from IOP scheduling failure:

- underrun with `missed_refills == 0`: render queue ran short;
- increasing `missed_refills`: refill thread missed a DMA deadline;
- overrun: producer met configured queue backpressure;
- min/max occupancy: observed queue envelope;
- refill/silent counters: workload and silence inserted by the backend.
