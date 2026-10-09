#pragma once
// The carrier board, simulated as a circuit (no JUCE in here: the native tests build it too).
//
// Built from the REAL routed board (BoardData.h, exported from the KiCad file: every part and the net each of its
// pads sits on) plus the jumper wires you make by hand (the wiring list: board header pins <-> Seed3 pins, pots,
// DC jack, fx loop bridges). Each part is an electrical model (resistors, capacitors, clamp diodes, regulators, the
// isolated converter, op-amps with their supply rails, the headphone amp, the codec's analog pins and supplies).
//
// Two solvers share one netlist (modified nodal analysis, piecewise-linear parts):
//  - SLOW: the whole board (power, bias, every current) every few tens of ms on a background thread: power-up,
//    fuse heating, regulators, shorts, overloads, what's isolated from what -> the faults list + the board view.
//  - FAST: the audio paths (jack -> input stage -> codec ADC, codec DAC -> fx loop -> pg-hp -> headphone amp -> jack),
//    sample by sample at 48 kHz on the audio thread, with the rails held at the slow solver's values.
// Wrong wiring therefore really changes the sound: an open fx loop is silence, a pot on the wrong pins is the wrong
// level or no sound, swapped clock / data jumpers mean the codec never gets audio, no ground means nothing powers up.
#include <array>
#include <complex>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace pgsim
{
struct Wire
{
    std::string a, b; // endpoints: "J10.3" (board part.pad), "SEED.39" (Seed3 pin), "PGLINE.L3" (pot pin), "DC.+"
};
std::vector<Wire> DefaultWiring(); // what WIRING.md says (everything right)

struct Fault
{
    int         sev;   // 2 = damage / smoke, 1 = won't work, 0 = note
    std::string where; // part ref or "wiring"
    std::string what;  // plain words
};

struct ElemReport
{
    std::string ref;     // board part (or "SEED3", "PGLINE", "DC jack"...)
    float       amps;    // current through it (largest terminal current)
    float       watts;   // heat in it
    float       rating;  // watts it can take (0 = n/a)
};

struct Report
{
    double                  t = 0.0;     // simulated seconds since power-up
    std::vector<float>      netVolts;    // per board net (BoardData kNetNames order), relative to its own ground
    std::vector<ElemReport> parts;
    std::vector<Fault>      faults;
    bool seedPowered = false, codecPowered = false, codecConfigured = false, i2sIn = false, i2sOut = false,
         isolated = true, ampOn = false;
    float supplyAmps = 0.f; // drawn from the 9 V jack
};

class BoardSim
{
  public:
    BoardSim();
    ~BoardSim();

    // ---- set-up (any thread except audio)
    void setWiring(const std::vector<Wire>& w); // rebuilds the circuit; power restarts from off
    const std::vector<Wire>& wiring() const { return wires_; }
    void setKnobs(float pgLine, float pgHp);    // 0..1 (0 = full left), 0.5 = the centre click
    void setPower(bool on);                     // the 9 V adapter plugged in or not
    void setHeadphones(float ohms);             // load on the output jack (0 = nothing plugged in)
    // test signals for the whole-board solver (stepSlow): what's played into the input jack (1.0 = 2 V peak) and what
    // the codec's DAC sends out (1.0 = full scale)
    void setTestSignals(double inL, double inR, double dacL, double dacR);
    // the analog leveller: what the firmware wrote to its DAC (volts; 0 = LEDs dark = untouched)
    void setLeveller(double volts);
    // small-signal (AC) analysis at the present operating point: each board net's response (volts) to 1 unit of a
    // source at hz: src 0/1 = input jack L/R (1 unit = 2 V peak), 2/3 = DAC L/R (1 unit = full scale)
    std::vector<std::complex<double>> acResponse(int src, double hz) const;
    std::vector<double>               nodeVolts() const; // each board net's resting voltage (BoardData order)

    // ---- audio thread, 48 kHz. Levels: 1.0 = 2 V peak at a jack (the pedal's own units).
    // adc / dac are what the firmware sees / sends (it multiplies by kInGain / kOutGain, PgCore units).
    void runInput(const float* inL, const float* inR, float* adcL, float* adcR, int n);
    void runOutput(const float* dacL, const float* dacR, float* outL, float* outR, int n);
    bool adcWorks() const { return adcOk_.load(); }
    bool dacWorks() const { return dacOk_.load(); }

    // ---- slow solve: call from a timer / thread every ~20-50 ms with the time that passed
    void stepSlow(double dt);
    Report report() const;

    struct Impl;

  private:
    std::vector<Wire>     wires_;
    std::unique_ptr<Impl> d_;
    std::atomic<bool>     adcOk_{false}, dacOk_{false};
};
} // namespace pgsim
