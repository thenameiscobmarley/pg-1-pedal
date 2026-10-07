#pragma once
#include "PluginProcessor.h"
#include "PedalView.h"

class PG1Editor : public juce::AudioProcessorEditor
{
public:
    explicit PG1Editor (PG1Processor&);
    void resized() override { view.setBounds (getLocalBounds()); }

private:
    PedalView view;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PG1Editor)
};
