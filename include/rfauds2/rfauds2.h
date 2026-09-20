#ifndef RFAUDS2_H
#define RFAUDS2_H

#include <tamtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RFAUDS2_OUTPUT_RATE 48000u
#define RFAUDS2_VOLUME_MAX  0x3FFFu

typedef enum {
    RFAUDS2_RESAMPLE_NEAREST = 0,
    RFAUDS2_RESAMPLE_LINEAR = 1,
    RFAUDS2_RESAMPLE_CUBIC = 2
} rfauds2_resample_mode;

typedef struct {
    u32 input_rate;
    u32 output_rate;
    u8 channels;
    rfauds2_resample_mode mode;
    u64 phase_q32;
    u64 step_q32;
} rfauds2_rate_converter;

typedef struct {
    u32 queued_frames;
    u32 capacity_frames;
    u32 underruns;
    u32 overruns;
    u32 latency_ms;
    u32 volume;
    u32 started;
    u32 paused;
    u32 min_queued_frames;
    u32 max_queued_frames;
    u32 refill_count;
    u32 silent_frames;
} rfauds2_stats;

/* Load an embedded rfauds2.irx, bind RPC and initialize the device. */
int rfauds2_init(const void *irx, u32 irx_size);

/* Bind to an already-loaded rfauds2.irx and initialize the device. */
int rfauds2_bind(void);

int rfauds2_submit_s16(const s16 *interleaved_stereo, u32 frames);
int rfauds2_start(void);
int rfauds2_pause(void);
int rfauds2_resume(void);
int rfauds2_stop(void);
int rfauds2_flush(void);
int rfauds2_set_volume(u32 volume);
int rfauds2_set_latency_ms(u32 latency_ms);
int rfauds2_get_stats(rfauds2_stats *stats);
int rfauds2_reset_stats(void);

int rfauds2_rate_converter_init(
    rfauds2_rate_converter *converter,
    u32 input_rate,
    u32 output_rate,
    u8 channels,
    rfauds2_resample_mode mode);

void rfauds2_rate_converter_reset(rfauds2_rate_converter *converter);

u32 rfauds2_rate_converter_process_s16(
    rfauds2_rate_converter *converter,
    const s16 *input,
    u32 input_frames,
    s16 *output,
    u32 output_frames_capacity,
    u32 *input_frames_consumed);

void rfauds2_mix_s16(
    s16 *destination,
    const s16 *source,
    u32 sample_count,
    s32 gain_q15);

#ifdef __cplusplus
}
#endif

#endif
