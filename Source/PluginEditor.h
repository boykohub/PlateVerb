// Flexiverb 480 editor (included by PluginProcessor.cpp): a LARC-style remote that works like the 480L's.
//
//  Main display    2 lines: the loaded program (machine, bank.program or register, name) and what the keys expect next
//  Mode keys       BANK  PROG  VAR  REG - one is always lit; the numeric keys act in that mode
//  Numeric keys    1-0, each with an LED showing the current choice in the lit mode
//  Function keys   PAGE (next slider page)  CTRL (control mode: setup pages)  MACH (machine A/B)  MUTE  STO (store)
//  Slider display  the 4-character labels of the six sliders on this page
//  Six sliders     edit the page's parameters; the value shows in the main display while a slider moves
//
//  Loading:   BANK, 1-0 selects a bank (press BANK again for 11-20, 21-30, 31-32), then 1-0 loads a program.
//             PROG, 1-0 loads a program from the bank shown.
//  Variation: VAR, 1 = factory values, 2-0 = your stored variations of the loaded program (STO in VAR mode stores one).
//  Registers: REG, bank 1-0, then number 1-0 recalls a register; STO, bank, number stores one.
//  Control:   CTRL turns the sliders into setup controls (configuration, hardware model, EQ, compressor, delay);
//             PAGE or 1-8 picks the control page; CTRL again (or any mode key) returns.
// Click the display for a program list; right-click the panel for the window size.
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

// ============================================================================ colours of the remote
namespace larc
{
    static const Colour panel   { 0xff2c2d30 };   // dark grey front
    static const Colour cheek   { 0xff141415 };   // black side cheeks
    static const Colour legend  { 0xffe9e5da };   // white printing
    static const Colour key     { 0xffbdbab2 };   // light grey key caps
    static const Colour keyDark { 0xff4a4c50 };   // dark caps (mode keys)
    static const Colour led     { 0xffff2a14 };   // red LEDs and display
    static const Colour ink     { 0xff1c1d1f };
}

static void drawLegend (Graphics& g, const String& s, Rectangle<float> r, const Font& f, Justification j = Justification::centred, Colour c = larc::legend)
{
    g.setFont (f);
    g.setColour (Colours::black.withAlpha (0.3f)); g.drawText (s, r.translated (0, 1.0f), j, false);
    g.setColour (c); g.drawText (s, r, j, false);
}

// ============================================================================ a key with an LED (the LARC's keys light a small red LED)
class LarcKey : public Component, private Timer
{
public:
    LarcKey (const String& t, bool dark = false, bool hasLed = true) : text (t), darkCap (dark), led (hasLed) { setRepaintsOnMouseActivity (true); }
    std::function<void()> onClick;
    void setLit (bool b) { if (b != lit) { lit = b; repaint(); } }
    void blink() { flashOn = true; startTimer (140); repaint(); }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const float rad = 3.5f;
        const bool on = lit || flashOn;
        // panel cut-out (bezel): dark ring with a light lower lip
        g.setColour (Colours::black.withAlpha (0.75f)); g.fillRoundedRectangle (r, rad + 1.5f);
        g.setColour (Colours::white.withAlpha (0.07f)); g.drawLine (r.getX() + 3, r.getBottom() - 0.5f, r.getRight() - 3, r.getBottom() - 0.5f, 1.0f);
        // the cap: sides (darker skirt) and a slightly smaller, dished top face
        const float travel = pressed ? 1.2f : 0.0f;
        auto skirt = r.reduced (2.0f).translated (0, travel);
        const Colour c = darkCap ? larc::keyDark : larc::key;
        g.setGradientFill (ColourGradient (c.darker (0.15f), skirt.getX(), skirt.getY(), c.darker (0.7f), skirt.getX(), skirt.getBottom(), false));
        g.fillRoundedRectangle (skirt, rad);
        auto face = skirt.reduced (2.0f, 1.5f).withTrimmedBottom (pressed ? 1.0f : 2.5f);
        ColourGradient fg (c.brighter (0.28f), face.getCentreX(), face.getY(), c.darker (0.12f), face.getCentreX(), face.getBottom(), false);
        fg.addColour (0.45, c.brighter (0.08f));
        g.setGradientFill (fg); g.fillRoundedRectangle (face, rad - 1.0f);
        // dish: a soft darker centre and a crisp top highlight
        g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.10f), face.getCentreX(), face.getCentreY(), Colours::transparentBlack, face.getCentreX(), face.getY() - face.getHeight() * 0.2f, true));
        g.fillRoundedRectangle (face, rad - 1.0f);
        g.setColour (Colours::white.withAlpha (darkCap ? 0.22f : 0.55f)); g.drawLine (face.getX() + 3, face.getY() + 0.7f, face.getRight() - 3, face.getY() + 0.7f, 1.0f);
        g.setColour (Colours::black.withAlpha (0.35f)); g.drawRoundedRectangle (face, rad - 1.0f, 0.6f);
        if (isMouseOver() && ! pressed) { g.setColour (Colours::white.withAlpha (0.07f)); g.fillRoundedRectangle (face, rad - 1.0f); }
        auto txt = face;
        if (led)
        {   // round red LED lens near the top edge of the cap, with a glint
            const float d = jlimit (4.5f, 7.0f, face.getHeight() * 0.2f);
            auto l = Rectangle<float> (d, d).withCentre ({ face.getCentreX(), face.getY() + d * 0.5f + 3.0f });
            txt = face.withTrimmedTop (d + 3.0f);
            if (on) { g.setGradientFill (ColourGradient (larc::led.withAlpha (0.55f), l.getCentreX(), l.getCentreY(), larc::led.withAlpha (0.0f), l.getCentreX() + d * 1.6f, l.getCentreY(), true)); g.fillEllipse (l.expanded (d * 1.2f)); }
            g.setColour (Colours::black.withAlpha (0.6f)); g.fillEllipse (l.expanded (1.0f));
            g.setGradientFill (ColourGradient (on ? larc::led.brighter (0.5f) : Colour (0xff4a1510), l.getCentreX() - d * 0.2f, l.getY(),
                                               on ? larc::led.darker (0.2f) : Colour (0xff220806), l.getCentreX(), l.getBottom(), false));
            g.fillEllipse (l);
            g.setColour (Colours::white.withAlpha (on ? 0.7f : 0.18f)); g.fillEllipse (l.getX() + d * 0.22f, l.getY() + d * 0.15f, d * 0.3f, d * 0.25f);
        }
        // legend: bold, upper case, tracked, printed (slight emboss)
        const Colour ink = darkCap ? larc::legend : larc::ink;
        const float th = jmin (txt.getHeight() * 0.6f, 12.5f);
        g.setFont (ui (th, W::Bold, 0.09f));
        g.setColour (darkCap ? Colours::black.withAlpha (0.5f) : Colours::white.withAlpha (0.45f)); g.drawText (text, txt.translated (0, darkCap ? -0.8f : 0.8f), Justification::centred, false);
        g.setColour (ink); g.drawText (text, txt, Justification::centred, false);
    }
    void mouseDown (const MouseEvent&) override { pressed = true; repaint(); }
    void mouseUp (const MouseEvent& e) override
    {
        pressed = false; repaint();
        if (getLocalBounds().contains (e.getPosition()) && onClick) onClick();
    }
private:
    void timerCallback() override { stopTimer(); flashOn = false; repaint(); }
    String text; bool darkCap, led, lit = false, pressed = false, flashOn = false;
};

