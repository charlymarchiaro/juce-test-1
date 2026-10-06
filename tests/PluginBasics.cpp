#include "helpers/test_helpers.h"
#include <PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_approx.hpp>

TEST_CASE ("one is equal to one", "[dummy]")
{
    REQUIRE (1 == 1);
}

TEST_CASE ("Plugin instance", "[instance]")
{
    PluginProcessor testPlugin;

    SECTION ("name")
    {
        CHECK_THAT (testPlugin.getName().toStdString(),
            Catch::Matchers::Equals ("Pamplejuce Demo"));
    }

    SECTION ("program name")
    {
        // Steinberg's VST3 validator fails plugins whose programs have no name
        CHECK (testPlugin.getProgramName (0).isNotEmpty());
    }
}


#ifdef PAMPLEJUCE_IPP
    #include <ipp.h>

TEST_CASE ("IPP version", "[ipp]")
{
    #if defined(__APPLE__)
        // macOS uses 2021.9.1 from pip wheel (only x86_64 version available)
        CHECK_THAT (ippsGetLibVersion()->Version, Catch::Matchers::Equals ("2021.9.1 (r0x7e208212)"));
    #else
        CHECK_THAT (ippsGetLibVersion()->Version, Catch::Matchers::Equals ("2026.0.0 (r0xa7ad6ebc)"));
    #endif
}
#endif

TEST_CASE ("Center note moves in semitone steps", "[parameters]")
{
    PluginProcessor testPlugin;
    auto* param = testPlugin.parameters.getParameter (PluginProcessor::centerNoteId);
    REQUIRE (param != nullptr);

    for (auto normalised : { 0.0f, 0.1234f, 0.5f, 0.777f, 1.0f })
    {
        param->setValueNotifyingHost (normalised);
        const auto note = testPlugin.parameters.getRawParameterValue (PluginProcessor::centerNoteId)->load();
        CHECK (note == std::round (note));
    }

    CHECK (PluginProcessor::noteToFrequency (PluginProcessor::minCenterNote) == Catch::Approx (27.5));
    CHECK (PluginProcessor::noteToFrequency (PluginProcessor::defaultCenterNote) == Catch::Approx (220.0));
    CHECK (PluginProcessor::noteToFrequency (PluginProcessor::maxCenterNote) == Catch::Approx (4186.01).epsilon (1.0e-5));
}

TEST_CASE ("MIDI input is logged", "[midi]")
{
    PluginProcessor testPlugin;
    CHECK (testPlugin.acceptsMidi());

    testPlugin.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    midi.addEvent (juce::MidiMessage::noteOff (1, 60), 480);
    testPlugin.processBlock (buffer, midi);

    std::array<PluginProcessor::MidiLogEntry, 8> entries;
    REQUIRE (testPlugin.popMidiLogEntries (entries.data(), (int) entries.size()) == 2);

    CHECK (juce::MidiMessage (entries[0].bytes.data(), entries[0].size).isNoteOn());
    CHECK (juce::MidiMessage (entries[1].bytes.data(), entries[1].size).isNoteOff());
    CHECK (entries[1].timeSeconds == Catch::Approx (0.01));

    CHECK (testPlugin.popMidiLogEntries (entries.data(), (int) entries.size()) == 0);
}
