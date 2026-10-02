/* Run the production EE client and IOP handler together. Only kernel/DMA
   scheduling is mocked; finish_rpc deliberately delays the IOP response. */
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

static int sema_count[16];
static int sema_max[16];
static int sema_next;
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

static int CreateSema(iop_sema_t *sema)
{
    int id = sema_next++;
    assert(id < 16);
    sema_count[id] = sema->initial;
    sema_max[id] = sema->max;
    return id;
}
static int WaitSema(int id)
{
    /* A zero semaphore here means the production handler would deadlock
       waiting for a consumer which the test has deliberately paused. */
    assert(sema_count[id] > 0);
    if (id == g_space_sema) ++space_waits;
    --sema_count[id];
    return 0;
}
static int SignalSema(int id)
{
    if (sema_count[id] < sema_max[id]) ++sema_count[id];
    return 0;
}
static int iSignalSema(int id) { return SignalSema(id); }
static int CreateThread(iop_thread_t *t) { (void)t; return 1; }
static int StartThread(int id, void *arg) { (void)id; (void)arg; return 0; }
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
{ (void)cb; (void)arg; return 0; }
void rfauds2_spu2_set_volume(unsigned int volume) { (void)volume; }
int rfauds2_spu2_start_loop(void *buffer, unsigned int size)
{ (void)buffer; (void)size; return 0; }
int rfauds2_spu2_stop(void) { return 0; }
unsigned int rfauds2_spu2_active_block(void) { return 0; }

void sceSifInitRpc(int mode) { (void)mode; }
int sceSifBindRpc(SifRpcClientData_t *client, int sid, int mode)
{ (void)sid; (void)mode; client->server = &g_rpc_server; return 0; }
int SifExecModuleBuffer(void *irx, u32 bytes, int argc, char **argv, int *result)
{ (void)irx; (void)bytes; (void)argc; (void)argv; *result = 0; return 1; }
void sceSifSetRpcQueue(SifRpcDataQueue_t *q, int thread)
{ (void)q; (void)thread; }
void sceSifRegisterRpc(SifRpcServerData_t *s, int sid, SifRpcFunc_t fn,
    void *buf, SifRpcFunc_t cfn, void *cbuf, SifRpcDataQueue_t *q)
{ (void)s; (void)sid; (void)fn; (void)buf; (void)cfn; (void)cbuf; (void)q; }
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
        assert(((uintptr_t)send & 63u) == 0);
        assert(((uintptr_t)receive & 63u) == 0);
        assert(function == RFAUDS2_RPC_TRY_SUBMIT);
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
    memcpy(pending_receive,
        rpc_handler(pending_function, pending_send, pending_length),
        pending_receive_length);
    pending_end(pending_end_arg);
    pending = 0;
}

static void make_pcm(s16 *out, u32 frames, u32 base)
{
    u32 i;
    for (i = 0; i < frames; ++i) {
        out[2u*i] = (s16)((base + i) * 7919u);
        out[2u*i+1u] = (s16)((base + i) * 3571u + 123u);
    }
}

static void check_block(u32 frames, u32 base)
{
    s16 expected[RFAUDS2_BLOCK_FRAMES * 2u];
    u32 i;
    make_pcm(expected, frames, base);
    fill_render_block();
    for (i = 0; i < frames; ++i) {
        assert(g_render_left[i] == expected[i*2u]);
        assert(g_render_right[i] == expected[i*2u+1u]);
    }
    for (; i < RFAUDS2_BLOCK_FRAMES; ++i) {
        assert(g_render_left[i] == 0);
        assert(g_render_right[i] == 0);
    }
}

