#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TearedTuneAudioProcessor::createLayout()
{
    using FP = juce::AudioParameterFloat;
    using NR = juce::NormalisableRange<float>;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<FP> (juce::ParameterID { pid::retune, 1 },    "Retune Speed (ms)", NR (0.0f, 400.0f, 0.0f, 0.5f), 20.0f));
    layout.add (std::make_unique<FP> (juce::ParameterID { pid::amount, 1 },    "Amount (%)",        NR (0.0f, 100.0f, 0.0f, 1.0f), 100.0f));
    layout.add (std::make_unique<FP> (juce::ParameterID { pid::vibrato, 1 },   "Keep Vibrato (%)",  NR (0.0f, 100.0f, 0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<FP> (juce::ParameterID { pid::flex, 1 },      "Flex (cents)",      NR (0.0f, 50.0f, 0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<FP> (juce::ParameterID { pid::transpose, 1 }, "Transpose (st)",    NR (-12.0f, 12.0f, 1.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<FP> (juce::ParameterID { pid::detune, 1 },    "Detune (cents)",    NR (-50.0f, 50.0f, 0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<FP> (juce::ParameterID { pid::refa, 1 },      "Reference A (Hz)",  NR (430.0f, 450.0f, 0.1f, 1.0f), 440.0f));
    layout.add (std::make_unique<FP> (juce::ParameterID { pid::mix, 1 },       "Mix (%)",           NR (0.0f, 100.0f, 0.0f, 1.0f), 100.0f));
    layout.add (std::make_unique<FP> (juce::ParameterID { pid::ingain, 1 },    "Input (dB)",        NR (-12.0f, 12.0f, 0.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<FP> (juce::ParameterID { pid::outgain, 1 },   "Output (dB)",       NR (-12.0f, 12.0f, 0.0f, 1.0f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { pid::range, 1 }, "Voice Range",
                juce::StringArray { kRangeNames[0], kRangeNames[1], kRangeNames[2], kRangeNames[3] }, 1));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { pid::key, 1 }, "Key",
                juce::StringArray { kNoteNames[0], kNoteNames[1], kNoteNames[2], kNoteNames[3], kNoteNames[4], kNoteNames[5],
                                    kNoteNames[6], kNoteNames[7], kNoteNames[8], kNoteNames[9], kNoteNames[10], kNoteNames[11] }, 0));

    juce::StringArray scales;
    for (auto* s : kScaleNames) scales.add (s);
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { pid::scale, 1 }, "Scale", scales, 0));

    for (int i = 0; i < 12; ++i)
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { kNoteIds[i], 1 }, juce::String ("Note ") + kNoteNames[i], true));

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { pid::bypass, 1 }, "Bypass", false));
    return layout;
}

TearedTuneAudioProcessor::TearedTuneAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TearedTuneState", createLayout())
{
    for (auto& a : histDet) a.store (-1.0f);
    for (auto& a : histTgt) a.store (-1.0f);
}

//==============================================================================
void TearedTuneAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRateHz = sampleRate;
    engine.prepare (sampleRate);
    setLatencySamples (engine.latencySamples());
    telemCounter = 0;
}

bool TearedTuneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

uint16_t TearedTuneAudioProcessor::getEffectiveMask() const
{
    const int scale = (int) val (pid::scale);
    if (scale != kCustomScale)
        return tt::scaleMask (scale, (int) val (pid::key));
    uint16_t m = 0;
    for (int i = 0; i < 12; ++i)
        if (val (kNoteIds[i]) > 0.5f) m = (uint16_t) (m | (1 << i));
    return m;
}

tt::Params TearedTuneAudioProcessor::readParams() const
{
    tt::Params p;
    p.retuneMs   = val (pid::retune);
    p.amount     = val (pid::amount) * 0.01f;
    p.vibrato    = val (pid::vibrato) * 0.01f;
    p.toleranceC = val (pid::flex);
    p.transpose  = val (pid::transpose);
    p.detuneC    = val (pid::detune);
    p.refA       = val (pid::refa);
    p.mix        = val (pid::mix) * 0.01f;
    p.inGain     = juce::Decibels::decibelsToGain (val (pid::ingain));
    p.outGain    = juce::Decibels::decibelsToGain (val (pid::outgain));
    p.range      = (int) val (pid::range);
    p.mask       = getEffectiveMask();
    p.bypass     = val (pid::bypass) > 0.5f;
    return p;
}

void TearedTuneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nc = juce::jmin (buffer.getNumChannels(), 2);
    for (int c = nc; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
    if (nc < 1 || n <= 0) return;

    const tt::Params p = readParams();
    float* ch[2] = { buffer.getWritePointer (0), nc > 1 ? buffer.getWritePointer (1) : nullptr };
    engine.process (ch, nc, n, p);

    // telemetry for the editor (one point roughly every 16 ms)
    uiIn.store (engine.peakIn);
    uiOut.store (engine.peakOut);
    telemCounter += n;
    const int step = juce::jmax (1, (int) (sampleRateHz * 0.016));
    while (telemCounter >= step)
    {
        telemCounter -= step;
        const int i = histIdx.load();
        histDet[(size_t) (i % kHist)].store (engine.telemVoiced ? engine.telemDetected : -1.0f);
        histTgt[(size_t) (i % kHist)].store (engine.telemVoiced ? engine.telemTarget : -1.0f);
        histIdx.store (i + 1);
    }
}

//==============================================================================
void TearedTuneAudioProcessor::setParamValue (const char* id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
}

void TearedTuneAudioProcessor::toggleNote (int pc)
{
    const uint16_t eff = getEffectiveMask();
    for (int i = 0; i < 12; ++i)
        setParamValue (kNoteIds[i], (eff & (1 << i)) ? 1.0f : 0.0f);
    setParamValue (pid::scale, (float) kCustomScale);
    const bool wasOn = (eff & (1 << pc)) != 0;
    setParamValue (kNoteIds[pc], wasOn ? 0.0f : 1.0f);
}

juce::AudioProcessorEditor* TearedTuneAudioProcessor::createEditor() { return new TearedTuneEditor (*this); }

void TearedTuneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TearedTuneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TearedTuneAudioProcessor(); }
