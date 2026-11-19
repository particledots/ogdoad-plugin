#include "Parameters.h"
#include "ogdoad/Rates.h"

namespace OgdoadParams
{
    juce::String speedText (double hz)
    {
        if (hz <= 0.0) return "held";
        if (hz >= 10.0) return juce::String (juce::roundToInt (hz)) + " Hz";
        if (hz >= 1.0)  return juce::String (hz, 1) + " Hz";
        if (hz >= 0.5)  return juce::String (hz, 2) + " Hz";
        const double s = 1.0 / hz;
        if (s < 10.0)  return juce::String (s, 1) + " s";
        if (s < 60.0)  return juce::String (juce::roundToInt (s)) + " s";
        return juce::String (s / 60.0, 1) + " min";
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;
        const int v = 1;

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { rate, v }, "Rate", NormalisableRange<float> (0.0f, 1.0f), 0.5f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int)
                { return "LFO 1 " + speedText (ogdoad::laneFrequency (0, x)); })));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { modAmount, v }, "Mod", NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int)
                { const int p = roundToInt (x * 100.0f); return (p > 0 ? "+" : "") + String (p) + "%"; })));

        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { modSource, v }, "Mod source", modSourceNames, 8));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { hold, v }, "Hold", false));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { holdPedal, v }, "Pedal holds", false));
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { drift, v }, "Drift", driftNames, 1));
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { outChannel, v }, "MIDI out channel", 1, 16, 16));
        return layout;
    }
}
