#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rfauds2/rfauds2.h>

static void fail(const char *name)
{
    fprintf(stderr, "FAIL: %s\n", name);
    exit(1);
}

static void check_int(const char *name, long long got, long long expected)
{
    if (got != expected) {
        fprintf(
            stderr,
            "FAIL: %s got=%lld expected=%lld\n",
            name,
            got,
            expected);
        exit(1);
    }
}

static void test_exact_copy(void)
{
    rfauds2_rate_converter c;
    s16 input[] = { 1, -1, 2, -2, 3, -3, 4, -4 };
    s16 output[8] = { 0 };
    u32 consumed = 0;
    u32 produced;

    check_int(
        "exact init",
        rfauds2_rate_converter_init(
            &c, 48000, 48000, 2, RFAUDS2_RESAMPLE_LINEAR),
        0);

    produced = rfauds2_rate_converter_process_s16(
        &c, input, 4, output, 4, &consumed);

    check_int("exact produced", produced, 4);
    check_int("exact consumed", consumed, 4);

    if (memcmp(input, output, sizeof(input)) != 0)
        fail("exact samples");
}

static void test_linear_midpoint(void)
{
    rfauds2_rate_converter c;
    s16 input[] = { 0, 1000, 2000 };
    s16 output[8] = { 0 };
    u32 consumed = 0;
    u32 produced;

    check_int(
        "linear init",
        rfauds2_rate_converter_init(
            &c, 24000, 48000, 1, RFAUDS2_RESAMPLE_LINEAR),
        0);

    produced = rfauds2_rate_converter_process_s16(
        &c, input, 3, output, 8, &consumed);

    if (produced < 4)
        fail("linear produced");

    check_int("linear sample 0", output[0], 0);
    check_int("linear sample 1", output[1], 500);
    check_int("linear sample 2", output[2], 1000);
    check_int("linear sample 3", output[3], 1500);
}

static void test_cubic_constant(void)
{
    rfauds2_rate_converter c;
    s16 input[12];
    s16 output[32];
    u32 consumed = 0;
    u32 produced;
    u32 i;

    for (i = 0; i < 12; ++i)
        input[i] = 1234;

    check_int(
        "cubic init",
        rfauds2_rate_converter_init(
            &c, 32000, 48000, 1, RFAUDS2_RESAMPLE_CUBIC),
        0);

    produced = rfauds2_rate_converter_process_s16(
        &c, input, 12, output, 32, &consumed);

    if (produced == 0)
        fail("cubic produced");

    if (consumed > 9)
        fail("cubic retained tail");

    for (i = 0; i < produced; ++i) {
        if (output[i] != 1234)
            fail("cubic constant");
    }
}

static void test_mixer_saturation(void)
{
    s16 dst[] = { 30000, -30000 };
    const s16 src[] = { 10000, -10000 };

    rfauds2_mix_s16(dst, src, 2, 32768);

    check_int("mix sat +", dst[0], 32767);
    check_int("mix sat -", dst[1], -32768);
}

static void test_buses(void)
{
    rfauds2_bus_mixer mixer;
    s16 dst[4];
    const s16 game[] = { 1000, -1000, 2000, -2000 };
    const s16 ui[] = { 200, 200, 200, 200 };

    rfauds2_bus_mixer_init(&mixer);
    rfauds2_bus_mixer_clear_s16(dst, 4);

    check_int(
        "game gain",
        rfauds2_bus_mixer_set_gain(
            &mixer, RFAUDS2_BUS_GAME, 16384),
        0);

    check_int(
        "game mix",
        rfauds2_bus_mixer_mix_s16(
            &mixer, RFAUDS2_BUS_GAME, dst, game, 4),
        0);

    check_int("bus game 0", dst[0], 500);
    check_int("bus game 1", dst[1], -500);

    check_int(
        "ui mute",
        rfauds2_bus_mixer_set_mute(
            &mixer, RFAUDS2_BUS_UI, 1),
        0);

    check_int(
        "ui muted mix",
        rfauds2_bus_mixer_mix_s16(
            &mixer, RFAUDS2_BUS_UI, dst, ui, 4),
        0);

    check_int("bus muted unchanged", dst[0], 500);
}

int main(void)
{
    test_exact_copy();
    test_linear_midpoint();
    test_cubic_constant();
    test_mixer_saturation();
    test_buses();

    puts("RFAuds2 host audio regressions: OK");
    return 0;
}
