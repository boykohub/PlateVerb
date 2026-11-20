// Flexiverb 480 editor (included by PluginProcessor.cpp): a LARC-style remote.
// Main display (480L text or live spectrum) with headroom meters, PROG/REG, keypad, function keys, slider display line,
// six faders with their slider-display keys, and a drawer (EQ, COMP, DELAY, SETUP, LOOK) opened by the EXPAND tab.
#include <juce_dsp/juce_dsp.h>
#include "FlexFonts.h"

// ============================================================================ fonts
struct FlexFonts
{
    Typeface::Ptr regular, medium, semibold, bold, seg;
    FlexFonts()
    {
        regular  = Typeface::createSystemTypefaceFor (flexfonts::InterRegular,  (size_t) flexfonts::InterRegularSize);
        medium   = Typeface::createSystemTypefaceFor (flexfonts::InterMedium,   (size_t) flexfonts::InterMediumSize);
        semibold = Typeface::createSystemTypefaceFor (flexfonts::InterSemiBold, (size_t) flexfonts::InterSemiBoldSize);
        bold     = Typeface::createSystemTypefaceFor (flexfonts::InterBold,     (size_t) flexfonts::InterBoldSize);
        seg      = Typeface::createSystemTypefaceFor (flexfonts::Dseg14,        (size_t) flexfonts::Dseg14Size);
    }
    static FlexFonts& get() { static FlexFonts f; return f; }
};
enum class W { Regular, Medium, Semibold, Bold };
static Font ui (float h, W w = W::Semibold, float kern = 0.0f)
{
    auto& f = FlexFonts::get();
    auto tf = w == W::Bold ? f.bold : w == W::Semibold ? f.semibold : w == W::Regular ? f.regular : f.medium;
    return Font (FontOptions (tf).withHeight (h).withKerningFactor (kern));
}
static Font segFont (float h) { return Font (FontOptions (FlexFonts::get().seg).withHeight (h)); }
static float textW (const Font& f, const String& s) { return GlyphArrangement::getStringWidth (f, s); }

// ============================================================================ look (all adjustable in LOOK)
struct Look
{
    Colour panel { 0xff3b3c3e }, sides { 0xff1b1b1c }, letters { 0xffe6e1d6 }, keys { 0xffc9c6bf }, led { 0xffff3a1e };
    float panelGrad = 0.45f, sidesGrad = 0.5f, lettersGrad = 0.2f, depth = 0.7f;
    static Colour lamp() { return Colour (0xffffb43c); }          // the 1081's amber-yellow
    static Colour ink()  { return Colour (0xff1c1d1f); }
    Colour legendOn (Colour cap) const { return cap.getPerceivedBrightness() > 0.55f ? ink() : letters; }

    void load (const Flexiverb480Processor& p)
    {
        auto c = [&p] (const char* id, Colour d) { return Colour::fromString (p.getUiProperty (id, d.toString()).toString()); };
        auto f = [&p] (const char* id, float d) { return (float) (double) p.getUiProperty (id, d); };
        panel = c ("lkPanel", panel); sides = c ("lkSides", sides); letters = c ("lkLetters", letters); keys = c ("lkKeys", keys); led = c ("lkLed", led);
        panelGrad = f ("lkPanelGrad", panelGrad); sidesGrad = f ("lkSidesGrad", sidesGrad); lettersGrad = f ("lkLettersGrad", lettersGrad); depth = f ("lkDepth", depth);
    }
    void save (Flexiverb480Processor& p) const
    {
        p.setUiProperty ("lkPanel", panel.toString()); p.setUiProperty ("lkSides", sides.toString()); p.setUiProperty ("lkLetters", letters.toString());
        p.setUiProperty ("lkKeys", keys.toString()); p.setUiProperty ("lkLed", led.toString());
        p.setUiProperty ("lkPanelGrad", panelGrad); p.setUiProperty ("lkSidesGrad", sidesGrad); p.setUiProperty ("lkLettersGrad", lettersGrad); p.setUiProperty ("lkDepth", depth);
    }
    static Look preset (int i)
    {
        Look l;
        if (i == 1) { l.panel = Colour (0xff17181a); l.sides = Colour (0xff0d0d0e); l.letters = Colour (0xffd8d4ca); l.keys = Colour (0xff3a3c40); l.led = Colour (0xffff3a1e); l.panelGrad = 0.35f; }
        if (i == 2) { l.panel = Colour (0xffa9a8a3); l.sides = Colour (0xff4a3426); l.letters = Colour (0xff1f2023); l.keys = Colour (0xffe9e6df); l.led = Colour (0xffff4a22); l.panelGrad = 0.5f; l.sidesGrad = 0.7f; }
        if (i == 3) { l.panel = Colour (0xff2e3236); l.sides = Colour (0xff5b3a24); l.letters = Colour (0xffefe9dc); l.keys = Colour (0xffb9b4a9); l.led = Colour (0xff5cf0ff); }
        return l;
    }
};

// printed lettering with optional gradient and a subtle engraving (depth)
static void drawLegend (Graphics& g, const Look& lk, const String& s, Rectangle<float> r, const Font& f, Justification j = Justification::centred, Colour base = {})
{
    const Colour c = base.isTransparent() ? lk.letters : base;
    g.setFont (f);
    if (lk.depth > 0.05f) { g.setColour (Colours::black.withAlpha (0.35f * lk.depth)); g.drawText (s, r.translated (0, 1.0f), j, false); }
    if (lk.lettersGrad > 0.01f)
        g.setGradientFill (ColourGradient (c.brighter (0.35f * lk.lettersGrad), r.getX(), r.getY(), c.darker (0.45f * lk.lettersGrad), r.getX(), r.getBottom(), false));
    else g.setColour (c);
    g.drawText (s, r, j, false);
}

// ============================================================================ keys: every key the same height, as wide as its legend, lit amber when on
class LarcKey : public Component, private Timer
{
public:
    LarcKey (const Look& l, const String& t) : lk (l), text (t) { setRepaintsOnMouseActivity (true); }
    std::function<void()> onClick, onPress, onRelease;
    void setLit (bool b) { if (b != lit) { lit = b; startTimerHz (60); } }
    bool isLit() const { return lit; }
    void setText (const String& t) { text = t; repaint(); }
    void flash() { glow = 1.0f; lit = false; startTimerHz (60); repaint(); }
    float preferredWidth (float h) const { return jmax (h * 1.05f, textW (ui (h * 0.42f), text) + h * 0.9f); }
    bool small = false;
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const float d = lk.depth, rad = 2.5f;
        // well and drop shadow
        g.setColour (Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r, rad + 1);
        auto cap = r.reduced (1.5f).withTrimmedBottom (pressed ? 0.0f : 1.5f * d).translated (0, pressed ? 1.0f : 0.0f);
        Colour c = lk.keys;
        if (glow > 0.001f) c = c.interpolatedWith (Look::lamp(), glow);
        g.setGradientFill (ColourGradient (c.brighter (0.18f + 0.15f * d), cap.getX(), cap.getY(), c.darker (0.25f + 0.25f * d), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, rad);
        g.setColour (Colours::white.withAlpha (0.28f * d)); g.drawLine (cap.getX() + 2, cap.getY() + 0.8f, cap.getRight() - 2, cap.getY() + 0.8f, 1.0f);
        g.setColour (Colours::black.withAlpha (0.35f)); g.drawRoundedRectangle (cap, rad, 0.8f);
        if (glow > 0.01f) { g.setColour (Look::lamp().withAlpha (0.35f * glow)); g.drawRoundedRectangle (cap.expanded (1.5f), rad + 1, 2.0f); }
        if (isMouseOver() && ! pressed) { g.setColour (Colours::white.withAlpha (0.07f)); g.fillRoundedRectangle (cap, rad); }
        g.setColour (glow > 0.5f ? Look::ink() : lk.legendOn (lk.keys));
        g.setFont (ui (jmin (cap.getHeight() * 0.44f, 13.0f), W::Bold, 0.04f));
        g.drawText (text, cap, Justification::centred, false);
    }
    void mouseDown (const MouseEvent&) override { pressed = true; repaint(); if (onPress) onPress(); }
    void mouseUp (const MouseEvent& e) override
    {
        pressed = false; repaint();
        if (onRelease) onRelease();
        if (getLocalBounds().contains (e.getPosition()) && onClick) onClick();
    }
private:
    void timerCallback() override
    {
        const float tgt = lit ? 1.0f : 0.0f;
        glow += (tgt - glow) * 0.35f;
        if (std::abs (tgt - glow) < 0.01f) { glow = tgt; stopTimer(); }
        repaint();
    }
    const Look& lk; String text; bool lit = false, pressed = false; float glow = 0.0f;
};

