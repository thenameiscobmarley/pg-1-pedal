// PG-1 DSP building blocks and the 7 processing stages. Plain C++, no hardware, no allocation:
// the same code runs on the Seed3 (inside the audio interrupt) and in the Carla plugin.
//
// Signal flow (one Core::Process call):
//   in -> hum -> input level -> dyn eq -> comp -> multiband -> clarity -> saturate -> de-harsh -> width -> takeback
//      -> loudness -> [bypass] -> safety -> out
// Safety is after the bypass switch: it is always on.
#pragma once
#include <cmath>
#include <cstdint>

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
    void Reset() { ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0.f; }
    // |H| in dB, where t = tan(pi * hz / fs) (shared by every filter at that frequency)
    float MagDbT(float t) const
    {
        const float W = t / g, dre = 1.f - W * W, dim = k * W;
        const float nre = m0 * dre + m2, nim = (m0 * k + m1) * W;
        return 10.f * log10f((nre * nre + nim * nim + 1e-20f) / (dre * dre + dim * dim + 1e-20f));
    }
};

// ------------------------------------------------------------------ 0. input auto-level
// Brings any source to the same working level: quiet ones up (at most `boost`), hot ones down (at
// most `cut`). Slow and gated, so it rides the level like a careful engineer and never pumps up
// silence or noise. The 119 dB converter in the Seed3 has the headroom to do this digitally.
struct LevelStage
{
    float p = 0.f, gain_db = 0.f, cur = 1.f, in_db = -120.f;
    float c_pow = 0.f, c_gain = 0.f, fs = 48000.f;

    void Init(float sr)
    {
        fs = sr;
        p = 0.f, gain_db = 0.f, cur = 1.f, in_db = -120.f;
        c_pow = Coef(400.f, fs), c_gain = Coef(20.f, fs);
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
        gain_db = Clampf(gain_db, -cut, boost);
    }
    inline void Run(float& l, float& r)
    {
        p += (0.5f * (l * l + r * r) - p) * c_pow;
        cur += (DbToLin(gain_db) - cur) * c_gain;
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
        fs = sr;
        static const float dets[4] = {50.f, 100.f, 60.f, 120.f};
        for(int i = 0; i < 4; i++)
            det[i].Set(Svf::BP, fs, dets[i], 12.f), det[i].Reset(), e_det[i] = 0.f;
        Retune(f0);
        hiss_hp.Set(Svf::HP, fs, 6000.f, 0.707f), hiss_hp.Reset();
        hiss_shelf.Set(Svf::HSHELF, fs, 5000.f, 0.7f, 0.f), hiss_shelf.Reset();
        hiss_env = 0.f, hiss_db = 0.f, hiss_applied = 0.f;
        c_det     = Coef(60.f, fs);
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
            notch[h].Set(Svf::BELL, fs, fh, NotchQ(fh), 0.f), notch[h].Reset();
            narrow[h].Set(Svf::BP, fs, fh, NotchQ(fh)), narrow[h].Reset();
            broad[h].Set(Svf::BP, fs, fh, 2.f), broad[h].Reset();
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
        for(int b = 0; b < NoiseProfile::kBands; b++) // band levels: for learning, the gate and the display
        {
            const float d = hb_det[b].Run(0, m), sq = d * d;
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
        for(int i = 0; i < 4; i++)
        {
            const float d = det[i].Run(0, m);
            e_det[i] += (d * d - e_det[i]) * c_det;
        }
        for(int h = 0; h < kH; h++)
        {
            const float n = narrow[h].Run(0, m), b = broad[h].Run(0, m);
            en[h] += (n * n - en[h]) * c_det;
            eb[h] += (b * b - eb[h]) * c_det;
            l = notch[h].Run(0, l);
            r = notch[h].Run(1, r);
        }
        const float hf = hiss_hp.Run(0, m), p = hf * hf;
        hiss_env += (p - hiss_env) * (p > hiss_env ? c_hiss_up : c_hiss_dn);
        l = hiss_shelf.Run(0, l);
        r = hiss_shelf.Run(1, r);
    }
};

// ------------------------------------------------------------------ 2. dynamic eq
struct DynEqStage
{
    static constexpr int kB = 4;
    Svf   peak[kB], detect[kB];
    float env[kB] = {}, dyn_db[kB] = {}, applied[kB] = {}, cached_f[kB] = {}, cached_q[kB] = {};
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
            const float dl = detect[b].Run(0, il), dr = detect[b].Run(1, ir);
            const float sq = fmaxf(dl * dl, dr * dr);
            env[b] += (sq - env[b]) * (sq > env[b] ? att : rel);
            l = peak[b].Run(0, l);
            r = peak[b].Run(1, r);
        }
    }
};

