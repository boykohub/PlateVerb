// N1081 — plugin wrapper: parameters, oversampling, editor
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "N1081Engine.h"

using namespace juce;

namespace ids {
    static const char* line = "line";   static const char* out = "output";
    static const char* hpf = "hpf";     static const char* lpf = "lpf";
    static const char* eqIn = "eqin";   static const char* phase = "phase";
    static const char* freq[4] = { "lf_freq", "lmf_freq", "hmf_freq", "hf_freq" };
    static const char* gain[4] = { "lf_gain", "lmf_gain", "hmf_gain", "hf_gain" };
    static const char* alt [4] = { "lf_peak", "lmf_hiq", "hmf_hiq", "hf_peak" };
}

static String hz (double f) { return f >= 1000 ? String (f / 1000.0, f < 10000 && std::fmod (f, 1000.0) != 0 ? 1 : 0) + "k" : String ((int) f); }

static StringArray freqNames (const double* t, int n) { StringArray a; for (int i = 0; i < n; ++i) a.add (hz (t[i])); return a; }

static AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    StringArray lineNames; for (int i = 0; i < n1081::kLineGainSteps; ++i) { const int g = (int) n1081::lineGainDb (i); lineNames.add ((g > 0 ? "+" : "") + String (g) + " dB"); }
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::line, 1 }, "Line Gain", lineNames, 3));
    StringArray hp { "Off" }; hp.addArray (freqNames (n1081::kHpfFreqs, 5));
    StringArray lp { "Off" }; lp.addArray (freqNames (n1081::kLpfFreqs, 5));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::hpf, 1 }, "High Pass", hp, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::lpf, 1 }, "Low Pass", lp, 0));
    const double* tabs[4] = { n1081::kLfFreqs, n1081::kLmfFreqs, n1081::kHmfFreqs, n1081::kHfFreqs };
    const int counts[4] = { 5, 10, 10, 5 }, defs[4] = { 2, 4, 5, 3 };
    const char* names[4] = { "LF", "Low Mid", "High Mid", "HF" };
    const char* altNames[4] = { "LF Peak", "Low Mid Hi-Q", "High Mid Hi-Q", "HF Peak" };
    for (int b = 0; b < 4; ++b) {
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { ids::freq[b], 1 }, String (names[b]) + " Freq", freqNames (tabs[b], counts[b]), defs[b]));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::gain[b], 1 }, String (names[b]) + " Gain",
                   NormalisableRange<float> (-18.f, 18.f, 0.1f), 0.f, AudioParameterFloatAttributes().withLabel ("dB")));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { ids::alt[b], 1 }, altNames[b], false));
    }
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ids::eqIn, 1 }, "EQ In", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { ids::phase, 1 }, "Phase", false));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::out, 1 }, "Output",
               NormalisableRange<float> (-24.f, 12.f, 0.1f), 0.f, AudioParameterFloatAttributes().withLabel ("dB")));
    return l;
}

//==============================================================================
class N1081Processor : public AudioProcessor
{
public:
    N1081Processor()
        : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                           .withOutput ("Output", AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "N1081", makeLayout())
    {
        auto p = [this] (const char* id) { return apvts.getRawParameterValue (id); };
        pLine = p (ids::line); pOut = p (ids::out); pHpf = p (ids::hpf); pLpf = p (ids::lpf);
        pEq = p (ids::eqIn); pPhase = p (ids::phase);
        for (int b = 0; b < 4; ++b) { pFreq[b] = p (ids::freq[b]); pGain[b] = p (ids::gain[b]); pAlt[b] = p (ids::alt[b]); }
    }

    const String getName() const override { return "N1081"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const String getProgramName (int) override { return {}; }
    void changeProgramName (int, const String&) override {}

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        const auto& o = l.getMainOutputChannelSet();
        return (o == AudioChannelSet::mono() || o == AudioChannelSet::stereo()) && o == l.getMainInputChannelSet();
    }

