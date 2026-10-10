#include "BoardSim.h"
#include "BoardData.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <complex>
#include <functional>
#include <cstdlib>
#include <map>
#include <set>

namespace pgsim
{
// ======================================================================= the default (correct) wiring: WIRING.md
std::vector<Wire> DefaultWiring()
{
    std::vector<Wire> w = {
        {"DC.+", "J10.1"},     {"DC.-", "J10.2"},      // the panel 9 V jack (centre negative) -> dc + / dc -
        {"J10.3", "SEED.39"},  {"J10.4", "SEED.40"},   // vin, gnd
        {"J10.5", "SEED.12"},  {"J10.6", "SEED.13"},   // scl, sda
        {"J10.7", "SEED.35"},  {"J10.8", "SEED.34"},   // sck, fs
        {"J10.9", "SEED.33"},  {"J10.10", "SEED.32"},  // tx (Seed3 out -> codec), rx (codec -> Seed3 in)
        // (J22, the fx loop, stays open: the analog leveller's followers drive its returns)
    };
    // the pots: pot pin 3 (clockwise end) on header pin 1, 2 on 2, 1 on 3; left gang then right gang
    for(const char* pot : {"PGLINE", "PGHP"})
    {
        const char* hdr = std::strcmp(pot, "PGLINE") == 0 ? "J20" : "J19";
        for(int g = 0; g < 2; g++)
            for(int p = 1; p <= 3; p++)
                w.push_back({std::string(pot) + "." + (g ? "R" : "L") + std::to_string(p),
                             std::string(hdr) + "." + std::to_string(g * 3 + (4 - p))});
    }
    return w;
}

// parts on the board that are NOT fitted (found by the simulation: C31 / C41, 100 pF on the 500 k bias divider, made a
// 3.2 kHz low-pass on the input). Must match make_jlc.py / make_bom.py DNP.
static const char* const kNotFitted[] = {"C31", "C41", "R31", "R41"};

// ======================================================================= the circuit
enum Type
{
    RES, CAP, DIODE, OPAMP, TPA, VSS, REG, DCDC, VSRC, SINK, ADC, DAC, LDO, REFV, JACKIN, SEEDLOAD,
    NPN,    // the leveller's LED driver: B n0, E n1, C n2
    LDR,    // a light-dependent resistor n0-n1, lit by the LED n2 (anode) - n3 (cathode)
    LEVDAC  // the leveller's MCP4725: out n0, VDD n1, GND n2; the level the firmware set (setLeveller)
};
// a home-made vactrol's LDR against the current in its LED: ~2k at 1 mA, R ~ I^-0.75, megohms dark
static double LdrOhms(double ledAmps)
{
    const double ma = ledAmps * 1e3;
    return ma < 1e-4 ? 1e6 : std::min(1e6, std::max(60.0, 2000.0 * std::pow(ma, -0.75)));
}

struct Elem
{
    Type        t;
    int         n[6] = {0, 0, 0, 0, 0, 0};
    double      p[4] = {0, 0, 0, 0};
    int         st = 0, stF = 0; // piecewise-linear state (slow / fast)
    int         knob = -1;       // pots: 0 = pg-line, 1 = pg-hp; p[1] = which end (0: pins 1-2, 1: pins 2-3)
    int         ch = 0;
    std::string ref;
    double      vS = 0, vF = 0, iF = 0; // capacitor history: slow (backward Euler), fast (trapezoid)
    double      i = 0, w = 0;            // last slow current / heat
    double      heat = 0;                // PTC: running average current
    double      dv0 = 0, vt0 = 0, vn0 = 0;   // op-amps: their resting input difference / output target (AC analysis)
};

static double ParseValue(const std::string& v, char unit)
{
    // "4.7k", "1M", "220", "100nF", "4.7uF", "10uF 25V", "100pF C0G" -> SI
    char*  e = nullptr;
    double x = std::strtod(v.c_str(), &e);
    if(e == v.c_str())
        return 0;
    double m = 1;
    switch(*e)
    {
        case 'p': m = 1e-12; break;
        case 'n': m = 1e-9; break;
        case 'u': m = 1e-6; break;
        case 'm': m = 1e-3; break;
        case 'k': m = 1e3; break;
        case 'M': m = unit == 'F' ? 1e-3 : 1e6; break;
        default: break;
    }
    return x * m;
}

// a dense LU solver for the small matrices here (partial pivoting)
struct Lu
{
    int                 n = 0;
    std::vector<double> a;
    std::vector<int>    piv;
    bool factor(const std::vector<double>& m, int size)
    {
        n = size, a = m, piv.resize(size);
        for(int k = 0; k < n; k++)
        {
            int    p  = k;
            double mx = std::fabs(a[k * n + k]);
            for(int r = k + 1; r < n; r++)
                if(std::fabs(a[r * n + k]) > mx)
                    mx = std::fabs(a[r * n + k]), p = r;
            if(mx < 1e-30)
                return false;
            piv[k] = p;
            if(p != k)
                for(int c = 0; c < n; c++)
                    std::swap(a[k * n + c], a[p * n + c]);
            const double inv = 1.0 / a[k * n + k];
            for(int r = k + 1; r < n; r++)
            {
                const double f = (a[r * n + k] *= inv);
                if(f != 0)
                    for(int c = k + 1; c < n; c++)
                        a[r * n + c] -= f * a[k * n + c];
            }
        }
        return true;
    }
    void solve(std::vector<double>& b) const
    {
        for(int k = 0; k < n; k++)
        {
            if(piv[k] != k)
                std::swap(b[k], b[piv[k]]);
            for(int r = k + 1; r < n; r++)
                b[r] -= a[r * n + k] * b[k];
        }
        for(int k = n - 1; k >= 0; k--)
        {
            double s = b[k];
            for(int c = k + 1; c < n; c++)
                s -= a[k * n + c] * b[c];
            b[k] = s / a[k * n + k];
        }
    }
};

// One solve context: which nodes are unknown (col >= 0) and which are held (their voltage in `held`).
struct Stamp
{
    int                        n = 0;
    std::vector<double>        Y, J;
    const std::vector<int>*    col  = nullptr; // node -> unknown index, -1 = held
    const std::vector<double>* held = nullptr; // node voltages for held nodes
    void reset(int size)
    {
        n = size;
        Y.assign((size_t) n * n, 0.0);
        J.assign((size_t) n, 0.0);
    }
    double hv(int node) const { return (*held)[(size_t) node]; }
    // a current i = sum(k_j * V(node_j)) + c flows out of `from` and into `to`
    void inj(int from, int to, std::initializer_list<std::pair<int, double>> k, double c)
    {
        double cc = c;
        for(auto& kv : k)
            if((*col)[(size_t) kv.first] < 0)
                cc += kv.second * hv(kv.first);
        auto row = [&](int node, double sign) {
            const int r = (*col)[(size_t) node];
            if(r < 0)
                return;
            for(auto& kv : k)
            {
                const int cI = (*col)[(size_t) kv.first];
                if(cI >= 0)
                    Y[(size_t) r * n + cI] += sign * kv.second;
            }
            J[(size_t) r] -= sign * cc;
        };
        row(from, +1.0);
        row(to, -1.0);
    }
    void g(int a, int b, double gg) { inj(a, b, {{a, gg}, {b, -gg}}, 0.0); }
};

struct Island
{
    std::vector<int>    nodes;   // its unknowns
    std::vector<int>    elems;   // the parts touching it
    std::vector<int>    col;     // node -> unknown index (size = node count)
    std::vector<double> v;       // node voltages (all nodes: held ones copied from the slow solve)
    Lu                  lu;
    std::vector<int>    sig;     // the states the LU was built for
    int                 knobVer = -1;
    bool                hasJack = false, hasDac = false;
    std::vector<int>    sigTmp;   // (preallocated: no allocation on the audio thread)
    std::vector<double> nvTmp;
    Stamp               st;
    int                 adcNode[2] = {-1, -1}, jackNode[2] = {-1, -1}, outNode[2] = {-1, -1};
};

struct BoardSim::Impl
{
    // ---- topology
    std::map<std::string, int> point;   // endpoint name -> point id
    std::vector<int>           parent;  // union-find over points
    std::vector<int>           nodeOf;  // point -> node
    int                        nodes = 1;
    std::vector<Elem>          el;
    std::vector<int>           netNode; // board net -> node
    int                        gnd = 0, ignd = -1;
    std::vector<bool>          slowNode;
    std::vector<Island>        isl;
    std::vector<Wire>          wires;

    // ---- state
    std::vector<double> V;               // slow node voltages
    double              t = 0;
    bool                power = true;
    float               knob[2] = {0.5f, 0.5f};
    int                 knobVer = 0;
    float               phones = 32.f;
    bool                adcOk = false, dacOk = false, codecPowered = false, codecConf = false, seedOn = false;
    bool                isolatedOk = true, ampOn = false, i2sIn = false, i2sOut = false;
    std::vector<Fault>  faults;
    std::vector<int>    domainIso;       // node -> 1 if on the isolated side (conductive graph without wires... see below)
    mutable std::mutex  mx;              // topology + slow results (the audio thread only try-locks)
    std::vector<double> railsForAudio;   // the held voltages the fast solver uses (copied under the lock)

    int  pt(const std::string& name)
    {
        auto it = point.find(name);
        if(it != point.end())
            return it->second;
        const int id = (int) parent.size();
        point[name]  = id;
        parent.push_back(id);
        return id;
    }
    int find(int a) { return parent[(size_t) a] == a ? a : parent[(size_t) a] = find(parent[(size_t) a]); }
    void join(int a, int b) { parent[(size_t) find(a)] = find(b); }

    int padNet(const std::string& ref, const std::string& num) const
    {
        for(int i = 0; i < kPads; i++)
            if(ref == kBPads[i].ref && num == kBPads[i].num)
                return kBPads[i].net;
        return -2; // no such pad
    }
    // an endpoint: a board pad ("J10.3") becomes its net; anything else is its own point
    int endpoint(const std::string& e)
    {
        const auto dot = e.find('.');
        if(dot != std::string::npos)
        {
            const int net = padNet(e.substr(0, dot), e.substr(dot + 1));
            if(net >= 0)
                return pt(std::string("N:") + kNetNames[net]);
        }
        return pt("X:" + e);
    }
    int node(const std::string& e) { return nodeOf[(size_t) find(endpoint(e))]; }
    int netN(const char* name) const
    {
        for(int i = 0; i < kNets; i++)
            if(std::strcmp(kNetNames[i], name) == 0)
                return netNode[(size_t) i];
        return -1;
    }
    int pin(const char* ref, const char* num) const
    {
        const int net = padNet(ref, num);
        return net >= 0 ? netNode[(size_t) net] : -1;
    }

