#pragma once
#include <cmath>
#include <vector>
#include "PgCore.h"

/** Runs the core exactly as the Seed3 does: 48 kHz, 48-sample blocks (main.cpp: SetAudioBlockSize(48),
    SAI_48KHZ), whatever the host's sample rate and block size are. The host's audio is resampled to 48 kHz
    (4-point Hermite, with a 20 kHz low-pass first when the host runs faster), cut into 48-sample blocks,
    processed, and resampled back. At a 48 kHz host rate nothing is resampled: samples pass through as is.
    Costs a fixed delay of about one Seed3 block (reported to the host as latency). */
class Seed3Runner
{
public:
    static constexpr double kRate  = 48000.0;
    static constexpr int    kBlock = 48;

    void prepare (double hostRate)
    {
        step = hostRate / kRate;   // host samples per Seed3 sample
        same = std::abs (step - 1.0) < 1e-9;
        for (int ch = 0; ch < 2; ++ch)
        {
            inHist[ch].assign (4, 0.f), inHist[ch].reserve (1 << 16);   // no allocation on the audio thread
            outHist[ch].assign ((size_t) kPrefill, 0.f), outHist[ch].reserve (1 << 16);   // a block of slack, so the host never waits on a block
            for (auto& f : aa[ch])
                f.Set (pg::Svf::LP, (float) hostRate, 20000.f, 0.7071f), f.Reset();
        }
        inPos = 1.0, outPos = 1.0;
        seedN = 0;
    }

    /** host-rate latency in samples (the Seed3-side delay plus the safety limiter's look-ahead) */
    int latency (const pg::Core& c) const { return (int) std::lround ((kPrefill - 3 + c.LatencySamples()) * step); } // 96 samples at 48 kHz

    void process (pg::Core& core, const float* const* in, float* const* out, int n, juce::uint32 nowMs)
    {
        // 1. host -> 48 kHz, in whole Seed3 blocks
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
            {
                float x = in[ch][i];
                if (step > 1.0 + 1e-9)   // a faster host: keep what the Seed3 couldn't carry from folding back
                    x = aa[ch][1].Run (0, aa[ch][0].Run (0, x));
                inHist[ch].push_back (x);
            }
        const int have = (int) inHist[0].size();
        while (inPos + 2.0 < have)
        {
            for (int ch = 0; ch < 2; ++ch)
                seedIn[ch][seedN] = same ? inHist[ch][(size_t) inPos] : hermite (inHist[ch], inPos);
            inPos += step;
            if (++seedN == kBlock)   // a full Seed3 block: run the pedal, exactly 48 samples
            {
                const float* si[2] = { seedIn[0], seedIn[1] };
                float*       so[2] = { seedOut[0], seedOut[1] };
                core.Process (si, so, (size_t) kBlock, nowMs);
                analogLeveller (core, si, so);
                for (int ch = 0; ch < 2; ++ch)
                    outHist[ch].insert (outHist[ch].end(), seedOut[ch], seedOut[ch] + kBlock);
                seedN = 0;
            }
        }
        trim (inHist, inPos);
        // 2. 48 kHz -> host
        const int haveOut = (int) outHist[0].size();
        for (int i = 0; i < n; ++i)
        {
            const bool ok = outPos + 2.0 < haveOut;
            for (int ch = 0; ch < 2; ++ch)
                out[ch][i] = ! ok ? 0.f : (same ? outHist[ch][(size_t) outPos] : hermite (outHist[ch], outPos));
            if (ok)
                outPos += 1.0 / step;
        }
        trim (outHist, outPos);
    }

private:
    // the carrier board's analog leveller + the face's level lights, as main.cpp runs them (every 30 ms): gentle 2:1
    // above -14 dBFS rms, the LDRs' ~30 ms ride glided here; lights green above -45 dBFS peak, red near clipping
    void analogLeveller (pg::Core& core, const float* const* si, float* const* so)
    {
        float ms = 0.f;
        for (int i = 0; i < kBlock; ++i)
        {
            ms += 0.5f * (so[0][i] * so[0][i] + so[1][i] * so[1][i]);
            for (int ch = 0; ch < 2; ++ch)
            {
                inPk  = std::max (inPk, std::abs (si[ch][i]));
                outPk = std::max (outPk, std::abs (so[ch][i]));
            }
        }
        outMs += (ms / (float) kBlock - outMs) * 0.05f;
        const float g = std::pow (10.f, -levCut / 20.f);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kBlock; ++i)
            {
                levG += (g - levG) * 0.0007f;   // ~30 ms
                so[ch][i] *= levG;
            }
        if (++levBlocks < 30)
            return;
        levBlocks = 0, levT += 30;
        const float db = 10.f * std::log10 (outMs + 1e-12f);
        levCut = core.LevellerOn() && db > -14.f ? std::min (20.f, (db + 14.f) * 0.5f) : 0.f;
        if (inPk > 0.708f)  inHot = levT;
        if (outPk > 0.891f) outHot = levT;
        uint8_t lights = (inPk > 0.0056f ? 1 : 0) | (inHot && levT - inHot < 300 ? 2 : 0)
                       | (outPk > 0.0056f ? 4 : 0) | (outHot && levT - outHot < 300 ? 8 : 0);
        inPk = outPk = 0.f;
        core.ReportLeveller (true, levCut, db, lights);
    }
    float levCut = 0.f, levG = 1.f, outMs = 0.f, inPk = 0.f, outPk = 0.f;
    int   levBlocks = 0;
    juce::uint32 levT = 0, inHot = 0, outHot = 0;

    static constexpr int kPrefill = kBlock + 4;
    double step = 1.0, inPos = 1.0, outPos = 1.0;
    bool   same = true;
    std::vector<float> inHist[2], outHist[2];
    float  seedIn[2][kBlock] {}, seedOut[2][kBlock] {};
    int    seedN = 0;
    pg::Svf aa[2][2];

    static float hermite (const std::vector<float>& h, double pos)   // 4-point, 3rd-order Hermite
    {
        const int   i = (int) pos;
        const float t = (float) (pos - i), y0 = h[(size_t) i - 1], y1 = h[(size_t) i], y2 = h[(size_t) i + 1], y3 = h[(size_t) i + 2];
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }
    static void trim (std::vector<float> (&h)[2], double& pos)   // drop what's been read (keep one sample behind)
    {
        const int drop = (int) pos - 1;
        if (drop <= 0)
            return;
        for (auto& v : h)
            v.erase (v.begin(), v.begin() + drop);
        pos -= drop;
    }
};
