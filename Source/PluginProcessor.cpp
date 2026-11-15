// PlateVerb - multi-algorithm reverb with delay, compressor and output transformer.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"

using namespace juce;
using APVTS = AudioProcessorValueTreeState;

static const char* const kAlgoNames[] = { "PLATE", "ROOM", "CHAMBER", "HALL", "CHURCH", "STADIUM" };
static const int kOsValues[] = { 1, 2, 3, 4, 6 };
static const float kDivBeats[] = { 0.0625f, 1.0f / 12.0f, 0.125f, 1.0f / 6.0f, 0.25f, 0.375f, 1.0f / 3.0f, 0.5f,
                                   0.75f, 2.0f / 3.0f, 1.0f, 1.5f, 4.0f / 3.0f, 2.0f, 3.0f, 4.0f };

static StringArray osNames()  { return { "1x", "2x", "3x", "4x", "6x" }; }
static StringArray divNames() { return { "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8",
                                         "1/8D", "1/4T", "1/4", "1/4D", "1/2T", "1/2", "1/2D", "1/1" }; }
static StringArray trNames()  { StringArray s; for (int i = 0; i < pv::kNumTransformers; ++i) s.add (pv::transformerModel (i).name); return s; }

static String algoId (const char* prefix, int a) { return String (prefix) + String (a); }