    void add(Type t, std::initializer_list<int> n, std::initializer_list<double> p, const std::string& ref, int ch = 0)
    {
        Elem e;
        e.t = t, e.ref = ref, e.ch = ch;
        int k = 0;
        for(int x : n)
            e.n[k++] = x < 0 ? 0 : x;
        k = 0;
        for(double x : p)
            e.p[k++] = x;
        el.push_back(e);
    }

    void build(const std::vector<Wire>& w);
    void partition();
    bool solveSlow(double dt);
    bool solveStep(double h);
    void currents(const Elem& e, const double* v, double* i, double h) const;
    std::vector<double> Vstart, Ven; // step-start voltages; what the chips' enables see
    bool                relaxEn = false;
    double              gPseudo = 0;
    double              testIn[2] = {0, 0}, testDac[2] = {0, 0}; // test signals for the whole-board solver
    double              levelV = 0.0;                            // the leveller DAC's output (volts)
    std::vector<double> nodeScale;                               // resting-state search: per-node pretend capacitor scale
    bool                noCaps = false;
    double              opGain = 200.0; // op-amp gain in the whole-board solver (the AC analysis uses the real 1e5)
    bool                acMode = false;
    std::vector<std::complex<double>> ac(int src, double hz) const;
    void residual(const std::vector<double>& v, double h, std::vector<double>& F, std::vector<double>* Jm) const;
    double hLast = 1e-4, rampT = 0;
    bool   needOp = true;
    void   operatingPoint();
    void afterSlow(double dt);
    void checks();
    double vd(int a, int b) const { return V[(size_t) a] - V[(size_t) b]; }
    void stampElem(Stamp& s, Elem& e, bool fast, double dt, const double* src, const std::vector<double>& v);
    int  updateState(Elem& e, bool fast, const std::vector<double>& v, const double* src);
};

// ----------------------------------------------------------------------- building the circuit from the board
void BoardSim::Impl::build(const std::vector<Wire>& w)
{
    wires = w;
    point.clear(), parent.clear(), el.clear();
    for(int i = 0; i < kNets; i++)
        pt(std::string("N:") + kNetNames[i]);
    for(const auto& x : w)
        join(endpoint(x.a), endpoint(x.b));
    // the external parts' own points (so a pot pin or a Seed3 pin nobody wired still exists, floating)
    for(const char* e : {"X:SEED.39", "X:SEED.40", "X:SEED.38", "X:SEED.12", "X:SEED.13", "X:SEED.32", "X:SEED.33",
                         "X:SEED.34", "X:SEED.35", "X:DC.+", "X:DC.-"})
        pt(e);
    for(const char* pot : {"PGLINE", "PGHP"})
        for(const char* g : {"L", "R"})
            for(int p = 1; p <= 3; p++)
                pt(std::string("X:") + pot + "." + g + std::to_string(p));
    // nodes: one per union-find root, GND first
    nodeOf.assign(parent.size(), -1);
    const int groot = find(point["N:GND"]);
    nodeOf[(size_t) groot] = 0;
    nodes                   = 1;
    for(size_t i = 0; i < parent.size(); i++)
    {
        const int r = find((int) i);
        if(nodeOf[(size_t) r] < 0)
            nodeOf[(size_t) r] = nodes++;
        nodeOf[i] = nodeOf[(size_t) r];
    }
    netNode.assign(kNets, 0);
    for(int i = 0; i < kNets; i++)
        netNode[(size_t) i] = nodeOf[(size_t) find(point[std::string("N:") + kNetNames[i]])];
    gnd  = 0;
    ignd = netN("IGND");

    // ---- the board's parts, by their value (the routed board's own nets on each pad)
    for(int k = 0; k < kParts; k++)
    {
        const BPart&      bp  = kBParts[k];
        const std::string ref = bp.ref, val = bp.value;
        if(std::find(std::begin(kNotFitted), std::end(kNotFitted), ref) != std::end(kNotFitted))
            continue; // its pads stay empty
        auto              P   = [&](const char* num) { return pin(bp.ref, num); };
        if(ref[0] == 'R' && ref != "R")
            add(RES, {P("1"), P("2")}, {std::max(0.05, ParseValue(val, 'R'))}, ref);
        else if(ref[0] == 'C')
            add(CAP, {P("1"), P("2")}, {ParseValue(val, 'F')}, ref);
        else if(val == "PTC 300mA")
            add(RES, {P("1"), P("2")}, {1.7, 1.0 /* = PTC */}, ref);
        else if(val == "BAT54S" || val == "BAS40W-04") // Schottky pair: 1 -> 3 and 3 -> 2 (pin 3 = the middle)
        {
            add(DIODE, {P("1"), P("3")}, {0.22, 25.0}, ref);
            add(DIODE, {P("3"), P("2")}, {0.22, 25.0}, ref);
        }
        else if(val == "1N4148WS") // pin 2 anode, pin 1 cathode
            add(DIODE, {P("2"), P("1")}, {0.6, 5.0}, ref);
        else if(val == "B5819W") // pin 2 anode, pin 1 cathode
            add(DIODE, {P("2"), P("1")}, {0.32, 0.15}, ref);
        else if(val == "PSM712") // lines 1 and 2 to the common pin 3: +12 V stand-off one way, 7 V the other
            for(const char* l : {"1", "2"})
            {
                add(DIODE, {P(l), P("3")}, {13.3, 1.0}, ref);
                add(DIODE, {P("3"), P(l)}, {7.5, 1.0}, ref);
            }
        else if(val == "TLV9062") // A: 3 +, 2 -, 1 out; B: 5 +, 6 -, 7 out; 8 V+, 4 V-
        {
            add(OPAMP, {P("3"), P("2"), P("1"), P("8"), P("4")}, {1e5, 20.0}, ref, 0);
            add(OPAMP, {P("5"), P("6"), P("7"), P("8"), P("4")}, {1e5, 20.0}, ref, 1);
            for(const char* in : {"3", "2", "5", "6"}) // its inputs' protection diodes to both supplies
            {
                add(DIODE, {P(in), P("8")}, {0.6, 100.0}, ref);
                add(DIODE, {P("4"), P(in)}, {0.6, 100.0}, ref);
            }
        }
        else if(val == "TPA6139A2") // 1 -IN L, 2 OUT L, 14 -IN R, 13 OUT R, 10 VDD, 3/11 GND, 4 HP_ON, 5 VSS
        {
            add(TPA, {P("1"), P("2"), P("10"), P("3"), P("4")}, {15e3, 0.3}, ref, 0);
            add(TPA, {P("14"), P("13"), P("10"), P("3"), P("4")}, {15e3, 0.3}, ref, 1);
            add(VSS, {P("5"), P("10"), P("3")}, {2.0}, ref);
        }
        else if(val == "XC6206P332MR") // 3 IN, 2 OUT, 1 GND
            add(REG, {P("3"), P("2"), P("1")}, {3.3, 0.25, 0.25, 1e-6}, ref);
        else if(val == "AMS1117-5.0") // 3 IN, 2 OUT, 1 GND
            add(REG, {P("3"), P("2"), P("1")}, {5.0, 1.1, 1.0, 5e-3}, ref);
        else if(val.rfind("B0505S", 0) == 0) // 2 +Vin, 1 GND in; 4 +Vo, 3 0 V out: isolated
            add(DCDC, {P("2"), P("1"), P("4"), P("3")}, {1.03, 1.5, 0.75, 0.03}, ref);
        else if(val == "ISO7741")
        {
            add(SINK, {P("1"), P("2")}, {2.5e-3, 2.25}, ref, 0);
            add(SINK, {P("16"), P("15")}, {2.5e-3, 2.25}, ref, 1);
        }
        else if(val == "ISO1540")
        {
            add(SINK, {P("1"), P("4")}, {3e-3, 2.25}, ref, 0);
            add(SINK, {P("8"), P("5")}, {3e-3, 2.25}, ref, 1);
        }
        else if(val == "TLV320AIC3204")
        {
            add(SINK, {P("26"), P("28")}, {14e-3, 2.7}, ref);                  // LDOIN / IOVDD
            add(LDO, {P("24"), P("26"), P("28"), P("30")}, {1.8, 2.0}, ref, 0); // AVDD from its own LDO
            add(LDO, {P("29"), P("26"), P("28"), P("30")}, {1.8, 2.0}, ref, 1); // DVDD
            add(REFV, {P("18"), P("26"), P("28")}, {0.9, 1e3}, ref);
            add(ADC, {P("15"), P("26"), P("28")}, {20e3, 0.9}, ref, 0);
            add(ADC, {P("16"), P("26"), P("28")}, {20e3, 0.9}, ref, 1);
            add(DAC, {P("22"), P("26"), P("28")}, {1.65, 1.0}, ref, 0);
            add(DAC, {P("23"), P("26"), P("28")}, {1.65, 1.0}, ref, 1);
        }
        else if(val == "LED+LDR") // OC1 / OC2: 1 LED +, 2 LED -, 3 / 4 the LDR
        {
            add(DIODE, {P("1"), P("2")}, {1.80, 15.0}, ref); // a red LED
            add(RES, {P("1"), P("2")}, {1e6, 0.0}, ref);      // (its leakage: keeps the nodes between dark LEDs defined)
            add(LDR, {P("3"), P("4"), P("1"), P("2")}, {0.0}, ref);
        }
        else if(val == "MMBT3904") // 1 B, 2 E, 3 C
            add(NPN, {P("1"), P("2"), P("3")}, {150.0}, ref);
        else if(val == "MCP4725") // 1 VOUT, 2 VSS, 3 VDD
        {
            add(LEVDAC, {P("1"), P("3"), P("2")}, {}, ref);
            add(SINK, {P("3"), P("2")}, {2e-4, 2.25}, ref, 1);
        }
        else if(val == "in") // J1: the input jack: what's plugged in drives tip / ring against the sleeve
        {
            add(JACKIN, {P("T"), P("S")}, {100.0}, ref, 0);
            add(JACKIN, {P("R"), P("S")}, {100.0}, ref, 1);
        }
        else if(val == "out") // J2: headphones on tip / ring
        {
            add(RES, {P("T"), P("S")}, {32.0, 2.0 /* = headphone load */}, ref, 0);
            add(RES, {P("R"), P("S")}, {32.0, 2.0}, ref, 1);
        }
    }
    // ---- outside the board: the 9 V adapter, the Seed3, the two pots
    add(VSRC, {node("DC.+"), node("DC.-")}, {9.0, 0.5, 1.0 /* current limit */}, "9V adapter");
    if(ignd > 0) // the converter's leakage across the barrier (~10 MOhm): gives the isolated side a voltage
        add(RES, {ignd, 0}, {10e6, 3.0 /* = leakage, not a part */}, "barrier");
    add(SEEDLOAD, {node("SEED.39"), node("SEED.40"), node("SEED.38")}, {0.12, 4.0}, "SEED3");
    for(const char* io : {"SEED.12", "SEED.13", "SEED.32", "SEED.33", "SEED.34", "SEED.35"})
    {
        add(DIODE, {node(io), node("SEED.38")}, {0.6, 5.0}, "SEED3");  // its pins' protection diodes
        add(DIODE, {node("SEED.40"), node(io)}, {0.6, 5.0}, "SEED3");
    }
    for(int k = 0; k < 2; k++)
        for(const char* g : {"L", "R"})
        {
            const std::string pot = k ? "PGHP" : "PGLINE";
            Elem              a, b;
            add(RES, {node(pot + "." + g + "1"), node(pot + "." + g + "2")}, {5e3, 0.0}, pot);
            el.back().knob = k, el.back().p[1] = 0;
            add(RES, {node(pot + "." + g + "2"), node(pot + "." + g + "3")}, {5e3, 0.0}, pot);
            el.back().knob = k, el.back().p[1] = 1;
        }
    V.assign((size_t) nodes, 0.0);
    railsForAudio = V;
    t             = 0;
    needOp        = true;
    partition();
}

// which nodes the audio solver holds still (rails, grounds, bias, digital) and which it solves every sample
void BoardSim::Impl::partition()
{
    slowNode.assign((size_t) nodes, false);
    slowNode[0] = true;
    if(ignd >= 0)
        slowNode[(size_t) ignd] = true;
    for(const auto& e : el)
        switch(e.t)
        {
            case OPAMP: slowNode[(size_t) e.n[3]] = slowNode[(size_t) e.n[4]] = true; break;
            case TPA: slowNode[(size_t) e.n[2]] = slowNode[(size_t) e.n[3]] = slowNode[(size_t) e.n[4]] = true; break;
            case VSS: case REG: case DCDC: case VSRC: case SINK: case LDO: case REFV: case SEEDLOAD:
                for(int k = 0; k < 6; k++)
                    slowNode[(size_t) e.n[k]] = true;
                break;
            case ADC: case DAC: slowNode[(size_t) e.n[1]] = slowNode[(size_t) e.n[2]] = true; break;
            case NPN: case LEVDAC:
                for(int k = 0; k < 3; k++)
                    slowNode[(size_t) e.n[k]] = true;
                break;
            case LDR: slowNode[(size_t) e.n[2]] = slowNode[(size_t) e.n[3]] = true; break;
            case JACKIN: slowNode[(size_t) e.n[1]] = true; break;
            default: break;
        }
    for(bool more = true; more;) // a big capacitor to a held node holds its other end too (bias points, references)
    {
        more = false;
        for(const auto& e : el)
            if(e.t == CAP && e.p[0] >= 4.7e-6)
                for(int q = 0; q < 2; q++)
                    if(slowNode[(size_t) e.n[q]] && !slowNode[(size_t) e.n[1 - q]])
                        slowNode[(size_t) e.n[1 - q]] = true, more = true;
    }
    // the islands: what the solved nodes split into
    isl.clear();
    std::vector<int> seen((size_t) nodes, -1);
    for(int s = 1; s < nodes; s++)
    {
        if(slowNode[(size_t) s] || seen[(size_t) s] >= 0)
            continue;
        Island I;
        std::vector<int> st = {s};
        seen[(size_t) s]    = (int) isl.size();
        while(!st.empty())
        {
            const int x = st.back();
            st.pop_back();
            I.nodes.push_back(x);
            for(const auto& e : el)
            {
                int  cnt = e.t == OPAMP || e.t == TPA ? 3 : (e.t == DCDC || e.t == SEEDLOAD ? 0 : 2);
                bool touches = false;
                for(int k = 0; k < cnt; k++)
                    touches |= e.n[k] == x;
                if(!touches)
                    continue;
                for(int k = 0; k < cnt; k++)
                {
                    const int y = e.n[k];
                    if(!slowNode[(size_t) y] && seen[(size_t) y] < 0)
                        seen[(size_t) y] = (int) isl.size(), st.push_back(y);
                }
            }
        }
        isl.push_back(I);
    }
    for(size_t i = 0; i < isl.size(); i++)
    {
        auto& I = isl[i];
        I.col.assign((size_t) nodes, -1);
        for(size_t k = 0; k < I.nodes.size(); k++)
            I.col[(size_t) I.nodes[k]] = (int) k;
        for(size_t k = 0; k < el.size(); k++)
        {
            const auto& e = el[k];
            bool        in = false;
            for(int q = 0; q < 6; q++)
                in |= I.col[(size_t) e.n[q]] >= 0;
            if(in)
                I.elems.push_back((int) k);
            if(in && e.t == JACKIN)
                I.hasJack = true, I.jackNode[e.ch] = e.n[0];
            if(in && e.t == ADC)
                I.adcNode[e.ch] = e.n[0];
            if(in && e.t == DAC)
                I.hasDac = true;
            if(in && e.t == RES && e.p[1] == 2.0)
                I.outNode[e.ch] = e.n[0];
        }
        I.v.assign((size_t) nodes, 0.0);
        I.nvTmp.assign((size_t) nodes, 0.0);
        I.sigTmp.reserve(I.elems.size() + 1);
        I.st.reset((int) I.nodes.size());
    }
}

// ----------------------------------------------------------------------- the part models
// src: the audio values for this sample (fast) or nullptr (slow: inputs silent, DAC at rest)
void BoardSim::Impl::stampElem(Stamp& s, Elem& e, bool fast, double dt, const double* src, const std::vector<double>& v)
{
    const int st = fast ? e.stF : e.st;
    auto      VV = [&](int n) { return v[(size_t) n]; };
    switch(e.t)
    {
        case RES:
        {
            double r = e.p[0];
            if(e.knob >= 0) // a pot: wiper 0..1 left..right; clockwise moves the wiper towards pin 3
            {
                const double pos = knob[e.knob];
                r                = 50.0 + 10e3 * (e.p[1] == 0 ? pos : 1.0 - pos);
            }
            if(e.p[1] == 1.0 && e.heat > 0.6) // the PTC fuse, tripped
                r = 900.0;
            if(e.p[1] == 2.0) // headphones (0 = nothing plugged in)
                r = phones > 0 ? phones : 1e9;
            s.g(e.n[0], e.n[1], 1.0 / r);
            break;
        }
        case CAP:
        {
            const double C = e.p[0];
            if(fast)
            {
                const double G = 2.0 * C / dt;
                s.inj(e.n[0], e.n[1], {{e.n[0], G}, {e.n[1], -G}}, -(G * e.vF + e.iF));
            }
            else
            {
                const double G = C / dt;
                s.inj(e.n[0], e.n[1], {{e.n[0], G}, {e.n[1], -G}}, -G * e.vS);
            }
            break;
        }
        case DIODE: // anode n0, cathode n1: off, or on (a voltage drop p0 behind p1 ohms)
            if(st)
            {
                const double G = 1.0 / e.p[1];
                s.inj(e.n[0], e.n[1], {{e.n[0], G}, {e.n[1], -G}}, -G * e.p[0]);
            }
            else
                s.g(e.n[0], e.n[1], 1e-9);
            break;
        case OPAMP: // + n0, - n1, out n2, V+ n3, V- n4
        {
            const double G = 1.0 / e.p[1], A = e.p[0];
            if(st == 3)
                break; // unpowered: its output floats
            if(st == 0)
                s.inj(e.n[3], e.n[2], {{e.n[0], G * A}, {e.n[1], -G * A}, {e.n[4], G}, {e.n[2], -G}}, 0.0);
            else if(st == 1)
                s.inj(e.n[3], e.n[2], {{e.n[3], G}, {e.n[2], -G}}, -0.03 * G);
            else
                s.inj(e.n[3], e.n[2], {{e.n[4], G}, {e.n[2], -G}}, 0.03 * G);
            break;
        }
        case TPA: // -IN n0 (into a virtual ground through p0), OUT n1, VDD n2, GND n3, HP_ON n4
        {
            s.g(e.n[0], e.n[3], 1.0 / e.p[0]);
            const double G = 1.0 / e.p[1];
            if(st == 3)
                break;
            if(st == 0) // x-2, ground-centred
                s.inj(e.n[2], e.n[1], {{e.n[0], -2.0 * G}, {e.n[3], 3.0 * G}, {e.n[1], -G}}, 0.0);
            else if(st == 1)
                s.inj(e.n[2], e.n[1], {{e.n[2], G}, {e.n[1], -G}}, -0.3 * G);
            else if(st == 2)
                s.inj(e.n[2], e.n[1], {{e.n[2], -G}, {e.n[3], 2.0 * G}, {e.n[1], -G}}, 0.3 * G);
            else // muted: held at ground
                s.inj(e.n[2], e.n[1], {{e.n[3], G}, {e.n[1], -G}}, 0.0);
            break;
        }
        case VSS: // the charge pump's negative rail: -VDD (n1) under GND (n2) at n0
            if(st)
            {
                const double G = 1.0 / e.p[0];
                s.inj(e.n[2], e.n[0], {{e.n[1], -G}, {e.n[2], 2.0 * G}, {e.n[0], -G}}, 0.0);
            }
            else
                s.g(e.n[0], e.n[2], 1e-6);
            break;
        case REG: // in n0, out n1, gnd n2: p0 volts, p1 dropout, p2 current limit, p3 its own current
        {
            if(st == 0)
            {
                s.g(e.n[1], e.n[2], 1e-6);
                break;
            }
            const double G = 20.0;
            if(st == 1)
                s.inj(e.n[0], e.n[1], {{e.n[2], G}, {e.n[1], -G}}, G * e.p[0]);
            else if(st == 2)
                s.inj(e.n[0], e.n[1], {{e.n[0], G}, {e.n[1], -G}}, -G * e.p[1]);
            else // current-limited: still aims at its voltage, but through enough resistance to cap the current
            {
                const double Gl = e.p[2] / std::max(0.5, e.p[0]);
                s.inj(e.n[0], e.n[1], {{e.n[2], Gl}, {e.n[1], -Gl}}, Gl * e.p[0]);
            }
            s.g(e.n[0], e.n[2], e.p[3] / 5.0);
            break;
        }
        case DCDC: // in+ n0, in- n1, out+ n2, out- n3: unregulated 1:1 (p0), p1 ohms out; draws the power it gives / p2
        {
            if(st == 0)
            {
                s.g(e.n[2], e.n[3], 1e-6);
                break;
            }
            const double G = 1.0 / e.p[1];
            s.inj(e.n[3], e.n[2], {{e.n[0], G * e.p[0]}, {e.n[1], -G * e.p[0]}, {e.n[3], G}, {e.n[2], -G}}, 0.0);
            const double k = e.p[0] * G / e.p[2]; // input current = output current x (ratio / efficiency)
            s.inj(e.n[0], e.n[1], {{e.n[0], k * e.p[0]}, {e.n[1], -k * e.p[0]}, {e.n[3], k}, {e.n[2], -k}}, 0.0);
            s.g(e.n[0], e.n[1], e.p[3] / 5.0);   // its no-load draw
            break;
        }
        case VSRC: // the adapter: p0 volts behind p1 ohms, folds back to p2 amps
            if(!power)
                s.g(e.n[0], e.n[1], 1e-9);
            else if(st == 0)
            {
                const double G = 1.0 / e.p[1];
                s.inj(e.n[1], e.n[0], {{e.n[1], G}, {e.n[0], -G}}, G * e.p[0] * std::min(1.0, rampT / 2e-3));
            }
            else // overloaded: folds back to a soft source (can't push past its own voltage)
            {
                const double Gl = e.p[2] / e.p[0];
                s.inj(e.n[1], e.n[0], {{e.n[1], Gl}, {e.n[0], -Gl}}, Gl * e.p[0] * std::min(1.0, rampT / 2e-3));
            }
            break;
        case SINK: // an IC's supply: draws ~p0 amps at 3.3 V (as a resistance, so it can't push a rail anywhere odd)
            s.g(e.n[0], e.n[1], st ? e.p[0] / 3.3 : 1e-6);
            break;
        case LDO: // the codec's own LDO: out n0 from n1, gnd n2, enable n3 (LDO_SELECT)
            if(st)
            {
                const double G = 1.0 / e.p[1];
                s.inj(e.n[1], e.n[0], {{e.n[2], G}, {e.n[0], -G}}, G * e.p[0]);
            }
            else
                s.g(e.n[0], e.n[2], 1e-6);
            break;
        case REFV:
            if(st)
            {
                const double G = 1.0 / e.p[1];
                s.inj(e.n[1], e.n[0], {{e.n[2], G}, {e.n[0], -G}}, G * e.p[0]);
            }
            break;
        case ADC: // the codec's input: 20k to its common mode while it's powered
            if(st)
            {
                const double G = 1.0 / e.p[0];
                s.inj(e.n[0], e.n[2], {{e.n[0], G}, {e.n[2], -G}}, -G * e.p[1]);
            }
            else
                s.g(e.n[0], e.n[2], 1e-7);
            break;
        case DAC: // the codec's line out: its common mode + the sample, 1 ohm; floats when unpowered
            if(st)
            {
                const double G = 1.0 / e.p[1];
                const double x = (st == 2 && src) ? src[2 + e.ch] * 1.41 : 0.0;
                s.inj(e.n[1], e.n[0], {{e.n[2], G}, {e.n[0], -G}}, G * (e.p[0] + x));
            }
            break;
        case JACKIN: // what's plugged into the input: the audio (1.0 = 2 V peak) behind p0 ohms
        {
            const double G = 1.0 / e.p[0];
            const double x = src ? src[e.ch] * 2.0 : 0.0;
            s.inj(e.n[1], e.n[0], {{e.n[1], G}, {e.n[0], -G}}, G * x);
            break;
        }
        case LDR: s.g(e.n[0], e.n[1], 1.0 / LdrOhms(e.heat)); break; // (e.heat = its LED's current, slow solver)
        case NPN: case LEVDAC: break; // (their nodes are held by the slow solver)
        case SEEDLOAD: // VIN n0, GND n1, its 3V3 n2: ~120 mA once there's 4 V, and it makes its own 3.3 V
            if(st)
            {
                s.g(e.n[0], e.n[1], e.p[0] / 9.0);
                s.inj(e.n[1], e.n[2], {{e.n[1], 20.0}, {e.n[2], -20.0}}, 20.0 * 3.3);
            }
            else
                s.g(e.n[0], e.n[1], 1e-6), s.g(e.n[2], e.n[1], 1e-6);
            break;
    }
}

// the piecewise-linear parts pick their state from the latest voltages; returns 1 if it changed
int BoardSim::Impl::updateState(Elem& e, bool fast, const std::vector<double>& v, const double* src)
{
    int&  st = fast ? e.stF : e.st;
    auto  VV = [&](int n) { return v[(size_t) n]; };
    int   ns = st;
    switch(e.t)
    {
        case DIODE:
        {
            const double vd = VV(e.n[0]) - VV(e.n[1]);
            ns              = st ? (vd - e.p[0] > -1e-6 ? 1 : 0) : (vd > e.p[0] ? 1 : 0);
            break;
        }
        case OPAMP:
        {
            const double vp = VV(e.n[3]), vn = VV(e.n[4]), dv = VV(e.n[0]) - VV(e.n[1]);
            if(vp - vn < (st == 3 ? 1.8 : 1.5)) // unpowered (on above 1.8 V, off below 1.5 V)
                ns = 3;
            else if(st == 1) // pinned high: back to linear once its inputs ask for lower (never straight to low)
                ns = dv < 0 ? 0 : 1;
            else if(st == 2)
                ns = dv > 0 ? 0 : 2;
            else
            {
                const double vt = e.p[0] * dv + vn;
                ns              = vt > vp - 0.03 ? 1 : (vt < vn + 0.03 ? 2 : 0);
            }
            break;
        }
        case TPA:
        {
            const double vdd = VV(e.n[2]) - VV(e.n[3]), vin = VV(e.n[0]) - VV(e.n[3]);
            if(vdd < (st == 3 ? 2.9 : 2.6))
                ns = 3;
            else if(VV(e.n[4]) - VV(e.n[3]) < (st == 4 ? 1.3 : 1.1))
                ns = 4;
            else if(st == 1)
                ns = vin > 0 ? 0 : 1; // (inverting: a positive input pulls it down)
            else if(st == 2)
                ns = vin < 0 ? 0 : 2;
            else
            {
                const double vt = -2.0 * vin;
                ns              = vt > vdd - 0.3 ? 1 : (vt < -(vdd - 0.3) ? 2 : 0);
            }
            break;
        }
        case VSS: ns = VV(e.n[1]) - VV(e.n[2]) > (st ? 2.6 : 2.9) ? 1 : 0; break;
        case REG:
        {
            const double vin = VV(e.n[0]) - VV(e.n[2]), vout = VV(e.n[1]) - VV(e.n[2]);
            if(vin < (st == 0 ? 1.2 : 0.8)) // (on above 1.2 V, off below 0.8 V)
            {
                ns = 0;
                break;
            }
            const double tgt  = std::min(e.p[0], vin - e.p[1]);
            const double head = vin - e.p[1] - e.p[0]; // > 0: room to regulate, < 0: in dropout
            const int    norm = st == 2 ? (head > 0.02 ? 1 : 2) : (head < -0.02 ? 2 : 1); // (20 mV either way)
            if(st == 3) // limiting: back to normal once regulating would need less than the limit
                ns = 20.0 * (tgt - vout) < e.p[2] ? norm : 3;
            else if(st == 0)
                ns = head < 0 ? 2 : 1;
            else
            {
                const double i = 20.0 * ((st == 1 ? e.p[0] : vin - e.p[1]) - vout); // its current in this state
                ns             = i > e.p[2] ? 3 : norm;
            }
            if(std::getenv("SIMDEBUG4") && e.ref == "U7" && ns != st)
                std::fprintf(stderr, "U7 %d->%d vin %.3f vout %.3f head %.3f\n", st, ns, vin, vout, head);
            break;
        }
        case DCDC: ns = VV(e.n[0]) - VV(e.n[1]) > (st ? 2.2 : 3.0) ? 1 : 0; break;
        case VSRC:
        {
            const double v = VV(e.n[0]) - VV(e.n[1]);
            if(st == 1) // folded back: recovers once the load would take less than the limit
                ns = (e.p[0] - v) / e.p[1] < e.p[2] * 0.9 ? 0 : 1;
            else
                ns = (e.p[0] - v) / e.p[1] > e.p[2] ? 1 : 0;
            break;
        }
        case SINK: ns = VV(e.n[0]) - VV(e.n[1]) >= e.p[1] - (st ? 0.3 : 0.0) ? 1 : 0; break;
        case LDO: ns = (VV(e.n[1]) - VV(e.n[2]) >= 2.7 && VV(e.n[3]) - VV(e.n[2]) > 1.2) ? 1 : 0; break;
        case REFV: case ADC: ns = VV(e.n[1]) - VV(e.n[2]) >= 2.7 ? 1 : 0; break;
        case DAC: ns = VV(e.n[1]) - VV(e.n[2]) < 2.7 ? 0 : (dacOk ? 2 : 1); break;
        case SEEDLOAD: ns = VV(e.n[0]) - VV(e.n[1]) >= e.p[1] - (st ? 0.5 : 0.0) ? 1 : 0; break;
        default: break;
    }
    (void) src;
    const int ch = ns != st;
    st           = ns;
    return ch;
}

// ----------------------------------------------------------------------- the slow solve: the whole board
// Smooth models + Newton's method (how SPICE does it): each part is a smooth curve of terminal currents against
// terminal voltages, and Newton walks to the operating point. (The audio islands use the piecewise-linear states.)
static double Sp(double x, double s) // softplus: a smooth max(0, x)
{
    const double z = x / s;
    return z > 30 ? x : (z < -30 ? 0.0 : s * std::log1p(std::exp(z)));
}
static double Sg(double x, double s) { return 1.0 / (1.0 + std::exp(-std::max(-60.0, std::min(60.0, x / s)))); }
static double SMin(double a, double b, double s) { return a - Sp(a - b, s); }
static double Clamp(double x, double lo, double hi, double s) { return lo + Sp(x - lo, s) - Sp(x - hi, s); }

// the currents flowing INTO the part at each of its terminals (n[0..5]) for terminal voltages v[0..5]
void BoardSim::Impl::currents(const Elem& e, const double* v, double* i, double h) const
{
    for(int k = 0; k < 6; k++)
        i[k] = 0;
    // the chips' enables (power good, mute, LDO_SELECT...) follow the voltages at the START of the step: logic like
    // that reacts slowly anyway, and it keeps each step's maths smooth
    double p0[6];
    for(int k = 0; k < 6; k++)
        p0[k] = Ven.empty() ? v[k] : Ven[(size_t) e.n[k]];
    switch(e.t)
    {
        case RES:
        {
            double r = e.p[0];
            if(e.knob >= 0)
                r = 50.0 + 10e3 * (e.p[1] == 0 ? knob[e.knob] : 1.0 - knob[e.knob]);
            if(e.p[1] == 1.0 && e.heat > 0.6)
                r = 900.0;
            if(e.p[1] == 2.0)
                r = phones > 0 ? phones : 1e9;
            i[0] = (v[0] - v[1]) / r, i[1] = -i[0];
            break;
        }
        case CAP: i[0] = noCaps ? 0.0 : e.p[0] / h * ((v[0] - v[1]) - e.vS), i[1] = -i[0]; break;
        case DIODE:
        {
            const double vd = v[0] - v[1];
            i[0]            = Sp(vd - e.p[0], 0.02) / e.p[1] + 1e-9 * vd, i[1] = -i[0];
            break;
        }
        case OPAMP: // + 0, - 1, out 2, V+ 3, V- 4
        {
            const double on = Sg(p0[3] - p0[4] - 1.65, 0.2);
            const double lo = v[4] + 0.03, hi = lo + 0.05 + Sp(v[3] - v[4] - 0.11, 0.05); // (smoothly above lo while rails rise)
            // (gain 200 in the whole-board solver; the AC analysis uses the real gain around the resting point)
            const double x  = acMode ? e.vt0 + opGain * ((v[0] - v[1]) - e.dv0) + (v[4] - e.vn0) : opGain * (v[0] - v[1]) + v[4];
            const double vt = Clamp(x, lo, hi, 0.02 + 0.05 * Sg(hi - lo - 0.5, 0.2));
            const double io = on * (vt - v[2]) / e.p[1]; // into the output node
            const double iq = on * 5.5e-4 / 3.3 * (v[3] - v[4]);
            i[2]            = -io;
            i[3]            = 0.5 * io + iq; // (the output current comes half from each rail: smooth, and KCL holds)
            i[4]            = 0.5 * io - iq;
            break;
        }
        case TPA: // -IN 0, OUT 1, VDD 2, GND 3, HP_ON 4
        {
            const double vdd = v[2] - v[3];
            const double on  = Sg(p0[2] - p0[3] - 2.75, 0.2);
            const double un  = Sg(p0[4] - p0[3] - 1.2, 0.03); // (HP_ON is a logic input: muted or not)
            const double lim = std::max(0.05, vdd - 0.3);
            const double vt  = v[3] + un * Clamp(-2.0 * (v[0] - v[3]), -lim, lim, 0.02);
            const double io  = on * (vt - v[1]) / e.p[1];
            i[0]             = (v[0] - v[3]) / e.p[0];
            i[1]             = -io;
            i[2]             = io + on * 4e-3 / 3.3 * vdd;
            i[3]             = -i[0] - on * 4e-3 / 3.3 * vdd;
            break;
        }
        case VSS: // n0 = VSS, n1 = VDD, n2 = GND: the charge pump makes -VDD
        {
            const double on = Sg(p0[1] - p0[2] - 2.75, 0.2);
            const double io = on * ((v[2] - (v[1] - v[2])) - v[0]) / e.p[0];
            i[0]            = -io, i[2] = io;
            break;
        }
        case REG: // in 0, out 1, gnd 2
        {
            const double vin = v[0] - v[2];
            const double on  = Sg(p0[0] - p0[2] - 1.0, 0.3);
            const double tgt = SMin(e.p[0], vin - e.p[1], 0.02);
            const double ir  = 20.0 * (tgt - (v[1] - v[2]));
            const double io  = on * ir; // (overload is spotted by its current)
            const double iq  = e.p[3] / 5.0 * vin;
            i[1] = -io, i[0] = io + iq, i[2] = -iq;
            break;
        }
        case DCDC: // in+ 0, in- 1, out+ 2, out- 3
        {
            const double vin = v[0] - v[1];
            const double on  = Sg(p0[0] - p0[1] - 2.6, 0.4);
            const double io  = on * (e.p[0] * vin - (v[2] - v[3])) / e.p[1];
            const double iin = io * e.p[0] / e.p[2] + on * e.p[3] / 5.0 * vin;
            i[2] = -io, i[3] = io, i[0] = iin, i[1] = -iin;
            break;
        }
        case VSRC: // + 0, - 1
        {
            if(!power)
                break;
            const double ramp = std::min(1.0, rampT / 2e-3);
            const double is   = (e.p[0] * ramp - (v[0] - v[1])) / e.p[1]; // (overload is spotted by its current)
            i[0] = -is, i[1] = is;
            break;
        }
        case SINK:
        {
            const double vv = v[0] - v[1];
            i[0]            = Sg(p0[0] - p0[1] - e.p[1], 0.2) * e.p[0] / 3.3 * vv + 1e-6 * vv, i[1] = -i[0];
            break;
        }
        case LDO: // out 0, from 1, gnd 2, enable 3
        {
            const double on = Sg(p0[1] - p0[2] - 2.7, 0.2) * Sg(p0[3] - p0[2] - 1.2, 0.2);
            const double io = on * ((v[2] + e.p[0]) - v[0]) / e.p[1];
            i[0] = -io, i[1] = io;
            break;
        }
        case REFV: // out 0, supply 1, gnd 2
        {
            const double on = Sg(p0[1] - p0[2] - 2.7, 0.2);
            const double io = on * ((v[2] + e.p[0]) - v[0]) / e.p[1];
            i[0] = -io, i[1] = io;
            break;
        }
        case ADC: // in 0, supply 1, gnd 2: 20k to 0.9 V while powered
        {
            const double on = Sg(p0[1] - p0[2] - 2.7, 0.2);
            const double ii = on * ((v[0] - v[2]) - e.p[1]) / e.p[0] + 1e-7 * (v[0] - v[2]);
            i[0] = ii, i[2] = -ii;
            break;
        }
        case DAC: // out 0, supply 1, gnd 2: at its common mode while powered (the slow solve has no audio)
        {
            const double on = Sg(p0[1] - p0[2] - 2.7, 0.2);
            const double io = on * ((v[2] + e.p[0] + 1.41 * testDac[e.ch]) - v[0]) / e.p[1]; // (full scale = 1.41 V peak)
            i[0] = -io, i[1] = io;
            break;
        }
        case JACKIN: i[0] = (v[0] - v[1] - 2.0 * testIn[e.ch]) / e.p[0], i[1] = -i[0]; break; // (1.0 = 2 V peak)
        case NPN: // B 0, E 1, C 2: beta x the base current, falling off as it saturates
        {
            const double ib = Sp(v[0] - v[1] - 0.62, 0.025) / 300.0 + 1e-9 * (v[0] - v[1]);
            const double ic = e.p[0] * ib * std::tanh(Sp(v[2] - v[1], 0.02) / 0.08);
            i[0] = ib, i[2] = ic, i[1] = -(ib + ic);
            break;
        }
        case LDR: // the resistance follows its LED's current at the start of the step (an LDR is slow anyway)
        {
            const double il = Sp(p0[2] - p0[3] - 1.80, 0.02) / 15.0;
            const double ii = (v[0] - v[1]) / LdrOhms(il);
            i[0] = ii, i[1] = -ii;
            break;
        }
        case LEVDAC: // out 0, VDD 1, GND 2: the set level (never above its supply), ~1 ohm out
        {
            const double on = Sg(p0[1] - p0[2] - 2.5, 0.2);
            const double io = on * ((v[2] + std::min(levelV, std::max(0.0, p0[1] - p0[2]))) - v[0]) / 10.0 - (1.0 - on) * (v[0] - v[2]) * 1e-3;
            i[0] = -io, i[2] = io;
            break;
        }
        case SEEDLOAD: // VIN 0, GND 1, its 3V3 2
        {
            const double vv = v[0] - v[1];
            const double on = Sg(p0[0] - p0[1] - 4.0, 0.3);
            const double il = on * e.p[0] / 9.0 * vv + 1e-6 * vv;
            const double i3 = on * 20.0 * (3.3 - (v[2] - v[1]));
            i[0] = il, i[1] = -il + i3, i[2] = -i3;
            break;
        }
    }
}

// One time step of h seconds by Newton's method (with a line search: a step is only taken if it brings the
// currents closer to balancing; otherwise it's halved).
void BoardSim::Impl::residual(const std::vector<double>& v, double h, std::vector<double>& F, std::vector<double>* Jm) const
{
    const int n = nodes - 1;
    std::fill(F.begin(), F.end(), 0.0);
    if(Jm)
        std::fill(Jm->begin(), Jm->end(), 0.0);
    const double gp = gPseudo > 0 ? gPseudo : 1e-6 / h; // every node: gmin + a "pseudo" capacitor to ground (keeps
                                // Newton's steps small; it only slows the approach to rest, the resting voltages are unchanged)
    for(int k = 0; k < n; k++)
    {
        // finding the resting state: each node's pretend capacitor scaled to how stiffly the circuit holds that node
        // (a node behind megohms gets a tiny one), so every node settles at the same pace in pretend time
        const double g = gPseudo > 0 && !nodeScale.empty() ? gp * nodeScale[(size_t) k + 1] : gp;
        F[(size_t) k] += 1e-9 * v[(size_t) k + 1] + g * (v[(size_t) k + 1] - Vstart[(size_t) k + 1]);
        if(Jm)
            (*Jm)[(size_t) k * n + k] += 1e-9 + g;
    }
    double vv[6], ii[6], i2[6], w[6];
    for(const auto& e : el)
    {
        for(int k = 0; k < 6; k++)
            vv[k] = v[(size_t) e.n[k]];
        currents(e, vv, ii, h);
        for(int k = 0; k < 6; k++)
            if(e.n[k] > 0)
                F[(size_t) e.n[k] - 1] += ii[k];
        if(!Jm)
            continue;
        for(int c = 0; c < 6; c++) // the Jacobian, by small nudges of each terminal
        {
            if(e.n[c] <= 0)
                continue;
            bool dup = false;
            for(int q = 0; q < c; q++)
                dup |= e.n[q] == e.n[c];
            if(dup)
                continue;
            const double d = 1e-5; // central differences
            double       i3[6];
            for(int k = 0; k < 6; k++)
                w[k] = vv[k] + (e.n[k] == e.n[c] ? d : 0.0);
            currents(e, w, i2, h);
            for(int k = 0; k < 6; k++)
                w[k] = vv[k] - (e.n[k] == e.n[c] ? d : 0.0);
            currents(e, w, i3, h);
            for(int r = 0; r < 6; r++)
                if(e.n[r] > 0)
                    (*Jm)[(size_t) (e.n[r] - 1) * n + (e.n[c] - 1)] += (i2[r] - i3[r]) / (2 * d);
        }
    }
}

bool BoardSim::Impl::solveStep(double h)
{
    Vstart = V;
    if(!relaxEn)
        Ven = V;
    const int           n = nodes - 1;
    std::vector<double> Jm((size_t) n * n), F((size_t) n), F2((size_t) n), Vt;
    auto norm = [](const std::vector<double>& f) {
        double s = 0;
        for(double x : f)
            s += x * x;
        return s;
    };
    residual(V, h, F, &Jm);
    double f0 = norm(F), fCheck = 1e300;
    for(int it = 0; it < 60; it++)
    {
        Lu lu;
        if(!lu.factor(Jm, n))
            return false;
        std::vector<double> dx = F;
        lu.solve(dx);
        double mx = 0;
        for(int k = 0; k < n; k++)
            mx = std::max(mx, std::fabs(dx[(size_t) k]));
        if(std::getenv("NDEBUG7") && it == 20)
        {
            for(double t : {1e-2, 1e-4, 1e-6})
            {
                std::vector<double> Vq = V;
                for(int k = 0; k < n; k++)
                    Vq[(size_t) k + 1] -= t * dx[(size_t) k];
                residual(Vq, h, F2, nullptr);
                std::fprintf(stderr, "descent t %.0e: |F|^2 %.6g -> %.6g\n", t, f0, norm(F2));
            }
            // and which node's current changes against the prediction
            std::vector<double> Vq = V;
            for(int k = 0; k < n; k++)
                Vq[(size_t) k + 1] -= 1e-6 * dx[(size_t) k];
            residual(Vq, h, F2, nullptr);
            double worst = 0;
            int    wk    = 0;
            for(int k = 0; k < n; k++)
            {
                const double pred = F[(size_t) k] * (1 - 1e-6), got = F2[(size_t) k];
                if(std::fabs(pred - got) > worst)
                    worst = std::fabs(pred - got), wk = k;
            }
            std::string nm = "?";
            for(int z = 0; z < kNets; z++)
                if(netNode[(size_t) z] == wk + 1)
                    nm = kNetNames[z];
            for(auto& kv : point)
                if(nm == "?" && nodeOf[(size_t) kv.second] == wk + 1)
                    nm = kv.first;
            std::fprintf(stderr, "worst mismatch at %s: F %.4g predicted %.4g got %.4g\n", nm.c_str(), F[(size_t) wk],
                         F[(size_t) wk] * (1 - 1e-6), F2[(size_t) wk]);
        }
        // each node moves at most 2 V; if that doesn't reduce the imbalance, halve the step (no 2-cycles on a knee)
        const std::vector<double> V0 = V;
        double                    a  = mx > 2.0 ? 2.0 / mx : 1.0; // the whole step scaled (keeps it pointing downhill)
        for(int ls = 0; ls < 24; ls++, a *= 0.5)
        {
            V = V0;
            for(int k = 0; k < n; k++)
                V[(size_t) k + 1] -= a * dx[(size_t) k];
            residual(V, h, F, nullptr);
            if(norm(F) < f0)
                break;
        }
        residual(V, h, F, &Jm);
        f0             = norm(F);
        const bool took = true;
        if(std::getenv("NDEBUG3") && it == 10)
        {
            std::vector<double> Fp((size_t) n), Fm((size_t) n), Vp;
            double              worst = 0;
            int                 wr = 0, wc = 0;
            double              jn = 0, ja = 0;
            for(int c = 0; c < n; c++)
            {
                Vp = V;
                Vp[(size_t) c + 1] += 1e-5;
                residual(Vp, h, Fp, nullptr);
                Vp[(size_t) c + 1] -= 2e-5;
                residual(Vp, h, Fm, nullptr);
                for(int r = 0; r < n; r++)
                {
                    const double num = (Fp[(size_t) r] - Fm[(size_t) r]) / 2e-5, an = Jm[(size_t) r * n + c];
                    const double err = std::fabs(num - an) / (1e-6 + std::fabs(num) + std::fabs(an));
                    if(err > worst)
                        worst = err, wr = r, wc = c, jn = num, ja = an;
                }
            }
            auto nmOf = [&](int node) {
                for(int z = 0; z < kNets; z++)
                    if(netNode[(size_t) z] == node)
                        return std::string(kNetNames[z]);
                for(auto& kv : point)
                    if(nodeOf[(size_t) kv.second] == node)
                        return kv.first;
                return std::string("?");
            };
            std::fprintf(stderr, "JAC worst rel err %.3g at row %s col %s: numeric %.4g vs matrix %.4g\n", worst,
                         nmOf(wr + 1).c_str(), nmOf(wc + 1).c_str(), jn, ja);
        }
        if(std::getenv("NDEBUG8") && it == 30)
        {
            std::vector<std::pair<double, int>> top;
            for(int k = 0; k < n; k++)
                top.push_back({-std::fabs(F[(size_t) k]), k + 1});
            std::sort(top.begin(), top.end());
            std::fprintf(stderr, "stuck h %.1e res %.3g:", h, f0);
            for(int q = 0; q < 5; q++)
            {
                std::string nm = "?";
                for(int z = 0; z < kNets; z++)
                    if(netNode[(size_t) z] == top[(size_t) q].second)
                        nm = kNetNames[z];
                for(auto& kv : point)
                    if(nm == "?" && nodeOf[(size_t) kv.second] == top[(size_t) q].second)
                        nm = kv.first;
                std::fprintf(stderr, " %s(F=%.3g V=%.3g)", nm.c_str(), -top[(size_t) q].first, V[(size_t) top[(size_t) q].second]);
            }
            std::fprintf(stderr, "\n");
        }
        if(std::getenv("NDEBUG5"))
            std::fprintf(stderr, "S h %.2e it %d res %.2g\n", h, it, f0);
        if(std::getenv("NDEBUG2") && it >= 10 && it <= 14)
        {
            size_t w = 0;
            for(int k = 0; k < n; k++)
                if(std::fabs(dx[(size_t) k]) > std::fabs(dx[w]))
                    w = (size_t) k;
            std::string nm = "?";
            for(int z = 0; z < kNets; z++)
                if(netNode[(size_t) z] == (int) w + 1)
                    nm = kNetNames[z];
            for(auto& kv : point)
                if(nm == "?" && nodeOf[(size_t) kv.second] == (int) w + 1)
                    nm = kv.first;
            std::fprintf(stderr, "h %.2e it %d mx %.3g res %.3g worst %s V %.4f F %.3g\n", h, it, mx, f0, nm.c_str(), V[w + 1], F[w]);
        }
        if(f0 < 1e-13 || (mx < 1e-5 && f0 < 1e-11)) // every node's currents balance to ~0.3 uA
            return true;
        if(it % 8 == 7) // stalled close to balance (tens of uA at most): good enough, the next steps refine it
        {
            if(f0 < (h < 1e-4 ? 1e-8 : 1e-11) && f0 > 0.99 * fCheck) // (tiny steps inside a violent event: a looser stop, or it can wander off)
                return true;
            fCheck = f0;
        }
        if(!took)
            return false;
    }
    if(std::getenv("NDEBUG6"))
        for(const auto& e : el)
            if(e.t == OPAMP && (e.ref == "U11" || e.ref == "U8"))
                std::fprintf(stderr, "FAIL h %.1e %s.%d +%.3f -%.3f out %.3f V+ %.3f V- %.3f start: +%.3f -%.3f out %.3f V+ %.3f\n", h,
                             e.ref.c_str(), e.ch, V[(size_t) e.n[0]], V[(size_t) e.n[1]], V[(size_t) e.n[2]], V[(size_t) e.n[3]],
                             V[(size_t) e.n[4]], Vstart[(size_t) e.n[0]], Vstart[(size_t) e.n[1]], Vstart[(size_t) e.n[2]],
                             Vstart[(size_t) e.n[3]]);
    return f0 < 1e-10;
}

// the resting state after power-up, found directly (SPICE's "source stepping"): the adapter's voltage is raised in
// notches, each solved at rest from the last one
void BoardSim::Impl::operatingPoint()
{
    // each node's stiffness: the resistors on it (1 / 1k = 1); megohm nodes get ~1e-3, never below 1e-4
    nodeScale.assign((size_t) nodes, 0.0);
    for(const auto& e : el)
        if(e.t == RES && e.p[0] > 0)
            for(int q = 0; q < 2; q++)
                nodeScale[(size_t) e.n[q]] += 1e3 / e.p[0];
    for(auto& x : nodeScale)
        x = std::max(1e-4, std::min(1.0, x == 0.0 ? 1.0 : x));
    // pseudo-transient continuation (how circuit simulators find a resting state): straight to full voltage, with a
    // strong pretend capacitor on every node so nothing runs off; it's weakened pass by pass as things settle, and
    // the chips' enables follow each pass
    rampT   = 2e-3;
    double g = 1e-2;
    for(int pass = 0; pass < 200 && g > 1e-9; pass++)
    {
        gPseudo       = g;
        const auto V0 = V;
        const bool ok = solveStep(1.0);
        double     mv = 0;
        for(size_t k = 0; k < V.size(); k++)
            mv = std::max(mv, std::fabs(V[k] - V0[k]));
        if(std::getenv("NDEBUGOP"))
            std::fprintf(stderr, "op pass %d g %.1e ok %d moved %.4f\n", pass, g, (int) ok, mv);
        if(!ok)
        {
            V = V0, g *= 10.0; // too bold: hold things back more
            continue;
        }
        for(auto& e : el)
            if(e.t == CAP)
                e.vS = V[(size_t) e.n[0]] - V[(size_t) e.n[1]];
        g *= mv < 0.05 ? 0.1 : (mv < 0.5 ? 0.5 : 1.0);
    }
    gPseudo = 0, needOp = false, hLast = 1e-3;
}

bool BoardSim::Impl::solveSlow(double dt)
{
    if(needOp && power)
        operatingPoint();
    double left = dt, h = std::min(dt, hLast * 4.0);
    while(left > 1e-12)
    {
        h = std::min(h, left);
        if(power && rampT < 2e-3) // while the plug goes in, small steps
            h = std::min(h, 5e-5);
        const std::vector<double> V0 = V;
        if(rampT < 2e-3 && power) // the adapter's plug going in: 0 -> 9 V over 2 ms
            rampT += h;
        const bool ok = solveStep(h);
        if(ok || h <= 1e-7)
        {
            for(auto& e : el) // the capacitors remember this step
                if(e.t == CAP)
                    e.vS = V[(size_t) e.n[0]] - V[(size_t) e.n[1]];
            left -= h, hLast = h;
            h *= 2.0;
            continue;
        }
        V = V0; // didn't converge: back up and take a smaller step
        h = std::max(1e-7, h * 0.25);
    }
    // the audio solver and the checks use the parts' piecewise-linear states: read them off the solution
    for(auto& e : el)
        for(int k = 0; k < 4; k++)
            if(!updateState(e, false, V, nullptr))
                break;
    return true;
}

static double RatingW(const std::string& fp)
{
    if(fp.find("0402") != std::string::npos)
        return 0.0625;
    if(fp.find("0603") != std::string::npos)
        return 0.1;
    if(fp.find("1206") != std::string::npos)
        return 0.25;
    return 0;
}

void BoardSim::Impl::afterSlow(double dt)
{
    double vv[6], ii[6];
    for(auto& e : el)
    {
        for(int k = 0; k < 6; k++)
            vv[k] = V[(size_t) e.n[k]];
        currents(e, vv, ii, 1e3); // (capacitors: no current at rest)
        double imax = 0, w = 0;
        for(int k = 0; k < 6; k++)
        {
            bool dup = false;
            for(int q = 0; q < k; q++)
                dup |= e.n[q] == e.n[k] && e.n[k] != 0;
            if(!dup)
                imax = std::max(imax, std::fabs(ii[k]));
            w += vv[k] * ii[k]; // the power going into the part
        }
        e.i = imax, e.w = std::max(0.0, w);
        if(e.t == VSRC)
            e.i = -ii[0], e.w = 0, e.st = e.i > e.p[2] ? 1 : 0;
        if(e.t == REG)
            e.st = e.st == 0 ? 0 : (-ii[1] > e.p[2] ? 3 : e.st);
        if(e.t == LDR) // remember its LED's current (the audio solver's LDR value)
            e.heat = Sp(vv[2] - vv[3] - 1.80, 0.02) / 15.0;
        if(e.t == RES && e.p[1] == 1.0) // the PTC warms with the current through it (and trips when it's too much)
        {
            const double a = std::min(1.0, dt / 0.5);
            e.heat += (std::fabs(ii[0]) - e.heat) * a;
            if(!power)
                e.heat = 0;
        }
    }
}

// ----------------------------------------------------------------------- small-signal (AC) analysis
// Linearised at the present operating point: (G + j w C) dV = -dF/dsource. src 0/1 = the input jack L/R (per unit of
// testIn), 2/3 = the codec's DAC L/R (per unit of testDac). Returns dV for every node (index = node).
std::vector<std::complex<double>> BoardSim::Impl::ac(int src, double hz) const
{
    auto&     self = const_cast<Impl&>(*this);
    const int n    = nodes - 1;
    std::vector<double> G((size_t) n * n), F0((size_t) n), F1((size_t) n);
    const double gpSave = self.gPseudo;
    for(auto& e : self.el) // the op-amps' resting points (as the whole-board solver found them)
        if(e.t == OPAMP)
        {
            e.dv0 = V[(size_t) e.n[0]] - V[(size_t) e.n[1]];
            e.vn0 = V[(size_t) e.n[4]];
            e.vt0 = 200.0 * e.dv0 + e.vn0;
        }
    self.gPseudo        = 1e-12; // no pretend capacitors
    self.noCaps         = true;
    self.opGain         = 1e5;
    self.acMode         = true;
    self.Vstart = V, self.Ven = V;
    self.residual(V, 1.0, F0, &G);
    double* t = src < 2 ? &self.testIn[src] : &self.testDac[src - 2];
    const double t0 = *t;
    *t              = t0 + 1e-3;
    self.residual(V, 1.0, F1, nullptr);
    *t           = t0;
    self.noCaps  = false;
    self.opGain  = 200.0;
    self.acMode  = false;
    self.gPseudo = gpSave;
    const double w = 2.0 * 3.14159265358979 * hz;
    using C = std::complex<double>;
    std::vector<C> A((size_t) n * n), b((size_t) n);
    for(size_t k = 0; k < A.size(); k++)
        A[k] = G[k];
    for(const auto& e : el) // the capacitors' admittance
        if(e.t == CAP)
        {
            const int a = e.n[0] - 1, c = e.n[1] - 1;
            const C   y(0.0, w * e.p[0]);
            if(a >= 0)
                A[(size_t) a * n + a] += y;
            if(c >= 0)
                A[(size_t) c * n + c] += y;
            if(a >= 0 && c >= 0)
                A[(size_t) a * n + c] -= y, A[(size_t) c * n + a] -= y;
        }
    for(int k = 0; k < n; k++)
        b[(size_t) k] = -(F1[(size_t) k] - F0[(size_t) k]) / 1e-3;
    for(int k = 0; k < n; k++) // Gaussian elimination with partial pivoting
    {
        int p = k;
        for(int r = k + 1; r < n; r++)
            if(std::abs(A[(size_t) r * n + k]) > std::abs(A[(size_t) p * n + k]))
                p = r;
        if(p != k)
        {
            for(int c = 0; c < n; c++)
                std::swap(A[(size_t) k * n + c], A[(size_t) p * n + c]);
            std::swap(b[(size_t) k], b[(size_t) p]);
        }
        for(int r = k + 1; r < n; r++)
        {
            const C f = A[(size_t) r * n + k] / A[(size_t) k * n + k];
            if(f == C(0))
                continue;
            for(int c = k; c < n; c++)
                A[(size_t) r * n + c] -= f * A[(size_t) k * n + c];
            b[(size_t) r] -= f * b[(size_t) k];
        }
    }
    std::vector<C> x((size_t) nodes, C(0));
    for(int k = n - 1; k >= 0; k--)
    {
        C sum = b[(size_t) k];
        for(int c = k + 1; c < n; c++)
            sum -= A[(size_t) k * n + c] * x[(size_t) c + 1];
        x[(size_t) k + 1] = sum / A[(size_t) k * n + k];
    }
    return x;
}

// ----------------------------------------------------------------------- what works and what's wrong
void BoardSim::Impl::checks()
{
    faults.clear();
    auto F   = [&](int sev, const std::string& where, const std::string& what) { faults.push_back({sev, where, what}); };
    auto nd  = [&](const char* net) { return netN(net); };
    auto vOf = [&](const char* net, const char* ref) { return V[(size_t) nd(net)] - V[(size_t) nd(ref)]; };
    char buf[160];

    // power in
    const Elem* src = nullptr;
    for(auto& e : el)
        if(e.t == VSRC)
            src = &e;
    const double vin = vOf("+9V_RAW", "GND");
    if(!power)
        F(0, "9 V", "the adapter is unplugged");
    else if(src && src->st)
    {
        std::snprintf(buf, sizeof(buf), "short circuit: the 9 V adapter is overloaded (%.1f A) - look for the hot part", src->i);
        F(2, "9V adapter", buf);
    }
    else if(std::fabs(V[(size_t) node("DC.+")] - V[(size_t) node("DC.-")]) > 5 && std::fabs(vin) < 1)
        F(1, "wiring", "the 9 V jack isn't reaching the board: wire it to dc + / dc - on the seed3 + 9v header");
    else if(vin < -1)
        F(1, "D1", "9 V is reversed (dc + and dc - swapped): D1 blocks it, nothing runs, nothing is harmed");
    for(auto& e : el)
    {
        if(e.t == RES && e.p[1] == 1.0 && e.heat > 0.6)
            F(2, e.ref, "the fuse tripped: too much current (a short somewhere after it)");
        if(e.t == REG && e.st == 3)
            F(2, e.ref, "regulator at its current limit: something on its rail is shorted or overloaded");
        if(e.t == REG && e.w > (e.ref == "U1" ? 1.2 : 0.3))
        {
            std::snprintf(buf, sizeof(buf), "regulator overheating (%.2f W)", e.w);
            F(2, e.ref, buf);
        }
        if(e.t == RES && e.knob < 0 && e.p[1] == 0.0)
        {
            double rating = 0.0625;
            for(int k = 0; k < kParts; k++)
                if(e.ref == kBParts[k].ref)
                    rating = RatingW(kBParts[k].fp);
            if(rating > 0 && e.w > rating * 1.2)
            {
                std::snprintf(buf, sizeof(buf), "burning: %.2f W in a %.3f W resistor", e.w, rating);
                F(2, e.ref, buf);
            }
        }
        if(e.t == DIODE && std::fabs(e.i) > (e.ref == "SEED3" ? 0.02 : 1.0))
            F(2, e.ref, e.ref == "SEED3" ? "a Seed3 pin is being pushed past its supply: that pin can die - check its wire"
                                         : "a protection clamp is carrying a big current");
        const double vo = V[(size_t) e.n[2]], vhi = V[(size_t) e.n[3]], vlo = V[(size_t) e.n[4]];
        if(e.t == OPAMP && vhi - vlo > 1.8 && (vo > vhi - 0.1 || vo < vlo + 0.1)) // its output really at a rail
        {
            const char* hint = e.ref == "U11" ? "pg-line stage stuck at its rail: check pg-line's 6 wires"
                               : e.ref == "U12" ? "pg-hp stage stuck at its rail: check pg-hp's 6 wires and the fx loop bridges"
                                                : "input buffer stuck at its rail";
            F(1, e.ref, hint);
        }
    }
    // rails
    auto rail = [&](const char* net, const char* ref, double lo, double hi, const char* who) {
        const double v = vOf(net, ref);
        if(v > hi)
        {
            std::snprintf(buf, sizeof(buf), "%s is %.2f V (max %.1f): %s can be damaged", net, v, hi, who);
            F(2, net, buf);
        }
        return v >= lo && v <= hi;
    };
    const bool p5   = rail("+5V", "GND", 4.5, 5.6, "the isolated converter");
    const bool p33  = rail("+3V3", "GND", 3.0, 3.6, "the isolators' pedal side");
    const bool i5   = rail("ISO5V_RAW", "IGND", 4.0, 6.0, "the isolated side");
    const bool i33  = rail("ISO3V3", "IGND", 3.0, 3.6, "the codec / headphone amp");
    (void) p5, (void) i5;
    // the Seed3
    const int  sv = node("SEED.39"), sg = node("SEED.40");
    seedOn        = V[(size_t) sv] - V[(size_t) sg] >= 4.0;
    const bool sameGnd = sg == 0;
    if(!seedOn && vin > 5)
        F(1, "wiring", "the Seed3 has no power: vin -> Seed3 pin 39, gnd -> pin 40");
    else if(seedOn && !sameGnd)
        F(1, "wiring", "the Seed3's ground isn't the board's ground: wire gnd to Seed3 pin 40");
    auto linked = [&](const char* seedPin, const char* net) { return node(seedPin) == nd(net); };
    const bool i2c = seedOn && sameGnd && p33 && i33 && linked("SEED.12", "SEED_SCL") && linked("SEED.13", "SEED_SDA");
    const bool clk = seedOn && sameGnd && p33 && i33 && linked("SEED.35", "SEED_SCK") && linked("SEED.34", "SEED_FS");
    i2sOut          = clk && linked("SEED.33", "SEED_TX");
    i2sIn           = clk && linked("SEED.32", "SEED_RX");
    codecPowered    = i33 && vOf("CRESET", "IGND") > 2.0;
    codecConf       = codecPowered && i2c && clk;
    adcOk = codecConf && i2sIn, dacOk = codecConf && i2sOut;
    if(seedOn && sameGnd && !i2c)
        F(1, "wiring", "the codec can't be set up: scl -> Seed3 pin 12, sda -> pin 13 (swapped or missing?)");
    if(seedOn && sameGnd && !clk)
        F(1, "wiring", "the codec gets no clock: sck -> Seed3 pin 35, fs -> pin 34");
    if(clk && !i2sOut)
        F(1, "wiring", "no audio to the codec: tx -> Seed3 pin 33 (tx / rx swapped?)");
    if(clk && !i2sIn)
        F(1, "wiring", "no audio from the codec: rx -> Seed3 pin 32 (tx / rx swapped?)");
    // the headphone amp: un-mutes ~0.5 s after power
    ampOn = false;
    for(auto& e : el)
        if(e.t == TPA && e.st != 3 && e.st != 4)
            ampOn = true;
    // the fx loop
    if(nd("LO_L") == nd("FXR_L") || nd("LO_R") == nd("FXR_R"))
        F(1, "J22", "the fx loop is bridged: take the jumpers off J22 (s l - r l, s r - r r) - the leveller drives the returns, "
                    "a bridge shorts its output to the codec's");
    // isolation: is anything joining the two grounds (wires, not the barrier parts)?
    {
        std::vector<int> pr((size_t) nodes);
        for(int i = 0; i < nodes; i++)
            pr[(size_t) i] = i;
        std::function<int(int)> fd = [&](int a) { return pr[(size_t) a] == a ? a : pr[(size_t) a] = fd(pr[(size_t) a]); };
        for(auto& e : el)
            if((e.t == RES && e.p[0] < 1e7 && e.p[1] != 3.0) || e.t == DIODE || e.t == REG || e.t == OPAMP)
                for(int k = 1; k < (e.t == OPAMP ? 5 : (e.t == REG ? 3 : 2)); k++)
                    pr[(size_t) fd(e.n[0])] = fd(e.n[k]);
        isolatedOk = ignd < 0 || fd(0) != fd(ignd);
        if(!isolatedOk)
            F(2, "wiring", "isolation broken: a wire joins the isolated side to the pedal side (check the jack, pot and fx loop wires)");
    }
    if(power && seedOn && !adcOk && !dacOk && faults.empty())
        F(1, "codec", "the codec isn't running");
}

// ======================================================================= the public side
BoardSim::BoardSim() : d_(new Impl())
{
    wires_ = DefaultWiring();
    d_->build(wires_);
}
BoardSim::~BoardSim() = default;

void BoardSim::setWiring(const std::vector<Wire>& w)
{
    std::lock_guard<std::mutex> l(d_->mx);
    wires_ = w;
    d_->build(w);
}
void BoardSim::setKnobs(float a, float b)
{
    std::lock_guard<std::mutex> l(d_->mx);
    if(a != d_->knob[0] || b != d_->knob[1])
        d_->knob[0] = a, d_->knob[1] = b, d_->knobVer++;
}
void BoardSim::setLeveller(double volts)
{
    std::lock_guard<std::mutex> l(d_->mx);
    d_->levelV = std::max(0.0, std::min(2.0, volts)); // (the firmware stops at 1.45 V = 8 mA; past ~2 V the NPN saturates)
}
void BoardSim::setTestSignals(double inL, double inR, double dacL, double dacR)
{
    std::lock_guard<std::mutex> l(d_->mx);
    d_->testIn[0] = inL, d_->testIn[1] = inR, d_->testDac[0] = dacL, d_->testDac[1] = dacR;
}
std::vector<std::complex<double>> BoardSim::acResponse(int src, double hz) const
{
    std::lock_guard<std::mutex> l(d_->mx);
    auto                        x = d_->ac(src, hz);
    std::vector<std::complex<double>> r(kNets);
    for(int i = 0; i < kNets; i++)
        r[(size_t) i] = x[(size_t) d_->netNode[(size_t) i]];
    return r;
}
std::vector<double> BoardSim::nodeVolts() const
{
    std::lock_guard<std::mutex> l(d_->mx);
    std::vector<double> r(kNets);
    for(int i = 0; i < kNets; i++)
        r[(size_t) i] = d_->V[(size_t) d_->netNode[(size_t) i]];
    return r;
}
void BoardSim::setPower(bool on)
{
    std::lock_guard<std::mutex> l(d_->mx);
    if(on && !d_->power)
        d_->needOp = true;
    d_->power = on;
    if(!on)
        d_->rampT = 0;
}
void BoardSim::setHeadphones(float ohms)
{
    std::lock_guard<std::mutex> l(d_->mx);
    d_->phones = ohms;
    d_->knobVer++;
}

void BoardSim::stepSlow(double dt)
{
    std::lock_guard<std::mutex> l(d_->mx);
    d_->solveSlow(dt);
    d_->afterSlow(dt);
    d_->t += dt;
    d_->checks();
    d_->railsForAudio = d_->V;
    adcOk_            = d_->adcOk;
    dacOk_            = d_->dacOk;
}

// one audio island, one sample (no allocation: everything lives in the island)
static void StepIsland(BoardSim::Impl& d, Island& I, const double* src)
{
    const double dt = 1.0 / 48000.0;
    const int    n  = (int) I.nodes.size();
    Stamp&       s  = I.st;
    s.col = &I.col, s.held = &I.v;
    for(int it = 0; it < 6; it++)
    {
        I.sigTmp.clear();
        for(int k : I.elems)
            I.sigTmp.push_back(d.el[(size_t) k].stF);
        I.sigTmp.push_back(d.knobVer);
        std::fill(s.Y.begin(), s.Y.end(), 0.0), std::fill(s.J.begin(), s.J.end(), 0.0);
        for(int k = 0; k < n; k++)
            s.Y[(size_t) k * n + k] += 1e-9;
        for(int k : I.elems)
            d.stampElem(s, d.el[(size_t) k], true, dt, src, I.v);
        if(I.sigTmp != I.sig)
        {
            if(!I.lu.factor(s.Y, n))
                return;
            I.sig = I.sigTmp;
        }
        I.lu.solve(s.J);
        std::copy(I.v.begin(), I.v.end(), I.nvTmp.begin());
        for(int k = 0; k < n; k++)
            I.nvTmp[(size_t) I.nodes[(size_t) k]] = s.J[(size_t) k];
        int changed = 0;
        for(int k : I.elems)
        {
            auto& e = d.el[(size_t) k];
            if(e.t == DIODE || e.t == OPAMP || e.t == TPA)
                changed += d.updateState(e, true, I.nvTmp, src);
        }
        if(!changed || it == 5)
        {
            for(int k : I.elems) // the capacitors remember this sample
            {
                auto& e = d.el[(size_t) k];
                if(e.t != CAP)
                    continue;
                const double vnew = I.nvTmp[(size_t) e.n[0]] - I.nvTmp[(size_t) e.n[1]];
                const double G    = 2.0 * e.p[0] / dt;
                e.iF              = G * vnew - (G * e.vF + e.iF);
                e.vF              = vnew;
            }
            std::swap(I.v, I.nvTmp);
            return;
        }
    }
}

static void SyncIsland(BoardSim::Impl& d, Island& I, bool first)
{
    for(int k = 0; k < (int) d.nodes; k++) // the held nodes follow the slow solve
        if(I.col[(size_t) k] < 0 || first)
            I.v[(size_t) k] = d.railsForAudio[(size_t) k];
    for(int k : I.elems)
    {
        auto& e = d.el[(size_t) k];
        if(first || !(e.t == DIODE || e.t == OPAMP || e.t == TPA)) // regulators, codec, sources: the slow state
            e.stF = e.st;
        if(e.t == TPA && (e.st == 3 || e.st == 4))                   // powered down / muted wins
            e.stF = e.st;
        if(first && e.t == CAP)
            e.vF = d.railsForAudio[(size_t) e.n[0]] - d.railsForAudio[(size_t) e.n[1]], e.iF = 0;
    }
}

void BoardSim::runInput(const float* inL, const float* inR, float* adcL, float* adcR, int n)
{
    std::unique_lock<std::mutex> l(d_->mx, std::try_to_lock);
    if(!l.owns_lock())
    {
        std::fill(adcL, adcL + n, 0.f), std::fill(adcR, adcR + n, 0.f);
        return;
    }
    auto& d = *d_;
    for(auto& I : d.isl)
        if(I.hasJack || I.adcNode[0] >= 0 || I.adcNode[1] >= 0)
            SyncIsland(d, I, I.sig.empty());
    for(int i = 0; i < n; i++)
    {
        const double src[4] = {inL[i], inR[i], 0, 0};
        double       a[2]   = {0, 0};
        for(auto& I : d.isl)
            if(I.hasJack || I.adcNode[0] >= 0 || I.adcNode[1] >= 0)
            {
                StepIsland(d, I, src);
                for(int c = 0; c < 2; c++)
                    if(I.adcNode[c] >= 0) // the codec samples its input against its 0.9 V common mode
                        a[c] = (I.v[(size_t) I.adcNode[c]] - I.v[(size_t) d.ignd] - 0.9) / 1.41;
            }
        const bool ok = d.adcOk;
        adcL[i]       = ok ? (float) std::max(-1.0, std::min(1.0, a[0])) : 0.f;
        adcR[i]       = ok ? (float) std::max(-1.0, std::min(1.0, a[1])) : 0.f;
    }
}

void BoardSim::runOutput(const float* dacL, const float* dacR, float* outL, float* outR, int n)
{
    std::unique_lock<std::mutex> l(d_->mx, std::try_to_lock);
    if(!l.owns_lock())
    {
        std::fill(outL, outL + n, 0.f), std::fill(outR, outR + n, 0.f);
        return;
    }
    auto& d = *d_;
    for(auto& I : d.isl)
        if(I.hasDac)
            SyncIsland(d, I, I.sig.empty());
    for(int i = 0; i < n; i++)
    {
        const double src[4] = {0, 0, std::max(-1.0f, std::min(1.0f, dacL[i])), std::max(-1.0f, std::min(1.0f, dacR[i]))};
        double       o[2]   = {0, 0};
        for(auto& I : d.isl)
            if(I.hasDac)
            {
                StepIsland(d, I, src);
                for(int c = 0; c < 2; c++)
                    if(I.outNode[c] >= 0)
                        o[c] = (I.v[(size_t) I.outNode[c]] - I.v[(size_t) d.ignd]) / 2.0; // 1.0 = 2 V peak
            }
        outL[i] = (float) o[0], outR[i] = (float) o[1];
    }
}

Report BoardSim::report() const
{
    std::lock_guard<std::mutex> l(d_->mx);
    const auto& d = *d_;
    Report      r;
    r.t = d.t;
    r.netVolts.resize(kNets);
    for(int i = 0; i < kNets; i++)
    {
        const int n = d.netNode[(size_t) i];
        r.netVolts[(size_t) i] = (float) d.V[(size_t) n];
    }
    std::map<std::string, ElemReport> by;
    for(const auto& e : d.el)
    {
        auto& x = by[e.ref];
        x.ref   = e.ref;
        x.amps  = std::max(x.amps, (float) std::fabs(e.i));
        x.watts += (float) e.w;
    }
    for(auto& kv : by)
        r.parts.push_back(kv.second);
    r.faults        = d.faults;
    r.seedPowered   = d.seedOn;
    r.codecPowered  = d.codecPowered;
    r.codecConfigured = d.codecConf;
    r.i2sIn = d.i2sIn, r.i2sOut = d.i2sOut, r.isolated = d.isolatedOk, r.ampOn = d.ampOn;
    for(const auto& e : d.el)
        if(e.t == VSRC)
            r.supplyAmps = (float) e.i;
    return r;
}
} // namespace pgsim
