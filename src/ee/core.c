#include <rfauds2/rfauds2.h>

#define RFAUDS2_Q15_ONE 32768

static u64 divide_u64_u32(u64 numerator, u32 denominator)
{
    u64 quotient = 0;
    u64 remainder = 0;
    s32 bit;

    if (denominator == 0)
        return 0;

    for (bit = 63; bit >= 0; --bit) {
        remainder =
            (remainder << 1) |
            ((numerator >> (u32)bit) & 1u);

        if (remainder >= denominator) {
            remainder -= denominator;
            quotient |= (u64)1u << (u32)bit;
        }
    }

    return quotient;
}

static s16 saturate_s16(s64 value)
{
    if (value > 32767)
        return 32767;

    if (value < -32768)
        return -32768;

    return (s16)value;
}

int rfauds2_rate_converter_init(
    rfauds2_rate_converter *converter,
    u32 input_rate,
    u32 output_rate,
    u8 channels,
    rfauds2_resample_mode mode)
{
    u64 numerator;

    if (converter == 0)
        return -1;

    if (input_rate == 0 || output_rate == 0)
        return -2;

    if (channels != 1 && channels != 2)
        return -3;

    if (mode != RFAUDS2_RESAMPLE_NEAREST &&
        mode != RFAUDS2_RESAMPLE_LINEAR)
        return -4;

    converter->input_rate = input_rate;
    converter->output_rate = output_rate;
    converter->channels = channels;
    converter->mode = mode;
    converter->phase_q32 = 0;

    numerator = (u64)input_rate << 32;
    converter->step_q32 =
        divide_u64_u32(numerator, output_rate);

    if (converter->step_q32 == 0)
        return -5;

    return 0;
}

void rfauds2_rate_converter_reset(
    rfauds2_rate_converter *converter)
{
    if (converter != 0)
        converter->phase_q32 = 0;
}

u32 rfauds2_rate_converter_process_s16(
    rfauds2_rate_converter *converter,
    const s16 *input,
    u32 input_frames,
    s16 *output,
    u32 output_frames_capacity,
    u32 *input_frames_consumed)
{
    u32 produced = 0;
    u32 channels;
    u64 phase;

    if (input_frames_consumed != 0)
        *input_frames_consumed = 0;

    if (converter == 0 ||
        input == 0 ||
        output == 0 ||
        input_frames_consumed == 0 ||
        output_frames_capacity == 0 ||
        input_frames == 0)
        return 0;

    channels = converter->channels;

    if (converter->input_rate == converter->output_rate) {
        u32 frames = input_frames;
        u32 sample_count;
        u32 i;

        if (frames > output_frames_capacity)
            frames = output_frames_capacity;

        sample_count = frames * channels;

        for (i = 0; i < sample_count; ++i)
            output[i] = input[i];

        converter->phase_q32 = 0;
        *input_frames_consumed = frames;
        return frames;
    }

    if (input_frames < 2)
        return 0;

    phase = converter->phase_q32;

    while (produced < output_frames_capacity) {
        u32 input_index = (u32)(phase >> 32);
        u32 fraction = (u32)phase;
        u32 channel;

        if (input_index + 1u >= input_frames)
            break;

        for (channel = 0; channel < channels; ++channel) {
            s32 a = input[input_index * channels + channel];
            s32 b = input[(input_index + 1u) * channels + channel];
            s32 sample;

            if (converter->mode == RFAUDS2_RESAMPLE_NEAREST) {
                sample =
                    (fraction < 0x80000000u) ? a : b;
            } else {
                u32 fraction_q16 = fraction >> 16;
                s32 delta = b - a;
                s64 interpolated =
                    (s64)delta * (s64)fraction_q16;

                sample =
                    a + (s32)(interpolated >> 16);
            }

            output[produced * channels + channel] =
                (s16)sample;
        }

        ++produced;
        phase += converter->step_q32;
    }

    {
        u64 consumed64 = phase >> 32;
        u32 consumed;

        if (consumed64 >= input_frames)
            consumed = input_frames - 1u;
        else
            consumed = (u32)consumed64;

        phase -= (u64)consumed << 32;
        converter->phase_q32 = phase;
        *input_frames_consumed = consumed;
    }

    return produced;
}

void rfauds2_mix_s16(
    s16 *destination,
    const s16 *source,
    u32 sample_count,
    s32 gain_q15)
{
    u32 i;

    if (destination == 0 || source == 0)
        return;

    for (i = 0; i < sample_count; ++i) {
        s64 scaled =
            (s64)source[i] * (s64)gain_q15;
        s64 mixed;

        if (scaled >= 0)
            scaled =
                (scaled + RFAUDS2_Q15_ONE / 2) >> 15;
        else
            scaled =
                -(((-scaled) + RFAUDS2_Q15_ONE / 2) >> 15);

        mixed = (s64)destination[i] + scaled;
        destination[i] = saturate_s16(mixed);
    }
}