static void test_delayed_reply(void)
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
    memset(source, 0, sizeof(source)); /* Producer may reuse the buffer. */
    assert(rfauds2_submit_poll(&accepted) == 0);
    assert(accepted == 9999);
    calls = rpc_calls;
    assert(rfauds2_submit_s16_async(source, 512) == RFAUDS2_ERROR_BUSY);
    assert(rfauds2_submit_s16(source, 512) == RFAUDS2_ERROR_BUSY);
    assert(rfauds2_bind() == RFAUDS2_ERROR_BUSY);
    assert(rfauds2_init(source, 16) == RFAUDS2_ERROR_BUSY);
    assert(rfauds2_pause() == RFAUDS2_ERROR_BUSY);
    assert(rfauds2_resume() == RFAUDS2_ERROR_BUSY);
    assert(rfauds2_stop() == RFAUDS2_ERROR_BUSY);
    assert(rfauds2_start() == RFAUDS2_ERROR_BUSY);
    assert(rfauds2_flush() == RFAUDS2_ERROR_BUSY);
    assert(rfauds2_set_volume(1) == RFAUDS2_ERROR_BUSY);
    assert(rfauds2_set_latency_ms(43) == RFAUDS2_ERROR_BUSY);
    assert(rfauds2_reset_stats() == RFAUDS2_ERROR_BUSY);
    assert(rfauds2_get_stats(&stats) == RFAUDS2_ERROR_BUSY);
    assert(rpc_calls == calls);
    finish_rpc();
    assert(rfauds2_start() == RFAUDS2_ERROR_BUSY); /* Result not collected. */
    assert(rfauds2_submit_poll(0) < 0);
    assert(rfauds2_submit_poll(&accepted) == 1 && accepted == 512);
    assert(g_queued_frames == 512);
    /* Full and stopped: admission completes with zero, no blocked RPC. */
    assert(rfauds2_submit_s16_async(source, 960) == 0);
    finish_rpc();
    assert(rfauds2_submit_poll(&accepted) == 1 && accepted == 0);
    assert(rfauds2_flush() == 0);
    assert(g_queued_frames == 0);

    /* Failure releases the borrowed DMA buffers; no stale result escapes. */
    next_rpc_error = -77;
    assert(rfauds2_submit_s16_async(source, 1) == -77);
    assert(rfauds2_submit_poll(&accepted) < 0);
    assert(rfauds2_submit_s16_async(source, 1) == 0);
    finish_rpc();
    g_reply.result = -100; /* An older IRX rejects the new opcode. */
    assert(rfauds2_submit_poll(&accepted) == -100);
    assert(rfauds2_submit_s16_async(source, 1) == 0);
    finish_rpc();
    g_reply.result = 2;
    assert(rfauds2_submit_poll(&accepted) == RFAUDS2_ERROR_PROTOCOL);
    assert(rfauds2_flush() == 0);
}

static void test_stream(u32 latency_ms)
{
    const u32 total = 131072;
    s16 source[960u * 2u];
    u32 sent = 0, consumed = 0, step = 0;
    u32 accepted;
    unsigned int zero_acceptances = 0;
    unsigned int partial_acceptances = 0;

    assert(rfauds2_flush() == 0);
    assert(rfauds2_set_latency_ms(latency_ms) == 0);
    assert(rfauds2_start() == 0);
    while (consumed < total) {
        u32 request;
        if (sent < total) {
            request = 1u + ((step * 811u + 17u) % 960u);
            if (request > total - sent) request = total - sent;
            make_pcm(source, request, sent);
            assert(rfauds2_submit_s16_async(source, request) == 0);
            if ((step & 7u) == 3u) assert(rfauds2_submit_poll(&accepted) == 0);
            finish_rpc();
            assert(rfauds2_submit_poll(&accepted) == 1);
            assert(accepted <= request);
            if (accepted == 0) ++zero_acceptances;
            if (accepted && accepted != request) ++partial_acceptances;
            sent += accepted; /* Preserve and retry the exact unaccepted tail. */
        }
        if (g_queued_frames && (step % 3u == 2u || sent == total)) {
            u32 take = g_queued_frames;
            if (take > 512u) take = 512u;
            check_block(take, consumed);
            consumed += take;
        }
        assert(g_queued_frames <= g_queue_limit_frames);
        assert(++step < 10000u);
    }
    assert(sent == total);
    assert(zero_acceptances > 0 && partial_acceptances > 0);
    assert(rfauds2_pause() == 0);
    make_pcm(source, 960, 0);
    do {
        assert(rfauds2_submit_s16_async(source, 960) == 0);
        finish_rpc();
        assert(rfauds2_submit_poll(&accepted) == 1);
    } while (accepted);
    assert(g_queued_frames == g_queue_limit_frames);
    /* fill_render_block must not consume the paused queue. */
    {
        u32 queued = g_queued_frames;
        fill_render_block();
        assert(g_queued_frames == queued);
    }
    assert(rfauds2_resume() == 0);
    assert(rfauds2_stop() == 0);
    assert(rfauds2_flush() == 0);
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
    assert(g_queued_frames == 0);
}

static void test_legacy_submit(void)
{
    s16 source[1800u * 2u];
    u32 consumed = 0;
    unsigned int calls = rpc_calls;
    assert(rfauds2_set_latency_ms(86) == 0);
    make_pcm(source, 1800, 0);
    assert(rfauds2_submit_s16(source, 1800) == 1800);
    assert(rpc_calls == calls + 3u); /* latency control plus two submits */
    while (consumed < 1800) {
        u32 take = 1800u - consumed;
        if (take > 512) take = 512;
        check_block(take, consumed);
        consumed += take;
    }
    assert(g_queued_frames == 0);
}

int main(void)
{
    assert(sizeof(g_receive) == 64);
    assert(rfauds2_submit_s16_async((const s16 *)&g_submit, 1) < 0);
    assert(rfauds2_bind() == 0);
    assert(rfauds2_set_latency_ms(1) == 0);
    test_delayed_reply();
    test_stream(1);
    test_stream(43);
    test_stream(86);
    test_wire_validation();
    test_legacy_submit();
    assert(space_waits == 0);
    puts("RFAuds2 async transport: 393216 PCM frames, partial/full queues, delayed RPC and controls OK");
    return 0;
}
