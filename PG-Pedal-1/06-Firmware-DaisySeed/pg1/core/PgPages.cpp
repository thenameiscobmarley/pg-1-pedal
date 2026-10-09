// PG-1 pages: the graph / visualizer, the live strip and the 4 knob boxes of every tab.
// Layout of a page: title 0-19, graph 22-157, grey panel from 160 (strip 162-177, page dots 180, knob boxes 186-237).
#include "PgCore.h"
#include "PgUi.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace pg
{
using namespace ui;

// numbers without printf's float support (the Seed3 build leaves it out to save space)
static void F1(char* buf, int n, float v, bool sign)
{
    if(!(fabsf(v) < 1e6f))
        v = 0.f;
    const int t = int(lroundf(v * 10.f));
    snprintf(buf, size_t(n), "%s%d.%d", t < 0 ? "-" : (sign ? "+" : ""), abs(t) / 10, abs(t) % 10);
}
static int Di(float v) { return (fabsf(v) < 1e6f) ? int(lroundf(v)) : 0; }

// ------------------------------------------------------------------ mapping helpers
int Core::XF(float hz, float fmin, float dec) const
{
    const float v = float(kGw - 1) * log10f(fmaxf(hz, 1.f) / fmin) / dec;
    return kGx + int(Clampf(v, -2.f, float(kGw + 1)) + 0.5f);
}
float Core::FX(int x, float fmin, float dec) const { return fmin * powf(10.f, dec * float(x - kGx) / float(kGw - 1)); }
int   Core::YDb(float db, float top, float bottom) const
{
    if(!(fabsf(db) < 1e6f)) // NaN / inf can't become a coordinate
        db = bottom;
    const float v = (top - db) / (top - bottom) * float(kGh - 1);
    return kGy + int(Clampf(v, 0.f, float(kGh - 1)) + 0.5f);
}
static int YGain(float db) // eq curves: +-18 dB around the middle
{
    if(!(fabsf(db) < 1e6f))
        db = 0.f;
    const int y = kGy + kGh / 2 - int(db * float(kGh / 2 - 6) / 18.f);
    return y < kGy ? kGy : (y >= kGy + kGh ? kGy + kGh - 1 : y);
}

Core::Rect Core::ParamRect(int k) const { return {4 + k * 79 + box_dx_, 186, 76, 52}; }

void Core::RoundRect(int x, int y, int w, int h, uint16_t c, int r, int bg)
{
    const uint16_t kGrey = bg < 0 ? ui::kGrey : uint16_t(bg); // (the colour the corners are cut back to)
    FillRect(x, y, w, h, c);
    if(r < 2 || w < 2 * r || h < 2 * r)
        return;
    for(int i = 0; i < r; i++) // cut each corner along a small quarter circle, back to the panel colour
    {
        const int cut = r - int(sqrtf(float(r * r - (r - i) * (r - i))) + 0.5f);
        if(cut <= 0)
            continue;
        FillRect(x, y + i, cut, 1, kGrey), FillRect(x + w - cut, y + i, cut, 1, kGrey);
        FillRect(x, y + h - 1 - i, cut, 1, kGrey), FillRect(x + w - cut, y + h - 1 - i, cut, 1, kGrey);
    }
}

// the page dots between the live numbers and the boxes (only on tabs with a second page; tap them to flip)
void Core::DrawDots()
{
    FillRect(120, 178, 80, 7, kGrey);
    const int n = PageCount();
    if(n > 1)
        for(int i = 0; i < n; i++)
        {
            const bool cur = i == page_;
            RoundRect(160 - n * 9 + i * 18 + (cur ? 0 : 3), 179, cur ? 16 : 10, 4, cur ? kBlue : kDimText, 2);
        }
    Dirty(120, 178, 80, 7);
}

// one frame of the boxes sliding to the other page: the old page leaves, the new one comes in behind it
void Core::SlideFrame(uint32_t now)
{
    const float t = Clampf(float(now - slide_.t0) / 150.f, 0.f, 1.f), e = 1.f - (1.f - t) * (1.f - t) * (1.f - t); // ease out
    const int   shift = int(e * float(Canvas::kW));
    FillRect(0, 186, Canvas::kW, 52, kGrey);
    const int to = page_;
    page_ = slide_.from, box_dx_ = -slide_.dir * shift;
    for(int k = 0; k < kKnobs; k++)
        DrawParamBox(k, now);
    page_ = to, box_dx_ = slide_.dir * (Canvas::kW - shift);
    for(int k = 0; k < kKnobs; k++)
        DrawParamBox(k, now);
    box_dx_ = 0;
    Dirty(0, 186, Canvas::kW, 52);
    if(t >= 1.f)
        slide_.on = false, redraw_panel_ = true, DrawDots();
}

int Core::NodeAt(int x, int y) const
{
    int best = -1, bd = 12 * 12;
    for(int b = 0; b < kBands; b++)
    {
        const int dx = x - XF(FreqHz(b), 20.f, 3.f), dy = y - YGain(Pf(ParamIndex(b, B_GAIN)) * 0.1f), d = dx * dx + dy * dy;
        if(d < bd)
            bd = d, best = b;
    }
    return best;
}

void Core::Format(int p, char* buf, int n) const
{
    const Param& pr = params_[p];
    const int    v  = pr.value;
    switch(pr.fmt)
    {
        case F_STENTH: F1(buf, n, float(v) * 0.1f, true); break;
        case F_FREQ:
        {
            const float hz = 20.f * powf(2.f, float(v) / 12.f);
            if(hz >= 1000.f)
                snprintf(buf, size_t(n), "%d.%02dk", int(hz / 1000.f), int(fmodf(hz, 1000.f) / 10.f));
            else
                snprintf(buf, size_t(n), "%d", int(hz + 0.5f));
            break;
        }
        case F_Q:
        {
            const int q = int(lroundf(30.f * powf(2.f, float(v) / 8.f)));
            snprintf(buf, size_t(n), "%d.%02d", q / 100, q % 100);
            break;
        }
        case F_RATIO: snprintf(buf, size_t(n), "%d.%d", v / 10, v % 10); break;
        case F_ATTACK:
        {
            const float ms = 0.1f * powf(2.f, float(v) / 4.f);
            if(ms < 100.f)
                F1(buf, n, ms, false);
            else
                snprintf(buf, size_t(n), "%d", Di(ms));
            break;
        }
        case F_RELEASE:
            if(v == 0)
                snprintf(buf, size_t(n), "auto");
            else
                snprintf(buf, size_t(n), "%d", Di(20.f * powf(2.f, float(v - 1) / 6.f)));
            break;
        case F_MAINS: snprintf(buf, size_t(n), "%s", v == 0 ? "auto" : (v == 1 ? "50 hz" : (v == 2 ? "60 hz" : (hum_.prof.valid ? "learned" : "learn")))); break;
        case F_VMODE:
        {
            static const char* const m[7] = {"spectrum", "waterfall", "stereo", "levels", "scope", "bars", "history"};
            snprintf(buf, size_t(n), "%s", m[(v >= 0 && v < 7) ? v : 0]);
            break;
        }
        case F_VSRC: snprintf(buf, size_t(n), "%s", v == 0 ? "in" : (v == 1 ? "out" : "both")); break;
        case F_ROW: snprintf(buf, size_t(n), "%d/%d", v + 1, pr.max + 1); break;
        case F_SENS: snprintf(buf, size_t(n), "%s", v == 0 ? "relaxed" : (v == 1 ? "normal" : "strict")); break;
        case F_ONOFF: snprintf(buf, size_t(n), "%s", v ? "on" : "off"); break;
        case F_HVIEW: snprintf(buf, size_t(n), "%s", v ? "history" : "now"); break;
        case F_CLMODE: snprintf(buf, size_t(n), "%s", v == 0 ? "dynamic" : (v == 1 ? "add" : "normalise")); break;
        case F_HZOFF:
            if(v == 0)
                snprintf(buf, size_t(n), "off");
            else
                snprintf(buf, size_t(n), "%d", v);
            break;
        case F_SLOT: snprintf(buf, size_t(n), "%d%s", v + 1, slots_[v].used ? "" : " (empty)"); break;
        case F_SAVE: snprintf(buf, size_t(n), "push x2"); break;
        case F_THEME: snprintf(buf, size_t(n), "%s", v ? "bios" : "pearl"); break;
        case F_KNOBS: snprintf(buf, size_t(n), "%s", v ? "reverse" : "normal"); break;
        case F_HUND: snprintf(buf, size_t(n), "%d.%02d", v / 100, v % 100); break;
        case F_PIDGRP:
        {
            static const char* const g[PidStage::kG] = {"eq", "comp", "mband", "clarity", "deharsh", "width"};
            snprintf(buf, size_t(n), "%s", g[(v >= 0 && v < PidStage::kG) ? v : 0]);
            break;
        }
        default: snprintf(buf, size_t(n), "%d", v); break;
    }
}

// ------------------------------------------------------------------ spectrum (1024-point FFT)
void Core::Spectrum(const float* ring, float* out, float fmin, float dec, float fall)
{
    static PG_BIG_BSS float re[kRing], im[kRing]; // UI thread only
    const int    start = ring_pos_;
    for(int i = 0; i < kRing; i++)
    {
        const float hann = 0.5f - 0.5f * cosf(2.f * kPi * float(i) / float(kRing - 1));
        const float s    = ring[(start + i) & (kRing - 1)];
        re[i]            = (fabsf(s) < 8.f ? s : 0.f) * hann;
        im[i]            = 0.f;
    }
    for(int i = 1, j = 0; i < kRing; i++) // bit reversal
    {
        int bit = kRing >> 1;
        for(; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if(i < j)
        {
            const float t = re[i];
            re[i] = re[j], re[j] = t;
        }
    }
    for(int len = 2; len <= kRing; len <<= 1) // radix-2 butterflies
    {
        const float ang = -2.f * kPi / float(len), wr = cosf(ang), wi = sinf(ang);
        for(int i = 0; i < kRing; i += len)
        {
            float cr = 1.f, ci = 0.f;
            for(int k = 0; k < len / 2; k++)
            {
                const int   a = i + k, b = a + len / 2;
                const float tr = re[b] * cr - im[b] * ci, ti = re[b] * ci + im[b] * cr;
                re[b] = re[a] - tr, im[b] = im[a] - ti;
                re[a] += tr, im[a] += ti;
                const float nr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr, cr = nr;
            }
        }
    }
    for(int k = 0; k < kRing / 2; k++)
        re[k] = sqrtf(re[k] * re[k] + im[k] * im[k]) / 256.f; // full-scale sine = 1 (0 dB)
    const float bin_hz = sample_rate_ / float(kRing);
    for(int x = kGx; x < kGx + kGw; x++)
    {
        const float b0 = FX(x, fmin, dec) / bin_hz, b1 = FX(x + 1, fmin, dec) / bin_hz;
        float       m;
        if(b1 - b0 < 1.f) // low end: between two bins
        {
            const int   k = b0 < 1.f ? 1 : (b0 > kRing / 2 - 2 ? kRing / 2 - 2 : int(b0));
            const float f = Clampf(b0 - float(k), 0.f, 1.f);
            m             = re[k] + (re[k + 1] - re[k]) * f;
        }
        else // high end: the loudest bin under this pixel
        {
            m = 0.f;
            for(int k = int(b0); k <= int(b1) && k < kRing / 2; k++)
                m = re[k] > m ? re[k] : m;
        }
        const float db = 20.f * log10f(m + 1e-9f);
        float&      s  = out[x];
        s              = db > s ? db : s - fall; // fast up, slow fall
        if(!(s > -200.f))
            s = -200.f;
    }
}

void Core::SpecBars(const float* spec, float top, float bottom, uint16_t fill, uint16_t edge)
{
    for(int x = kGx; x < kGx + kGw; x++)
    {
        if(spec[x] <= bottom)
            continue;
        const int y0 = YDb(spec[x], top, bottom);
        FillRect(x, y0, 1, kGy + kGh - y0, fill);
        Px(x, y0, edge);
    }
}

// the eq as set and as it is right now (with the dynamics), per graph column
void Core::EqResponse(float* set, float* now)
{
    Svf a[kBands], b[kBands];
    for(int k = 0; k < kBands; k++)
    {
        const float f = fminf(FreqHz(k), sample_rate_ * 0.45f), g = Pf(ParamIndex(k, B_GAIN)) * 0.1f;
        a[k].Set(Svf::BELL, sample_rate_, f, QOf(k), g);
        b[k].Set(Svf::BELL, sample_rate_, f, QOf(k), g + eq_.dyn_db[k]);
    }
    for(int x = kGx; x < kGx + kGw; x++)
    {
        const float t = tanf(kPi * fminf(FX(x, 20.f, 3.f), sample_rate_ * 0.49f) / sample_rate_);
        float       s = 0.f, d = 0.f;
        for(int k = 0; k < kBands; k++)
            s += a[k].MagDbT(t), d += b[k].MagDbT(t);
        set[x - kGx] = s, now[x - kGx] = d;
    }
}

// frequency grid for a log view
void Core::FreqGrid(float fmin, float dec)
{
    static const float lines[] = {20, 30, 50, 100, 200, 300, 500, 1000, 2000, 3000, 5000, 10000, 20000};
    for(float f : lines)
        if(f > fmin * 1.05f && f < fmin * powf(10.f, dec) * 0.97f)
            FillRect(XF(f, fmin, dec), kGy, 1, kGh, kGrid);
}

// ------------------------------------------------------------------ page
void Core::DrawPage(uint32_t now)
{
    FillRect(0, kGy - 2, Canvas::kW, kGh + 4, kGrey);
    FillRect(0, kPanelY, Canvas::kW, Canvas::kH - kPanelY, kGrey);
    vis_drawn_ = -1;
    DrawGraph(now);
    DrawStrip(now);
    for(int k = 0; k < kKnobs; k++)
        DrawParamBox(k, now);
    Dirty(0, 0, Canvas::kW, Canvas::kH);
}

void Core::DrawGraph(uint32_t now)
{
    switch(tab_)
    {
        case T_HUM: GraphHum(now); break;
        case T_EQ: GraphEq(now); break;
        case T_COMP: GraphComp(now); break;
        case T_CLARITY: GraphClarity(now); break;
        case T_SAT: GraphSat(now); break;
        case T_DEHARSH: GraphDeHarsh(now); break;
        case T_SAFETY: GraphSafety(now); break;
        case T_INPUT: GraphInput(now); break;
        case T_HEALTH: GraphHealth(now); break;
        case T_TAKEBACK: GraphTakeback(now); break;
        case T_WIDTH: GraphWidth(now); break;
        case T_LOUD: GraphLoud(now); break;
        case T_CONFIG: GraphConfig(now); break;
        case T_MBAND: GraphMband(now); break;
        case T_PID: GraphPid(now); break;
        default: GraphVis(now); break;
    }
    pad_dirty_ = true; // (the d-pad sits on top of the graph)
    if(tab_ != T_SAFETY && tab_ != T_VIS && tab_ != T_HEALTH && tab_ != T_CONFIG && !StageOn(tab_))
    {
        char off[24];
        snprintf(off, sizeof(off), "off (hold %s)", ui::FootName(1));
        TextFb(kGx + kGw - TextW(off, Font_6x8) - 4, kGy + kGh - 10, off, Font_6x8, kYellow);
    }
    Dirty(kGx, kGy, kGw, kGh);
}

// ------------------------------------------------------------------ strip (live numbers)
void Core::DrawStrip(uint32_t now)
{
    FillRect(0, kPanelY, Canvas::kW, 24, kGrey);
    if(tab_ == T_EQ)
    {
        for(int b = 0; b < kBands; b++) // band chips
        {
            const Rect r = {4 + b * 79, 162, 76, 16};
            RoundRect(r.x, r.y, r.w, r.h, b == band_ ? kBlue : kGreyDark);
            FillRect(r.x + 4, r.y + 4, 8, 8, kBandCol[b]);
            char buf[16], f[10];
            Format(ParamIndex(b, B_FREQ), f, sizeof(f));
            snprintf(buf, sizeof(buf), "%d  %s", b + 1, f);
            TextFb(r.x + 16, r.y + 4, buf, Font_6x8, b == band_ ? kYellow : kGrey);
            if(b == band_)
                PearlBorder(r, now);
        }
        drawn_band_ = band_;
        Dirty(0, kPanelY, Canvas::kW, 24);
        DrawDots();
        return;
    }
    char cell[4][20] = {};
    char a[12], b[12];
    switch(tab_)
    {
        case T_HUM:
        {
            float deepest = 0.f;
            for(int h = 0; h < HumStage::kH; h++)
                deepest = fminf(deepest, hum_.depth_db[h]);
            if(params_[P_HUM_MAINS].value == 3 && hum_.prof.valid)
            {
                float cut = 0.f, fl = 0.f;
                for(int k = 0; k < NoiseProfile::kBands; k++)
                    cut += hum_.hb_cut[k] / float(NoiseProfile::kBands), fl += hum_.prof.floor_db[k] / float(NoiseProfile::kBands);
                snprintf(cell[0], 20, "%d tones", hum_.prof.n_tones);
                snprintf(cell[1], 20, "floor %d", Di(fl));
                F1(a, 12, cut, false), snprintf(cell[2], 20, "gate %s", a);
                snprintf(cell[3], 20, "margin %d", params_[P_LRN_MARGIN].value);
                break;
            }
            snprintf(cell[0], 20, "%d hz %s", Di(hum_.f0), params_[P_HUM_MAINS].value == 0 ? "auto" : "set");
            F1(a, 12, deepest, false), snprintf(cell[1], 20, "hum %s", a);
            F1(a, 12, hum_.hiss_db, false), snprintf(cell[2], 20, "hiss %s", a);
            snprintf(cell[3], 20, "top %d db", Di(hum_.hf_level_db));
            break;
        }
        case T_COMP:
            F1(a, 12, comp_.gr_db, false), snprintf(cell[0], 20, "gr %s", a);
            snprintf(cell[1], 20, "in %d db", Di(comp_.in_db));
            F1(a, 12, comp_.makeup, true), snprintf(cell[2], 20, "gain %s", a);
            snprintf(cell[3], 20, "rel %s", params_[P_C_RELEASE].value ? "set" : "auto");
            break;
        case T_CLARITY: // what each knob is doing right now (mud cut and top taming show on the graph)
            F1(a, 12, clar_.thump_db, true), snprintf(cell[0], 20, "thump %s", a);
            F1(a, 12, clar_.detail_view_db, true), snprintf(cell[1], 20, "detail %s", a);
            snprintf(cell[2], 20, "%d of %d bands", clar_.live_n, params_[P_CL_BANDS].value);
            F1(a, 12, clar_.warm_lo_db, true), snprintf(cell[3], 20, "warm %s", a);
            break;
        case T_SAT:
            snprintf(cell[0], 20, "in %d db", Di(20.f * log10f(sat_.drive_pk / sat_.gain + 1e-9f)));
            snprintf(cell[1], 20, "hits %d db", Di(20.f * log10f(sat_.drive_pk + 1e-9f)));
            snprintf(cell[2], 20, "top %dk", Di(sat_.tone_hz / 1000.f));
            snprintf(cell[3], 20, "mix %d", params_[P_MIX].value);
            break;
        case T_DEHARSH:
        {
            int   worst = 0;
            float cut   = 0.f;
            for(int i = 0; i < DeHarshStage::kD; i++)
                if(dh_.cut_db[i] < cut)
                    cut = dh_.cut_db[i], worst = i;
            F1(a, 12, cut, false), snprintf(cell[0], 20, "cut %s", a);
            F1(b, 12, kDeHarshF[worst] / 1000.f, false), snprintf(cell[1], 20, "at %sk", b);
            F1(a, 12, dh_.comfort_db, false), snprintf(cell[2], 20, "3k %s", a);
            snprintf(cell[3], 20, "loud %d", Di(PowToDb(dh_.p_full) + 3.f));
            break;
        }
        case T_SAFETY:
            F1(a, 12, safety_.limit_db, false), snprintf(cell[0], 20, "peak %s", a);
            F1(a, 12, safety_.ear_db + safety_.blast_db, false), snprintf(cell[1], 20, "ears %s", a);
            F1(a, 12, safety_.woof_db, false), snprintf(cell[2], 20, "woof %s", a);
            F1(a, 12, safety_.tweet_db, false), snprintf(cell[3], 20, "tweet %s", a);
            break;
        case T_TAKEBACK:
            F1(a, 12, tb_.punch_db, true), snprintf(cell[0], 20, "punch %s", a);
            F1(a, 12, tb_.detail_db, true), snprintf(cell[1], 20, "detail %s", a);
            F1(a, 12, tb_.air_db, true), snprintf(cell[2], 20, "air %s", a);
            F1(a, 12, tb_.space_db, true), snprintf(cell[3], 20, "space %s", a);
            break;
        case T_WIDTH:
            snprintf(cell[0], 20, "now %d%%", Di(width_.w_eff * 100.f));
            F1(a, 12, width_.corr, true), snprintf(cell[1], 20, "corr %s", a);
            if(params_[P_W_MONO].value)
                snprintf(cell[2], 20, "mono <%d", params_[P_W_MONO].value);
            else
                snprintf(cell[2], 20, "mono off");
            snprintf(cell[3], 20, "guard %s", params_[P_W_GUARD].value ? (width_.w_eff < width_.w - 0.05f ? "acting" : "on") : "off");
            break;
        case T_LOUD:
            F1(a, 12, loud_.lo_db, true), snprintf(cell[0], 20, "bass %s", a);
            F1(a, 12, loud_.hi_db, true), snprintf(cell[1], 20, "treble %s", a);
            snprintf(cell[2], 20, "at ~%d phon", Di(loud_.eff));
            snprintf(cell[3], 20, "follow %s", params_[P_LD_AUTO].value ? "on" : "off");
            break;
        case T_MBAND:
        {
            static const char* const n[4] = {"bass", "lmid", "hmid", "high"};
            for(int b = 0; b < 4; b++)
                F1(a, 12, mb_.gr[b], false), snprintf(cell[b], 20, "%s %s", n[b], a);
            break;
        }
        case T_PID:
            snprintf(cell[0], 20, "level %d", Di(pid_.level_db + 3.f));
            F1(a, 12, pid_.crest_db, false), snprintf(cell[1], 20, "crest %s", a);
            F1(a, 12, pid_.corr, true), snprintf(cell[2], 20, "corr %s", a);
            snprintf(cell[3], 20, "%s", !StageOn(T_PID) ? "off" : (pid_.tracking ? "steering" : "waiting"));
            break;
        case T_CONFIG:
        {
            int used = 0;
            for(int k = 0; k < kSlots; k++)
                used += slots_[k].used;
            snprintf(cell[0], 20, "%d of %d saved", used, kSlots);
            snprintf(cell[1], 20, "theme %s", params_[P_CF_THEME].value ? "bios" : "pearl");
            snprintf(cell[2], 20, "autosave on");
            snprintf(cell[3], 20, "knobs %s", params_[P_CF_KNOBS].value ? "rev" : "normal");
            break;
        }
        case T_HEALTH:
        {
            int c[3] = {};
            for(int k = 0; k < kChecks; k++)
                if(hs_[k].on)
                    c[CheckSev(k)]++;
            snprintf(cell[0], 20, "ok %d", kChecks - c[0] - c[1] - c[2]);
            snprintf(cell[1], 20, "notes %d", c[0]);
            snprintf(cell[2], 20, "warnings %d", c[1]);
            snprintf(cell[3], 20, "faults %d", c[2]);
            break;
        }
        case T_INPUT:
            snprintf(cell[0], 20, "in %d db", Di(lvl_.in_db));
            F1(a, 12, lvl_.gain_db, true), snprintf(cell[1], 20, "gain %s", a);
            snprintf(cell[2], 20, "now %d db", Di(lvl_.in_db + lvl_.gain_db));
            snprintf(cell[3], 20, "clips %d", int(clips_ > 999 ? 999 : clips_));
            break;
        default:
            snprintf(cell[0], 20, "in %d", Di(20.f * log10f(fmaxf(in_pk_[0], in_pk_[1]) + 1e-9f)));
            snprintf(cell[1], 20, "out %d", Di(20.f * log10f(fmaxf(out_pk_[0], out_pk_[1]) + 1e-9f)));
            F1(a, 12, corr_, true), snprintf(cell[2], 20, "corr %s", a);
            snprintf(cell[3], 20, "glitch %d", safety_.glitches);
            break;
    }
    for(int k = 0; k < 4; k++)
    {
        const Rect r = {4 + k * 79, 162, 76, 16};
        RoundRect(r.x, r.y, r.w, r.h, kGreyDark);
        TextFb(r.x + 5, r.y + 4, cell[k], Font_6x8, kCream);
    }
    Dirty(0, kPanelY, Canvas::kW, 24);
    DrawDots();
}

// ------------------------------------------------------------------ knob boxes
void Core::DrawParamBox(int k, uint32_t now)
{
    (void)now;
    const Rect r = ParamRect(k);
    const int  p = KnobParam(k);
    if(p < 0) // nothing on this knob on this page: a quiet empty slot
    {
        RoundRect(r.x, r.y, r.w, r.h, kGreyDark, 4);
        RoundRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, kGrey, 4);
        FillRect(r.x + 3, r.y + 2, 9, 10, kDimText);
        TextFb(r.x + 5, r.y + 3, ui::KnobName(k) + 3, Font_6x8, kGrey);
        Dirty(r.x, r.y, r.w, r.h);
        return;
    }
    const Param& pr = params_[p];
    RoundRect(r.x, r.y, r.w, r.h, kBlue, 4);
    FillRect(r.x + 4, r.y + 1, r.w - 8, 1, kLtBlue); // a thin light edge along the top: a little depth
    char lab[20], val[16];
    const bool q_held = ((tab_ == T_EQ || tab_ == T_CLARITY) && k == 0 && knob_down_[0]) || (tab_ == T_CONFIG && k == 3 && knob_down_[3]) ||
                        (tab_ == T_PID && k == 3 && knob_down_[3]) || (tab_ == T_CLARITY && k == 2 && knob_down_[2]);
    // knob number as a cream chip (matches pg-1..pg-4 under the knobs), then what it does
    FillRect(r.x + 3, r.y + 2, 9, 10, q_held ? kYellow : kCream);
    snprintf(lab, sizeof(lab), "%s", ui::KnobName(k) + strlen(ui::kCtrlPrefix)); // (the number part of the knob's name)
    TextFb(r.x + 5, r.y + 3, lab, Font_6x8, kBlue);
    TextFb(r.x + 15, r.y + 3, pr.name, Font_6x8, q_held ? kYellow : kGrey);
    Format(p, val, sizeof(val));
    const bool small = TextW(val, Font_11x18) > r.w - 14;
    const int  xe    = TextFb(r.x + 4, r.y + (small ? 16 : 12), val, small ? Font_7x10 : Font_11x18, kWhite);
    if(pr.unit[0])
        TextFb(xe + 2, r.y + 21, pr.unit, Font_6x8, kDimText);

    const int bx = r.x + 4, bw = r.w - 8, by = r.y + r.h - 6;
    if(page_ == 0 && tab_ == T_EQ && (k == 2 || k == 3))
    {
        const int b = band_;
        if(k == 2) // thresh: the band's live level, lit where it is over the threshold
        {
            const float lv = Clampf((eq_.level_db[b] + 60.f) / 60.f, 0.f, 1.f), th = Clampf((Pf(p) + 60.f) / 60.f, 0.f, 1.f);
            const int   wl = int(lv * float(bw)), wt = int(th * float(bw));
            FillRect(bx, by - 4, bw, 6, kBlueDeep);
            FillRect(bx, by - 4, wl < wt ? wl : wt, 6, kLtBlue);
            if(wl > wt)
                FillRect(bx + wt, by - 4, wl - wt, 6, kYellow);
            FillRect(bx + wt, by - 6, 1, 10, kWhite);
            TextFb(bx, by - 14, wl > wt ? "acting" : "band level", Font_6x8, wl > wt ? kYellow : kDimText);
        }
        else // range: how far it may move (dim) and how far it is moving now (band colour)
        {
            const int   cx = bx + bw / 2;
            const float sc = float(bw / 2) / 24.f;
            const int   rw = int(fabsf(Pf(p) * 0.1f) * sc), dw = int(fabsf(eq_.dyn_db[b]) * sc);
            FillRect(bx, by - 4, bw, 6, kBlueDeep);
            if(Pf(p) < 0.f)
                FillRect(cx - rw, by - 4, rw, 6, kGrid), FillRect(cx - dw, by - 4, dw, 6, kBandCol[b]);
            else
                FillRect(cx, by - 4, rw, 6, kGrid), FillRect(cx, by - 4, dw, 6, kBandCol[b]);
            FillRect(cx, by - 6, 1, 10, kWhite);
            char nb[12], t[20];
            F1(nb, 12, eq_.dyn_db[b], true);
            snprintf(t, sizeof(t), "now %s", nb);
            TextFb(bx, by - 14, t, Font_6x8, dw > 0 ? kBandCol[b] : kDimText);
        }
    }
    else
    {
        if(page_ == 0 && tab_ == T_CONFIG && k == 2) // the theme box also shows the screen speed (hold pg-3 + turn)
            TextFb(bx, by - 13, ScreenFast() ? "screen fast" : "screen safe", Font_6x8, knob_down_[2] ? kYellow : kDimText);
        if(page_ == 0 && tab_ == T_CONFIG && k == 3) // the knobs box also shows the d-pad switch (hold pg-4 + turn)
        {
            const bool on = params_[P_DPAD].value != 0;
            TextFb(bx, by - 13, on ? "d-pad on" : "d-pad off", Font_6x8, q_held ? kYellow : kDimText);
        }
        if(page_ == 0 && tab_ == T_PID && k == 3) // the group box: is that group steered? (held: the target tilt)
        {
            const bool st = (params_[P_PID_MASK].value >> params_[P_PID_GROUP].value) & 1;
            if(q_held)
                TextFb(bx, by - 13, "target tilt", Font_6x8, kYellow);
            else
                TextFb(bx, by - 13, st ? "steered" : "push: steer", Font_6x8, st ? kCyan : kDimText);
        }
        if(page_ == 0 && tab_ == T_CLARITY && k == 2) // the clarity box also shows how many bands (hold pg-3 + turn)
        {
            char t[24], m[12];
            Format(P_CLARITY, m, sizeof(m));
            if(q_held) // the box shows the band count now; this line keeps the clarity amount in view
                snprintf(t, sizeof(t), "each %s db", m);
            else
                snprintf(t, sizeof(t), "%d bands", params_[P_CL_BANDS].value);
            TextFb(bx, by - 13, t, Font_6x8, q_held ? kYellow : kDimText);
        }
        if(page_ == 0 && tab_ == T_CLARITY && k == 0) // the thump box also shows the mode (hold pg-1 + turn)
        {
            static const char* const shortm[3] = {"dyn", "add", "norm"};
            char                     t[24];
            snprintf(t, sizeof(t), q_held ? "%s" : "mode %s", shortm[params_[P_CL_MODE].value % 3]);
            TextFb(bx, by - 13, t, Font_6x8, q_held ? kYellow : kDimText);
        }
        if(page_ == 0 && tab_ == T_EQ && k == 0) // the freq box also shows the width
        {
            char q[12], t[20];
            Format(q_held ? ParamIndex(band_, B_FREQ) : ParamIndex(band_, B_Q), q, sizeof(q));
            snprintf(t, sizeof(t), q_held ? "at %s" : "q %s", q);
            TextFb(bx, by - 13, t, Font_6x8, q_held ? kYellow : kDimText);
        }
        // where the value sits in its range
        const float f = float(pr.value - pr.min) / float(pr.max - pr.min);
        FillRect(bx, by, bw, 2, kGrid);
        FillRect(bx, by, int(Clampf(f, 0.f, 1.f) * float(bw)), 2, kCyan);
    }
    if(pad_shown_ && k == pad_sel_) // the box the touch d-pad is working on
        FrameRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, kYellow), FrameRect(r.x + 2, r.y + 2, r.w - 4, r.h - 4, kYellow);
    Dirty(r.x, r.y, r.w, r.h);
}

