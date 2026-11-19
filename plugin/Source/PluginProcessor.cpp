#include "PluginProcessor.h"
#include "PluginEditor.h"

OgdoadProcessor::OgdoadProcessor()
    : AudioProcessor (BusesProperties()),                       // a MIDI effect: no audio buses
      apvts (*this, nullptr, "Ogdoad", OgdoadParams::createLayout())
{
    rateParam       = apvts.getRawParameterValue (OgdoadParams::rate);
    modAmountParam  = apvts.getRawParameterValue (OgdoadParams::modAmount);
    modSourceParam  = apvts.getRawParameterValue (OgdoadParams::modSource);
    holdParam       = apvts.getRawParameterValue (OgdoadParams::hold);
    holdPedalParam  = apvts.getRawParameterValue (OgdoadParams::holdPedal);
    driftParam      = apvts.getRawParameterValue (OgdoadParams::drift);
    outChannelParam = apvts.getRawParameterValue (OgdoadParams::outChannel);
    exOnParam       = apvts.getRawParameterValue (OgdoadParams::exOn);
    for (int k = 0; k < ogdoad::kNumTrigs; ++k)
        trigUseParam[(size_t) k] = apvts.getRawParameterValue (OgdoadParams::trigUse[k]);
    trigModeParam   = apvts.getRawParameterValue (OgdoadParams::trigMode);
    inputs.reserve (4096);
    outBuffer.ensureSize (65536);
    engine.prepare (48000.0);
}

void OgdoadProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    outBuffer.ensureSize (65536);
}

bool OgdoadProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet().isDisabled() && layouts.getMainInputChannelSet().isDisabled();
}

void OgdoadProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    ogdoad::Settings s;
    s.rate      = rateParam->load();
    s.modAmount = modAmountParam->load();
    s.modSource = juce::roundToInt (modSourceParam->load());
    s.hold      = holdParam->load() >= 0.5f;
    s.holdPedal = holdPedalParam->load() >= 0.5f;
    s.drift     = juce::roundToInt (driftParam->load());
    s.exOn      = exOnParam->load() >= 0.5f;
    for (int k = 0; k < ogdoad::kNumTrigs; ++k)
        s.trigUse[k] = trigUseParam[(size_t) k]->load() >= 0.5f;
    s.trigMode  = juce::roundToInt (trigModeParam->load());
    engine.setSettings (s);

    // Everything that comes in goes out unchanged; the mod wheel and the sustain pedal are also read.
    inputs.clear();
    outBuffer.clear();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int offset = juce::jlimit (0, numSamples - 1, meta.samplePosition);
        outBuffer.addEvent (m, offset);
        if (m.isController() && inputs.size() < inputs.capacity())
        {
            if (m.getControllerNumber() == 1)
                inputs.push_back ({ offset, ogdoad::InputEvent::modWheel, m.getControllerValue() / 127.0 });
            else if (m.getControllerNumber() == 64)
                inputs.push_back ({ offset, ogdoad::InputEvent::pedal, m.getControllerValue() >= 64 ? 1.0 : 0.0 });
        }
    }

    sink.out = &outBuffer;
    sink.channel = juce::jlimit (1, 16, juce::roundToInt (outChannelParam->load()));
    engine.render (numSamples, inputs.data(), (int) inputs.size(), sink);
    midi.swapWith (outBuffer);
    publishToPanel();
}

void OgdoadProcessor::processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer& midi)
{
    // Bypassed: incoming MIDI passes untouched and nothing new is added, except the end of a trigger that was on.
    sink.out = &midi;
    engine.closeTriggers (0, sink);
}

void OgdoadProcessor::publishToPanel()
{
    for (int i = 0; i < ogdoad::kNumLanes; ++i)
    {
        lanes[(size_t) i].value.store ((float) engine.laneValue (i));
        lanes[(size_t) i].hz.store ((float) engine.laneHz (i));
        lanes[(size_t) i].phase.store ((float) engine.lanePhase (i));
    }
    for (int i = 0; i < ogdoad::kNumExSmooth; ++i)
        exValues[(size_t) i].store ((float) engine.exValue (i));
    for (int k = 0; k < ogdoad::kNumTrigs; ++k)
    {
        trigOn[(size_t) k].store (engine.trigOn (k));
        trigPulses[(size_t) k].store ((unsigned) engine.trigPulses (k));
    }
    held.store (engine.isHeld());
    blocksDone.fetch_add (1);
}

void OgdoadProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void OgdoadProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* OgdoadProcessor::createEditor()
{
    return new OgdoadEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OgdoadProcessor();
}
