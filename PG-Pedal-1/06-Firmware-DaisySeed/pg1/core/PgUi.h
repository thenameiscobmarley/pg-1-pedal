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
constexpr uint16_t kBandCol[4] = {Canvas::Rgb(255, 150, 220), Canvas::Rgb(200, 165, 255), Canvas::Rgb(120, 205, 255),
                                  Canvas::Rgb(140, 255, 215)};

static const char* const kTabNames[16] = {"hum",   "dyn eq", "comp",     "clarity", "saturate", "de-harsh", "safety",   "visual",
                                          "input", "health", "takeback", "width",   "loudness", "config",   "multiband", "pid"};
static const char* const kTabTitles[16] = {"hum + hiss",    "dynamic eq", "compressor",           "clarity + bass", "saturation",
                                           "de-harsh",      "ear + speaker safety", "visualizer",   "input auto-level",
                                           "health checks", "takeback",   "stereo width",         "quiet loudness", "configs",
                                           "multiband dynamics", "pid auto-adjust"};

constexpr int kGx = 3, kGy = 22, kGw = 314, kGh = 136; // graph area on every page
constexpr int kPanelY = 160;                          // grey panel: strip at 164, knob boxes at 186
} // namespace ui
} // namespace pg