// ------------------------------------------------------------------ graphs
void Core::GraphEq(uint32_t)
{
    FillRect(kGx, kGy, kGw, kGh, kBlueDeep);
    FreqGrid(20.f, 3.f);
    for(int db = -12; db <= 12; db += 6)
        for(int x = kGx; x < kGx + kGw; x += (db ? 3 : 1))
            Px(x, YGain(float(db)), db ? kGrid : kLtBlue);
    TextFb(XF(100, 20, 3) + 2, kGy + kGh - 9, "100", Font_6x8, kDimText);
    TextFb(XF(1000, 20, 3) + 2, kGy + kGh - 9, "1k", Font_6x8, kDimText);
    TextFb(XF(10000, 20, 3) + 2, kGy + kGh - 9, "10k", Font_6x8, kDimText);
    TextFb(kGx + 3, YGain(12.f) - 9, "+12", Font_6x8, kDimText);
    TextFb(kGx + 3, YGain(-12.f) + 2, "-12", Font_6x8, kDimText);

    Spectrum(ring_in_, spec_a_, 20.f, 3.f, 1.8f);
    SpecBars(spec_a_, 0.f, -90.f, kSpecFill, kCyan);

    // the selected band: its threshold on the spectrum (bars crossing it = the band acts)
    const int   b  = band_;
    const float f  = FreqHz(b);
    const int   ty = YDb(Pf(ParamIndex(b, B_THRESH)), 0.f, -90.f);
    HLineDots(XF(f / 1.7f, 20, 3), XF(f * 1.7f, 20, 3), ty, 2, kBandCol[b]);
    TextFb(XF(f * 1.7f, 20, 3) + 2, ty - 4, "thr", Font_6x8, kBandCol[b]);

    static float set[320], cur[320];
    EqResponse(set, cur);
    for(int x = kGx; x < kGx + kGw; x += 2)
        Px(x, YGain(set[x - kGx]), kDimText);
    int prev = -1;
    for(int x = kGx; x < kGx + kGw; x++)
    {
        const int y = YGain(cur[x - kGx]);
        if(prev >= 0)
            Line(x - 1, prev, x, y, kWhite), Line(x - 1, prev + 1, x, y + 1, kWhite);
        prev = y;
    }
    for(int k = 0; k < kBands; k++)
    {
        const int   x = XF(FreqHz(k), 20, 3);
        const float g = Pf(ParamIndex(k, B_GAIN)) * 0.1f;
        const int   y = YGain(g), yd = YGain(g + eq_.dyn_db[k]);
        if(k == band_) // range rail: how far the dynamics may take this band
        {
            const int yr = YGain(g + Pf(ParamIndex(k, B_RANGE)) * 0.1f);
            VLineDots(x + 7, y, yr, 2, kBandCol[k]);
            FillRect(x + 5, yr, 5, 1, kBandCol[k]);
            FillRect(x + 5, yd - 1, 5, 3, kWhite);
        }
        if(abs(yd - y) > 1)
        {
            Line(x, y, x, yd, kBandCol[k]);
            FillRect(x - 2, yd - 1, 5, 3, kBandCol[k]);
        }
        for(int dy = -5; dy <= 5; dy++)
            for(int dx = -5; dx <= 5; dx++)
                if(dx * dx + dy * dy <= 25)
                    Px(x + dx, y + dy, dx * dx + dy * dy >= 17 ? (k == band_ ? kWhite : kBlueDeep) : kBandCol[k]);
        char n[2] = {char('1' + k), 0};
        TextFb(x - 2, y - 4, n, Font_6x8, kBlueDeep);
    }
}

