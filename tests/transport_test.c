/* Production EE client + IOP RPC handler regression with mocked scheduling,
   SIF DMA and SPU2. Hardware register timing is tested on real PS2 separately. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <tamtypes.h>
#include <sifrpc.h>

#define RFAUDS2_IRX_IMPORTS_H
#define IRX_ID(name, major, minor)
#define TH_C 0
#define MODULE_NO_RESIDENT_END 1
#define MODULE_RESIDENT_END 0

typedef struct { int attr, option, initial, max; } iop_sema_t;
typedef struct {
    int attr, option;
    void (*thread)(void *);
    int stacksize, priority;
} iop_thread_t;

static int sema_count[32];
static int sema_max[32];
static int sema_alive[32];
static int sema_next;
static int fail_create_sema;
static int fail_create_thread;
static int fail_start_thread;
static int pending;
static int next_rpc_error;
static int pending_function;
static int pending_length;
static void *pending_send;
static void *pending_receive;
static int pending_receive_length;
static SifRpcEndFunc_t pending_end;
static void *pending_end_arg;
static unsigned int rpc_calls;
static unsigned int space_waits;
static int g_space_sema;
static int spu_start_result;
static unsigned int spu_missed_refills;
static unsigned int spu_pending_refills;
static int spu_initialized;
static int spu_running;

static int CreateSema(iop_sema_t *sema)
{
    int id;
    if (fail_create_sema) {
        --fail_create_sema;
        if (fail_create_sema == 0)
            return -77;
    }
    id = sema_next++;
    assert(id < 32);
    sema_count[id] = sema->initial;
    sema_max[id] = sema->max;
    sema_alive[id] = 1;
    return id;
}

static int DeleteSema(int id)
{
    if (id >= 0 && id < 32)
        sema_alive[id] = 0;
    return 0;
}

static int WaitSema(int id)
{
    assert(id >= 0 && id < 32 && sema_alive[id]);
    assert(sema_count[id] > 0);
    if (id == g_space_sema)
        ++space_waits;
    --sema_count[id];
    return 0;
}

static int SignalSema(int id)
{
    assert(id >= 0 && id < 32 && sema_alive[id]);
    if (sema_count[id] < sema_max[id])
        ++sema_count[id];
    return 0;
}

static int iSignalSema(int id) { return SignalSema(id); }
static int CreateThread(iop_thread_t *t)
{
    (void)t;
    if (fail_create_thread) {
        fail_create_thread = 0;
        return -88;
    }
    return 1;
}
static int DeleteThread(int id) { (void)id; return 0; }
static int TerminateThread(int id) { (void)id; return 0; }
static int StartThread(int id, void *arg)
{
    (void)id;
    (void)arg;
    if (fail_start_thread) {
        fail_start_thread = 0;
        return -89;
    }
    return 0;
}
static int GetThreadId(void) { return 1; }
static void FlushDcache(void) {}
static void CpuEnableIntr(void) {}
static void CpuSuspendIntr(int *state) { *state = 0; }
static void CpuResumeIntr(int state) { (void)state; }

#define _start test_module_start
#include "../src/iop/src/main.c"
#undef _start
#include "../src/ee/client.c"

int rfauds2_spu2_init(rfauds2_spu2_transfer_callback cb, void *arg)
{
    (void)cb;
    (void)arg;
    spu_initialized = 1;
    return 0;
}
void rfauds2_spu2_set_volume(unsigned int volume) { (void)volume; }
int rfauds2_spu2_start_loop(void *buffer, unsigned int size)
{
    (void)buffer;
    (void)size;
    if (spu_start_result)
        return spu_start_result;
    spu_running = 1;
    return 0;
}
int rfauds2_spu2_stop(void) { spu_running = 0; spu_pending_refills = 0; return 0; }
int rfauds2_spu2_shutdown(void)
{
    spu_running = 0;
    spu_initialized = 0;
    spu_pending_refills = 0;
    return 0;
}
int rfauds2_spu2_take_refill_block(void)
{
    if (spu_pending_refills & 1u) {
        spu_pending_refills &= ~1u;
        return 0;
    }
    if (spu_pending_refills & 2u) {
        spu_pending_refills &= ~2u;
        return 1;
    }
    return -1;
}
void rfauds2_spu2_mark_block_ready(unsigned int block) { (void)block; }
unsigned int rfauds2_spu2_take_missed_refills(void)
{
    unsigned int value = spu_missed_refills;
    spu_missed_refills = 0;
    return value;
}
unsigned int rfauds2_spu2_active_block(void) { return 0; }

void sceSifInitRpc(int mode) { (void)mode; }
int sceSifBindRpc(SifRpcClientData_t *client, int sid, int mode)
{
    (void)sid;
    (void)mode;
    client->server = &g_rpc_server;
    return 0;
}
int SifExecModuleBuffer(void *irx, u32 bytes, int argc, char **argv, int *result)
{
    (void)irx;
    (void)bytes;
    (void)argc;
    (void)argv;
    *result = 0;
    return 1;
}
void sceSifSetRpcQueue(SifRpcDataQueue_t *q, int thread)
{ (void)q; (void)thread; }
void sceSifRegisterRpc(SifRpcServerData_t *s, int sid, SifRpcFunc_t fn,
    void *buf, SifRpcFunc_t cfn, void *cbuf, SifRpcDataQueue_t *q)
{
    (void)s; (void)sid; (void)fn; (void)buf; (void)cfn; (void)cbuf; (void)q;
}
void sceSifRpcLoop(SifRpcDataQueue_t *q) { (void)q; }
int sceSifCheckStatRpc(SifRpcClientData_t *client)
{ (void)client; return pending; }

int sceSifCallRpc(SifRpcClientData_t *client, int function, int mode,
    void *send, int length, void *receive, int receive_length,
    SifRpcEndFunc_t end, void *end_arg)
{
    void *reply;
    (void)client;
    assert(!pending);
    ++rpc_calls;

    if (next_rpc_error) {
        int error = next_rpc_error;
        next_rpc_error = 0;
        return error;
    }

    if (mode & SIF_RPC_M_NOWAIT) {
        assert(end != 0);
        assert(((uintptr_t)receive & 63u) == 0);
        if (send != 0)
            assert(((uintptr_t)send & 63u) == 0);
        assert(function == RFAUDS2_RPC_TRY_SUBMIT ||
            function == RFAUDS2_RPC_STATS);
        pending = 1;
        pending_function = function;
        pending_send = send;
        pending_length = length;
        pending_receive = receive;
        pending_receive_length = receive_length;
        pending_end = end;
        pending_end_arg = end_arg;
        return 0;
    }

    reply = rpc_handler(function, send, length);
    memcpy(receive, reply, receive_length);
    return 0;
}

static void finish_rpc(void)
{
    assert(pending);
    memcpy(
        pending_receive,
        rpc_handler(pending_function, pending_send, pending_length),
        pending_receive_length);
    pending_end(pending_end_arg);
    pending = 0;
}

static void make_pcm(s16 *out, u32 frames, u32 base)
{
    u32 i;
    for (i = 0; i < frames; ++i) {
        out[2u * i] = (s16)((base + i) * 7919u);
        out[2u * i + 1u] = (s16)((base + i) * 3571u + 123u);
    }
}

static void check_block(u32 frames, u32 base)
{
    s16 expected[RFAUDS2_BLOCK_FRAMES * 2u];
    u32 i;

    make_pcm(expected, frames, base);
    fill_render_block();

    for (i = 0; i < frames; ++i) {
        assert(g_render_left[i] == expected[i * 2u]);
        assert(g_render_right[i] == expected[i * 2u + 1u]);
    }
    for (; i < RFAUDS2_BLOCK_FRAMES; ++i) {
        assert(g_render_left[i] == 0);
        assert(g_render_right[i] == 0);
    }
}

static void test_protocol_and_latency(void)
{
    rfauds2_stats stats;

    assert(rfauds2_bind() == 0);
    assert(spu_initialized);
    assert(rfauds2_set_latency_ms(43) == 0);
    assert(rfauds2_get_stats(&stats) == 0);
    assert(stats.capacity_frames == 2048u);
    assert(stats.latency_ms == 43u);
}

static void test_async_and_busy_guards(void)
{
    s16 source[960u * 2u];
    u32 accepted = 9999;
    unsigned int calls;
    rfauds2_stats stats;

    make_pcm(source, 960, 0);
    assert(rfauds2_submit_s16_async(source, 0) < 0);
    assert(rfauds2_submit_s16_async(source, 961) < 0);
    assert(rfauds2_submit_s16_async(0, 800) < 0);
    assert(rfauds2_submit_s16_async(source, 960) == 0);
    memset(source, 0, sizeof(source));
    assert(rfauds2_submit_poll(&accepted) == 0);
    calls = rpc_calls;
    assert(rfauds2_start() == RFAUDS2_ERROR_BUSY);
    assert(rfauds2_flush() == RFAUDS2_ERROR_BUSY);
    assert(rfauds2_get_stats(&stats) == RFAUDS2_ERROR_BUSY);
    assert(rpc_calls == calls);
    finish_rpc();
    assert(rfauds2_submit_poll(&accepted) == 1);
    assert(accepted == 960);
    assert(g_queued_frames == 960);
}

static void test_start_rollback(void)
{
    s16 source[1200u * 2u];
    u32 before;

    assert(rfauds2_flush() == 0);
    assert(rfauds2_set_latency_ms(86) == 0);
    make_pcm(source, 1200, 100);
    assert(rfauds2_submit_s16(source, 1200) == 1200);
    before = g_queued_frames;
    spu_start_result = -55;
    assert(rfauds2_start() == -2);
    spu_start_result = 0;
    assert(g_queued_frames == before);
    assert(!g_started);
    assert(rfauds2_start() == 0);
    assert(spu_running);
    assert(g_queued_frames == before - 1024u);
    assert(rfauds2_stop() == 0);
}

static void test_missed_refill_stats(void)
{
    rfauds2_stats stats;
    u32 underruns;
    u32 silent;

    assert(rfauds2_get_stats(&stats) == 0);
    underruns = stats.underruns;
    silent = stats.silent_frames;
    spu_missed_refills = 2;
    assert(rfauds2_get_stats(&stats) == 0);
    assert(stats.missed_refills >= 2u);
    assert(stats.underruns == underruns + 2u);
    assert(stats.silent_frames == silent + 1024u);
}

static void test_stream_and_partial_admission(void)
{
    const u32 total = 32768u;
    s16 source[960u * 2u];
    u32 sent = 0;
    u32 consumed = 0;
    u32 accepted;
    u32 step = 0;
    unsigned int partial = 0;
    unsigned int zero = 0;

    assert(rfauds2_flush() == 0);
    assert(rfauds2_set_latency_ms(43) == 0);

    while (consumed < total) {
        if (sent < total) {
            u32 request = 1u + ((step * 811u + 17u) % 960u);
            if (request > total - sent)
                request = total - sent;
            make_pcm(source, request, sent);
            assert(rfauds2_submit_s16_async(source, request) == 0);
            finish_rpc();
            assert(rfauds2_submit_poll(&accepted) == 1);
            assert(accepted <= request);
            if (accepted == 0)
                ++zero;
            else if (accepted != request)
                ++partial;
            sent += accepted;
        }

        if (g_queued_frames && (step % 3u == 2u || sent == total)) {
            u32 take = g_queued_frames;
            if (take > RFAUDS2_BLOCK_FRAMES)
                take = RFAUDS2_BLOCK_FRAMES;
            check_block(take, consumed);
            consumed += take;
        }

        assert(g_queued_frames <= g_queue_limit_frames);
        assert(++step < 5000u);
    }

    assert(sent == total);
    assert(partial > 0);
    assert(zero > 0);
}

static void test_latency_shrink_guard(void)
{
    s16 source[1800u * 2u];

    assert(rfauds2_flush() == 0);
    assert(rfauds2_set_latency_ms(86) == 0);
    make_pcm(source, 1800, 0);
    assert(rfauds2_submit_s16(source, 1800) == 1800);
    assert(rfauds2_set_latency_ms(11) == RFAUDS2_ERROR_BUSY);
    assert(g_queue_limit_frames == 4096u);
    assert(rfauds2_flush() == 0);
    assert(rfauds2_set_latency_ms(11) == 0);
    assert(g_queue_limit_frames == 512u);
}

static void test_wire_validation(void)
{
    rfauds2_rpc_submit packet;
    rfauds2_rpc_reply *reply;

    packet.frames = 961;
    reply = rpc_handler(RFAUDS2_RPC_TRY_SUBMIT, &packet, sizeof(packet));
    assert(reply->result == -11);
    packet.frames = 512;
    reply = rpc_handler(RFAUDS2_RPC_TRY_SUBMIT, &packet, 3);
    assert(reply->result == -10);
    reply = rpc_handler(RFAUDS2_RPC_TRY_SUBMIT, &packet, 4);
    assert(reply->result == -11);
}

static void test_async_stats(void)
{
    rfauds2_stats before;
    rfauds2_stats after;

    assert(rfauds2_get_stats(&before) == 0);
    assert(rfauds2_get_stats_async() == 0);
    assert(rfauds2_get_stats_poll(&after) == 0);
    finish_rpc();
    assert(rfauds2_get_stats_poll(&after) == 1);
    assert(rfauds2_get_cached_stats(&before) == 0);
    assert(!memcmp(&before, &after, sizeof(before)));
}

static void test_shutdown_and_rebind(void)
{
    assert(rfauds2_shutdown() == 0);
    assert(!spu_initialized);
    assert(g_ring_mutex < 0);
    assert(g_space_sema < 0);
    assert(g_transfer_sema < 0);
    assert(rfauds2_start() < 0);
    assert(rfauds2_bind() == 0);
    assert(spu_initialized);
}

static void test_init_cleanup_failures(void)
{
    rfauds2_rpc_reply *reply;

    assert(rfauds2_shutdown() == 0);

    fail_create_sema = 2;
    reply = rpc_handler(RFAUDS2_RPC_INIT, 0, 0);
    assert(reply->result < 0);
    assert(g_ring_mutex < 0 && g_space_sema < 0 && g_transfer_sema < 0);

    fail_create_thread = 1;
    reply = rpc_handler(RFAUDS2_RPC_INIT, 0, 0);
    assert(reply->result == -3);
    assert(!spu_initialized);
    assert(g_ring_mutex < 0 && g_space_sema < 0 && g_transfer_sema < 0);

    fail_start_thread = 1;
    reply = rpc_handler(RFAUDS2_RPC_INIT, 0, 0);
    assert(reply->result == -4);
    assert(!spu_initialized);

    assert(rfauds2_bind() == 0);
}

int main(void)
{
    assert(sizeof(g_receive) == 64);
    assert(sizeof(rfauds2_rpc_reply) <= 64);
    assert(rfauds2_submit_s16_async((const s16 *)&g_submit, 1) < 0);

    test_protocol_and_latency();
    test_async_and_busy_guards();
    test_start_rollback();
    test_missed_refill_stats();
    test_stream_and_partial_admission();
    test_latency_shrink_guard();
    test_wire_validation();
    test_async_stats();
    test_shutdown_and_rebind();
    test_init_cleanup_failures();

    assert(space_waits == 0);
    assert(rfauds2_shutdown() == 0);
    puts("RFAuds2 transport/lifecycle regressions: OK");
    return 0;
}