    void prepareToPlay (double sr, int block) override
    {
        osFactor = sr >= 88000.0 ? 1 : 2;                      // run the circuit at 176.4 kHz or above
        oversampler = std::make_unique<dsp::Oversampling<float>> ((size_t) jmax (1, getTotalNumOutputChannels()), (size_t) osFactor,
                          dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        maxBlock = jmax (1, block);
        oversampler->initProcessing ((size_t) maxBlock);
        setLatencySamples ((int) std::round (oversampler->getLatencyInSamples()));
        osRate = sr * (1 << osFactor);
        for (auto& c : ch) c.prepare (osRate);
        for (int b = 0; b < 4; ++b) { gainSm[b].reset (osRate, 0.03); gainSm[b].setCurrentAndTargetValue (pGain[b]->load()); }
        lineSm.reset (sr, 0.03); lineSm.setCurrentAndTargetValue ((float) Decibels::decibelsToGain (n1081::lineGainDb ((int) pLine->load())));
        outSm.reset (sr, 0.03);  outSm.setCurrentAndTargetValue ((float) Decibels::decibelsToGain (pOut->load()));
        lastHpf = lastLpf = -1; for (auto& k : lastKey) k = -1;
        updateCoefs (true);
    }
    void releaseResources() override {}

    void processBlock (AudioBuffer<float>& whole, MidiBuffer&) override
    {
        ScopedNoDenormals nd;
        // never hand the oversampler more than it was prepared for
        const int total = whole.getNumSamples();
        for (int start = 0; start < total; start += maxBlock) {
            AudioBuffer<float> part (whole.getArrayOfWritePointers(), whole.getNumChannels(), start, jmin (maxBlock, total - start));
            processChunk (part);
        }
    }

    void processChunk (AudioBuffer<float>& buf)
    {
        const int nCh = jmin (2, buf.getNumChannels()), n = buf.getNumSamples();
        for (int c = nCh; c < getTotalNumOutputChannels(); ++c) buf.clear (c, 0, n);

        lineSm.setTargetValue ((float) Decibels::decibelsToGain (n1081::lineGainDb ((int) pLine->load())));
        outSm.setTargetValue ((float) Decibels::decibelsToGain (pOut->load()));
        for (int b = 0; b < 4; ++b) gainSm[b].setTargetValue (pEq->load() > 0.5f ? pGain[b]->load() : 0.f);

        dsp::AudioBlock<float> block (buf.getArrayOfWritePointers(), (size_t) nCh, (size_t) n);
        auto up = oversampler->processSamplesUp (block);
        const int m = (int) up.getNumSamples();
        const float lineStart = lineSm.getCurrentValue();
        lineSm.skip (n);
        const float lineEnd = lineSm.getCurrentValue();
        const int hpf = (int) pHpf->load(), lpf = (int) pLpf->load();
        if (hpf != lastHpf || lpf != lastLpf) { for (auto& c : ch) c.setFilters (hpf, lpf); lastHpf = hpf; lastLpf = lpf; }

        constexpr int sub = 32;
        for (int s0 = 0; s0 < m; s0 += sub) {
            updateCoefs (false, jmin (sub, m - s0));
            const int len = jmin (sub, m - s0);
            for (int c = 0; c < nCh; ++c) {
                float* d = up.getChannelPointer ((size_t) c) + s0;
                for (int i = 0; i < len; ++i) {
                    const double g = lineStart + (lineEnd - lineStart) * (double) (s0 + i) / (double) m;
                    d[i] = (float) ch[c].process (d[i], g, hpf, lpf);
                }
            }
        }
        oversampler->processSamplesDown (block);

        const bool flip = pPhase->load() > 0.5f;
        float peak = 0;
        for (int i = 0; i < n; ++i) {
            const float g = outSm.getNextValue() * (flip ? -1.f : 1.f);
            for (int c = 0; c < nCh; ++c) { auto* d = buf.getWritePointer (c); d[i] *= g; peak = jmax (peak, std::abs (d[i])); }
        }
        meter.store (jmax (peak, meter.load() * 0.85f));
    }

    n1081::Params currentParams() const
    {
        n1081::Params p;
        p.lineStep = (int) pLine->load(); p.outputDb = pOut->load();
        p.eqIn = pEq->load() > 0.5f; p.phase = pPhase->load() > 0.5f;
        p.hpf = (int) pHpf->load(); p.lpf = (int) pLpf->load();
        for (int b = 0; b < 4; ++b) { p.freqIdx[b] = (int) pFreq[b]->load(); p.gainDb[b] = pGain[b]->load(); p.alt[b] = pAlt[b]->load() > 0.5f; }
        return p;
    }

    bool hasEditor() const override { return true; }
    AudioProcessorEditor* createEditor() override;

    void getStateInformation (MemoryBlock& d) override
    {
        auto st = apvts.copyState();
        st.setProperty ("uiScale", uiScale, nullptr);
        if (auto x = st.createXml()) copyXmlToBinary (*x, d);
    }
    void setStateInformation (const void* d, int size) override
    {
        if (auto x = getXmlFromBinary (d, size)) {
            auto st = ValueTree::fromXml (*x);
            if (st.hasType (apvts.state.getType())) { uiScale = (float) (double) st.getProperty ("uiScale", 1.0); apvts.replaceState (st); }
        }
    }

    AudioProcessorValueTreeState apvts;
    std::atomic<float> meter { 0 };
    float uiScale = 1.0f;

private:
    void updateCoefs (bool force, int advance = 0)
    {
        n1081::Params p = currentParams();
        for (int b = 0; b < 4; ++b) {
            if (advance > 0) gainSm[b].skip (advance);
            p.gainDb[b] = gainSm[b].getCurrentValue();
            const int key = p.freqIdx[b] * 2 + (p.alt[b] ? 1 : 0);
            const bool moving = gainSm[b].isSmoothing();
            if (force || moving || key != lastKey[b] || std::abs (p.gainDb[b] - lastGain[b]) > 1e-4f) {
                const auto s = n1081::bandSpec (p, b);
                for (auto& c : ch) c.setBand (b, s, true);   // EQ in/out is a smoothed fade of the band gains
                lastKey[b] = key; lastGain[b] = (float) p.gainDb[b];
            }
        }
    }

    std::atomic<float>* pLine {}, * pOut {}, * pHpf {}, * pLpf {}, * pEq {}, * pPhase {};
    std::atomic<float>* pFreq[4] {}, * pGain[4] {}, * pAlt[4] {};
    std::unique_ptr<dsp::Oversampling<float>> oversampler;
    int osFactor = 1, maxBlock = 512; double osRate = 96000;
    n1081::Channel ch[2];
    SmoothedValue<float> gainSm[4], lineSm, outSm;
    int lastHpf = -1, lastLpf = -1, lastKey[4] { -1, -1, -1, -1 };
    float lastGain[4] { 99, 99, 99, 99 };
};

//==============================================================================
// Look: grey-blue console panel, coloured knob caps, cream legends
namespace col {
    static const Colour panel  (0xff4f5b66), panelDark (0xff3c4650), well (0xff2b3239);
    static const Colour legend (0xffece6d6), dim (0xffa9b0b4);
    static const Colour capRed (0xffb8322c), capBlue (0xff2f5d8c), capGrey (0xff8c9196), capBlack (0xff23272b);
    static const Colour lampOn (0xffffb43c), curve (0xffffc864);
}

class KnobLnF : public LookAndFeel_V4
{
public:
    void drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s) override
    {
        const auto cap = s.findColour (Slider::rotarySliderFillColourId);
        const bool stepped = s.getProperties()["stepped"];
        auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4);
        const float d = jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (d, d);
        const auto c = r.getCentre();
        const float ang = a0 + pos * (a1 - a0);