// ============================================================================ LARC fader: slot with scale, realistic cap, smooth motion on program changes
class LarcFader : public Component
{
public:
    explicit LarcFader (const Look& l) : lk (l) {}
    std::function<void (float)> onMove; std::function<void()> onBegin, onEnd, onReset;
    bool enabled = true;
    void setTarget (float t) { target = t; }
    bool animate()                                    // called at 30 Hz by the editor
    {
        if (dragging) return false;
        const float d = target - shown;
        if (std::abs (d) < 0.0008f) { if (shown != target) { shown = target; return true; } return false; }
        shown += d * 0.32f; return true;
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float cx = r.getCentreX(), top = r.getY() + 10, bot = r.getBottom() - 10;
        // scale ticks
        for (int i = 0; i <= 10; ++i)
        {
            const float y = bot - (bot - top) * i / 10.0f, w = (i % 5 == 0) ? 9.0f : 5.0f;
            g.setColour (lk.letters.withAlpha (i % 5 == 0 ? 0.55f : 0.3f));
            g.fillRect (cx - 10 - w, y - 0.5f, w, 1.0f); g.fillRect (cx + 10, y - 0.5f, w, 1.0f);
        }
        // slot
        auto slot = Rectangle<float> (cx - 3.0f, top - 4, 6.0f, bot - top + 8);
        g.setColour (Colours::black.withAlpha (0.85f)); g.fillRoundedRectangle (slot, 3);
        g.setColour (Colours::white.withAlpha (0.10f * lk.depth)); g.drawLine (slot.getX() + 0.5f, slot.getBottom(), slot.getRight() - 0.5f, slot.getBottom(), 1.0f);
        // cap
        const float y = bot - (bot - top) * shown, cw = jmin (36.0f, r.getWidth() - 6), ch = 22.0f;
        auto cap = Rectangle<float> (cx - cw / 2, y - ch / 2, cw, ch);
        g.setColour (Colours::black.withAlpha (0.45f * lk.depth)); g.fillRoundedRectangle (cap.translated (1.5f, 3.0f * lk.depth + 1.0f), 3);
        const Colour base = enabled ? Colour (0xff3d3f43) : Colour (0xff2a2b2e);
        g.setGradientFill (ColourGradient (base.brighter (0.55f), cap.getX(), cap.getY(), base.darker (0.5f), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 3);
        auto face = cap.reduced (2.5f, 3.0f);
        g.setGradientFill (ColourGradient (base.brighter (0.25f), face.getX(), face.getY(), base.darker (0.15f), face.getX(), face.getBottom(), false));
        g.fillRoundedRectangle (face, 2);
        for (int k = -2; k <= 2; ++k) { g.setColour (Colours::black.withAlpha (0.35f)); g.fillRect (face.getX() + 3, face.getCentreY() + k * 3.0f - 0.5f, face.getWidth() - 6, 1.0f); }
        g.setColour (enabled ? lk.letters.withAlpha (0.95f) : lk.letters.withAlpha (0.25f));
        g.fillRect (face.getX() + 1, face.getCentreY() - 0.75f, face.getWidth() - 2, 1.5f);
        g.setColour (Colours::white.withAlpha (0.25f)); g.drawLine (cap.getX() + 3, cap.getY() + 0.7f, cap.getRight() - 3, cap.getY() + 0.7f, 1.0f);
    }
    void mouseDown (const MouseEvent& e) override
    {
        if (! enabled) return;
        dragging = true; startShown = shown; startY = (float) e.position.y;
        if (onBegin) onBegin();
        // clicking beside the cap jumps there
        const float bot = getHeight() - 10.0f, top = 10.0f, capY = bot - (bot - top) * shown;
        if (std::abs (e.position.y - capY) > 14) { shown = jlimit (0.0f, 1.0f, (bot - e.position.y) / (bot - top)); startShown = shown; if (onMove) onMove (shown); repaint(); }
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (! enabled) return;
        const float span = getHeight() - 20.0f, fine = e.mods.isAltDown() || e.mods.isShiftDown() ? 0.1f : 1.0f;
        shown = jlimit (0.0f, 1.0f, startShown - (e.position.y - startY) / span * fine);
        target = shown; if (onMove) onMove (shown); repaint();
    }
    void mouseUp (const MouseEvent&) override { if (dragging) { dragging = false; if (onEnd) onEnd(); } }
    void mouseDoubleClick (const MouseEvent&) override { if (enabled && onReset) onReset(); }
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override
    {
        if (! enabled) return;
        if (onBegin) onBegin();
        shown = target = jlimit (0.0f, 1.0f, shown + w.deltaY * 0.05f); if (onMove) onMove (shown);
        if (onEnd) onEnd(); repaint();
    }
private:
    const Look& lk; float shown = 0, target = 0, startShown = 0, startY = 0; bool dragging = false;
};

// ============================================================================ main display: 480L text screen or live spectrum, plus headroom meters
class LarcDisplay : public Component
{
public:
    explicit LarcDisplay (const Look& l) : lk (l) {}
    String line1, line2; bool live = false; float mL = 0, mR = 0, pkL = 0, pkR = 0;
    std::function<void()> onProgramClick;
    float spec[256] = {}; float peaks[256] = {};
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        // recessed window
        g.setColour (Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r, 5);
        auto glass = r.reduced (3.0f);
        g.setGradientFill (ColourGradient (Colour (0xff0b0605), glass.getX(), glass.getY(), Colour (0xff040303), glass.getX(), glass.getBottom(), false));
        g.fillRoundedRectangle (glass, 3);
        auto meters = glass.removeFromRight (64).reduced (4, 6);
        auto txt = glass.reduced (10, 6);
        if (live) paintSpectrum (g, txt);
        else
        {
            const float h = jmin (txt.getHeight() * 0.36f, 21.0f);
            auto r1 = txt.removeFromTop (txt.getHeight() * 0.5f), r2 = txt;
            segLine (g, line1, r1, h); segLine (g, line2, r2, h);
        }
        paintMeters (g, meters);
        // glass reflection
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.07f), r.getX(), r.getY(), Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.45f, false));
        g.fillRoundedRectangle (r.reduced (3.0f).withHeight (r.getHeight() * 0.45f), 3);
    }
    void mouseDown (const MouseEvent& e) override { if (e.x < getWidth() - 70 && onProgramClick) onProgramClick(); }
    void mouseMove (const MouseEvent& e) override { setMouseCursor (e.x < getWidth() - 70 ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor); }
private:
    // one line of 14-segment characters in fixed cells (like the LARC's display): unlit segments faintly visible
    void segLine (Graphics& g, const String& s, Rectangle<float> r, float h)
    {
        const auto f = segFont (h);
        g.setFont (f);
        const float cw = textW (f, "8") * 1.06f;
        const int cells = jmax (1, (int) (r.getWidth() / cw));
        const String t = s.toUpperCase();
        std::vector<std::pair<juce_wchar, bool>> cell;                 // character, decimal point after it
        for (int i = 0; i < t.length() && (int) cell.size() < cells; ++i)
        {
            const juce_wchar c = t[i];
            if ((c == '.' || c == ',') && ! cell.empty() && ! cell.back().second && cell.back().first != ' ') { cell.back().second = true; continue; }
            cell.push_back ({ c, false });
        }
        for (int i = 0; i < cells; ++i)
        {
            auto cr = Rectangle<float> (r.getX() + i * cw, r.getY(), cw, r.getHeight());
            g.setColour (lk.led.withAlpha (0.075f)); g.drawText ("~", cr, Justification::centredLeft, false);
            if (i >= (int) cell.size()) continue;
            const juce_wchar c = cell[(size_t) i].first;
            String ch = (c == ' ') ? String() : String::charToString (c);
            if (cell[(size_t) i].second) ch << ".";
            if (ch.isEmpty()) continue;
            g.setColour (lk.led.withAlpha (0.22f));                    // bloom
            for (auto o : { Point<float> (-0.8f, 0), Point<float> (0.8f, 0), Point<float> (0, 0.8f) }) g.drawText (ch, cr.translated (o.x, o.y), Justification::centredLeft, false);
            g.setColour (lk.led); g.drawText (ch, cr, Justification::centredLeft, false);
        }
    }
    void paintMeters (Graphics& g, Rectangle<float> r)
    {
        static const char* lab[] = { "-24", "", "-12", "", "0", "", "+12", "" };
        const float dbs[] = { -24, -18, -12, -6, 0, 6, 12, 15 };
        const float colW = r.getWidth() / 8.0f;
        auto bars = r.removeFromBottom (r.getHeight() - 12);
        auto lr = bars.removeFromRight (8);
        for (int i = 0; i < 8; ++i)
        {
            g.setColour (lk.led.withAlpha (0.55f)); g.setFont (ui (7.5f, W::Bold));
            g.drawText (lab[i], Rectangle<float> (r.getX() + i * colW - 6, r.getY(), colW + 12, 10), Justification::centred, false);
            for (int ch = 0; ch < 2; ++ch)
            {
                const float lv = 20.0f * std::log10 (jmax (1.0e-6f, ch == 0 ? mL : mR)) + 12.0f;    // headroom: +12 dB = clip
                const bool on = lv >= dbs[i];
                auto seg = Rectangle<float> (bars.getX() + i * colW + 1, bars.getY() + ch * bars.getHeight() * 0.5f + 2, colW - 2, bars.getHeight() * 0.5f - 4);
                const Colour c = i >= 6 ? Colour (0xffff2a1a) : lk.led;
                g.setColour (on ? c : c.withAlpha (0.08f)); g.fillRect (seg);
                if (on) { g.setColour (c.withAlpha (0.25f)); g.fillRect (seg.expanded (1.0f)); }
            }
        }
        g.setColour (lk.led.withAlpha (0.6f)); g.setFont (ui (7.0f, W::Bold));
        g.drawText ("L", lr.removeFromTop (lr.getHeight() * 0.5f), Justification::centred, false);
        g.drawText ("R", lr, Justification::centred, false);
    }
    void paintSpectrum (Graphics& g, Rectangle<float> r)
    {
        // grid
        g.setColour (lk.led.withAlpha (0.06f));
        for (double f : { 100.0, 1000.0, 10000.0 }) { const float x = r.getX() + r.getWidth() * (float) (std::log10 (f / 20.0) / 3.0); g.drawVerticalLine ((int) x, r.getY(), r.getBottom()); }
        for (int k = 1; k < 4; ++k) g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * k / 4.0f), r.getX(), r.getRight());
        Path p, pk; const int N = 256;
        p.startNewSubPath (r.getX(), r.getBottom());
        for (int i = 0; i < N; ++i)
        {
            const float x = r.getX() + r.getWidth() * i / (float) (N - 1);
            p.lineTo (x, r.getBottom() - r.getHeight() * jlimit (0.0f, 1.0f, spec[i]));
            const float yp = r.getBottom() - r.getHeight() * jlimit (0.0f, 1.0f, peaks[i]);
            if (i == 0) pk.startNewSubPath (x, yp); else pk.lineTo (x, yp);
        }
        p.lineTo (r.getRight(), r.getBottom()); p.closeSubPath();
        g.setGradientFill (ColourGradient (lk.led.withAlpha (0.55f), r.getX(), r.getY(), lk.led.withAlpha (0.08f), r.getX(), r.getBottom(), false));
        g.fillPath (p);
        g.setColour (lk.led.withAlpha (0.9f)); g.strokePath (p, PathStrokeType (1.2f));
        g.setColour (lk.led.withAlpha (0.35f)); g.strokePath (pk, PathStrokeType (0.8f));
        g.setColour (lk.led.withAlpha (0.55f)); g.setFont (ui (8.0f, W::Bold));
        g.drawText ("100", Rectangle<float> (r.getX() + r.getWidth() / 3.0f + 2, r.getBottom() - 10, 30, 10), Justification::left, false);
        g.drawText ("1K", Rectangle<float> (r.getX() + r.getWidth() * 2.0f / 3.0f + 2, r.getBottom() - 10, 30, 10), Justification::left, false);
        g.drawText ("10K", Rectangle<float> (r.getRight() - 22, r.getBottom() - 10, 22, 10), Justification::left, false);
    }
    const Look& lk;
};

