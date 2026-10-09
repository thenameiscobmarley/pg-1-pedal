// PG-1 screen constants shared by PgCore.cpp and PgPages.cpp: BIOS palette, tab names, layout.
#pragma once
#include "Canvas.h"

namespace pg
{
namespace ui
{
// The palette is switchable (config tab: "pearl" or the classic "bios"), so these are variables.
extern uint16_t kBlue;     // boxes / tabs
extern uint16_t kBlueDeep; // graph background
extern uint16_t kGrey;     // screen chrome (title, panels)
extern uint16_t kGreyDark; // live-number chips
extern uint16_t kWhite, kYellow, kCyan, kLtBlue, kDimText, kGrid, kSpecFill, kPink, kCream;
enum Theme
{
    THEME_PEARL, // light blue-white chrome like the pedal, bright royal-blue boxes, deep navy graphs
    THEME_BIOS   // the classic BIOS blue on grey
};
void SetTheme(int theme);
// Every control's name comes from this one rule: the prefix, then a number for the knobs (pg-1 .. pg-4)
// and a letter for the footswitches (pg-a .. pg-c). Nothing else on the screen spells a control's name out.
constexpr const char* kCtrlPrefix = "pg-";
const char* KnobName(int k); // 0 -> "pg-1"
const char* FootName(int f); // 0 -> "pg-a"
constexpr uint16_t kBandCol[4] = {Canvas::Rgb(255, 150, 220), Canvas::Rgb(200, 165, 255), Canvas::Rgb(120, 205, 255),
                                  Canvas::Rgb(140, 255, 215)};

static const char* const kTabNames[16] = {"hum",   "dyn eq", "comp",     "clarity", "saturate", "de-harsh", "safety",   "visual",
                                          "input", "health", "takeback", "width",   "loudness", "config",   "multiband", "pid"};
static const char* const kTabTitles[16] = {"hum + hiss",    "dynamic eq", "compressor",           "clarity + bass", "saturation",
                                           "de-harsh",      "ear + speaker safety", "visualizer",   "input auto-level",
                                           "health checks", "takeback",   "stereo width",         "quiet loudness", "configs",
                                           "multiband dynamics", "pid auto-adjust"};

// one line each, shown at the bottom of the home screen for the focused tab (what it's for, in plain words)
static const char* const kTabWhat[16] = {
    "hum: removes mains hum + background hiss",     "dyn eq: 4 bands that act only when needed",
    "comp: evens out loud and quiet parts",          "clarity: finds muddy / buried bands + fixes",
    "saturate: warm analog-style drive",             "de-harsh: tames sharp, piercing highs",
    "safety: protects your ears + speakers",         "visual: spectrum, scope + meters",
    "input: levels whatever comes in",               "health: checks the pedal's wiring + sound",
    "takeback: gives back punch, detail, air",       "width: wider or narrower stereo",
    "loudness: fuller sound at low volume",          "config: save / load setups, screen, knobs",
    "multiband: a compressor per frequency range",   "pid: auto-adjusts the sound to a target"};

constexpr int kGx = 3, kGy = 22, kGw = 314, kGh = 136; // graph area on every page
constexpr int kPanelY = 160;                          // grey panel: strip at 164, knob boxes at 186
} // namespace ui
} // namespace pg