// ============================================================================ processor
class PlateVerbProcessor : public AudioProcessor
{
public:
    PlateVerbProcessor()
        : AudioProcessor (BusesProperties()
              .withInput ("In", AudioChannelSet::stereo(), true)
              .withOutput ("Out", AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "PARAMS", createLayout())
    {
        auto P = [this] (const String& id) { return apvts.getRawParameterValue (id); };
        pOs = P ("os");
        for (int a = 0; a < pv::kNumAlgos; ++a) { pOn[a] = P (algoId ("on", a)); pLv[a] = P (algoId ("lv", a)); }
        pDecay = P ("decay"); pSize = P ("size"); pPre = P ("predelay"); pDamp = P ("damping"); pTone = P ("tone");
        pMod = P ("mod"); pEarly = P ("early"); pMix = P ("mix");
        for (int b = 0; b < 4; ++b) { pEqF[b] = P (algoId ("eqF", b)); pEqG[b] = P (algoId ("eqG", b)); }
        pCpOn = P ("cpOn"); pCpThr = P ("cpThr"); pCpRatio = P ("cpRatio"); pCpAtk = P ("cpAtk"); pCpRel = P ("cpRel");
        pCpMakeup = P ("cpMakeup"); pCpMix = P ("cpMix");
        pDlOn = P ("dlOn"); pDlSync = P ("dlSync"); pDlPing = P ("dlPing"); pDlTimeL = P ("dlTimeL"); pDlTimeR = P ("dlTimeR");
        pDlDivL = P ("dlDivL"); pDlDivR = P ("dlDivR"); pDlFb = P ("dlFb"); pDlMix = P ("dlMix");
        pDlLow = P ("dlLow"); pDlMid = P ("dlMid"); pDlMidF = P ("dlMidF"); pDlHigh = P ("dlHigh");
        pTrOn = P ("trOn"); pTrModel = P ("trModel"); pTrFreq = P ("trFreq"); pTrAmt = P ("trAmt"); pOut = P ("out");
    }

    // unit "%" shows a 0..1 parameter as 0..100 %
    static std::unique_ptr<AudioParameterFloat> fp (const String& id, const String& name, float lo, float hi,
                                                    float def, float centre = -1.0f, const String& unit = "%", int decimals = 0)
    {
        NormalisableRange<float> r (lo, hi);
        if (centre > lo && centre < hi) r.setSkewForCentre (centre);
        const bool pct = unit == "%";
        auto attr = AudioParameterFloatAttributes()
            .withLabel (unit)
            .withStringFromValueFunction ([pct, decimals] (float v, int) { return pct ? String (roundToInt (v * 100.0f)) : String (v, decimals); })
            .withValueFromStringFunction ([pct] (const String& t) { return pct ? t.getFloatValue() / 100.0f : t.getFloatValue(); });
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def, attr);
    }
    static std::unique_ptr<AudioParameterBool> bp (const String& id, const String& name, bool def)
    {
        return std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def);
    }
    static std::unique_ptr<AudioParameterChoice> cp (const String& id, const String& name, const StringArray& c, int def)
    {
        return std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, c, def);
    }

    static APVTS::ParameterLayout createLayout()
    {
        APVTS::ParameterLayout l;
        l.add (cp ("os", "Oversampling", osNames(), 0));
        for (int a = 0; a < pv::kNumAlgos; ++a)
        {
            String nm = String (kAlgoNames[a]).toLowerCase(); nm = nm.substring (0, 1).toUpperCase() + nm.substring (1);
            l.add (bp (algoId ("on", a), nm + " On", a == 0));
            l.add (fp (algoId ("lv", a), nm + " Level", 0.0f, 1.0f, 1.0f));
        }
        l.add (fp ("decay", "Decay", 0.2f, 20.0f, 2.5f, 2.5f, "s", 2));
        l.add (fp ("size", "Size", 0.5f, 1.5f, 1.0f, -1.0f, "x", 2));
        l.add (fp ("predelay", "Pre-Delay", 0.0f, 250.0f, 10.0f, 40.0f, "ms"));
        l.add (fp ("damping", "Damping", 0.0f, 1.0f, 0.35f));
        l.add (fp ("tone", "Input Tone", 0.0f, 1.0f, 0.75f));
        l.add (fp ("mod", "Modulation", 0.0f, 1.0f, 0.5f));
        l.add (fp ("early", "Early Reflections", 0.0f, 1.0f, 0.7f));
        l.add (fp ("mix", "Reverb Mix", 0.0f, 1.0f, 0.3f));
        const char* bandNames[] = { "Low", "Low Mid", "High Mid", "High" };
        const float fLo[] = { 20, 100, 800, 2000 }, fHi[] = { 500, 2000, 8000, 20000 }, fDef[] = { 100, 400, 2500, 8000 };
        for (int b = 0; b < 4; ++b)
        {
            l.add (fp (algoId ("eqF", b), String ("Reverb EQ ") + bandNames[b] + " Freq", fLo[b], fHi[b], fDef[b], fDef[b], "Hz"));
            l.add (fp (algoId ("eqG", b), String ("Reverb EQ ") + bandNames[b] + " Gain", -15.0f, 15.0f, 0.0f, -1.0f, "dB", 1));
        }
        l.add (bp ("cpOn", "Comp On", false));
        l.add (fp ("cpThr", "Comp Threshold", -60.0f, 0.0f, -20.0f, -1.0f, "dB"));
        l.add (fp ("cpRatio", "Comp Ratio", 1.0f, 20.0f, 3.0f, 4.0f, ":1", 1));
        l.add (fp ("cpAtk", "Comp Attack", 0.1f, 200.0f, 10.0f, 10.0f, "ms", 1));
        l.add (fp ("cpRel", "Comp Release", 10.0f, 3000.0f, 200.0f, 200.0f, "ms"));
        l.add (fp ("cpMakeup", "Comp Makeup", 0.0f, 24.0f, 0.0f, -1.0f, "dB", 1));
        l.add (fp ("cpMix", "Comp Mix", 0.0f, 1.0f, 1.0f));
        l.add (bp ("dlOn", "Delay On", false));
        l.add (bp ("dlSync", "Delay Sync", true));
        l.add (bp ("dlPing", "Delay Ping-Pong", false));
        l.add (fp ("dlTimeL", "Delay Time L", 1.0f, 2990.0f, 375.0f, 400.0f, "ms"));
        l.add (fp ("dlTimeR", "Delay Time R", 1.0f, 2990.0f, 500.0f, 400.0f, "ms"));
        l.add (cp ("dlDivL", "Delay Div L", divNames(), 10));
        l.add (cp ("dlDivR", "Delay Div R", divNames(), 8));
        l.add (fp ("dlFb", "Delay Feedback", 0.0f, 0.95f, 0.35f));
        l.add (fp ("dlMix", "Delay Mix", 0.0f, 1.0f, 0.3f));
        l.add (fp ("dlLow", "Delay EQ Low", -12.0f, 12.0f, 0.0f, -1.0f, "dB", 1));
        l.add (fp ("dlMid", "Delay EQ Mid", -12.0f, 12.0f, 0.0f, -1.0f, "dB", 1));
        l.add (fp ("dlMidF", "Delay EQ Mid Freq", 200.0f, 5000.0f, 1000.0f, 1000.0f, "Hz"));
        l.add (fp ("dlHigh", "Delay EQ High", -12.0f, 12.0f, 0.0f, -1.0f, "dB", 1));
        l.add (bp ("trOn", "Transformer On", false));
        l.add (cp ("trModel", "Transformer Model", trNames(), 0));
        l.add (fp ("trFreq", "Transformer Freq", 20.0f, 400.0f, 80.0f, 80.0f, "Hz"));
        l.add (fp ("trAmt", "Transformer Amount", 0.0f, 1.0f, 0.3f));
        l.add (fp ("out", "Output", -24.0f, 12.0f, 0.0f, -1.0f, "dB", 1));
        return l;
    }

    void prepareToPlay (double sampleRate, int) override
    {
        engine.prepare (sampleRate);
        readParams();
        engine.setOversampling (prm.oversampling, true);
        setLatencySamples (engine.latency());
    }
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& b) const override
    {
        const auto in = b.getMainInputChannelSet(), out = b.getMainOutputChannelSet();
        return out == AudioChannelSet::stereo() && (in == AudioChannelSet::stereo() || in == AudioChannelSet::mono());
    }

    void processBlock (AudioBuffer<float>& buffer, MidiBuffer&) override
    {
        ScopedNoDenormals noDenormals;
        const int n = buffer.getNumSamples();
        if (buffer.getNumChannels() < 2 || n == 0) return;
        if (getTotalNumInputChannels() == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
        readParams();
        engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, prm);
        compGr.store (prm.compOn ? engine.compReductionDb() : 0.0f);
        const int lat = engine.latency();
        if (lat != getLatencySamples()) setLatencySamples (lat);
    }

    void readParams()
    {
        auto f = [] (std::atomic<float>* p) { return p->load(); };
        auto on = [] (std::atomic<float>* p) { return p->load() > 0.5f; };
        prm.oversampling = kOsValues[jlimit (0, 4, (int) f (pOs))];
        for (int a = 0; a < pv::kNumAlgos; ++a) { prm.algoOn[a] = on (pOn[a]); prm.algoLevel[a] = f (pLv[a]); }
        prm.decaySec = f (pDecay); prm.size = f (pSize); prm.predelayMs = f (pPre); prm.damping = f (pDamp);
        prm.tone = f (pTone); prm.mod = f (pMod); prm.early = f (pEarly); prm.rvMix = f (pMix);
        for (int b = 0; b < 4; ++b) { prm.eqF[b] = f (pEqF[b]); prm.eqG[b] = f (pEqG[b]); }
        prm.compOn = on (pCpOn); prm.compThr = f (pCpThr); prm.compRatio = f (pCpRatio); prm.compAtk = f (pCpAtk);
        prm.compRel = f (pCpRel); prm.compMakeup = f (pCpMakeup); prm.compMix = f (pCpMix);
        prm.delayOn = on (pDlOn); prm.pingPong = on (pDlPing); prm.delayFb = f (pDlFb); prm.delayMix = f (pDlMix);
        prm.dlLowDb = f (pDlLow); prm.dlMidDb = f (pDlMid); prm.dlMidHz = f (pDlMidF); prm.dlHighDb = f (pDlHigh);
        if (on (pDlSync))
        {
            double bpm = 120.0;
            if (auto* ph = getPlayHead())
                if (auto pos = ph->getPosition())
                    if (auto b = pos->getBpm())
                        bpm = *b;
            bpm = jlimit (20.0, 400.0, bpm);
            const int dl = jlimit (0, 15, (int) f (pDlDivL)), dr = jlimit (0, 15, (int) f (pDlDivR));
            prm.delayMsL = jmin (2990.0f, (float) (kDivBeats[dl] * 60000.0 / bpm));
            prm.delayMsR = jmin (2990.0f, (float) (kDivBeats[dr] * 60000.0 / bpm));
        }
        else { prm.delayMsL = f (pDlTimeL); prm.delayMsR = f (pDlTimeR); }
        prm.trOn = on (pTrOn); prm.trModel = jlimit (0, pv::kNumTransformers - 1, (int) f (pTrModel));
        prm.trFreq = f (pTrFreq); prm.trAmount = f (pTrAmt); prm.outGainDb = f (pOut);
    }

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const String getName() const override { return "PlateVerb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const String getProgramName (int) override { return {}; }
    void changeProgramName (int, const String&) override {}

    void getStateInformation (MemoryBlock& dest) override
    {
        if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
    }
    void setStateInformation (const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary (data, size))
            if (xml->hasTagName (apvts.state.getType()))
                apvts.replaceState (ValueTree::fromXml (*xml));
    }

    APVTS apvts;
    std::atomic<float> compGr { 0.0f };

