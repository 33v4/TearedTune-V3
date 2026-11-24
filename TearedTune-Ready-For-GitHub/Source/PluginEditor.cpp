#include "PluginEditor.h"
#include <cmath>

using namespace tui;

//==============================================================================
// Look and feel
//==============================================================================
TearedLookAndFeel::TearedLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, kPanel2);
    setColour (juce::ComboBox::textColourId, kText);
    setColour (juce::ComboBox::outlineColourId, kBorder);
    setColour (juce::ComboBox::arrowColourId, kCyan);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff0b1527));
    setColour (juce::PopupMenu::textColourId, kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff1c6f93));
    setColour (juce::PopupMenu::highlightedTextColourId, kText);
    setColour (juce::Slider::textBoxTextColourId, kText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, juce::Colour (0xff1c6f93));
    setColour (juce::Label::textColourId, kText);
    setColour (juce::TextButton::textColourOffId, kText);
}

void TearedLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          float startAngle, float endAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const float trackR = radius - 2.0f;
    const bool bipolar = (bool) slider.getProperties().getWithDefault ("bipolar", false);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, trackR, trackR, 0.0f, startAngle, endAngle, true);
    g.setColour (kTrack);
    g.strokePath (track, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    const float a0 = juce::jmin (from, angle), a1 = juce::jmax (from, angle);
    if (a1 - a0 > 0.01f)
    {
        juce::Path val;
        val.addCentredArc (centre.x, centre.y, trackR, trackR, 0.0f, a0, a1, true);
        g.setColour (kCyan.withAlpha (0.25f));
        g.strokePath (val, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (kCyan);
        g.strokePath (val, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float bodyR = juce::jmax (4.0f, radius - 9.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b4568), centre.x, centre.y - bodyR,
                                             juce::Colour (0xff0a1424), centre.x, centre.y + bodyR, false));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (juce::Colour (0xff3a5a85));
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.2f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.8f, -bodyR + 3.0f, 3.6f, bodyR * 0.45f, 1.6f);
    g.setColour (kPink);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

void TearedLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? kPink : kPanel2);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (highlighted ? kText.withAlpha (0.8f) : (on ? kPink : kBorder));
    g.drawRoundedRectangle (r, 8.0f, 1.2f);
    g.setColour (on ? juce::Colour (0xff1a0610) : kText);
    g.setFont (mkFont (13.0f, true));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

void TearedLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (down ? juce::Colour (0xff1c6f93) : (highlighted ? juce::Colour (0xff1b3558) : kPanel2));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (kBorder);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);
}

juce::Font TearedLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return mkFont (12.5f, true); }

