#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include "Parameters.h"
#include "PitchCorrector.h"

class TearedTuneAudioProcessor : public juce::AudioProcessor
{
public:
    TearedTuneAudioProcessor();
    ~TearedTuneAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TearedTune V3"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // helpers used by the editor
    uint16_t getEffectiveMask() const;          // note mask after applying key + scale (or the custom notes)
    void toggleNote (int pitchClass);           // switches to "Custom" and flips one note
    void setParamValue (const char* id, float plainValue);

    juce::AudioProcessorValueTreeState apvts;

    // live telemetry (written on the audio thread, read by the editor)
    static constexpr int kHist = 256;
    std::array<std::atomic<float>, kHist> histDet, histTgt;
    std::atomic<int> histIdx { 0 };
    std::atomic<float> uiIn { 0.0f }, uiOut { 0.0f };

private:
    tt::Params readParams() const;
    float val (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    tt::PitchCorrector engine;
    double sampleRateHz = 44100.0;
    int telemCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TearedTuneAudioProcessor)
};
