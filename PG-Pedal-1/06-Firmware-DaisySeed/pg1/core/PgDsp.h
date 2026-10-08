// PG-1 DSP building blocks and the 7 processing stages. Plain C++, no hardware, no allocation:
// the same code runs on the Seed3 (inside the audio interrupt) and in the Carla plugin.
//
// Signal flow (one Core::Process call):
//   in -> hum -> input level -> dyn eq -> comp -> multiband -> clarity -> saturate -> de-harsh -> width -> takeback
//      -> loudness -> anti-duck -> [bypass] -> safety -> out
// (the pid tab only listens after loudness and steers the other stages' settings)
// Safety is after the bypass switch: it is always on.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

namespace pg
{
constexpr float kPi = 3.14159265f;

inline float Clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float Smooth01(float x)
{
    x = Clampf(x, 0.f, 1.f);
    return x * x * (3.f - 2.f * x);
}
inline float DbToLin(float db) { return expf(db * 0.11512925f); }
inline float PowToDb(float p) { return 10.f * log10f(p + 1e-20f); }
// Fast log2 / exp2 (fitted polynomials; errors ~0.0001 dB) for levels and gains worked out on every sample:
// on the Seed3's Cortex-M7 the library expf / log10f cost several times more, and they ran ~25 times per sample.
inline float FastLog2(float x)
{
    if(!(x > 1e-30f)) // (also catches NaN)
        x = 1e-30f;
    uint32_t i;
    memcpy(&i, &x, 4);
    const float e = float(int(i >> 23) - 127);
    i             = (i & 0x007FFFFFu) | 0x3F800000u;
    float m;
    memcpy(&m, &i, 4);
    return e + ((((0.0434289078f * m - 0.404867174f) * m + 1.59390136f) * m - 3.49249428f) * m + 5.04687604f) * m - 2.78681295f;
}
inline float FastExp2(float x)
{
    x              = Clampf(x, -126.f, 126.f);
    const float fl = floorf(x), f = x - fl;
    const float p  = ((((0.00189437942f * f + 0.00894058253f) * f + 0.0558765569f) * f + 0.240131692f) * f + 0.693156777f) * f + 0.99999977f;
    const uint32_t i = uint32_t(int(fl) + 127) << 23;
    float          sc;
    memcpy(&sc, &i, 4);
    return p * sc;
}
inline float FastPowToDb(float p) { return 3.01029996f * FastLog2(p + 1e-20f); }
inline float FastDbToLin(float db) { return FastExp2(db * 0.166096405f); }
// one-pole smoothing coefficient for a time constant `ms` at a update rate `rate_hz`
inline float Coef(float ms, float rate_hz) { return 1.f - expf(-1000.f / (ms * rate_hz)); }
// envelope step: towards `target` with `up` when rising, `down` when falling
inline void Follow(float& v, float target, float up, float down) { v += (target - v) * (target > v ? up : down); }

// Topology-preserving state-variable filter (Simper). Stable at every frequency and Q, cheap to
// retune per block, and its magnitude response can be drawn exactly (MagDbT).
struct Svf
{
    enum Type
    {
        LP,
        HP,
        BP, // unity gain at the centre
        AP, // all-pass: flat level, phase only (keeps band splits in step)
        BELL,
        LSHELF,
        HSHELF
    };
    float g = 0.1f, k = 1.f, a1 = 1.f, a2 = 0.f, a3 = 0.f, m0 = 1.f, m1 = 0.f, m2 = 0.f;
    float ic1[2] = {}, ic2[2] = {};

