#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "VegaUI.h"

class ConsoleStripEditor : public juce::AudioProcessorEditor
{
public:
    explicit ConsoleStripEditor(ConsoleStripProcessor&);
    ~ConsoleStripEditor() override;
    void resized() override;
    void paint(juce::Graphics& g) override { g.fillAll(VegaColours::background); }

    static constexpr int kBaseW = 1580, kBaseH = 480;
private:
    class Content;
    VegaLookAndFeel lnf;
    std::unique_ptr<Content> content;
    juce::ComponentBoundsConstrainer constrainer;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ConsoleStripEditor)
};
