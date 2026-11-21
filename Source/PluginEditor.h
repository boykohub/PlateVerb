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
        const float h = jmin (txt.getHeight() * 0.34f, 21.0f);
        auto r1 = txt.removeFromTop (txt.getHeight() * 0.5f), r2 = txt;
        segLine (g, line1, r1, h, 16); segLine (g, line2, r2, h, 16);
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

// ============================================================================ CTRL pages: setup on the sliders (the 480L's control mode)
struct CtrlSlot { const char* lab; const char* id; const char* name; };
static constexpr int kCtrlPages = 8;
static const CtrlSlot kCtrl[kCtrlPages][6] = {
    { { "CNFG", "config", "Configuration" }, { "OUT", "out", "Output Level" }, { "HW", "hwOn", "480L Hardware" },
      { "IN", "hwIn", "Input Type" }, { "OUTT", "hwOut", "Output Type" }, { "NOIS", "hwNoise", "Converter Noise" } },
    { { "XFIN", "hwXfIn", "Input Transformers" }, { "XFOU", "hwXfOut", "Output Transformers" }, { "IRON", "trOn", "Iron" },
      { "MODL", "trModel", "Iron Model" }, { "FREQ", "trFreq", "Iron Frequency" }, { "AMT", "trAmt", "Iron Amount" } },
    { { "EQ", "eqOn", "EQ In" }, { "LOW", "eqG0", "EQ Low Gain" }, { "LMID", "eqG1", "EQ Low Mid Gain" },
      { "HMID", "eqG2", "EQ High Mid Gain" }, { "HIGH", "eqG3", "EQ High Gain" }, {} },
    { {}, { "LOF", "eqF0", "EQ Low Freq" }, { "LMF", "eqF1", "EQ Low Mid Freq" },
      { "HMF", "eqF2", "EQ High Mid Freq" }, { "HIF", "eqF3", "EQ High Freq" }, {} },
    { { "COMP", "cpOn", "Compressor In" }, { "TYPE", "cpType", "Comp Type" }, { "THR", "cpThr", "Threshold" },
      { "RAT", "cpRatio", "Ratio" }, { "ATK", "cpAtk", "Attack" }, { "REL", "cpRel", "Release" } },
    { { "GAIN", "cpMakeup", "Makeup Gain" }, { "MIX", "cpMix", "Comp Mix" }, { "SAT", "cpColor", "Colour" },
      { "DUCK", "cpDuck", "Ducking" }, {}, {} },
    { { "DLY", "dlOn", "Delay In" }, { "SYNC", "dlSync", "Tempo Sync" }, { "TIML", "dlTimeL", "Delay Left" },
      { "TIMR", "dlTimeR", "Delay Right" }, { "FBK", "dlFb", "Feedback" }, { "MIX", "dlMix", "Delay Mix" } },
    { { "PING", "dlPing", "Ping-Pong" }, { "LOW", "dlLow", "Delay Low" }, { "HIGH", "dlHigh", "Delay High" }, {}, {}, {} },
};
static const char* kCtrlPageName[kCtrlPages] = { "SETUP", "IRON", "EQ GAIN", "EQ FREQ", "COMPRESSOR", "COMP 2", "DELAY", "DELAY 2" };

struct FlexLnf : public LookAndFeel_V4
{
    Font getPopupMenuFont() override { return ui (13.5f, W::Medium); }
};

// ============================================================================ the editor
class Flexiverb480Editor : public AudioProcessorEditor, private Timer
{
public:
    static constexpr int kW = 452, kH = 640;
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

        lnf.setColour (PopupMenu::backgroundColourId, Colour (0xff1c1d1f)); lnf.setColour (PopupMenu::textColourId, Colour (0xffe8e6e1));
        lnf.setColour (PopupMenu::highlightedBackgroundColourId, larc::led.darker (0.2f)); lnf.setColour (PopupMenu::highlightedTextColourId, Colours::white);
        lnf.setColour (PopupMenu::headerTextColourId, Colour (0xff9a968e));
        lnf.setDefaultSansSerifTypeface (FlexFonts::get().medium);
        setLookAndFeel (&lnf);
        layout();
        setResizable (false, false);
        applySize();
        refreshPage (true);
        updateLeds();
        startTimerHz (30);
    }
    ~Flexiverb480Editor() override { stopTimer(); setLookAndFeel (nullptr); }

    void paint (Graphics& g) override { g.fillAll (larc::cheek); }
    void resized() override {}

    class Content : public Component
    {
    public:
        Flexiverb480Editor* ed = nullptr;
        void paint (Graphics& g) override { if (ed) ed->paintPanel (g); }
        void mouseDown (const MouseEvent& e) override { if (ed && e.mods.isPopupMenu()) ed->showSizeMenu(); }
    };

private:
    // ---------------------------------------------------------------- panel
    void paintPanel (Graphics& g)
    {
        auto r = Rectangle<float> (0, 0, (float) kW, (float) kH);
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
    }

    void layout()
    {
        const int x0 = 30, x1 = kW - 30, wAll = x1 - x0;
        display.setBounds (x0, 26, wAll, 96);
        const int kh = 34, gap = 6;
        int y = 150;
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
    void applySize()
    {
        content.setBounds (0, 0, kW, kH);
        content.setTransform (AffineTransform::scale (scale));
        setSize (roundToInt (kW * scale), roundToInt (kH * scale));
    }
    void showSizeMenu()
    {
        PopupMenu m; m.addSectionHeader ("Window size");
        const float sizes[] = { 0.75f, 0.9f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };
        for (int i = 0; i < 7; ++i) m.addItem (i + 1, String (roundToInt (sizes[i] * 100)) + "%", true, std::abs (sizes[i] - scale) < 0.01f);
        m.showMenuAsync (PopupMenu::Options(), [this, sizes] (int r) { if (r > 0) { scale = sizes[r - 1]; proc.setUiProperty ("uiScale", scale); applySize(); } });
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
    }

    // ---------------------------------------------------------------- members
    Flexiverb480Processor& proc;
    FlexLnf lnf;
    Content content;
    LarcDisplay display; SliderDisplay sliderDisplay;
    std::unique_ptr<LarcKey> modeKey[4], numKey[10], pageKey, ctrlKey, machKey, muteKey, stoKey;
    std::unique_ptr<LarcFader> fader[6];
    bool triggered[6] = {};
    Rectangle<int> modeArea, numArea, fnArea; Rectangle<float> namePlate;
    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Flexiverb480Editor)
};