    void Set(Type t, float fs, float f, float q, float db = 0.f)
    {
        f              = Clampf(f, 5.f, fs * 0.47f);
        q              = Clampf(q, 0.1f, 200.f);
        const float A  = t >= BELL ? expf(db * 0.05756463f) : 1.f; // 10^(db/40)
        float       gg = tanf(kPi * f / fs);
        switch(t)
        {
            case LP: k = 1.f / q, m0 = 0.f, m1 = 0.f, m2 = 1.f; break;
            case HP: k = 1.f / q, m0 = 1.f, m1 = -k, m2 = -1.f; break;
            case BP: k = 1.f / q, m0 = 0.f, m1 = k, m2 = 0.f; break;
            case AP: k = 1.f / q, m0 = 1.f, m1 = -2.f * k, m2 = 0.f; break;
            case BELL: k = 1.f / (q * A), m0 = 1.f, m1 = k * (A * A - 1.f), m2 = 0.f; break;
            case LSHELF:
                gg /= sqrtf(A);
                k = 1.f / q, m0 = 1.f, m1 = k * (A - 1.f), m2 = A * A - 1.f;
                break;
            case HSHELF:
                gg *= sqrtf(A);
                k = 1.f / q, m0 = A * A, m1 = k * (1.f - A) * A, m2 = 1.f - A * A;
                break;
        }
        g  = gg;
        a1 = 1.f / (1.f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    inline float Run(int ch, float v0)
    {
        const float v3 = v0 - ic2[ch];
        const float v1 = a1 * ic1[ch] + a2 * v3;
        const float v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;
        ic1[ch]        = 2.f * v1 - ic1[ch];
        ic2[ch]        = 2.f * v2 - ic2[ch];
        return m0 * v0 + m1 * v1 + m2 * v2;
    }
    // the same filter when only one output is wanted (set up as LP / HP / BP): fewer loads and multiplies,
    // which matters on the Seed3 where these run ~60 times per sample
    inline float Lp(int ch, float v0)
    {
        const float v3 = v0 - ic2[ch], v1 = a1 * ic1[ch] + a2 * v3, v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;
        ic1[ch] = 2.f * v1 - ic1[ch], ic2[ch] = 2.f * v2 - ic2[ch];
        return v2;
    }
    inline float Hp(int ch, float v0)
    {
        const float v3 = v0 - ic2[ch], v1 = a1 * ic1[ch] + a2 * v3, v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;
        ic1[ch] = 2.f * v1 - ic1[ch], ic2[ch] = 2.f * v2 - ic2[ch];
        return v0 - k * v1 - v2;
    }
    inline float Bp(int ch, float v0)
    {
        const float v3 = v0 - ic2[ch], v1 = a1 * ic1[ch] + a2 * v3, v2 = ic2[ch] + a2 * ic1[ch] + a3 * v3;
        ic1[ch] = 2.f * v1 - ic1[ch], ic2[ch] = 2.f * v2 - ic2[ch];
        return k * v1;
    }
    void Reset() { ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0.f; }
    // |H| in dB, where t = tan(pi * hz / fs) (shared by every filter at that frequency)
    float MagDbT(float t) const
    {
        const float W = t / g, dre = 1.f - W * W, dim = k * W;
        const float nre = m0 * dre + m2, nim = (m0 * k + m1) * W;
        return 10.f * log10f((nre * nre + nim * nim + 1e-20f) / (dre * dre + dim * dim + 1e-20f));
    }
};

// Anti-duck weighting for level detectors: a steep (4th-order) high-pass, so the bass hardly counts.
// Every detector that can turn the WHOLE sound down listens through this, so a bass-heavy moment is never
// mistaken for "too loud" and the rest of the track never dips under it (150 Hz: a 50 Hz note counts
// ~38 dB less, an 80 Hz one ~22 dB less; 300 Hz for the anti-duck guard's "rest of the track").
struct DuckWeight
{
    Svf  a, b;
    void Init(float fs, float hp_hz)
    {
        a.Set(Svf::HP, fs, hp_hz, 0.5412f), b.Set(Svf::HP, fs, hp_hz, 1.3066f); // Butterworth
        a.Reset(), b.Reset();
    }
    inline float Run(int ch, float x) { return b.Hp(ch, a.Hp(ch, x)); }
};

// ------------------------------------------------------------------ 0. input auto-level
// Brings any source to the same working level: quiet ones up (at most `boost`), hot ones down (at
// most `cut`). Slow and gated, so it rides the level like a careful engineer and never pumps up
// silence or noise. The 119 dB converter in the Seed3 has the headroom to do this digitally.
struct LevelStage
{
    DuckWeight kw; // anti-duck: bass-heavy passages don't read as "hot"
    float p = 0.f, gain_db = 0.f, cur = 1.f, in_db = -120.f, want_lin = 1.f;
    float c_pow = 0.f, c_gain = 0.f, fs = 48000.f;

    void Init(float sr)
    {
        fs = sr;
        p = 0.f, gain_db = 0.f, cur = 1.f, in_db = -120.f;
        c_pow = Coef(400.f, fs), c_gain = Coef(20.f, fs);
        kw.Init(fs, 150.f);
    }
    // target dBFS rms, boost / cut limits in dB, speed 0..1
    void Update(float target, float boost, float cut, float speed, float block_s)
    {
        in_db = PowToDb(p);
        if(in_db > -60.f) // below that it's silence or noise: hold the gain where it is
        {
            const float want = Clampf(target - in_db, -cut, boost);
            const float up = (1.f + 5.f * speed) * block_s, down = (3.f + 12.f * speed) * block_s; // dB per block
            gain_db += Clampf(want - gain_db, -down, up);
        }
        gain_db  = Clampf(gain_db, -cut, boost);
        want_lin = DbToLin(gain_db); // (once per block, not per sample)
    }
    inline void Run(float& l, float& r)
    {
        const float w = kw.Run(0, 0.5f * (l + r));
        p += (w * w - p) * c_pow;
        cur += (want_lin - cur) * c_gain;
        l *= cur, r *= cur;
    }
};

// ------------------------------------------------------------------ 1. hum + hiss
// Learned noise profile (from the hum tab's "learn"): the exact tones to notch out and the hiss floor in
// 8 bands. Filled on the UI thread, then `valid` is set; the audio thread only reads it.
struct NoiseProfile
{
    static constexpr int kTones = 8, kBands = 8;
    int   n_tones = 0;
    float tone_hz[kTones] = {}, tone_db[kTones] = {}; // frequency, how far it stood above the floor
    float floor_db[kBands] = {};                       // hiss floor per band (detector level, dBFS)
    bool  valid = false;
};
constexpr float kHissBandHz[NoiseProfile::kBands] = {250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f, 12000.f, 16000.f};

struct HumStage
{
    static constexpr int kH = 6; // fundamental + 5 harmonics
    Svf   notch[kH], narrow[kH], broad[kH], det[4];
    // the hum detectors only look below ~400 Hz, so they run on a low-passed 1/8-rate copy (6 kHz)
    static constexpr int kDec = 8;
    Svf   aa[2];
    float acc_m = 0.f, fs_d = 6000.f;
    int   dec_n = 0;
    bool  notch_on[kH] = {}, hiss_on = false;
    float en[kH] = {}, eb[kH] = {}, e_det[4] = {};
    float depth_db[kH] = {}, applied[kH] = {};
    float f0 = 60.f;
    Svf   hiss_hp, hiss_shelf;
    float hiss_env = 0.f, hiss_db = 0.f, hiss_applied = 1000.f, hf_level_db = -120.f;
    float fs = 48000.f, c_det = 0.f, c_hiss_up = 0.f, c_hiss_dn = 0.f;
    // learned mode: notches at the learned tones + an 8-band gate down to the learned hiss floor
    NoiseProfile prof;
    bool         use_learned = false, applied_learned = false;
    float        applied_depth = 0.f;
    Svf          ln[NoiseProfile::kTones], hb_det[NoiseProfile::kBands], hb_bell[NoiseProfile::kBands];
    float        hb_env[NoiseProfile::kBands] = {}, hb_cut[NoiseProfile::kBands] = {}, hb_ap[NoiseProfile::kBands] = {};
    float        hb_level[NoiseProfile::kBands] = {};
    // while learning: the band detector levels are averaged here (audio thread writes, UI reads after)
    volatile bool learning = false;
    float         learn_acc[NoiseProfile::kBands] = {}, learn_lvl = 0.f, learn_lvl2 = 0.f;
    volatile int  learn_n = 0;
    float         c_hb_up = 0.f, c_hb_dn = 0.f;

    void ApplyProfile(float depth_max)
    {
        for(int t = 0; t < NoiseProfile::kTones; t++)
        {
            const float f = prof.tone_hz[t], q = Clampf(f / (0.8f + f * 0.002f), 5.f, 150.f); // ~1 Hz wide at 60 Hz
            const float d = t < prof.n_tones ? -fminf(depth_max, prof.tone_db[t] + 10.f) : 0.f; // all the way down
            ln[t].Set(Svf::BELL, fs, t < prof.n_tones ? f : 1000.f, q, d);
        }
        applied_learned = true, applied_depth = depth_max;
    }

    float NotchQ(float f) const { return Clampf(f / 4.f, 10.f, 60.f); }
    void  Init(float sr)
    {
        fs = sr, fs_d = sr / kDec;
        for(Svf& f : aa)
            f.Set(Svf::LP, fs, 1000.f, 0.7071f), f.Reset();
        acc_m = 0.f, dec_n = 0, hiss_on = false;
        static const float dets[4] = {50.f, 100.f, 60.f, 120.f};
        for(int i = 0; i < 4; i++)
            det[i].Set(Svf::BP, fs_d, dets[i], 12.f), det[i].Reset(), e_det[i] = 0.f;
        Retune(f0);
        hiss_hp.Set(Svf::HP, fs, 6000.f, 0.707f), hiss_hp.Reset();
        hiss_shelf.Set(Svf::HSHELF, fs, 5000.f, 0.7f, 0.f), hiss_shelf.Reset();
        hiss_env = 0.f, hiss_db = 0.f, hiss_applied = 0.f;
        c_det     = Coef(60.f, fs_d);
        for(int b = 0; b < NoiseProfile::kBands; b++)
        {
            hb_det[b].Set(Svf::BP, fs, kHissBandHz[b], 1.4f), hb_det[b].Reset();
            hb_bell[b].Set(Svf::BELL, fs, kHissBandHz[b], 1.8f, 0.f), hb_bell[b].Reset();
            hb_env[b] = hb_cut[b] = hb_ap[b] = 0.f;
        }
        for(Svf& f : ln)
            f.Set(Svf::BELL, fs, 1000.f, 10.f, 0.f), f.Reset();
        applied_learned = false;
        c_hb_up = Coef(3.f, fs), c_hb_dn = Coef(150.f, fs);
        c_hiss_up = Coef(2.f, fs), c_hiss_dn = Coef(60.f, fs);
    }
    void Retune(float f)
    {
        f0 = f;
        for(int h = 0; h < kH; h++)
        {
            const float fh = f0 * float(h + 1);
            notch[h].Set(Svf::BELL, fs, fh, NotchQ(fh), 0.f), notch[h].Reset(), notch_on[h] = false;
            narrow[h].Set(Svf::BP, fs_d, fh, NotchQ(fh)), narrow[h].Reset();
            broad[h].Set(Svf::BP, fs_d, fh, 2.f), broad[h].Reset();
            en[h] = eb[h] = 0.f, depth_db[h] = 0.f, applied[h] = 0.f;
        }
    }
    // once per block (block_s seconds)
    void Update(int mains, float depth_max, float hiss_cut, float hiss_thr, float block_s, float margin = 4.f)
    {
        // learning: average each band's level (dB) and the overall level (for "was it steady?")
        if(learning)
        {
            float tot = 0.f;
            for(int b = 0; b < NoiseProfile::kBands; b++)
                learn_acc[b] += PowToDb(hb_env[b]) + 3.f, tot += hb_env[b];
            const float l = PowToDb(tot);
            learn_lvl += l, learn_lvl2 += l * l;
            learn_n = learn_n + 1;
        }
        use_learned = mains == 3 && prof.valid;
        if(use_learned)
        {
            if(!applied_learned || fabsf(depth_max - applied_depth) > 0.5f)
                ApplyProfile(depth_max);
            const float up = 1.f - expf(-block_s / 0.003f), dn = 1.f - expf(-block_s / 0.15f);
            for(int b = 0; b < NoiseProfile::kBands; b++)
            {
                // a band only opens when something is clearly louder than its learned hiss floor
                hb_level[b]       = PowToDb(hb_env[b]) + 3.f;
                const float open  = Smooth01((hb_level[b] - (prof.floor_db[b] + margin)) / 6.f);
                const float target = -hiss_cut * (1.f - open);
                hb_cut[b] += (target - hb_cut[b]) * (target > hb_cut[b] ? up : dn);
                if(fabsf(hb_cut[b] - hb_ap[b]) > 0.1f)
                    hb_bell[b].Set(Svf::BELL, fs, kHissBandHz[b], 1.8f, hb_cut[b]), hb_ap[b] = hb_cut[b];
            }
            return;
        }
        applied_learned = false;
        float want = f0;
        if(mains == 1)
            want = 50.f;
        else if(mains == 2)
            want = 60.f;
        else // auto: which family is louder, 50/100 or 60/120 (with hysteresis)
        {
            const float p50 = e_det[0] + e_det[1], p60 = e_det[2] + e_det[3];
            if(p50 > 1e-11f && p50 > 2.f * p60)
                want = 50.f;
            else if(p60 > 1e-11f && p60 > 2.f * p50)
                want = 60.f;
        }
        if(want != f0)
            Retune(want);
        const float up = 1.f - expf(-block_s / 0.3f), dn = 1.f - expf(-block_s / 1.5f);
        for(int h = 0; h < kH; h++)
        {
            // hum is a steady tone: nearly all of the energy around fh sits in the narrow band
            const float r      = en[h] / (eb[h] + 1e-20f);
            const float tonal  = en[h] > 1e-9f ? Smooth01((r - 0.3f) / 0.4f) : 0.f;
            const float target = -depth_max * tonal;
            Follow(depth_db[h], target, dn, up); // in over 0.3 s, back out over 1.5 s
            if(fabsf(depth_db[h] - applied[h]) > 0.1f)
            {
                const float fh = f0 * float(h + 1);
                notch[h].Set(Svf::BELL, fs, fh, NotchQ(fh), depth_db[h]);
                applied[h] = depth_db[h];
            }
        }
        // hiss: when the top end is only hiss (below the threshold), turn it down; open fast for music
        hf_level_db         = PowToDb(hiss_env) + 3.f;
        const float below   = hiss_thr - hf_level_db;
        const float target  = -hiss_cut * Smooth01(below / 12.f);
        const float c_close = 1.f - expf(-block_s / 0.2f), c_open = 1.f - expf(-block_s / 0.005f);
        hiss_db += (target - hiss_db) * (target < hiss_db ? c_close : c_open);
        if(fabsf(hiss_db - hiss_applied) > 0.1f)
            hiss_shelf.Set(Svf::HSHELF, fs, 5000.f, 0.7f, hiss_db), hiss_applied = hiss_db;
    }
    inline void Run(float& l, float& r)
    {
        const float m = 0.5f * (l + r);
        if(learning || use_learned) // band levels: for learning and the learned gate
            for(int b = 0; b < NoiseProfile::kBands; b++)
            {
                const float d = hb_det[b].Bp(0, m), sq = d * d;
                hb_env[b] += (sq - hb_env[b]) * (sq > hb_env[b] ? c_hb_up : c_hb_dn);
            }
        if(use_learned)
        {
            for(int t = 0; t < prof.n_tones; t++)
                l = ln[t].Run(0, l), r = ln[t].Run(1, r);
            for(int b = 0; b < NoiseProfile::kBands; b++)
                l = hb_bell[b].Run(0, l), r = hb_bell[b].Run(1, r);
            return;
        }
        acc_m += aa[1].Lp(0, aa[0].Lp(0, m));
        if(++dec_n == kDec) // the hum detectors, at 6 kHz
        {
            const float md = acc_m / kDec;
            acc_m = 0.f, dec_n = 0;
            for(int i = 0; i < 4; i++)
            {
                const float d = det[i].Bp(0, md);
                e_det[i] += (d * d - e_det[i]) * c_det;
            }
            for(int h = 0; h < kH; h++)
            {
                const float n = narrow[h].Bp(0, md), b = broad[h].Bp(0, md);
                en[h] += (n * n - en[h]) * c_det;
                eb[h] += (b * b - eb[h]) * c_det;
            }
        }
        for(int h = 0; h < kH; h++) // a notch with no depth is a plain wire: skipped
        {
            const bool on = fabsf(applied[h]) > 0.05f;
            if(on && !notch_on[h])
                notch[h].Reset();
            notch_on[h] = on;
            if(on)
                l = notch[h].Run(0, l), r = notch[h].Run(1, r);
        }
        const float hf = hiss_hp.Hp(0, m), p = hf * hf;
        hiss_env += (p - hiss_env) * (p > hiss_env ? c_hiss_up : c_hiss_dn);
        const bool hs = fabsf(hiss_applied) > 0.05f;
        if(hs && !hiss_on)
            hiss_shelf.Reset();
        hiss_on = hs;
        if(hs)
            l = hiss_shelf.Run(0, l), r = hiss_shelf.Run(1, r);
    }
};

// ------------------------------------------------------------------ 2. dynamic eq
struct DynEqStage
{
    static constexpr int kB = 4;
    Svf   peak[kB], detect[kB];
    float env[kB] = {}, dyn_db[kB] = {}, applied[kB] = {}, cached_f[kB] = {}, cached_q[kB] = {};
    bool  bell_on[kB] = {};
    float level_db[kB] = {-120.f, -120.f, -120.f, -120.f}; // detector level (for the display)
    float fs = 48000.f, att = 0.f, rel = 0.f;

    void Init(float sr)
    {
        fs = sr;
        for(int b = 0; b < kB; b++)
            peak[b].Reset(), detect[b].Reset(), env[b] = 0.f, dyn_db[b] = 0.f, applied[b] = 1000.f, cached_f[b] = 0.f;
        att = Coef(3.f, fs), rel = Coef(120.f, fs);
    }
    // per band: f (Hz), q, gain (dB), threshold (dBFS), range (dB, - = cut when loud)
    void Update(int b, float f, float q, float gain, float thr, float range, float block_s)
    {
        f = fminf(f, fs * 0.45f);
        if(f != cached_f[b] || q != cached_q[b])
        {
            detect[b].Set(Svf::BP, fs, f, q);
            cached_f[b] = f, cached_q[b] = q, applied[b] = 1000.f;
        }
        level_db[b]       = PowToDb(env[b]) + 3.f; // mean square -> sine peak dBFS
        const float over  = level_db[b] - thr;
        float       target = 0.f;
        if(over > 0.f) // 3:1 above the threshold, never more than the range
        {
            const float k = over * (2.f / 3.f);
            target        = range < 0.f ? fmaxf(range, -k) : fminf(range, k);
        }
        const float tau = fabsf(target) > fabsf(dyn_db[b]) ? 0.005f : 0.150f;
        dyn_db[b] += (target - dyn_db[b]) * (1.f - expf(-block_s / tau));
        const float g = gain + dyn_db[b];
        if(fabsf(g - applied[b]) > 0.02f)
            peak[b].Set(Svf::BELL, fs, f, q, g), applied[b] = g;
    }
    inline void Run(float& l, float& r)
    {
        const float il = l, ir = r;
        for(int b = 0; b < kB; b++)
        {
            const float dl = detect[b].Bp(0, il), dr = detect[b].Bp(1, ir);
            const float sq = fmaxf(dl * dl, dr * dr);
            env[b] += (sq - env[b]) * (sq > env[b] ? att : rel);
            const bool on = fabsf(applied[b]) > 0.02f && applied[b] < 999.f; // a 0 dB band is a plain wire: skipped
            if(on && !bell_on[b])
                peak[b].Reset();
            bell_on[b] = on;
            if(on)
                l = peak[b].Run(0, l), r = peak[b].Run(1, r);
        }
    }
};

// ------------------------------------------------------------------ 3. compressor
struct CompStage
{
    DuckWeight sc;            // sidechain (anti-duck): bass hits barely drive it, so it never pumps the track down
    float p = 0.f;            // detector power
    float env_fast = 0.f, env_slow = 0.f, gr_db = 0.f, in_db = -120.f;
    float thr = -18.f, ratio = 2.5f, makeup = 0.f;
    float c_det = 0.f, c_att = 0.f, c_rel = 0.f, c_slow_att = 0.f, c_slow_rel = 0.f;
    bool  auto_rel = true;
    float fs = 48000.f;

    void Init(float sr)
    {
        fs = sr;
        sc.Init(fs, 150.f);
        p = 0.f, env_fast = env_slow = gr_db = 0.f;
        c_det      = Coef(5.f, fs);
        c_slow_att = Coef(400.f, fs), c_slow_rel = Coef(1200.f, fs);
    }
    void Update(float threshold, float rat, float attack_ms, float release_ms /* 0 = auto */)
    {
        thr = threshold, ratio = rat;
        c_att    = Coef(attack_ms, fs);
        auto_rel = release_ms <= 0.f;
        c_rel    = Coef(auto_rel ? 60.f : release_ms, fs);
        makeup   = -thr * (1.f - 1.f / ratio) * 0.35f; // gentle automatic make-up
    }
    float Curve(float level) const // static gain change in dB (soft 6 dB knee)
    {
        const float over = level - thr, w = 6.f, s = 1.f / ratio - 1.f;
        if(over <= -w * 0.5f)
            return 0.f;
        if(over >= w * 0.5f)
            return over * s;
        const float t = over + w * 0.5f;
        return s * t * t / (2.f * w);
    }
    inline void Run(float& l, float& r)
    {
        const float hl = sc.Run(0, l), hr = sc.Run(1, r);
        const float sq = fmaxf(hl * hl, hr * hr);
        p += (sq - p) * c_det;
        in_db          = FastPowToDb(p) + 3.f;
        const float gr = Curve(in_db);
        Follow(env_fast, gr, c_rel, c_att); // gr is negative: "falling" = more reduction = attack
        float g = env_fast;
        if(auto_rel) // program-dependent: long squeezes recover slowly, single hits quickly
        {
            Follow(env_slow, gr, c_slow_rel, c_slow_att);
            g = fminf(env_fast, env_slow);
        }
        gr_db         = g;
        const float k = FastDbToLin(g + makeup);
        l *= k, r *= k;
    }
};

// ------------------------------------------------------------------ 3b. multiband dynamics
// Four bands (split at 120 Hz, 1 kHz, 6 kHz with Linkwitz-Riley crossovers plus all-pass matching, so the
// untouched bands add back up to the original exactly). Each band has its own compressor that works
// relative to that band's own recent level, so it behaves the same at any input level: it holds back the
// band's peaks (a boomy note, a harsh hit) and leaves its body alone. Knobs = how firmly each band is held.
struct MultibandStage
{
    static constexpr int kB = 4;
    struct Lr4 // Linkwitz-Riley 4th order: two Butterworth sections, low and high outputs
    {
        Svf  lp[2], hp[2];
        void Set(float fs, float f)
        {
            for(int i = 0; i < 2; i++)
                lp[i].Set(Svf::LP, fs, f, 0.70710678f), hp[i].Set(Svf::HP, fs, f, 0.70710678f), lp[i].Reset(), hp[i].Reset();
        }
        inline void Run(int ch, float x, float& lo, float& hi)
        {
            lo = lp[1].Lp(ch, lp[0].Lp(ch, x));
            hi = hp[1].Hp(ch, hp[0].Hp(ch, x));
        }
    };
    Lr4   x_mid, x_low, x_high;  // 1 kHz, then 120 Hz (low side) and 6 kHz (high side)
    Svf   ap_low, ap_high;       // the low side gets the 6 kHz phase, the high side the 120 Hz phase
    float p[kB] = {}, avg[kB] = {}, gr[kB] = {}, mk[kB] = {}, g[kB] = {1.f, 1.f, 1.f, 1.f}, amt[kB] = {};
    float c_det[kB] = {}, c_att[kB] = {}, c_rel[kB] = {}, c_avg = 0.f, c_warm = 0.f, c_g = 0.f, fs = 48000.f;
    int   warm[kB] = {}, warm_n = 9600;
    static constexpr int kGrEvery = 8; // the gain maths every 8 samples (the level is still followed every sample)
    int   gr_n = 0;
    float g_want[kB] = {1.f, 1.f, 1.f, 1.f};
    float level_db[kB] = {-120.f, -120.f, -120.f, -120.f};
    static float Xover(int i) { return i == 0 ? 120.f : (i == 1 ? 1000.f : 6000.f); }

    void Init(float sr)
    {
        fs = sr;
        x_mid.Set(fs, 1000.f), x_low.Set(fs, 120.f), x_high.Set(fs, 6000.f);
        ap_low.Set(Svf::AP, fs, 6000.f, 0.70710678f), ap_high.Set(Svf::AP, fs, 120.f, 0.70710678f);
        ap_low.Reset(), ap_high.Reset();
        static const float att_ms[kB] = {30.f, 15.f, 8.f, 3.f}, rel_ms[kB] = {220.f, 150.f, 100.f, 60.f}, det_ms[kB] = {20.f, 10.f, 5.f, 3.f};
        for(int b = 0; b < kB; b++)
        {
            p[b] = avg[b] = 0.f, gr[b] = mk[b] = 0.f, g[b] = 1.f, warm[b] = 0;
            c_det[b] = Coef(det_ms[b], fs), c_att[b] = Coef(att_ms[b], fs / kGrEvery), c_rel[b] = Coef(rel_ms[b], fs / kGrEvery); // (gain maths every 8 samples)
        }
        c_avg = Coef(3000.f, fs), c_warm = Coef(60.f, fs), c_g = Coef(2.f, fs);
        warm_n = int(0.2f * fs);
    }
    // amounts 0..1 per band (bass, low mids, high mids, highs)
    void Update(const float a[kB], float block_s)
    {
        for(int b = 0; b < kB; b++)
        {
            amt[b]      = a[b];
            level_db[b] = PowToDb(p[b]) + 3.f;
            // make-up: give back about half of what the band has been losing on average, so its tone holds
            mk[b] += (-0.5f * gr[b] - mk[b]) * (1.f - expf(-block_s / 2.f));
        }
    }
    inline void Run(float& l, float& r)
    {
        float band[2][kB];
        const float in[2] = {l, r};
        for(int ch = 0; ch < 2; ch++)
        {
            float lo, hi, b0, b1, b2, b3;
            x_mid.Run(ch, in[ch], lo, hi);
            x_low.Run(ch, ap_low.Run(ch, lo), b0, b1);
            x_high.Run(ch, ap_high.Run(ch, hi), b2, b3);
            band[ch][0] = b0, band[ch][1] = b1, band[ch][2] = b2, band[ch][3] = b3;
        }
        float ol = 0.f, orr = 0.f;
        for(int b = 0; b < kB; b++)
        {
            const float sq = fmaxf(band[0][b] * band[0][b], band[1][b] * band[1][b]);
            p[b] += (sq - p[b]) * c_det[b];
            if(p[b] > 1e-10f) // the band's own recent level (only while something plays; catches up fast at first)
            {
                avg[b] += (p[b] - avg[b]) * (warm[b] < warm_n ? c_warm : c_avg);
                warm[b] += warm[b] < warm_n ? 1 : 0;
            }
            if(gr_n == 0)
            {
                if(amt[b] > 0.001f)
                {
                    // above (its average + 8 dB, down to + 2 dB at full amount): ratio 1.5 .. 5
                    const float over   = FastPowToDb(p[b]) - (FastPowToDb(avg[b]) + 8.f - 6.f * amt[b]);
                    const float target = over > 0.f ? -over * (1.f - 1.f / (1.5f + 3.5f * amt[b])) : 0.f;
                    gr[b] += (target - gr[b]) * (target < gr[b] ? c_att[b] : c_rel[b]);
                }
                else
                    gr[b] += (0.f - gr[b]) * c_rel[b];
                g_want[b] = FastDbToLin(gr[b] + mk[b]);
            }
            g[b] += (g_want[b] - g[b]) * c_g;
            ol += band[0][b] * g[b], orr += band[1][b] * g[b];
        }
        gr_n = (gr_n + 1) % kGrEvery;
        l = ol, r = orr;
    }
};

// ------------------------------------------------------------------ 4. clarity + bass
// Bass: thump (a short low lift on each hit), detail (the bass's own harmonics, heard on small speakers)
// and warmth (a steady low shelf). Clarity: up to 10 bands that find their own spots. 20 detectors listen
// half an octave apart; where the sound bulges above a smooth balance a band moves there and cuts, where it
// sags a band moves there and lifts. Each band glides its own frequency, width (q) and gain, so it follows
// the music instead of jumping. Modes: dynamic (cuts and lifts), add (only lifts), normalise (stronger,
// slower, towards an even balance).
struct ClarityStage
{
    static constexpr int kMaxBands = 10, kD = 20;
    struct Band
    {
        Svf   f;
        float hz = 1000.f, q = 1.f, db = 0.f;      // where it is now
        float t_hz = 1000.f, t_q = 1.f, t_db = 0.f; // where it is heading
        float ap_hz = 0.f, ap_q = 0.f, ap_db = 0.f; // what the filter is set to
        bool  live = false;
    };
    Svf   d_low, thump, warm_lo, det_lp, det_hp, det_out;
    float low_fast = 0.f, low_slow = 0.f, p_full = 0.f, bass_pk = 0.f;
    float thump_db = 0.f, warm_lo_db = 0.f, ap_thump = 1000.f, ap_warm = 1000.f;
    float detail = 0.f, p_harm = 0.f, detail_view_db = 0.f;
    Svf   det[kD];
    float det_hz[kD] = {}, p_det[kD] = {};
    // the 12 lowest detectors (up to ~1.3 kHz) run on a low-passed 1/8-rate copy (6 kHz); the rest at full rate
    static constexpr int kLowDet = 12, kDec = 8;
    Svf   aa[2];
    float acc_m = 0.f, c_det_d = 0.f;
    int   dec_n = 0;
    float dev_db[kD] = {}; // what it hears: each spot above (+) or below (-) the smooth balance (for the screen)
    Band  band[kMaxBands];
    int   mode = 0, n_bands = 6, live_n = 0;
    float max_db = 2.f, since = 0.f, cut_most = 0.f, lift_most = 0.f;
    float c_lf_up = 0.f, c_lf_dn = 0.f, c_ls_up = 0.f, c_ls_dn = 0.f, c_avg = 0.f, c_pk_dn = 0.f, c_det = 0.f;
    float fs = 48000.f;

    void Init(float sr)
    {
        fs = sr;
        d_low.Set(Svf::LP, fs, 100.f, 0.707f);
        det_lp.Set(Svf::LP, fs, 120.f, 0.707f), det_hp.Set(Svf::HP, fs, 90.f, 0.707f), det_out.Set(Svf::LP, fs, 1500.f, 0.707f);
        Svf* all[] = {&d_low, &thump, &warm_lo, &det_lp, &det_hp, &det_out};
        for(Svf* f : all)
            f->Reset();
        thump_db = warm_lo_db = 0.f, ap_thump = ap_warm = 1000.f;
        low_fast = low_slow = p_full = bass_pk = 0.f;
        p_harm = 0.f, detail_view_db = 0.f;
        for(int i = 0; i < kD; i++) // 40 Hz .. 16 kHz, ~0.45 octave apart
        {
            det_hz[i] = 40.f * powf(400.f, float(i) / float(kD - 1));
            det[i].Set(Svf::BP, i < kLowDet ? fs / kDec : fs, det_hz[i], 2.2f), det[i].Reset();
            p_det[i] = 0.f, dev_db[i] = 0.f;
        }
        for(Svf& f : aa)
            f.Set(Svf::LP, fs, 2000.f, 0.7071f), f.Reset();
        acc_m = 0.f, dec_n = 0;
        for(Band& b : band)
            b = Band{}, b.f.Set(Svf::BELL, fs, 1000.f, 1.f, 0.f), b.f.Reset();
        live_n = 0, since = 0.f, cut_most = lift_most = 0.f;
        c_lf_up = Coef(2.f, fs), c_lf_dn = Coef(40.f, fs);
        c_ls_up = Coef(150.f, fs), c_ls_dn = Coef(300.f, fs);
        c_avg = Coef(150.f, fs), c_pk_dn = Coef(60.f, fs), c_det = Coef(200.f, fs), c_det_d = Coef(200.f, fs / kDec);
        Apply(true);
    }
    void Apply(bool force)
    {
        if(force || fabsf(thump_db - ap_thump) > 0.05f)
            thump.Set(Svf::LSHELF, fs, 90.f, 0.7f, thump_db), ap_thump = thump_db;
        if(force || fabsf(warm_lo_db - ap_warm) > 0.05f)
            warm_lo.Set(Svf::LSHELF, fs, 220.f, 0.7f, warm_lo_db), ap_warm = warm_lo_db;
        for(Band& b : band)
            if(force || fabsf(b.db - b.ap_db) > 0.05f || fabsf(b.hz - b.ap_hz) > b.ap_hz * 0.004f || fabsf(b.q - b.ap_q) > b.ap_q * 0.01f)
                b.f.Set(Svf::BELL, fs, b.hz, b.q, b.db), b.ap_hz = b.hz, b.ap_q = b.q, b.ap_db = b.db;
    }
    static float Oct(float hz) { return log2f(hz / 1000.f); }
    // the band search: find the bulges and dips, hand them to bands (each band keeps the spot nearest to it)
    void Search()
    {
        const float full = PowToDb(p_full);
        if(full < -60.f) // silence: hold every band where it is
            return;
        // vs the overall trend (a straight line through the whole spectrum): only bulges and dips count,
        // the overall tilt is warmth's (and the pid tab's) job
        float d[kD], sx = 0.f, sy = 0.f, sxx = 0.f, sxy = 0.f;
        for(int i = 0; i < kD; i++)
        {
            d[i] = PowToDb(p_det[i]);
            sx += float(i), sy += d[i], sxx += float(i * i), sxy += float(i) * d[i];
        }
        const float slope = (float(kD) * sxy - sx * sy) / (float(kD) * sxx - sx * sx), icpt = (sy - slope * sx) / float(kD);
        for(int i = 0; i < kD; i++)
            d[i] -= icpt + slope * float(i);
        const float mean = 0.f;
        for(int i = 0; i < kD; i++) // a little smoothing across neighbours, so one busy note isn't a "spot"
        {
            const float a = d[i > 0 ? i - 1 : i], c = d[i < kD - 1 ? i + 1 : i];
            dev_db[i] = 0.25f * a + 0.5f * d[i] + 0.25f * c;
        }
        struct Spot
        {
            float hz, q, db, w;
        } spot[kD];
        int         ns       = 0;
        const float strength = mode == 2 ? 0.9f : 0.6f, step = log2f(det_hz[1] / det_hz[0]);
        float       raw[kD];
        for(int i = 0; i < kD; i++)
            raw[i] = d[i] - mean;
        for(int i = 1; i < kD - 1; i++) // the end detectors only help measure: no band below ~50 Hz or above ~12 kHz
        {
            const float v = dev_db[i];
            if(fabsf(v) < 1.5f)
                continue;
            const float l = dev_db[i - 1], r = dev_db[i + 1];
            if(!((v > 0.f && v >= l && v >= r) || (v < 0.f && v <= l && v <= r))) // only the tip of a bulge / dip
                continue;
            if(mode == 1 && v > 0.f) // add: never cut
                continue;
            // the exact centre (a parabola through the three raw readings)
            const float rl = raw[i - 1], rv = raw[i], rr = raw[i + 1], den = rl - 2.f * rv + rr;
            const float off = fabsf(den) > 1e-6f ? Clampf(0.5f * (rl - rr) / den, -0.5f, 0.5f) : 0.f;
            // how wide: where the raw reading falls to half on each side (in octaves), minus the detectors' own
            // width (~0.75 octave), so a narrow ring gets a narrow band and a broad hump a broad one
            auto edge = [&](int dir) {
                float x = 0.f;
                for(int j = i + dir; j >= 0 && j < kD; j += dir)
                {
                    const float prev = raw[j - dir], cur = raw[j];
                    if(cur * rv <= 0.f || fabsf(cur) < 0.5f * fabsf(rv))
                        return x + step * Clampf((fabsf(prev) - 0.5f * fabsf(rv)) / (fabsf(prev) - fabsf(cur) + 1e-6f), 0.f, 1.f);
                    x += step;
                }
                return x;
            };
            const float bw_seen = edge(-1) + edge(1), bw = sqrtf(fmaxf(bw_seen * bw_seen - 0.75f * 0.75f, 0.12f * 0.12f));
            const float k = powf(2.f, bw);
            spot[ns++] = {det_hz[i] * powf(2.f, off * step), Clampf(sqrtf(k) / (k - 1.f), 0.5f, 8.f), Clampf(-v * strength, -max_db, max_db), fabsf(v)};
        }
        for(int a = 1; a < ns; a++) // biggest first
            for(int b = a; b > 0 && spot[b].w > spot[b - 1].w; b--)
            {
                const Spot t = spot[b];
                spot[b] = spot[b - 1], spot[b - 1] = t;
            }
        if(ns > n_bands)
            ns = n_bands;
        bool taken[kMaxBands] = {};
        for(int s = 0; s < ns; s++)
        {
            int best = -1;
            float bd = 1.f; // a live band within an octave follows the spot (glides there)
            for(int b = 0; b < n_bands; b++)
                if(!taken[b] && band[b].live && fabsf(Oct(band[b].hz) - Oct(spot[s].hz)) < bd)
                    bd = fabsf(Oct(band[b].hz) - Oct(spot[s].hz)), best = b;
            if(best < 0) // otherwise a free band (no gain) jumps there
                for(int b = 0; b < n_bands && best < 0; b++)
                    if(!taken[b] && !band[b].live)
                        best = b, band[b].hz = spot[s].hz, band[b].q = spot[s].q, band[b].f.Reset();
            if(best < 0)
                continue;
            taken[best]       = true;
            band[best].live   = true;
            band[best].t_hz   = spot[s].hz, band[best].t_q = spot[s].q, band[best].t_db = spot[s].db;
        }
        for(int b = 0; b < kMaxBands; b++)
            if(!taken[b])
                band[b].t_db = 0.f; // nothing to do here now: fade out (and become free once silent)
    }
    // the knobs: thump (most per hit, dB), detail (harmonics), clarity (most each band may move, dB),
    // warmth (steady low shelf, dB); m = mode; bands = how many bands (1-10)
    void Update(float thump_max, float detail_db, float clarity_db, float warmth_db, float block_s, int m = 0, int bands = 6)
    {
        mode    = m;
        n_bands = bands < 1 ? 1 : (bands > kMaxBands ? kMaxBands : bands);
        max_db  = clarity_db;
        detail  = detail_db / 6.f;
        const float c3 = 1.f - expf(-block_s / 0.003f), c80 = 1.f - expf(-block_s / 0.08f), c250 = 1.f - expf(-block_s / 0.25f);
        const float transient = fmaxf(0.f, PowToDb(low_fast) - PowToDb(low_slow));
        const float t_thump   = fminf(thump_max, transient);
        thump_db += (t_thump - thump_db) * (t_thump > thump_db ? c3 : c80);
        warm_lo_db += (warmth_db - warm_lo_db) * c250;
        since += block_s;
        if(since >= 0.02f) // look for spots 50 times a second
            since = 0.f, Search();
        // glide: frequency and width over ~0.3 s (normalise ~1 s), gain in ~0.15 s / out ~0.4 s
        const float slow = mode == 2 ? 3.f : 1.f;
        const float c_f = 1.f - expf(-block_s / (0.3f * slow)), c_in = 1.f - expf(-block_s / (0.15f * slow)),
                    c_out = 1.f - expf(-block_s / (0.4f * slow));
        live_n = 0, cut_most = lift_most = 0.f;
        for(Band& b : band)
        {
            if(!b.live)
                continue;
            b.hz *= powf(b.t_hz / b.hz, c_f);
            b.q *= powf(b.t_q / b.q, c_f);
            b.db += (b.t_db - b.db) * (fabsf(b.t_db) > fabsf(b.db) ? c_in : c_out);
            if(b.t_db == 0.f && fabsf(b.db) < 0.03f)
                b.live = false, b.db = 0.f;
            live_n += b.live;
            cut_most = fminf(cut_most, b.db), lift_most = fmaxf(lift_most, b.db);
        }
        detail_view_db = detail > 0.001f ? 10.f * log10f((low_slow + p_harm + 1e-20f) / (low_slow + 1e-20f)) : 0.f;
        Apply(false);
    }
    inline void Run(float& l, float& r)
    {
        const float m  = 0.5f * (l + r);
        const float lo = d_low.Lp(0, m), lp = lo * lo;
        Follow(low_fast, lp, c_lf_up, c_lf_dn);
        Follow(low_slow, lp, c_ls_up, c_ls_dn);
        p_full += (m * m - p_full) * c_avg;
        for(int i = kLowDet; i < kD; i++)
        {
            const float v = det[i].Bp(0, m);
            p_det[i] += (v * v - p_det[i]) * c_det;
        }
        acc_m += aa[1].Lp(0, aa[0].Lp(0, m));
        if(++dec_n == kDec)
        {
            const float md = acc_m / kDec;
            acc_m = 0.f, dec_n = 0;
            for(int i = 0; i < kLowDet; i++)
            {
                const float v = det[i].Bp(0, md);
                p_det[i] += (v * v - p_det[i]) * c_det_d;
            }
        }
        float x[2] = {l, r};
        if(detail > 0.001f)
        {
            // bass detail: 2nd + 3rd harmonics of the bass (Chebyshev, level-tracking), only the harmonics kept
            const float bl = det_lp.Lp(0, l), br = det_lp.Lp(1, r);
            const float pk = fmaxf(fabsf(bl), fabsf(br));
            bass_pk        = pk > bass_pk ? pk : bass_pk + (pk - bass_pk) * c_pk_dn;
            const float b[2] = {bl, br};
            for(int ch = 0; ch < 2; ch++)
            {
                const float xn = Clampf(b[ch] / (bass_pk + 1e-5f), -1.f, 1.f);
                float       h  = (0.55f * (2.f * xn * xn - 1.f) + 0.45f * (4.f * xn * xn * xn - 3.f * xn)) * bass_pk;
                h              = det_out.Lp(ch, det_hp.Hp(ch, h));
                h *= detail * 1.5f;
                x[ch] += h;
                if(ch == 0)
                    p_harm += (h * h - p_harm) * c_avg;
            }
        }
        for(int ch = 0; ch < 2; ch++)
        {
            float y = warm_lo.Run(ch, thump.Run(ch, x[ch]));
            for(Band& b : band)
                if(b.live)
                    y = b.f.Run(ch, y);
            x[ch] = y;
        }
        l = x[0], r = x[1];
    }
};

// ------------------------------------------------------------------ 5. saturation (anti-aliased)
struct SaturateStage
{
    Svf   dc, tone;
    float x1[2] = {}, f1[2] = {}; // last input and its antiderivative (worked out once, used twice)
    float gain = 1.f, bias = 0.f, tb = 0.f, norm = 1.f, mix = 1.f, tone_hz = 0.f;
    float drive_pk = 0.f; // for the display
    float fs = 48000.f;

    static float LogCosh(float u) // log(cosh(u)), with the fast exp / log (error ~0.00002)
    {
        const float a = fabsf(u);
        return a + 0.693147181f * FastLog2(1.f + FastExp2(-2.885390082f * a)) - 0.69314718f;
    }
    float F(float x) const { return LogCosh(x + bias) - x * tb; } // antiderivative of tanh(x+b) - tanh(b)
    float Fn(float x) const { return tanhf(x + bias) - tb; }

    void Init(float sr)
    {
        fs = sr;
        dc.Set(Svf::HP, fs, 10.f, 0.707f), dc.Reset(), tone.Reset();
        x1[0] = x1[1] = 0.f, tone_hz = 0.f;
    }
    void Update(float drive_db, float even, float tone01, float mix01)
    {
        gain             = DbToLin(drive_db);
        bias             = even * 0.6f;
        tb               = tanhf(bias);
        norm             = 1.f / (gain * (1.f - tb * tb));
        for(int ch = 0; ch < 2; ch++) // the curve may have changed: refresh the cached antiderivative
            f1[ch] = F(x1[ch]);
        mix              = mix01;
        const float want = 4000.f * powf(2.f, tone01 * 2.3f);
        if(fabsf(want - tone_hz) > 1.f)
            tone.Set(Svf::LP, fs, want, 0.6f), tone_hz = want;
    }
    inline void Run(float& l, float& r)
    {
        float* io[2] = {&l, &r};
        float  pk    = 0.f;
        for(int ch = 0; ch < 2; ch++)
        {
            const float dry = *io[ch], x = dry * gain;
            pk              = fmaxf(pk, fabsf(x));
            const float d   = x - x1[ch];
            // 1st-order antiderivative anti-aliasing: the average of the curve between two samples
            const float fx = F(x);
            const float y  = fabsf(d) > 1e-4f ? (fx - f1[ch]) / d : Fn(0.5f * (x + x1[ch]));
            x1[ch] = x, f1[ch] = fx;
            const float wet = tone.Run(ch, dc.Run(ch, y * norm));
            *io[ch]         = dry + (wet - dry) * mix;
        }
        drive_pk = fmaxf(pk, drive_pk * 0.9995f);
    }
};

// ------------------------------------------------------------------ 6. de-harsh
constexpr float kDeHarshF[6] = {2000.f, 2700.f, 3600.f, 4800.f, 6400.f, 8500.f}; // band centres
struct DeHarshStage
{
    static constexpr int kD = 6;
    Svf   det[kD], cut[kD], comfort;
    DuckWeight kw; // anti-duck: "how loud" for the comfort dip ignores the bass
    float p[kD] = {}, cut_db[kD] = {}, applied[kD] = {}, p_full = 0.f, comfort_db = 0.f, comfort_ap = 1000.f;
    bool  cut_on[kD] = {};
    float c_att = 0.f, c_rel = 0.f, c_full = 0.f;
    float fs = 48000.f;

    void Init(float sr)
    {
        fs = sr;
        for(int i = 0; i < kD; i++)
            det[i].Set(Svf::BP, fs, kDeHarshF[i], 3.f), det[i].Reset(), cut[i].Set(Svf::BELL, fs, kDeHarshF[i], 3.f, 0.f), cut[i].Reset(),
                p[i] = 0.f, cut_db[i] = 0.f, applied[i] = 0.f;
        comfort.Set(Svf::BELL, fs, 3000.f, 0.7f, 0.f), comfort.Reset();
        kw.Init(fs, 150.f);
        comfort_db = 0.f, comfort_ap = 0.f, p_full = 0.f;
        c_att = Coef(1.f, fs), c_full = Coef(300.f, fs);
    }
    // depth dB (max cut), sens 0..1, speed 0..1 (release 300 -> 30 ms), comfort 0..1
    void Update(float depth, float sens, float speed, float comf, float block_s)
    {
        c_rel             = Coef(300.f - 270.f * speed, fs);
        float rel[kD], sum = 0.f;
        for(int i = 0; i < kD; i++) // add back the natural ~3 dB/octave fall so only real peaks stand out
            rel[i] = PowToDb(p[i]) + 3.f * log2f(kDeHarshF[i] / 4000.f), sum += rel[i];
        const float mean = sum / float(kD), margin = 9.f - 7.f * sens;
        const float up = 1.f - expf(-block_s / 0.002f), dn = 1.f - expf(-block_s / (0.3f - 0.27f * speed));
        for(int i = 0; i < kD; i++)
        {
            const float excess = rel[i] - mean - margin;
            const float target = (PowToDb(p[i]) > -70.f && excess > 0.f) ? -fminf(depth, excess) : 0.f;
            cut_db[i] += (target - cut_db[i]) * (target < cut_db[i] ? up : dn);
            if(fabsf(cut_db[i] - applied[i]) > 0.05f)
                cut[i].Set(Svf::BELL, fs, kDeHarshF[i], 3.f, cut_db[i]), applied[i] = cut_db[i];
        }
        // comfort: ears get touchier around 3 kHz the louder it is
        const float loud = PowToDb(p_full) + 3.f;
        const float t_c  = -comf * 3.f * Smooth01((loud + 20.f) / 10.f);
        comfort_db += (t_c - comfort_db) * (1.f - expf(-block_s / 0.3f));
        if(fabsf(comfort_db - comfort_ap) > 0.05f)
            comfort.Set(Svf::BELL, fs, 3000.f, 0.7f, comfort_db), comfort_ap = comfort_db;
    }
    inline void Run(float& l, float& r)
    {
        const float m = 0.5f * (l + r), w = kw.Run(0, m);
        p_full += (w * w - p_full) * c_full;
        for(int i = 0; i < kD; i++)
        {
            const float d = det[i].Bp(0, m), sq = d * d;
            p[i] += (sq - p[i]) * (sq > p[i] ? c_att : c_rel);
            const bool on = fabsf(applied[i]) > 0.05f; // an idle cut is a plain wire: skipped
            if(on && !cut_on[i])
                cut[i].Reset();
            cut_on[i] = on;
            if(on)
                l = cut[i].Run(0, l), r = cut[i].Run(1, r);
        }
        l = comfort.Run(0, l);
        r = comfort.Run(1, r);
    }
};

// ------------------------------------------------------------------ 7. width (stereo space)
// Mid / side: wider or narrower, the bass kept in the middle (mono-safe), a little side "air", and a guard
// that eases the width back if left and right start to cancel (mono speakers / phone would lose sound).
struct WidthStage
{
    Svf   side_hp, side_air;
    float w = 1.f, w_eff = 1.f, mono_hz = 0.f, air_db = 0.f, air_ap = 1000.f, mono_ap = -1.f;
    bool  guard = true;
    float p_lr = 0.f, p_ll = 0.f, p_rr = 0.f, corr = 1.f, c_corr = 0.f, fs = 48000.f;

    void Init(float sr)
    {
        fs = sr;
        side_hp.Reset(), side_air.Reset();
        side_air.Set(Svf::HSHELF, fs, 5000.f, 0.7f, 0.f);
        w = w_eff = 1.f, air_ap = 1000.f, mono_ap = -1.f;
        p_lr = p_ll = p_rr = 0.f, corr = 1.f;
        c_corr = Coef(300.f, fs);
    }
    void Update(float width, float mono, float air, bool g, float block_s)
    {
        w = width, guard = g;
        corr = p_lr / (sqrtf(p_ll * p_rr) + 1e-12f);
        // the guard: below +0.1 correlation, ease back toward plain stereo; otherwise follow the knob
        const float target = (guard && corr < 0.1f && w > 1.f) ? fmaxf(1.f, w_eff - 0.5f) : w;
        w_eff += (target - w_eff) * (1.f - expf(-block_s / 0.3f));
        if(mono != mono_ap)
        {
            mono_hz = mono, mono_ap = mono;
            if(mono_hz > 0.f)
                side_hp.Set(Svf::HP, fs, mono_hz, 0.707f);
        }
        if(fabsf(air - air_ap) > 0.05f)
            side_air.Set(Svf::HSHELF, fs, 5000.f, 0.7f, air), air_ap = air;
        air_db = air;
    }
    inline void Run(float& l, float& r)
    {
        const float m = 0.5f * (l + r);
        float       s = 0.5f * (l - r);
        if(mono_hz > 0.f)
            s = side_hp.Run(0, s); // bass stays in the middle
        s = side_air.Run(0, s) * w_eff;
        l = m + s, r = m - s;
        p_lr += (l * r - p_lr) * c_corr, p_ll += (l * l - p_ll) * c_corr, p_rr += (r * r - p_rr) * c_corr;
    }
};

// ------------------------------------------------------------------ 8. takeback
// Compares the sound now with the sound as it came in (after hum + input level) and gives back what the
// processing took away or buried: the punch of each hit (transients squashed by comp / limiting), quiet
// details (tails, room, breaths) lifted a little, top-end air that went missing, and stereo space that
// got narrower. Each is capped, so it restores rather than exaggerates.
struct TakebackStage
{
    Svf   hf_ref, hf_cur, air;
    float f_ref = 0.f, s_ref = 0.f, f_cur = 0.f, s_cur = 0.f;                 // transient envelopes
    float p_short = 0.f, p_long = 0.f;                                       // buried detail
    float ph_ref = 0.f, pf_ref = 0.f, ph_cur = 0.f, pf_cur = 0.f;            // air
    float pm_ref = 0.f, ps_ref = 0.f, pm_cur = 0.f, ps_cur = 0.f;            // space
    float punch_db = 0.f, detail_db = 0.f, air_db = 0.f, space_db = 0.f, air_ap = 1000.f;
    float g = 1.f, g_target = 1.f, side_g = 1.f;
    float c_fr = 0.f, c_s = 0.f, c_short = 0.f, c_long = 0.f, c_300 = 0.f, c_g = 0.f, fs = 48000.f;

    void Init(float sr)
    {
        fs = sr;
        hf_ref.Set(Svf::HP, fs, 8000.f, 0.707f), hf_cur.Set(Svf::HP, fs, 8000.f, 0.707f);
        hf_ref.Reset(), hf_cur.Reset(), air.Reset();
        air.Set(Svf::HSHELF, fs, 9000.f, 0.7f, 0.f), air_ap = 0.f;
        f_ref = s_ref = f_cur = s_cur = p_short = p_long = 0.f;
        ph_ref = pf_ref = ph_cur = pf_cur = pm_ref = ps_ref = pm_cur = ps_cur = 0.f;
        punch_db = detail_db = air_db = space_db = 0.f, g = g_target = side_g = 1.f;
        c_fr = Coef(10.f, fs), c_s = Coef(40.f, fs), c_short = Coef(30.f, fs), c_long = Coef(2000.f, fs);
        c_300 = Coef(300.f, fs), c_g = Coef(1.f, fs);
    }
    // amounts 0..1
    void Update(float a_punch, float a_detail, float a_air, float a_space, float block_s)
    {
        // punch: how much sharper the hits were coming in than they are now
        const float lost_tr = fmaxf(0.f, (PowToDb(f_ref) - PowToDb(s_ref)) - (PowToDb(f_cur) - PowToDb(s_cur))); // (amplitude: half the dB lost)
        const float t_punch = a_punch * fminf(lost_tr, 6.f);
        punch_db += (t_punch - punch_db) * (t_punch > punch_db ? 1.f : 1.f - expf(-block_s / 0.06f));
        // buried detail: a quiet moment (not silence) below the recent average gets lifted a little
        const float sh = PowToDb(p_short), gap = PowToDb(p_long) - sh;
        const float t_det = (sh > -60.f && gap > 0.f) ? a_detail * fminf(gap * 0.5f, 6.f) : 0.f;
        detail_db += (t_det - detail_db) * (1.f - expf(-block_s / (t_det > detail_db ? 0.05f : 0.2f)));
        // air: the top end's share of the sound, then vs now
        const float lost_air = (PowToDb(ph_ref) - PowToDb(pf_ref)) - (PowToDb(ph_cur) - PowToDb(pf_cur));
        const float t_air    = (PowToDb(pf_cur) > -60.f && lost_air > 0.f) ? a_air * fminf(lost_air, 6.f) : 0.f;
        air_db += (t_air - air_db) * (1.f - expf(-block_s / 0.3f));
        if(fabsf(air_db - air_ap) > 0.05f)
            air.Set(Svf::HSHELF, fs, 9000.f, 0.7f, air_db), air_ap = air_db;
        // space: the side's share, then vs now
        const float lost_sp = (PowToDb(ps_ref) - PowToDb(pm_ref)) - (PowToDb(ps_cur) - PowToDb(pm_cur));
        const float t_sp    = (PowToDb(pm_cur) > -60.f && lost_sp > 0.f) ? a_space * fminf(lost_sp, 4.f) : 0.f;
        space_db += (t_sp - space_db) * (1.f - expf(-block_s / 0.3f));
        side_g   = DbToLin(space_db);
        g_target = DbToLin(punch_db + detail_db);
    }
    // l, r = the sound now; rl, rr = the same moment as it came in
    inline void Run(float& l, float& r, float rl, float rr)
    {
        const float mr = 0.5f * (rl + rr), sr = 0.5f * (rl - rr), m = 0.5f * (l + r), s = 0.5f * (l - r);
        const float ar = fabsf(mr), a = fabsf(m);
        f_ref = ar > f_ref ? ar : f_ref + (ar - f_ref) * c_fr;
        f_cur = a > f_cur ? a : f_cur + (a - f_cur) * c_fr;
        s_ref += (ar - s_ref) * c_s, s_cur += (a - s_cur) * c_s;
        const float pw = m * m;
        p_short += (pw - p_short) * c_short, p_long += (pw - p_long) * c_long;
        const float hr = hf_ref.Run(0, mr), hc = hf_cur.Run(0, m);
        ph_ref += (hr * hr - ph_ref) * c_300, pf_ref += (mr * mr - pf_ref) * c_300;
        ph_cur += (hc * hc - ph_cur) * c_300, pf_cur += (pw - pf_cur) * c_300;
        pm_ref += (mr * mr - pm_ref) * c_300, ps_ref += (sr * sr - ps_ref) * c_300;
        pm_cur += (pw - pm_cur) * c_300, ps_cur += (s * s - ps_cur) * c_300;
        g += (g_target - g) * c_g;
        const float m2 = m * g, s2 = s * g * side_g;
        l = air.Run(0, m2 + s2), r = air.Run(1, m2 - s2);
    }
};

// ------------------------------------------------------------------ 9. loudness (quiet listening)
// At low volume ears hear less bass and treble than at a loud level. This lifts them by about the
// amount they fade (equal-loudness curves), more the quieter you listen. "auto" also follows the music:
// quiet passages get a little more.
struct LoudnessStage
{
    Svf   lo, hi;
    DuckWeight kw; // anti-duck: a bass-heavy part doesn't count as a loud part
    float lo_db = 0.f, hi_db = 0.f, lo_ap = 1000.f, hi_ap = 1000.f, p = 0.f, c_p = 0.f, eff = 80.f, fs = 48000.f;

    void Init(float sr)
    {
        fs = sr;
        lo.Reset(), hi.Reset();
        lo.Set(Svf::LSHELF, fs, 90.f, 0.7f, 0.f), hi.Set(Svf::HSHELF, fs, 9000.f, 0.7f, 0.f);
        lo_db = hi_db = 0.f, lo_ap = hi_ap = 0.f, p = 0.f;
        c_p = Coef(1000.f, fs);
        kw.Init(fs, 150.f);
    }
    // listen = how loud you listen (phon, 40 quiet .. 90 loud); bass / treble 0..1; follow the music level
    void Update(float listen, float bass, float treble, bool follow, float block_s)
    {
        float l = listen;
        if(follow) // a passage 10 dB under the usual -18 dBFS counts as listening 10 phon quieter
            l += Clampf(PowToDb(p) + 18.f, -20.f, 6.f);
        eff              = l;
        const float t_lo = Clampf((80.f - l) * 0.35f * bass, 0.f, 12.f), t_hi = Clampf((80.f - l) * 0.12f * treble, 0.f, 5.f);
        const float c    = 1.f - expf(-block_s / 0.5f);
        lo_db += (t_lo - lo_db) * c, hi_db += (t_hi - hi_db) * c;
        if(fabsf(lo_db - lo_ap) > 0.05f)
            lo.Set(Svf::LSHELF, fs, 90.f, 0.7f, lo_db), lo_ap = lo_db;
        if(fabsf(hi_db - hi_ap) > 0.05f)
            hi.Set(Svf::HSHELF, fs, 9000.f, 0.7f, hi_db), hi_ap = hi_db;
    }
    inline void Run(float& l, float& r)
    {
        const float w = kw.Run(0, 0.5f * (l + r));
        p += (w * w - p) * c_p;
        l = hi.Run(0, lo.Run(0, l)), r = hi.Run(1, lo.Run(1, r));
    }
};

// ------------------------------------------------------------------ 9b. anti-duck guard (always on)
// The last word on ducking. It compares how loud the mids and highs are coming out against going in
// (bass-free loudness, K-weighted), and learns the normal ratio from ordinary moments. When the bass gets
// heavy and the processing has still pulled the rest of the track under that ratio, it lifts everything
// above ~150 Hz back up with a high shelf (at most +6 dB, before the safety limiter). Ordinary compression of loud parts is
// left alone: it only acts while the bass is heavy. A changed setting re-learns the ratio (Rebase).
struct AntiDuckStage
{
    DuckWeight w_in, w_out;
    Svf   lo_in;
    Svf   lift;            // the lift: a high shelf from ~150 Hz up (only run while it lifts)
    float lift_ap = 0.f;
    bool  lift_on = false;
    float p_in = 0.f, p_out = 0.f, p_full = 0.f, p_lo = 0.f, c_p = 0.f;
    float ref = 0.f, share_avg = -12.f, boost_db = 0.f, heavy = 0.f, heavy_hold = 0.f, rebase_s = 0.f;
    bool  have_ref = false;
    float fs = 48000.f;

    void Init(float sr)
    {
        fs = sr;
        w_in.Init(fs, 300.f), w_out.Init(fs, 300.f); // "the rest of the track": everything from 300 Hz up
        lo_in.Set(Svf::LP, fs, 120.f, 0.707f), lo_in.Reset();
        lift.Set(Svf::HSHELF, fs, 150.f, 0.6f, 0.f), lift.Reset(), lift_ap = 0.f, lift_on = false;
        p_in = p_out = p_full = p_lo = 0.f;
        ref = 0.f, share_avg = -12.f, boost_db = 0.f, heavy = heavy_hold = 0.f, rebase_s = 0.f, have_ref = false;
        c_p = Coef(400.f, fs);
    }
    void Rebase() { rebase_s = 0.6f; } // a setting changed: learn the normal ratio again in a moment
    void Update(float block_s)
    {
        const float in_db = PowToDb(p_in), out_db = PowToDb(p_out);
        float       target = 0.f;
        if(in_db > -50.f && out_db > -70.f)
        {
            const float ratio = out_db - in_db, share = PowToDb(p_lo) - PowToDb(p_full);
            share_avg += (share - share_avg) * (1.f - expf(-block_s / 8.f));
            // heavy: the bass carries most of the energy, or clearly more than usual for this music
            heavy      = fmaxf(Smooth01((share + 6.f) / 5.f), Smooth01((share - share_avg) / 4.f));
            heavy_hold = fmaxf(heavy, heavy_hold * expf(-block_s / 2.f)); // bass was heavy in the last ~2 s
            if(rebase_s > 0.f)
                rebase_s -= block_s, have_ref = rebase_s <= 0.f ? false : have_ref;
            else if(!have_ref)
                ref = ratio, have_ref = true;
            else if(heavy_hold < 0.2f) // learn only from ordinary moments (not as the bass comes or goes)
                ref += (ratio - ref) * (1.f - expf(-block_s / (ratio > ref ? 5.f : 20.f)));
            if(have_ref && rebase_s <= 0.f)
                target = Clampf(ref - ratio, 0.f, 6.f) * heavy;
        }
        boost_db += (target - boost_db) * (1.f - expf(-block_s / (target > boost_db ? 0.08f : 0.4f)));
        if(fabsf(boost_db - lift_ap) > 0.05f)
            lift.Set(Svf::HSHELF, fs, 150.f, 0.6f, boost_db), lift_ap = boost_db;
    }
    // dl, dr = the sound as it came in; l, r = after every stage (changed in place)
    inline void Run(float dl, float dr, float& l, float& r)
    {
        const float mi = 0.5f * (dl + dr), wi = w_in.Run(0, mi), lo = lo_in.Lp(0, mi);
        p_in += (wi * wi - p_in) * c_p, p_full += (mi * mi - p_full) * c_p, p_lo += (lo * lo - p_lo) * c_p;
        const float wo = w_out.Run(0, 0.5f * (l + r)); // measured before the lift: no feedback loop
        p_out += (wo * wo - p_out) * c_p;
        const bool on = fabsf(lift_ap) > 0.05f;
        if(on && !lift_on)
            lift.Reset();
        lift_on = on;
        if(on)
            l = lift.Run(0, l), r = lift.Run(1, r);
    }
};

// ------------------------------------------------------------------ 10. safety (always on)
struct SafetyStage
{
    static constexpr int kMaxLook = 128;
    // hearing range only: 8th-order high-pass at 20 Hz, 16th-order low-pass at 19.5 kHz (Butterworth).
    // Anything outside what people can hear is removed before it can leave, and watched for on both sides.
    static constexpr int kHp = 4, kLp = 8;
    Svf   hp[kHp], lp[kLp];
    // detectors, steep so normal music never trips them: ultrasonic = 8th-order high-pass at 21 kHz,
    // infrasonic = 6th-order low-pass at 10 Hz (input) / 5 Hz (output, after the 20 Hz high-pass)
    Svf   u_in[2], u_out[4], i_in[3], i_out[3]; // (the input's ultrasonic check is only a note: 4th order is plenty)
    float p_u_in = 0.f, p_i_in = 0.f, p_u_out = 0.f, p_i_out = 0.f, c_det = 0.f;
    float ultra_in_db = -200.f, infra_in_db = -200.f, ultra_out_db = -200.f, infra_out_db = -200.f;
    bool  ultra_possible = true; // false when the sample rate can't even carry ultrasonic sound
    Svf   d_low, d_high, woof, tweet;
    DuckWeight ew;   // anti-duck: the ear meters weigh bass like ears do (dBA-like), so bass never turns the track down
    Svf   post;      // 8 Hz high-pass after the limiter: clears the slow wobble its gain riding can leave
    MultibandStage::Lr4 xo; // 120 Hz Linkwitz-Riley split (low + high add back up flat): peaks from the bass are caught in the bass
    float d_lo[2][kMaxLook] = {}, need_lo[kMaxLook] = {}, gb = 1.f, bass_limit_db = 0.f, c_rel_b = 0.f;
    float k_ear = 1.f; // the ear cap + blast guard as a gain (worked out once per block)
    bool  woof_on = false, tweet_on = false;
    // the limiter's look-ahead minimum, kept as it slides (instead of scanning 48 values every sample)
    struct SlideMin
    {
        float v[kMaxLook] = {};
        int   t[kMaxLook] = {}, head = 0, n = 0, now = 0;
        void  Reset() { head = n = now = 0; }
        float Push(float x, int window)
        {
            while(n > 0 && v[(head + n - 1) % kMaxLook] >= x) // drop the newer-but-bigger ones from the back
                n--;
            const int at = (head + n) % kMaxLook;
            v[at] = x, t[at] = now, n++;
            while(t[head] <= now - window) // drop what slid out of the window at the front
                head = (head + 1) % kMaxLook, n--;
            now++;
            return v[head];
        }
    } win, win_lo;
    // infrasonic detectors run on a 1/16-rate copy (they only look below 10 Hz)
    static constexpr int kInfraDec = 16;
    float i_acc_in = 0.f, i_acc_out = 0.f, c_det_i = 0.f;
    int   i_n = 0;
    float p_low = 0.f, p_high = 0.f, p_ear = 0.f, p_fast = 0.f, dc_avg = 0.f;
    float woof_db = 0.f, tweet_db = 0.f, ear_db = 0.f, blast_db = 0.f, woof_ap = 1000.f, tweet_ap = 1000.f;
    float lvl_low = -120.f, lvl_high = -120.f, lvl_ear = -120.f; // for the display
    float gl = 1.f, ceiling = 0.89f, limit_db = 0.f;
    float delay[2][kMaxLook] = {}, need[kMaxLook] = {};
    int   look = 48, pos = 0;
    float mute = 1.f, mute_target = 1.f;
    int   mute_hold = 0, glitches = 0, range_trips = 0;
    float u_over_ms = 0.f, i_over_ms = 0.f;
    bool  want_reset = false; // tells the core to clear every stage's filters after a glitch
    float c_low = 0.f, c_high = 0.f, c_ear = 0.f, c_fast = 0.f, c_att = 0.f, c_rel = 0.f, c_dc = 0.f;
    float c_mute_dn = 0.f, c_mute_up = 0.f;
    float fs = 48000.f;

    void Init(float sr)
    {
        fs = sr;
        static const float hq[kHp] = {0.5098f, 0.6013f, 0.9000f, 2.5629f};
        static const float q6[3]   = {0.5176f, 0.7071f, 1.9319f};
        static const float lq[kLp] = {0.5024f, 0.5224f, 0.5669f, 0.6468f, 0.7882f, 1.0606f, 1.7224f, 5.1011f};
        for(int i = 0; i < kHp; i++)
            hp[i].Set(Svf::HP, fs, 20.f, hq[i]), hp[i].Reset();
        for(int i = 0; i < kLp; i++)
            lp[i].Set(Svf::LP, fs, 19500.f, lq[i]), lp[i].Reset();
        ultra_possible = fs * 0.47f > 21000.f;
        static const float q8[4] = {0.5098f, 0.6013f, 0.9000f, 2.5629f};
        for(int i = 0; i < 4; i++)
            u_out[i].Set(Svf::HP, fs, 21000.f, q8[i]), u_out[i].Reset();
        static const float q4[2] = {0.5412f, 1.3066f};
        for(int i = 0; i < 2; i++)
            u_in[i].Set(Svf::HP, fs, 21000.f, q4[i]), u_in[i].Reset();
        for(int i = 0; i < 3; i++)
            i_in[i].Set(Svf::LP, fs / kInfraDec, 10.f, q6[i]), i_out[i].Set(Svf::LP, fs / kInfraDec, 5.f, q6[i]), i_in[i].Reset(), i_out[i].Reset();
        i_acc_in = i_acc_out = 0.f, i_n = 0, c_det_i = Coef(50.f, fs / kInfraDec);
        win.Reset(), win_lo.Reset(), k_ear = 1.f;
        u_over_ms = i_over_ms = 0.f;
        p_u_in = p_i_in = p_u_out = p_i_out = 0.f;
        c_det = Coef(50.f, fs);
        d_low.Set(Svf::LP, fs, 150.f, 0.707f), d_low.Reset();
        d_high.Set(Svf::HP, fs, 4500.f, 0.707f), d_high.Reset();
        woof.Set(Svf::LSHELF, fs, 150.f, 0.7f, 0.f), woof.Reset();
        ew.Init(fs, 150.f);
        xo.Set(fs, 120.f);
        gb = 1.f, bass_limit_db = 0.f, c_rel_b = Coef(60.f, fs);
        post.Set(Svf::HP, fs, 8.f, 0.7071f), post.Reset();
        tweet.Set(Svf::HSHELF, fs, 4500.f, 0.7f, 0.f), tweet.Reset();
        p_low = p_high = p_ear = p_fast = dc_avg = 0.f;
        woof_db = tweet_db = ear_db = blast_db = 0.f, woof_ap = tweet_ap = 0.f;
        gl = 1.f, pos = 0;
        look = int(fs * 0.001f + 0.5f);
        look = look < 1 ? 1 : (look > kMaxLook ? kMaxLook : look);
        for(int i = 0; i < kMaxLook; i++)
            delay[0][i] = delay[1][i] = 0.f, need[i] = 1.f, d_lo[0][i] = d_lo[1][i] = 0.f, need_lo[i] = 1.f;
        c_low = Coef(2000.f, fs), c_high = Coef(1000.f, fs), c_ear = Coef(3000.f, fs), c_fast = Coef(10.f, fs);
        c_att = 1.f - expf(-10.f / float(look)), c_rel = Coef(80.f, fs), c_dc = Coef(50.f, fs); // fully down in time
        c_mute_dn = Coef(1.f, fs), c_mute_up = Coef(150.f, fs);
    }
    // ceiling dBFS (peak), ear / woofer / tweeter: long-term maximum levels in dBFS
    void Update(float ceil_db, float ear_max, float woof_max, float tweet_max, float block_s)
    {
        ceiling       = DbToLin(ceil_db);
        ultra_in_db   = ultra_possible ? PowToDb(p_u_in) + 3.f : -200.f;
        ultra_out_db  = ultra_possible ? PowToDb(p_u_out) + 3.f : -200.f;
        infra_in_db   = PowToDb(p_i_in) + 3.f;
        infra_out_db  = PowToDb(p_i_out) + 3.f;
        // outside the hearing range got out and STAYED out (only a failed filter does that). A sudden, loud
        // bass note can leave a short slow wobble behind the limiter; that dies away well within a second,
        // and a false trip would mute (duck) the whole track, so the infrasonic side needs a full second.
        const float block_ms = block_s * 1000.f;
        u_over_ms            = ultra_out_db > -50.f ? u_over_ms + block_ms : 0.f;
        i_over_ms            = infra_out_db > -40.f ? i_over_ms + block_ms : 0.f;
        if(u_over_ms >= 100.f || i_over_ms >= 1000.f)
            range_trips++, u_over_ms = i_over_ms = 0.f, MuteReset();
        lvl_low       = PowToDb(p_low) + 3.f;
        lvl_high      = PowToDb(p_high) + 3.f;
        lvl_ear       = PowToDb(p_ear) + 3.f;
        const float a = 1.f - expf(-block_s / 0.3f), d = 1.f - expf(-block_s / 2.f);
        Follow(woof_db, -Clampf(lvl_low - woof_max, 0.f, 12.f), d, a); // (negative values: "rising" = releasing)
        Follow(tweet_db, -Clampf(lvl_high - tweet_max, 0.f, 12.f), d, a);
        Follow(ear_db, -Clampf(lvl_ear - ear_max, 0.f, 24.f), 1.f - expf(-block_s / 3.f), 1.f - expf(-block_s / 0.5f));
        // blast guard: something suddenly far louder than the last few seconds -> duck it at once
        const float fast  = PowToDb(p_fast) + 3.f;
        const float blast = (fast > ear_max + 6.f && fast - lvl_ear > 15.f) ? -Clampf(fast - (ear_max + 6.f), 0.f, 30.f) : 0.f;
        Follow(blast_db, blast, 1.f - expf(-block_s / 0.4f), 1.f - expf(-block_s / 0.001f));
        k_ear = DbToLin(ear_db + blast_db);
        if(fabsf(woof_db - woof_ap) > 0.05f)
            woof.Set(Svf::LSHELF, fs, 150.f, 0.7f, woof_db), woof_ap = woof_db;
        if(fabsf(tweet_db - tweet_ap) > 0.05f)
            tweet.Set(Svf::HSHELF, fs, 4500.f, 0.7f, tweet_db), tweet_ap = tweet_db;
        if(mute_hold > 0)
        {
            mute_hold -= int(block_s * 1000.f + 0.5f);
            if(mute_hold <= 0)
                mute_target = 1.f;
        }
        limit_db      = 20.f * log10f(gl + 1e-9f);
        bass_limit_db = 20.f * log10f(gb + 1e-9f);
    }
    void Glitch()
    {
        glitches++;
        MuteReset();
    }
    void MuteReset()
    {
        mute_target = 0.f, mute = 0.f, mute_hold = 150;
        want_reset  = true;
        for(Svf& f : hp)
            f.Reset();
        for(Svf& f : lp)
            f.Reset();
        d_low.Reset(), d_high.Reset(), woof.Reset(), tweet.Reset();
        p_fast = 0.f, dc_avg = 0.f;
        p_u_out = p_i_out = 0.f;
        for(Svf& f : u_out)
            f.Reset();
        for(Svf& f : i_out)
            f.Reset();
        for(int i = 0; i < kMaxLook; i++)
            delay[0][i] = delay[1][i] = 0.f, need[i] = 1.f, d_lo[0][i] = d_lo[1][i] = 0.f, need_lo[i] = 1.f;
        xo.Set(fs, 120.f);
        post.Reset();
        ew.a.Reset(), ew.b.Reset(), gb = 1.f;
        win.Reset(), win_lo.Reset(), i_acc_out = 0.f;
    }
    inline void Run(float& l, float& r)
    {
        // 1. glitch guard: broken numbers, a blown-up filter, or a stuck full-scale DC level
        if(!(fabsf(l) < 8.f) || !(fabsf(r) < 8.f)) // also catches NaN / inf
        {
            Glitch();
            l = r = 0.f;
        }
        dc_avg += (0.5f * (l + r) - dc_avg) * c_dc;
        if(fabsf(dc_avg) > 0.5f)
        {
            Glitch();
            l = r = 0.f;
        }
        // 2. hearing range only: watch what's outside it coming in, then remove it (20 Hz - 19.5 kHz)
        {
            const float mi = 0.5f * (l + r);
            float u = mi;
            for(Svf& f : u_in)
                u = f.Hp(0, u);
            p_u_in += (u * u - p_u_in) * c_det;
            i_acc_in += mi;
        }
        for(Svf& f : hp)
            l = f.Hp(0, l), r = f.Hp(1, r);
        for(Svf& f : lp)
            l = f.Lp(0, l), r = f.Lp(1, r);
        // 3. speakers: the long-term power in the woofer and tweeter ranges kept under their limits
        //    (voice coils stay cool)
        const bool wo = fabsf(woof_ap) > 0.05f, tw = fabsf(tweet_ap) > 0.05f; // at 0 dB they are plain wires: skipped
        if(wo && !woof_on)
            woof.Reset();
        if(tw && !tweet_on)
            tweet.Reset();
        woof_on = wo, tweet_on = tw;
        if(wo)
            l = woof.Run(0, l), r = woof.Run(1, r);
        if(tw)
            l = tweet.Run(0, l), r = tweet.Run(1, r);
        const float m = 0.5f * (l + r), lo = d_low.Lp(0, m), hi = d_high.Hp(0, m);
        p_low += (lo * lo - p_low) * c_low;
        p_high += (hi * hi - p_high) * c_high;
        const float el = ew.Run(0, l), er = ew.Run(1, r), pw = fmaxf(el * el, er * er);
        p_ear += (pw - p_ear) * c_ear;
        p_fast += (pw - p_fast) * c_fast;
        // 3. ears: long-term loudness cap + blast guard + glitch mute
        mute += (mute_target - mute) * (mute_target < mute ? c_mute_dn : c_mute_up);
        const float k = k_ear * mute;
        l *= k, r *= k;
        // 4. 1 ms look-ahead peak limiter, then a hard ceiling that nothing can pass. Anti-duck: a peak that
        //    comes from the bass is taken out of the bass alone (the mids and highs stay as loud as they were);
        //    only what the bass can't fix pulls the whole sound down.
        const float c   = ceiling * 0.98f; // a hair under, so the hard ceiling stays a backstop
        float lol, hil, lor, hir;
        xo.Run(0, l, lol, hil), xo.Run(1, r, lor, hir);
        auto bass_room = [c](float lo, float hi) { // most the bass may stay at so lo * g + hi fits under c
            const float a = fabsf(lo);
            if(fabsf(lo + hi) <= c || a < 1e-6f)
                return 1.f;
            const float same = (lo >= 0.f) == (hi >= 0.f) ? 1.f : -1.f;
            return Clampf((c - same * fabsf(hi)) / a, 0.25f, 1.f); // at most -12 dB on the bass
        };
        const float gn = fminf(bass_room(lol, hil), bass_room(lor, hir));
        need_lo[pos]   = gn;
        const float pk = fmaxf(fabsf(lol * gn + hil), fabsf(lor * gn + hir));
        need[pos]      = pk > c ? c / pk : 1.f;
        delay[0][pos] = hil, delay[1][pos] = hir, d_lo[0][pos] = lol, d_lo[1][pos] = lor;
        const float mn = win.Push(need[pos], look), mb = win_lo.Push(need_lo[pos], look);
        gl += (mn - gl) * (mn < gl ? c_att : c_rel);
        gb += (mb - gb) * (mb < gb ? c_att : c_rel_b);
        const int rd = (pos + 1) % look;
        l            = Clampf(post.Hp(0, (d_lo[0][rd] * gb + delay[0][rd]) * gl), -ceiling, ceiling);
        r            = Clampf(post.Hp(1, (d_lo[1][rd] * gb + delay[1][rd]) * gl), -ceiling, ceiling);
        pos          = rd;
        // 5. check what actually leaves: nothing outside the hearing range may get out
        const float mo = 0.5f * (l + r);
        float       uo = mo;
        for(Svf& f : u_out)
            uo = f.Hp(0, uo);
        p_u_out += (uo * uo - p_u_out) * c_det;
        i_acc_out += mo;
        if(++i_n == kInfraDec) // the infrasonic detectors, on the averaged 1/16-rate signal
        {
            float i = i_acc_in / kInfraDec, io = i_acc_out / kInfraDec;
            for(Svf& f : i_in)
                i = f.Lp(0, i);
            for(Svf& f : i_out)
                io = f.Lp(0, io);
            p_i_in += (i * i - p_i_in) * c_det_i, p_i_out += (io * io - p_i_out) * c_det_i;
            i_acc_in = i_acc_out = 0.f, i_n = 0;
        }
    }
};
// ------------------------------------------------------------------ 11. pid (auto-adjust)
// Listens to the processed sound and steers the other tabs' settings towards a target, one PID
// controller per group. Nothing here touches the audio: it measures (Run) and hands out corrections
// that Core adds on top of the user's own settings (Update). Every correction has a fixed safe range,
// all-zero P / I / D means no correction, and in silence it holds still instead of drifting.
//   eq       each band's level vs the target tilt          -> that band's gain (+-6 dB)
//   comp     crest (peak over average) vs 12 dB             -> threshold (+-8 dB) and ratio
//   mband    each band's level vs the target tilt           -> how firmly it holds that band
//   clarity  bass and presence vs the target tilt           -> thump / warmth and clarity (+-4 dB)
//   de-harsh the 3-5 kHz level vs the target tilt           -> depth (+-6 dB)
//   width    stereo correlation vs 0.35                     -> width (+-40 %)
// (namespace scope: libDaisy builds as C++14, where class-scope constexpr arrays need a separate definition)
constexpr float kPidCentre[6] = {60.f, 160.f, 450.f, 1200.f, 3200.f, 9000.f}; // the 6 bands it listens to
constexpr float kPidMbHz[4]   = {60.f, 400.f, 2500.f, 10000.f};                // the multiband's 4 bands
struct PidStage
{
    static constexpr int kA = 6, kG = 6;
    enum Group
    {
        G_EQ,
        G_COMP,
        G_MB,
        G_CLAR,
        G_DH,
        G_WIDTH
    };
    struct Loop
    {
        float i = 0.f, prev = 0.f, d = 0.f, u = 0.f, e = 0.f;
        bool  fresh = true;
    };
    static constexpr float kCrestTarget = 12.f, kCorrTarget = 0.35f;

    Svf   bp[kA];
    float acc[kA] = {}, acc_sq = 0.f, acc_pk = 0.f, acc_ll = 0.f, acc_rr = 0.f, acc_lr = 0.f;
    int   acc_n = 0;
    float env[kA] = {}, p_full = 0.f, pk_env = 0.f, sll = 0.f, srr = 0.f, slr = 0.f;
    // what it hears (for the screen): each band vs the mean of all six, and the target for it
    float rel_db[kA] = {}, target_db[kA] = {}, level_db = -120.f, crest_db = 0.f, corr = 0.f;
    bool  tracking = false;
    Loop  eq[4], comp, mb[4], lo, pres, dh, width;
    // the corrections Core adds (0 when off)
    float eq_db[4] = {}, comp_db = 0.f, mb_amt[4] = {}, lo_db = 0.f, pres_db = 0.f, dh_db = 0.f, width_add = 0.f;

    void Init(float fs)
    {
        *this = PidStage{};
        for(int a = 0; a < kA; a++)
            bp[a].Set(Svf::BP, fs, kPidCentre[a], 1.f), bp[a].Reset(), oct_c[a] = Oct(kPidCentre[a]);
    }
    inline void Run(float l, float r)
    {
        const float m = 0.5f * (l + r);
        for(int a = 0; a < kA; a++)
        {
            const float y = bp[a].Bp(0, m);
            acc[a] += y * y;
        }
        acc_sq += m * m;
        acc_pk = fmaxf(acc_pk, fabsf(m));
        acc_ll += l * l, acc_rr += r * r, acc_lr += l * r;
        acc_n++;
    }
    static float Oct(float hz) { return log2f(hz / 1000.f); }
    float oct_c[kA] = {}; // the six bands' positions in octaves (worked out once)
    // a band's level vs the mean, read at any frequency (straight lines between the six bands)
    float At(const float* v, float hz) const
    {
        const float o = FastLog2(hz * 0.001f);
        if(o <= oct_c[0])
            return v[0];
        for(int a = 1; a < kA; a++)
            if(o <= oct_c[a])
                return v[a - 1] + (v[a] - v[a - 1]) * (o - oct_c[a - 1]) / (oct_c[a] - oct_c[a - 1]);
        return v[kA - 1];
    }
    // one step of a PID loop: e = error, the output is held inside [lo, hi] and the integral
    // only winds while the output can still move that way (no wind-up)
    static void Step(Loop& c, float e, float lo, float hi, float kp, float ki, float kd, float dt)
    {
        c.e = e;
        if(c.fresh)
            c.prev = e, c.d = 0.f, c.fresh = false;
        c.d += ((e - c.prev) / dt - c.d) * (1.f - expf(-dt / 0.05f));
        c.prev            = e;
        const float i_new = c.i + e * dt;
        const float u_try = kp * e + ki * i_new + kd * c.d;
        if((u_try <= hi || e < 0.f) && (u_try >= lo || e > 0.f))
            c.i = i_new;
        if(ki > 1e-6f)
            c.i = Clampf(c.i, lo / ki, hi / ki);
        const float want = Clampf(kp * e + ki * c.i + kd * c.d, lo, hi);
        c.u += (want - c.u) * (1.f - expf(-dt / 0.08f));
    }
    static void Release(Loop& c, float dt) // off: ease back to no correction
    {
        c.u += (0.f - c.u) * (1.f - expf(-dt / 0.3f));
        c.i = 0.f, c.d = 0.f, c.fresh = true;
    }
    void Update(bool on, int mask, float kp, float ki, float kd, float tilt, const float eq_hz[4], float dt)
    {
        if(acc_n > 0)
        {
            const float n = 1.f / float(acc_n), c = 1.f - expf(-dt / 0.3f);
            for(int a = 0; a < kA; a++)
                env[a] += (acc[a] * n - env[a]) * c, acc[a] = 0.f;
            p_full += (acc_sq * n - p_full) * c;
            pk_env = fmaxf(acc_pk, pk_env * expf(-dt / 0.2f));
            sll += (acc_ll * n - sll) * c, srr += (acc_rr * n - srr) * c, slr += (acc_lr * n - slr) * c;
            acc_sq = acc_pk = acc_ll = acc_rr = acc_lr = 0.f;
            acc_n  = 0;
        }
        level_db = PowToDb(p_full);
        tracking = on && level_db > -55.f;
        if(level_db > -55.f)
        {
            crest_db    = 20.f * log10f(pk_env + 1e-9f) - level_db;
            corr        = Clampf(slr / (sqrtf(sll * srr) + 1e-12f), -1.f, 1.f);
            float mean = 0.f, tmean = 0.f, db[kA];
            for(int a = 0; a < kA; a++)
                db[a] = FastPowToDb(env[a]), mean += db[a] / float(kA), tmean += tilt * oct_c[a] / float(kA);
            for(int a = 0; a < kA; a++)
                rel_db[a] = db[a] - mean, target_db[a] = tilt * oct_c[a] - tmean;
        }
        const bool use[kG] = {(mask & 1) != 0, (mask & 2) != 0, (mask & 4) != 0, (mask & 8) != 0, (mask & 16) != 0, (mask & 32) != 0};
        auto loop = [&](Loop& l, int g, float e, float lo_, float hi_) {
            if(!on || !use[g])
                Release(l, dt);
            else if(tracking)
                Step(l, e, lo_, hi_, kp, ki, kd, dt);
        };
        auto gap = [&](float hz) { return At(target_db, hz) - At(rel_db, hz); }; // + = quieter than the target
        for(int b = 0; b < 4; b++)
            loop(eq[b], G_EQ, gap(eq_hz[b]), -6.f, 6.f), eq_db[b] = eq[b].u;
        loop(comp, G_COMP, crest_db - kCrestTarget, -8.f, 8.f), comp_db = comp.u;
        for(int b = 0; b < 4; b++)
            loop(mb[b], G_MB, -gap(kPidMbHz[b]), -10.f, 10.f), mb_amt[b] = mb[b].u * 0.05f;
        loop(lo, G_CLAR, gap(80.f), -4.f, 4.f), lo_db = lo.u;
        loop(pres, G_CLAR, gap(3000.f), -4.f, 4.f), pres_db = pres.u;
        loop(dh, G_DH, -gap(3500.f), -6.f, 6.f), dh_db = dh.u;
        loop(width, G_WIDTH, (corr - kCorrTarget) * 10.f, -10.f, 10.f), width_add = width.u * 0.04f;
    }
};
} // namespace pg