// slider display line: the six parameter labels of the page
class LabelLine : public Component
{
public:
    explicit LabelLine (const Look& l) : lk (l) {}
    String labels[6]; int hi = -1;
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r, 4);
        auto glass = r.reduced (2.5f);
        g.setColour (Colour (0xff070404)); g.fillRoundedRectangle (glass, 3);
        const float w = glass.getWidth() / 6.0f, h = jmin (glass.getHeight() * 0.55f, 15.0f);
        for (int i = 0; i < 6; ++i)
        {
            auto c = Rectangle<float> (glass.getX() + i * w, glass.getY(), w, glass.getHeight());
            g.setFont (segFont (h));
            g.setColour (lk.led.withAlpha (0.07f)); g.drawText ("~~~~", c, Justification::centred, false);
            if (labels[i].isEmpty()) continue;
            g.setColour (lk.led.withAlpha (0.25f)); g.drawText (labels[i], c.translated (0.7f, 0), Justification::centred, false);
            g.setColour (i == hi ? lk.led.brighter (0.4f) : lk.led); g.drawText (labels[i], c, Justification::centred, false);
        }
    }
private:
    const Look& lk;
};

// ============================================================================ drawer knob (realistic skirted knob) and a small readout
class Knob : public Component, private AudioProcessorValueTreeState::Listener, private AsyncUpdater
{
public:
    Knob (const Look& l, AudioProcessorValueTreeState& s, const String& id, const String& label) : lk (l), st (s), pid (id), name (label)
    {
        param = st.getParameter (pid); st.addParameterListener (pid, this);
        setRepaintsOnMouseActivity (true);
    }
    ~Knob() override { st.removeParameterListener (pid, this); }
    std::function<String()> textFn;
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto lab = r.removeFromBottom (13), val = r.removeFromBottom (12);
        const float d = jmin (r.getWidth(), r.getHeight()) - 6; auto k = Rectangle<float> (d, d).withCentre (r.getCentre());
        const float v = param->getValue(), a0 = MathConstants<float>::pi * 1.25f, ang = -a0 + v * a0 * 2.0f;
        // scale dots
        for (int i = 0; i <= 10; ++i)
        {
            const float aa = -a0 + i / 10.0f * a0 * 2.0f;
            g.setColour (lk.letters.withAlpha (0.45f)); g.fillEllipse (Rectangle<float> (2, 2).withCentre (k.getCentre().getPointOnCircumference (d * 0.5f + 3.0f, aa)));
        }
        // shadow, skirt, cap
        g.setColour (Colours::black.withAlpha (0.5f * lk.depth)); g.fillEllipse (k.translated (1.0f, 2.5f * lk.depth));
        g.setGradientFill (ColourGradient (Colour (0xff4a4c50), k.getX(), k.getY(), Colour (0xff141516), k.getRight(), k.getBottom(), false)); g.fillEllipse (k);
        auto cap = k.reduced (d * 0.14f);
        g.setGradientFill (ColourGradient (Colour (0xff6a6d72), cap.getX(), cap.getY(), Colour (0xff222326), cap.getRight(), cap.getBottom(), false)); g.fillEllipse (cap);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.25f), cap.getCentreX(), cap.getY(), Colours::transparentWhite, cap.getCentreX(), cap.getCentreY(), false)); g.fillEllipse (cap.reduced (2));
        g.setColour (Look::lamp()); g.drawLine ({ k.getCentre().getPointOnCircumference (d * 0.12f, ang), k.getCentre().getPointOnCircumference (d * 0.46f, ang) }, 2.2f);
        g.setColour (lk.letters.withAlpha (0.85f)); g.setFont (ui (9.5f, W::Medium));
        g.drawText (textFn ? textFn() : param->getCurrentValueAsText(), val, Justification::centred, false);
        drawLegend (g, lk, name, lab, ui (9.5f, W::Bold, 0.05f));
    }
    void mouseDown (const MouseEvent&) override { start = param->getValue(); param->beginChangeGesture(); }
    void mouseDrag (const MouseEvent& e) override
    {
        const float fine = e.mods.isAltDown() || e.mods.isShiftDown() ? 0.1f : 1.0f;
        param->setValueNotifyingHost (jlimit (0.0f, 1.0f, start - (float) e.getDistanceFromDragStartY() * 0.005f * fine + (float) e.getDistanceFromDragStartX() * 0.002f * fine));
    }
    void mouseUp (const MouseEvent&) override { param->endChangeGesture(); }
    void mouseDoubleClick (const MouseEvent&) override { param->beginChangeGesture(); param->setValueNotifyingHost (param->getDefaultValue()); param->endChangeGesture(); }
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override { param->beginChangeGesture(); param->setValueNotifyingHost (jlimit (0.0f, 1.0f, param->getValue() + w.deltaY * 0.04f)); param->endChangeGesture(); }
private:
    void parameterChanged (const String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { repaint(); }
    const Look& lk; AudioProcessorValueTreeState& st; String pid, name; RangedAudioParameter* param = nullptr; float start = 0;
};

// a key bound to a bool or choice parameter (lit = on / = this choice)
class ParamKey : public LarcKey, private AudioProcessorValueTreeState::Listener, private AsyncUpdater
{
public:
    ParamKey (const Look& l, AudioProcessorValueTreeState& s, const String& id, const String& t, int choiceIndex = -1)
        : LarcKey (l, t), st (s), pid (id), choice (choiceIndex)
    {
        param = st.getParameter (pid); st.addParameterListener (pid, this);
        onClick = [this]
        {
            param->beginChangeGesture();
            if (choice < 0) param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f);
            else param->setValueNotifyingHost (param->convertTo0to1 ((float) choice));
            param->endChangeGesture();
        };
        handleAsyncUpdate();
    }
    ~ParamKey() override { st.removeParameterListener (pid, this); }
private:
    void parameterChanged (const String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override
    {
        const float v = param->convertFrom0to1 (param->getValue());
        setLit (choice < 0 ? v > 0.5f : std::lround (v) == choice);
    }
    AudioProcessorValueTreeState& st; String pid; int choice; RangedAudioParameter* param = nullptr;
};