private:
    pv::Engine engine;
    pv::Params prm;
    std::atomic<float> *pOs = nullptr, *pOn[pv::kNumAlgos] = {}, *pLv[pv::kNumAlgos] = {}, *pDecay = nullptr, *pSize = nullptr,
        *pPre = nullptr, *pDamp = nullptr, *pTone = nullptr, *pMod = nullptr, *pEarly = nullptr, *pMix = nullptr,
        *pEqF[4] = {}, *pEqG[4] = {}, *pCpOn = nullptr, *pCpThr = nullptr, *pCpRatio = nullptr, *pCpAtk = nullptr,
        *pCpRel = nullptr, *pCpMakeup = nullptr, *pCpMix = nullptr, *pDlOn = nullptr, *pDlSync = nullptr, *pDlPing = nullptr,
        *pDlTimeL = nullptr, *pDlTimeR = nullptr, *pDlDivL = nullptr, *pDlDivR = nullptr, *pDlFb = nullptr, *pDlMix = nullptr,
        *pDlLow = nullptr, *pDlMid = nullptr, *pDlMidF = nullptr, *pDlHigh = nullptr, *pTrOn = nullptr, *pTrModel = nullptr,
        *pTrFreq = nullptr, *pTrAmt = nullptr, *pOut = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlateVerbProcessor)
};

// ============================================================================ look & feel (blue / white)
namespace col
{
    static const Colour navy    { 0xff0a1d45 };
    static const Colour panel   { 0xff1d4a9e };
    static const Colour panelHi { 0xff2a5cb8 };
    static const Colour white   { 0xfff4f7fb };
    static const Colour lcdBg   { 0xff06122b };
    static const Colour lcdText { 0xffcfe3ff };
    static const Colour led     { 0xff3d9bff };
}