void Core::GraphHum(uint32_t now)
{
    const bool learned = params_[P_HUM_MAINS].value == 3 && hum_.prof.valid;
    const float fmin = learned ? 20.f : 25.f, dec = learned ? 3.f : 1.60206f; // learned: 20 Hz - 20 kHz
    FillRect(kGx, kGy, kGw, kGh, kBlueDeep);
    FreqGrid(fmin, dec);
    TextFb(XF(100, fmin, dec) + 2, kGy + kGh - 9, "100", Font_6x8, kDimText);
    TextFb(XF(learned ? 1000.f : 500.f, fmin, dec) + 2, kGy + kGh - 9, learned ? "1k" : "500", Font_6x8, kDimText);
    if(learned)
        TextFb(XF(10000, fmin, dec) + 2, kGy + kGh - 9, "10k", Font_6x8, kDimText);
    Spectrum(ring_in_, spec_a_, fmin, dec, 1.2f);
    SpecBars(spec_a_, 0.f, -100.f, kSpecFill, kCyan);
    if(learned)
    {
        const NoiseProfile& pr = hum_.prof;
        for(int b = 0; b < NoiseProfile::kBands; b++) // learned hiss floor (dotted) and how hard each band is gated now
        {
            const int x = XF(kHissBandHz[b], fmin, dec), fy = YDb(pr.floor_db[b], 0.f, -100.f);
            HLineDots(x - 12, x + 12, fy, 2, kCream);
            const int h = int(Clampf(-hum_.hb_cut[b] / 40.f, 0.f, 1.f) * 40.f);
            FillRect(x - 3, kGy + 16, 7, h, Pearl(float(b) / 8.f));
        }
        for(int t = 0; t < pr.n_tones; t++) // each learned tone: a notch, and its frequency
        {
            const int x = XF(pr.tone_hz[t], fmin, dec);
            VLineDots(x, kGy + 16, kGy + kGh - 12, 2, kPink);
            char f[10];
            if(pr.tone_hz[t] >= 1000.f)
                snprintf(f, sizeof(f), "%d.%dk", int(pr.tone_hz[t] / 1000.f), int(fmodf(pr.tone_hz[t], 1000.f) / 100.f));
            else
                snprintf(f, sizeof(f), "%d", Di(pr.tone_hz[t]));
            TextFb(x + 2 + TextW(f, Font_6x8) > kGx + kGw - 2 ? x - 2 - TextW(f, Font_6x8) : x + 2, kGy + 18 + (t % 4) * 10, f, Font_6x8, kPink);
        }
    }
    else
    {
        // each notch: where it sits and how deep it is cutting right now
        for(int h = 0; h < HumStage::kH; h++)
        {
            const int x  = XF(hum_.f0 * float(h + 1), fmin, dec);
            const int dy = int(Clampf(-hum_.depth_db[h] / 40.f, 0.f, 1.f) * float(kGh - 40));
            VLineDots(x, kGy + 14, kGy + kGh - 12, 3, kDimText);
            FillRect(x - 2, kGy + 14, 5, dy, kPink);
            char n[4];
            snprintf(n, sizeof(n), "%d", h + 1);
            TextFb(x - 2, kGy + 4, n, Font_6x8, dy > 2 ? kPink : kDimText);
        }
        // hiss: top-end level against the threshold (right edge)
        const int mx = kGx + kGw - 14, top = kGy + 18, h = kGh - 32;
        FillRect(mx, top, 8, h, kBlue);
        const float lv = Clampf((hum_.hf_level_db + 100.f) / 80.f, 0.f, 1.f), th = Clampf((Pf(P_HISS_THR) + 100.f) / 80.f, 0.f, 1.f);
        FillRect(mx, top + h - int(lv * float(h)), 8, int(lv * float(h)), lv < th ? kPink : kCyan);
        FillRect(mx - 3, top + h - int(th * float(h)), 14, 1, kWhite);
        TextFb(mx - 26, top + h + 2, "hiss", Font_6x8, kDimText);
    }
    // the learn button (top right): tap it, or set pg-1 to "learn" and push pg-1
    const int bx = kGx + kGw - 54;
    FillRect(bx, kGy + 2, 52, 13, learn_state_ == 1 ? kYellow : kGrey);
    TextFb(bx + 8, kGy + 5, learn_state_ == 1 ? "learning" : "learn", Font_6x8, kBlue);
    if(learn_state_ == 1) // listening: what to do, and how long is left
    {
        const float t = Clampf(float(now - learn_t0_) / 3000.f, 0.f, 1.f);
        const int   wx = kGx + 40, wy = kGy + 40, ww = kGw - 80, wh = 56;
        FillRect(wx, wy, ww, wh, kBlue);
        FrameRect(wx, wy, ww, wh, kCream);
        TextFb(wx + 8, wy + 6, "learning the hum + hiss...", Font_6x8, kWhite);
        TextFb(wx + 8, wy + 18, "only the noise may play now", Font_6x8, kCream);
        FillRect(wx + 8, wy + 34, ww - 16, 10, kBlueDeep);
        FillRect(wx + 8, wy + 34, int(t * float(ww - 16)), 10, Pearl(t));
    }
    else if((learn_state_ == 2 || learn_state_ == 3) && now - learn_msg_t0_ < 4000)
    {
        FillRect(kGx, kGy + kGh - 24, kGw, 12, learn_state_ == 2 ? kCyan : kYellow);
        TextFb(kGx + 4, kGy + kGh - 22, learn_msg_, Font_6x8, kBlue);
    }
}

