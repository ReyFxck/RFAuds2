#include <kernel.h>
#include <loadfile.h>
#include <sifrpc.h>
#include <string.h>

#include <rfauds2/rfauds2.h>
#include <rfauds2/rpc.h>

static SifRpcClientData_t g_client __attribute__((aligned(64)));
static rfauds2_rpc_submit g_submit __attribute__((aligned(64)));
static rfauds2_rpc_control g_control __attribute__((aligned(64)));
static rfauds2_rpc_reply g_reply __attribute__((aligned(64)));
static int g_bound;

static int rpc_simple(int function)
{
    int result;

    memset(&g_reply, 0, sizeof(g_reply));

    result = sceSifCallRpc(
        &g_client,
        function,
        0,
        0,
        0,
        &g_reply,
        sizeof(g_reply),
        0,
        0);

    if (result < 0)
        return result;

    return g_reply.result;
}

static int rpc_control(int function, u32 value)
{
    int result;

    g_control.value = value;
    memset(&g_reply, 0, sizeof(g_reply));

    result = sceSifCallRpc(
        &g_client,
        function,
        0,
        &g_control,
        sizeof(g_control),
        &g_reply,
        sizeof(g_reply),
        0,
        0);

    if (result < 0)
        return result;

    return g_reply.result;
}

int rfauds2_bind(void)
{
    int result;
    int tries;

    sceSifInitRpc(0);
    memset(&g_client, 0, sizeof(g_client));

    for (tries = 0; tries < 5000; ++tries) {
        result = sceSifBindRpc(
            &g_client,
            RFAUDS2_RPC_SID,
            0);

        if (result < 0)
            return -1000 + result;

        if (g_client.server != 0)
            break;

        DelayThread(1000);
    }

    if (g_client.server == 0)
        return -2000;

    g_bound = 1;

    result = rpc_simple(RFAUDS2_RPC_INIT);
    if (result < 0) {
        g_bound = 0;
        return -3000 + result;
    }

    return 0;
}

int rfauds2_init(const void *irx, u32 irx_size)
{
    int module_id;
    int module_result = 1;

    if (irx == 0 || irx_size == 0)
        return -1;

    module_id = SifExecModuleBuffer(
        (void *)irx,
        irx_size,
        0,
        0,
        &module_result);

    if (module_id < 0)
        return -100 + module_id;

    if (module_result < 0 || module_result == 1)
        return -200 - module_result;

    return rfauds2_bind();
}

int rfauds2_submit_s16(const s16 *samples, u32 frames)
{
    u32 offset = 0;

    if (!g_bound || samples == 0)
        return -1;

    while (offset < frames) {
        u32 count = frames - offset;
        u32 sample_count;
        u32 i;
        u32 bytes;
        int result;

        if (count > RFAUDS2_RPC_MAX_FRAMES)
            count = RFAUDS2_RPC_MAX_FRAMES;

        g_submit.frames = count;
        sample_count = count * 2u;

        for (i = 0; i < sample_count; ++i)
            g_submit.samples[i] = samples[offset * 2u + i];

        bytes = sizeof(u32) + sample_count * sizeof(s16);
        memset(&g_reply, 0, sizeof(g_reply));

        result = sceSifCallRpc(
            &g_client,
            RFAUDS2_RPC_SUBMIT,
            0,
            &g_submit,
            bytes,
            &g_reply,
            sizeof(g_reply),
            0,
            0);

        if (result < 0)
            return result;

        if (g_reply.result < 0)
            return g_reply.result;

        offset += count;
    }

    return (int)frames;
}

int rfauds2_start(void)
{
    if (!g_bound)
        return -1;
    return rpc_simple(RFAUDS2_RPC_START);
}

int rfauds2_pause(void)
{
    if (!g_bound)
        return -1;
    return rpc_simple(RFAUDS2_RPC_PAUSE);
}

int rfauds2_resume(void)
{
    if (!g_bound)
        return -1;
    return rpc_simple(RFAUDS2_RPC_RESUME);
}

int rfauds2_stop(void)
{
    if (!g_bound)
        return -1;
    return rpc_simple(RFAUDS2_RPC_STOP);
}

int rfauds2_flush(void)
{
    if (!g_bound)
        return -1;
    return rpc_simple(RFAUDS2_RPC_FLUSH);
}

int rfauds2_set_volume(u32 volume)
{
    if (!g_bound || volume > RFAUDS2_VOLUME_MAX)
        return -1;

    return rpc_control(RFAUDS2_RPC_SET_VOLUME, volume);
}

int rfauds2_set_latency_ms(u32 latency_ms)
{
    if (!g_bound || latency_ms == 0)
        return -1;

    return rpc_control(RFAUDS2_RPC_SET_LATENCY, latency_ms);
}

int rfauds2_get_stats(rfauds2_stats *stats)
{
    int result;

    if (!g_bound || stats == 0)
        return -1;

    result = rpc_simple(RFAUDS2_RPC_STATS);
    if (result < 0)
        return result;

    stats->queued_frames = g_reply.queued_frames;
    stats->capacity_frames = g_reply.capacity_frames;
    stats->underruns = g_reply.underruns;
    stats->overruns = g_reply.overruns;
    stats->latency_ms = g_reply.latency_ms;
    stats->volume = g_reply.volume;
    stats->started =
        (g_reply.flags & RFAUDS2_RPC_FLAG_STARTED) != 0;
    stats->paused =
        (g_reply.flags & RFAUDS2_RPC_FLAG_PAUSED) != 0;
    stats->min_queued_frames = g_reply.min_queued_frames;
    stats->max_queued_frames = g_reply.max_queued_frames;
    stats->refill_count = g_reply.refill_count;
    stats->silent_frames = g_reply.silent_frames;

    return 0;
}

int rfauds2_reset_stats(void)
{
    if (!g_bound)
        return -1;
    return rpc_simple(RFAUDS2_RPC_RESET_STATS);
}