// ------------------------------------------------------------------ 3. compressor
struct CompStage
{
    Svf   sc_hp;              // sidechain: ignore the deepest bass so it doesn't pump
    float p = 0.f;            // detector power
    float env_fast = 0.f, env_slow = 0.f, gr_db = 0.f, in_db = -120.f;
    float thr = -18.f, ratio = 2.5f, makeup = 0.f;
    float c_det = 0.f, c_att = 0.f, c_rel = 0.f, c_slow_att = 0.f, c_slow_rel = 0.f;
    bool  auto_rel = true;
    float fs = 48000.f;

    void Init(float sr)
    {
        fs = sr;
        sc_hp.Set(Svf::HP, fs, 90.f, 0.707f), sc_hp.Reset();
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
        const float hl = sc_hp.Run(0, l), hr = sc_hp.Run(1, r);
        const float sq = fmaxf(hl * hl, hr * hr);
        p += (sq - p) * c_det;
        in_db          = PowToDb(p) + 3.f;
        const float gr = Curve(in_db);
        Follow(env_fast, gr, c_rel, c_att); // gr is negative: "falling" = more reduction = attack
        float g = env_fast;
        if(auto_rel) // program-dependent: long squeezes recover slowly, single hits quickly
        {
            Follow(env_slow, gr, c_slow_rel, c_slow_att);
            g = fminf(env_fast, env_slow);
        }
        gr_db         = g;
        const float k = DbToLin(g + makeup);
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
            lo = lp[1].Run(ch, lp[0].Run(ch, x));
            hi = hp[1].Run(ch, hp[0].Run(ch, x));
        }
    };
    Lr4   x_mid, x_low, x_high;  // 1 kHz, then 120 Hz (low side) and 6 kHz (high side)
    Svf   ap_low, ap_high;       // the low side gets the 6 kHz phase, the high side the 120 Hz phase
    float p[kB] = {}, avg[kB] = {}, gr[kB] = {}, mk[kB] = {}, g[kB] = {1.f, 1.f, 1.f, 1.f}, amt[kB] = {};
    float c_det[kB] = {}, c_att[kB] = {}, c_rel[kB] = {}, c_avg = 0.f, c_warm = 0.f, c_g = 0.f, fs = 48000.f;
    int   warm[kB] = {}, warm_n = 9600;
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
            c_det[b] = Coef(det_ms[b], fs), c_att[b] = Coef(att_ms[b], fs), c_rel[b] = Coef(rel_ms[b], fs);
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
            if(amt[b] > 0.001f)
            {
                // above (its average + 8 dB, down to + 2 dB at full amount): ratio 1.5 .. 5
                const float over = PowToDb(p[b]) - (PowToDb(avg[b]) + 8.f - 6.f * amt[b]);
                const float target = over > 0.f ? -over * (1.f - 1.f / (1.5f + 3.5f * amt[b])) : 0.f;
                gr[b] += (target - gr[b]) * (target < gr[b] ? c_att[b] : c_rel[b]);
            }
            else
                gr[b] += (0.f - gr[b]) * c_rel[b];
            g[b] += (DbToLin(gr[b] + mk[b]) - g[b]) * c_g;
            ol += band[0][b] * g[b], orr += band[1][b] * g[b];
        }
        l = ol, r = orr;
    }
};

// ------------------------------------------------------------------ 4. clarity + bass
struct ClarityStage
{
    Svf   d_low, d_mud, d_pres, d_hi;              // detectors (mono)
    Svf   thump, mud, pres, warm_lo, warm_hi;      // the moves (stereo)
    Svf   det_lp, det_hp, det_out;                 // bass detail: harmonics of the bass (stereo)
    float low_fast = 0.f, low_slow = 0.f, p_mud = 0.f, p_pres = 0.f, p_hi = 0.f, p_full = 0.f, bass_pk = 0.f;
    float thump_db = 0.f, mud_db = 0.f, pres_db = 0.f, warm_lo_db = 0.f, warm_hi_db = 0.f;
    float ap[5] = {};
    float detail = 0.f, p_harm = 0.f, detail_view_db = 0.f;
    // mode: 0 dynamic (lifts and cuts), 1 add (only lifts, never cuts), 2 normalise (8-band tonal balance)
    static constexpr int kNB = 8;
    int   mode = 0;
    Svf   bal_det[kNB], bal[kNB];
    float p_bal[kNB] = {}, bal_db[kNB] = {}, bal_ap[kNB] = {}, c_bal = 0.f;
    bool  bal_active = false;
    float c_lf_up = 0.f, c_lf_dn = 0.f, c_ls_up = 0.f, c_ls_dn = 0.f, c_avg = 0.f, c_pk_dn = 0.f;
    float fs = 48000.f;