//==============================================================================
// Pitch graph
//==============================================================================
void PitchGraph::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff070e1c));
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (kBorder);
    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

    auto plot = b.reduced (6.0f, 8.0f).withTrimmedLeft (44.0f).withTrimmedRight (46.0f);
    const int N = TearedTuneAudioProcessor::kHist;
    const int idx = proc.histIdx.load();
    const uint16_t mask = proc.getEffectiveMask();

    float sum = 0.0f; int cnt = 0;
    for (int k = 0; k < N; ++k)
    {
        const float v = proc.histDet[(size_t) ((idx + k) % N)].load();
        if (v > 0.0f) { sum += v; ++cnt; }
    }
    if (cnt > 0)
    {
        const float c = sum / (float) cnt;
        viewCenter = viewCenter < 1.0f ? c : viewCenter + (c - viewCenter) * 0.06f;
    }
    else if (viewCenter < 1.0f) viewCenter = 60.0f;

    const float range = 7.0f;                                  // semitones above and below the centre
    auto mapY = [&] (float m) { return plot.getCentreY() - (m - viewCenter) / range * (plot.getHeight() * 0.5f); };

    g.saveState();
    g.reduceClipRegion (b.reduced (2.0f).toNearestInt());
    for (int n = (int) std::floor (viewCenter - range) - 1; n <= (int) std::ceil (viewCenter + range) + 1; ++n)
    {
        const bool allowed = (mask & (1 << (((n % 12) + 12) % 12))) != 0;
        const float y = mapY ((float) n);
        if (allowed)
        {
            g.setColour (kCyan.withAlpha (0.22f));
            g.drawLine (plot.getX(), y, plot.getRight(), y, 1.2f);
            g.setColour (kText);
            g.setFont (mkFont (12.0f, true));
            g.drawText (noteName (n), juce::Rectangle<float> (b.getX() + 6.0f, y - 8.0f, 40.0f, 16.0f), juce::Justification::centredLeft);
        }
        else
        {
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawLine (plot.getX(), y, plot.getRight(), y, 1.0f);
            g.setColour (kDim.withAlpha (0.45f));
            g.setFont (mkFont (10.0f));
            g.drawText (noteName (n), juce::Rectangle<float> (b.getX() + 6.0f, y - 8.0f, 40.0f, 16.0f), juce::Justification::centredLeft);
        }
    }

    auto drawLine = [&] (bool target, juce::Colour col, float thick)
    {
        juce::Path path; bool pen = false;
        for (int k = 0; k < N; ++k)
        {
            const float v = (target ? proc.histTgt : proc.histDet)[(size_t) ((idx + k) % N)].load();
            if (v <= 0.0f) { pen = false; continue; }
            const float xx = plot.getX() + plot.getWidth() * (float) k / (float) (N - 1), yy = mapY (v);
            if (! pen) { path.startNewSubPath (xx, yy); pen = true; } else path.lineTo (xx, yy);
        }
        g.setColour (col.withAlpha (0.22f));
        g.strokePath (path, juce::PathStrokeType (thick + 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col);
        g.strokePath (path, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    drawLine (true, kPink, 2.4f);
    drawLine (false, kCyan, 1.8f);
    g.restoreState();

    // readout of the newest value
    const float latest = proc.histDet[(size_t) ((idx + N - 1) % N)].load();
    g.setFont (mkFont (15.0f, true));
    g.setColour (kText);
    juce::String txt = "-";
    if (latest > 0.0f)
    {
        const int nn = (int) std::lround (latest);
        const int c = (int) std::lround ((latest - (float) nn) * 100.0f);
        txt = noteName (nn) + "   " + (c >= 0 ? "+" : "") + juce::String (c) + " ct";
    }
    g.drawText (txt, juce::Rectangle<float> (plot.getRight() - 160.0f, b.getY() + 8.0f, 154.0f, 20.0f), juce::Justification::centredRight);
    g.setFont (mkFont (10.5f, true));
    g.setColour (kCyan);
    g.drawText ("YOUR PITCH", juce::Rectangle<float> (plot.getX() + 6.0f, b.getY() + 8.0f, 90.0f, 14.0f), juce::Justification::centredLeft);
    g.setColour (kPink);
    g.drawText ("CORRECTED", juce::Rectangle<float> (plot.getX() + 96.0f, b.getY() + 8.0f, 90.0f, 14.0f), juce::Justification::centredLeft);

    // IN / OUT meters
    auto meter = [&] (float x, float level, const char* name)
    {
        const juce::Rectangle<float> m (x, plot.getY() + 14.0f, 9.0f, plot.getHeight() - 36.0f);
        g.setColour (kTrack);
        g.fillRoundedRectangle (m, 3.0f);
        const float dB = juce::Decibels::gainToDecibels (level, -60.0f);
        const float f = juce::jlimit (0.0f, 1.0f, (dB + 60.0f) / 60.0f);
        g.setGradientFill (juce::ColourGradient (kPink, 0.0f, m.getY(), kCyan, 0.0f, m.getBottom(), false));
        g.fillRoundedRectangle (m.withTrimmedTop (m.getHeight() * (1.0f - f)), 3.0f);
        g.setColour (kDim);
        g.setFont (mkFont (9.5f, true));
        g.drawText (name, juce::Rectangle<float> (x - 9.0f, m.getBottom() + 3.0f, 27.0f, 12.0f), juce::Justification::centred);
    };
    meter (plot.getRight() + 8.0f, proc.uiIn.load(), "IN");
    meter (plot.getRight() + 26.0f, proc.uiOut.load(), "OUT");
}

//==============================================================================
// Note keys
//==============================================================================
juce::Rectangle<float> NoteKeys::keyRect (int pc) const
{
    const auto b = getLocalBounds().toFloat();
    const float wW = b.getWidth() / 7.0f;
    static const int whiteIndex[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    static const int blackAfter[12] = { -1, 0, -1, 1, -1, -1, 3, -1, 4, -1, 5, -1 };   // white index the black key sits to the right of
    if (whiteIndex[pc] >= 0)
        return { b.getX() + wW * (float) whiteIndex[pc] + 1.0f, b.getY(), wW - 2.0f, b.getHeight() };
    const float bw = wW * 0.6f;
    return { b.getX() + wW * (float) (blackAfter[pc] + 1) - bw * 0.5f, b.getY(), bw, b.getHeight() * 0.62f };
}

int NoteKeys::keyAt (juce::Point<float> p) const
{
    static const int blacks[5] = { 1, 3, 6, 8, 10 }, whites[7] = { 0, 2, 4, 5, 7, 9, 11 };
    for (int pc : blacks) if (keyRect (pc).contains (p)) return pc;
    for (int pc : whites) if (keyRect (pc).contains (p)) return pc;
    return -1;
}

void NoteKeys::mouseDown (const juce::MouseEvent& e)
{
    const int pc = keyAt (e.position);
    if (pc >= 0) proc.toggleNote (pc);
}

void NoteKeys::paint (juce::Graphics& g)
{
    const uint16_t mask = proc.getEffectiveMask();
    int detPc = -1;
    const int idx = proc.histIdx.load();
    const float latest = proc.histDet[(size_t) ((idx + TearedTuneAudioProcessor::kHist - 1) % TearedTuneAudioProcessor::kHist)].load();
    if (latest > 0.0f) detPc = ((int) std::lround (latest) % 12 + 12) % 12;

    static const int blacks[5] = { 1, 3, 6, 8, 10 }, whites[7] = { 0, 2, 4, 5, 7, 9, 11 };
    auto drawKey = [&] (int pc, bool black)
    {
        const auto r = keyRect (pc);
        const bool on = (mask & (1 << pc)) != 0;
        if (black) g.setColour (on ? juce::Colour (0xff1c8fb8) : juce::Colour (0xff070e1c));
        else       g.setColour (on ? kCyan : kPanel2);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (black ? kBorder : (on ? kCyan.brighter (0.4f) : kBorder));
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        g.setColour (on ? juce::Colour (0xff04121e) : kDim);
        g.setFont (mkFont (black ? 11.0f : 13.0f, true));
        g.drawText (kNoteNames[pc], r.withTrimmedBottom (6.0f), juce::Justification::centredBottom);
        if (pc == detPc)
        {
            g.setColour (kPink);
            g.fillEllipse (r.getCentreX() - 5.0f, r.getY() + 8.0f, 10.0f, 10.0f);
        }
    };
    for (int pc : whites) drawKey (pc, false);
    for (int pc : blacks) drawKey (pc, true);
}

//==============================================================================
// Editor
//==============================================================================
static void paintDrop (juce::Graphics& g, float cx, float cy, float r, juce::Colour c)
{
    juce::Path p;
    p.addEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    p.addTriangle (cx, cy - r * 1.9f, cx - r * 0.85f, cy - r * 0.526f, cx + r * 0.85f, cy - r * 0.526f);
    g.setColour (c);
    g.fillPath (p);
}

TearedTuneEditor::TearedTuneEditor (TearedTuneAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), graph (p), keys (p)
{
    setLookAndFeel (&laf);
    setSize (1020, 620);
    setResizable (false, false);

    addAndMakeVisible (graph);
    addAndMakeVisible (keys);

    Panel graphPanel { "PITCH MONITOR", { 12, 84, 996, 236 }, {}, 0 };

    Panel keyPanel { "KEY & SCALE", { 12, 332, 372, 276 }, {}, 0 };

    Panel corr { "CORRECTION", { 396, 332, 330, 276 }, {}, 2 };
    corr.knobs = { addKnob (pid::retune, "RETUNE SPEED", " ms", 0),
                   addKnob (pid::amount, "AMOUNT", " %", 0),
                   addKnob (pid::vibrato, "KEEP VIBRATO", " %", 0),
                   addKnob (pid::flex, "FLEX", " ct", 0) };

    Panel fine { "PITCH & OUTPUT", { 738, 332, 270, 276 }, {}, 3 };
    fine.knobs = { addKnob (pid::transpose, "TRANSPOSE", " st", 0, true),
                   addKnob (pid::detune, "DETUNE", " ct", 0, true),
                   addKnob (pid::refa, "REF A", " Hz", 1),
                   addKnob (pid::mix, "MIX", " %", 0),
                   addKnob (pid::ingain, "INPUT", " dB", 1, true),
                   addKnob (pid::outgain, "OUTPUT", " dB", 1, true) };

    panels = { graphPanel, keyPanel, corr, fine };

    addCombo (keyBox, juce::StringArray { kNoteNames[0], kNoteNames[1], kNoteNames[2], kNoteNames[3], kNoteNames[4], kNoteNames[5],
                                          kNoteNames[6], kNoteNames[7], kNoteNames[8], kNoteNames[9], kNoteNames[10], kNoteNames[11] }, pid::key, keyAtt);
    juce::StringArray scales; for (auto* s : kScaleNames) scales.add (s);
    addCombo (scaleBox, scales, pid::scale, scaleAtt);
    juce::StringArray ranges; for (auto* s : kRangeNames) ranges.add (s);
    addCombo (rangeBox, ranges, pid::range, rangeAtt);

    addCaption (keyCap, "KEY");
    addCaption (scaleCap, "SCALE");
    addCaption (rangeCap, "VOICE RANGE");
    addCaption (keysCap, "ALLOWED NOTES  -  CLICK A KEY TO TURN IT ON / OFF");

    addAndMakeVisible (naturalBtn); addAndMakeVisible (classicBtn); addAndMakeVisible (hardBtn);
    naturalBtn.onClick = [this] { applyPreset (110.0f, 85.0f, 12.0f); };
    classicBtn.onClick = [this] { applyPreset (35.0f, 40.0f, 0.0f); };
    hardBtn.onClick    = [this] { applyPreset (0.0f, 0.0f, 0.0f); };

    addAndMakeVisible (bypassBtn);
    bypassBtn.setButtonText ("BYPASS");
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, pid::bypass, bypassBtn);

    resized();
}

TearedTuneEditor::~TearedTuneEditor() { setLookAndFeel (nullptr); }

void TearedTuneEditor::applyPreset (float retune, float vibrato, float flex)
{
    proc.setParamValue (pid::retune, retune);
    proc.setParamValue (pid::vibrato, vibrato);
    proc.setParamValue (pid::flex, flex);
    proc.setParamValue (pid::amount, 100.0f);
}

void TearedTuneEditor::addCaption (juce::Label& l, const juce::String& text)
{
    addAndMakeVisible (l);
    l.setText (text, juce::dontSendNotification);
    l.setFont (mkFont (11.0f, true));
    l.setColour (juce::Label::textColourId, kDim);
    l.setInterceptsMouseClicks (false, false);
}

void TearedTuneEditor::addCombo (juce::ComboBox& box, const juce::StringArray& items, const char* id,
                                 std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att)
{
    addAndMakeVisible (box);
    box.addItemList (items, 1);                      // items must exist before the attachment is made
    att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, box);
}

TearedTuneEditor::Knob* TearedTuneEditor::addKnob (const char* id, const juce::String& name, const juce::String& suffix, int decimals, bool bipolar)
{
    auto k = std::make_unique<Knob>();
    auto* raw = k.get();

    raw->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    raw->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 86, 20);
    raw->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    raw->slider.getProperties().set ("bipolar", bipolar);
    addAndMakeVisible (raw->slider);

    raw->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, raw->slider);

    // the attachment installs its own text functions, so ours (units + decimals) go in afterwards
    raw->slider.textFromValueFunction = [suffix, decimals] (double v) { return juce::String (v, decimals) + suffix; };
    raw->slider.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };
    if (auto* par = proc.apvts.getParameter (id))
        raw->slider.setDoubleClickReturnValue (true, (double) par->convertFrom0to1 (par->getDefaultValue()));
    raw->slider.updateText();

    raw->label.setText (name, juce::dontSendNotification);
    raw->label.setJustificationType (juce::Justification::centred);
    raw->label.setFont (mkFont (11.5f, true));
    raw->label.setColour (juce::Label::textColourId, kDim);
    raw->label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (raw->label);

    knobs.push_back (std::move (k));
    return raw;
}

