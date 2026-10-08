# Roadmap

## Backend / transport

- [x] private SIFRPC protocol
- [x] protocol-version handshake before hardware initialization
- [x] blocking producer path with lossless backpressure
- [x] bounded async PCM RPC with explicit accepted-prefix accounting
- [x] async stats query and cached completed telemetry
- [x] explicit IOP queued-frame accounting
- [x] configurable queue capacity in 512-frame hardware blocks
- [x] pause / resume / stop / flush
- [x] direct SPU2 register setup and IOP DMA streaming
- [x] queue, underrun, overrun, refill and silent-frame diagnostics
- [x] missed-refill telemetry
- [x] cache-safe DMA-half publication after `FlushDcache()`
- [x] dedicated silence DMA fallback instead of stale-block replay
- [x] transactional start priming rollback on hardware-start failure
- [x] explicit shutdown/resource cleanup path
- [x] reject latency shrink below current queue occupancy
- [ ] long-duration emulator stress test
- [ ] FAT PS2 validation
- [ ] Slim PS2 validation
- [ ] tune production default latency from hardware measurements
- [ ] repeated backend/ROM transition stress on hardware
- [ ] analog/digital output validation where practical

## Conversion and mixing

- [x] arbitrary-rate Q32 converter
- [x] nearest converter
- [x] linear converter
- [x] four-point cubic converter
- [x] optional 256-phase / 8-tap Sinc8 upsampler
- [x] saturating Q15 S16 mixer helper
- [x] Game/Music/SFX/UI bus mixer
- [x] long-run chunked stream continuity regression
- [x] deterministic non-constant one-shot vs chunked equivalence regression
- [ ] ratio-dependent anti-alias low-pass for sinc downsampling

## Build / release quality

- [x] host regressions with warnings as errors
- [x] EE and IOP cross-build CI
- [x] generated EE dependency files (`-MMD -MP`)
- [x] pinned CI container digest
- [x] pinned PS2SDK source revision
- [x] package README / VERSION / LICENSE / docs / examples
- [x] repository `.gitignore`
- [x] runnable examples directory matching README layout
- [ ] first tagged standalone release
- [ ] release changelog and downloadable package artifact

## Integration

- [ ] SNESticleRevive bounded async adapter
- [ ] preserve/retry blocks larger than 960 frames in that adapter
- [ ] A/B against audsrv with identical producer/resampler path
- [ ] document real emulator/hardware integration results

## 1.0 gate

Do not call the backend production-ready until FAT + Slim hardware validation,
long-duration stress, repeated lifecycle transitions and default-latency tuning
have been completed with stable underrun/missed-refill diagnostics.