    void Init(float sr)
    {
        fs = sr;
        d_low.Set(Svf::LP, fs, 100.f, 0.707f), d_mud.Set(Svf::BP, fs, 300.f, 1.2f);
        d_pres.Set(Svf::BP, fs, 3200.f, 1.f), d_hi.Set(Svf::HP, fs, 7000.f, 0.707f);
        det_lp.Set(Svf::LP, fs, 120.f, 0.707f), det_hp.Set(Svf::HP, fs, 90.f, 0.707f), det_out.Set(Svf::LP, fs, 1500.f, 0.707f);
        Svf* all[] = {&d_low, &d_mud, &d_pres, &d_hi, &thump, &mud, &pres, &warm_lo, &warm_hi, &det_lp, &det_hp, &det_out};
        for(Svf* s : all)
            s->Reset();
        thump_db = mud_db = pres_db = warm_lo_db = warm_hi_db = 0.f;
        for(float& a : ap)
            a = 1000.f;
        low_fast = low_slow = p_mud = p_pres = p_hi = p_full = bass_pk = 0.f;
        p_harm = 0.f, detail_view_db = 0.f;
        c_lf_up = Coef(2.f, fs), c_lf_dn = Coef(40.f, fs);
        c_ls_up = Coef(150.f, fs), c_ls_dn = Coef(300.f, fs);
        c_avg = Coef(150.f, fs), c_pk_dn = Coef(60.f, fs);
        for(int b = 0; b < kNB; b++)
            bal_det[b].Set(Svf::BP, fs, BalHz(b), 1.2f), bal_det[b].Reset(), bal[b].Set(Svf::BELL, fs, BalHz(b), 1.2f, 0.f), bal[b].Reset(),
                p_bal[b] = 0.f, bal_db[b] = bal_ap[b] = 0.f;
        c_bal      = Coef(2000.f, fs);
        bal_active = false;
        Apply(true);
    }
    void Apply(bool force)
    {
        const float v[5] = {thump_db, mud_db, pres_db, warm_lo_db, warm_hi_db};
        for(int i = 0; i < 5; i++)
            if(force || fabsf(v[i] - ap[i]) > 0.05f)
            {
                switch(i)
                {
                    case 0: thump.Set(Svf::LSHELF, fs, 90.f, 0.7f, v[i]); break;
                    case 1: mud.Set(Svf::BELL, fs, 300.f, 0.9f, v[i]); break;
                    case 2: pres.Set(Svf::BELL, fs, 3200.f, 0.8f, v[i]); break;
                    case 3: warm_lo.Set(Svf::LSHELF, fs, 220.f, 0.7f, v[i]); break;
                    default: warm_hi.Set(Svf::HSHELF, fs, 8000.f, 0.7f, v[i]); break;
                }
                ap[i] = v[i];
            }
    }
    static float BalHz(int b)
    {
        static const float hz[kNB] = {60.f, 150.f, 350.f, 800.f, 1800.f, 4000.f, 8000.f, 14000.f};
        return hz[b];
    }
    // the knobs, in dB of lift: thump (most per hit), detail (harmonics), clarity (presence), warmth (low shelf)
    void Update(float thump_max, float detail_db, float clarity_db, float warmth_db, float block_s, int m = 0)
    {
        mode                  = m;
        detail                = detail_db / 6.f;
        const float a_clarity = clarity_db / 6.f, a_warmth = warmth_db / 4.f;
        const float c3  = 1.f - expf(-block_s / 0.003f), c80 = 1.f - expf(-block_s / 0.08f);
        const float c250 = 1.f - expf(-block_s / 0.25f), c2s = 1.f - expf(-block_s / 2.f);
        // thump: a short lift of the lows on each hit (fast level jumping above the slow level)
        const float transient = fmaxf(0.f, PowToDb(low_fast) - PowToDb(low_slow));
        const float t_thump   = fminf(thump_max, transient);
        thump_db += (t_thump - thump_db) * (t_thump > thump_db ? c3 : c80);
        const float full = PowToDb(p_full);
        float       t_mud = 0.f, t_pres = 0.f, t_whi = 0.f;
        if(full > -60.f)
        {
            // mud: 300 Hz holding more than its share -> cut. presence: always lifted by the knob's amount,
            // down to a quarter of it when the 3 kHz range is already strong (so bright music isn't pushed)
            t_mud  = -a_clarity * Clampf((PowToDb(p_mud) - full + 8.f) * 0.8f, 0.f, 6.f);
            t_pres = clarity_db * Clampf(1.f - ((PowToDb(p_pres) - full) + 18.f) / 6.f, 0.25f, 1.f);
            t_whi  = -a_warmth * Clampf((PowToDb(p_hi) - full + 24.f) * 0.5f, 0.f, 4.f); // only when bright
        }
        if(mode == 1) // add: only ever lift (no mud cut, no taming of the top)
            t_mud = 0.f, t_whi = 0.f;
        // normalise: pull the 8 bands toward a smooth, slightly warm slope (warmth = a warmer slope)
        float t_bal[kNB] = {};
        if(mode == 2 && full > -60.f)
        {
            t_mud = 0.f, t_pres = 0.f, t_whi = 0.f; // the balance does their job
            float dev[kNB], mean = 0.f;
            for(int b = 0; b < kNB; b++)
                dev[b] = PowToDb(p_bal[b]) + (4.5f + 1.5f * a_warmth) * log2f(BalHz(b) / 1000.f), mean += dev[b] / float(kNB);
            for(int b = 0; b < kNB; b++)
                t_bal[b] = -Clampf((dev[b] - mean) * 0.7f, -6.f, 6.f) * a_clarity;
        }
        bal_active = mode == 2;
        for(int b = 0; b < kNB; b++)
        {
            bal_db[b] += (t_bal[b] - bal_db[b]) * (1.f - expf(-block_s / 1.5f));
            bal_active = bal_active || fabsf(bal_db[b]) > 0.05f;
            if(fabsf(bal_db[b] - bal_ap[b]) > 0.05f)
                bal[b].Set(Svf::BELL, fs, BalHz(b), 1.2f, bal_db[b]), bal_ap[b] = bal_db[b];
        }
        const float c_moves = full > -60.f ? c250 : c2s;
        mud_db += (t_mud - mud_db) * c_moves;
        pres_db += (t_pres - pres_db) * c_moves;
        warm_hi_db += (t_whi - warm_hi_db) * c_moves;
        warm_lo_db += (warmth_db - warm_lo_db) * c250;
        // what the detail knob is adding right now: the bass's harmonics, as extra weight on the bass
        detail_view_db = detail > 0.001f ? 10.f * log10f((low_slow + p_harm + 1e-20f) / (low_slow + 1e-20f)) : 0.f;
        Apply(false);
    }
    inline void Run(float& l, float& r)
    {
        const float m  = 0.5f * (l + r);
        const float lo = d_low.Run(0, m), lp = lo * lo;
        Follow(low_fast, lp, c_lf_up, c_lf_dn);
        Follow(low_slow, lp, c_ls_up, c_ls_dn);
        const float mu = d_mud.Run(0, m), pr = d_pres.Run(0, m), hi = d_hi.Run(0, m);
        p_mud += (mu * mu - p_mud) * c_avg;
        p_pres += (pr * pr - p_pres) * c_avg;
        p_hi += (hi * hi - p_hi) * c_avg;
        p_full += (m * m - p_full) * c_avg;

        float x[2] = {l, r};
        if(detail > 0.001f)
        {
            // bass detail: 2nd + 3rd harmonics of the bass (Chebyshev, level-tracking) so it is heard
            // on small speakers too; only the harmonics are kept (90 Hz - 1.5 kHz)
            const float bl = det_lp.Run(0, l), br = det_lp.Run(1, r);
            const float pk = fmaxf(fabsf(bl), fabsf(br));
            bass_pk        = pk > bass_pk ? pk : bass_pk + (pk - bass_pk) * c_pk_dn;
            const float b[2] = {bl, br};
            for(int ch = 0; ch < 2; ch++)
            {
                const float xn = Clampf(b[ch] / (bass_pk + 1e-5f), -1.f, 1.f);
                float       h  = (0.55f * (2.f * xn * xn - 1.f) + 0.45f * (4.f * xn * xn * xn - 3.f * xn)) * bass_pk;
                h              = det_out.Run(ch, det_hp.Run(ch, h));
                h *= detail * 1.5f;
                x[ch] += h;
                if(ch == 0)
                    p_harm += (h * h - p_harm) * c_avg;
            }
        }
        for(int ch = 0; ch < 2; ch++)
        {
            float y = thump.Run(ch, x[ch]);
            y       = mud.Run(ch, y);
            y       = pres.Run(ch, y);
            y       = warm_lo.Run(ch, y);
            x[ch]   = warm_hi.Run(ch, y);
        }
        if(mode == 2)
            for(int b = 0; b < kNB; b++)
            {
                const float d = bal_det[b].Run(0, m);
                p_bal[b] += (d * d - p_bal[b]) * c_bal;
            }
        if(bal_active)
            for(int b = 0; b < kNB; b++)
                x[0] = bal[b].Run(0, x[0]), x[1] = bal[b].Run(1, x[1]);
        l = x[0], r = x[1];
    }
};

