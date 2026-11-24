#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <vector>
#include "PluginProcessor.h"

namespace tui
{
    const juce::Colour kBgTop   { 0xff0a1628 };
    const juce::Colour kBgBot   { 0xff050912 };
    const juce::Colour kPanel   { 0xff0f1c33 };
    const juce::Colour kPanel2  { 0xff14243f };
    const juce::Colour kBorder  { 0xff203a5e };
    const juce::Colour kCyan    { 0xff3ad0ff };
    const juce::Colour kPink    { 0xffff5fa8 };
    const juce::Colour kText    { 0xffe8f4ff };
    const juce::Colour kDim     { 0xff7f9bbd };
    const juce::Colour kTrack   { 0xff1f3350 };

    inline juce::Font mkFont (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }
    inline juce::String noteName (int midi)
    {
        return juce::String (kNoteNames[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
    }
}

//==============================================================================
class TearedLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TearedLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
};

//==============================================================================
// Scrolling pitch graph: detected pitch (cyan), corrected pitch (pink), allowed notes as grid lines
class PitchGraph : public juce::Component, private juce::Timer
{
public:
    explicit PitchGraph (TearedTuneAudioProcessor& p) : proc (p) { startTimerHz (30); }
    ~PitchGraph() override { stopTimer(); }
    void paint (juce::Graphics&) override;
private:
    void timerCallback() override { repaint(); }
    TearedTuneAudioProcessor& proc;
    float viewCenter = 0.0f;
};

// 12-note keyboard: click to allow / forbid a pitch class
class NoteKeys : public juce::Component, private juce::Timer
{
public:
    explicit NoteKeys (TearedTuneAudioProcessor& p) : proc (p) { startTimerHz (20); }
    ~NoteKeys() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    void timerCallback() override { repaint(); }
    int keyAt (juce::Point<float>) const;
    juce::Rectangle<float> keyRect (int pc) const;
    TearedTuneAudioProcessor& proc;
};

//==============================================================================
class TearedTuneEditor : public juce::AudioProcessorEditor
{
public:
    explicit TearedTuneEditor (TearedTuneAudioProcessor&);
    ~TearedTuneEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    struct Panel
    {
        juce::String title;
        juce::Rectangle<int> bounds;
        std::vector<Knob*> knobs;
        int cols = 0;
    };

    Knob* addKnob (const char* id, const juce::String& name, const juce::String& suffix, int decimals, bool bipolar = false);
    void addCombo (juce::ComboBox& box, const juce::StringArray& items, const char* id,
                   std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att);
    void addCaption (juce::Label& l, const juce::String& text);
    void applyPreset (float retune, float vibrato, float flex);

    TearedTuneAudioProcessor& proc;
    TearedLookAndFeel laf;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<Panel> panels;

    PitchGraph graph;
    NoteKeys keys;

    juce::ComboBox keyBox, scaleBox, rangeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt, rangeAtt;
    juce::Label keyCap, scaleCap, rangeCap, keysCap;

    juce::TextButton naturalBtn { "NATURAL" }, classicBtn { "CLASSIC" }, hardBtn { "HARD TUNE" };
    juce::ToggleButton bypassBtn;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TearedTuneEditor)
};
