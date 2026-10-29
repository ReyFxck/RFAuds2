#include <string.h>
#include <rfauds2/rfauds2.h>

typedef struct {
    const s16 *samples;
    u32 frames;
    u32 offset;
    u32 in_flight;
} async_pcm_pump;

void async_pcm_pump_begin(async_pcm_pump *pump, const s16 *samples, u32 frames)
{
    pump->samples = samples;
    pump->frames = frames;
    pump->offset = 0;
    pump->in_flight = 0;
}

/* Call once from a normal scheduling point. Returns 1 when the entire block
   has been accepted, 0 while work remains, or a negative error. */
int async_pcm_pump_step(async_pcm_pump *pump)
{
    u32 accepted;
    int result;

    if (pump->offset >= pump->frames)
        return 1;

    if (pump->in_flight) {
        result = rfauds2_submit_poll(&accepted);
        if (result <= 0)
            return result;

        pump->offset += accepted;
        pump->in_flight = 0;
        return pump->offset >= pump->frames ? 1 : 0;
    }

    {
        u32 remaining = pump->frames - pump->offset;
        u32 request = remaining;

        if (request > RFAUDS2_ASYNC_MAX_FRAMES)
            request = RFAUDS2_ASYNC_MAX_FRAMES;

        result = rfauds2_submit_s16_async(
            pump->samples + pump->offset * 2u,
            request);
        if (result < 0)
            return result;

        pump->in_flight = 1;
    }

    return 0;
}
