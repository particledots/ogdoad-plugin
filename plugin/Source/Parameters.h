// The list of controls Logic can see and automate.
//
// The short ids below are what Logic stores in saved projects and automation. They were chosen once, before the
// first build, and are never renamed. Milestone 2 (0.2.0) added exOn, trigUse1..trigUse4 and trigMode after them;
// those are fixed from now on too.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace OgdoadParams
{
    inline constexpr const char* rate       = "rate";        // the big knob, all eight together
    inline constexpr const char* modAmount  = "modAmount";   // Mod (attenuverter), -1..+1
    inline constexpr const char* modSource  = "modSource";   // menu: Off, LFO 1..8, Mod wheel
    inline constexpr const char* hold       = "hold";        // freeze all eight
    inline constexpr const char* holdPedal  = "holdPedal";   // also freeze while the sustain pedal is down
    inline constexpr const char* drift      = "drift";       // menu: Off, Low, More
    inline constexpr const char* outChannel = "outChannel";  // MIDI channel the CC goes out on, 1..16

    // Ogdoad Ex (milestone 2)
    inline constexpr const char* exOn       = "exOn";        // send the sixteen Ex outputs
    inline constexpr const char* trigUse[4] = { "trigUse1", "trigUse2", "trigUse3", "trigUse4" };  // trig 2, 4, 6, 8 "in use"
    inline constexpr const char* trigMode   = "trigMode";    // menu: CC pulse, Note, Both

    inline const juce::StringArray modSourceNames { "Off", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "LFO 6", "LFO 7", "LFO 8", "Mod wheel" };
    inline const juce::StringArray driftNames { "Off", "Low", "More" };
    inline const juce::StringArray trigModeNames { "CC pulse", "Note", "Both" };

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // "4.9 Hz", "0.72 Hz", "2.6 s", "3.1 min", for a frequency in Hz (0 = "held").
    juce::String speedText (double hz);
}