void Core::GraphComp(uint32_t)
{
    FillRect(kGx, kGy, kGw, kGh, kBlueDeep);
    // left: the curve (in -> out, -60..0 dB) with the live level riding on it
    const int sx = kGx, sw = kGh, sy = kGy;
    auto      px = [&](float db) { return sx + int(Clampf((db + 60.f) / 60.f, 0.f, 1.f) * float(sw - 1)); };
    auto      py = [&](float db) { return sy + sw - 1 - int(Clampf((db + 60.f) / 60.f, 0.f, 1.f) * float(sw - 1)); };
    for(int db = -48; db < 0; db += 12)
        HLineDots(sx, sx + sw - 1, py(float(db)), 3, kGrid), VLineDots(px(float(db)), sy, sy + sw - 1, 3, kGrid);
    Line(px(-60), py(-60), px(0), py(0), kGrid);
    int prev = -1;
    for(int i = 0; i < sw; i++)
    {
        const float in = -60.f + 60.f * float(i) / float(sw - 1);
        const int   y  = py(in + comp_.Curve(in));
        if(prev >= 0)
            Line(sx + i - 1, prev, sx + i, y, kWhite);
        prev = y;
    }
    FillRect(px(comp_.thr) , sy, 1, sw, kLtBlue);
    const float in = comp_.in_db;
    FillRect(px(in) - 2, py(in + comp_.Curve(in)) - 2, 5, 5, kYellow);
    TextFb(sx + sw - 16, sy + sw - 10, "in", Font_6x8, kDimText), TextFb(sx + 3, sy + 14, "out", Font_6x8, kDimText);
    // right: gain reduction over the last ~8 s (0 at the top, -20 dB at the bottom)
    gr_hist_[gr_pos_] = comp_.gr_db;
    gr_pos_           = (gr_pos_ + 1) % 160;
    const int hx = sx + sw + 8, hw = 160;
    for(int db = 5; db <= 20; db += 5)
        HLineDots(hx, hx + hw - 1, kGy + int(float(db) / 20.f * float(kGh - 1)), 4, kGrid);
    for(int i = 0; i < hw; i++)
    {
        const float g  = gr_hist_[(gr_pos_ + i) % 160];
        const int   hh = int(Clampf(-g / 20.f, 0.f, 1.f) * float(kGh - 1));
        if(hh > 0)
            FillRect(hx + i, kGy, 1, hh, i == hw - 1 ? kWhite : Pearl(float(i) / 400.f));
    }
    TextFb(hx + 2, kGy + kGh - 10, "gain reduction", Font_6x8, kDimText);
}