struct FlexLnf : public LookAndFeel_V4
{
    Font getComboBoxFont (ComboBox&) override { return ui (12.5f, W::Medium); }
    Font getPopupMenuFont() override { return ui (13.5f, W::Medium); }
    Font getAlertWindowMessageFont() override { return ui (13.5f, W::Medium); }
    Font getTextButtonFont (TextButton&, int) override { return ui (12.5f, W::Semibold); }
};

// ============================================================================ the editor
class Flexiverb480Editor : public AudioProcessorEditor, private Timer
{
public:
    static constexpr int kLarcW = 452, kLarcH = 800, kDrawerW = 372, kTabW = 22;

    explicit Flexiverb480Editor (Flexiverb480Processor& p) : AudioProcessorEditor (p), proc (p), display (look), labels (look), fft (11)
    {
        look.load (proc);
        live = (bool) proc.getUiProperty ("uiLive", false);
        drawerOpen = (bool) proc.getUiProperty ("uiDrawer", false);
        drawerTab = (int) proc.getUiProperty ("uiTab", 0);
        scale = (float) (double) proc.getUiProperty ("uiScale", 1.0);
        machine = 0;

        addAndMakeVisible (content);
        content.addAndMakeVisible (display);
        display.onProgramClick = [this] { showProgramMenu(); };
        content.addAndMakeVisible (labels);

        auto mk = [this] (const String& t) { auto k = std::make_unique<LarcKey> (look, t); content.addAndMakeVisible (*k); return k; };
        progKey = mk ("PROG"); regKey = mk ("REG");
        progKey->onClick = [this] { pressProg(); };
        regKey->onClick = [this] { showRegisterMenu(); };
        for (int i = 0; i < 10; ++i) { numKey[i] = mk (String ((i + 1) % 10)); numKey[i]->onClick = [this, i] { pressNumber (i == 9 ? 0 : i + 1); }; }
        const char* fn[] = { "BANK", "VAR", "STO", "CTRL", "MACH", "MUTE", "LIVE", "PAGE" };
        for (int i = 0; i < 8; ++i) fnKey[i] = mk (fn[i]);
        fnKey[0]->onClick = [this] { pressBank(); };
        fnKey[1]->onClick = [this] { mode = Mode::Normal; showMessage (machineLabel() + " " + progText (machine), "PAGE " + String (page + 1) + " OF " + String (jmax (1, pages()))); };
        fnKey[2]->onClick = [this] { storeRegister(); };
        fnKey[3]->onClick = [this] { openDrawer (3); };
        fnKey[4]->onClick = [this] { pressMach(); };
        fnKey[5]->onPress = [this] { proc.mute.store (true); fnKey[5]->setLit (true); };
        fnKey[5]->onRelease = [this] { proc.mute.store (false); fnKey[5]->setLit (false); };
        fnKey[6]->onClick = [this] { live = ! live; proc.setUiProperty ("uiLive", live); display.live = live; display.repaint(); fnKey[6]->setLit (live); };
        fnKey[7]->onClick = [this] { pressPage(); };
        for (int i = 0; i < 6; ++i)
        {
            fader[i] = std::make_unique<LarcFader> (look); content.addAndMakeVisible (*fader[i]);
            fader[i]->onBegin = [this, i] { proc.beginSlot (machine, page * 6 + i); focus = i; };
            fader[i]->onMove = [this, i] (float t) { proc.setSlot (machine, page * 6 + i, t); focus = i; focusTime = Time::getMillisecondCounter(); };
            fader[i]->onEnd = [this, i] { proc.endSlot (machine, page * 6 + i); };
            fader[i]->onReset = [this, i] { resetSlot (i); };
            sdKey[i] = mk (""); sdKey[i]->onClick = [this, i] { pressSlotKey (i); };
        }
        // drawer
        const char* tabs[] = { "EQ", "COMP", "DELAY", "SETUP", "LOOK" };
        for (int i = 0; i < 5; ++i) { tabKey[i] = std::make_unique<LarcKey> (look, tabs[i]); drawer.addAndMakeVisible (*tabKey[i]); tabKey[i]->onClick = [this, i] { openDrawer (i); }; }
        addAndMakeVisible (drawer);
        buildDrawer();

        display.live = live; fnKey[6]->setLit (live);
        lnf.setColour (PopupMenu::backgroundColourId, Colour (0xff1c1d1f)); lnf.setColour (PopupMenu::textColourId, Colour (0xffe8e6e1));
        lnf.setColour (PopupMenu::highlightedBackgroundColourId, Look::lamp()); lnf.setColour (PopupMenu::highlightedTextColourId, Colour (0xff1a1a1a));
        lnf.setColour (PopupMenu::headerTextColourId, Colour (0xff9a968e));
        lnf.setColour (ComboBox::backgroundColourId, Colour (0xff141516)); lnf.setColour (ComboBox::textColourId, Colour (0xffe8e6e1));
        lnf.setColour (ComboBox::outlineColourId, Colours::black); lnf.setColour (ComboBox::arrowColourId, Look::lamp());
        lnf.setColour (AlertWindow::backgroundColourId, Colour (0xff222325)); lnf.setColour (TextEditor::backgroundColourId, Colour (0xff111213));
        lnf.setDefaultSansSerifTypeface (FlexFonts::get().medium);
        setLookAndFeel (&lnf);
        layoutLarc();
        setResizable (false, false);
        applySize();
        startTimerHz (30);
        refreshPage (true);
    }
    ~Flexiverb480Editor() override { stopTimer(); setLookAndFeel (nullptr); }

    // ---------------------------------------------------------------- painting
    void paint (Graphics& g) override { g.fillAll (Colour (0xff0c0c0d)); }

    class Content : public Component
    {
    public:
        Flexiverb480Editor* ed = nullptr;
        void paint (Graphics& g) override { if (ed) ed->paintLarc (g); }
    };
    class Drawer : public Component
    {
    public:
        Flexiverb480Editor* ed = nullptr;
        void paint (Graphics& g) override { if (ed) ed->paintDrawer (g); }
        void mouseDown (const MouseEvent& e) override { if (! ed) return; if (e.x < kTabW) ed->toggleDrawer(); else ed->drawerClick (e.getPosition()); }
    };

