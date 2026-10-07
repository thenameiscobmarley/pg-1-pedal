// PG-1 configs + saving. A config is every sound setting and which stages are on; the config tab keeps 8
// of them. The whole state (current settings, the 8 configs, the learned hum / hiss profile) packs into one
// block of bytes with a checksum: the pedal writes it to its flash a few seconds after you stop changing
// things (so it comes back after power-off), the plugin keeps it in the DAW session.
#include "PgCore.h"
#include <cstring>

namespace pg
{
static constexpr uint32_t kMagic = 0x53314750; // "PG1S"

// these belong to the pedal, not to a sound, so loading a config leaves them alone
static bool PrefParam(int p)
{
    return p == P_CF_SLOT || p == P_CF_SAVE || p == P_CF_THEME || p == P_CF_KNOBS || p == P_H_ROW || p == P_TOUR_DONE;
}

void Core::TakeSnap(Snap& s) const
{
    for(int p = 0; p < P_COUNT; p++)
        s.v[p] = int16_t(params_[p].value);
    for(int t = 0; t < kTabs; t++)
        s.st[t] = stage_on_[t] ? 1 : 0;
    s.on = effect_on_ ? 1 : 0;
}

void Core::PutSnap(const Snap& s)
{
    for(int p = 0; p < P_COUNT; p++)
        if(!PrefParam(p))
            SetParam(p, s.v[p]);
    for(int t = 0; t < kTabs; t++)
        SetStageOn(t, s.st[t] != 0);
    effect_on_ = s.on != 0, redraw_title_ = true;
}

static uint32_t Crc32(const uint8_t* d, int n)
{
    uint32_t c = 0xFFFFFFFFu;
    for(int i = 0; i < n; i++)
    {
        c ^= d[i];
        for(int k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

namespace
{
struct Writer
{
    uint8_t* p;
    int      n, max;
    bool     ok = true;
    void     Bytes(const void* d, int k)
    {
        if(n + k > max)
        {
            ok = false;
            return;
        }
        memcpy(p + n, d, size_t(k)), n += k;
    }
    template <typename T>
    void Put(T v) { Bytes(&v, int(sizeof(T))); }
};
struct Reader
{
    const uint8_t* p;
    int            n, max;
    bool           ok = true;
    void           Bytes(void* d, int k)
    {
        if(n + k > max)
        {
            ok = false;
            memset(d, 0, size_t(k));
            return;
        }
        memcpy(d, p + n, size_t(k)), n += k;
    }
    template <typename T>
    T Get()
    {
        T v{};
        Bytes(&v, int(sizeof(T)));
        return v;
    }
};
} // namespace

int Core::SaveState(uint8_t* out, int max) const
{
    Writer w{out, 0, max};
    w.Put<uint32_t>(kMagic);
    w.Put<uint16_t>(1); // format version
    w.Put<uint16_t>(uint16_t(P_COUNT));
    w.Put<uint16_t>(uint16_t(kTabs));
    w.Put<uint16_t>(uint16_t(kSlots));
    Snap cur;
    TakeSnap(cur);
    auto snap = [&](const Snap& s) {
        for(int p = 0; p < P_COUNT; p++)
            w.Put<int16_t>(s.v[p]);
        w.Bytes(s.st, kTabs);
        w.Put<uint8_t>(s.on);
    };
    snap(cur);
    for(int k = 0; k < kSlots; k++)
        w.Put<uint8_t>(slots_[k].used), snap(slots_[k]);
    const NoiseProfile& pr = hum_.prof;
    w.Put<uint8_t>(pr.valid ? 1 : 0);
    w.Put<uint8_t>(uint8_t(pr.n_tones));
    w.Bytes(pr.tone_hz, int(sizeof(pr.tone_hz)));
    w.Bytes(pr.tone_db, int(sizeof(pr.tone_db)));
    w.Bytes(pr.floor_db, int(sizeof(pr.floor_db)));
    const uint32_t crc = Crc32(out, w.n);
    w.Put<uint32_t>(crc);
    return w.ok ? w.n : 0;
}

bool Core::LoadState(const uint8_t* in, int n)
{
    if(n < 16)
        return false;
    Reader r{in, 0, n};
    if(r.Get<uint32_t>() != kMagic || r.Get<uint16_t>() != 1)
        return false;
    const int pc = r.Get<uint16_t>(), tc = r.Get<uint16_t>(), sc = r.Get<uint16_t>();
    if(pc <= 0 || pc > 1024 || tc <= 0 || tc > 64 || sc <= 0 || sc > 64)
        return false;
    const int body = 12 + (pc * 2 + tc + 1) * (1 + sc) + sc + 2 + int(sizeof(NoiseProfile::tone_hz) * 2 + sizeof(NoiseProfile::floor_db));
    if(body + 4 > n || Crc32(in, body) != uint32_t(in[body] | in[body + 1] << 8 | in[body + 2] << 16 | uint32_t(in[body + 3]) << 24))
        return false; // damaged or cut short: keep the defaults
    // older saves may have fewer parameters / tabs (new ones only ever go at the end): read what's there
    auto snap = [&](Snap& s) {
        TakeSnap(s); // anything the save doesn't have keeps its current value
        for(int p = 0; p < pc; p++)
        {
            const int16_t v = r.Get<int16_t>();
            if(p < P_COUNT)
                s.v[p] = v;
        }
        for(int t = 0; t < tc; t++)
        {
            const uint8_t v = r.Get<uint8_t>();
            if(t < kTabs)
                s.st[t] = v;
        }
        s.on = r.Get<uint8_t>();
    };
    Snap cur;
    snap(cur);
    for(int k = 0; k < sc; k++)
    {
        Snap s{};
        const uint8_t used = r.Get<uint8_t>();
        snap(s);
        if(k < kSlots)
            slots_[k] = s, slots_[k].used = used;
    }
    NoiseProfile pr;
    pr.valid   = r.Get<uint8_t>() != 0;
    pr.n_tones = r.Get<uint8_t>();
    r.Bytes(pr.tone_hz, int(sizeof(pr.tone_hz)));
    r.Bytes(pr.tone_db, int(sizeof(pr.tone_db)));
    r.Bytes(pr.floor_db, int(sizeof(pr.floor_db)));
    if(!r.ok)
        return false;
    pr.n_tones = pr.n_tones > NoiseProfile::kTones ? NoiseProfile::kTones : pr.n_tones;
    // the current settings, including the pedal's own preferences (theme, knob direction)
    for(int p = 0; p < P_COUNT; p++)
        SetParam(p, cur.v[p]);
    for(int t = 0; t < kTabs; t++)
        SetStageOn(t, cur.st[t] != 0);
    effect_on_ = cur.on != 0;
    if(pr.valid)
        SetProfile(pr);
    state_dirty_ = false;
    redraw_all_  = true;
    return true;
}
} // namespace pg