static Font uiFont (float h, bool bold = false) { return Font (FontOptions (h, bold ? Font::bold : Font::plain)); }
static Font monoFont (float h) { return Font (FontOptions (Font::getDefaultMonospacedFontName(), h, Font::bold)); }

class PVLook : public LookAndFeel_V4
{
public:
    PVLook()
    {
        setColour (ComboBox::backgroundColourId, col::navy);
        setColour (ComboBox::textColourId, col::white);
        setColour (ComboBox::outlineColourId, col::white.withAlpha (0.5f));
        setColour (ComboBox::arrowColourId, col::white);
        setColour (PopupMenu::backgroundColourId, col::navy);
        setColour (PopupMenu::textColourId, col::white);
        setColour (PopupMenu::highlightedBackgroundColourId, col::panelHi);
        setColour (PopupMenu::highlightedTextColourId, Colours::white);
        setColour (BubbleComponent::backgroundColourId, col::white);
        setColour (BubbleComponent::outlineColourId, col::navy);
        setColour (TooltipWindow::textColourId, col::navy);
    }

    void drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider&) override
    {
        auto b = Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
        const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        Path track; track.addCentredArc (c.x, c.y, r - 2, r - 2, 0.0f, a0, a1, true);
        g.setColour (col::navy);
        g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        Path val; val.addCentredArc (c.x, c.y, r - 2, r - 2, 0.0f, a0, ang, true);
        g.setColour (Colour (0xffbfe0ff));
        g.strokePath (val, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        const float kr = r - 7.0f;
        g.setColour (Colours::black.withAlpha (0.3f));
        g.fillEllipse (c.x - kr, c.y - kr + 2.0f, kr * 2, kr * 2);
        g.setGradientFill (ColourGradient (Colours::white, c.x, c.y - kr, Colour (0xffbccbe3), c.x, c.y + kr, false));
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
        g.setColour (col::navy.withAlpha (0.6f));
        g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);
        g.setColour (col::panel);
        g.drawLine (Line<float> (c.getPointOnCircumference (kr * 0.2f, ang), c.getPointOnCircumference (kr * 0.85f, ang)), 3.0f);
    }

    void drawLinearSlider (Graphics& g, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           const Slider::SliderStyle style, Slider& s) override
    {
        if (style != Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s);
            return;
        }
        const float cx = (float) x + (float) w * 0.5f;
        g.setColour (col::white.withAlpha (0.35f));
        for (int i = 0; i <= 10; ++i)
        {
            const float ty = (float) y + (float) h * (float) i / 10.0f;
            const float len = (i % 5 == 0) ? 9.0f : 5.0f;
            g.drawLine (cx - 9.0f - len, ty, cx - 9.0f, ty, 1.0f);
            g.drawLine (cx + 9.0f, ty, cx + 9.0f + len, ty, 1.0f);
        }
        g.setColour (col::lcdBg);
        g.fillRoundedRectangle (cx - 3.0f, (float) y, 6.0f, (float) h, 3.0f);
        Rectangle<float> cap (cx - 14.0f, sliderPos - 10.0f, 28.0f, 20.0f);
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 3.0f);
        g.setGradientFill (ColourGradient (Colours::white, cap.getX(), cap.getY(), Colour (0xffb9c8e0), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 3.0f);
        g.setColour (col::panel);
        g.fillRect (cap.getX() + 3.0f, cap.getCentreY() - 1.0f, cap.getWidth() - 6.0f, 2.0f);
    }

    int getSliderThumbRadius (Slider& s) override { return s.isVertical() ? 10 : LookAndFeel_V4::getSliderThumbRadius (s); }

    void drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.5f);
        const bool on = b.getToggleState();
        g.setGradientFill (ColourGradient (on ? Colours::white : Colour (0xffdfe7f3), r.getX(), r.getY(),
                                           on ? Colour (0xffd2e4ff) : Colour (0xffaebdd4), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 3.0f);
        if (over || down) { g.setColour (Colours::white.withAlpha (down ? 0.35f : 0.18f)); g.fillRoundedRectangle (r, 3.0f); }
        const float d = 7.0f;
        Rectangle<float> led (r.getX() + 7.0f, r.getCentreY() - d * 0.5f, d, d);
        if (on)
        {
            g.setColour (col::led.withAlpha (0.35f)); g.fillEllipse (led.expanded (3.0f));
            g.setColour (col::led);
        }
        else g.setColour (Colour (0xff56657f));
        g.fillEllipse (led);
        g.setColour (col::navy);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

    void drawButtonText (Graphics& g, TextButton& b, bool, bool) override
    {
        g.setColour (col::navy);
        g.setFont (uiFont (11.5f, true));
        g.drawText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (14), Justification::centred, false);
    }
};