    void paintLarc (Graphics& g)
    {
        auto r = Rectangle<float> (0, 0, (float) kLarcW, (float) kLarcH);
        // side panels (cheeks)
        const float sw = 16.0f;
        for (auto sr : { r.withWidth (sw), r.withLeft (r.getRight() - sw) })
        {
            g.setGradientFill (ColourGradient (look.sides.brighter (0.3f * look.sidesGrad), sr.getX(), sr.getY(), look.sides.darker (0.6f * look.sidesGrad), sr.getRight(), sr.getBottom(), false));
            g.fillRect (sr);
            g.setColour (Colours::white.withAlpha (0.08f * look.depth)); g.drawVerticalLine ((int) sr.getX() + 1, sr.getY(), sr.getBottom());
            g.setColour (Colours::black.withAlpha (0.5f)); g.drawVerticalLine ((int) sr.getRight() - 1, sr.getY(), sr.getBottom());
        }
        auto face = r.reduced (sw, 0);
        g.setGradientFill (ColourGradient (look.panel.brighter (0.3f * look.panelGrad), face.getX(), face.getY(), look.panel.darker (0.45f * look.panelGrad), face.getX(), face.getBottom(), false));
        g.fillRect (face);
        if (texture.isNull() || texture.getWidth() != (int) face.getWidth()) makeTexture ((int) face.getWidth(), (int) face.getHeight());
        g.setOpacity (1.0f); g.drawImageAt (texture, (int) face.getX(), 0);
        // bevelled frame and the inner panel outline of the LARC drawing
        g.setColour (Colours::white.withAlpha (0.10f * look.depth)); g.drawLine (face.getX(), 1, face.getRight(), 1, 1.5f);
        g.setColour (Colours::black.withAlpha (0.5f * look.depth)); g.drawLine (face.getX(), face.getBottom() - 1, face.getRight(), face.getBottom() - 1, 2.0f);
        auto inner = face.reduced (10, 10);
        g.setColour (Colours::black.withAlpha (0.35f)); g.drawRoundedRectangle (inner, 10, 1.2f);
        g.setColour (Colours::white.withAlpha (0.06f * look.depth)); g.drawRoundedRectangle (inner.translated (0, 1), 10, 1.0f);
        // logo and section legends
        {
            auto la = logoArea; auto top = la.removeFromTop (la.getHeight() * 0.6f);
            const auto f1 = ui (19.0f, W::Bold, -0.02f), f2 = ui (19.0f, W::Regular);
            const float w1 = textW (f1, "flexiverb"), w2 = textW (f2, " 480"), x = top.getCentreX() - (w1 + w2) / 2;
            drawLegend (g, look, "flexiverb", Rectangle<float> (x, top.getY(), w1 + 2, top.getHeight()), f1, Justification::centredLeft);
            drawLegend (g, look, " 480", Rectangle<float> (x + w1, top.getY(), w2 + 4, top.getHeight()), f2, Justification::centredLeft, look.letters.withAlpha (0.6f));
            drawLegend (g, look, machineLegend(), la, ui (8.5f, W::Bold, 0.14f), Justification::centredTop, look.letters.withAlpha (0.6f));
        }
        for (int i = 0; i < 6; ++i)
        {
            auto b = fader[i]->getBounds().toFloat();
            drawLegend (g, look, String (i + 1), Rectangle<float> (b.getX(), b.getBottom() + 1, b.getWidth(), 10), ui (8.0f, W::Bold), Justification::centred, look.letters.withAlpha (0.5f));
        }
    }
    void paintDrawer (Graphics& g)
    {
        auto r = drawer.getLocalBounds().toFloat();
        auto tab = r.removeFromLeft ((float) kTabW);
        // EXPAND tab: a strip with a small sign
        g.setGradientFill (ColourGradient (look.sides.brighter (0.25f), tab.getX(), tab.getY(), look.sides.darker (0.4f), tab.getRight(), tab.getY(), false));
        g.fillRect (tab);
        g.setColour (Colours::black.withAlpha (0.5f)); g.drawVerticalLine ((int) tab.getRight() - 1, tab.getY(), tab.getBottom());
        {
            Graphics::ScopedSaveState ss (g);
            g.addTransform (AffineTransform::rotation (-MathConstants<float>::halfPi, tab.getCentreX(), tab.getCentreY()));
            auto t = Rectangle<float> (90, 14).withCentre (tab.getCentre());
            g.setColour (drawerOpen ? Look::lamp() : look.letters.withAlpha (0.8f)); g.setFont (ui (9.0f, W::Bold, 0.2f));
            g.drawText ("EXPAND", t, Justification::centred, false);
        }
        if (! drawerOpen) return;
        g.setGradientFill (ColourGradient (look.panel.brighter (0.2f * look.panelGrad), r.getX(), r.getY(), look.panel.darker (0.5f * look.panelGrad), r.getX(), r.getBottom(), false));
        g.fillRect (r);
        if (! texture.isNull()) g.drawImageAt (texture, (int) r.getX(), 0);
        auto inner = r.reduced (10, 10);
        g.setColour (Colours::black.withAlpha (0.35f)); g.drawRoundedRectangle (inner, 10, 1.2f);
        auto sr = r.withLeft (r.getRight() - 16);
        g.setGradientFill (ColourGradient (look.sides.brighter (0.3f * look.sidesGrad), sr.getX(), sr.getY(), look.sides.darker (0.6f * look.sidesGrad), sr.getRight(), sr.getBottom(), false));
        g.fillRect (sr);
        for (auto& s : sections) drawLegend (g, look, s.text, s.area, ui (9.5f, W::Bold, 0.14f), Justification::centredLeft, look.letters.withAlpha (0.75f));
        if (drawerTab == 0) paintMiniSpectrum (g, specArea);
        if (drawerTab == 1) paintGrMeter (g, grArea);
        if (drawerTab == 4) paintSwatches (g);
    }

    // ---------------------------------------------------------------- layout
    void applySize()
    {
        const int w = kLarcW + kTabW + (drawerOpen ? kDrawerW : 0);
        content.setBounds (0, 0, kLarcW, kLarcH);
        drawer.setBounds (kLarcW, 0, kTabW + (drawerOpen ? kDrawerW : 0), kLarcH);
        content.setTransform (AffineTransform::scale (scale)); drawer.setTransform (AffineTransform::scale (scale));
        setSize (roundToInt (w * scale), roundToInt (kLarcH * scale));
        layoutDrawer();
        repaint();
    }
    void resized() override {}
    void layoutLarc()
    {
        const int x0 = 30, x1 = kLarcW - 30, wAll = x1 - x0;
        display.setBounds (x0, 28, wAll, 104);
        const int kh = 26;
        int y = 152;
        progKey->setBounds (x0, y, 52, kh); regKey->setBounds (x0, y + kh + 6, 52, kh);
        const int kw = 34, gap = 6, kx = x1 - (kw * 5 + gap * 4);
        for (int i = 0; i < 10; ++i) numKey[i]->setBounds (kx + (i % 5) * (kw + gap), y + (i / 5) * (kh + 6), kw, kh);
        logoArea = Rectangle<float> ((float) x0 + 64, (float) y, (float) (kx - x0 - 72), (float) (2 * kh + 6));
        y += 2 * (kh + 6) + 10;
        // function keys: same height, each as wide as its legend, spread over the row
        float tot = 0; for (auto& k : fnKey) tot += k->preferredWidth ((float) kh);
        const float sp = (wAll - tot) / 7.0f; float fx = (float) x0;
        for (auto& k : fnKey) { const float kw2 = k->preferredWidth ((float) kh); k->setBounds ((int) fx, y, (int) kw2, kh); fx += kw2 + sp; }
        y += kh + 18;
        labels.setBounds (x0, y, wAll, 30);
        y += 40;
        const int fh = kLarcH - y - 92, colW = wAll / 6;
        for (int i = 0; i < 6; ++i) fader[i]->setBounds (x0 + i * colW, y, colW, fh);
        y += fh + 16;
        for (int i = 0; i < 6; ++i) sdKey[i]->setBounds (x0 + i * colW + colW / 2 - 20, y, 40, kh);
    }

    // ---------------------------------------------------------------- LARC behaviour
    enum class Mode { Normal, Bank, Prog, Page };
    int pages() const { return l480::algoDef (proc.algoOf (machine)).pages; }
    String machineLabel() const { return machine == 0 ? "A" : "B"; }
    String machineLegend() const
    {
        static const char* cfg[] = { "SINGLE", "CASCADE", "STEREO SPLIT", "MONO SPLIT" };
        const int c = (int) proc.apvts.getRawParameterValue ("config")->load();
        return String (cfg[jlimit (0, 3, c)]) + "   MACH " + (machine == 0 ? "A" : "B");
    }
    String progText (int m) const
    {
        const auto& p = proc.prog (m);
        String s = String (p.bank) + "." + String (p.num) + " " + p.name;
        if (proc.registerName[m].isNotEmpty()) s = "R " + proc.registerName[m];
        return s;
    }
    void showMessage (const String& a, const String& b, int ms = 1600) { msg1 = a; msg2 = b; msgUntil = Time::getMillisecondCounter() + (uint32) ms; }
    void pressBank()
    {
        if (mode != Mode::Bank) { mode = Mode::Bank; browseBank = proc.prog (machine).bank; }
        else browseBank = browseBank % l480::kNumBanks + 1;
        fnKey[0]->setLit (true); progKey->setLit (false); fnKey[7]->setLit (false);
        showMessage ("BANK " + String (browseBank), l480::bankName (browseBank), 6000);
    }
    void pressProg()
    {
        if (mode == Mode::Bank || mode == Mode::Normal || mode == Mode::Page) { if (mode != Mode::Bank) browseBank = proc.prog (machine).bank; browseNum = mode == Mode::Bank ? 1 : proc.prog (machine).num; mode = Mode::Prog; }
        else { browseNum = browseNum % 10 + 1; if (l480::findProgram (browseBank, browseNum % 10) < 0) browseNum = 1; }
        fnKey[0]->setLit (false); progKey->setLit (true); fnKey[7]->setLit (false);
        const int pi = l480::findProgram (browseBank, browseNum % 10);
        showMessage ("PROG " + String (browseBank) + "." + String (browseNum % 10), pi >= 0 ? String (l480::programs()[(size_t) pi].name) : String ("-"), 6000);
    }
    void pressNumber (int n)
    {
        if (mode == Mode::Page)
        {
            const int pg = (n == 0 ? 10 : n) - 1;
            if (pg < pages()) { page = pg; refreshPage (true); }
            mode = Mode::Normal; fnKey[7]->setLit (false); msgUntil = 0; return;
        }
        const int bank = (mode == Mode::Bank || mode == Mode::Prog) ? browseBank : proc.prog (machine).bank;
        const int pi = l480::findProgram (bank, n);
        numKey[n == 0 ? 9 : n - 1]->flash();
        if (pi >= 0) { proc.loadProgram (machine, pi); page = 0; refreshPage (true); msgUntil = 0; }
        else showMessage ("NO PROGRAM", String (bank) + "." + String (n));
        mode = Mode::Normal; fnKey[0]->setLit (false); progKey->setLit (false);
    }
    void pressPage()
    {
        if (mode == Mode::Page) { page = (page + 1) % jmax (1, pages()); refreshPage (true); }
        else { mode = Mode::Page; page = (page + 1) % jmax (1, pages()); refreshPage (true); }
        fnKey[7]->setLit (true); fnKey[0]->setLit (false); progKey->setLit (false);
        showMessage ("PAGE " + String (page + 1) + " OF " + String (jmax (1, pages())), "1-" + String (jmin (10, jmax (1, pages()))) + " SELECTS A PAGE", 2500);
    }
    void pressMach()
    {
        const int c = (int) proc.apvts.getRawParameterValue ("config")->load();
        if (c == 0) { showMessage ("SINGLE CONFIG", "MACHINE A ONLY"); machine = 0; }
        else { machine = 1 - machine; page = 0; refreshPage (true); showMessage ("MACHINE " + String (machine == 0 ? "A" : "B"), progText (machine)); }
        fnKey[4]->setLit (machine == 1);
        content.repaint();
    }
    void pressSlotKey (int i)
    {
        const int slot = page * 6 + i;
        const auto& s = proc.spec (machine, slot);
        if (! s.lab) return;
        if (s.unit == l480::uKey) { proc.pressKey (machine, slot); sdKey[i]->flash(); showMessage (s.name, proc.machineStatus (machine)); return; }
        focus = i; focusTime = Time::getMillisecondCounter();
        sdKey[i]->flash();
    }
    void resetSlot (int i)
    {
        const int slot = page * 6 + i;
        const auto& p = proc.prog (machine); const auto& s = proc.spec (machine, slot);
        if (! s.lab || s.unit == l480::uKey) return;
        proc.beginSlot (machine, slot); proc.setSlot (machine, slot, l480::toNorm (s, l480::programValue (p, l480::realIndex (p.algo, slot)))); proc.endSlot (machine, slot);
    }
    void refreshPage (bool snap)
    {
        const int np = jmax (1, pages());
        if (page >= np) page = 0;
        for (int i = 0; i < 6; ++i)
        {
            const int slot = page * 6 + i;
            const auto& s = proc.spec (machine, slot);
            const bool has = pages() > 0 && s.lab != nullptr;
            labels.labels[i] = has ? String (s.lab) : String();
            fader[i]->enabled = has && s.unit != l480::uKey;
            sdKey[i]->setText (has && s.unit == l480::uKey ? String (s.lab) : String());
            fader[i]->setTarget (fader[i]->enabled ? proc.slotNorm (machine, slot) : 0.0f);
            if (snap) for (int k = 0; k < 2; ++k) fader[i]->animate();
            fader[i]->repaint();
        }
        labels.repaint();
    }