void Core::GraphClarity(uint32_t)
{
    FillRect(kGx, kGy, kGw, kGh, kBlueDeep);
    FreqGrid(20.f, 3.f);
    Spectrum(ring_out_, spec_a_, 20.f, 3.f, 1.8f);
    SpecBars(spec_a_, 0.f, -90.f, kSpecFill, kCyan);
    // bass detail: the harmonics it adds live in 150 Hz - 1.5 kHz
    const int d0 = XF(150, 20, 3), d1 = XF(1500, 20, 3);
    const int dh = int(Pf(P_DETAIL) / 60.f * 10.f);
    for(int x = d0; x < d1; x += 2)
        FillRect(x, kGy + kGh - 2 - dh, 1, dh, kPink);
    // what it hears: each spot above / below the smooth balance (dotted, around the middle)
    for(int i = 0; i < ClarityStage::kD; i++)
        for(int k = -1; k <= 1; k++)
            Px(XF(clar_.det_hz[i], 20, 3) + k, YGain(Clampf(clar_.dev_db[i], -12.f, 12.f)), kCream);
    // what it does: the curve all the moves make right now (+-12 dB)
    int prev = -1;
    for(int x = kGx; x < kGx + kGw; x++)
    {
        const float t = tanf(kPi * fminf(FX(x, 20, 3), sample_rate_ * 0.49f) / sample_rate_);
        float       s = clar_.thump.MagDbT(t) + clar_.warm_lo.MagDbT(t);
        for(const ClarityStage::Band& b : clar_.band)
            if(b.live)
                s += b.f.MagDbT(t);
        const int y = YGain(s * 1.5f);
        if(prev >= 0)
            Line(x - 1, prev, x, y, kWhite), Line(x - 1, prev + 1, x, y + 1, kWhite);
        prev = y;
    }
    // each band: a dot where it sits, and a bar as wide as it is (its q)
    for(int i = 0; i < ClarityStage::kMaxBands; i++)
    {
        const ClarityStage::Band& b = clar_.band[i];
        if(!b.live)
            continue;
        const float half = asinhf(1.f / (2.f * b.q)) / logf(2.f); // half the bandwidth, octaves
        const int   x = XF(b.hz, 20, 3), y = YGain(b.db * 1.5f);
        const int   x0 = XF(b.hz / powf(2.f, half), 20, 3), x1 = XF(b.hz * powf(2.f, half), 20, 3);
        const uint16_t c = b.db < 0.f ? kPink : kCyan;
        FillRect(x0, y, x1 - x0 + 1, 1, c);
        FillRect(x - 2, y - 2, 5, 5, c);
    }
    char nb[24];
    snprintf(nb, sizeof(nb), "%d bands", params_[P_CL_BANDS].value);
    TextFb(kGx + 4, kGy + 4, nb, Font_6x8, kDimText);
    TextFb(kGx + 4, kGy + kGh - 20, "lift", Font_6x8, kCyan), TextFb(kGx + 34, kGy + kGh - 20, "cut", Font_6x8, kPink);
    // mode button (tap to change): dynamic / add / normalise
    char m[16], t[24];
    Format(P_CL_MODE, m, sizeof(m));
    snprintf(t, sizeof(t), "mode: %s", m);
    FillRect(kGx + kGw - 102, kGy + 2, 100, 13, kGrey);
    TextFb(kGx + kGw - 98, kGy + 5, t, Font_6x8, kBlue);
}

void Core::GraphSat(uint32_t)
{
    FillRect(kGx, kGy, kGw, kGh, kBlueDeep);
    // left: the curve (input -1.5..1.5 -> output), as the mix blends it
    const int   sx = kGx, sw = kGh, sy = kGy, mid = sy + sw / 2;
    const float span = 1.5f;
    HLineDots(sx, sx + sw - 1, mid, 3, kGrid), VLineDots(sx + sw / 2, sy, sy + sw - 1, 3, kGrid);
    Line(sx, sy + sw - 1, sx + sw - 1, sy, kGrid);
    auto curve = [&](float x) {
        const float y = (tanhf(x * sat_.gain + sat_.bias) - sat_.tb) * sat_.norm;
        return x + (y - x) * sat_.mix;
    };
    int prev = -1;
    for(int i = 0; i < sw; i++)
    {
        const float x = -span + 2.f * span * float(i) / float(sw - 1);
        const int   y = mid - int(Clampf(curve(x) / span, -1.f, 1.f) * float(sw / 2 - 1));
        if(prev >= 0)
            Line(sx + i - 1, prev, sx + i, y, kWhite), Line(sx + i - 1, prev + 1, sx + i, y + 1, kWhite);
        prev = y;
    }
    const float pk = Clampf(sat_.drive_pk / sat_.gain, 0.f, span);
    for(int s = -1; s <= 1; s += 2)
    {
        const int x = sx + sw / 2 + int(float(s) * pk / span * float(sw / 2 - 1));
        FillRect(x - 2, mid - int(Clampf(curve(float(s) * pk) / span, -1.f, 1.f) * float(sw / 2 - 1)) - 2, 5, 5, kYellow);
    }
    // right: the output waveform (triggered on a rising zero crossing)
    const int ox = sx + sw + 8, ow = kGw - sw - 8, omid = kGy + kGh / 2;
    HLineDots(ox, ox + ow - 1, omid, 3, kGrid);
    int start = (scope_pos_ - ow - 200) & (kScope - 1);
    for(int i = 0; i < 200; i++)
    {
        const int a = (start + i) & (kScope - 1), b2 = (a + 1) & (kScope - 1);
        if(scope_l_[a] <= 0.f && scope_l_[b2] > 0.f)
        {
            start = b2;
            break;
        }
    }
    prev = -1;
    for(int i = 0; i < ow; i++)
    {
        const float v = scope_l_[(start + i) & (kScope - 1)];
        const int   y = omid - int(Clampf(v, -1.f, 1.f) * float(kGh / 2 - 4));
        if(prev >= 0)
            Line(ox + i - 1, prev, ox + i, y, kCyan);
        prev = y;
    }
    TextFb(ox + 2, kGy + kGh - 10, "output", Font_6x8, kDimText);
}

void Core::GraphDeHarsh(uint32_t)
{
    const float fmin = 1000.f, dec = 1.30103f; // 1 - 20 kHz
    FillRect(kGx, kGy, kGw, kGh, kBlueDeep);
    FreqGrid(fmin, dec);
    TextFb(XF(2000, fmin, dec) + 2, kGy + kGh - 9, "2k", Font_6x8, kDimText);
    TextFb(XF(5000, fmin, dec) + 2, kGy + kGh - 9, "5k", Font_6x8, kDimText);
    TextFb(XF(10000, fmin, dec) + 2, kGy + kGh - 9, "10k", Font_6x8, kDimText);
    Spectrum(ring_out_, spec_a_, fmin, dec, 1.8f);
    SpecBars(spec_a_, 0.f, -90.f, kSpecFill, kCyan);
    const int y0 = kGy + 30;
    HLineDots(kGx, kGx + kGw - 1, y0, 2, kLtBlue);
    for(int i = 0; i < DeHarshStage::kD; i++) // each harsh-spot cut, hanging from the 0 line
    {
        const int x = XF(kDeHarshF[i], fmin, dec), h = int(Clampf(-dh_.cut_db[i] / 12.f, 0.f, 1.f) * 90.f);
        FillRect(x - 4, y0, 9, h, Pearl(float(i) / 6.f));
    }
    int prev = -1;
    for(int x = kGx; x < kGx + kGw; x++)
    {
        const float t = tanf(kPi * fminf(FX(x, fmin, dec), sample_rate_ * 0.49f) / sample_rate_);
        float       s = dh_.comfort.MagDbT(t);
        for(int i = 0; i < DeHarshStage::kD; i++)
            s += dh_.cut[i].MagDbT(t);
        const int y = y0 - int(s * 7.5f);
        if(prev >= 0)
            Line(x - 1, prev, x, y, kWhite);
        prev = y;
    }
}

void Core::Meter(int x, int y, int w, int h, float db, float lo, float hi, float limit, uint16_t c, const char* label)
{
    FillRect(x, y, w, h, kBlue);
    FrameRect(x - 1, y - 1, w + 2, h + 2, kGrid);
    const float v  = Clampf((db - lo) / (hi - lo), 0.f, 1.f), lv = Clampf((limit - lo) / (hi - lo), 0.f, 1.f);
    const int   fh = int(v * float(h)), ly = y + h - int(lv * float(h));
    FillRect(x, y + h - fh, w, fh, db > limit ? kYellow : c);
    FillRect(x - 4, ly, w + 8, 1, kWhite);
    TextFb(x + w / 2 - TextW(label, Font_6x8) / 2, y + h + 3, label, Font_6x8, kCream);
}