// ============================================================================ display (double-click = service page)
class LcdDisplay : public Component
{
public:
    std::function<void()> onDoubleClick;
    StringArray lines; String badges[3]; bool badgeOn[3] = { false, false, false };

    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (r, 6.0f);
        auto in = r.reduced (4.0f);
        g.setGradientFill (ColourGradient (Colour (0xff0b1d42), in.getX(), in.getY(), col::lcdBg, in.getX(), in.getBottom(), false));
        g.fillRoundedRectangle (in, 4.0f);
        g.setColour (col::lcdText.withAlpha (0.06f));
        for (float yy = in.getY() + 2.0f; yy < in.getBottom(); yy += 3.0f) g.drawHorizontalLine ((int) yy, in.getX(), in.getRight());
        auto t = in.reduced (12.0f, 8.0f);
        for (int i = 0; i < lines.size(); ++i)
        {
            g.setColour (i == 0 ? Colours::white : col::lcdText);
            g.setFont (monoFont (i == 0 ? 17.0f : 13.5f));
            g.drawText (lines[i], t.removeFromTop (i == 0 ? 24.0f : 19.0f), Justification::centredLeft, true);
        }
        float bx = in.getRight() - 12.0f;
        for (int i = 2; i >= 0; --i)
        {
            Rectangle<float> br (bx - 54.0f, in.getY() + 8.0f, 54.0f, 16.0f); bx -= 60.0f;
            g.setColour (badgeOn[i] ? col::lcdText : col::lcdText.withAlpha (0.18f));
            g.drawRoundedRectangle (br, 2.0f, 1.0f);
            g.setFont (monoFont (10.5f));
            g.drawText (badges[i], br, Justification::centred, false);
        }
    }
    void mouseDoubleClick (const MouseEvent&) override { if (onDoubleClick) onDoubleClick(); }
};

// ============================================================================ hidden service page (transformer)
class ServicePanel : public Component
{
public:
    ServicePanel (APVTS& s) : st (s)
    {
        engage.setButtonText ("ENGAGE"); engage.setClickingTogglesState (true);
        addAndMakeVisible (engage);
        engageAtt = std::make_unique<APVTS::ButtonAttachment> (st, "trOn", engage);
        model.addItemList (trNames(), 1);
        addAndMakeVisible (model);
        modelAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, "trModel", model);
        for (auto* k : { &freq, &amount })
        {
            k->setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
            k->setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
            k->setPopupDisplayEnabled (true, true, this);
            addAndMakeVisible (*k);
        }
        freqAtt = std::make_unique<APVTS::SliderAttachment> (st, "trFreq", freq);
        amtAtt = std::make_unique<APVTS::SliderAttachment> (st, "trAmt", amount);
        close.setButtonText ("CLOSE");
        close.onClick = [this] { setVisible (false); };
        addAndMakeVisible (close);
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r.translated (0, 4), 8.0f);
        g.setColour (col::navy); g.fillRoundedRectangle (r, 8.0f);
        g.setColour (col::white.withAlpha (0.6f)); g.drawRoundedRectangle (r.reduced (1.0f), 8.0f, 1.5f);
        g.setColour (col::white); g.setFont (uiFont (14.0f, true));
        g.drawText ("SERVICE PAGE  -  OUTPUT TRANSFORMER", 18, 12, getWidth() - 36, 20, Justification::centredLeft);
        g.setColour (col::lcdText.withAlpha (0.8f)); g.setFont (uiFont (11.0f));
        g.drawText ("Models are approximations inspired by classic hardware.", 18, 32, getWidth() - 36, 16, Justification::centredLeft);
        g.setColour (col::white); g.setFont (uiFont (11.0f, true));
        g.drawText ("FREQUENCY", freq.getBounds().translated (0, freq.getHeight() - 4).withHeight (16), Justification::centred);
        g.drawText ("AMOUNT", amount.getBounds().translated (0, amount.getHeight() - 4).withHeight (16), Justification::centred);
        g.drawText ("MODEL", model.getBounds().translated (0, -18).withHeight (16), Justification::centredLeft);
    }
    void resized() override
    {
        engage.setBounds (18, 58, 110, 28);
        model.setBounds (18, 112, 250, 26);
        freq.setBounds (290, 56, 70, 70);
        amount.setBounds (370, 56, 70, 70);
        close.setBounds (getWidth() - 110, getHeight() - 40, 92, 26);
    }

private:
    APVTS& st;
    TextButton engage, close; ComboBox model; Slider freq, amount;
    std::unique_ptr<APVTS::ButtonAttachment> engageAtt;
    std::unique_ptr<APVTS::ComboBoxAttachment> modelAtt;
    std::unique_ptr<APVTS::SliderAttachment> freqAtt, amtAtt;
};

