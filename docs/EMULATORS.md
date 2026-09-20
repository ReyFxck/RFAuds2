# Emulator integration

RFAuds2 is designed for emulators and other real-time producers that generate
continuous PCM with timing owned by the emulated system rather than by the
PlayStation 2 output device.

## Recommended pipeline

```text
emulated audio core
        |
        | native-rate PCM
        v
rate conversion (if needed)
        |
        | 48 kHz stereo S16
        v
rfauds2_submit_s16()
        |
        v
IOP ring buffer
        |
        v
SPU2 DMA
```

Keep emulation timing and hardware transport separate. An emulator should not
change its emulated DSP/APU clock merely to match the SPU2 output rate.

For a 32 kHz source, such as the SNES S-DSP path used by SNESticleRevive,
RFAuds2 can either receive PCM that the emulator already converted to 48 kHz or
use its own converter. During backend A/B testing, keep the existing emulator
converter unchanged so the comparison isolates only the output transport.

## Queue policy

Game/emulator audio should use lossless submission. When the configured IOP
queue is full, RFAuds2 applies producer backpressure instead of discarding the
unsent tail.

An underrun is handled differently: the unavailable portion of the next
512-frame hardware block is filled with zeroes. RFAuds2 never intentionally
replays old PCM to hide a shortage.

A practical initial latency for emulator testing is about 43 ms, which rounds
to 2048 frames. The correct production default still needs real FAT/Slim PS2
measurements.

## Diagnostics

Use `rfauds2_get_stats()` during an audio reproduction and record at least:

- `queued_frames` / `capacity_frames`
- `min_queued_frames`
- `max_queued_frames`
- `underruns`
- `overruns`
- `refill_count`
- `silent_frames`

If an audible cut coincides with an increment in `underruns` or
`silent_frames`, the transport did not receive PCM quickly enough. If the
counters remain stable, investigate the producer, resampler, emulated audio
core, or another timing source instead of assuming the SPU2 transport failed.

Call `rfauds2_reset_stats()` immediately before a focused reproduction to make
the diagnostic window unambiguous.

## SNESticleRevive A/B

The experimental SNESticleRevive integration lives on
`test/audio-mesence-parity-v1`.

The intended comparison keeps these components identical:

```text
SNES S-DSP -> existing SNESticle 32 kHz -> 48 kHz converter -> backend
```

Only the backend changes:

```text
A: audsrv
B: RFAuds2
```

The RFAuds2 build uses the legacy `Aud_*` surface as a thin adapter, embeds
`rfauds2.irx` in the ELF, and does not require audsrv, freesd or LIBSD for the
game/menu PCM path.

The first runtime targets are the games that exposed the existing audio
problems, especially long continuous playback and transitions where short
cuts were previously audible. Runtime conclusions must be based on actual
emulator/console tests; successful compilation alone does not establish audio
correctness.

## Hardware validation checklist

Before a 1.0 release, validate at minimum:

- one FAT PS2;
- one Slim PS2;
- long continuous playback;
- repeated pause/resume/flush/ROM transitions;
- low and high queue-latency settings;
- sustained CPU-heavy emulator scenes;
- no increasing underrun/overrun count during stable playback;
- analog and digital audio output where practical.

NetherSX2 remains useful for development and regression checks, but it is not a
replacement for retail PS2 validation.
