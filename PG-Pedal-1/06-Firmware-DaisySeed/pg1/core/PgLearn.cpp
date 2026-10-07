// PG-1 hum + hiss "learn": with only the hum / hiss playing, listen for 3 s and learn exactly which tones
// are there (mains hum and its harmonics at their real frequency, monitor / charger whine, anything tonal)
// and how loud the hiss is in 8 bands. After that the hum tab notches those tones all the way down and
// gates each band to silence whenever nothing louder than the learned noise is playing.
#include "PgCore.h"
#include <cmath>
#include <cstring>

namespace pg
{
static constexpr uint32_t kLearnMs = 3000;
static PG_BIG_BSS float pw_lo_[2048 / 2], pw_hi_[1024 / 2]; // averaged power spectra while learning

static void Fft(float* re, float* im, int n)
{
    for(int i = 1, j = 0; i < n; i++)
    {
        int bit = n >> 1;
        for(; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if(i < j)
        {
            float t = re[i];
            re[i] = re[j], re[j] = t;
            t = im[i], im[i] = im[j], im[j] = t;
        }
    }
    for(int len = 2; len <= n; len <<= 1)
    {
        const float ang = -2.f * kPi / float(len), wr = cosf(ang), wi = sinf(ang);
        for(int i = 0; i < n; i += len)
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
}

// power spectrum of a ring buffer (Hann window), added into acc[0 .. n/2)
static void AddSpectrum(const float* ring, int ring_n, int pos, float* acc)
{
    static PG_BIG_BSS float re[2048], im[2048]; // UI thread only
    for(int i = 0; i < ring_n; i++)
    {
        const float s = ring[(pos + i) & (ring_n - 1)];
        re[i]         = (fabsf(s) < 8.f ? s : 0.f) * (0.5f - 0.5f * cosf(2.f * kPi * float(i) / float(ring_n - 1)));
        im[i]         = 0.f;
    }
    Fft(re, im, ring_n);
    for(int k = 0; k < ring_n / 2; k++)
        acc[k] += re[k] * re[k] + im[k] * im[k];
}

void Core::StartLearn(uint32_t now)
{
    memset(pw_lo_, 0, sizeof(pw_lo_));
    memset(pw_hi_, 0, sizeof(pw_hi_));
    learn_frames_ = 0;
    for(float& a : hum_.learn_acc)
        a = 0.f;
    hum_.learn_lvl = hum_.learn_lvl2 = 0.f;
    hum_.learn_n   = 0;
    hum_.learning  = true;
    SetStageOn(T_HUM, true); // its band detectors have to be running
    learn_state_ = 1, learn_t0_ = now, last_learn_fft_ = 0;
}

void Core::UpdateLearn(uint32_t now)
{
    if(learn_state_ != 1)
        return;
    if(now - last_learn_fft_ >= 60) // average the spectrum: fine low end (decimated) + full range
    {
        last_learn_fft_ = now;
        AddSpectrum(ring_lo_, kRingLo, lo_pos_, pw_lo_);
        AddSpectrum(ring_in_, kRing, ring_pos_, pw_hi_);
        learn_frames_++;
    }
    if(now - learn_t0_ < kLearnMs)
        return;
    hum_.learning = false;
    learn_msg_t0_ = now;
    const int n   = hum_.learn_n;
    if(n < 10 || learn_frames_ < 5)
    {
        learn_state_ = 3, learn_msg_ = "learn failed: no audio came through";
        return;
    }
    const float mean = hum_.learn_lvl / float(n), sd = sqrtf(fmaxf(0.f, hum_.learn_lvl2 / float(n) - mean * mean));
    if(mean < -110.f)
    {
        learn_state_ = 3, learn_msg_ = "nothing to learn: it's already silent";
        return;
    }
    if(sd > 3.f)
    {
        learn_state_ = 3, learn_msg_ = "it changed while learning: only hum/hiss please";
        return;
    }

    // tones: bins standing 10 dB or more above the median of their neighbourhood
    struct Cand
    {
        float hz, prom;
    } cand[24];
    int  nc   = 0;
    auto scan = [&](const float* pw, int bins, float bin_hz, float f_lo, float f_hi, int half) {
        for(int k = half + 2; k < bins - half - 2; k++)
        {
            const float hz = float(k) * bin_hz;
            if(hz < f_lo || hz > f_hi)
                continue;
            const float p = pw[k];
            if(!(p > pw[k - 1] && p >= pw[k + 1] && p > pw[k - 2] && p >= pw[k + 2]))
                continue;
            float nb[40];
            int   m = 0;
            for(int j = -half; j <= half; j++)
                if(j < -2 || j > 2)
                    nb[m++] = pw[k + j];
            for(int a = 1; a < m; a++) // insertion sort (small)
                for(int b = a; b > 0 && nb[b] < nb[b - 1]; b--)
                {
                    const float t = nb[b];
                    nb[b] = nb[b - 1], nb[b - 1] = t;
                }
            const float med = nb[m / 2] + 1e-30f, prom = 10.f * log10f(p / med);
            if(prom < 10.f)
                continue;
            // exact frequency: parabola through the peak and its neighbours (in dB)
            const float a = 10.f * log10f(pw[k - 1] + 1e-30f), b = 10.f * log10f(p), c = 10.f * log10f(pw[k + 1] + 1e-30f);
            const float den = a - 2.f * b + c, off = fabsf(den) > 1e-6f ? Clampf(0.5f * (a - c) / den, -0.5f, 0.5f) : 0.f;
            if(nc < 24)
                cand[nc++] = {(float(k) + off) * bin_hz, prom};
        }
    };
    const float lo_rate = sample_rate_ / float(kDecim);
    scan(pw_lo_, kRingLo / 2, lo_rate / float(kRingLo), 20.f, fminf(2900.f, lo_rate * 0.45f), 16);
    scan(pw_hi_, kRing / 2, sample_rate_ / float(kRing), 3000.f, fminf(20000.f, sample_rate_ * 0.45f), 8);
    for(int a = 1; a < nc; a++) // strongest first
        for(int b = a; b > 0 && cand[b].prom > cand[b - 1].prom; b--)
        {
            const Cand t = cand[b];
            cand[b] = cand[b - 1], cand[b - 1] = t;
        }

    NoiseProfile& pr = hum_.prof;
    pr.valid         = false; // the audio thread ignores it while it's being filled
    pr.n_tones       = nc < NoiseProfile::kTones ? nc : NoiseProfile::kTones;
    for(int t = 0; t < pr.n_tones; t++)
        pr.tone_hz[t] = cand[t].hz, pr.tone_db[t] = cand[t].prom;
    for(int b = 0; b < NoiseProfile::kBands; b++)
        pr.floor_db[b] = hum_.learn_acc[b] / float(n);
    hum_.applied_learned = false;
    pr.valid             = true;

    // "cut it completely": notches all the way, hiss gated hard (both can be turned back down)
    SetParam(P_HUM_MAINS, 3);
    if(params_[P_HUM_DEPTH].value < 40)
        SetParam(P_HUM_DEPTH, 40);
    if(params_[P_HISS_CUT].value < 30)
        SetParam(P_HISS_CUT, 30);
    learn_state_ = 2, learn_msg_ = "learned: hum + hiss now cut";
    state_dirty_ = true; // keep it over power-off
    redraw_panel_ = true;
}
} // namespace pg