    // program menu: grouped by bank, the current one ticked; it is always what the display shows
    void showProgramMenu()
    {
        PopupMenu m; const int cur = proc.programOf (machine);
        for (int b = 1; b <= l480::kNumBanks; ++b)
        {
            PopupMenu sub; bool has = false;
            for (int i = 0; i < l480::numPrograms(); ++i)
            {
                const auto& p = l480::programs()[(size_t) i];
                if (p.bank != b) continue;
                sub.addItem (i + 1, String (p.bank) + "." + String (p.num) + "  " + p.name, true, i == cur);
                has = true;
            }
            if (has) m.addSubMenu (String (b) + "  " + l480::bankName (b), sub, true, nullptr, proc.prog (machine).bank == b);
        }
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&display).withMinimumWidth (220).withStandardItemHeight (22),
                         [this] (int r) { if (r > 0) { proc.loadProgram (machine, r - 1); page = 0; refreshPage (true); msgUntil = 0; } });
    }
    void showRegisterMenu()
    {
        PopupMenu m; auto names = proc.registerNames();
        for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, names[i] == proc.registerName[machine]);
        if (names.isEmpty()) m.addItem (-1, "(no registers stored yet - use STO)", false);
        m.addSeparator(); m.addItem (9000, "Store current program...");
        regKey->setLit (true);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (regKey.get()), [this, names] (int r)
        {
            regKey->setLit (false);
            if (r == 9000) storeRegister();
            else if (r > 0 && r <= names.size()) { proc.recallRegister (machine, names[r - 1]); page = 0; refreshPage (true); showMessage ("REGISTER", names[r - 1]); }
        });
    }
    void storeRegister()
    {
        fnKey[2]->setLit (true);
        auto* w = new AlertWindow ("Store register", "Name for this register (machine " + String (machine == 0 ? "A" : "B") + "):", MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", proc.registerName[machine].isNotEmpty() ? proc.registerName[machine] : String (proc.prog (machine).name));
        w->addButton ("Store", 1, KeyPress (KeyPress::returnKey)); w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        w->enterModalState (true, ModalCallbackFunction::create ([this, w] (int r)
        {
            fnKey[2]->setLit (false);
            if (r == 1) { const auto n = w->getTextEditorContents ("name").trim(); if (n.isNotEmpty() && proc.storeRegister (machine, n)) showMessage ("STORED", n); }
        }), true);
    }

    // ---------------------------------------------------------------- drawer
    struct Section { String text; Rectangle<float> area; };
    std::vector<Section> sections;
    std::vector<std::unique_ptr<Component>> drawerItems[5];
    Rectangle<float> specArea, grArea; Rectangle<int> swatchArea[5];
    std::unique_ptr<ComboBox> trBox; std::unique_ptr<ComboBox> sizeBox;
    std::unique_ptr<Slider> lookSl[4];

    void toggleDrawer() { drawerOpen = ! drawerOpen; proc.setUiProperty ("uiDrawer", drawerOpen); applySize(); }
    void openDrawer (int tab)
    {
        if (drawerOpen && drawerTab == tab && tab == 3 && fnKey[3]->isLit()) { toggleDrawer(); return; }
        drawerTab = tab; proc.setUiProperty ("uiTab", tab);
        if (! drawerOpen) { drawerOpen = true; proc.setUiProperty ("uiDrawer", true); applySize(); }
        else layoutDrawer();
        drawer.repaint();
    }
    template <class T> T* add (int tab, std::unique_ptr<T> c) { auto* p = c.get(); drawer.addChildComponent (*p); drawerItems[tab].push_back (std::move (c)); return p; }
    Knob* knob (int tab, const String& id, const String& label) { return add (tab, std::make_unique<Knob> (look, proc.apvts, id, label)); }
    ParamKey* pkey (int tab, const String& id, const String& t, int choice = -1) { return add (tab, std::make_unique<ParamKey> (look, proc.apvts, id, t, choice)); }

    void buildDrawer()
    {
        drawer.ed = this; content.ed = this;
        // EQ
        pkey (0, "eqOn", "EQ IN");
        const char* bn[] = { "LOW", "LO MID", "HI MID", "HIGH" };
        for (int b = 0; b < 4; ++b) { knob (0, "eqG" + String (b), String (bn[b]) + " dB"); knob (0, "eqF" + String (b), String (bn[b]) + " Hz"); }
        // COMP
        pkey (1, "cpOn", "COMP IN");
        pkey (1, "cpType", "VCA", 0); pkey (1, "cpType", "FET", 1); pkey (1, "cpType", "OPTO", 2);
        colorKey = pkey (1, "cpColor", "SAT"); pkey (1, "cpDuck", "DUCK");
        knob (1, "cpThr", "THRESH"); knob (1, "cpRatio", "RATIO"); knob (1, "cpAtk", "ATTACK"); knob (1, "cpRel", "RELEASE"); knob (1, "cpMakeup", "MAKEUP"); knob (1, "cpMix", "MIX");
        // DELAY
        pkey (2, "dlOn", "DELAY IN"); pkey (2, "dlSync", "SYNC"); pkey (2, "dlPing", "PING-PONG");
        auto* tl = knob (2, "dlTimeL", "TIME L"); auto* tr = knob (2, "dlTimeR", "TIME R");
        tl->textFn = [this] { return delayText (0); }; tr->textFn = [this] { return delayText (1); };
        knob (2, "dlFb", "FEEDBACK"); knob (2, "dlMix", "MIX"); knob (2, "dlLow", "LOW"); knob (2, "dlHigh", "HIGH");
        // SETUP
        pkey (3, "config", "SINGLE", 0); pkey (3, "config", "CASCADE", 1); pkey (3, "config", "ST SPLIT", 2); pkey (3, "config", "MONO SPLIT", 3);
        pkey (3, "hwOn", "ANALOG");
        pkey (3, "hwIn", "IN ANALOG", 0); pkey (3, "hwIn", "IN DIGITAL", 1); pkey (3, "hwOut", "OUT ANALOG", 0); pkey (3, "hwOut", "OUT DIGITAL", 1);
        pkey (3, "hwXfIn", "XFMR IN"); pkey (3, "hwXfOut", "XFMR OUT"); pkey (3, "hwNoise", "NOISE");
        pkey (3, "trOn", "IRON");
        trBox = std::make_unique<ComboBox>(); drawer.addChildComponent (*trBox);
        trBox->addItemList (proc.apvts.getParameter ("trModel")->getAllValueStrings(), 1);
        trAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "trModel", *trBox);
        knob (3, "trFreq", "IRON FREQ"); knob (3, "trAmt", "IRON AMT"); knob (3, "out", "OUTPUT");
        sizeBox = std::make_unique<ComboBox>(); drawer.addChildComponent (*sizeBox);
        const float sizes[] = { 0.75f, 0.9f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };
        for (int i = 0; i < 7; ++i) sizeBox->addItem (String (roundToInt (sizes[i] * 100)) + "%", i + 1);
        for (int i = 0; i < 7; ++i) if (std::abs (sizes[i] - scale) < 0.01f) sizeBox->setSelectedId (i + 1, dontSendNotification);
        sizeBox->onChange = [this, sizes] { const int i = sizeBox->getSelectedId() - 1; if (i >= 0) { scale = sizes[i]; proc.setUiProperty ("uiScale", scale); applySize(); } };
        // LOOK
        const char* th[] = { "CLASSIC", "BLACK", "SILVER", "STUDIO" };
        for (int i = 0; i < 4; ++i) { auto* k = add (4, std::make_unique<LarcKey> (look, th[i])); k->onClick = [this, i] { look = Look::preset (i); look.save (proc); lookChanged(); }; }
        const char* ln[] = { "PANEL GRAD", "SIDES GRAD", "LETTER GRAD", "3D DEPTH" };
        for (int i = 0; i < 4; ++i)
        {
            lookSl[i] = std::make_unique<Slider> (Slider::LinearHorizontal, Slider::NoTextBox); drawer.addChildComponent (*lookSl[i]);
            lookSl[i]->setRange (0.0, 1.0); lookSl[i]->setName (ln[i]);
            lookSl[i]->onValueChange = [this, i]
            {
                float* f[] = { &look.panelGrad, &look.sidesGrad, &look.lettersGrad, &look.depth };
                *f[i] = (float) lookSl[i]->getValue(); look.save (proc); lookChanged();
            };
        }
        syncLookSliders();
    }
    void syncLookSliders()
    {
        const float v[] = { look.panelGrad, look.sidesGrad, look.lettersGrad, look.depth };
        for (int i = 0; i < 4; ++i) { lookSl[i]->setValue (v[i], dontSendNotification); lookSl[i]->setColour (Slider::thumbColourId, Look::lamp()); lookSl[i]->setColour (Slider::trackColourId, look.letters.withAlpha (0.4f)); lookSl[i]->setColour (Slider::backgroundColourId, Colours::black.withAlpha (0.5f)); }
    }
    void lookChanged() { texture = {}; syncLookSliders(); repaint(); content.repaint(); drawer.repaint(); for (auto* c : content.getChildren()) c->repaint(); for (auto* c : drawer.getChildren()) c->repaint(); }
    String delayText (int ch)
    {
        const bool sync = proc.apvts.getRawParameterValue ("dlSync")->load() > 0.5f;
        if (sync) { auto* p = proc.apvts.getParameter (ch == 0 ? "dlDivL" : "dlDivR"); return p->getCurrentValueAsText(); }
        return String (roundToInt (proc.apvts.getRawParameterValue (ch == 0 ? "dlTimeL" : "dlTimeR")->load())) + " ms";
    }

    void layoutDrawer()
    {
        for (auto& t : drawerItems) for (auto& c : t) c->setVisible (false);
        trBox->setVisible (false); sizeBox->setVisible (false); for (auto& s : lookSl) s->setVisible (false);
        sections.clear();
        for (auto& k : tabKey) k->setVisible (drawerOpen);
        if (! drawerOpen) return;
        const int x0 = kTabW + 22, wAll = kDrawerW - 44; int y = 28;
        const int kh = 26;
        float fx = (float) x0; const float sp = 6;
        for (int i = 0; i < 5; ++i) { const float w = tabKey[i]->preferredWidth ((float) kh); tabKey[i]->setBounds ((int) fx, y, (int) w, kh); tabKey[i]->setLit (i == drawerTab); fx += w + sp; }
        y += kh + 18;
        auto& items = drawerItems[drawerTab];
        for (auto& c : items) c->setVisible (true);
        auto keyRow = [&] (int from, int to, int yy)
        {
            float x = (float) x0;
            for (int i = from; i < to; ++i) { auto* k = dynamic_cast<LarcKey*> (items[(size_t) i].get()); const float w = k->preferredWidth ((float) kh); k->setBounds ((int) x, yy, (int) w, kh); x += w + 6; }
        };
        auto knobGrid = [&] (int from, int to, int cols, int yy, int kw, int khh)
        {
            for (int i = from; i < to; ++i) { const int j = i - from; items[(size_t) i]->setBounds (x0 + (j % cols) * kw, yy + (j / cols) * khh, kw, khh); }
        };
        if (drawerTab == 0)
        {
            keyRow (0, 1, y); y += kh + 12;
            specArea = Rectangle<float> ((float) x0, (float) y, (float) wAll, 120); y += 136;
            sections.push_back ({ "GAIN", Rectangle<float> ((float) x0, (float) y, 100, 12) }); y += 14;
            for (int b = 0; b < 4; ++b) items[(size_t) (1 + b * 2)]->setBounds (x0 + b * (wAll / 4), y, wAll / 4, 96);
            y += 104;
            sections.push_back ({ "FREQUENCY", Rectangle<float> ((float) x0, (float) y, 100, 12) }); y += 14;
            for (int b = 0; b < 4; ++b) items[(size_t) (2 + b * 2)]->setBounds (x0 + b * (wAll / 4), y, wAll / 4, 96);
        }
        else if (drawerTab == 1)
        {
            keyRow (0, 1, y); y += kh + 14;
            sections.push_back ({ "TYPE", Rectangle<float> ((float) x0, (float) y, 100, 12) }); y += 16;
            keyRow (1, 6, y); y += kh + 16;
            const int t = jlimit (0, 2, (int) proc.apvts.getRawParameterValue ("cpType")->load());
            colorKey->setText (t == 0 ? "SAT" : t == 1 ? "DIST" : "TUBE");
            grArea = Rectangle<float> ((float) x0, (float) y, (float) wAll, 22); y += 36;
            knobGrid (6, 12, 3, y, wAll / 3, 104);
        }
        else if (drawerTab == 2)
        {
            keyRow (0, 3, y); y += kh + 18;
            knobGrid (3, 9, 3, y, wAll / 3, 104);
        }
        else if (drawerTab == 3)
        {
            sections.push_back ({ "CONFIGURATION", Rectangle<float> ((float) x0, (float) y, 200, 12) }); y += 16;
            keyRow (0, 4, y); y += kh + 16;
            sections.push_back ({ "480L HARDWARE", Rectangle<float> ((float) x0, (float) y, 200, 12) }); y += 16;
            keyRow (4, 5, y); y += kh + 6;
            keyRow (5, 9, y); y += kh + 6;
            keyRow (9, 12, y); y += kh + 16;
            sections.push_back ({ "OUTPUT", Rectangle<float> ((float) x0, (float) y, 200, 12) }); y += 16;
            keyRow (12, 13, y);
            trBox->setBounds (x0 + 70, y, wAll - 70, kh); trBox->setVisible (true); y += kh + 8;
            knobGrid (13, 16, 3, y, wAll / 3, 104); y += 112;
            sections.push_back ({ "WINDOW SIZE", Rectangle<float> ((float) x0, (float) y, 200, 12) }); y += 16;
            sizeBox->setBounds (x0, y, 120, kh); sizeBox->setVisible (true);
        }
        else
        {
            sections.push_back ({ "LOOKS", Rectangle<float> ((float) x0, (float) y, 200, 12) }); y += 16;
            keyRow (0, 4, y); y += kh + 18;
            sections.push_back ({ "COLOURS  (CLICK TO CHANGE)", Rectangle<float> ((float) x0, (float) y, 260, 12) }); y += 16;
            const int sw = wAll / 5;
            for (int i = 0; i < 5; ++i) swatchArea[i] = Rectangle<int> (x0 + i * sw, y, sw - 8, 54);
            y += 80;
            for (int i = 0; i < 4; ++i)
            {
                sections.push_back ({ lookSl[i]->getName(), Rectangle<float> ((float) x0, (float) y, 200, 12) }); y += 14;
                lookSl[i]->setBounds (x0, y, wAll, 22); lookSl[i]->setVisible (true); y += 34;
            }
        }
        drawer.repaint();
    }
    void paintSwatches (Graphics& g)
    {
        const char* n[] = { "PANEL", "SIDES", "LETTERS", "KEYS", "DISPLAY" };
        const Colour c[] = { look.panel, look.sides, look.letters, look.keys, look.led };
        for (int i = 0; i < 5; ++i)
        {
            auto r = swatchArea[i].toFloat(); auto s = r.removeFromTop (34);
            g.setColour (Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (s, 4);
            g.setGradientFill (ColourGradient (c[i].brighter (0.2f), s.getX(), s.getY(), c[i].darker (0.3f), s.getX(), s.getBottom(), false));
            g.fillRoundedRectangle (s.reduced (2), 3);
            drawLegend (g, look, n[i], r, ui (8.5f, W::Bold, 0.08f));
        }
    }
    void pickColour (int i)
    {
        Colour* c[] = { &look.panel, &look.sides, &look.letters, &look.keys, &look.led };
        auto sel = std::make_unique<ColourSelector> (ColourSelector::showColourspace | ColourSelector::showSliders);
        sel->setSize (300, 280); sel->setCurrentColour (*c[i]);
        struct L : ChangeListener { Flexiverb480Editor* e; Colour* t; void changeListenerCallback (ChangeBroadcaster* b) override { *t = static_cast<ColourSelector*> (b)->getCurrentColour(); e->look.save (e->proc); e->lookChanged(); } };
        auto* lst = new L(); lst->e = this; lst->t = c[i];
        sel->addChangeListener (lst);
        colourListener.reset (lst);
        CallOutBox::launchAsynchronously (std::move (sel), getLocalArea (&drawer, swatchArea[i]), this);
    }
    void drawerClick (Point<int> p)
    {
        if (! drawerOpen || drawerTab != 4) return;
        for (int i = 0; i < 5; ++i) if (swatchArea[i].contains (p)) pickColour (i);
    }
    void paintMiniSpectrum (Graphics& g, Rectangle<float> r)
    {
        g.setColour (Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r, 5);
        auto in = r.reduced (3);
        g.setColour (Colour (0xff070505)); g.fillRoundedRectangle (in, 3);
        in = in.reduced (6, 6);
        g.setColour (Look::lamp().withAlpha (0.07f));
        for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
        {
            const float x = in.getX() + in.getWidth() * (float) (std::log10 (f / 20.0) / 3.0);
            g.drawVerticalLine ((int) x, in.getY(), in.getBottom());
        }
        Path p; p.startNewSubPath (in.getX(), in.getBottom());
        for (int i = 0; i < 256; ++i) p.lineTo (in.getX() + in.getWidth() * i / 255.0f, in.getBottom() - in.getHeight() * jlimit (0.0f, 1.0f, display.spec[i]));
        p.lineTo (in.getRight(), in.getBottom()); p.closeSubPath();
        g.setGradientFill (ColourGradient (Colour (0xff7fa7c9).withAlpha (0.45f), in.getX(), in.getY(), Colour (0xff7fa7c9).withAlpha (0.05f), in.getX(), in.getBottom(), false));
        g.fillPath (p);
        g.setColour (Colour (0xff9fc3e0).withAlpha (0.8f)); g.strokePath (p, PathStrokeType (1.0f));
    }
    void paintGrMeter (Graphics& g, Rectangle<float> r)
    {
        g.setColour (Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r, 4);
        auto bar = r.reduced (4);
        const float gr = jlimit (0.0f, 24.0f, grShown);
        for (int i = 0; i < 24; ++i)
        {
            auto seg = Rectangle<float> (bar.getRight() - (i + 1) * bar.getWidth() / 24.0f + 1, bar.getY(), bar.getWidth() / 24.0f - 2, bar.getHeight());
            g.setColour (i < gr ? Look::lamp() : Look::lamp().withAlpha (0.08f)); g.fillRect (seg);
        }
        drawLegend (g, look, "GAIN REDUCTION", r.translated (0, 22).withHeight (12), ui (8.0f, W::Bold, 0.1f), Justification::centredLeft, look.letters.withAlpha (0.6f));
    }

    // ---------------------------------------------------------------- 30 Hz: display, meters, faders, spectrum
    void timerCallback() override
    {
        const uint32 now = Time::getMillisecondCounter();
        if (proc.programLoaded.exchange (false)) refreshPage (false);
        // display text
        String l1, l2;
        if (now < msgUntil) { l1 = msg1; l2 = msg2; }
        else
        {
            if (mode != Mode::Normal) { mode = Mode::Normal; fnKey[0]->setLit (false); progKey->setLit (false); fnKey[7]->setLit (false); }
            l1 = (machine == 0 ? "A " : "B ") + progText (machine) + (proc.isEdited (machine) && proc.registerName[machine].isEmpty() ? "*" : "");
            const auto st = proc.machineStatus (machine);
            if (focus >= 0 && now - focusTime < 2500)
            {
                const int slot = page * 6 + focus; const auto& s = proc.spec (machine, slot);
                l2 = s.lab ? String (s.lab) + " " + String (l480::formatValue (s, proc.slotValue (machine, slot))) : String();
            }
            else if (st.isNotEmpty()) l2 = st;
            else l2 = pages() == 0 ? String ("NO LARC DATA") : String (l480::algoDef (proc.algoOf (machine)).name) + " P" + String (page + 1);
            labels.hi = focus >= 0 && now - focusTime < 2500 ? focus : -1;
        }
        if (l1 != display.line1 || l2 != display.line2) { display.line1 = l1; display.line2 = l2; if (! live) display.repaint(); }
        labels.repaint();
        // meters (headroom)
        const float mi = proc.meterOut.load();
        display.mL = mi > display.mL ? mi : display.mL * 0.88f; display.mR = display.mL;
        // faders follow the parameters (smooth motion when a program loads)
        for (int i = 0; i < 6; ++i)
        {
            const int slot = page * 6 + i;
            if (fader[i]->enabled) fader[i]->setTarget (proc.slotNorm (machine, slot));
            if (fader[i]->animate()) fader[i]->repaint();
        }
        // spectrum: fast rise, gentle fall (as the 1081)
        if (live || (drawerOpen && drawerTab == 0)) updateSpectrum();
        if (! live) display.repaint (display.getLocalBounds().removeFromRight (70));
        else display.repaint();
        if (drawerOpen && drawerTab == 0) drawer.repaint (specArea.toNearestInt().expanded (4));
        if (drawerOpen && drawerTab == 1)
        {
            grShown = grShown * 0.7f + proc.compGr.load() * 0.3f;
            drawer.repaint (grArea.toNearestInt().expanded (4));
            const int t = jlimit (0, 2, (int) proc.apvts.getRawParameterValue ("cpType")->load());
            colorKey->setText (t == 0 ? "SAT" : t == 1 ? "DIST" : "TUBE");
        }
        fnKey[4]->setLit (machine == 1);
    }
    void updateSpectrum()
    {
        constexpr int N = 2048;
        const int pos = proc.scopeWrite.load (std::memory_order_acquire);
        for (int i = 0; i < N; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) (N - 1));
            fftData[i] = proc.scope[(size_t) ((pos - N + i) & (Flexiverb480Processor::kScopeSize - 1))] * w;
        }
        std::fill (fftData + N, fftData + 2 * N, 0.0f);
        fft.performFrequencyOnlyForwardTransform (fftData);
        const double sr = proc.sampleRateUi.load();
        for (int i = 0; i < 256; ++i)
        {
            const double f0 = 20.0 * std::pow (1000.0, i / 256.0), f1 = 20.0 * std::pow (1000.0, (i + 1) / 256.0);
            const int b0 = jmax (1, (int) (f0 / sr * N)), b1 = jmax (b0 + 1, (int) (f1 / sr * N));
            float mx = 0; for (int b = b0; b < jmin (N / 2, b1); ++b) mx = jmax (mx, fftData[b]);
            if (b1 - b0 <= 1) { const double fb = f0 / sr * N; const int i0 = jlimit (1, N / 2 - 2, (int) fb); const float fr = (float) (fb - i0); mx = fftData[i0] + (fftData[i0 + 1] - fftData[i0]) * fr; }
            const float db = Decibels::gainToDecibels (mx * 4.0f / (float) N, -100.0f);
            const float t = jlimit (0.0f, 1.0f, (db + 90.0f) / 90.0f);
            float& s = display.spec[i];
            s = t > s ? s + (t - s) * 0.6f : s * 0.86f + t * 0.14f;
            float& pk = display.peaks[i];
            pk = jmax (s, pk - 0.006f);
        }
    }

    void makeTexture (int w, int h)
    {
        texture = Image (Image::ARGB, w, h, true);
        Random rnd (480);
        Image::BitmapData bd (texture, Image::BitmapData::writeOnly);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
            {
                const float n = rnd.nextFloat();
                if (n > 0.82f) bd.setPixelColour (x, y, Colours::black.withAlpha (0.10f * (n - 0.82f) / 0.18f + 0.03f));
                else if (n < 0.06f) bd.setPixelColour (x, y, Colours::white.withAlpha (0.035f));
            }
    }

    // ---------------------------------------------------------------- members
    Flexiverb480Processor& proc;
    FlexLnf lnf;
    Look look;
    Content content; Drawer drawer;
    LarcDisplay display; LabelLine labels;
    std::unique_ptr<LarcKey> progKey, regKey, numKey[10], fnKey[8], sdKey[6], tabKey[5];
    std::unique_ptr<LarcFader> fader[6];
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> trAtt;
    std::unique_ptr<ChangeListener> colourListener;
    LarcKey* colorKey = nullptr;
    Rectangle<float> logoArea;
    Image texture;
    dsp::FFT fft; float fftData[4096] = {};
    int machine = 0, page = 0, focus = -1, browseBank = 1, browseNum = 1, drawerTab = 0; uint32 focusTime = 0, msgUntil = 0;
    String msg1, msg2; Mode mode = Mode::Normal;
    bool live = false, drawerOpen = false; float scale = 1.0f, grShown = 0;

    friend class Content;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Flexiverb480Editor)

public:
    void visibilityChanged() override {}
    void tick() { timerCallback(); }
    void setLive (bool b) { live = b; display.live = b; fnKey[6]->setLit (b); }
};
