#include <assert.h>
#include <stdio.h>

#include "../src/iop/src/refill_state.h"

static void test_ping_pong_and_silence_recovery(void)
{
    rfauds2_refill_state state;

    rfauds2_refill_state_init(&state);
    rfauds2_refill_state_start(&state);
    assert(state.active_source == 0u);

    assert(rfauds2_refill_state_complete(&state) == 1);
    assert(state.active_source == 1u);
    assert(rfauds2_refill_state_take_pending(&state) == 0);
    rfauds2_refill_state_mark_ready(&state, 0);

    assert(rfauds2_refill_state_complete(&state) == 1);
    assert(state.active_source == 0u);
    assert(rfauds2_refill_state_take_pending(&state) == 1);

    /* Block 1 misses its deadline: use dedicated silence, never stale PCM. */
    assert(rfauds2_refill_state_complete(&state) == 1);
    assert(state.active_source == RFAUDS2_REFILL_SILENCE_BLOCK);
    assert(rfauds2_refill_state_take_pending(&state) == 0);
    assert(rfauds2_refill_state_take_pending(&state) == -1);

    /* Silence completions do not invent additional real refill requests. */
    assert(rfauds2_refill_state_complete(&state) == 0);
    assert(state.active_source == RFAUDS2_REFILL_SILENCE_BLOCK);
    assert(rfauds2_refill_state_take_pending(&state) == -1);

    rfauds2_refill_state_mark_ready(&state, 1);
    assert(rfauds2_refill_state_complete(&state) == 0);
    assert(state.active_source == 1u);
    assert(rfauds2_refill_state_take_missed(&state) == 2u);
    assert(rfauds2_refill_state_take_missed(&state) == 0u);
}

static void test_stop_restart_and_saturation(void)
{
    rfauds2_refill_state state;

    rfauds2_refill_state_init(&state);
    rfauds2_refill_state_start(&state);
    assert(rfauds2_refill_state_complete(&state) == 1);
    rfauds2_refill_state_stop(&state);
    assert(!state.running);
    assert(state.pending_mask == 0u);
    assert(state.ready_mask == 0u);
    assert(rfauds2_refill_state_complete(&state) == 0);

    rfauds2_refill_state_mark_ready(&state, 0);
    assert(state.ready_mask == 0u);

    state.missed_refills = ~0u;
    rfauds2_refill_state_start(&state);
    assert(state.missed_refills == 0u);
    state.ready_mask = 0;
    state.missed_refills = ~0u;
    assert(rfauds2_refill_state_complete(&state) == 1);
    assert(state.missed_refills == ~0u);
}

int main(void)
{
    test_ping_pong_and_silence_recovery();
    test_stop_restart_and_saturation();
    puts("RFAuds2 refill state machine: OK");
    return 0;
}