        // scale ticks
        const int ticks = stepped ? (int) s.getMaximum() - (int) s.getMinimum() + 1 : 13;
        g.setColour (col::dim);
        for (int i = 0; i < ticks; ++i) {
            const float t = a0 + (a1 - a0) * (float) i / (float) jmax (1, ticks - 1);
            const auto p1 = c.getPointOnCircumference (d * 0.5f, t), p2 = c.getPointOnCircumference (d * 0.43f, t);
            g.drawLine ({ p1, p2 }, 1.2f);
        }
        // skirt + cap
        auto body = r.reduced (d * 0.12f);
        g.setColour (Colours::black.withAlpha (0.45f)); g.fillEllipse (body.translated (0, d * 0.03f));
        g.setGradientFill (ColourGradient (col::capBlack.brighter (0.4f), body.getTopLeft(), col::capBlack, body.getBottomRight(), false));
        g.fillEllipse (body);
        auto top = body.reduced (d * 0.09f);
        g.setGradientFill (ColourGradient (cap.brighter (0.35f), top.getTopLeft(), cap.darker (0.3f), top.getBottomRight(), false));
        g.fillEllipse (top);
        // pointer
        g.setColour (col::legend);
        g.drawLine ({ c.getPointOnCircumference (d * 0.08f, ang), c.getPointOnCircumference (body.getWidth() * 0.5f - 1, ang) }, 2.4f);
    }

    void drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.5f);
        const bool on = b.getToggleState();
        g.setColour (col::well); g.fillRoundedRectangle (r, 3);
        auto face = r.reduced (2.5f);
        g.setGradientFill (ColourGradient (on ? col::lampOn : Colour (0xffd9d4c6), face.getTopLeft(),
                                           on ? col::lampOn.darker (0.35f) : Colour (0xff9e998c), face.getBottomLeft(), false));
        g.fillRoundedRectangle (face, 2);
        if (over || down) { g.setColour (Colours::white.withAlpha (down ? 0.25f : 0.1f)); g.fillRoundedRectangle (face, 2); }
    }
    void drawButtonText (Graphics& g, TextButton& b, bool, bool) override
    {
        g.setColour (Colour (0xff23272b)); g.setFont (FontOptions (11.5f, Font::bold));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds(), Justification::centred, 1);
    }
};

