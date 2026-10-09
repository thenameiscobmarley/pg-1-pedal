#include "PluginEditor.h"

PG1Editor::PG1Editor (PG1Processor& p)
    : AudioProcessorEditor (p), view (p.core())
{
    view.power = &p.powered;
    addAndMakeVisible (view);
    setResizable (true, true);
    setResizeLimits (520, 420, 2400, 1800);
    setSize (980, 760);
}
