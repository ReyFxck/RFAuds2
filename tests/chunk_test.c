#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rfauds2/rfauds2.h>

#define INPUT_FRAMES 2048u
#define OUTPUT_FRAMES 4096u
#define PENDING_FRAMES 256u

static void fail(const char *name)
{
    fprintf(stderr, "FAIL: %s\n", name);
    exit(1);
}

static void fill_signal(s16 *samples)
{
    u32 state = 0x13579BDFu;
    u32 i;

    for (i = 0; i < INPUT_FRAMES; ++i) {
        state = state * 1664525u + 1013904223u;
        samples[i] = (s16)(state >> 16);
    }
}

static u32 run_one_shot(
    rfauds2_resample_mode mode,
    const s16 *input,
    s16 *output)
{
    rfauds2_rate_converter converter;
    u32 consumed = 0;
    u32 produced;

    if (rfauds2_rate_converter_init(
            &converter, 32000, 48000, 1, mode) != 0)
        fail("one-shot init");

    produced = rfauds2_rate_converter_process_s16(
        &converter,
        input,
        INPUT_FRAMES,
        output,
        OUTPUT_FRAMES,
        &consumed);

    if (produced == 0 || consumed == 0)
        fail("one-shot progress");

    return produced;
}

static u32 run_chunked(
    rfauds2_resample_mode mode,
    const s16 *input,
    s16 *output)
{
    rfauds2_rate_converter converter;
    s16 pending[PENDING_FRAMES];
    u32 pending_frames = 0;
    u32 input_offset = 0;
    u32 output_offset = 0;
    u32 iteration = 0;

    if (rfauds2_rate_converter_init(
            &converter, 32000, 48000, 1, mode) != 0)
        fail("chunked init");

    while (input_offset < INPUT_FRAMES) {
        u32 add = 1u + ((iteration * 37u + 11u) % 61u);
        u32 consumed = 0;
        u32 produced;

        if (add > INPUT_FRAMES - input_offset)
            add = INPUT_FRAMES - input_offset;
        if (pending_frames + add > PENDING_FRAMES)
            fail("pending capacity");

        memcpy(
            pending + pending_frames,
            input + input_offset,
            add * sizeof(pending[0]));
        pending_frames += add;
        input_offset += add;

        produced = rfauds2_rate_converter_process_s16(
            &converter,
            pending,
            pending_frames,
            output + output_offset,
            OUTPUT_FRAMES - output_offset,
            &consumed);

        if (consumed > pending_frames)
            fail("consumed range");
        if (output_offset + produced > OUTPUT_FRAMES)
            fail("output capacity");

        output_offset += produced;
        pending_frames -= consumed;
        memmove(
            pending,
            pending + consumed,
            pending_frames * sizeof(pending[0]));
        ++iteration;
    }

    for (;;) {
        u32 consumed = 0;
        u32 produced = rfauds2_rate_converter_process_s16(
            &converter,
            pending,
            pending_frames,
            output + output_offset,
            OUTPUT_FRAMES - output_offset,
            &consumed);

        output_offset += produced;
        pending_frames -= consumed;
        memmove(
            pending,
            pending + consumed,
            pending_frames * sizeof(pending[0]));

        if (produced == 0 && consumed == 0)
            break;
    }

    return output_offset;
}

static void compare_mode(const char *name, rfauds2_resample_mode mode)
{
    s16 input[INPUT_FRAMES];
    s16 one_shot[OUTPUT_FRAMES];
    s16 chunked[OUTPUT_FRAMES];
    u32 one_count;
    u32 chunk_count;

    fill_signal(input);
    memset(one_shot, 0, sizeof(one_shot));
    memset(chunked, 0, sizeof(chunked));

    one_count = run_one_shot(mode, input, one_shot);
    chunk_count = run_chunked(mode, input, chunked);

    if (one_count != chunk_count) {
        fprintf(stderr, "%s count mismatch: %u != %u\n",
            name, one_count, chunk_count);
        exit(1);
    }

    if (memcmp(one_shot, chunked, one_count * sizeof(one_shot[0])) != 0)
        fail(name);
}

int main(void)
{
    compare_mode("linear chunk continuity", RFAUDS2_RESAMPLE_LINEAR);
    compare_mode("cubic chunk continuity", RFAUDS2_RESAMPLE_CUBIC);
    compare_mode("sinc8 chunk continuity", RFAUDS2_RESAMPLE_SINC8);

    puts("RFAuds2 chunked resampler equivalence: OK");
    return 0;
}