//==============================================================================
class Graph : public Component, private Timer
{
public:
    explicit Graph (N1081Processor& p) : proc (p) { startTimerHz (30); }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::well); g.fillRoundedRectangle (r, 5);
        r = r.reduced (8, 6);
        auto xOf = [&] (double f) { return r.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * r.getWidth(); };
        auto yOf = [&] (double db) { return r.getCentreY() - (float) (db / 24.0) * r.getHeight() * 0.5f; };
        g.setFont (FontOptions (10.f));
        for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 }) {
            g.setColour (Colours::white.withAlpha (0.07f)); g.drawVerticalLine ((int) xOf (f), r.getY(), r.getBottom());
            g.setColour (col::dim.withAlpha (0.7f)); g.drawText (hz (f), (int) xOf (f) + 2, (int) r.getBottom() - 12, 30, 12, Justification::left);
        }
        for (double db : { -18.0, -12.0, -6.0, 6.0, 12.0, 18.0 }) {
            g.setColour (Colours::white.withAlpha (0.05f)); g.drawHorizontalLine ((int) yOf (db), r.getX(), r.getRight());
            g.setColour (col::dim.withAlpha (0.6f)); g.drawText (String ((int) db), (int) r.getX() + 2, (int) yOf (db) - 6, 24, 12, Justification::left);
        }
        g.setColour (Colours::white.withAlpha (0.18f)); g.drawHorizontalLine ((int) yOf (0), r.getX(), r.getRight());

        const auto p = proc.currentParams();
        Path path;
        const int N = (int) r.getWidth();
        for (int i = 0; i <= N; ++i) {
            const double f = 20.0 * std::pow (1000.0, (double) i / N);
            const float y = jlimit (r.getY(), r.getBottom(), yOf (n1081::responseDb (p, f)));
            if (i == 0) path.startNewSubPath (xOf (f), y); else path.lineTo (xOf (f), y);
        }
        Path fill (path); fill.lineTo (r.getRight(), yOf (0)); fill.lineTo (r.getX(), yOf (0)); fill.closeSubPath();
        g.setColour (col::curve.withAlpha (p.eqIn ? 0.12f : 0.04f)); g.fillPath (fill);
        g.setColour (p.eqIn ? col::curve : col::curve.withAlpha (0.35f)); g.strokePath (path, PathStrokeType (2.f));

        // output meter
        auto mr = getLocalBounds().toFloat().removeFromRight (10).reduced (2, 6);
        const float lvl = proc.meter.load();
        const float db = (float) Decibels::gainToDecibels (lvl, -60.0f);
        const float frac = jlimit (0.f, 1.f, (db + 60.f) / 60.f);
        g.setColour (Colours::black.withAlpha (0.4f)); g.fillRect (mr);
        g.setColour (db > -1.f ? Colours::red : db > -12.f ? col::lampOn : Colour (0xff7fc97f));
        g.fillRect (mr.withTop (mr.getBottom() - mr.getHeight() * frac));
    }
