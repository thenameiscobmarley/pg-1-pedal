// PG-1 configs + saving. A config is every sound setting and which stages are on; the config tab keeps 8
// of them. The whole state (current settings, the 8 configs, the learned hum / hiss profile) packs into one
// block of bytes with a checksum: the pedal writes it to its flash a few seconds after you stop changing
// things (so it comes back after power-off), the plugin keeps it in the DAW session.
//
// Saves survive new builds. Every setting and every tab has a permanent name (kParamKeys / kTabKeys below);
// a save stores values by those names, so a later build can add, remove or reorder settings and still read
// it: names it knows are loaded, names it doesn't are skipped, settings the save doesn't have keep their
// defaults. Each setting also has a version (kParamVersions): when a build changes what a setting means,
// bump its version and add a rule to Migrate(), and older saves are converted instead of misread.
//
// RULES for future changes: never rename a key or reuse an old one for something else; when a setting's
// meaning or scale changes, bump its version and teach Migrate() the old one.
#include "PgCore.h"
#include <cstdio>
#include <cstring>

namespace pg
{
static constexpr uint32_t kMagic = 0x53314750; // "PG1S"
static constexpr uint16_t kFormat = 2;         // 1 = by position (the first builds), 2 = by name

// ---------------------------------------------------------------- the permanent names
// eq1.freq .. eq4.range come first (made in ParamKey), then these, in P_ order
static const char* const kRestKeys[P_COUNT - P_EQ_END] = {
    "hum.mode",    "hum.depth",     "hum.hiss",      "hum.hissthr",                  // P_HUM_*
    "comp.thresh", "comp.ratio",    "comp.attack",   "comp.release",                 // P_C_*
    "clar.thump",  "clar.detail",   "clar.clarity",  "clar.warmth",                  // clarity
    "sat.drive",   "sat.even",      "sat.tone",      "sat.mix",                      // saturate
    "dh.depth",    "dh.sens",       "dh.speed",      "dh.comfort",                   // de-harsh
    "safe.ceiling", "safe.ears",    "safe.woofer",   "safe.tweeter",                 // safety
    "vis.view",    "vis.fall",      "vis.range",     "vis.source",                   // visual
    "in.target",   "in.boost",      "in.cut",        "in.speed",                     // input
    "health.row",  "health.sens",   "health.popups", "health.view",                  // health
    "hum.margin",  "clar.mode",                                                      // learned margin, clarity mode
    "tb.punch",    "tb.detail",     "tb.air",        "tb.space",                     // takeback
    "width.width", "width.mono",    "width.air",     "width.guard",                  // width
    "loud.listen", "loud.bass",     "loud.treble",   "loud.follow",                  // loudness
    "cfg.slot",    "cfg.save",      "cfg.theme",     "cfg.knobs",                    // config
    "ui.tour",                                                                       // first power-up tour shown
    "mb.bass",     "mb.lmid",       "mb.hmid",       "mb.high",                      // multiband
    "cfg.dpad",                                                                      // touch d-pad mode
    "cfg.screen",                                                                    // screen link fast / safe
    "pid.p",       "pid.i",         "pid.d",         "pid.group",                    // pid gains + group cursor
    "pid.mask",    "pid.tilt",                                                      // pid: steered groups, target balance
    "clar.bands",                                                                    // clarity: how many bands
};
static_assert(sizeof(kRestKeys) / sizeof(kRestKeys[0]) == P_COUNT - P_EQ_END, "every setting needs a permanent name");

static const char* const kTabKeys[kTabs] = {"hum",      "eq",    "comp", "clarity", "sat",    "deharsh", "safety", "vis",
                                            "input",    "health", "takeback", "width", "loud", "config", "mband", "pid"};
static_assert(sizeof(kTabKeys) / sizeof(kTabKeys[0]) == kTabs, "every tab needs a permanent name");

// a setting whose meaning changed gets a higher version here (everything else is 1)
struct KeyVersion
{
    const char* key;
    uint8_t     version;
};
static const KeyVersion kParamVersions[] = {
    {"clar.thump", 2}, {"clar.detail", 2}, {"clar.clarity", 2}, {"clar.warmth", 2}, // 2: tenths of a dB (1 was %)
};

static void ParamKey(int p, char* out, int n)
{
    static const char* const which[kPerBand] = {"freq", "gain", "q", "thresh", "range"};
    if(p < P_EQ_END)
        snprintf(out, size_t(n), "eq%d.%s", p / kPerBand + 1, which[p % kPerBand]);
    else
        snprintf(out, size_t(n), "%s", kRestKeys[p - P_EQ_END]);
}

static uint32_t Hash(const char* s) // FNV-1a: 4 bytes per name instead of the whole name
{
    uint32_t h = 2166136261u;
    for(; *s; s++)
        h = (h ^ uint8_t(*s)) * 16777619u;
    return h;
}
static uint32_t ParamHash(int p)
{
    static uint32_t cache[P_COUNT] = {};
    if(cache[p] == 0)
    {
        char k[32];
        ParamKey(p, k, sizeof(k));
        cache[p] = Hash(k);
    }
    return cache[p];
}
static uint8_t ParamVersion(int p)
{
    char k[32];
    ParamKey(p, k, sizeof(k));
    for(const KeyVersion& v : kParamVersions)
        if(strcmp(v.key, k) == 0)
            return v.version;
    return 1;
}
static int FindParam(uint32_t h)
{
    for(int p = 0; p < P_COUNT; p++)
        if(ParamHash(p) == h)
            return p;
    return -1; // a setting this build doesn't have (any more): skipped
}
static int FindTab(uint32_t h)
{
    for(int t = 0; t < kTabs; t++)
        if(Hash(kTabKeys[t]) == h)
            return t;
    return -1;
}

// an older save's value for a setting whose meaning has changed since -> today's meaning
static int Migrate(int p, uint8_t saved_version, int v)
{
    char k[32];
    ParamKey(p, k, sizeof(k));
    if(strncmp(k, "clar.", 5) == 0 && strcmp(k, "clar.mode") != 0 && saved_version < 2)
    {
        // clarity knobs were 0-100 %, now tenths of a dB of lift: the same share of each knob's range
        static const struct
        {
            const char* key;
            int         max;
        } m[] = {{"clar.thump", 60}, {"clar.detail", 60}, {"clar.clarity", 60}, {"clar.warmth", 40}};
        for(const auto& e : m)
            if(strcmp(k, e.key) == 0)
                return v * e.max / 100;
    }
    return v;
}

// ---------------------------------------------------------------- configs
// these belong to the pedal, not to a sound, so loading a config leaves them alone
static bool PrefParam(int p)
{
    return p == P_CF_SLOT || p == P_CF_SAVE || p == P_CF_THEME || p == P_CF_KNOBS || p == P_H_ROW || p == P_TOUR_DONE || p == P_DPAD || p == P_SCR_FAST;
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

// ---------------------------------------------------------------- bytes
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
        if(k < 0 || n + k > max)
        {
            ok = false;
            memset(d, 0, size_t(k > 0 ? k : 0));
            return;
        }
        memcpy(d, p + n, size_t(k)), n += k;
    }
    void Skip(int k)
    {
        if(k < 0 || n + k > max)
            ok = false;
        else
            n += k;
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

// format 2:
//   u32 magic, u16 format, u16 settings, u16 tabs, u16 configs
//   settings x (u32 name hash, u8 version)   tabs x (u32 name hash)
//   snapshot (current), then per config: u8 used + snapshot      snapshot = settings x i16, tabs x u8, u8 on
//   learned profile: u8 valid, u8 tones, u8 tone slots, u8 bands, slots x f32 hz, slots x f32 db, bands x f32 floor
//   u32 crc of everything before it
int Core::SaveState(uint8_t* out, int max) const
{
    Writer w{out, 0, max};
    w.Put<uint32_t>(kMagic);
    w.Put<uint16_t>(kFormat);
    w.Put<uint16_t>(uint16_t(P_COUNT));
    w.Put<uint16_t>(uint16_t(kTabs));
    w.Put<uint16_t>(uint16_t(kSlots));
    for(int p = 0; p < P_COUNT; p++)
        w.Put<uint32_t>(ParamHash(p)), w.Put<uint8_t>(ParamVersion(p));
    for(int t = 0; t < kTabs; t++)
        w.Put<uint32_t>(Hash(kTabKeys[t]));
    auto snap = [&](const Snap& s) {
        for(int p = 0; p < P_COUNT; p++)
            w.Put<int16_t>(s.v[p]);
        w.Bytes(s.st, kTabs);
        w.Put<uint8_t>(s.on);
    };
    Snap cur;
    TakeSnap(cur);
    snap(cur);
    for(int k = 0; k < kSlots; k++)
        w.Put<uint8_t>(slots_[k].used), snap(slots_[k]);
    const NoiseProfile& pr = hum_.prof;
    w.Put<uint8_t>(pr.valid ? 1 : 0);
    w.Put<uint8_t>(uint8_t(pr.n_tones));
    w.Put<uint8_t>(uint8_t(NoiseProfile::kTones));
    w.Put<uint8_t>(uint8_t(NoiseProfile::kBands));
    w.Bytes(pr.tone_hz, int(sizeof(pr.tone_hz)));
    w.Bytes(pr.tone_db, int(sizeof(pr.tone_db)));
    w.Bytes(pr.floor_db, int(sizeof(pr.floor_db)));
    const uint32_t crc = Crc32(out, w.n);
    w.Put<uint32_t>(crc);
    return w.ok ? w.n : 0;
}

// the first builds saved settings by position; this is the order they had (frozen: never edit)
static int V1Param(int i) { return i < P_COUNT ? i : -1; } // (format 1 was only ever written in today's P_ order)
static int V1Tab(int t) { return t < kTabs ? t : -1; }

bool Core::LoadState(const uint8_t* in, int n)
{
    if(n < 16)
        return false;
    Reader r{in, 0, n};
    const uint32_t magic = r.Get<uint32_t>();
    const uint16_t fmt   = r.Get<uint16_t>();
    if(magic != kMagic || (fmt != 1 && fmt != 2))
        return false; // not ours, or from a newer format than this build knows: keep the defaults
    const int pc = r.Get<uint16_t>(), tc = r.Get<uint16_t>(), sc = r.Get<uint16_t>();
    if(pc <= 0 || pc > 1024 || tc <= 0 || tc > 64 || sc <= 0 || sc > 64)
        return false;

    // where each saved setting / tab goes in this build (-1 = this build doesn't have it)
    int16_t pmap[1024], tmap[64];
    uint8_t pver[1024];
    if(fmt == 1)
    {
        for(int i = 0; i < pc; i++) // (format-1 saves come from builds where clarity was already in dB)
            pmap[i] = int16_t(V1Param(i)), pver[i] = pmap[i] >= 0 ? ParamVersion(pmap[i]) : 1;
        for(int t = 0; t < tc; t++)
            tmap[t] = int16_t(V1Tab(t));
    }
    else
    {
        for(int i = 0; i < pc; i++)
        {
            pmap[i] = int16_t(FindParam(r.Get<uint32_t>()));
            pver[i] = r.Get<uint8_t>();
        }
        for(int t = 0; t < tc; t++)
            tmap[t] = int16_t(FindTab(r.Get<uint32_t>()));
    }
    // the checksum covers everything up to it: find where it sits, check it before using anything
    const int snap_bytes = pc * 2 + tc + 1;
    const int prof_head  = fmt == 1 ? 2 : 4;
    int       end        = r.n + snap_bytes + sc * (1 + snap_bytes) + prof_head;
    int       tones = NoiseProfile::kTones, bands = NoiseProfile::kBands;
    if(end > n)
        return false;
    if(fmt == 2)
        tones = in[end - 2], bands = in[end - 1];
    end += (tones * 2 + bands) * 4;
    if(end + 4 > n || Crc32(in, end) != uint32_t(in[end] | in[end + 1] << 8 | in[end + 2] << 16 | uint32_t(in[end + 3]) << 24))
        return false; // damaged or cut short: keep the defaults

    auto snap = [&](Snap& s) {
        TakeSnap(s); // anything the save doesn't have keeps its current value
        for(int i = 0; i < pc; i++)
        {
            const int16_t v = r.Get<int16_t>();
            if(pmap[i] >= 0)
                s.v[pmap[i]] = int16_t(Migrate(pmap[i], pver[i], v));
        }
        for(int t = 0; t < tc; t++)
        {
            const uint8_t v = r.Get<uint8_t>();
            if(tmap[t] >= 0)
                s.st[tmap[t]] = v;
        }
        s.on = r.Get<uint8_t>();
    };
    Snap cur;
    snap(cur);
    Snap loaded[kSlots];
    bool have[kSlots] = {};
    for(int k = 0; k < sc; k++)
    {
        Snap          s{};
        const uint8_t used = r.Get<uint8_t>();
        snap(s);
        if(k < kSlots)
            loaded[k] = s, loaded[k].used = used, have[k] = true;
    }
    NoiseProfile pr;
    pr.valid   = r.Get<uint8_t>() != 0;
    pr.n_tones = r.Get<uint8_t>();
    if(fmt == 2)
        r.Skip(2); // (the slot counts, read above)
    for(int i = 0; i < tones; i++)
    {
        const float v = r.Get<float>();
        if(i < NoiseProfile::kTones)
            pr.tone_hz[i] = v;
    }
    for(int i = 0; i < tones; i++)
    {
        const float v = r.Get<float>();
        if(i < NoiseProfile::kTones)
            pr.tone_db[i] = v;
    }
    for(int i = 0; i < bands; i++)
    {
        const float v = r.Get<float>();
        if(i < NoiseProfile::kBands)
            pr.floor_db[i] = v;
    }
    if(!r.ok)
        return false;
    if(bands != NoiseProfile::kBands) // the hiss bands changed since: the learned floor no longer fits, relearn
        pr.valid = false;
    pr.n_tones = pr.n_tones > NoiseProfile::kTones ? NoiseProfile::kTones : pr.n_tones;

    for(int k = 0; k < kSlots; k++)
        if(have[k])
            slots_[k] = loaded[k];
    // the current settings, including the pedal's own preferences (theme, knob direction)
    for(int p = 0; p < P_COUNT; p++)
        SetParam(p, cur.v[p]);
    for(int t = 0; t < kTabs; t++)
        SetStageOn(t, cur.st[t] != 0);
    effect_on_ = cur.on != 0;
    if(pr.valid)
        SetProfile(pr);
    state_dirty_ = fmt != kFormat; // an older format gets rewritten in today's
    redraw_all_  = true;
    return true;
}
} // namespace pg