// ============================================================================ LARC slider: long slot, cap, smooth motion when a program loads
class LarcFader : public Component
{
public:
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
    void snap() { shown = target; repaint(); }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float cx = r.getCentreX(), top = r.getY() + 12, bot = r.getBottom() - 12;
        for (int i = 0; i <= 10; ++i)
        {
            const float y = bot - (bot - top) * i / 10.0f, w = (i % 5 == 0) ? 8.0f : 4.0f;
            g.setColour (larc::legend.withAlpha (i % 5 == 0 ? 0.5f : 0.28f));
            g.fillRect (cx - 11 - w, y - 0.5f, w, 1.0f); g.fillRect (cx + 11, y - 0.5f, w, 1.0f);
        }
        auto slot = Rectangle<float> (cx - 2.5f, top - 6, 5.0f, bot - top + 12);
        g.setColour (Colours::black.withAlpha (0.9f)); g.fillRoundedRectangle (slot, 2.5f);
        const float y = bot - (bot - top) * shown, cw = jmin (34.0f, r.getWidth() - 8), ch = 24.0f;
        auto cap = Rectangle<float> (cx - cw / 2, y - ch / 2, cw, ch);
        g.setColour (Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (cap.translated (1.5f, 3.0f), 3);
        const Colour base = enabled ? Colour (0xff1e1f21) : Colour (0xff17181a);
        g.setGradientFill (ColourGradient (base.brighter (0.6f), cap.getX(), cap.getY(), base.darker (0.3f), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 3);
        auto face = cap.reduced (2.5f, 3.0f);
        g.setGradientFill (ColourGradient (base.brighter (0.3f), face.getX(), face.getY(), base, face.getX(), face.getBottom(), false));
        g.fillRoundedRectangle (face, 2);
        g.setColour (enabled ? larc::legend : larc::legend.withAlpha (0.2f));
        g.fillRect (face.getX() + 1, face.getCentreY() - 0.75f, face.getWidth() - 2, 1.5f);
    }
    void mouseDown (const MouseEvent& e) override
    {
        if (! enabled) return;
        dragging = true; startShown = shown; startY = (float) e.position.y;
        if (onBegin) onBegin();
        const float bot = getHeight() - 12.0f, top = 12.0f, capY = bot - (bot - top) * shown;
        if (std::abs (e.position.y - capY) > 14) { shown = jlimit (0.0f, 1.0f, (bot - e.position.y) / (bot - top)); startShown = shown; target = shown; if (onMove) onMove (shown); repaint(); }
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (! enabled) return;
        const float span = getHeight() - 24.0f, fine = e.mods.isAltDown() || e.mods.isShiftDown() ? 0.1f : 1.0f;
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
    float shown = 0, target = 0, startShown = 0, startY = 0; bool dragging = false;
};

// one line of 14-segment characters in fixed cells; unlit segments faintly visible
static void segLine (Graphics& g, const String& s, Rectangle<float> r, float h, int cells = -1, Justification just = Justification::centredLeft)
{
    const auto f = segFont (h);
    g.setFont (f);
    const float cw = textW (f, "8") * 1.08f;
    if (cells < 0) cells = jmax (1, (int) (r.getWidth() / cw));
    const String t = s.toUpperCase();
    std::vector<std::pair<juce_wchar, bool>> cell;                 // character, decimal point after it
    for (int i = 0; i < t.length() && (int) cell.size() < cells; ++i)
    {
        const juce_wchar c = t[i];
        if ((c == '.' || c == ',') && ! cell.empty() && ! cell.back().second && cell.back().first != ' ') { cell.back().second = true; continue; }
        cell.push_back ({ c, false });
    }
    float x0 = r.getX();
    if (just == Justification::centred) x0 = r.getCentreX() - cells * cw * 0.5f;
    for (int i = 0; i < cells; ++i)
    {
        auto cr = Rectangle<float> (x0 + i * cw, r.getY(), cw, r.getHeight());
        g.setColour (larc::led.withAlpha (0.07f)); g.drawText ("~", cr, Justification::centredLeft, false);
        if (i >= (int) cell.size()) continue;
        const juce_wchar c = cell[(size_t) i].first;
        String ch = (c == ' ') ? String() : String::charToString (c);
        if (cell[(size_t) i].second) ch << ".";
        if (ch.isEmpty()) continue;
        g.setColour (larc::led.withAlpha (0.22f));
        for (auto o : { Point<float> (-0.8f, 0), Point<float> (0.8f, 0), Point<float> (0, 0.8f) }) g.drawText (ch, cr.translated (o.x, o.y), Justification::centredLeft, false);
        g.setColour (larc::led); g.drawText (ch, cr, Justification::centredLeft, false);
    }
}

// ============================================================================ main display with the headroom meters
class LarcDisplay : public Component
{
public:
    String line1, line2; float mL = 0, mR = 0;
    std::function<void()> onClick;
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black.withAlpha (0.65f)); g.fillRoundedRectangle (r, 5);
        auto glass = r.reduced (3.0f);
        g.setGradientFill (ColourGradient (Colour (0xff120605), glass.getX(), glass.getY(), Colour (0xff060303), glass.getX(), glass.getBottom(), false));
        g.fillRoundedRectangle (glass, 3);
        auto meters = glass.removeFromLeft (34).reduced (6, 8);
        auto txt = glass.reduced (8, 10);
        const float h = jmin (txt.getHeight() * 0.36f, txt.getWidth() / (18.0f * 0.9f));
        auto r1 = txt.removeFromTop (txt.getHeight() * 0.5f), r2 = txt;
        segLine (g, line1, r1, h, 18); segLine (g, line2, r2, h, 18);
        paintMeters (g, meters);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.06f), r.getX(), r.getY(), Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.45f, false));
        g.fillRoundedRectangle (r.reduced (3.0f).withHeight (r.getHeight() * 0.45f), 3);
    }
    void mouseDown (const MouseEvent&) override { if (onClick) onClick(); }
    void mouseMove (const MouseEvent&) override { setMouseCursor (MouseCursor::PointingHandCursor); }
private:
    // headroom: two vertical LED columns, top LED = overload
    void paintMeters (Graphics& g, Rectangle<float> r)
    {
        const float dbs[] = { -30, -24, -18, -12, -6, -3, 0 };
        const int n = 7;
        auto lab = r.removeFromBottom (10);
        const float colW = r.getWidth() / 2.0f, segH = r.getHeight() / n;
        for (int ch = 0; ch < 2; ++ch)
        {
            const float lv = 20.0f * std::log10 (jmax (1.0e-6f, ch == 0 ? mL : mR));
            for (int i = 0; i < n; ++i)
            {
                auto seg = Rectangle<float> (r.getX() + ch * colW + 1, r.getBottom() - (i + 1) * segH + 1, colW - 2, segH - 2);
                const bool on = lv >= dbs[i];
                const Colour c = i == n - 1 ? Colour (0xffff4020) : larc::led;
                g.setColour (on ? c : c.withAlpha (0.09f)); g.fillRect (seg);
            }
        }
        g.setColour (larc::led.withAlpha (0.6f)); g.setFont (ui (7.5f, W::Bold));
        g.drawText ("L", lab.removeFromLeft (colW), Justification::centred, false);
        g.drawText ("R", lab, Justification::centred, false);
    }
};

// slider display: six 4-character windows, one above each slider
class SliderDisplay : public Component
{
public:
    String labels[6]; int hi = -1;
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black.withAlpha (0.65f)); g.fillRoundedRectangle (r, 4);
        auto glass = r.reduced (2.5f);
        g.setColour (Colour (0xff080404)); g.fillRoundedRectangle (glass, 3);
        const float w = glass.getWidth() / 6.0f, h = jmin (glass.getHeight() * 0.55f, 14.0f);
        for (int i = 0; i < 6; ++i)
        {
            auto c = Rectangle<float> (glass.getX() + i * w, glass.getY(), w, glass.getHeight());
            if (i == hi) { g.setColour (larc::led.withAlpha (0.08f)); g.fillRect (c.reduced (2)); }
            segLine (g, labels[i], c, h, 4, Justification::centred);
        }
    }
};

// ============================================================================ knobs with printed scales
// All knobs turn 270 degrees, centred: the middle of the range points straight up (0 dB on a gain knob).
static constexpr float kSweep = MathConstants<float>::pi * 0.75f;      // +-135 degrees from 12 o'clock
static float knobAngle (float norm) { return -kSweep + 2.0f * kSweep * jlimit (0.0f, 1.0f, norm); }

struct ScaleMark { float value; const char* text; };

// numbers printed around a circle at the given values of a parameter
static void drawScale (Graphics& g, RangedAudioParameter* p, Point<float> c, float radius, const std::vector<ScaleMark>& marks, Colour col, float fontH)
{
    g.setFont (ui (fontH, W::Semibold));
    for (auto& m : marks)
    {
        const float a = knobAngle (p->convertTo0to1 (m.value));
        const auto pt = c.getPointOnCircumference (radius, a);
        g.setColour (col.withAlpha (0.55f));
        g.drawLine (Line<float> (c.getPointOnCircumference (radius - fontH * 0.95f, a), c.getPointOnCircumference (radius - fontH * 0.55f, a)), 1.0f);
        g.setColour (col);
        g.drawText (m.text, Rectangle<float> (40, fontH + 2).withCentre (pt), Justification::centred, false);
    }
}

// a moulded knob: shadow, skirt, cap with a pointer line
static void drawKnobBody (Graphics& g, Rectangle<float> k, float angle, Colour body, Colour pointer, bool skirt)
{
    const float d = k.getWidth();
    g.setColour (Colours::black.withAlpha (0.5f)); g.fillEllipse (k.translated (1.5f, 2.5f));
    if (skirt)
    {
        g.setGradientFill (ColourGradient (body.brighter (0.35f), k.getX(), k.getY(), body.darker (0.6f), k.getRight(), k.getBottom(), false));
        g.fillEllipse (k);
        // fluting on the skirt edge
        g.setColour (Colours::black.withAlpha (0.25f));
        for (int i = 0; i < 36; ++i) { const float a = i * MathConstants<float>::twoPi / 36.0f; g.drawLine (Line<float> (k.getCentre().getPointOnCircumference (d * 0.43f, a), k.getCentre().getPointOnCircumference (d * 0.5f, a)), 0.8f); }
        k = k.reduced (d * 0.1f);
    }
    g.setGradientFill (ColourGradient (body.brighter (0.25f), k.getCentreX(), k.getY(), body.darker (0.35f), k.getCentreX(), k.getBottom(), false));
    g.fillEllipse (k);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.22f), k.getCentreX(), k.getY(), Colours::transparentWhite, k.getCentreX(), k.getCentreY(), false));
    g.fillEllipse (k.reduced (1.5f));
    g.setColour (Colours::black.withAlpha (0.4f)); g.drawEllipse (k, 0.8f);
    g.setColour (pointer);
    const auto c = k.getCentre(); const float r = k.getWidth() * 0.5f;
    g.drawLine (Line<float> (c.getPointOnCircumference (r * 0.25f, angle), c.getPointOnCircumference (r * 0.95f, angle)), 2.2f);
}