private:
    void timerCallback() override { repaint(); }
    N1081Processor& proc;
};

//==============================================================================
class Panel : public Component
{
public:
    explicit Panel (N1081Processor& p) : proc (p), graph (p)
    {
        setLookAndFeel (&lnf);
        addAndMakeVisible (graph);
        knob (line, ids::line, col::capRed, true, -1);
        knob (hpf, ids::hpf, col::capGrey, true, -1);
        knob (lpf, ids::lpf, col::capGrey, true, -1);
        const Colour caps[4] = { col::capBlue, col::capGrey, col::capGrey, col::capBlue };
        for (int b = 0; b < 4; ++b) {
            knob (freq[b], ids::freq[b], caps[b], true, -1);
            knob (gain[b], ids::gain[b], col::capBlack.brighter (0.25f), false, 0.0);
            gain[b].getProperties().set ("stepped", false);
            button (alt[b], ids::alt[b], b == 0 || b == 3 ? "PEAK" : "HI-Q");
        }
        knob (out, ids::out, col::capGrey, false, 0.0);
        button (eqIn, ids::eqIn, "EQ");
        button (phase, ids::phase, "PH");
        setSize (kW, kH);
    }
    ~Panel() override { setLookAndFeel (nullptr); }

    static constexpr int kW = 1080, kH = 430;

    void paint (Graphics& g) override
    {
        g.setGradientFill (ColourGradient (col::panel.brighter (0.06f), 0, 0, col::panelDark, 0, (float) kH, false));
        g.fillAll();
        // screws
        for (auto pt : { Point<float> (14, 14), Point<float> (kW - 14.f, 14), Point<float> (14, kH - 14.f), Point<float> (kW - 14.f, kH - 14.f) }) {
            g.setColour (Colour (0xff9aa1a6)); g.fillEllipse (Rectangle<float> (9, 9).withCentre (pt));
            g.setColour (Colour (0xff4a4f53)); g.drawLine (pt.x - 3, pt.y, pt.x + 3, pt.y, 1.3f);
        }
        g.setColour (col::legend);
        g.setFont (FontOptions (30.f, Font::bold)); g.drawText ("1081", 28, 34, 130, 34, Justification::left);
        g.setFont (FontOptions (11.f)); g.setColour (col::dim);
        g.drawText ("CHANNEL AMPLIFIER", 28, 72, 140, 14, Justification::left);
        g.drawText ("CIRCUIT MODEL", 28, 88, 140, 14, Justification::left);

        // section frames + legends
        for (auto& s : sections) {
            g.setColour (Colours::black.withAlpha (0.18f)); g.fillRoundedRectangle (s.r.toFloat(), 6);
            g.setColour (col::legend); g.setFont (FontOptions (12.f, Font::bold));
            g.drawText (s.title, s.r.getX(), s.r.getY() + 6, s.r.getWidth(), 14, Justification::centred);
        }
        g.setFont (FontOptions (10.5f)); g.setColour (col::dim);
        for (auto& l : labels) g.drawText (l.text, l.r, Justification::centred);
    }