void Core::GraphSafety(uint32_t)
{
    FillRect(kGx, kGy, kGw, kGh, kBlueDeep);
    const int   top = kGy + 18, h = kGh - 34;
    const float pk  = 20.f * log10f(fmaxf(out_pk_[0], out_pk_[1]) + 1e-9f);
    Meter(kGx + 26, top, 22, h, pk, -40.f, 0.f, Pf(P_S_CEIL) * 0.1f, kCyan, "peak");
    Meter(kGx + 98, top, 22, h, safety_.lvl_ear, -50.f, 0.f, Pf(P_S_EAR), kCyan, "ears");
    Meter(kGx + 170, top, 22, h, safety_.lvl_low, -50.f, 0.f, Pf(P_S_WOOF), kPink, "woofer");
    Meter(kGx + 242, top, 22, h, safety_.lvl_high, -60.f, -10.f, Pf(P_S_TWEET), kBandCol[2], "tweeter");
    char buf[40];
    snprintf(buf, sizeof(buf), "glitches caught: %d", safety_.glitches);
    TextFb(kGx + 6, kGy + 4, buf, Font_6x8, safety_.glitches ? kYellow : kDimText);
    TextFb(kGx + kGw - 6 * 19 - 6, kGy + 4, "only 20 hz-19.5 khz", Font_6x8, kCyan);
}

void Core::GraphVis(uint32_t)
{
    const int   mode  = params_[P_V_MODE].value, src = params_[P_V_SRC].value;
    const float range = Pf(P_V_RANGE), fall = 0.4f * Pf(P_V_FALL);
    const bool  first = vis_drawn_ != mode;
    vis_drawn_        = mode;
    if(mode == 1) // waterfall: the graph scrolls down, newest line on top
    {
        Spectrum(src == 0 ? ring_in_ : ring_out_, spec_a_, 20.f, 3.f, fall);
        if(first)
            FillRect(kGx, kGy, kGw, kGh, kBlueDeep);
        const int step = ScreenFast() ? 1 : 2; // ~60 rows a second either way
        for(int y = kGy + kGh - 1; y >= kGy + step; y--)
            memcpy(fb_ + y * Canvas::kW + kGx, fb_ + (y - step) * Canvas::kW + kGx, size_t(kGw) * 2);
        for(int x = kGx; x < kGx + kGw; x++)
        {
            const uint16_t c = Heat((spec_a_[x] + range) / range);
            Px(x, kGy, c), Px(x, kGy + step - 1, c);
        }
        return;
    }
    FillRect(kGx, kGy, kGw, kGh, kBlueDeep);
    if(first)
        for(float& v : bar_pk_)
            v = -200.f; // the bars' peak caps start from the bottom
    if(mode == 4)
        return VisScope();
    if(mode == 5)
        return VisBars(fall, range);
    if(mode == 6)
        return VisHistory(range);
    if(mode == 0) // spectrum: input as filled bars, output as a white line
    {
        FreqGrid(20.f, 3.f);
        TextFb(XF(100, 20, 3) + 2, kGy + kGh - 9, "100", Font_6x8, kDimText);
        TextFb(XF(1000, 20, 3) + 2, kGy + kGh - 9, "1k", Font_6x8, kDimText);
        TextFb(XF(10000, 20, 3) + 2, kGy + kGh - 9, "10k", Font_6x8, kDimText);
        if(src != 1)
        {
            Spectrum(ring_in_, spec_a_, 20.f, 3.f, fall);
            SpecBars(spec_a_, 0.f, -range, kSpecFill, kLtBlue);
        }
        if(src != 0)
        {
            Spectrum(ring_out_, spec_b_, 20.f, 3.f, fall);
            int prev = -1;
            for(int x = kGx; x < kGx + kGw; x++)
            {
                const int y = YDb(spec_b_[x], 0.f, -range);
                if(prev >= 0)
                    Line(x - 1, prev, x, y, Pearl(float(x) / 600.f));
                prev = y;
            }
        }
        TextFb(kGx + 4, kGy + 14, src == 0 ? "input" : (src == 1 ? "output" : "in (bars)  out (line)"), Font_6x8, kDimText);
        return;
    }
    if(mode == 2) // stereo: vectorscope (mid up, sides across) + correlation
    {
        const int cx = kGx + kGh / 2, cy = kGy + kGh / 2, rad = kGh / 2 - 4;
        Line(cx - rad, cy, cx + rad, cy, kGrid), Line(cx, cy - rad, cx, cy + rad, kGrid);
        Line(cx - rad, cy - rad, cx + rad, cy + rad, kGrid), Line(cx - rad, cy + rad, cx + rad, cy - rad, kGrid);
        float sll = 0.f, srr = 0.f, slr = 0.f;
        for(int i = 0; i < kScope; i++)
        {
            const float l = scope_l_[i], r = scope_r_[i];
            sll += l * l, srr += r * r, slr += l * r;
            const int x = cx + int(Clampf((r - l) * 0.707f, -1.f, 1.f) * float(rad));
            const int y = cy - int(Clampf((l + r) * 0.707f, -1.f, 1.f) * float(rad));
            Px(x, y, Pearl(float(i) / float(kScope)));
        }
        const float c = slr / (sqrtf(sll * srr) + 1e-12f);
        corr_ += (Clampf(c, -1.f, 1.f) - corr_) * 0.2f;
        const int mx = kGx + kGh + 24, mw = kGw - kGh - 40, my = kGy + 40;
        FillRect(mx, my, mw, 10, kBlue);
        FillRect(mx + mw / 2, my - 3, 1, 16, kWhite);
        const int cp = mx + int((corr_ + 1.f) * 0.5f * float(mw - 1));
        FillRect(cp - 2, my, 5, 10, corr_ < 0.f ? kYellow : kCyan);
        TextFb(mx, my + 14, "-1  correlation  +1", Font_6x8, kDimText);
        const float bal = (sqrtf(srr) - sqrtf(sll)) / (sqrtf(srr) + sqrtf(sll) + 1e-12f);
        const int   by  = my + 50, bp = mx + int((bal + 1.f) * 0.5f * float(mw - 1));
        FillRect(mx, by, mw, 10, kBlue);
        FillRect(mx + mw / 2, by - 3, 1, 16, kWhite);
        FillRect(bp - 2, by, 5, 10, kPink);
        TextFb(mx, by + 14, "l     balance     r", Font_6x8, kDimText);
        return;
    }
    // levels: input / output peaks, and how much each stage is taking off right now
    const float lo = -range;
    auto        db = [](float v) { return 20.f * log10f(v + 1e-9f); };
    Meter(kGx + 14, kGy + 14, 14, kGh - 36, db(in_pk_[0]), lo, 0.f, 0.f, kLtBlue, "in");
    Meter(kGx + 32, kGy + 14, 14, kGh - 36, db(in_pk_[1]), lo, 0.f, 0.f, kLtBlue, "");
    Meter(kGx + 74, kGy + 14, 14, kGh - 36, db(out_pk_[0]), lo, 0.f, 0.f, kCyan, "out");
    Meter(kGx + 92, kGy + 14, 14, kGh - 36, db(out_pk_[1]), lo, 0.f, 0.f, kCyan, "");
    struct G
    {
        const char* n;
        float       v;
    } gr[5] = {{"comp", comp_.gr_db}, {"de-harsh", 0.f}, {"safety", safety_.limit_db + safety_.ear_db + safety_.blast_db},
               {"bass lim", safety_.bass_limit_db}, {"clarity", clar_.cut_most}};
    for(int i = 0; i < DeHarshStage::kD; i++)
        gr[1].v = fminf(gr[1].v, dh_.cut_db[i]);
    for(int i = 0; i < 5; i++)
    {
        const int y = kGy + 16 + i * 22, x = kGx + 132;
        TextFb(x, y, gr[i].n, Font_6x8, kCream);
        FillRect(x + 54, y, 92, 8, kBlue);
        FillRect(x + 54, y, int(Clampf(-gr[i].v / 20.f, 0.f, 1.f) * 92.f), 8, Pearl(float(i) / 5.f));
        char b[12];
        F1(b, 12, gr[i].v, false);
        TextFb(x + 150, y, b, Font_6x8, kDimText);
    }
    // anti-duck: how much it is lifting the mids and highs back up right now (bass-heavy moments)
    {
        const int y = kGy + 16 + 5 * 22, x = kGx + 132;
        TextFb(x, y, "anti-duck", Font_6x8, kCream);
        FillRect(x + 54, y, 92, 8, kBlue);
        FillRect(x + 54, y, int(Clampf(antiduck_.boost_db / 6.f, 0.f, 1.f) * 92.f), 8, kCyan);
        char b[12];
        F1(b, 12, antiduck_.boost_db, true);
        TextFb(x + 150, y, b, Font_6x8, kDimText);
    }
}

// input auto-level: the source's level (dim) and where it was brought to (pearl) over ~8 s, the target
// line, the gain it's using now, and a lamp when the source is hotter than the converter can take
void Core::GraphInput(uint32_t now)
{
    FillRect(kGx, kGy, kGw, kGh, kBlueDeep);
    const int hw = 160, hx = kGx + 8;
    lv_in_[lv_pos_]  = lvl_.in_db;
    lv_out_[lv_pos_] = lvl_.in_db + lvl_.gain_db;
    lv_pos_          = (lv_pos_ + 1) % hw;
    auto y           = [&](float db) { return YDb(db, 0.f, -60.f); };
    for(int db = -12; db >= -48; db -= 12)
        HLineDots(hx, hx + hw - 1, y(float(db)), 4, kGrid);
    const int ty = y(Pf(P_IN_TARGET));
    FillRect(hx, ty, hw, 1, kLtBlue);
    TextFb(hx + 2, ty - 9, "target", Font_6x8, kLtBlue);
    int pi = -1, po = -1;
    for(int i = 0; i < hw; i++)
    {
        const int k = (lv_pos_ + i) % hw, yi = y(lv_in_[k]), yo = y(lv_out_[k]);
        if(lv_in_[k] < -110.f) // not filled yet
        {
            pi = po = -1;
            continue;
        }
        if(pi >= 0)
            Line(hx + i - 1, pi, hx + i, yi, kDimText), Line(hx + i - 1, po, hx + i, yo, Pearl(float(i) / 320.f));
        pi = yi, po = yo;
    }
    TextFb(hx + 2, kGy + kGh - 10, "source (dim) - leveled (pearl)", Font_6x8, kDimText);
    // right: the gain now, big, and the clip lamp
    const int rx = hx + hw + 14;
    char g[12];
    F1(g, 12, lvl_.gain_db, true);
    TextFb(rx, kGy + 18, "gain now", Font_6x8, kDimText);
    TextFb(rx, kGy + 30, g, Font_11x18, kWhite);
    TextFb(rx + TextW(g, Font_11x18) + 3, kGy + 39, "db", Font_6x8, kDimText);
    const float span = Pf(P_IN_BOOST) + Pf(P_IN_CUT);
    const int   bw   = kGx + kGw - 10 - rx, by = kGy + 60;
    FillRect(rx, by, bw, 8, kBlue);
    const int z = rx + int(Pf(P_IN_CUT) / (span > 0.f ? span : 1.f) * float(bw));
    const int gx = rx + int((lvl_.gain_db + Pf(P_IN_CUT)) / (span > 0.f ? span : 1.f) * float(bw));
    FillRect(gx < z ? gx : z, by, abs(gx - z), 8, Pearl(float(now % 6000) / 6000.f));
    FillRect(z, by - 3, 1, 14, kWhite);
    TextFb(rx, by + 11, "cut", Font_6x8, kDimText), TextFb(rx + bw - 30, by + 11, "boost", Font_6x8, kDimText);
    const bool hot = clips_ != 0 && now - clip_t_ < 2000;
    FillRect(rx, kGy + 98, bw, 18, hot ? kYellow : kBlue);
    TextFb(rx + 4, kGy + 103, hot ? "source too hot!" : "no clipping", Font_6x8, hot ? kBlue : kDimText);
}

