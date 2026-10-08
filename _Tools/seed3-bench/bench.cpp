// Seed3 load bench: the pedal's real core, built exactly like the firmware (Cortex-M7, -O2, hard float),
// run inside an emulator by seed3_load.py, which counts the instructions between the markers.
// Worst case: every stage on, pid steering every group, clarity with all 10 bands, a busy test signal.
#include <new>
#include <cmath>
#include "PgCore.h"

extern "C" {
void __attribute__((noinline)) mark_audio_start() { __asm volatile("nop"); }
void __attribute__((noinline)) mark_audio_end() { __asm volatile("nop"); }
void __attribute__((noinline)) mark_ui_start() { __asm volatile("nop"); }
void __attribute__((noinline)) mark_ui_end() { __asm volatile("nop"); }
void __attribute__((noinline)) mark_done() { for(;;) __asm volatile("nop"); }
}

struct NullCanvas : pg::Canvas // the screen link isn't timed here (on the pedal it's DMA-free SPI, measured separately)
{
    void Fill(int, int, int, int, uint16_t) override {}
    int  Text(int x, int, const char*, const FontDef&, uint16_t, uint16_t, int) override { return x; }
    void Blit(int, int, int, int, const uint16_t*, int) override {}
};

alignas(8) static unsigned char core_mem[sizeof(pg::Core)];
static uint16_t fb[320 * 240];
static float    in_l[48], in_r[48], out_l[48], out_r[48];

static void Fill(unsigned& rng, float& ph, uint32_t blk)
{
    for(int i = 0; i < 48; i++)
    {
        rng = rng * 1664525u + 1013904223u;
        const float nz = float(int(rng >> 8) - (1 << 23)) / float(1 << 23);
        ph += 2.f * pg::kPi * 55.f / 48000.f;
        if(ph > 2.f * pg::kPi)
            ph -= 2.f * pg::kPi;
        const float m = 0.4f * sinf(ph) * ((blk % 500) < 80 ? 1.f : 0.4f) + 0.15f * nz;
        in_l[i] = m, in_r[i] = 0.8f * m + 0.05f * nz;
    }
}

extern "C" void bench_main(int warm_blocks, int timed_blocks, int ui_frames, int mask)
{
    *(volatile uint32_t*)0xE000ED88 |= (0xFu << 20); // FPU on (as the startup code does)
    pg::Core* core = new(core_mem) pg::Core();
    core->Init(48000.f, fb);
    for(int t = 0; t < pg::kTabs; t++) // mask = which stages run (-1 = every stage: the worst case)
        core->SetStageOn(t, mask < 0 || ((mask >> t) & 1));
    core->SetParam(pg::P_PID_MASK, 63);
    core->SetParam(pg::P_CL_BANDS, 10), core->SetParam(pg::P_CLARITY, 60);
    const float* in[2]  = {in_l, in_r};
    float*       out[2] = {out_l, out_r};
    unsigned     rng    = 1;
    float        ph     = 0.f;
    uint32_t     blk    = 0;
    for(; blk < uint32_t(warm_blocks); blk++) // let every detector, band and controller settle
        Fill(rng, ph, blk), core->Process(in, out, 48, blk);
    mark_audio_start();
    for(int k = 0; k < timed_blocks; k++, blk++)
        Fill(rng, ph, blk), core->Process(in, out, 48, blk);
    mark_audio_end();
    if(ui_frames <= 0)
        mark_done();
    // a screen frame on the busiest page (the visualizer's spectrum: two 1024-point FFTs)
    NullCanvas c;
    core->DrawUi(c, blk); // the start-up logo begins ...
    core->KnobTurn(1, 1, blk + 16), core->KnobTurn(1, -1, blk + 16); // ... and a knob skips it
    for(int k = 2; k < 12; k++)
        core->DrawUi(c, blk + k * 16);
    core->KnobTurn(1, 1, blk + 200), core->KnobTurn(1, -1, blk + 200); // and the first-power-up tour
    for(int k = 13; k < 30; k++)
        core->DrawUi(c, blk + k * 16);
    core->KnobTurn(0, pg::T_VIS - pg::T_EQ, blk + 7000);
    core->KnobPress(0, true, blk + 7010), core->KnobPress(0, false, blk + 7060);
    for(int k = 0; k < 20; k++)
        core->DrawUi(c, blk + 7100 + k * 16);
    mark_ui_start();
    for(int k = 0; k < ui_frames; k++)
        core->DrawUi(c, blk + 9000 + k * 16);
    mark_ui_end();
    mark_done();
}

// what the emulator jumps to: a stack is set up by the runner, arguments in r0-r2
extern "C" void _start() { bench_main(300, 20, 4, -1); }