    void resized() override
    {
        sections.clear(); labels.clear();
        graph.setBounds (170, 16, kW - 200, 118);
        const int top = 148, h = kH - top - 22;
        int x = 22;
        auto section = [&] (const String& t, int w) { Rectangle<int> r (x, top, w, h); sections.push_back ({ t, r }); x += w + 8; return r; };
        auto place = [&] (Slider& s, Rectangle<int> r, const String& lab) { s.setBounds (r); labels.push_back ({ lab, r.withY (r.getBottom() - 2).withHeight (14) }); };

        auto r = section ("INPUT", 110);
        place (line, { r.getX() + 10, r.getY() + 28, 90, 90 }, "LINE GAIN");
        phase.setBounds (r.getX() + 25, r.getY() + 160, 60, 30);
        labels.push_back ({ "PHASE", { r.getX(), r.getY() + 192, r.getWidth(), 14 } });

        r = section ("FILTERS", 110);
        place (hpf, { r.getX() + 18, r.getY() + 26, 74, 74 }, "HIGH PASS");
        place (lpf, { r.getX() + 18, r.getY() + 128, 74, 74 }, "LOW PASS");

        const char* titles[4] = { "LF", "LOW MID", "HIGH MID", "HF" };
        // order on the panel: HF on the left like the hardware's top-to-bottom reading? keep low → high
        for (int b = 0; b < 4; ++b) {
            r = section (titles[b], 160);
            place (freq[b], { r.getX() + 8, r.getY() + 26, 70, 70 }, "FREQ");
            place (gain[b], { r.getX() + 80, r.getY() + 26, 74, 74 }, "GAIN dB");
            alt[b].setBounds (r.getX() + 45, r.getY() + 150, 70, 30);
            labels.push_back ({ b == 0 || b == 3 ? "SHELF / PEAK" : "WIDE / HI-Q", { r.getX(), r.getY() + 182, r.getWidth(), 14 } });
        }
        r = section ("OUTPUT", kW - x - 22);
        place (out, { r.getX() + 12, r.getY() + 28, r.getWidth() - 24, r.getWidth() - 24 }, "TRIM dB");
        eqIn.setBounds (r.getX() + (r.getWidth() - 60) / 2, r.getY() + 160, 60, 30);
        labels.push_back ({ "EQ IN", { r.getX(), r.getY() + 192, r.getWidth(), 14 } });
    }

private:
    void knob (Slider& s, const char* id, Colour cap, bool stepped, double resetTo)
    {
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::TextBoxBelow, false, 64, 16);
        s.setColour (Slider::rotarySliderFillColourId, cap);
        s.setColour (Slider::textBoxTextColourId, col::legend);
        s.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        s.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        s.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
        s.getProperties().set ("stepped", stepped);
        s.setMouseDragSensitivity (stepped ? 160 : 260);
        s.setVelocityBasedMode (false);
        addAndMakeVisible (s);
        attachments.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, s));
        if (resetTo >= -100) s.setDoubleClickReturnValue (true, resetTo);
        s.onValueChange = [this] { graph.repaint(); };
    }
    void button (TextButton& b, const char* id, const String& text)
    {
        b.setButtonText (text); b.setClickingTogglesState (true);
        addAndMakeVisible (b);
        buttonAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, b));
    }

    struct Section { String title; Rectangle<int> r; };
    struct Label { String text; Rectangle<int> r; };
    N1081Processor& proc;
    KnobLnF lnf;
    Graph graph;
    Slider line, hpf, lpf, out, freq[4], gain[4];
    TextButton alt[4], eqIn, phase;
    std::vector<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>> attachments;
    std::vector<std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>> buttonAtt;
    std::vector<Section> sections; std::vector<Label> labels;
};

//==============================================================================
class N1081Editor : public AudioProcessorEditor
{
public:
    explicit N1081Editor (N1081Processor& p) : AudioProcessorEditor (p), proc (p), panel (p)
    {
        addAndMakeVisible (panel);
        const float start = jlimit (0.6f, 2.0f, p.uiScale);
        setSize ((int) (Panel::kW * start), (int) (Panel::kH * start));
        setResizable (true, true);
        setResizeLimits (Panel::kW * 6 / 10, Panel::kH * 6 / 10, Panel::kW * 2, Panel::kH * 2);
        if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) Panel::kW / Panel::kH);
        ready = true;
    }
    void resized() override
    {
        const float s = (float) getWidth() / (float) Panel::kW;
        if (ready) proc.uiScale = s;
        panel.setTransform (AffineTransform::scale (s));
    }
private:
    N1081Processor& proc;
    Panel panel;
    bool ready = false;
};

AudioProcessorEditor* N1081Processor::createEditor() { return new N1081Editor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new N1081Processor(); }