// takeback: what it's giving back right now, and the last few seconds of it
void Core::GraphTakeback(uint32_t)
{
    FillRect(kGx, kGy, kGw, kGh, kBlueDeep);
    const float v[4] = {tb_.punch_db, tb_.detail_db, tb_.air_db, tb_.space_db};
    static const char* const n[4] = {"punch", "detail", "air", "space"};
    TextFb(kGx + 6, kGy + 4, "giving back now (db)", Font_6x8, kDimText);
    for(int i = 0; i < 4; i++)
        Meter(kGx + 14 + i * 36, kGy + 18, 18, kGh - 36, v[i], 0.f, 6.f, 7.f, Pearl(float(i) / 4.f), n[i]);
    gr_hist_[gr_pos_] = v[0] + v[1];
    gr_pos_           = (gr_pos_ + 1) % 160;
    const int hx = kGx + 160, hw = kGw - 166, hb = kGy + kGh - 14;
    for(int i = 0; i < hw; i++)
    {
        const float g  = gr_hist_[(gr_pos_ + 160 - hw + i) % 160];
        const int   hh = int(Clampf(g / 8.f, 0.f, 1.f) * float(kGh - 30));
        if(hh > 0)
            FillRect(hx + i, hb - hh, 1, hh, Pearl(float(i) / 300.f));
    }
    FillRect(hx, hb, hw, 1, kGrid);
    TextFb(hx, hb + 3, "punch + detail, last ~8 s", Font_6x8, kDimText);
}

// width: the output's stereo picture (vectorscope) and how well it holds up in mono
void Core::GraphWidth(uint32_t)
{
    FillRect(kGx, kGy, kGw, kGh, kBlueDeep);
    const int cx = kGx + kGh / 2, cy = kGy + kGh / 2, rad = kGh / 2 - 4;
    Line(cx - rad, cy, cx + rad, cy, kGrid), Line(cx, cy - rad, cx, cy + rad, kGrid);
    Line(cx - rad, cy - rad, cx + rad, cy + rad, kGrid), Line(cx - rad, cy + rad, cx + rad, cy - rad, kGrid);
    for(int i = 0; i < kScope; i++)
    {
        const float l = scope_l_[i], r = scope_r_[i];
        Px(cx + int(Clampf((r - l) * 0.707f, -1.f, 1.f) * float(rad)), cy - int(Clampf((l + r) * 0.707f, -1.f, 1.f) * float(rad)),
           Pearl(float(i) / float(kScope)));
    }
    const int mx = kGx + kGh + 24, mw = kGw - kGh - 40;
    TextFb(mx, kGy + 10, "mono-safe", Font_6x8, kDimText);
    FillRect(mx, kGy + 22, mw, 10, kBlue);
    FillRect(mx + mw / 2, kGy + 19, 1, 16, kWhite);
    const int cp = mx + int((Clampf(width_.corr, -1.f, 1.f) + 1.f) * 0.5f * float(mw - 1));
    FillRect(cp - 2, kGy + 22, 5, 10, width_.corr < 0.1f ? kYellow : kCyan);
    TextFb(mx, kGy + 36, "-1  correlation  +1", Font_6x8, kDimText);
    char b[24];
    snprintf(b, sizeof(b), "width now %d%%", Di(width_.w_eff * 100.f));
    TextFb(mx, kGy + 62, b, Font_7x10, kWhite);
    if(params_[P_W_GUARD].value && width_.w_eff < width_.w - 0.05f)
        TextFb(mx, kGy + 78, "guard is easing it off", Font_6x8, kYellow);
}

// loudness: the lift it's adding now (bass / treble), on the frequency axis
void Core::GraphLoud(uint32_t)
{
    FillRect(kGx, kGy, kGw, kGh, kBlueDeep);
    FreqGrid(20.f, 3.f);
    for(int db = -12; db <= 12; db += 6)
        for(int x = kGx; x < kGx + kGw; x += (db ? 3 : 1))
            Px(x, YGain(float(db)), db ? kGrid : kLtBlue);
    int prev = -1;
    for(int x = kGx; x < kGx + kGw; x++)
    {
        const float t = tanf(kPi * fminf(FX(x, 20, 3), sample_rate_ * 0.49f) / sample_rate_);
        const int   y = YGain(loud_.lo.MagDbT(t) + loud_.hi.MagDbT(t));
        if(prev >= 0)
            Line(x - 1, prev, x, y, kWhite), Line(x - 1, prev + 1, x, y + 1, kWhite);
        prev = y;
    }
    char b[48];
    snprintf(b, sizeof(b), "listening at about %d phon", Di(loud_.eff));
    TextFb(kGx + 6, kGy + 6, b, Font_6x8, kCream);
    TextFb(kGx + 6, kGy + 18, "quieter = more bass + treble back", Font_6x8, kDimText);
}

// configs: the 8 slots, which are saved, and how to use them
void Core::GraphConfig(uint32_t now)
{
    FillRect(kGx, kGy, kGw, kGh, kBlueDeep);
    const int sel = params_[P_CF_SLOT].value;
    for(int k = 0; k < kSlots; k++)
    {
        const int y = kGy + 4 + k * 13;
        if(k == sel)
            FillRect(kGx + 2, y - 2, 150, 13, kBlue), FrameRect(kGx + 2, y - 2, 150, 13, kLtBlue);
        FillRect(kGx + 8, y + 1, 6, 6, slots_[k].used ? kCyan : kGrid);
        char b[24];
        snprintf(b, sizeof(b), "config %d", k + 1);
        TextFb(kGx + 20, y, b, Font_6x8, k == sel ? kWhite : kDimText);
        TextFb(kGx + 100, y, slots_[k].used ? "saved" : "empty", Font_6x8, slots_[k].used ? kCyan : kGrid);
    }
    const int tx = kGx + 162;
    char a1[32], a2[32];
    snprintf(a1, sizeof(a1), "push %s: load", ui::KnobName(0));
    snprintf(a2, sizeof(a2), "push %s twice: save", ui::KnobName(1));
    TextFb(tx, kGy + 6, a1, Font_6x8, kCream);
    TextFb(tx, kGy + 18, a2, Font_6x8, kCream);
    TextFb(tx, kGy + 34, "a config = every sound", Font_6x8, kDimText);
    TextFb(tx, kGy + 44, "setting + stages on/off", Font_6x8, kDimText);
    TextFb(tx, kGy + 58, "everything is kept over", Font_6x8, kDimText);
    TextFb(tx, kGy + 68, "power-off (autosave)", Font_6x8, kDimText);
    // the two themes, as swatches
    const uint16_t pearl[3] = {Canvas::Rgb(222, 230, 246), Canvas::Rgb(34, 62, 178), Canvas::Rgb(12, 22, 74)};
    const uint16_t bios[3]  = {Canvas::Rgb(170, 170, 170), Canvas::Rgb(0, 0, 170), Canvas::Rgb(0, 0, 128)};
    for(int i = 0; i < 3; i++)
        FillRect(tx + i * 10, kGy + 86, 9, 9, pearl[i]), FillRect(tx + 70 + i * 10, kGy + 86, 9, 9, bios[i]);
    TextFb(tx, kGy + 98, "pearl", Font_6x8, params_[P_CF_THEME].value ? kDimText : kYellow);
    TextFb(tx + 70, kGy + 98, "bios", Font_6x8, params_[P_CF_THEME].value ? kYellow : kDimText);
    { // screen speed switch (left, under the configs)
        const bool fast = ScreenFast();
        FillRect(kGx + 2, kGy + 109, 150, 13, fast ? kCyan : kBlue);
        TextFb(kGx + 6, kGy + 112, fast ? "screen: fast 60 fps" : "screen: safe 25 fps", Font_6x8, fast ? kBlue : kCream);
    }
    { // touch d-pad switch
        const bool on = params_[P_DPAD].value != 0;
        FillRect(tx, kGy + 109, 148, 13, on ? kCyan : kBlue);
        TextFb(tx + 4, kGy + 112, on ? "d-pad on: tap the title" : "touch d-pad: off (tap)", Font_6x8, on ? kBlue : kCream);
    }
    { // about this pedal
        char a[54];
        if(device_id_)
            snprintf(a, sizeof(a), "pg audio pg-1  fw %s  id %04X", kFirmwareVersion, unsigned(device_id_ & 0xFFFF));
        else
            snprintf(a, sizeof(a), "pg audio pg-1  fw %s  (plugin)", kFirmwareVersion);
        TextFb(kGx + 4, kGy + kGh - 12, a, Font_6x8, kDimText);
    }
    if(now - cfg_msg_t0_ < 3000 && cfg_msg_[0])
    {
        FillRect(kGx, kGy + kGh - 14, kGw, 12, kCyan);
        char b[54];
        snprintf(b, sizeof(b), "%s (config %d)", cfg_msg_, sel + 1);
        TextFb(kGx + 4, kGy + kGh - 12, b, Font_6x8, kBlue);
    }
}

