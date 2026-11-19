// What Logic sees: a MIDI effect (AU type aumi) with no audio. Incoming MIDI passes through untouched; the eight
// LFOs and the Ogdoad Ex outputs are added as smooth 14-bit CC on the chosen channel (16 by default), the Ex triggers
// as CC pulses and/or short notes. The incoming mod wheel (CC 1) and sustain pedal (CC 64) are also read, for the
// Mod source menu and "Pedal holds".

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ogdoad/Engine.h"
#include "Parameters.h"

#include <array>
#include <atomic>
#include <vector>

class OgdoadProcessor : public juce::AudioProcessor
{
public:
    OgdoadProcessor();
    ~OgdoadProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;   // MIDI passes; a trigger still on is ended

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Ogdoad"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }

    // For the panel: the latest state of each LFO (written by the audio thread after every block).
    struct LaneView { std::atomic<float> value { 0.0f }, hz { 0.0f }, phase { 0.0f }; };
    std::array<LaneView, ogdoad::kNumLanes> lanes;
    std::atomic<bool> held { false };
    std::atomic<int> blocksDone { 0 };

    // For the panel: Ogdoad Ex.
    std::array<std::atomic<float>, ogdoad::kNumExSmooth> exValues {};
    std::array<std::atomic<bool>, ogdoad::kNumTrigs> trigOn {};
    std::array<std::atomic<unsigned>, ogdoad::kNumTrigs> trigPulses {};

private:
    struct Sink : ogdoad::CcSink
    {
        juce::MidiBuffer* out = nullptr;
        int channel = 16;
        // A trigger pulse or note ends on the channel it started on, even if the channel was changed in between.
        std::array<int, 128> pulseChannel {}, noteChannel {};
        void cc (int offset, int number, int value) override
        {
            int ch = channel;
            if (number >= 102 && number <= 105)
            {
                if (value > 0) pulseChannel[(size_t) number] = channel;
                else if (pulseChannel[(size_t) number] > 0) ch = pulseChannel[(size_t) number];
            }
            out->addEvent (juce::MidiMessage::controllerEvent (ch, number, value), offset);
        }
        void note (int offset, int number, int velocity) override
        {
            if (velocity > 0)
            {
                noteChannel[(size_t) number] = channel;
                out->addEvent (juce::MidiMessage::noteOn (channel, number, (juce::uint8) velocity), offset);
            }
            else
                out->addEvent (juce::MidiMessage::noteOff (noteChannel[(size_t) number] > 0 ? noteChannel[(size_t) number] : channel, number), offset);
        }
    };
    void publishToPanel();

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float>* rateParam = nullptr;
    std::atomic<float>* modAmountParam = nullptr;
    std::atomic<float>* modSourceParam = nullptr;
    std::atomic<float>* holdParam = nullptr;
    std::atomic<float>* holdPedalParam = nullptr;
    std::atomic<float>* driftParam = nullptr;
    std::atomic<float>* outChannelParam = nullptr;
    std::atomic<float>* exOnParam = nullptr;
    std::array<std::atomic<float>*, ogdoad::kNumTrigs> trigUseParam {};
    std::atomic<float>* trigModeParam = nullptr;

    ogdoad::Engine engine;
    std::vector<ogdoad::InputEvent> inputs;
    juce::MidiBuffer outBuffer;
    Sink sink;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OgdoadProcessor)
};