class ParamWatcher : private AudioProcessorValueTreeState::Listener, private AsyncUpdater
{
public:
    ParamWatcher (AudioProcessorValueTreeState& s, StringArray ids, Component& c) : st (s), idList (ids), comp (c) { for (auto& i : idList) st.addParameterListener (i, this); }
    ~ParamWatcher() override { for (auto& i : idList) st.removeParameterListener (i, this); }
private:
    void parameterChanged (const String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { comp.repaint(); }
    AudioProcessorValueTreeState& st; StringArray idList; Component& comp;
};

// one knob with its scale numbers, title below; drag up/down, double-click = default, wheel
class ScaleKnob : public Component
{
public:
    ScaleKnob (AudioProcessorValueTreeState& s, const String& id, const String& title, std::vector<ScaleMark> m, Colour cap = Colour (0xff3a3c40))
        : st (s), param (s.getParameter (id)), name (title), marks (std::move (m)), capColour (cap), watch (s, { id }, *this) {}
    std::function<void (const String&, const String&)> onShow;      // (name, value text) for the main display
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto lab = r.removeFromBottom (14);
        const float d = jmin (r.getWidth(), r.getHeight()) * 0.52f;
        const auto c = r.getCentre().translated (0, 3);
        drawScale (g, param, c, d * 0.5f + 11.0f, marks, larc::legend.withAlpha (0.85f), 8.5f);
        drawKnobBody (g, Rectangle<float> (d, d).withCentre (c), knobAngle (param->getValue()), capColour, larc::legend, true);
        drawLegend (g, name, lab, ui (9.5f, W::Bold, 0.08f));
    }
    void mouseDown (const MouseEvent&) override { start = param->getValue(); param->beginChangeGesture(); show(); }
    void mouseDrag (const MouseEvent& e) override
    {
        const float fine = e.mods.isShiftDown() || e.mods.isAltDown() ? 0.1f : 1.0f;
        param->setValueNotifyingHost (jlimit (0.0f, 1.0f, start - (float) e.getDistanceFromDragStartY() * 0.005f * fine)); show();
    }
    void mouseUp (const MouseEvent&) override { param->endChangeGesture(); }
    void mouseDoubleClick (const MouseEvent&) override { param->beginChangeGesture(); param->setValueNotifyingHost (param->getDefaultValue()); param->endChangeGesture(); show(); }
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override
    {
        param->beginChangeGesture(); param->setValueNotifyingHost (jlimit (0.0f, 1.0f, param->getValue() + w.deltaY * 0.03f)); param->endChangeGesture(); show();
    }
private:
    void show() { if (onShow) onShow (name, param->getCurrentValueAsText()); }
    AudioProcessorValueTreeState& st; RangedAudioParameter* param; String name; std::vector<ScaleMark> marks; Colour capColour; float start = 0;
    ParamWatcher watch;
};

// concentric EQ knob (console style): outer skirt = frequency, inner cap = gain.
// White numbers outside = frequency; red numbers on the inner ring = gain.
class DualKnob : public Component
{
public:
    DualKnob (AudioProcessorValueTreeState& s, const String& freqId, const String& gainId, const String& title, std::vector<ScaleMark> fMarks)
        : st (s), fP (s.getParameter (freqId)), gP (s.getParameter (gainId)), name (title), fm (std::move (fMarks)), watch (s, { freqId, gainId }, *this) {}
    std::function<void (const String&, const String&)> onShow;
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto lab = r.removeFromBottom (14);
        geometry (r);
        static const std::vector<ScaleMark> gm { { -15, "-15" }, { -10, "-10" }, { 0, "0" }, { 10, "+10" }, { 15, "+15" } };
        const Colour red (0xffff6a4a);
        g.setColour (red.withAlpha (0.6f));
        for (float db = -15; db <= 15; db += 5)
        {
            const float a = knobAngle (gP->convertTo0to1 (db));
            g.drawLine (Line<float> (c.getPointOnCircumference (rOuter + 2.0f, a), c.getPointOnCircumference (rOuter + 5.5f, a)), 1.2f);
        }
        drawScale (g, fP, c, rOuter + 27.0f, fm, larc::legend.withAlpha (0.9f), 8.5f);
        drawScale (g, gP, c, rOuter + 12.5f, gm, red, 7.0f);
        drawKnobBody (g, Rectangle<float> (rOuter * 2, rOuter * 2).withCentre (c), knobAngle (fP->getValue()), Colour (0xff5a6470), larc::legend, true);
        drawKnobBody (g, Rectangle<float> (rInner * 2, rInner * 2).withCentre (c), knobAngle (gP->getValue()), Colour (0xff8a2a22), Colours::white, false);
        drawLegend (g, name, lab, ui (9.5f, W::Bold, 0.08f));
    }
    void mouseDown (const MouseEvent& e) override
    {
        geometry (getLocalBounds().toFloat().withTrimmedBottom (14));
        active = e.position.getDistanceFrom (c) <= rInner + 1.0f ? gP : fP;
        start = active->getValue(); active->beginChangeGesture(); show();
    }
    void mouseDrag (const MouseEvent& e) override
    {
        const float fine = e.mods.isShiftDown() || e.mods.isAltDown() ? 0.1f : 1.0f;
        active->setValueNotifyingHost (jlimit (0.0f, 1.0f, start - (float) e.getDistanceFromDragStartY() * 0.005f * fine)); show();
    }
    void mouseUp (const MouseEvent&) override { if (active) active->endChangeGesture(); }
    void mouseDoubleClick (const MouseEvent& e) override
    {
        geometry (getLocalBounds().toFloat().withTrimmedBottom (14));
        auto* p = e.position.getDistanceFrom (c) <= rInner + 1.0f ? gP : fP;
        p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); active = p; show();
    }
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        geometry (getLocalBounds().toFloat().withTrimmedBottom (14));
        auto* p = e.position.getDistanceFrom (c) <= rInner + 1.0f ? gP : fP;
        p->beginChangeGesture(); p->setValueNotifyingHost (jlimit (0.0f, 1.0f, p->getValue() + w.deltaY * 0.03f)); p->endChangeGesture(); active = p; show();
    }
private:
    bool hitTest (int x, int y) override { geometry (getLocalBounds().toFloat().withTrimmedBottom (14)); return Point<float> ((float) x, (float) y).getDistanceFrom (c) < rOuter + 6.0f || y > getHeight() - 14; }
    void geometry (Rectangle<float> r) { c = r.getCentre().translated (0, 4); rOuter = jmin ((r.getWidth() - 28.0f) * 0.22f, r.getHeight() * 0.22f); rInner = rOuter * 0.58f; }
    void show() { if (onShow && active) onShow (name + (active == gP ? " GAIN" : " FREQ"), active->getCurrentValueAsText()); }
    AudioProcessorValueTreeState& st; RangedAudioParameter *fP, *gP, *active = nullptr; String name; std::vector<ScaleMark> fm;
    Point<float> c; float rOuter = 20, rInner = 11, start = 0;
    ParamWatcher watch;
};

// a key bound to an on/off parameter: lit when on, text centred
class ParamKey : public LarcKey
{
public:
    ParamKey (AudioProcessorValueTreeState& s, const String& id, const String& t) : LarcKey (t, false, true), param (s.getParameter (id))
    {
        onClick = [this] { param->beginChangeGesture(); param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f); param->endChangeGesture(); };
    }
    void sync() { setLit (param->getValue() > 0.5f); }
private:
    RangedAudioParameter* param;
};

