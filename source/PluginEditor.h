#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"
#include "melatonin_inspector/melatonin_inspector.h"

//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    static juce::String describe(const PluginProcessor::MidiLogEntry& entry);

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PluginProcessor& processorRef;
    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton{"Inspect the UI"};

    juce::Slider centerNoteSlider;
    juce::Slider deltaPercentSlider;
    juce::Label centerNoteLabel;
    juce::Label deltaPercentLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> centerNoteAttachment;
    std::unique_ptr<SliderAttachment> deltaPercentAttachment;

    // Shows the last numMidiLogLines received MIDI messages, newest at the bottom
    static constexpr int numMidiLogLines = 100;
    juce::Label midiLogLabel;
    juce::TextEditor midiLogText;
    std::deque<juce::String> midiLogLines;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
