#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor(
          BusesProperties()
#if !JucePlugin_IsMidiEffect
    #if !JucePlugin_IsSynth
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
    #endif
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
      ),
      parameters(*this, nullptr, "Parameters", createParameterLayout())
{
    centerNoteParam = parameters.getRawParameterValue(centerNoteId);
    deltaPercentParam = parameters.getRawParameterValue(deltaPercentId);
}

PluginProcessor::~PluginProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { centerNoteId, 1 },
            "Center Note",
            juce::NormalisableRange<float>(minCenterNote, maxCenterNote, centerNoteStep),
            defaultCenterNote,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction([](float note, int) {
                    return juce::MidiMessage::getMidiNoteName(juce::roundToInt(note), true, true, 4)
                           + " (" + juce::String(noteToFrequency(note), 1) + " Hz)";
                })
                .withValueFromStringFunction([](const juce::String& text) {
                    // Accepts a frequency in Hz and picks the nearest note
                    const auto hz = juce::jmax(1.0, text.getDoubleValue());
                    return static_cast<float>(std::round(69.0 + 12.0 * std::log2(hz / 440.0)));
                })
        )
    );

    layout.add(
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { deltaPercentId, 1 },
            "Delta",
            juce::NormalisableRange<float>(0.0f, maxDeltaPercent),
            defaultDeltaPercent,
            juce::AudioParameterFloatAttributes().withLabel("%")
        )
    );

    return layout;
}

//==============================================================================
const juce::String PluginProcessor::getName() const { return JucePlugin_Name; }

bool PluginProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool PluginProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool PluginProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double PluginProcessor::getTailLengthSeconds() const { return 0.0; }

int PluginProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them
    // there are 0 programs,
    // so this should be at least 1, even if you're not really
    // implementing programs.
}

int PluginProcessor::getCurrentProgram() { return 0; }

void PluginProcessor::setCurrentProgram(int index) { juce::ignoreUnused(index); }

const juce::String PluginProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    // Steinberg's VST3 validator fails plugins whose single default
    // program has no name
    return "Default";
}

void PluginProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = sampleRate;
    phaseL = 0.0;
    phaseR = 0.0;
    samplesProcessed = 0;

    centerFrequencySmoothed.reset(sampleRate, smoothingTimeSeconds);
    centerFrequencySmoothed.setCurrentAndTargetValue(noteToFrequency(centerNoteParam->load()));

    deltaPercentSmoothed.reset(sampleRate, smoothingTimeSeconds);
    deltaPercentSmoothed.setCurrentAndTargetValue(deltaPercentParam->load());
}

void PluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free
    // up any spare memory, etc.
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
    #if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    #endif

    return true;
#endif
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    for (const auto metadata : midiMessages)
    {
        auto scope = midiLogFifo.write(1);
        if (scope.blockSize1 + scope.blockSize2 == 0)
            break; // FIFO full, drop

        auto& entry = scope.blockSize1 > 0 ? midiLogBuffer[(size_t) scope.startIndex1]
                                           : midiLogBuffer[(size_t) scope.startIndex2];
        entry.size = metadata.numBytes;
        std::copy_n(
            metadata.data,
            juce::jmin(metadata.numBytes, (int) entry.bytes.size()),
            entry.bytes.begin()
        );
        entry.timeSeconds = (double) (samplesProcessed + metadata.samplePosition)
                            / currentSampleRate;
    }

    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto numSamples = buffer.getNumSamples();

    centerFrequencySmoothed.setTargetValue(noteToFrequency(centerNoteParam->load()));
    deltaPercentSmoothed.setTargetValue(deltaPercentParam->load());

    const auto twoPiOverSampleRate = juce::MathConstants<double>::twoPi / currentSampleRate;

    // Sine at centerFrequency - delta/2 on even (L) channels,
    // centerFrequency + delta/2 on odd (R) channels, replacing any
    // input
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto centerFrequency = centerFrequencySmoothed.getNextValue();
        const auto deltaFrequency = centerFrequency * deltaPercentSmoothed.getNextValue() / 100.0;
        const auto phaseIncrementL = twoPiOverSampleRate * (centerFrequency - 0.5 * deltaFrequency);
        const auto phaseIncrementR = twoPiOverSampleRate * (centerFrequency + 0.5 * deltaFrequency);

        auto valueL = sineAmplitude * static_cast<float>(std::sin(phaseL));
        auto valueR = sineAmplitude * static_cast<float>(std::sin(phaseR));

        for (int channel = 0; channel < totalNumOutputChannels; ++channel)
        {
            if (channel % 2 == 0)
            {
                buffer.setSample(channel, sample, valueL);
            }
            else
            {
                buffer.setSample(channel, sample, valueR);
            }
        }

        phaseL += phaseIncrementL;
        if (phaseL >= juce::MathConstants<double>::twoPi)
            phaseL -= juce::MathConstants<double>::twoPi;

        phaseR += phaseIncrementR;
        if (phaseR >= juce::MathConstants<double>::twoPi)
            phaseR -= juce::MathConstants<double>::twoPi;
    }

    samplesProcessed += numSamples;
}

int PluginProcessor::popMidiLogEntries(MidiLogEntry* dest, int maxEntries)
{
    auto scope = midiLogFifo.read(juce::jmin(maxEntries, midiLogFifo.getNumReady()));

    for (int i = 0; i < scope.blockSize1; ++i)
        dest[i] = midiLogBuffer[(size_t) (scope.startIndex1 + i)];

    for (int i = 0; i < scope.blockSize2; ++i)
        dest[scope.blockSize1 + i] = midiLogBuffer[(size_t) (scope.startIndex2 + i)];

    return scope.blockSize1 + scope.blockSize2;
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an
    // editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor() { return new PluginEditor(*this); }

//==============================================================================
void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PluginProcessor(); }