void TearedTuneEditor::resized()
{
    // knob grids
    for (auto& p : panels)
    {
        if (p.cols <= 0 || p.knobs.empty()) continue;
        auto content = p.bounds.reduced (10, 0).withTrimmedTop (32).withTrimmedBottom (8);
        const int rows = ((int) p.knobs.size() + p.cols - 1) / p.cols;
        const int cw = content.getWidth() / p.cols, rh = content.getHeight() / rows;
        for (size_t i = 0; i < p.knobs.size(); ++i)
        {
            const int cx = content.getX() + ((int) i % p.cols) * cw, cy = content.getY() + ((int) i / p.cols) * rh;
            juce::Rectangle<int> cell (cx, cy, cw, rh);
            p.knobs[i]->label.setBounds (cell.removeFromTop (18));
            p.knobs[i]->slider.setBounds (cell);
        }
    }

    graph.setBounds (panels[0].bounds.reduced (8).withTrimmedTop (24));

    // key & scale panel
    keyCap.setBounds (24, 364, 90, 14);      keyBox.setBounds (24, 380, 84, 28);
    scaleCap.setBounds (118, 364, 120, 14);  scaleBox.setBounds (118, 380, 254, 28);
    rangeCap.setBounds (24, 416, 200, 14);   rangeBox.setBounds (24, 432, 348, 28);
    keysCap.setBounds (24, 470, 348, 14);    keys.setBounds (24, 488, 348, 108);

    // header buttons
    naturalBtn.setBounds (560, 22, 96, 28);
    classicBtn.setBounds (662, 22, 96, 28);
    hardBtn.setBounds (764, 22, 100, 28);
    bypassBtn.setBounds (896, 22, 100, 28);
}

void TearedTuneEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (kBgTop, 0.0f, 0.0f, kBgBot, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // header
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d2748), 0.0f, 0.0f, juce::Colour (0xff134a78), (float) getWidth(), 0.0f, false));
    g.fillRect (0.0f, 0.0f, (float) getWidth(), 72.0f);
    g.setColour (kCyan.withAlpha (0.6f));
    g.fillRect (0.0f, 72.0f, (float) getWidth(), 2.0f);

    // falling teardrops
    const float dx[] = { 395, 435, 473, 510, 542 }, dy[] = { 24, 48, 20, 46, 28 }, dr[] = { 6, 4.5f, 7, 5, 4 };
    for (int i = 0; i < 5; ++i) paintDrop (g, dx[i], dy[i], dr[i], kCyan.withAlpha (0.28f));

    paintDrop (g, 38.0f, 42.0f, 11.0f, kCyan);
    paintDrop (g, 38.0f, 42.0f, 4.5f, juce::Colour (0xff0d2748));

    g.setColour (kText);
    g.setFont (mkFont (32.0f, true));
    g.drawText ("TEARED TUNE", juce::Rectangle<int> (64, 8, 240, 42), juce::Justification::centredLeft);
    g.setColour (kPink);
    g.fillRoundedRectangle (306.0f, 20.0f, 40.0f, 20.0f, 6.0f);
    g.setColour (juce::Colour (0xff1a0610));
    g.setFont (mkFont (13.0f, true));
    g.drawText ("V3", juce::Rectangle<int> (306, 20, 40, 20), juce::Justification::centred);
    g.setColour (kText.withAlpha (0.75f));
    g.setFont (mkFont (11.0f, true));
    g.drawText ("REAL-TIME PITCH CORRECTION", juce::Rectangle<int> (66, 48, 300, 16), juce::Justification::centredLeft);

    for (const auto& p : panels)
    {
        const auto r = p.bounds.toFloat();
        g.setColour (kPanel);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (kBorder);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        g.setColour (kCyan);
        g.fillRoundedRectangle (r.getX() + 14.0f, r.getY() + 12.0f, 4.0f, 14.0f, 2.0f);
        g.setColour (kText);
        g.setFont (mkFont (13.0f, true));
        g.drawText (p.title, juce::Rectangle<float> (r.getX() + 24.0f, r.getY() + 9.0f, r.getWidth() - 30.0f, 20.0f), juce::Justification::centredLeft);
    }
}