// ============================================================================ decay-time graph (RT against frequency) for the selected machine
// Dashed: the program's own decay. Solid: your curve. Click to add a point, drag to move it, double-click to remove it.
class DecayGraph : public Component
{
public:
    explicit DecayGraph (Flexiverb480Processor& p) : proc (p) {}
    std::function<int()> machine;
    std::function<void (const String&, const String&)> onShow;
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black.withAlpha (0.65f)); g.fillRoundedRectangle (r, 5);
        auto glass = r.reduced (3.0f);
        g.setGradientFill (ColourGradient (Colour (0xff120605), glass.getX(), glass.getY(), Colour (0xff060303), glass.getX(), glass.getBottom(), false));
        g.fillRoundedRectangle (glass, 3);
        plot = glass.reduced (34, 14).withTrimmedRight (-20).withTrimmedBottom (6);
        const int m = machine ? machine() : 0;
        if (! proc.hasDecayCurve (m))
        {
            g.setColour (larc::led.withAlpha (0.7f)); g.setFont (ui (12.0f, W::Semibold));
            g.drawText ("This program has no reverb decay to shape", glass, Justification::centred, false);
            return;
        }
        rtMain = jmax (0.05f, proc.mainRt (m));
        yLo = jmax (0.02f, rtMain / 10.0f); yHi = rtMain * 6.0f;
        // grid
        g.setFont (ui (8.0f, W::Medium));
        for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            const float x = xOf (f); g.setColour (larc::led.withAlpha (0.10f)); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (larc::led.withAlpha (0.55f)); g.drawText (f >= 1000 ? String (f / 1000, 0) + "k" : String ((int) f), Rectangle<float> (x - 15, plot.getBottom() + 1, 30, 10), Justification::centred, false);
        }
        for (float t : { 0.05f, 0.1f, 0.2f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 50.0f })
        {
            if (t < yLo || t > yHi) continue;
            const float y = yOf (t); g.setColour (larc::led.withAlpha (0.10f)); g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            g.setColour (larc::led.withAlpha (0.55f)); g.drawText (String (t, t < 1 ? 2 : 0) + " s", Rectangle<float> (plot.getX() - 34, y - 5, 31, 10), Justification::centredRight, false);
        }
        // the program's own decay (dashed)
        {
            Path pp; bool first = true;
            for (int i = 0; i <= 120; ++i) { const float f = 20.0f * std::pow (1000.0f, i / 120.0f); const Point<float> pt (xOf (f), yOf (rtMain * proc.programDecayMul (m, f))); if (first) pp.startNewSubPath (pt); else pp.lineTo (pt); first = false; }
            Path dashed; const float dl[] = { 4.0f, 3.0f }; PathStrokeType (1.0f).createDashedStroke (dashed, pp, dl, 2);
            g.setColour (larc::led.withAlpha (proc.curveOn[m] ? 0.35f : 0.8f)); g.fillPath (dashed);
        }
        // your curve
        const auto& pts = proc.curvePts[m];
        if (proc.curveOn[m] && ! pts.empty())
        {
            Path pc;
            for (int i = 0; i <= 160; ++i) { const float f = 20.0f * std::pow (1000.0f, i / 160.0f); const Point<float> pt (xOf (f), yOf (rtMain * Flexiverb480Processor::curveEval (pts, f))); if (i == 0) pc.startNewSubPath (pt); else pc.lineTo (pt); }
            g.setColour (larc::led.withAlpha (0.25f)); g.strokePath (pc, PathStrokeType (4.0f));
            g.setColour (larc::led); g.strokePath (pc, PathStrokeType (1.8f));
            for (size_t i = 0; i < pts.size(); ++i)
            {
                const Point<float> pt (xOf (pts[i].f), yOf (rtMain * pts[i].m));
                g.setColour (Colours::black); g.fillEllipse (Rectangle<float> (11, 11).withCentre (pt));
                g.setColour ((int) i == drag || (int) i == hover ? Colours::white : larc::led); g.drawEllipse (Rectangle<float> (9, 9).withCentre (pt), 2.0f);
            }
        }
        g.setColour (larc::led.withAlpha (0.7f)); g.setFont (ui (9.0f, W::Bold, 0.06f));
        g.drawText ("MACHINE " + String (m == 0 ? "A" : "B") + (proc.curveOn[m] ? "   YOUR CURVE" : "   PROGRAM DECAY  -  CLICK TO ADD A POINT"), glass.reduced (8, 3).withHeight (12), Justification::topLeft, false);
    }
    void mouseMove (const MouseEvent& e) override { const int h = hit (e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const MouseEvent& e) override
    {
        const int m = machine ? machine() : 0;
        if (! proc.hasDecayCurve (m) || ! plot.contains (e.position.x, e.position.y) && hit (e.position) < 0) return;
        auto pts = proc.curvePts[m];
        if (! proc.curveOn[m]) pts = proc.programCurve (m);           // start from the program's own shape
        drag = hit (e.position);
        if (drag < 0 || ! proc.curveOn[m])
        {
            pts.push_back ({ fOf (e.position.x), jlimit (0.02f, 20.0f, tOf (e.position.y) / rtMain) });
            std::sort (pts.begin(), pts.end(), [] (auto& a, auto& b) { return a.f < b.f; });
            proc.setCurve (m, pts, true);
            drag = hit (e.position);
        }
        else proc.setCurve (m, pts, true);
        show(); repaint();
    }
    void mouseDrag (const MouseEvent& e) override
    {
        const int m = machine ? machine() : 0;
        if (drag < 0 || drag >= (int) proc.curvePts[m].size()) return;
        auto pts = proc.curvePts[m];
        const float lo = drag > 0 ? pts[(size_t) drag - 1].f * 1.05f : 20.0f, hi = drag + 1 < (int) pts.size() ? pts[(size_t) drag + 1].f / 1.05f : 20000.0f;
        pts[(size_t) drag] = { jlimit (lo, hi, fOf (e.position.x)), jlimit (0.02f, 20.0f, tOf (jlimit (plot.getY(), plot.getBottom(), e.position.y)) / rtMain) };
        proc.setCurve (m, pts, true); show(); repaint();
    }
    void mouseUp (const MouseEvent&) override { drag = -1; repaint(); }
    void mouseDoubleClick (const MouseEvent& e) override
    {
        const int m = machine ? machine() : 0; const int h = hit (e.position);
        if (h < 0 || ! proc.curveOn[m]) return;
        auto pts = proc.curvePts[m]; pts.erase (pts.begin() + h);
        proc.setCurve (m, pts, ! pts.empty()); drag = hover = -1; repaint();
    }
private:
    float xOf (float f) const { return plot.getX() + plot.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); }
    float fOf (float x) const { return jlimit (20.0f, 20000.0f, 20.0f * std::pow (1000.0f, (x - plot.getX()) / plot.getWidth())); }
    float yOf (float t) const { return plot.getBottom() - plot.getHeight() * std::log (jlimit (yLo, yHi, t) / yLo) / std::log (yHi / yLo); }
    float tOf (float y) const { return yLo * std::pow (yHi / yLo, (plot.getBottom() - y) / plot.getHeight()); }
    int hit (Point<float> p) const
    {
        const int m = machine ? machine() : 0; if (! proc.curveOn[m]) return -1;
        const auto& pts = proc.curvePts[m];
        for (size_t i = 0; i < pts.size(); ++i) if (p.getDistanceFrom ({ xOf (pts[i].f), yOf (rtMain * pts[i].m) }) < 8.0f) return (int) i;
        return -1;
    }
    void show()
    {
        const int m = machine ? machine() : 0; const auto& pts = proc.curvePts[m];
        if (onShow && drag >= 0 && drag < (int) pts.size())
        {
            const float f = pts[(size_t) drag].f, t = rtMain * pts[(size_t) drag].m;
            onShow ("DECAY " + (f >= 1000 ? String (f / 1000, 2) + " KHZ" : String ((int) f) + " HZ"), String (t, t < 10 ? 2 : 1) + " S");
        }
    }
    Flexiverb480Processor& proc; Rectangle<float> plot; float rtMain = 1, yLo = 0.1f, yHi = 10; int drag = -1, hover = -1;
};

// ============================================================================ CTRL pages: setup on the sliders (the 480L's control mode)
struct CtrlSlot { const char* lab; const char* id; const char* name; };
static constexpr int kCtrlPages = 4;
static const CtrlSlot kCtrl[kCtrlPages][6] = {
    { { "CNFG", "config", "Configuration" }, { "OUT", "out", "Output Level" }, { "HW", "hwOn", "480L Hardware" },
      { "IN", "hwIn", "Input Type" }, { "OUTT", "hwOut", "Output Type" }, { "NOIS", "hwNoise", "Converter Noise" } },
    { { "XFIN", "hwXfIn", "Input Transformers" }, { "XFOU", "hwXfOut", "Output Transformers" }, { "IRON", "trOn", "Iron" },
      { "MODL", "trModel", "Iron Model" }, { "FREQ", "trFreq", "Iron Frequency" }, { "AMT", "trAmt", "Iron Amount" } },
    { { "DLY", "dlOn", "Delay On" }, { "SYNC", "dlSync", "Tempo Sync" }, { "TIML", "dlTimeL", "Delay Left" },
      { "TIMR", "dlTimeR", "Delay Right" }, { "FBK", "dlFb", "Feedback" }, { "MIX", "dlMix", "Delay Mix" } },
    { { "PING", "dlPing", "Ping-Pong" }, { "LOW", "dlLow", "Delay Low" }, { "HIGH", "dlHigh", "Delay High" }, {}, {}, {} },
};
static const char* kCtrlPageName[kCtrlPages] = { "SETUP", "IRON", "DELAY", "DELAY 2" };

struct FlexLnf : public LookAndFeel_V4
{
    Font getPopupMenuFont() override { return ui (13.5f, W::Medium); }
};

// ============================================================================ the editor
class Flexiverb480Editor : public AudioProcessorEditor, private Timer
{
public:
    static constexpr int kL = 452, kH = 700;          // the remote
    static constexpr int kTab = 24, kPanel = 440;     // drawer tab and drawer panel
    static constexpr int kBankGroups = (l480::kNumBanks + 9) / 10;