// ------------------------------------------------------------------ 5. saturation (anti-aliased)
struct SaturateStage
{
    Svf   dc, tone;
    float x1[2] = {};
    float gain = 1.f, bias = 0.f, tb = 0.f, norm = 1.f, mix = 1.f, tone_hz = 0.f;
    float drive_pk = 0.f; // for the display
    float fs = 48000.f;

    static float LogCosh(float u)
    {
        const float a = fabsf(u);
        return a + log1pf(expf(-2.f * a)) - 0.69314718f;
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
            const float y = fabsf(d) > 1e-4f ? (F(x) - F(x1[ch])) / d : Fn(0.5f * (x + x1[ch]));
            x1[ch]        = x;
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
    float p[kD] = {}, cut_db[kD] = {}, applied[kD] = {}, p_full = 0.f, comfort_db = 0.f, comfort_ap = 1000.f;
    float c_att = 0.f, c_rel = 0.f, c_full = 0.f;
    float fs = 48000.f;

    void Init(float sr)
    {
        fs = sr;
        for(int i = 0; i < kD; i++)
            det[i].Set(Svf::BP, fs, kDeHarshF[i], 3.f), det[i].Reset(), cut[i].Set(Svf::BELL, fs, kDeHarshF[i], 3.f, 0.f), cut[i].Reset(),
                p[i] = 0.f, cut_db[i] = 0.f, applied[i] = 0.f;
        comfort.Set(Svf::BELL, fs, 3000.f, 0.7f, 0.f), comfort.Reset();
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
        const float m = 0.5f * (l + r);
        p_full += (m * m - p_full) * c_full;
        for(int i = 0; i < kD; i++)
        {
            const float d = det[i].Run(0, m), sq = d * d;
            p[i] += (sq - p[i]) * (sq > p[i] ? c_att : c_rel);
            l = cut[i].Run(0, l);
            r = cut[i].Run(1, r);
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
    float lo_db = 0.f, hi_db = 0.f, lo_ap = 1000.f, hi_ap = 1000.f, p = 0.f, c_p = 0.f, eff = 80.f, fs = 48000.f;

    void Init(float sr)
    {
        fs = sr;
        lo.Reset(), hi.Reset();
        lo.Set(Svf::LSHELF, fs, 90.f, 0.7f, 0.f), hi.Set(Svf::HSHELF, fs, 9000.f, 0.7f, 0.f);
        lo_db = hi_db = 0.f, lo_ap = hi_ap = 0.f, p = 0.f;
        c_p = Coef(1000.f, fs);
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
        const float m = 0.5f * (l + r);
        p += (m * m - p) * c_p;
        l = hi.Run(0, lo.Run(0, l)), r = hi.Run(1, lo.Run(1, r));
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
    Svf   u_in[4], u_out[4], i_in[3], i_out[3];
    float p_u_in = 0.f, p_i_in = 0.f, p_u_out = 0.f, p_i_out = 0.f, c_det = 0.f;
    float ultra_in_db = -200.f, infra_in_db = -200.f, ultra_out_db = -200.f, infra_out_db = -200.f;
    bool  ultra_possible = true; // false when the sample rate can't even carry ultrasonic sound
    Svf   d_low, d_high, woof, tweet;
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
            u_in[i].Set(Svf::HP, fs, 21000.f, q8[i]), u_out[i].Set(Svf::HP, fs, 21000.f, q8[i]), u_in[i].Reset(), u_out[i].Reset();
        for(int i = 0; i < 3; i++)
            i_in[i].Set(Svf::LP, fs, 10.f, q6[i]), i_out[i].Set(Svf::LP, fs, 5.f, q6[i]), i_in[i].Reset(), i_out[i].Reset();
        u_over_ms = i_over_ms = 0.f;
        p_u_in = p_i_in = p_u_out = p_i_out = 0.f;
        c_det = Coef(50.f, fs);
        d_low.Set(Svf::LP, fs, 150.f, 0.707f), d_low.Reset();
        d_high.Set(Svf::HP, fs, 4500.f, 0.707f), d_high.Reset();
        woof.Set(Svf::LSHELF, fs, 150.f, 0.7f, 0.f), woof.Reset();
        tweet.Set(Svf::HSHELF, fs, 4500.f, 0.7f, 0.f), tweet.Reset();
        p_low = p_high = p_ear = p_fast = dc_avg = 0.f;
        woof_db = tweet_db = ear_db = blast_db = 0.f, woof_ap = tweet_ap = 0.f;
        gl = 1.f, pos = 0;
        look = int(fs * 0.001f + 0.5f);
        look = look < 1 ? 1 : (look > kMaxLook ? kMaxLook : look);
        for(int i = 0; i < kMaxLook; i++)
            delay[0][i] = delay[1][i] = 0.f, need[i] = 1.f;
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
        // outside the hearing range got out and STAYED out (only a failed filter does that; the steep 20 Hz
        // filter rings for a moment after a loud, sudden bass hit, and that dies away well within these times)
        const float block_ms = block_s * 1000.f;
        u_over_ms            = ultra_out_db > -50.f ? u_over_ms + block_ms : 0.f;
        i_over_ms            = infra_out_db > -40.f ? i_over_ms + block_ms : 0.f;
        if(u_over_ms >= 100.f || i_over_ms >= 300.f)
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
        limit_db = 20.f * log10f(gl + 1e-9f);
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
            delay[0][i] = delay[1][i] = 0.f, need[i] = 1.f;
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
            float u = mi, i = mi;
            for(Svf& f : u_in)
                u = f.Run(0, u);
            for(Svf& f : i_in)
                i = f.Run(0, i);
            p_u_in += (u * u - p_u_in) * c_det, p_i_in += (i * i - p_i_in) * c_det;
        }
        for(Svf& f : hp)
            l = f.Run(0, l), r = f.Run(1, r);
        for(Svf& f : lp)
            l = f.Run(0, l), r = f.Run(1, r);
        // 3. speakers: the long-term power in the woofer and tweeter ranges kept under their limits
        //    (voice coils stay cool)
        l = tweet.Run(0, woof.Run(0, l));
        r = tweet.Run(1, woof.Run(1, r));
        const float m = 0.5f * (l + r), lo = d_low.Run(0, m), hi = d_high.Run(0, m);
        p_low += (lo * lo - p_low) * c_low;
        p_high += (hi * hi - p_high) * c_high;
        const float pw = fmaxf(l * l, r * r);
        p_ear += (pw - p_ear) * c_ear;
        p_fast += (pw - p_fast) * c_fast;
        // 3. ears: long-term loudness cap + blast guard + glitch mute
        mute += (mute_target - mute) * (mute_target < mute ? c_mute_dn : c_mute_up);
        const float k = DbToLin(ear_db + blast_db) * mute;
        l *= k, r *= k;
        // 4. 1 ms look-ahead peak limiter, then a hard ceiling that nothing can pass
        const float pk = fmaxf(fabsf(l), fabsf(r));
        need[pos]      = pk > ceiling * 0.98f ? ceiling * 0.98f / pk : 1.f; // a hair under, so the hard ceiling stays a backstop
        delay[0][pos] = l, delay[1][pos] = r;
        float mn = 1.f;
        for(int i = 0; i < look; i++)
            mn = need[i] < mn ? need[i] : mn;
        gl += (mn - gl) * (mn < gl ? c_att : c_rel);
        const int rd = (pos + 1) % look;
        l            = Clampf(delay[0][rd] * gl, -ceiling, ceiling);
        r            = Clampf(delay[1][rd] * gl, -ceiling, ceiling);
        pos          = rd;
        // 5. check what actually leaves: nothing outside the hearing range may get out
        const float mo = 0.5f * (l + r);
        float       uo = mo, io = mo;
        for(Svf& f : u_out)
            uo = f.Run(0, uo);
        for(Svf& f : i_out)
            io = f.Run(0, io);
        p_u_out += (uo * uo - p_u_out) * c_det, p_i_out += (io * io - p_i_out) * c_det;
    }
};
} // namespace pg