// ============================================================================ editor
class PlateVerbEditor : public AudioProcessorEditor, private Timer
{
public:
    explicit PlateVerbEditor (PlateVerbProcessor& p) : AudioProcessorEditor (&p), proc (p), service (p.apvts)
    {
        setLookAndFeel (&look);
        // algorithms
        for (int a = 0; a < pv::kNumAlgos; ++a)
        {
            addButton (algoId ("on", a), kAlgoNames[a]);
            addSlider (algoId ("lv", a), "LEVEL", true);
        }
        // reverb faders
        const char* rv[][2] = { { "decay", "DECAY" }, { "size", "SIZE" }, { "predelay", "PRE-DLY" }, { "damping", "DAMPING" },
                                { "tone", "TONE" }, { "mod", "MOD" }, { "early", "EARLY" }, { "mix", "MIX" } };
        for (auto& e : rv) addSlider (e[0], e[1], true);
        // reverb EQ
        const char* bn[] = { "LOW", "LO-MID", "HI-MID", "HIGH" };
        for (int b = 0; b < 4; ++b) { addSlider (algoId ("eqF", b), String (bn[b]) + " HZ", false); addSlider (algoId ("eqG", b), String (bn[b]) + " dB", false); }
        // compressor
        addButton ("cpOn", "COMP");
        const char* cpn[][2] = { { "cpThr", "THRESH" }, { "cpRatio", "RATIO" }, { "cpAtk", "ATTACK" },
                                 { "cpRel", "RELEASE" }, { "cpMakeup", "MAKEUP" }, { "cpMix", "MIX" } };
        for (auto& e : cpn) addSlider (e[0], e[1], false);
        // delay
        addButton ("dlOn", "DELAY"); addButton ("dlSync", "SYNC"); addButton ("dlPing", "PING");
        const char* dln[][2] = { { "dlTimeL", "TIME L" }, { "dlTimeR", "TIME R" }, { "dlFb", "FEEDBACK" }, { "dlMix", "MIX" },
                                 { "dlLow", "LOW" }, { "dlMid", "MID" }, { "dlMidF", "MID HZ" }, { "dlHigh", "HIGH" } };
        for (auto& e : dln) addSlider (e[0], e[1], false);
        addCombo ("dlDivL", divNames()); addCombo ("dlDivR", divNames());
        // output
        addCombo ("os", osNames());
        addSlider ("out", "OUTPUT", false);

        display.badges[0] = "COMP"; display.badges[1] = "DELAY"; display.badges[2] = "XFMR";
        display.onDoubleClick = [this] { service.setVisible (! service.isVisible()); if (service.isVisible()) service.toFront (true); };
        addAndMakeVisible (display);
        addChildComponent (service);

        for (auto* prm : proc.getParameters()) lastValues.add (prm->getValue());
        setSize (1200, 690);
        startTimerHz (20);
    }
    ~PlateVerbEditor() override { stopTimer(); setLookAndFeel (nullptr); }

    void paint (Graphics& g) override
    {
        g.setGradientFill (ColourGradient (col::panelHi, 0, 0, col::panel, 0, (float) getHeight(), false));
        g.fillAll();
        // header
        auto head = getLocalBounds().removeFromTop (124).toFloat();
        g.setGradientFill (ColourGradient (Colour (0xff12306c), 0, 0, col::navy, 0, head.getBottom(), false));
        g.fillRect (head);
        g.setColour (col::white.withAlpha (0.6f)); g.fillRect (0.0f, head.getBottom() - 2.0f, (float) getWidth(), 2.0f);
        g.setColour (col::white); g.setFont (uiFont (30.0f, true));
        g.drawText ("P L A T E V E R B", 24, 26, 330, 40, Justification::centredLeft);
        g.setColour (col::lcdText.withAlpha (0.85f)); g.setFont (uiFont (11.5f, true));
        g.drawText ("DIGITAL REVERBERATION PROCESSOR", 26, 66, 330, 16, Justification::centredLeft);
        g.drawText ("MODEL PV-6  /  6 ALGORITHMS", 26, 84, 330, 16, Justification::centredLeft);
        // screws
        for (auto pt : { Point<float> (10, 10), Point<float> (getWidth() - 10.0f, 10), Point<float> (10, getHeight() - 10.0f),
                         Point<float> (getWidth() - 10.0f, getHeight() - 10.0f) })
        {
            g.setColour (Colour (0xffc9d4e6)); g.fillEllipse (pt.x - 5, pt.y - 5, 10, 10);
            g.setColour (col::navy); g.drawLine (pt.x - 3, pt.y, pt.x + 3, pt.y, 1.5f);
        }
        // sections
        for (auto& s : sections)
        {
            auto r = s.area.toFloat();
            g.setColour (col::navy.withAlpha (0.28f)); g.fillRoundedRectangle (r, 6.0f);
            g.setColour (col::white.withAlpha (0.45f)); g.drawRoundedRectangle (r, 6.0f, 1.0f);
            auto tab = Rectangle<float> (r.getX() + 12.0f, r.getY() - 9.0f, (float) s.title.length() * 8.2f + 18.0f, 18.0f);
            g.setColour (col::navy); g.fillRoundedRectangle (tab, 3.0f);
            g.setColour (col::white); g.setFont (uiFont (11.5f, true));
            g.drawText (s.title, tab, Justification::centred);
        }
        // labels
        g.setFont (uiFont (10.5f, true));
        for (auto& c : sliders)
        {
            if (! c->slider.isVisible()) continue;
            auto b = c->slider.getBounds();
            g.setColour (col::white);
            g.drawText (c->label, b.getX() - 10, b.getBottom() + 1, b.getWidth() + 20, 14, Justification::centred);
        }
        for (auto& c : combos)
            if (c->box.isVisible())
                g.drawText (c->label, c->box.getX(), c->box.getY() - 15, c->box.getWidth(), 14, Justification::centredLeft);
        // compressor gain-reduction meter
        auto m = grMeter.toFloat();
        g.setColour (col::lcdBg); g.fillRoundedRectangle (m, 3.0f);
        const float gr = jlimit (0.0f, 24.0f, proc.compGr.load());
        g.setColour (col::led); g.fillRoundedRectangle (m.withHeight (m.getHeight() * gr / 24.0f).reduced (2.0f, 0.0f), 2.0f);
        g.setColour (col::white); g.setFont (uiFont (10.0f, true));
        g.drawText ("GR", grMeter.getX() - 6, grMeter.getBottom() + 1, grMeter.getWidth() + 12, 14, Justification::centred);
        g.setColour (col::lcdText.withAlpha (0.8f));
        g.drawText ("ANALOG EQ", delayEqLabel, Justification::centredLeft);
    }