    explicit Flexiverb480Editor (Flexiverb480Processor& p) : AudioProcessorEditor (p), proc (p)
    {
        scale = (float) (double) proc.getUiProperty ("uiScale", 1.0);
        browseBank = proc.prog (0).bank;

        addAndMakeVisible (content);
        content.ed = this;
        content.addAndMakeVisible (display);
        display.onClick = [this] { showProgramMenu(); };
        content.addAndMakeVisible (sliderDisplay);

        auto mk = [this] (const String& t, bool dark, bool led) { auto k = std::make_unique<LarcKey> (t, dark, led); content.addAndMakeVisible (*k); return k; };
        const char* modes[] = { "BANK", "PROG", "VAR", "REG" };
        for (int i = 0; i < 4; ++i) { modeKey[i] = mk (modes[i], true, true); modeKey[i]->onClick = [this, i] { pressMode ((Mode) i); }; }
        for (int i = 0; i < 10; ++i) { numKey[i] = mk (String ((i + 1) % 10), false, true); numKey[i]->onClick = [this, i] { pressNumber (i + 1); }; }
        pageKey = mk ("PAGE", false, true); pageKey->onClick = [this] { pressPage(); };
        ctrlKey = mk ("CTRL", false, true); ctrlKey->onClick = [this] { pressCtrl(); };
        machKey = mk ("MACH", false, true); machKey->onClick = [this] { pressMach(); };
        muteKey = mk ("MUTE", false, true); muteKey->onClick = [this] { proc.mute.store (! proc.mute.load()); };
        stoKey  = mk ("STO", false, true);  stoKey->onClick  = [this] { pressStore(); };

        for (int i = 0; i < 6; ++i)
        {
            fader[i] = std::make_unique<LarcFader>(); content.addAndMakeVisible (*fader[i]);
            fader[i]->onBegin = [this, i] { faderBegin (i); };
            fader[i]->onMove  = [this, i] (float t) { faderMove (i, t); };
            fader[i]->onEnd   = [this, i] { faderEnd (i); };
            fader[i]->onReset = [this, i] { faderReset (i); };
        }

        // drawer: decay graph, EQ (concentric knobs), compressor
        drawerOpen = (bool) proc.getUiProperty ("uiDrawer", false);
        content.addAndMakeVisible (drawer); drawer.ed = this;
        drawer.addAndMakeVisible (graph);
        graph.machine = [this] { return machine; };
        auto shower = [this] (const String& a, const String& b) { showMessage (a, b, 1500); };
        graph.onShow = shower;
        curveKey = std::make_unique<LarcKey> ("CURVE ON", false, true); drawer.addAndMakeVisible (*curveKey);
        curveKey->onClick = [this]
        {
            if (! proc.hasDecayCurve (machine)) return;
            if (proc.curveOn[machine]) proc.setCurve (machine, proc.curvePts[machine], false);
            else proc.setCurve (machine, proc.curvePts[machine].empty() ? proc.programCurve (machine) : proc.curvePts[machine], true);
            graph.repaint();
        };
        resetKey = std::make_unique<LarcKey> ("RESET", false, false); drawer.addAndMakeVisible (*resetKey);
        resetKey->onClick = [this] { proc.setCurve (machine, {}, false); graph.repaint(); };
        const char* bn[] = { "LOW", "LOW MID", "HIGH MID", "HIGH" };
        const std::vector<ScaleMark> fm[4] = {
            { { 20, "20" }, { 50, "50" }, { 100, "100" }, { 200, "200" }, { 500, "500" } },
            { { 100, "100" }, { 200, "200" }, { 500, "500" }, { 1000, "1k" }, { 2000, "2k" } },
            { { 800, ".8k" }, { 1500, "1.5k" }, { 3000, "3k" }, { 5000, "5k" }, { 8000, "8k" } },
            { { 2000, "2k" }, { 4000, "4k" }, { 8000, "8k" }, { 12000, "12k" }, { 20000, "20k" } } };
        for (int b = 0; b < 4; ++b)
        {
            eqKnob[b] = std::make_unique<DualKnob> (proc.apvts, "eqF" + String (b), "eqG" + String (b), bn[b], fm[b]);
            eqKnob[b]->onShow = shower; drawer.addAndMakeVisible (*eqKnob[b]);
        }
        auto knob = [this, shower] (const String& id, const String& t, std::vector<ScaleMark> m)
        { auto k = std::make_unique<ScaleKnob> (proc.apvts, id, t, std::move (m)); k->onShow = shower; drawer.addAndMakeVisible (*k); return k; };
        cpKnob[0] = knob ("cpType", "TYPE", { { 0, "VCA" }, { 1, "FET" }, { 2, "OPTO" } });
        cpKnob[1] = knob ("cpThr", "THRESHOLD", { { -60, "-60" }, { -48, "-48" }, { -36, "-36" }, { -24, "-24" }, { -12, "-12" }, { 0, "0" } });
        cpKnob[2] = knob ("cpRatio", "RATIO", { { 1, "1" }, { 2, "2" }, { 4, "4" }, { 8, "8" }, { 20, "20" } });
        cpKnob[3] = knob ("cpAtk", "ATTACK", { { 0.05f, ".05" }, { 1, "1" }, { 10, "10" }, { 100, "100" }, { 300, "300" } });
        cpKnob[4] = knob ("cpRel", "RELEASE", { { 20, "20" }, { 100, "100" }, { 500, "500" }, { 2000, "2k" }, { 5000, "5k" } });
        cpKnob[5] = knob ("cpMakeup", "MAKEUP", { { 0, "0" }, { 6, "6" }, { 12, "12" }, { 18, "18" }, { 24, "24" } });
        cpKnob[6] = knob ("cpMix", "MIX", { { 0, "0" }, { 0.25f, "25" }, { 0.5f, "50" }, { 0.75f, "75" }, { 1, "100" } });
        auto pk = [this] (const String& id, const String& t) { auto k = std::make_unique<ParamKey> (proc.apvts, id, t); drawer.addAndMakeVisible (*k); return k; };
        eqOnKey = pk ("eqOn", "EQ ON"); cpOnKey = pk ("cpOn", "COMP ON"); satKey = pk ("cpColor", "SATURATE"); duckKey = pk ("cpDuck", "DUCKING");

        lnf.setColour (PopupMenu::backgroundColourId, Colour (0xff1c1d1f)); lnf.setColour (PopupMenu::textColourId, Colour (0xffe8e6e1));
        lnf.setColour (PopupMenu::highlightedBackgroundColourId, larc::led.darker (0.2f)); lnf.setColour (PopupMenu::highlightedTextColourId, Colours::white);
        lnf.setColour (PopupMenu::headerTextColourId, Colour (0xff9a968e));
        lnf.setDefaultSansSerifTypeface (FlexFonts::get().medium);
        setLookAndFeel (&lnf);
        layout(); layoutDrawer();
        setResizable (true, true);
        applySize (false);
        refreshPage (true);
        updateLeds();
        startTimerHz (30);
    }
    ~Flexiverb480Editor() override { stopTimer(); setLookAndFeel (nullptr); }

    void paint (Graphics& g) override { g.fillAll (larc::cheek); }
    // the whole window scales: the content is laid out at 1x and scaled to the window, keeping its proportions
    void resized() override
    {
        if (applying) return;
        scale = jlimit (0.5f, 3.0f, getWidth() / (float) baseWidth());
        content.setTransform (AffineTransform::scale (scale));
        proc.setUiProperty ("uiScale", scale);
    }

