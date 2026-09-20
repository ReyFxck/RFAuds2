# Roadmap

## Backend

- [x] private SIFRPC protocol
- [x] explicit IOP queued-frame accounting
- [x] lossless producer backpressure
- [x] underrun-to-silence policy
- [x] configurable queue latency
- [x] pause/resume/stop/flush
- [x] direct SPU2 register setup
- [x] direct IOP DMA streaming
- [x] queue/underrun/overrun diagnostics
- [x] min/max queue, refill and silent-frame telemetry
- [ ] long-duration emulator stress test
- [ ] FAT PS2 validation
- [ ] Slim PS2 validation
- [ ] tune default latency from hardware measurements

## Conversion and mixing

- [x] arbitrary-rate Q32 converter
- [x] nearest converter
- [x] linear converter
- [x] saturating Q15 S16 mixer helper
- [x] cubic converter
- [x] optional 256-phase / 8-tap Sinc8 upsampler
- [x] long-run chunked-stream continuity regression (32 kHz -> 48 kHz)
- [ ] ratio-dependent anti-alias low-pass for sinc downsampling
- [x] reusable multi-stream / Game-Music-SFX-UI bus mixer

## Integration

- [ ] SNESticleRevive backend A/B against audsrv
- [ ] document emulator integration results
- [ ] first tagged standalone release