// multiband: per band, the spectrum share it covers, how much it's being held back now, and its level
void Core::GraphMband(uint32_t)
{
    FillRect(kGx, kGy, kGw, kGh, kBlueDeep);
    FreqGrid(20.f, 3.f);
    Spectrum(ring_out_, spec_a_, 20.f, 3.f, 1.8f);
    SpecBars(spec_a_, 0.f, -90.f, kSpecFill, kCyan);
    static const char* const n[4] = {"bass", "low mids", "high mids", "highs"};
    const float edge[5] = {20.f, 120.f, 1000.f, 6000.f, 20000.f};
    for(int b = 0; b < 4; b++)
    {
        const int x0 = XF(edge[b], 20.f, 3.f), x1 = XF(edge[b + 1], 20.f, 3.f);
        if(b > 0)
            VLineDots(x0, kGy, kGy + kGh - 1, 2, kCream);
        // how much it is holding back right now, hanging from the top (0 .. 12 dB)
        const int h = int(Clampf(-mb_.gr[b] / 12.f, 0.f, 1.f) * float(kGh - 30));
        FillRect(x0 + 3, kGy + 16, x1 - x0 - 6, h, Pearl(float(b) / 4.f));
        TextFb(x0 + (x1 - x0 - TextW(n[b], Font_6x8)) / 2, kGy + 4, n[b], Font_6x8, kCream);
    }
    TextFb(XF(120, 20, 3) - 9, kGy + kGh - 9, "120", Font_6x8, kDimText);
    TextFb(XF(1000, 20, 3) - 6, kGy + kGh - 9, "1k", Font_6x8, kDimText);
    TextFb(XF(6000, 20, 3) - 6, kGy + kGh - 9, "6k", Font_6x8, kDimText);
}
// ------------------------------------------------------------------ visualizer: scope, bars, history
// oscilloscope: the last ~13 ms of sound, triggered on a rising zero crossing so a steady note stands
// still, and auto-scaled (the x number) so quiet sounds still fill the screen
void Core::VisScope()
{
    const int src = params_[P_V_SRC].value;
    const int pos = ring_pos_, step = 2, n = kGw * step;
    const float* main = src == 0 ? ring_in_ : ring_out_;
    auto at = [&](const float* r, int k) { return r[(pos + k) & (kRing - 1)]; }; // k = 0 is the oldest sample
    int start = 0;
    for(int k = 1; k < kRing - n; k++)
        if(at(main, k - 1) < 0.f && at(main, k) >= 0.f && at(main, k + 3) > at(main, k))
        {
            start = k;
            break;
        }
    float pk = 0.f;
    for(int k = 0; k < n; k++)
        pk = fmaxf(pk, fabsf(at(main, start + k)));
    const float want = Clampf(0.9f / fmaxf(pk, 0.004f), 1.f, 200.f);
    scope_gain_ += (want - scope_gain_) * (want < scope_gain_ ? 0.5f : 0.08f); // shrink fast, grow slowly
    const int cy = kGy + kGh / 2, half = kGh / 2 - 6;
    for(int g = 1; g < 8; g++)
        VLineDots(kGx + g * kGw / 8, kGy, kGy + kGh - 1, 3, kGrid);
    HLineDots(kGx, kGx + kGw - 1, cy - half / 2, 3, kGrid), HLineDots(kGx, kGx + kGw - 1, cy + half / 2, 3, kGrid);
    Line(kGx, cy, kGx + kGw - 1, cy, kGrid);
    auto trace = [&](const float* r, bool bright) {
        int prev = -1;
        for(int x = 0; x < kGw; x++)
        {
            const float v = Clampf(at(r, start + x * step) * scope_gain_, -1.f, 1.f);
            const int   y = cy - int(v * float(half));
            if(prev >= 0)
                Line(kGx + x - 1, prev, kGx + x, y, bright ? Pearl(float(x) / 500.f) : kLtBlue);
            prev = y;
        }
    };
    if(src == 2)
        trace(ring_in_, false);
    trace(main, true);
    char b[24];
    snprintf(b, sizeof(b), "scope %s  x%d", src == 0 ? "in" : (src == 1 ? "out" : "in+out"), Di(scope_gain_));
    TextFb(kGx + 4, kGy + 4, b, Font_6x8, kDimText);
    TextFb(kGx + kGw - 6 * 9 - 4, kGy + kGh - 10, "13 ms", Font_6x8, kDimText);
}

// bars: the spectrum in 31 chunky bands (about a third of an octave each) with falling peak caps
void Core::VisBars(float fall, float range)
{
    const int src = params_[P_V_SRC].value;
    Spectrum(src == 0 ? ring_in_ : ring_out_, spec_a_, 20.f, 3.f, fall);
    for(int j = 0; j < kBars; j++)
    {
        const int x0 = kGx + j * kGw / kBars, x1 = kGx + (j + 1) * kGw / kBars;
        float     v  = -200.f;
        for(int x = x0; x < x1; x++)
            v = fmaxf(v, spec_a_[x]);
        bar_pk_[j] = fmaxf(v, bar_pk_[j] - 0.15f * fall);
        const int y = YDb(v, 0.f, -range), yp = YDb(bar_pk_[j], 0.f, -range);
        for(int yy = y; yy < kGy + kGh; yy++) // coloured by height, like the waterfall
            FillRect(x0 + 1, yy, x1 - x0 - 2, 1, Heat(float(kGy + kGh - yy) / float(kGh)));
        FillRect(x0 + 1, yp, x1 - x0 - 2, 2, kCream);
    }
    TextFb(kGx + 4, kGy + 4, src == 0 ? "bars: input" : "bars: output", Font_6x8, kDimText);
}

void Core::RecordHistory(uint32_t now)
{
    if(now - last_hist_ < 50)
        return;
    last_hist_          = now;
    hist_in_[hist_pos_] = PowToDb(in_rms_) + 3.f, hist_out_[hist_pos_] = PowToDb(out_rms_) + 3.f;
    hist_pos_           = (hist_pos_ + 1) % kGw;
}

// history: how loud the input (blue) and output (pearl) were over the last ~15 s, newest on the right
void Core::VisHistory(float range)
{
    for(int db = -10; db > -int(range); db -= 10)
    {
        const int y = YDb(float(db), 0.f, -range);
        HLineDots(kGx, kGx + kGw - 1, y, 3, kGrid);
        char b[8];
        snprintf(b, sizeof(b), "%d", db);
        TextFb(kGx + kGw - 20, y - 9, b, Font_6x8, kDimText);
    }
    const int pos = hist_pos_;
    for(int pass = 0; pass < 2; pass++)
    {
        const float* h    = pass ? hist_out_ : hist_in_;
        int          prev = -1;
        for(int x = 0; x < kGw; x++)
        {
            const int y = YDb(h[(pos + x) % kGw], 0.f, -range);
            if(prev >= 0)
                Line(kGx + x - 1, prev, kGx + x, y, pass ? Pearl(float(x) / 600.f) : kLtBlue);
            prev = y;
        }
    }
    TextFb(kGx + 4, kGy + 4, "loudness 15 s: in blue, out pearl", Font_6x8, kDimText);
}

// ------------------------------------------------------------------ pid
// left: what it hears - each band vs the mix (bars) and the target balance (cream ticks).
// right: the 6 groups, which are steered, and how far each is being moved right now.
void Core::GraphPid(uint32_t)
{
    FillRect(kGx, kGy, kGw, kGh, kBlueDeep);
    const int cy = kGy + 66, sc = 4; // 4 px per dB, +-12 dB
    static const char* const f[PidStage::kA] = {"60", "160", "450", "1.2k", "3.2k", "9k"};
    TextFb(kGx + 4, kGy + 3, "what it hears", Font_6x8, kDimText);
    Line(kGx + 4, cy, kGx + 156, cy, kGrid);
    for(int a = 0; a < PidStage::kA; a++)
    {
        const int x = kGx + 8 + a * 25, w = 18;
        const int h = int(Clampf(pid_.rel_db[a], -12.f, 12.f) * float(sc));
        if(pid_.level_db > -55.f)
            FillRect(x, h > 0 ? cy - h : cy, w, abs(h) + 1, Pearl(float(a) / 6.f));
        const int t = cy - int(Clampf(pid_.target_db[a], -12.f, 12.f) * float(sc));
        FillRect(x - 2, t, w + 4, 2, kCream);
        TextFb(x + (w - TextW(f[a], Font_6x8)) / 2, kGy + kGh - 10, f[a], Font_6x8, kDimText);
    }
    if(pid_.level_db <= -55.f)
        TextFb(kGx + 26, cy - 20, "waiting for sound", Font_6x8, kYellow);
    // the groups
    static const char* const g[PidStage::kG] = {"eq", "comp", "mband", "clarity", "deharsh", "width"};
    float big = 0.f;
    for(int b = 0; b < 4; b++)
        big = fabsf(pid_.eq_db[b]) > fabsf(big) ? pid_.eq_db[b] : big;
    float mb = 0.f;
    for(int b = 0; b < 4; b++)
        mb += pid_.mb_amt[b] * 25.f;
    const float clar = fabsf(pid_.lo_db) > fabsf(pid_.pres_db) ? pid_.lo_db : pid_.pres_db;
    const float v[PidStage::kG]    = {big, -pid_.comp_db, mb, clar, pid_.dh_db, pid_.width_add * 100.f};
    const float span[PidStage::kG] = {6.f, 8.f, 50.f, 4.f, 6.f, 40.f};
    const bool  pct[PidStage::kG]  = {false, false, true, false, false, true};
    const int   gx = kGx + 166, mask = params_[P_PID_MASK].value, cur = params_[P_PID_GROUP].value;
    VLineDots(gx - 6, kGy + 2, kGy + kGh - 3, 2, kGrid);
    for(int i = 0; i < PidStage::kG; i++)
    {
        const int  y  = kGy + 6 + i * 21;
        const bool st = (mask >> i) & 1;
        TextFb(gx, y, g[i], Font_6x8, i == cur ? kYellow : (st ? kCream : kDimText));
        const int bx = gx + 50, bw = 60, mid = bx + bw / 2;
        FillRect(bx, y, bw, 8, kBlue);
        const int d = int(Clampf(v[i] / span[i], -1.f, 1.f) * float(bw / 2));
        if(st)
            FillRect(d < 0 ? mid + d : mid, y, abs(d) + 1, 8, Pearl(float(i) / 6.f));
        FillRect(mid, y - 2, 1, 12, kWhite);
        char b[12];
        F1(b, 12, v[i], true);
        if(pct[i])
            snprintf(b, sizeof(b), "%+d%%", Di(v[i]));
        TextFb(bx + bw + 4, y, st ? b : "off", Font_6x8, st ? kDimText : kGrid);
    }
}
} // namespace pg
