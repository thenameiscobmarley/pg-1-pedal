// PG-1 health checks: the pedal watches its own input, output, processor, controls and screen, and when
// anything looks even a little off it says so (title chip + a pop-up line) and explains on the health
// page what it thinks is wrong and what to try.
//
// What software can't see (so these still need the one-time multimeter check): the OUT jack wiring after
// the converter, the 9 V supply level, and a wire touching the box.
#include "PgCore.h"
#include "PgUi.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace pg
{
using namespace ui;

enum Sev : uint8_t
{
    S_INFO,
    S_WARN,
    S_FAULT
};

struct CheckDef
{
    const char* name; // list row
    const char* what; // what it thinks is wrong (one line)
    const char* fix;  // what to try (wrapped)
    uint8_t     sev;
    float       raise_s; // how long it must look wrong (normal sensitivity; 0 = the test itself is timed)
};

static const CheckDef kDefs[Core::kChecks] = {
    {"input dc", "DC voltage is coming in on the input",
     "a source or a wire is sending DC. Check the IN jack wires (pins 16, 17) and use the ground loop isolator.", S_WARN, 2.f},
    {"input clip", "the source is louder than the input can take",
     "turn the source (SC3 volume) down until the 'in clip' chip stays off.", S_WARN, 0.3f},
    {"one side", "only one side of the input has sound",
     "fine for a mono (TS) cable or a guitar. Otherwise the IN jack's ring or tip wire may be loose.", S_INFO, 5.f},
    {"input phase", "left and right inputs cancel each other out",
     "one input wire or the source is phase-flipped: on a mono speaker the sound would vanish.", S_WARN, 5.f},
    {"no input", "no input signal for a minute",
     "check the IN cable, that the source is playing, and its volume.", S_INFO, 60.f},
    {"limiter", "the output limiter is working hard",
     "lower the source, or the gain on dyn eq / comp / clarity / saturate.", S_WARN, 10.f},
    {"ears guard", "it's been loud for a while: ears guard eased it down",
     "that's the safety tab doing its job. Turn the source down if it keeps happening.", S_INFO, 10.f},
    {"audio glitch", "the audio went wrong: muted and reset",
     "if it happens again, note what you were doing and tell me. Check the 9 V adapter and ground wires.", S_FAULT, 0.f},
    {"processor", "the processor is close to its limit",
     "switch off a stage you don't need (hold the middle footswitch on its tab).", S_WARN, 2.f},
    {"dropouts", "the sound stuttered (audio dropouts)",
     "the processor or the host is overloaded. In Carla, raise the buffer size.", S_WARN, 0.f},
    {"knob stuck", "a knob's push switch reads pressed all the time",
     "check that knob's 2 push-switch wires: they may touch each other or the box.", S_WARN, 0.f},
    {"knob jitter", "a knob is turning by itself",
     "its A or B wire is loose, or its C (middle) ground wire came off.", S_WARN, 1.f},
    {"switch stuck", "a footswitch reads pressed all the time",
     "check its 2 wires: they may be touching each other or the box.", S_WARN, 0.f},
    {"touch stuck", "the touchscreen reads a finger all the time",
     "check the 5 touch wires (pins 29-33) are pushed fully on.", S_WARN, 0.f},
    {"screen link", "the screen connection had errors and was restarted",
     "check the screen wires (CS, SCK, SDI, DC, RESET) are pushed fully on.", S_WARN, 0.f},
    {"ultrasonic in", "sound above hearing came in (removed)",
     "something upstream makes it: a cheap converter or electronic whine. It never reaches the output.", S_INFO, 0.6f},
    {"infrasonic in", "sound below hearing came in (removed)",
     "rumble under 20 Hz: a bumped cable, a turntable, or DC drift upstream. It never reaches the output.", S_INFO, 1.5f},
    {"ultrasonic out", "sound above hearing reached the output: muted",
     "the hearing-range filter failed. This should never happen: the output was muted and reset. Tell me.", S_FAULT, 0.f},
    {"infrasonic out", "sound below hearing reached the output: muted",
     "the hearing-range filter failed. This should never happen: the output was muted and reset. Tell me.", S_FAULT, 0.f},
};

static uint16_t SevCol(int sev) { return sev == 2 ? kPink : (sev == 1 ? kYellow : kLtBlue); }

int         Core::CheckSev(int i) { return kDefs[i].sev; }
const char* Core::CheckWhat(int i) { return kDefs[i].what; }

void Core::ReportLoad(float f)
{
    if(!(f >= 0.f && f < 100.f))
        return;
    load_ = f > load_ ? f : load_ * 0.999f; // peaks count, then it eases back over ~1 s
}

void Core::ReportDisplayFault() { disp_faults_++, last_disp_t_ = last_proc_; }

int Core::ActiveAlerts() const
{
    int n = 0;
    for(int i = 0; i < kChecks; i++)
        n += (hs_[i].on && kDefs[i].sev >= S_WARN) ? 1 : 0;
    return n;
}

// every 100 ms on the UI thread
void Core::UpdateHealth(uint32_t now)
{
    if(now - last_health_ < 100)
        return;
    const float dt = last_health_ ? float(now - last_health_) / 1000.f : 0.1f;
    last_health_   = now;
    const int   s  = params_[P_H_SENS].value; // 0 relaxed, 1 normal, 2 strict
    const float tm = s == 0 ? 2.f : (s == 1 ? 1.f : 0.5f);

    // knob jitter: direction flips per second, counted by KnobTurn
    if(now - last_jitter_ >= 1000)
    {
        last_jitter_ = now;
        for(int k = 0; k < kKnobs; k++)
            jitter_[k] = knob_flips_[k], knob_flips_[k] = 0;
    }
    if(safety_.glitches != seen_glitches_)
        seen_glitches_ = safety_.glitches, last_glitch_t_ = now;
    if(dropouts_ != drops_mark_)
        drops_window_ += int(dropouts_ - drops_mark_), drops_mark_ = dropouts_, drops_mark_t_ = now;
    if(now - drops_mark_t_ > 10000)
        drops_window_ = 0;

    const float pl = h_pow_[0], pr = h_pow_[1], loud = fmaxf(pl, pr);
    bool        bad[kChecks] = {};
    int         detail[kChecks];
    for(int i = 0; i < kChecks; i++)
        detail[i] = -1;
    static const float dc_thr[3] = {0.05f, 0.02f, 0.01f}, ph_thr[3] = {-0.7f, -0.5f, -0.3f};
    static const float lim_thr[3] = {-12.f, -6.f, -3.f}, cpu_thr[3] = {0.9f, 0.8f, 0.7f};
    static const int   jit_thr[3] = {20, 12, 8}, drop_thr[3] = {3, 2, 1};

    bad[C_DC] = fmaxf(fabsf(h_dc_[0]), fabsf(h_dc_[1])) > dc_thr[s];
    bad[C_CLIP] = clips_ != 0 && now - clip_t_ < 1500;
    if((pl > 1e-5f && pr < 1e-9f) || (pr > 1e-5f && pl < 1e-9f))
        bad[C_ONESIDE] = true, detail[C_ONESIDE] = pl > pr ? 1 : 0; // the silent side
    bad[C_PHASE] = pl > 1e-5f && pr > 1e-5f && h_x_ / sqrtf(pl * pr) < ph_thr[s];
    bad[C_NOINPUT] = loud < 1e-9f;
    bad[C_LIMITER] = safety_.limit_db < lim_thr[s];
    bad[C_EARS] = safety_.ear_db + safety_.blast_db < -3.f;
    bad[C_GLITCH] = seen_glitches_ > 0 && now - last_glitch_t_ < 60000;
    bad[C_CPU] = load_ > cpu_thr[s];
    bad[C_DROPOUT] = drops_window_ >= drop_thr[s];
    for(int k = 0; k < kKnobs; k++)
    {
        if(knob_down_[k] && now - press_t0_[k] > uint32_t(15000.f * tm))
            bad[C_KNOBSTUCK] = true, detail[C_KNOBSTUCK] = k;
        if(jitter_[k] > jit_thr[s])
            bad[C_KNOBJITTER] = true, detail[C_KNOBJITTER] = k;
    }
    for(int f = 0; f < 3; f++)
        if(fs_down_[f] && now - fs_t0_[f] > uint32_t(20000.f * tm))
            bad[C_FSSTUCK] = true, detail[C_FSSTUCK] = f;
    bad[C_TOUCH] = ts_.down && now - ts_.t0 > uint32_t(30000.f * tm);
    bad[C_SCREEN] = disp_faults_ != 0 && now - last_disp_t_ < 60000;
    static const float u_thr[3] = {-30.f, -38.f, -45.f}, i_thr[3] = {-22.f, -28.f, -34.f};
    bad[C_ULTRA_IN] = safety_.ultra_in_db > u_thr[s];
    bad[C_INFRA_IN] = safety_.infra_in_db > i_thr[s];
    if(safety_.range_trips != seen_trips_) // which side tripped, from the levels it saw just before
    {
        seen_trips_  = safety_.range_trips, last_trip_t_ = now;
        hs_[C_ULTRA_OUT].detail = safety_.ultra_out_db > -50.f ? 1 : 0;
    }
    const bool trip = seen_trips_ > 0 && now - last_trip_t_ < 60000;
    bad[C_ULTRA_OUT] = trip && hs_[C_ULTRA_OUT].detail != 0;
    bad[C_INFRA_OUT] = trip && hs_[C_ULTRA_OUT].detail == 0;

    for(int i = 0; i < kChecks; i++)
    {
        HealthState& h = hs_[i];
        if(bad[i])
        {
            h.bad += dt, h.good = 0.f;
            if(detail[i] >= 0)
                h.detail = detail[i];
            if(!h.on && h.bad >= kDefs[i].raise_s * tm)
            {
                h.on = true;
                h.seen++;
                if(kDefs[i].sev >= S_WARN && params_[P_H_POP].value)
                    alert_check_ = i, alert_t0_ = now;
            }
        }
        else
        {
            h.good += dt, h.bad = 0.f;
            if(h.on && h.good >= 3.f) // clear only after it has looked fine for 3 s
                h.on = false;
        }
    }
    const int n = ActiveAlerts();
    if(n != drawn_alerts_)
    {
        drawn_alerts_ = n;
        redraw_title_ = true;
        if(screen_ == HOME && focus_ / 8 == T_HEALTH / 8)
            DrawTab(T_HEALTH, now);
    }
}

// the home footer: help line + page dots, or a fresh alert
void Core::DrawFooter(uint32_t now)
{
    FillRect(0, 222, Canvas::kW, 18, kGrey);
    footer_alert_ = alert_check_ >= 0 && now - alert_t0_ < 5000;
    if(footer_alert_)
    {
        const CheckDef& d = kDefs[alert_check_];
        FillRect(4, 223, Canvas::kW - 8, 14, d.sev == S_FAULT ? kPink : kYellow);
        char buf[54];
        snprintf(buf, sizeof(buf), "! %s", d.what);
        TextFb(8, 226, buf, Font_6x8, kBlue);
    }
    else
    {
        const int page = focus_ / 8, pages = (kTabs + 7) / 8;
        char hint[64];
        snprintf(hint, sizeof(hint), "turn: move  push/tap: open  hold %s: on/off", ui::FootName(1));
        TextFb(8, 226, hint, Font_6x8, kBlue);
        for(int pg = 0; pg < pages; pg++) // page dots (tap them to switch)
        {
            const int x = Canvas::kW - 8 - (pages - pg) * 10;
            if(pg == page)
                FillRect(x, 226, 7, 7, kBlue);
            else
                FrameRect(x, 226, 7, 7, kBlue);
        }
    }
    Dirty(0, 222, Canvas::kW, 18);
}

static int Wrap(const char* s, int width, char out[][54], int max_lines)
{
    int n = 0;
    while(*s && n < max_lines)
    {
        int len = int(strlen(s)), cut = len;
        if(len > width)
        {
            cut = width;
            while(cut > 0 && s[cut] != ' ')
                cut--;
            if(cut == 0)
                cut = width;
        }
        memcpy(out[n], s, size_t(cut));
        out[n][cut] = 0;
        n++;
        s += cut;
        while(*s == ' ')
            s++;
    }
    return n;
}

void Core::GraphHealth(uint32_t now)
{
    (void)now;
    FillRect(kGx, kGy, kGw, kGh, kBlueDeep);
    const int sel = params_[P_H_ROW].value, hist = params_[P_H_VIEW].value;
    const int rows = 8, rh = 11, top = (sel / rows) * rows;
    for(int r = 0; r < rows && top + r < kChecks; r++)
    {
        const int          i = top + r, y = kGy + 2 + r * rh;
        const HealthState& h = hs_[i];
        if(i == sel)
            FillRect(kGx + 2, y - 1, kGw - 4, rh, kBlue), FrameRect(kGx + 2, y - 1, kGw - 4, rh, kLtBlue);
        const uint16_t dot = h.on ? SevCol(kDefs[i].sev) : kCyan;
        FillRect(kGx + 6, y + 1, 6, 6, dot);
        char name[32];
        if(h.detail >= 0 && (i == C_KNOBSTUCK || i == C_KNOBJITTER))
            snprintf(name, sizeof(name), "%s (pg-%d)", kDefs[i].name, h.detail + 1);
        else if(h.detail >= 0 && i == C_FSSTUCK)
            snprintf(name, sizeof(name), "%s (fs-%d)", kDefs[i].name, h.detail + 1);
        else if(h.detail >= 0 && i == C_ONESIDE)
            snprintf(name, sizeof(name), "%s (%s silent)", kDefs[i].name, h.detail ? "right" : "left");
        else
            snprintf(name, sizeof(name), "%s", kDefs[i].name);
        TextFb(kGx + 18, y, name, Font_6x8, h.on ? kWhite : kDimText);
        char st[16];
        static const char* const sevname[3] = {"note", "warning", "fault"};
        if(hist)
            snprintf(st, sizeof(st), "seen %d", h.seen);
        else
            snprintf(st, sizeof(st), "%s", h.on ? sevname[kDefs[i].sev] : "ok");
        TextFb(kGx + kGw - 8 - TextW(st, Font_6x8), y, st, Font_6x8, h.on ? SevCol(kDefs[i].sev) : kCyan);
    }
    // the selected check, in plain words
    const int ey = kGy + 2 + rows * rh + 3;
    FillRect(kGx, ey - 2, kGw, 1, kGrid);
    const CheckDef& d = kDefs[sel];
    char            line[54];
    snprintf(line, sizeof(line), "%s%s", hs_[sel].on ? "! " : "watching for: ", d.what);
    TextFb(kGx + 4, ey + 1, line, Font_6x8, hs_[sel].on ? SevCol(d.sev) : kDimText);
    char lines[3][54];
    const int n = Wrap(d.fix, 51, lines, 3);
    for(int k = 0; k < n; k++)
        TextFb(kGx + 4, ey + 12 + k * 10, lines[k], Font_6x8, hs_[sel].on ? kCream : kDimText);
}
} // namespace pg
