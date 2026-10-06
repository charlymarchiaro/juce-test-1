#include "PluginEditor.h"

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& text, const juce::String& suffix)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 120, 20);
        slider.setTextValueSuffix(suffix);
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.attachToComponent(&slider, true);
        addAndMakeVisible(label);
    };

    setupSlider(centerNoteSlider, centerNoteLabel, "Center", {});
    setupSlider(deltaPercentSlider, deltaPercentLabel, "Delta", " %");

    // The attachment copies the parameter's range to the slider, i.e. it does the equivalent of
    // setRange(minCenterNote, maxCenterNote, centerNoteStep) — one-semitone steps from A0 to C8
    centerNoteAttachment = std::make_unique<SliderAttachment>(processorRef.parameters, PluginProcessor::centerNoteId, centerNoteSlider);
    deltaPercentAttachment = std::make_unique<SliderAttachment>(processorRef.parameters, PluginProcessor::deltaPercentId, deltaPercentSlider);

    deltaPercentSlider.setNumDecimalPlacesToDisplay(2);

    midiLogLabel.setText("Last " + juce::String(numMidiLogLines) + " MIDI input messages:", juce::dontSendNotification);
    addAndMakeVisible(midiLogLabel);

    midiLogText.setMultiLine(true);
    midiLogText.setReadOnly(true);
    midiLogText.setCaretVisible(false);
    midiLogText.setFont(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 10.0f, juce::Font::plain));
    addAndMakeVisible(midiLogText);

    addAndMakeVisible(inspectButton);

    // this chunk of code instantiates and opens the melatonin inspector
    inspectButton.onClick = [&]
    {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector>(*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible(true);
    };

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize(500, 560);

    startTimerHz(30);
}

PluginEditor::~PluginEditor()
{
}

void PluginEditor::timerCallback()
{
    std::array<PluginProcessor::MidiLogEntry, 64> entries;
    bool changed = false;

    while (const auto numRead = processorRef.popMidiLogEntries(entries.data(), (int) entries.size()))
    {
        for (int i = 0; i < numRead; ++i)
            midiLogLines.push_back(describe(entries[(size_t) i]));

        changed = true;
    }

    if (! changed)
        return;

    while (midiLogLines.size() > (size_t) numMidiLogLines)
        midiLogLines.pop_front();

    juce::StringArray lines;
    for (const auto& line : midiLogLines)
        lines.add(line);

    midiLogText.setText(lines.joinIntoString("\n"), juce::dontSendNotification);
}

juce::String PluginEditor::describe(const PluginProcessor::MidiLogEntry& entry)
{
    const auto time = juce::String(entry.timeSeconds, 3).paddedLeft(' ', 9) + " s  ";

    if (entry.size > (int) entry.bytes.size())
        return time + "SysEx (" + juce::String(entry.size) + " bytes)";

    return time + juce::MidiMessage(entry.bytes.data(), entry.size).getDescription();
}

void PluginEditor::paint(juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    auto area = getLocalBounds();
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    auto helloWorld = juce::String("Hello from ") + PRODUCT_NAME_WITHOUT_VERSION + " v" VERSION + " running in " +
        CMAKE_BUILD_TYPE;
    g.drawText(helloWorld, area.removeFromTop(80), juce::Justification::centred, false);
}

void PluginEditor::resized()
{
    // layout the positions of your child components here
    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(60);

    area.removeFromLeft(60); // room for the attached labels
    centerNoteSlider.setBounds(area.removeFromTop(40));
    area.removeFromTop(10);
    deltaPercentSlider.setBounds(area.removeFromTop(40));

    area.removeFromTop(10);
    inspectButton.setBounds(area.removeFromTop(40).withSizeKeepingCentre(100, 40).translated(-30, 0));

    area = getLocalBounds().reduced(20).withTop(area.getY() + 10);
    midiLogLabel.setBounds(area.removeFromTop(24));
    midiLogText.setBounds(area);
}