    void resized() override
    {
        sections.clear();
        display.setBounds (380, 12, 470, 100);
        // header right: oversampling + output
        combo ("os").setBounds (880, 46, 90, 24);
        slider ("out").setBounds (1000, 22, 62, 62);
        service.setBounds (330, 150, 470, 230);

        // algorithms
        Rectangle<int> algo (16, 140, 500, 250); addSection (algo, "ALGORITHMS");
        for (int a = 0; a < pv::kNumAlgos; ++a)
        {
            const int x = algo.getX() + 10 + a * 81;
            button (algoId ("on", a)).setBounds (x, algo.getY() + 18, 78, 28);
            slider (algoId ("lv", a)).setBounds (x + 19, algo.getY() + 56, 40, 166);
        }
        // reverb faders
        Rectangle<int> rv (528, 140, 656, 250); addSection (rv, "REVERB");
        const char* rvIds[] = { "decay", "size", "predelay", "damping", "tone", "mod", "early", "mix" };
        for (int i = 0; i < 8; ++i) slider (rvIds[i]).setBounds (rv.getX() + 22 + i * 79, rv.getY() + 22, 40, 200);

        // reverb EQ
        Rectangle<int> eq (16, 410, 340, 266); addSection (eq, "REVERB EQ");
        for (int b = 0; b < 4; ++b)
        {
            const int x = eq.getX() + 14 + b * 80;
            slider (algoId ("eqF", b)).setBounds (x, eq.getY() + 26, 58, 58);
            slider (algoId ("eqG", b)).setBounds (x, eq.getY() + 126, 58, 58);
        }
        // compressor
        Rectangle<int> cp (368, 410, 330, 266); addSection (cp, "TAIL COMPRESSOR");
        button ("cpOn").setBounds (cp.getX() + 14, cp.getY() + 20, 86, 28);
        const char* cpIds[] = { "cpThr", "cpRatio", "cpAtk", "cpRel", "cpMakeup", "cpMix" };
        for (int i = 0; i < 6; ++i)
            slider (cpIds[i]).setBounds (cp.getX() + 14 + (i % 3) * 82, cp.getY() + 62 + (i / 3) * 98, 58, 58);
        grMeter = { cp.getRight() - 46, cp.getY() + 62, 22, 156 };

        // delay
        Rectangle<int> dl (710, 410, 474, 266); addSection (dl, "STEREO DELAY");
        button ("dlOn").setBounds (dl.getX() + 14, dl.getY() + 20, 92, 28);
        button ("dlSync").setBounds (dl.getX() + 112, dl.getY() + 20, 80, 28);
        button ("dlPing").setBounds (dl.getX() + 198, dl.getY() + 20, 80, 28);
        slider ("dlTimeL").setBounds (dl.getX() + 14, dl.getY() + 70, 58, 58);
        slider ("dlTimeR").setBounds (dl.getX() + 96, dl.getY() + 70, 58, 58);
        combo ("dlDivL").setBounds (dl.getX() + 14, dl.getY() + 86, 76, 24);
        combo ("dlDivR").setBounds (dl.getX() + 96, dl.getY() + 86, 76, 24);
        slider ("dlFb").setBounds (dl.getX() + 178, dl.getY() + 70, 58, 58);
        slider ("dlMix").setBounds (dl.getX() + 260, dl.getY() + 70, 58, 58);
        const char* eqIds[] = { "dlLow", "dlMid", "dlMidF", "dlHigh" };
        for (int i = 0; i < 4; ++i) slider (eqIds[i]).setBounds (dl.getX() + 14 + i * 82, dl.getY() + 168, 58, 58);
        delayEqLabel = { dl.getX() + 350, dl.getY() + 168, 110, 58 };
    }

private:
    struct SliderCtl { Slider slider; String label; std::unique_ptr<APVTS::SliderAttachment> att; };
    struct ButtonCtl { TextButton button; std::unique_ptr<APVTS::ButtonAttachment> att; };
    struct ComboCtl  { ComboBox box; String label; std::unique_ptr<APVTS::ComboBoxAttachment> att; };
    struct Section   { Rectangle<int> area; String title; };

