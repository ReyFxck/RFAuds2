# Async transport candidate, 2026-10-02

Base: main `6787ef5380e43fca79b53419af66595f473b5118`.
Candidate branch: `fix/async-pcm-transport`.

The original EE PCM API calls SIFRPC in blocking mode. Its IOP submit handler
waits for consumer space; a full paused/stopped queue can therefore prevent
the same server thread from handling a later start/resume/flush request.

The candidate adds `rfauds2_submit_s16_async()` and `rfauds2_submit_poll()`.
One copied, aligned block (up to 960 frames) is sent with NOWAIT. The new
IOP TRY_SUBMIT command admits only the available prefix and returns its
length without a space wait. The producer owns and retries the tail.
Initialization, controls and the existing blocking API stay synchronous.

The staging/reply buffers stay reserved until poll collects the response.
Other device requests return busy during that interval. The reply occupies
an entire 64-byte EE cache line, avoiding unrelated CPU accesses to the DMA
receive line. SIFRPC keeps its normal cache maintenance and RPC_END reply
ordering; the completion callback does no audio work. No resampling,
emulated audio clock, SPU2 DMA format, queue size or sample gain changes.

Validation completed:

- Existing host resampler/bus-mixer suite.
- Production client and IOP handler connected by a mock that delays RPC
  completion: 393216 streaming PCM frames across 512/2048/4096-frame queue
  limits, with partial/full admission and exact stereo ordering/zero-fill.
- Source reuse before reply, control/submit busy guards, full paused/stopped
  queues, resume/stop/flush, RPC errors, malformed wire lengths and the
  unchanged multi-chunk blocking API.
- AddressSanitizer and UndefinedBehaviorSanitizer for the transport fixture.
- EE library and IOP IRX cross-build with the installed ps2dev GCC 15.2
  toolchain and PS2SDK source rules `4996c6f`.
- Symbol inspection: receive storage is 64 bytes and independent of the
  pending/bound globals in the EE object.

The host fixture mocks kernel scheduling, SIF/DMA and SPU2. It verifies the
transport contract, not physical DMA timing, latency or audible quality.
No real-console FPS/underrun improvement is claimed.

SNESticle still uses audsrv. Its adapter will need a bounded PCM queue for
blocks larger than 960 frames, exact accepted-tail accounting, poll/retry
during useful EE work, and lifecycle handling. Keep its current conversion
and emulated mixer unchanged for the first backend A/B. Spawn's intro has
not been reproduced because its ROM was not supplied.

This completes a bounded asynchronous PCM transport primitive, not the whole
audio-module roadmap or emulator migration. FAT/Slim tests, long playback,
hardware refill timing and lifecycle stress remain necessary before main
adoption.

The 2026-10-02 follow-up also adds asynchronous stats launch/poll and a
separate cached snapshot refreshed by completed successful replies. An
adapter can query occupancy during EE work without a synchronous stats RPC
on every frame. Delayed stats completion, submit/control exclusion, cached
snapshot lifetime, queue consumption and error recovery are covered by the
production client/handler fixture. The wire protocol is unchanged.