    class Content : public Component
    {
    public:
        Flexiverb480Editor* ed = nullptr;
        void paint (Graphics& g) override { if (ed) ed->paintPanel (g); }
        void mouseDown (const MouseEvent& e) override
        {
            if (! ed) return;
            if (e.mods.isPopupMenu()) ed->showSizeMenu();
            else if (e.x >= kL && e.x < kL + kTab) ed->toggleDrawer();
        }
    };
    class Drawer : public Component
    {
    public:
        Flexiverb480Editor* ed = nullptr;
        void paint (Graphics& g) override { if (ed) ed->paintDrawer (g); }
        void mouseDown (const MouseEvent& e) override { if (ed && e.mods.isPopupMenu()) ed->showSizeMenu(); }
    };

private:
    // ---------------------------------------------------------------- panel
    void paintPanel (Graphics& g)
    {
        auto r = Rectangle<float> (0, 0, (float) kL, (float) kH);
        const float sw = 14.0f;
        for (auto sr : { r.withWidth (sw), r.withLeft (r.getRight() - sw) })
        {
            g.setGradientFill (ColourGradient (larc::cheek.brighter (0.25f), sr.getX(), sr.getY(), larc::cheek.darker (0.4f), sr.getRight(), sr.getBottom(), false));
            g.fillRect (sr);
        }
        auto face = r.reduced (sw, 0);
        g.setGradientFill (ColourGradient (larc::panel.brighter (0.12f), face.getX(), face.getY(), larc::panel.darker (0.25f), face.getX(), face.getBottom(), false));
        g.fillRect (face);
        g.setColour (Colours::white.withAlpha (0.07f)); g.drawLine (face.getX(), 1, face.getRight(), 1, 1.5f);
        // printed group outlines and legends, as on the remote
        auto box = [&g] (Rectangle<int> a, const String& title)
        {
            auto b = a.toFloat().expanded (6, 6).withTrimmedTop (-8);
            g.setColour (larc::legend.withAlpha (0.28f)); g.drawRoundedRectangle (b, 4, 1.0f);
            if (title.isNotEmpty())
            {
                const auto f = ui (8.5f, W::Bold, 0.12f); const float tw = textW (f, title) + 10;
                auto t = Rectangle<float> (b.getX() + 8, b.getY() - 6, tw, 12);
                g.setColour (larc::panel); g.fillRect (t);
                drawLegend (g, title, t, f, Justification::centred, larc::legend.withAlpha (0.8f));
            }
        };
        box (modeArea, "MODE"); box (numArea, "SELECT"); box (fnArea, {});
        // slider numbers and the name plate
        for (int i = 0; i < 6; ++i)
        {
            auto b = fader[i]->getBounds().toFloat();
            drawLegend (g, String (i + 1), Rectangle<float> (b.getX(), b.getBottom() + 1, b.getWidth(), 11), ui (8.5f, W::Bold), Justification::centred, larc::legend.withAlpha (0.55f));
        }
        auto np = namePlate;
        const auto f1 = ui (19.0f, W::Bold, -0.02f), f2 = ui (19.0f, W::Regular);
        const float w1 = textW (f1, "flexiverb"), w2 = textW (f2, " 480"), x = np.getCentreX() - (w1 + w2) / 2;
        drawLegend (g, "flexiverb", Rectangle<float> (x, np.getY(), w1 + 2, 24), f1, Justification::centredLeft);
        drawLegend (g, " 480", Rectangle<float> (x + w1, np.getY(), w2 + 4, 24), f2, Justification::centredLeft, larc::legend.withAlpha (0.6f));
        drawLegend (g, configLegend(), np.withTrimmedTop (26), ui (8.5f, W::Bold, 0.14f), Justification::centredTop, larc::legend.withAlpha (0.55f));
        // drawer tab
        auto tab = Rectangle<float> ((float) kL, 0, (float) kTab, (float) kH);
        g.setGradientFill (ColourGradient (larc::cheek.brighter (0.3f), tab.getX(), 0, larc::cheek.darker (0.3f), tab.getRight(), 0, false));
        g.fillRect (tab);
        g.setColour (Colours::black.withAlpha (0.6f)); g.drawVerticalLine (kL, 0, (float) kH);
        {
            Graphics::ScopedSaveState ss (g);
            g.addTransform (AffineTransform::rotation (-MathConstants<float>::halfPi, tab.getCentreX(), tab.getCentreY()));
            auto t = Rectangle<float> (260, 16).withCentre (tab.getCentre());
            g.setColour (drawerOpen ? larc::led : larc::legend.withAlpha (0.85f)); g.setFont (ui (10.0f, W::Bold, 0.18f));
            g.drawText (drawerOpen ? "CLOSE   EQ  -  COMP  -  DECAY" : "OPEN   EQ  -  COMP  -  DECAY", t, Justification::centred, false);
        }
    }
    void paintDrawer (Graphics& g)
    {
        auto r = drawer.getLocalBounds().toFloat();
        g.setGradientFill (ColourGradient (larc::panel.brighter (0.12f), 0, 0, larc::panel.darker (0.25f), 0, r.getBottom(), false));
        g.fillRect (r);
        auto sr = r.withLeft (r.getRight() - 14);
        g.setGradientFill (ColourGradient (larc::cheek.brighter (0.25f), sr.getX(), 0, larc::cheek.darker (0.4f), sr.getRight(), 0, false)); g.fillRect (sr);
        auto title = [&g] (Rectangle<int> a, const String& t)
        {
            auto b = a.toFloat().expanded (8, 6).withTrimmedTop (-10);
            g.setColour (larc::legend.withAlpha (0.28f)); g.drawRoundedRectangle (b, 4, 1.0f);
            const auto f = ui (9.0f, W::Bold, 0.14f); const float tw = textW (f, t) + 12;
            auto tr = Rectangle<float> (b.getX() + 10, b.getY() - 6, tw, 12);
            g.setColour (larc::panel); g.fillRect (tr);
            drawLegend (g, t, tr, f, Justification::centred, larc::legend.withAlpha (0.85f));
        };
        title (decayArea, "DECAY TIME"); title (eqArea, "EQUALISER"); title (compArea, "COMPRESSOR");
    }