    void addSlider (const String& id, const String& label, bool fader)
    {
        auto c = std::make_unique<SliderCtl>();
        c->slider.setName (id);
        c->slider.setSliderStyle (fader ? Slider::LinearVertical : Slider::RotaryHorizontalVerticalDrag);
        c->slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        c->slider.setPopupDisplayEnabled (true, true, this);
        c->label = label;
        addAndMakeVisible (c->slider);
        c->att = std::make_unique<APVTS::SliderAttachment> (proc.apvts, id, c->slider);
        if (auto* prm = proc.apvts.getParameter (id))
            c->slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        sliders.push_back (std::move (c));
    }
    void addButton (const String& id, const String& text)
    {
        auto c = std::make_unique<ButtonCtl>();
        c->button.setName (id);
        c->button.setButtonText (text);
        c->button.setClickingTogglesState (true);
        addAndMakeVisible (c->button);
        c->att = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, id, c->button);
        buttons.push_back (std::move (c));
    }
    void addCombo (const String& id, const StringArray& items)
    {
        auto c = std::make_unique<ComboCtl>();
        c->box.setName (id);
        c->box.addItemList (items, 1);
        c->label = id == "os" ? "OVERSAMPLING" : (id == "dlDivL" ? "DIV L" : "DIV R");
        addAndMakeVisible (c->box);
        c->att = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, id, c->box);
        combos.push_back (std::move (c));
    }
    Slider& slider (const String& id) { for (auto& c : sliders) if (c->slider.getName() == id) return c->slider; jassertfalse; return sliders.front()->slider; }
    TextButton& button (const String& id) { for (auto& c : buttons) if (c->button.getName() == id) return c->button; jassertfalse; return buttons.front()->button; }
    ComboBox& combo (const String& id) { for (auto& c : combos) if (c->box.getName() == id) return c->box; jassertfalse; return combos.front()->box; }
    void addSection (Rectangle<int> r, const String& t) { sections.push_back ({ r, t }); }

    float raw (const String& id) { auto* v = proc.apvts.getRawParameterValue (id); return v != nullptr ? v->load() : 0.0f; }
    String text (const String& id)
    {
        if (auto* prm = proc.apvts.getParameter (id)) return prm->getCurrentValueAsText() + (prm->getLabel() == "%" || prm->getLabel() == "x" ? "" : " ") + prm->getLabel();
        return {};
    }

    void timerCallback() override
    {
        // tempo-sync: swap time knobs for note-value boxes
        const bool sync = raw ("dlSync") > 0.5f;
        slider ("dlTimeL").setVisible (! sync); slider ("dlTimeR").setVisible (! sync);
        combo ("dlDivL").setVisible (sync); combo ("dlDivR").setVisible (sync);

        // last-touched parameter
        auto& params = proc.getParameters();
        for (int i = 0; i < params.size() && i < lastValues.size(); ++i)
        {
            const float v = params[i]->getValue();
            if (v != lastValues[i]) { lastValues.set (i, v); lastTouched = i; }
        }
        StringArray algos;
        for (int a = 0; a < pv::kNumAlgos; ++a) if (raw (algoId ("on", a)) > 0.5f) algos.add (kAlgoNames[a]);
        StringArray l;
        l.add (algos.isEmpty() ? "-- NO ALGORITHM --" : algos.joinIntoString (" + "));
        l.add ("DECAY " + text ("decay") + "   PRE " + text ("predelay") + "   OS " + osNames()[(int) raw ("os")]);
        l.add ("SIZE " + text ("size") + "   MIX " + text ("mix") + "   DELAY MIX " + text ("dlMix"));
        if (lastTouched >= 0 && lastTouched < params.size())
            if (auto* rp = dynamic_cast<RangedAudioParameter*> (params[lastTouched]))
                l.add ("> " + rp->getName (40).toUpperCase() + ": " + rp->getCurrentValueAsText() + (rp->getLabel() == "%" || rp->getLabel() == "x" ? "" : " ") + rp->getLabel());
        display.lines = l;
        display.badgeOn[0] = raw ("cpOn") > 0.5f; display.badgeOn[1] = raw ("dlOn") > 0.5f; display.badgeOn[2] = raw ("trOn") > 0.5f;
        display.repaint();
        repaint (grMeter.expanded (4, 20));
    }

    PlateVerbProcessor& proc;
    PVLook look;
    LcdDisplay display;
    ServicePanel service;
    std::vector<std::unique_ptr<SliderCtl>> sliders;
    std::vector<std::unique_ptr<ButtonCtl>> buttons;
    std::vector<std::unique_ptr<ComboCtl>> combos;
    std::vector<Section> sections;
    Rectangle<int> grMeter, delayEqLabel;
    Array<float> lastValues; int lastTouched = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlateVerbEditor)
};

AudioProcessorEditor* PlateVerbProcessor::createEditor() { return new PlateVerbEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PlateVerbProcessor(); }
