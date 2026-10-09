#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#if (MSVC)
#include "ipps.h"
#endif

class PluginProcessor : public juce::AudioProcessor {
public:
    PluginProcessor();

    ~PluginProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;

    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

    juce::AudioProcessorEditor *createEditor() override;

    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;

    bool producesMidi() const override;

    bool isMidiEffect() const override;

    double getTailLengthSeconds() const override;

    int getNumPrograms() override;

    int getCurrentProgram() override;

    void setCurrentProgram(int index) override;

    const juce::String getProgramName(int index) override;

    void changeProgramName(int index, const juce::String &newName) override;

    void getStateInformation(juce::MemoryBlock &destData) override;

    void setStateInformation(const void *data, int sizeInBytes) override;

    // Center pitch as a MIDI note number, in 1-semitone steps over the piano
    // register: A0 (21) to C8 (108). Linear in semitones is logarithmic in
    // frequency.
    static constexpr float minCenterNote = 21.0f;
    static constexpr float maxCenterNote = 108.0f;
    static constexpr float centerNoteStep = 1.0f;
    static constexpr float defaultCenterNote = 57.0f; // A3, 220 Hz

    static double noteToFrequency(double note) {
        return 440.0 * std::exp2((note - 69.0) / 12.0);
    }

    // Max L/R frequency difference, as a percent of the center frequency
    static constexpr float maxDeltaPercent = 10.0f;
    static constexpr float defaultDeltaPercent = 1.0f;

    static inline const juce::String centerNoteId = "centerNote";
    static inline const juce::String deltaPercentId = "deltaPercent";

    juce::AudioProcessorValueTreeState parameters;

    // A received MIDI message, copied into fixed-size storage so the audio thread
    // never allocates. Messages longer than 3 bytes (SysEx) only keep their
    // length.
    struct MidiLogEntry {
        std::array<juce::uint8, 3> bytes{};
        int size = 0;
        double timeSeconds = 0.0; // since playback started
    };

    // Message thread: moves up to maxEntries pending entries into dest, oldest
    // first. Returns the count.
    int popMidiLogEntries(MidiLogEntry *dest, int maxEntries);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    static constexpr float sineAmplitude = 0.2f;
    static constexpr double smoothingTimeSeconds = 0.05;

    std::atomic<float> *centerNoteParam = nullptr;
    std::atomic<float> *deltaPercentParam = nullptr;

    // Center frequency is smoothed multiplicatively (constant rate in pitch),
    // delta percent linearly
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> centerFrequencySmoothed;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear> deltaPercentSmoothed;

    double currentSampleRate = 44100.0;

    // Audio thread -> message thread. If nobody reads it (editor closed) it fills
    // up and new messages are dropped.
    static constexpr int midiLogCapacity = 256;
    juce::AbstractFifo midiLogFifo{midiLogCapacity};
    std::array<MidiLogEntry, midiLogCapacity> midiLogBuffer{};
    juce::int64 samplesProcessed = 0;

    double phaseL = 0.0;
    double phaseR = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