    void layout()
    {
        const int x0 = 30, x1 = kL - 30, wAll = x1 - x0;
        display.setBounds (x0, 24, wAll, 128);                 // the larger main display
        const int kh = 34, gap = 6;
        int y = 180;
        // mode keys 2 x 2 on the left, numeric keys 2 x 5 on the right
        const int mw = 52;
        for (int i = 0; i < 4; ++i) modeKey[i]->setBounds (x0 + (i % 2) * (mw + gap), y + (i / 2) * (kh + gap), mw, kh);
        modeArea = Rectangle<int> (x0, y, 2 * mw + gap, 2 * kh + gap);
        const int nw = 40, nx = x1 - (5 * nw + 4 * gap);
        for (int i = 0; i < 10; ++i) numKey[i]->setBounds (nx + (i % 5) * (nw + gap), y + (i / 5) * (kh + gap), nw, kh);
        numArea = Rectangle<int> (nx, y, 5 * nw + 4 * gap, 2 * kh + gap);
        y += 2 * kh + gap + 24;
        // function keys
        const int fw = (wAll - 4 * gap) / 5;
        LarcKey* fk[] = { pageKey.get(), ctrlKey.get(), machKey.get(), muteKey.get(), stoKey.get() };
        for (int i = 0; i < 5; ++i) fk[i]->setBounds (x0 + i * (fw + gap), y, fw, kh);
        fnArea = Rectangle<int> (x0, y, wAll, kh);
        y += kh + 22;
        sliderDisplay.setBounds (x0, y, wAll, 30);
        y += 38;
        const int fh = kH - y - 76, colW = wAll / 6;
        for (int i = 0; i < 6; ++i) fader[i]->setBounds (x0 + i * colW, y, colW, fh);
        namePlate = Rectangle<float> ((float) x0, (float) (kH - 54), (float) wAll, 44);
    }
    int baseWidth() const { return kL + kTab + (drawerOpen ? kPanel : 0); }
    bool applying = false;
    void applySize (bool keepScale = true)
    {
        (void) keepScale;
        applying = true;
        const int w = baseWidth();
        content.setBounds (0, 0, w, kH);
        drawer.setBounds (kL + kTab, 0, kPanel, kH); drawer.setVisible (drawerOpen);
        content.setTransform (AffineTransform::scale (scale));
        if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) w / kH);
        setResizeLimits (roundToInt (w * 0.5f), roundToInt (kH * 0.5f), roundToInt (w * 3.0f), roundToInt (kH * 3.0f));
        setSize (roundToInt (w * scale), roundToInt (kH * scale));
        applying = false;
        content.repaint();
    }
    void toggleDrawer() { drawerOpen = ! drawerOpen; proc.setUiProperty ("uiDrawer", drawerOpen); applySize(); }
    Rectangle<int> decayArea, eqArea, compArea;
    void layoutDrawer()
    {
        const int x0 = 26, wAll = kPanel - 26 - 30, kw = 96, kh2 = 28;
        int y = 30;
        graph.setBounds (x0, y, wAll, 150); y += 156;
        curveKey->setBounds (x0 + wAll - 2 * kw - 6, y, kw, kh2); resetKey->setBounds (x0 + wAll - kw, y, kw, kh2);
        decayArea = Rectangle<int> (x0, 30, wAll, y + kh2 - 30);
        y += kh2 + 34;
        eqOnKey->setBounds (x0 + wAll - kw, y - 2, kw, kh2);
        const int eqTop = y; y += 28;
        const int ew = wAll / 4;
        for (int b = 0; b < 4; ++b) eqKnob[b]->setBounds (x0 + b * ew - 14, y, ew + 28, 134);     // wider than the column: room for the numbers
        y += 134; eqArea = Rectangle<int> (x0, eqTop, wAll, y - eqTop);
        y += 32;
        const int cTop = y;
        cpOnKey->setBounds (x0 + wAll - 3 * kw - 12, y - 2, kw, kh2); satKey->setBounds (x0 + wAll - 2 * kw - 6, y - 2, kw, kh2); duckKey->setBounds (x0 + wAll - kw, y - 2, kw, kh2);
        y += 30;
        const int cw = wAll / 4;
        for (int i = 0; i < 4; ++i) cpKnob[i]->setBounds (x0 + i * cw, y, cw, 94);
        y += 96;
        for (int i = 4; i < 7; ++i) cpKnob[i]->setBounds (x0 + (i - 4) * cw + cw / 2, y, cw, 94);
        y += 94; compArea = Rectangle<int> (x0, cTop, wAll, y - cTop);
    }
    void showSizeMenu()
    {
        PopupMenu m; m.addSectionHeader ("Window size");
        const float sizes[] = { 0.75f, 0.9f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };
        for (int i = 0; i < 7; ++i) m.addItem (i + 1, String (roundToInt (sizes[i] * 100)) + "%", true, std::abs (sizes[i] - scale) < 0.01f);
        m.addSeparator(); m.addItem (100, "Drag the lower right corner to scale freely", false);
        m.showMenuAsync (PopupMenu::Options(), [this, sizes] (int r) { if (r > 0 && r < 100) { scale = sizes[r - 1]; proc.setUiProperty ("uiScale", scale); applySize(); } });
    }

    // ---------------------------------------------------------------- state
    enum class Mode { Bank = 0, Prog, Var, Reg };
    enum class Store { None, RegBank, RegNum, Var };
    Mode mode = Mode::Prog; Store store = Store::None;
    bool ctrl = false;
    int machine = 0, page = 0, ctrlPage = 0, bankGroup = 0, browseBank = 1, regBank = -1, storeBank = 0;
    int focus = -1; uint32 focusTime = 0, msgUntil = 0; String msg1, msg2;

    int config() const { return (int) proc.apvts.getRawParameterValue ("config")->load(); }
    int pages() const { return ctrl ? kCtrlPages : l480::algoDef (proc.algoOf (machine)).pages; }
    String machLetter() const { return machine == 0 ? "A" : "B"; }
    String configLegend() const
    {
        static const char* cfg[] = { "SINGLE", "CASCADE", "STEREO SPLIT", "MONO SPLIT" };
        return String (cfg[jlimit (0, 3, config())]) + "   -   MACHINE " + machLetter();
    }
    // what the machine holds: "1.1", "1.1/2" (variation 2) or "R3.2"
    String loadedTag (int m) const
    {
        const auto& p = proc.prog (m);
        if (proc.registerName[m].isNotEmpty()) return "R" + proc.registerName[m];
        String s = String (p.bank) + "." + String (p.num);
        if (proc.variation[m] > 1) s << "/" << String (proc.variation[m] % 10);
        return s;
    }
    void showMessage (const String& a, const String& b, int ms = 1800) { msg1 = a; msg2 = b; msgUntil = Time::getMillisecondCounter() + (uint32) ms; }
    void cancelStore() { store = Store::None; }

    // ---------------------------------------------------------------- keys
    void pressMode (Mode m)
    {
        cancelStore();
        if (ctrl) { ctrl = false; page = 0; refreshPage (true); }
        if (m == Mode::Bank)
        {
            if (mode == Mode::Bank) bankGroup = (bankGroup + 1) % kBankGroups;        // 1-10 -> 11-20 -> 21-30 -> 31-32
            else bankGroup = (proc.prog (machine).bank - 1) / 10;
        }
        if (m == Mode::Prog && mode != Mode::Bank) browseBank = proc.prog (machine).bank;
        if (m == Mode::Reg) regBank = -1;
        mode = m; msgUntil = 0;
        updateLeds();
    }
    void pressNumber (int n)                                                          // n = 1..10 (key "0" = 10)
    {
        numKey[n - 1]->blink();
        if (store == Store::RegBank) { storeBank = n; store = Store::RegNum; updateLeds(); return; }
        if (store == Store::RegNum)
        {
            if (proc.storeRegister (machine, storeBank, n)) showMessage ("STORED", "REGISTER " + String (storeBank % 10) + "." + String (n % 10));
            else showMessage ("STORE FAILED", "CHECK DOCUMENTS");
            cancelStore(); mode = Mode::Reg; regBank = -1; updateLeds(); return;
        }
        if (store == Store::Var)
        {
            if (n == 1) { showMessage ("VAR 1 IS FACTORY", "USE 2-0"); return; }
            if (proc.storeVariation (machine, n)) showMessage ("STORED", "VARIATION " + String (n % 10));
            cancelStore(); updateLeds(); return;
        }
        if (ctrl) { if (n <= kCtrlPages) { ctrlPage = n - 1; refreshPage (true); updateLeds(); } return; }
        switch (mode)
        {
            case Mode::Bank:
            {
                const int b = bankGroup * 10 + n;
                if (b > l480::kNumBanks) { showMessage ("NO BANK " + String (b), "BANKS 1-" + String (l480::kNumBanks)); return; }
                browseBank = b; mode = Mode::Prog; msgUntil = 0;                       // then pick the program
                break;
            }
            case Mode::Prog:
            {
                const int pi = l480::findProgram (browseBank, n % 10);
                if (pi < 0) { showMessage ("NO PROGRAM", String (browseBank) + "." + String (n % 10)); return; }
                proc.loadProgram (machine, pi); page = 0; refreshPage (true); msgUntil = 0;
                break;
            }
            case Mode::Var:
            {
                if (! proc.variationExists (machine, n)) { showMessage ("VARIATION " + String (n % 10), "EMPTY - STO STORES"); return; }
                proc.recallVariation (machine, n); refreshPage (true); msgUntil = 0;
                break;
            }
            case Mode::Reg:
            {
                if (regBank < 0) { regBank = n; break; }
                if (proc.recallRegister (machine, regBank, n)) { page = 0; refreshPage (true); msgUntil = 0; }
                else showMessage ("REGISTER " + String (regBank % 10) + "." + String (n % 10), "EMPTY");
                regBank = -1;
                break;
            }
        }
        updateLeds();
    }
    void pressPage()
    {
        pageKey->blink(); cancelStore();
        const int np = jmax (1, pages());
        if (ctrl) ctrlPage = (ctrlPage + 1) % np; else page = (page + 1) % np;
        refreshPage (true); updateLeds();
    }
    void pressCtrl()
    {
        cancelStore();
        ctrl = ! ctrl;
        refreshPage (true); updateLeds(); msgUntil = 0;
    }
    void pressMach()
    {
        machKey->blink(); cancelStore();
        if (config() == 0) { machine = 0; showMessage ("SINGLE CONFIG", "MACHINE A ONLY"); }
        else { machine = 1 - machine; page = 0; browseBank = proc.prog (machine).bank; refreshPage (true); msgUntil = 0; }
        updateLeds(); content.repaint();
    }
    void pressStore()
    {
        if (store != Store::None) { cancelStore(); updateLeds(); return; }
        store = mode == Mode::Var ? Store::Var : Store::RegBank;
        ctrl = false; refreshPage (true); updateLeds(); msgUntil = 0;
    }

    void updateLeds()
    {
        for (int i = 0; i < 4; ++i) modeKey[i]->setLit (! ctrl && store == Store::None && (int) mode == i);
        ctrlKey->setLit (ctrl);
        stoKey->setLit (store != Store::None);
        machKey->setLit (machine == 1);
        muteKey->setLit (proc.mute.load());
        pageKey->setLit (pages() > 1);
        int lit = -1;                                                                 // 1..10
        const auto& p = proc.prog (machine);
        if (store == Store::RegNum) lit = storeBank;
        else if (store != Store::None) lit = -1;
        else if (ctrl) lit = ctrlPage + 1;
        else switch (mode)
        {
            case Mode::Bank: { const int k = p.bank - bankGroup * 10; lit = (k >= 1 && k <= 10) ? k : -1; break; }
            case Mode::Prog: lit = (browseBank == p.bank && proc.registerName[machine].isEmpty()) ? (p.num == 0 ? 10 : p.num) : -1; break;
            case Mode::Var:  lit = proc.variation[machine] >= 1 ? proc.variation[machine] : -1; break;
            case Mode::Reg:
                if (regBank >= 0) lit = regBank;
                else if (proc.registerName[machine].isNotEmpty()) lit = proc.registerName[machine].fromFirstOccurrenceOf (".", false, false).getIntValue();
                break;
        }
        for (int i = 0; i < 10; ++i) numKey[i]->setLit (i + 1 == lit);
    }

    // ---------------------------------------------------------------- sliders
    RangedAudioParameter* ctrlParam (int i) const
    {
        const auto& c = kCtrl[ctrlPage][i];
        if (! c.id) return nullptr;
        String id (c.id);
        const bool sync = proc.apvts.getRawParameterValue ("dlSync")->load() > 0.5f;
        if (sync && id == "dlTimeL") id = "dlDivL";
        if (sync && id == "dlTimeR") id = "dlDivR";
        return proc.apvts.getParameter (id);
    }
    bool isTrigger (int i) const { return ! ctrl && proc.spec (machine, page * 6 + i).lab && proc.spec (machine, page * 6 + i).unit == l480::uKey; }
    void faderBegin (int i)
    {
        focus = i; focusTime = Time::getMillisecondCounter(); triggered[i] = false;
        if (ctrl) { if (auto* p = ctrlParam (i)) p->beginChangeGesture(); }
        else if (! isTrigger (i)) proc.beginSlot (machine, page * 6 + i);
    }
    void faderMove (int i, float t)
    {
        focus = i; focusTime = Time::getMillisecondCounter();
        if (ctrl) { if (auto* p = ctrlParam (i)) p->setValueNotifyingHost (t); return; }
        if (isTrigger (i))
        {   // a key-type parameter (PLAY, REC ...): pushing the slider up fires it, like the hardware's slider keys
            if (t > 0.6f && ! triggered[i]) { triggered[i] = true; proc.pressKey (machine, page * 6 + i); }
            return;
        }
        proc.setSlot (machine, page * 6 + i, t);
    }
    void faderEnd (int i)
    {
        if (ctrl) { if (auto* p = ctrlParam (i)) p->endChangeGesture(); return; }
        if (isTrigger (i)) { fader[i]->setTarget (0.0f); return; }
        proc.endSlot (machine, page * 6 + i);
    }
    void faderReset (int i)
    {
        if (ctrl) { if (auto* p = ctrlParam (i)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); } return; }
        const int slot = page * 6 + i;
        const auto& p = proc.prog (machine); const auto& s = proc.spec (machine, slot);
        if (! s.lab || s.unit == l480::uKey) return;
        proc.beginSlot (machine, slot); proc.setSlot (machine, slot, l480::toNorm (s, l480::programValue (p, l480::realIndex (p.algo, slot)))); proc.endSlot (machine, slot);
    }
    float faderValue (int i) const
    {
        if (ctrl) { auto* p = ctrlParam (i); return p ? p->getValue() : 0.0f; }
        const int slot = page * 6 + i; const auto& s = proc.spec (machine, slot);
        if (! s.lab || s.unit == l480::uKey) return 0.0f;
        return proc.slotNorm (machine, slot);
    }
    String faderText (int i) const
    {
        if (ctrl)
        {
            const auto& c = kCtrl[ctrlPage][i]; auto* p = ctrlParam (i);
            return p ? String (c.lab) + " " + p->getCurrentValueAsText() : String();
        }
        const int slot = page * 6 + i; const auto& s = proc.spec (machine, slot);
        if (! s.lab) return {};
        if (s.unit == l480::uKey) return String (s.lab) + " " + proc.machineStatus (machine);
        return String (s.lab) + " " + String (l480::formatValue (s, proc.slotValue (machine, slot)));
    }
    void refreshPage (bool snap)
    {
        const int np = jmax (1, pages());
        if (ctrl) { if (ctrlPage >= np) ctrlPage = 0; }
        else if (page >= np) page = 0;
        for (int i = 0; i < 6; ++i)
        {
            bool has;
            if (ctrl) { has = ctrlParam (i) != nullptr; sliderDisplay.labels[i] = has ? String (kCtrl[ctrlPage][i].lab) : String(); }
            else
            {
                const auto& s = proc.spec (machine, page * 6 + i);
                has = pages() > 0 && s.lab != nullptr;
                sliderDisplay.labels[i] = has ? String (s.lab) : String();
            }
            fader[i]->enabled = has;
            fader[i]->setTarget (has ? faderValue (i) : 0.0f);
            if (snap) fader[i]->snap();
            fader[i]->repaint();
        }
        focus = -1;
        sliderDisplay.repaint();
    }

    // ---------------------------------------------------------------- program list (click on the display)
    void showProgramMenu()
    {
        PopupMenu m; const int cur = proc.programOf (machine);
        m.addSectionHeader ("Machine " + machLetter());
        for (int b = 1; b <= l480::kNumBanks; ++b)
        {
            PopupMenu sub; bool has = false;
            for (int i = 0; i < l480::numPrograms(); ++i)
            {
                const auto& p = l480::programs()[(size_t) i];
                if (p.bank != b) continue;
                sub.addItem (i + 1, String (p.bank) + "." + String (p.num) + "  " + p.name, true, i == cur && proc.registerName[machine].isEmpty());
                has = true;
            }
            if (has) m.addSubMenu (String (b) + "  " + l480::bankName (b), sub, true, nullptr, proc.prog (machine).bank == b);
        }
        PopupMenu regs; bool anyReg = false;
        for (int b = 1; b <= 10; ++b)
            for (int n = 1; n <= 10; ++n)
                if (proc.registerExists (b, n))
                {
                    regs.addItem (10000 + b * 100 + n, "R" + String (b % 10) + "." + String (n % 10) + "  " + proc.registerTitle (b, n), true,
                                  proc.registerName[machine] == String (b) + "." + String (n));
                    anyReg = true;
                }
        m.addSeparator();
        if (anyReg) m.addSubMenu ("Registers", regs); else m.addItem (-1, "Registers: none stored (STO)", false);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&display).withMinimumWidth (240).withStandardItemHeight (22), [this] (int r)
        {
            if (r >= 10000) { const int b = (r - 10000) / 100, n = (r - 10000) % 100; if (proc.recallRegister (machine, b, n)) { mode = Mode::Reg; regBank = -1; } }
            else if (r > 0) { proc.loadProgram (machine, r - 1); browseBank = proc.prog (machine).bank; mode = Mode::Prog; }
            else return;
            cancelStore(); ctrl = false; page = 0; refreshPage (true); updateLeds(); msgUntil = 0;
        });
    }

    // ---------------------------------------------------------------- 30 Hz: display, meters, sliders
    void timerCallback() override
    {
        const uint32 now = Time::getMillisecondCounter();
        if (config() == 0 && machine == 1) { machine = 0; refreshPage (true); content.repaint(); }
        if (proc.programLoaded.exchange (false)) { refreshPage (false); if (mode != Mode::Prog || browseBank != proc.prog (machine).bank) {} }
        String l1, l2;
        const bool focused = focus >= 0 && now - focusTime < 2500;
        if (now < msgUntil) { l1 = msg1; l2 = msg2; }
        else
        {
            const bool edited = proc.isEdited (machine) && proc.registerName[machine].isEmpty() && proc.variation[machine] <= 1;
            l1 = machLetter() + " " + loadedTag (machine) + " " + String (proc.prog (machine).name) + (edited ? "*" : "");
            if (store == Store::RegBank)      l2 = "STO REG BANK 1-0";
            else if (store == Store::RegNum)  l2 = "STO REG " + String (storeBank % 10) + ". 1-0";
            else if (store == Store::Var)     l2 = "STO VAR 2-0";
            else if (focused)                 l2 = faderText (focus);
            else if (ctrl)                    l2 = "CTRL " + String (ctrlPage + 1) + " " + kCtrlPageName[ctrlPage];
            else switch (mode)
            {
                case Mode::Bank: l2 = "BANK " + String (bankGroup * 10 + 1) + "-" + String (jmin (l480::kNumBanks, bankGroup * 10 + 10)); break;
                case Mode::Prog:
                    if (browseBank != proc.prog (machine).bank) l2 = String (browseBank) + " " + l480::bankName (browseBank);
                    else
                    {
                        const auto st = proc.machineStatus (machine);
                        l2 = st.isNotEmpty() ? st : String (l480::algoDef (proc.algoOf (machine)).name) + (pages() > 1 ? " P" + String (page + 1) : String());
                    }
                    break;
                case Mode::Var:  l2 = "VARIATION " + String (proc.variation[machine] >= 1 ? String (proc.variation[machine] % 10) : String ("-")); break;
                case Mode::Reg:  l2 = regBank < 0 ? "REG BANK 1-0" : "REG " + String (regBank % 10) + ". 1-0"; break;
            }
        }
        if (l1 != display.line1 || l2 != display.line2) { display.line1 = l1; display.line2 = l2; display.repaint(); }
        const int hi = focused ? focus : -1;
        if (hi != sliderDisplay.hi) { sliderDisplay.hi = hi; sliderDisplay.repaint(); }
        // headroom meters
        const float mo = proc.meterOut.load();
        display.mL = mo > display.mL ? mo : display.mL * 0.86f; display.mR = display.mL;
        display.repaint (display.getLocalBounds().removeFromLeft (40));
        // sliders follow their parameters (smooth motion when a program loads)
        for (int i = 0; i < 6; ++i)
        {
            if (fader[i]->enabled && ! (! ctrl && isTrigger (i))) fader[i]->setTarget (faderValue (i));
            if (fader[i]->animate()) fader[i]->repaint();
        }
        updateLeds();
        if (drawerOpen)
        {
            for (auto* k : { eqOnKey.get(), cpOnKey.get(), satKey.get(), duckKey.get() }) k->sync();
            curveKey->setLit (proc.curveOn[machine]);
            const float rt = proc.mainRt (machine);
            const int sig = proc.programOf (machine) * 31 + machine * 7 + (proc.curveOn[machine] ? 1 : 0) + (int) proc.curvePts[machine].size() * 1000;
            if (rt != lastRt || sig != lastSig) { lastRt = rt; lastSig = sig; graph.repaint(); }
        }
    }
    float lastRt = -1; int lastSig = -1;

    // ---------------------------------------------------------------- members
    Flexiverb480Processor& proc;
    FlexLnf lnf;
    Content content; Drawer drawer; bool drawerOpen = false;
    DecayGraph graph { proc };
    std::unique_ptr<LarcKey> curveKey, resetKey;
    std::unique_ptr<DualKnob> eqKnob[4]; std::unique_ptr<ScaleKnob> cpKnob[7];
    std::unique_ptr<ParamKey> eqOnKey, cpOnKey, satKey, duckKey;
    LarcDisplay display; SliderDisplay sliderDisplay;
    std::unique_ptr<LarcKey> modeKey[4], numKey[10], pageKey, ctrlKey, machKey, muteKey, stoKey;
    std::unique_ptr<LarcFader> fader[6];
    bool triggered[6] = {};
    Rectangle<int> modeArea, numArea, fnArea; Rectangle<float> namePlate;
    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Flexiverb480Editor)
};
