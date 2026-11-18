// ChannelStrip editor (included by CSPlugin.cpp). Original vintage-rack faceplate: charcoal panel,
// cream engraving, black bakelite knobs, amber power lamps, analog VU. Modules can be dragged by
// their title bar to change the signal order.

namespace csui
{
    static const Colour panel     { 0xff232427 };
    static const Colour panelHi   { 0xff313236 };
    static const Colour panelLo   { 0xff18191b };
    static const Colour engrave   { 0xffe9dcbc };
    static const Colour engraveDim{ 0xff9d937b };
    static const Colour amber     { 0xffffb347 };
    static const Colour amberOff  { 0xff4a3a24 };
    static const Colour screen    { 0xff0f1412 };
    static const Colour curve     { 0xfff3c26b };
    static const Colour band[4]   { Colour (0xffff8a5b), Colour (0xffffd166), Colour (0xff7bd389), Colour (0xff7cc4ff) };
    static Font font (float h, bool bold = false) { return Font (FontOptions (h, bold ? Font::bold : Font::plain)); }
}

class CSLook : public LookAndFeel_V4
{
public:
    CSLook()
    {
        setColour (ComboBox::backgroundColourId, csui::panelLo);
        setColour (ComboBox::textColourId, csui::engrave);
        setColour (ComboBox::outlineColourId, csui::engraveDim.withAlpha (0.5f));
        setColour (ComboBox::arrowColourId, csui::engrave);
        setColour (PopupMenu::backgroundColourId, Colour (0xff1c1d20));
        setColour (PopupMenu::textColourId, csui::engrave);
        setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff5a4626));
        setColour (PopupMenu::highlightedTextColourId, Colours::white);
        setColour (Slider::textBoxTextColourId, csui::engrave);
        setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (Slider::textBoxHighlightColourId, Colour (0xff5a4626));
        setColour (TextEditor::textColourId, csui::engrave);
        setColour (TextEditor::backgroundColourId, csui::panelLo);
        setColour (Label::textColourId, csui::engrave);
        setColour (TooltipWindow::backgroundColourId, Colour (0xff1c1d20));
        setColour (TooltipWindow::textColourId, csui::engrave);
    }
    Font getComboBoxFont (ComboBox&) override { return csui::font (12.5f, true); }
    Font getPopupMenuFont() override { return csui::font (14.0f); }
    Font getLabelFont (Label& l) override { return l.getFont(); }

    void drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s) override
    {
        auto b = Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
        const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        // scale ticks
        g.setColour (csui::engraveDim);
        for (int i = 0; i <= 10; ++i)
        {
            const float t = a0 + (a1 - a0) * (float) i / 10.0f;
            g.drawLine (Line<float> (c.getPointOnCircumference (r - 1.0f, t), c.getPointOnCircumference (r - (i % 5 == 0 ? 5.0f : 3.0f), t)), 1.0f);
        }
        // bipolar knobs light the arc from centre
        const bool bip = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float from = bip ? a0 + (a1 - a0) * (float) s.valueToProportionOfLength (0.0) : a0;
        Path arc; arc.addCentredArc (c.x, c.y, r - 7.0f, r - 7.0f, 0.0f, jmin (from, ang), jmax (from, ang), true);
        g.setColour (csui::amber.withAlpha (s.isEnabled() ? 0.85f : 0.25f));
        g.strokePath (arc, PathStrokeType (2.0f));
        // skirt + bakelite cap
        const float kr = r - 10.0f;
        g.setColour (Colours::black.withAlpha (0.45f)); g.fillEllipse (c.x - kr, c.y - kr + 2.5f, kr * 2, kr * 2);
        g.setGradientFill (ColourGradient (Colour (0xffb9b6ae), c.x, c.y - kr, Colour (0xff55534f), c.x, c.y + kr, false));
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
        const float cr = kr * 0.78f;
        g.setGradientFill (ColourGradient (Colour (0xff3b3b3d), c.x, c.y - cr, Colour (0xff0d0d0e), c.x, c.y + cr, false));
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);
        g.setColour (Colours::white.withAlpha (0.08f)); g.drawEllipse (c.x - cr + 1, c.y - cr + 1, cr * 2 - 2, cr * 2 - 2, 1.0f);
        g.setColour (s.isEnabled() ? Colour (0xfff4efe2) : csui::engraveDim);
        g.drawLine (Line<float> (c.getPointOnCircumference (cr * 0.25f, ang), c.getPointOnCircumference (kr - 1.0f, ang)), 2.6f);
    }

    void drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        g.setColour (Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (r.translated (0, 1.2f), 3.0f);
        g.setGradientFill (ColourGradient (on ? Colour (0xff6a5430) : Colour (0xff3a3b3e), r.getX(), r.getY(),
                                           on ? Colour (0xff3f3119) : Colour (0xff222326), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r.translated (0, down ? 0.8f : 0.0f), 3.0f);
        g.setColour (on ? csui::amber.withAlpha (0.8f) : csui::engraveDim.withAlpha (0.35f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        if (over) { g.setColour (Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (r, 3.0f); }
    }
    void drawButtonText (Graphics& g, TextButton& b, bool, bool) override
    {
        g.setColour (b.getToggleState() ? Colour (0xffffe3b0) : csui::engrave);
        g.setFont (csui::font (11.5f, true));
        g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred, false);
    }
};

// slider with Option/Alt fine adjustment (as in PlateVerb)
class FineKnob : public Slider
{
public:
    FineKnob() : Slider (Slider::RotaryHorizontalVerticalDrag, Slider::TextBoxBelow)
    {
        setTextBoxStyle (Slider::TextBoxBelow, false, 74, 15);
        setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
    }
    void mouseDown (const MouseEvent& e) override { Slider::mouseDown (e); last = e.position; prop = valueToProportionOfLength (getValue()); }
    void mouseDrag (const MouseEvent& e) override
    {
        if (e.mods.isAltDown())
        {
            const auto d = e.position - last; last = e.position;
            prop = jlimit (0.0, 1.0, prop + (double) (d.x - d.y) / 3000.0);
            setValue (proportionOfLengthToValue (prop), sendNotificationSync); return;
        }
        Slider::mouseDrag (e); last = e.position; prop = valueToProportionOfLength (getValue());
    }
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        if (e.mods.isAltDown()) { auto w2 = w; w2.deltaY *= 0.1f; w2.deltaX *= 0.1f; Slider::mouseWheelMove (e, w2); }
        else Slider::mouseWheelMove (e, w);
    }
private:
    Point<float> last; double prop = 0.0;
};

// one knob with its caption
struct Knob
{
    FineKnob s; Label cap; std::unique_ptr<APVTS::SliderAttachment> att;
    void init (Component& parent, APVTS& ap, const String& id, const String& caption, double dblClick)
    {
        att = std::make_unique<APVTS::SliderAttachment> (ap, id, s);
        s.setDoubleClickReturnValue (true, dblClick);
        cap.setText (caption, dontSendNotification); cap.setJustificationType (Justification::centred);
        cap.setFont (csui::font (11.5f, true)); cap.setInterceptsMouseClicks (false, false);
        parent.addAndMakeVisible (s); parent.addAndMakeVisible (cap);
    }
    void setBounds (Rectangle<int> r) { cap.setBounds (r.removeFromTop (14)); s.setBounds (r); }
    void setVisible (bool v) { s.setVisible (v); cap.setVisible (v); }
};
static double paramDefault (APVTS& ap, const String& id)
{
    auto* p = ap.getParameter (id); return p != nullptr ? (double) p->convertFrom0to1 (p->getDefaultValue()) : 0.0;
}

struct Toggle
{
    TextButton b; std::unique_ptr<APVTS::ButtonAttachment> att;
    void init (Component& parent, APVTS& ap, const String& id, const String& text)
    {
        b.setButtonText (text); b.setClickingTogglesState (true);
        att = std::make_unique<APVTS::ButtonAttachment> (ap, id, b); parent.addAndMakeVisible (b);
    }
};
struct Choice
{
    ComboBox c; std::unique_ptr<APVTS::ComboBoxAttachment> att;
    void init (Component& parent, APVTS& ap, const String& id)
    {
        if (auto* p = dynamic_cast<AudioParameterChoice*> (ap.getParameter (id))) c.addItemList (p->choices, 1);
        att = std::make_unique<APVTS::ComboBoxAttachment> (ap, id, c); parent.addAndMakeVisible (c);
    }
};

// power lamp in a module's title bar
class Lamp : public Button
{
public:
    Lamp() : Button ("power") { setClickingTogglesState (true); }
    void paintButton (Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f); const float d = jmin (r.getWidth(), r.getHeight());
        auto c = r.withSizeKeepingCentre (d, d);
        const bool on = getToggleState();
        if (on) { g.setColour (csui::amber.withAlpha (0.35f)); g.fillEllipse (c.expanded (2.5f)); }
        g.setGradientFill (ColourGradient (on ? Colour (0xffffe0a0) : csui::amberOff.brighter (0.2f), c.getCentreX(), c.getY(),
                                           on ? Colour (0xffd17a12) : csui::amberOff, c.getCentreX(), c.getBottom(), false));
        g.fillEllipse (c);
        g.setColour (Colours::black.withAlpha (0.6f)); g.drawEllipse (c, 1.0f);
        if (over) { g.setColour (Colours::white.withAlpha (0.15f)); g.fillEllipse (c); }
    }
};

// horizontal gain-reduction / level bar
class Bar : public Component
{
public:
    std::function<float()> get; bool reduction = true; float range = 24.0f; String text;
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (csui::screen); g.fillRoundedRectangle (r, 2.0f);
        const float v = get ? get() : 0.0f;
        float frac;
        if (reduction) frac = jlimit (0.0f, 1.0f, v / range);
        else frac = jlimit (0.0f, 1.0f, (20.0f * std::log10 (v + 1e-9f) + 60.0f) / 66.0f);
        auto f = r.reduced (1.5f);
        if (reduction) f = f.withLeft (f.getRight() - f.getWidth() * frac); else f = f.withWidth (f.getWidth() * frac);
        g.setColour (reduction ? csui::amber : (v > 0.98f ? Colour (0xffff5b4d) : Colour (0xff7bd389)));
        g.fillRect (f);
        g.setColour (csui::engraveDim); g.setFont (csui::font (10.0f, true));
        g.drawText (text, getLocalBounds().reduced (3, 0), reduction ? Justification::centredLeft : Justification::centredRight, false);
    }
};

class ChannelStripEditor;

// ============================================================================ module panel base
class ModulePanel : public Component
{
public:
    ModulePanel (ChannelStripProcessor& p, int mod, const String& onId) : proc (p), module (mod)
    {
        lampAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, onId, lamp);
        addAndMakeVisible (lamp);
        lamp.setTooltip ("Module on / off");
    }
    static constexpr int kHeader = 30;
    std::function<void (ModulePanel&, const MouseEvent&, int phase)> onDrag;   // 0 = down, 1 = drag, 2 = up
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setGradientFill (ColourGradient (csui::panelHi, 0, 0, csui::panel, 0, r.getHeight(), false)); g.fillRect (r);
        g.setColour (Colours::black.withAlpha (0.55f)); g.drawRect (r, 1.0f);
        g.setColour (Colours::white.withAlpha (0.05f)); g.drawLine (1.0f, 1.0f, r.getRight() - 1.0f, 1.0f);
        auto h = r.removeFromTop ((float) kHeader);
        g.setColour (Colours::black.withAlpha (0.25f)); g.fillRect (h);
        g.setColour (lamp.getToggleState() ? csui::engrave : csui::engraveDim);
        g.setFont (csui::font (14.0f, true));
        g.drawText (cs::moduleName (module), h.withTrimmedLeft (30.0f).toNearestInt(), Justification::centredLeft, false);
        // grip
        g.setColour (csui::engraveDim.withAlpha (0.6f));
        for (int i = 0; i < 3; ++i) g.fillRect (h.getRight() - 16.0f, h.getY() + 10.0f + (float) i * 4.0f, 10.0f, 1.5f);
        if (dragging) { g.setColour (csui::amber.withAlpha (0.5f)); g.drawRect (getLocalBounds().toFloat(), 2.0f); }
        paintBody (g);
    }
    virtual void paintBody (Graphics&) {}
    virtual void layout (Rectangle<int> body) = 0;
    virtual void refresh() {}
    void resized() override
    {
        lamp.setBounds (6, 6, 18, 18);
        layout (getLocalBounds().withTrimmedTop (kHeader).reduced (6, 4));
    }
    void mouseDown (const MouseEvent& e) override { if (e.y < kHeader && onDrag) { dragging = true; onDrag (*this, e, 0); repaint(); } }
    void mouseDrag (const MouseEvent& e) override { if (dragging && onDrag) onDrag (*this, e, 1); }
    void mouseUp (const MouseEvent& e) override { if (dragging && onDrag) { dragging = false; onDrag (*this, e, 2); repaint(); } }
    void mouseMove (const MouseEvent& e) override { setMouseCursor (e.y < kHeader ? MouseCursor::DraggingHandCursor : MouseCursor::NormalCursor); }

    ChannelStripProcessor& proc; const int module; Lamp lamp; std::unique_ptr<APVTS::ButtonAttachment> lampAtt; bool dragging = false;
    float width = 160.0f;
};

// ---------------------------------------------------------------- PRE
class PrePanel : public ModulePanel
{
public:
    PrePanel (ChannelStripProcessor& p) : ModulePanel (p, cs::ModPre, "preOn")
    {
        width = 176; auto& ap = p.apvts;
        model.init (*this, ap, "preModel");
        drive.init (*this, ap, "preDrive", "DRIVE", 0.0); out.init (*this, ap, "preOut", "OUTPUT", 0.0);
        drive.s.setTooltip ("Drive into the preamp model; the output is level-compensated");
    }
    void layout (Rectangle<int> r) override
    {
        model.c.setBounds (r.removeFromTop (24)); r.removeFromTop (12);
        drive.setBounds (r.removeFromTop (118).withSizeKeepingCentre (96, 118)); r.removeFromTop (8);
        out.setBounds (r.removeFromTop (100).withSizeKeepingCentre (80, 100));
    }
    Choice model; Knob drive, out;
};

// ---------------------------------------------------------------- FILTER
class FilterPanel : public ModulePanel
{
public:
    FilterPanel (ChannelStripProcessor& p) : ModulePanel (p, cs::ModFilter, "fltOn")
    {
        width = 176; auto& ap = p.apvts;
        hpOn.init (*this, ap, "hpOn", "LOW CUT"); lpOn.init (*this, ap, "lpOn", "HIGH CUT");
        hpF.init (*this, ap, "hpFreq", "", paramDefault (ap, "hpFreq")); lpF.init (*this, ap, "lpFreq", "", paramDefault (ap, "lpFreq"));
        hpS.init (*this, ap, "hpSlope"); lpS.init (*this, ap, "lpSlope"); hpM.init (*this, ap, "hpMode"); lpM.init (*this, ap, "lpMode");
        chr.init (*this, ap, "fltChar");
        auto fmt = [] (double v) { return v >= 1000.0 ? String (v / 1000.0, v >= 10000.0 ? 1 : 2) + " kHz" : String ((int) std::lround (v)) + " Hz"; };
        hpF.s.textFromValueFunction = [this, fmt] (double v) {
            const int m = hpM.c.getSelectedItemIndex(); return fmt (ChannelStripProcessor::hpStep (m, (float) v)); };
        lpF.s.textFromValueFunction = [this, fmt] (double v) {
            const int m = lpM.c.getSelectedItemIndex(); return fmt (ChannelStripProcessor::lpStep (m, (float) v)); };
    }
    void layout (Rectangle<int> r) override
    {
        auto col = [&] (Rectangle<int> c, Toggle& on, Knob& k, Choice& s, Choice& m) {
            on.b.setBounds (c.removeFromTop (22)); c.removeFromTop (2);
            k.setBounds (c.removeFromTop (84).withSizeKeepingCentre (76, 84));
            s.c.setBounds (c.removeFromTop (20)); c.removeFromTop (3); m.c.setBounds (c.removeFromTop (20));
        };
        auto top = r.removeFromTop (154); col (top, hpOn, hpF, hpS, hpM);
        r.removeFromTop (8);
        auto mid = r.removeFromTop (154); col (mid, lpOn, lpF, lpS, lpM);
        r.removeFromTop (6); chr.c.setBounds (r.removeFromTop (20));
    }
    void refresh() override { hpS.c.setEnabled (hpM.c.getSelectedItemIndex() == 0); hpF.s.updateText(); lpF.s.updateText(); }
    Toggle hpOn, lpOn; Knob hpF, lpF; Choice hpS, lpS, hpM, lpM, chr;
};

// ---------------------------------------------------------------- GATE
class GatePanel : public ModulePanel
{
public:
    GatePanel (ChannelStripProcessor& p) : ModulePanel (p, cs::ModGate, "gtOn")
    {
        width = 206; auto& ap = p.apvts;
        mode.init (*this, ap, "gtMode");
        thr.init (*this, ap, "gtThr", "THRESH", paramDefault (ap, "gtThr")); range.init (*this, ap, "gtRange", "RANGE", paramDefault (ap, "gtRange"));
        atk.init (*this, ap, "gtAtk", "ATTACK", paramDefault (ap, "gtAtk")); hold.init (*this, ap, "gtHold", "HOLD", paramDefault (ap, "gtHold"));
        rel.init (*this, ap, "gtRel", "RELEASE", paramDefault (ap, "gtRel")); hyst.init (*this, ap, "gtHyst", "HYST", paramDefault (ap, "gtHyst"));
        key.init (*this, ap, "gtKey"); khp.init (*this, ap, "gtKeyHp", "KEY HPF", 20.0); klp.init (*this, ap, "gtKeyLp", "KEY LPF", 20000.0);
        listen.init (*this, ap, "gtListen", "LISTEN");
        gr.get = [&p] { return p.gateGr.load(); }; gr.range = 40.0f; gr.text = "GR"; addAndMakeVisible (gr);
    }
    void layout (Rectangle<int> r) override
    {
        mode.c.setBounds (r.removeFromTop (22)); r.removeFromTop (4);
        auto row = [&] (Knob& a, Knob& b) { auto rr = r.removeFromTop (76); a.setBounds (rr.removeFromLeft (rr.getWidth() / 2).reduced (2, 0)); b.setBounds (rr.reduced (2, 0)); };
        row (thr, range); row (atk, hold); row (rel, hyst);
        auto kr = r.removeFromTop (22); key.c.setBounds (kr.removeFromLeft (kr.getWidth() - 64)); listen.b.setBounds (kr.withTrimmedLeft (4));
        r.removeFromTop (2); row (khp, klp);
        r.removeFromTop (4); gr.setBounds (r.removeFromTop (12));
    }
    Choice mode, key; Knob thr, range, atk, hold, rel, hyst, khp, klp; Toggle listen; Bar gr;
};

// ---------------------------------------------------------------- COMP (PlateVerb compressor)
class CompPanel : public ModulePanel
{
public:
    CompPanel (ChannelStripProcessor& p) : ModulePanel (p, cs::ModComp, "cpOn")
    {
        width = 236; auto& ap = p.apvts;
        type.init (*this, ap, "cpType"); tone.init (*this, ap, "cpTone");
        thr.init (*this, ap, "cpThr", "THRESHOLD", paramDefault (ap, "cpThr")); ratio.init (*this, ap, "cpRatio", "RATIO", paramDefault (ap, "cpRatio"));
        atk.init (*this, ap, "cpAtk", "ATTACK", paramDefault (ap, "cpAtk")); rel.init (*this, ap, "cpRel", "RELEASE", paramDefault (ap, "cpRel"));
        mk.init (*this, ap, "cpMakeup", "MAKEUP", 0.0); mix.init (*this, ap, "cpMix", "MIX", 100.0);
        key.init (*this, ap, "cpKey"); sc.init (*this, ap, "cpScHp", "SC HPF", 20.0);
        lpf.init (*this, ap, "cpLpFreq", "LOW PASS", 40000.0); lpm.init (*this, ap, "cpLpMode");
        atk.s.textFromValueFunction = [] (double v) { return v < 0.001 ? String (v * 1.0e6, 0) + " us" : String (v * 1000.0, v < 0.01 ? 2 : 1) + " ms"; };
        rel.s.textFromValueFunction = [] (double v) { return v < 1.0 ? String (v * 1000.0, 0) + " ms" : String (v, 2) + " s"; };
        lpf.s.textFromValueFunction = [] (double v) { return v >= 39000.0 ? String ("OFF") : String (v / 1000.0, 1) + " kHz"; };
        sc.s.textFromValueFunction = [] (double v) { return v <= 20.5 ? String ("OFF") : String ((int) std::lround (v)) + " Hz"; };
        gr.get = [&p] { return p.compGr.load(); }; gr.text = "GR"; addAndMakeVisible (gr);
    }
    void layout (Rectangle<int> r) override
    {
        auto t = r.removeFromTop (22); type.c.setBounds (t.removeFromLeft (t.getWidth() - 80)); tone.c.setBounds (t.withTrimmedLeft (4));
        r.removeFromTop (4);
        const int w3 = r.getWidth() / 3;
        auto row1 = r.removeFromTop (80); thr.setBounds (row1.removeFromLeft (w3)); ratio.setBounds (row1.removeFromLeft (w3)); mk.setBounds (row1);
        auto row2 = r.removeFromTop (80); atk.setBounds (row2.removeFromLeft (w3)); rel.setBounds (row2.removeFromLeft (w3)); mix.setBounds (row2);
        r.removeFromTop (4);
        auto k = r.removeFromTop (22); key.c.setBounds (k.removeFromLeft (k.getWidth() / 2 - 2)); lpm.c.setBounds (k.withTrimmedLeft (4));
        auto row3 = r.removeFromTop (80); sc.setBounds (row3.removeFromLeft (row3.getWidth() / 2)); lpf.setBounds (row3);
        r.removeFromTop (6); gr.setBounds (r.removeFromTop (14));
    }
    void refresh() override
    {
        const bool opto = type.c.getSelectedItemIndex() == pv::CompOpto;   // LA-2A: fixed attack, programme release
        atk.setVisible (! opto); rel.setVisible (! opto);
    }
    Choice type, tone, key, lpm; Knob thr, ratio, atk, rel, mk, mix, sc, lpf; Bar gr;
};

// ---------------------------------------------------------------- LIMIT
class LimPanel : public ModulePanel
{
public:
    LimPanel (ChannelStripProcessor& p) : ModulePanel (p, cs::ModLim, "lmOn")
    {
        width = 136; auto& ap = p.apvts;
        drv.init (*this, ap, "lmDrive", "DRIVE", 0.0); ceil.init (*this, ap, "lmCeil", "CEILING", paramDefault (ap, "lmCeil"));
        rel.init (*this, ap, "lmRel", "RELEASE", paramDefault (ap, "lmRel"));
        gr.get = [&p] { return p.limGr.load(); }; gr.range = 12.0f; gr.text = "GR"; addAndMakeVisible (gr);
    }
    void layout (Rectangle<int> r) override
    {
        r.removeFromTop (26);
        drv.setBounds (r.removeFromTop (92).withSizeKeepingCentre (84, 92)); r.removeFromTop (4);
        ceil.setBounds (r.removeFromTop (92).withSizeKeepingCentre (84, 92)); r.removeFromTop (4);
        rel.setBounds (r.removeFromTop (92).withSizeKeepingCentre (84, 92));
        r.removeFromTop (10); gr.setBounds (r.removeFromTop (14));
    }
    Knob drv, ceil, rel; Bar gr;
};

// ---------------------------------------------------------------- EQ
class EqGraph : public Component, private Timer
{
public:
    EqGraph (ChannelStripProcessor& p) : proc (p) { startTimerHz (30); }
    static constexpr float fMin = 20.0f, fMax = 20000.0f, gMax = 20.0f;
    float xOf (float f) const { return (float) getWidth() * std::log (f / fMin) / std::log (fMax / fMin); }
    float fOf (float x) const { return fMin * std::pow (fMax / fMin, jlimit (0.0f, 1.0f, x / (float) getWidth())); }
    float yOf (float g) const { return (float) getHeight() * (0.5f - g / (2.0f * gMax) * 0.9f); }
    float gOf (float y) const { return (0.5f - y / (float) getHeight()) * 2.0f * gMax / 0.9f; }

    float pval (const String& id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    cs::BandSettings band (int b) const
    {
        cs::BandSettings s; s.on = pval (bid (b, "On")) > 0.5f; s.model = (int) pval (bid (b, "Model")); s.shelf = pval (bid (b, "Shape")) > 0.5f;
        s.freq = pval (bid (b, "Freq")); s.gain = pval (bid (b, "Gain")); s.qScale = pval (bid (b, "Q")); s.stepped = pval (bid (b, "Step")) > 0.5f;
        s.aux = pval (bid (b, "Aux")); s.auxSel = (int) pval (bid (b, "AuxSel")); return s;
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (csui::screen); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (Colours::white.withAlpha (0.06f));
        for (float f : { 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f }) g.drawVerticalLine ((int) xOf (f), 0, r.getHeight());
        for (float db : { -12.f, -6.f, 6.f, 12.f }) g.drawHorizontalLine ((int) yOf (db), 0, r.getWidth());
        g.setColour (Colours::white.withAlpha (0.14f)); g.drawHorizontalLine ((int) yOf (0), 0, r.getWidth());
        g.setColour (csui::engraveDim.withAlpha (0.7f)); g.setFont (csui::font (9.5f));
        for (auto [f, t] : { std::pair<float, const char*> { 100.f, "100" }, { 1000.f, "1k" }, { 10000.f, "10k" } })
            g.drawText (t, (int) xOf (f) + 2, (int) r.getBottom() - 12, 30, 11, Justification::left);

        const double fs = 96000.0;
        cs::Section sec[4][3];
        const bool eqOn = pval ("eqOn") > 0.5f;
        for (int b = 0; b < 4; ++b) cs::designBand (band (b), b, (float) fs, sec[b]);
        cs::CutFilter hp, lp;
        const bool fOn = pval ("fltOn") > 0.5f;
        auto hpF = ChannelStripProcessor::hpStep ((int) pval ("hpMode"), pval ("hpFreq"));
        auto lpF = ChannelStripProcessor::lpStep ((int) pval ("lpMode"), pval ("lpFreq"));
        static const int hs[] = { 6, 12, 18, 24, 36 }, ls[] = { 6, 12, 18, 24 };
        hp.set (true, fOn && pval ("hpOn") > 0.5f, (float) fs, hpF, (int) pval ("hpMode") > 0 ? 18 : hs[jlimit (0, 4, (int) pval ("hpSlope"))], pval ("fltChar") > 0.5f);
        lp.set (false, fOn && pval ("lpOn") > 0.5f, (float) fs, lpF, ls[jlimit (0, 3, (int) pval ("lpSlope"))], pval ("fltChar") > 0.5f);
        auto cutMag = [] (const cs::CutFilter& c, double f, double fs_) {
            if (! c.active) return 0.0;
            double m = 0; for (int k = 0; k < c.nBq; ++k) m += cs::sectionMagDb (c.s[0][k], f, fs_);
            if (c.odd) { cs::Section s; s.fromFirst (c.o[0]); m += cs::sectionMagDb (s.bq, f, fs_); }
            return m;
        };
        Path p, pb[4];
        const int W = getWidth();
        for (int x = 0; x <= W; x += 2)
        {
            const double f = fOf ((float) x);
            double total = cutMag (hp, f, fs) + cutMag (lp, f, fs);
            for (int b = 0; b < 4; ++b)
            {
                double m = 0; for (int s = 0; s < 3; ++s) if (sec[b][s].active) m += cs::sectionMagDb (sec[b][s].bq, f, fs);
                if (eqOn) total += m;
                const float yy = yOf ((float) m);
                if (x == 0) pb[b].startNewSubPath ((float) x, yy); else pb[b].lineTo ((float) x, yy);
            }
            const float y = yOf ((float) jlimit (-30.0, 30.0, total));
            if (x == 0) p.startNewSubPath ((float) x, y); else p.lineTo ((float) x, y);
        }
        for (int b = 0; b < 4; ++b) { g.setColour (csui::band[b].withAlpha (0.35f)); g.strokePath (pb[b], PathStrokeType (1.0f)); }
        g.setColour (eqOn ? csui::curve : csui::engraveDim); g.strokePath (p, PathStrokeType (2.0f));
        for (int b = 0; b < 4; ++b)
        {
            const auto s = band (b);
            const float f = cs::effectiveFreq (s, b), gn = cs::effectiveGain (s, b);
            const float x = xOf (f), y = yOf (gn);
            g.setColour (csui::band[b].withAlpha (s.on ? 1.0f : 0.3f));
            g.fillEllipse (x - 6, y - 6, 12, 12);
            g.setColour (Colours::black); g.setFont (csui::font (9.0f, true));
            g.drawText (String (b + 1), (int) x - 6, (int) y - 6, 12, 12, Justification::centred);
        }
    }
    void mouseDown (const MouseEvent& e) override
    {
        drag = -1; float best = 15.0f;
        for (int b = 0; b < 4; ++b)
        {
            const auto s = band (b);
            const float d = e.position.getDistanceFrom ({ xOf (cs::effectiveFreq (s, b)), yOf (cs::effectiveGain (s, b)) });
            if (d < best) { best = d; drag = b; }
        }
        if (drag >= 0) for (auto* id : { "Freq", "Gain" }) if (auto* p = proc.apvts.getParameter (bid (drag, id))) p->beginChangeGesture();
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (drag < 0) return;
        auto set = [&] (const char* id, float v) { if (auto* p = proc.apvts.getParameter (bid (drag, id))) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        set ("Freq", jlimit (20.0f, 20000.0f, fOf (e.position.x)));
        if (! e.mods.isShiftDown()) set ("Gain", jlimit (-20.0f, 20.0f, gOf (e.position.y)));
    }
    void mouseUp (const MouseEvent&) override
    {
        if (drag >= 0) for (auto* id : { "Freq", "Gain" }) if (auto* p = proc.apvts.getParameter (bid (drag, id))) p->endChangeGesture();
        drag = -1;
    }
    void mouseDoubleClick (const MouseEvent& e) override
    {
        mouseDown (e);
        if (drag >= 0) if (auto* p = proc.apvts.getParameter (bid (drag, "Gain"))) p->setValueNotifyingHost (p->convertTo0to1 (0.0f));
        mouseUp (e);
    }
private:
    void timerCallback() override { repaint(); }
    ChannelStripProcessor& proc; int drag = -1;
};

class EqPanel : public ModulePanel
{
public:
    EqPanel (ChannelStripProcessor& p) : ModulePanel (p, cs::ModEq, "eqOn"), graph (p)
    {
        width = 520; auto& ap = p.apvts;
        addAndMakeVisible (graph);
        setAll.addItemList ({ "Set all bands...", }, 1);
        for (int i = 0; i < cs::kNumEqModels; ++i) setAll.addItem (cs::eqModelName (i), 100 + i);
        setAll.setSelectedId (1, dontSendNotification);
        setAll.onChange = [this] {
            const int id = setAll.getSelectedId();
            if (id >= 100) for (int b = 0; b < 4; ++b)
                if (auto* prm = proc.apvts.getParameter (bid (b, "Model"))) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (id - 100)));
            setAll.setSelectedId (1, dontSendNotification);
        };
        addAndMakeVisible (setAll);
        drive.init (*this, ap, "eqDrive", "", 50.0);
        drive.s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        drive.s.setTooltip ("Model colour (transformers, tubes, VCAs): 50 % = as measured");
        for (int b = 0; b < 4; ++b)
        {
            auto& c = col[b];
            c.on.init (*this, ap, bid (b, "On"), kBandNames[b]);
            c.model.init (*this, ap, bid (b, "Model"));
            c.freq.init (*this, ap, bid (b, "Freq"), "FREQ", kBandDefaultHz[b]);
            c.gain.init (*this, ap, bid (b, "Gain"), "GAIN", 0.0);
            c.q.init (*this, ap, bid (b, "Q"), "Q", 1.0);
            c.shape.init (*this, ap, bid (b, "Shape"), "SHELF");
            c.step.init (*this, ap, bid (b, "Step"), "ORIG STEPS");
            c.aux.init (*this, ap, bid (b, "Aux"), "ATTEN", 0.0);
            c.auxSel.init (*this, ap, bid (b, "AuxSel"));
            c.step.b.setTooltip ("Snap to the original unit's fixed frequencies (and gain steps on the 550)");
            c.q.s.setTooltip ("Q / bandwidth: 1.0x = the model's own curve (double-click)");
            const int bb = b;
            c.freq.s.textFromValueFunction = [this, bb] (double) {
                const float f = cs::effectiveFreq (graph.band (bb), bb);
                return f >= 1000.0f ? String (f / 1000.0f, f >= 10000.0f ? 1 : 2) + " kHz" : String ((int) std::lround (f)) + " Hz"; };
            c.gain.s.textFromValueFunction = [this, bb] (double) {
                const float g = cs::effectiveGain (graph.band (bb), bb); return (g > 0 ? "+" : "") + String (g, 1) + " dB"; };
            c.q.s.textFromValueFunction = [this, bb] (double v) {
                const auto s = graph.band (bb);
                if (cs::bandIsShelf (s, bb)) return String (v, 2) + "x";
                return "Q " + String (cs::nativeBellQ (s.model, bb, cs::effectiveGain (s, bb)) * (float) v, 2); };
            c.aux.s.textFromValueFunction = [] (double v) { return String (v, 1) + " dB"; };
        }
    }
    void layout (Rectangle<int> r) override
    {
        auto top = r.removeFromTop (20);
        drive.s.setBounds (top.removeFromRight (22)); drive.cap.setBounds (0, 0, 0, 0);
        top.removeFromRight (4);
        setAll.setBounds (top.removeFromRight (190));
        r.removeFromTop (4);
        graph.setBounds (r.removeFromTop (150)); r.removeFromTop (6);
        const int w = r.getWidth() / 4;
        for (int b = 0; b < 4; ++b)
        {
            auto c = r.removeFromLeft (w).reduced (3, 0);
            auto& k = col[b];
            k.on.b.setBounds (c.removeFromTop (20)); c.removeFromTop (3);
            k.model.c.setBounds (c.removeFromTop (20)); c.removeFromTop (2);
            auto kr = c.removeFromTop (74); k.freq.setBounds (kr.removeFromLeft (kr.getWidth() / 2)); k.gain.setBounds (kr);
            auto kr2 = c.removeFromTop (74); k.q.setBounds (kr2.removeFromLeft (kr2.getWidth() / 2)); k.aux.setBounds (kr2);
            k.auxSel.c.setBounds (kr2.getX(), kr2.getBottom() - 2, kr2.getWidth(), 18);
            c.removeFromTop (18);
            auto br = c.removeFromTop (20); k.shape.b.setBounds (br.removeFromLeft (br.getWidth() / 2 - 2)); k.step.b.setBounds (br.withTrimmedLeft (4));
        }
    }
    void paintBody (Graphics& g) override
    {
        g.setColour (csui::engraveDim); g.setFont (csui::font (10.0f, true));
        g.drawText ("DRIVE", drive.s.getX() - 44, drive.s.getY(), 40, 20, Justification::centredRight, false);
        for (int b = 0; b < 4; ++b)
        {
            g.setColour (csui::band[b]); auto r = col[b].on.b.getBounds().toFloat();
            g.fillRect (r.getX(), r.getBottom() + 0.5f, r.getWidth(), 2.0f);
        }
    }
    void refresh() override
    {
        for (int b = 0; b < 4; ++b)
        {
            auto& k = col[b]; const auto s = graph.band (b);
            const bool aux = cs::eqHasAux (s.model, b);
            k.aux.setVisible (aux); k.auxSel.c.setVisible (aux && b == 3);
            k.aux.cap.setText (b == 0 ? "ATTEN" : "ATTEN", dontSendNotification);
            const bool shelfSel = (b == 0 || b == 3) && (s.model == cs::EqClean || s.model == cs::EqSslPA || s.model == cs::EqSslUAD || s.model == cs::EqApi);
            k.shape.b.setVisible (shelfSel);
            k.step.b.setEnabled (cs::eqHasSteps (s.model));
            k.gain.cap.setText (s.model == cs::EqPultec && (b == 0 || b == 3) ? "BOOST" : "GAIN", dontSendNotification);
            k.q.cap.setText (s.model == cs::EqPultec && b == 3 ? "BANDWIDTH" : "Q", dontSendNotification);
            k.freq.s.updateText(); k.gain.s.updateText(); k.q.s.updateText();
        }
    }
    struct Col { Toggle on, shape, step; Choice model, auxSel; Knob freq, gain, q, aux; };
    EqGraph graph; ComboBox setAll; Knob drive; Col col[4];
};

// ============================================================================ VU meter
class VuMeter : public Component
{
public:
    std::function<float()> get; float needle = 0;
    void tick()
    {
        const float v = get ? get() : 0.0f;
        const float db = 20.0f * std::log10 (v + 1e-9f) + 18.0f;   // 0 VU = -18 dBFS RMS
        const float target = jlimit (-22.0f, 4.0f, db);
        needle += (target - needle) * 0.18f;                         // ~300 ms ballistics at 30 Hz
        repaint();
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r, 4.0f);
        auto face = r.reduced (4.0f);
        g.setGradientFill (ColourGradient (Colour (0xfff6e3b0), face.getCentreX(), face.getY(), Colour (0xffe0b866), face.getCentreX(), face.getBottom(), false));
        g.fillRoundedRectangle (face, 3.0f);
        const Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.35f);
        const float len = face.getHeight() * 1.12f;
        auto angOf = [] (float db) { const float t = (db + 20.0f) / 23.0f; return -0.78f + 1.56f * t; };
        g.setFont (csui::font (9.0f, true));
        for (float db : { -20.f, -10.f, -7.f, -5.f, -3.f, -1.f, 0.f, 1.f, 2.f, 3.f })
        {
            const float a = angOf (db);
            const auto p1 = pivot.getPointOnCircumference (len * 0.86f, a), p2 = pivot.getPointOnCircumference (len * 0.93f, a);
            g.setColour (db > 0 ? Colour (0xffc0392b) : Colour (0xff2b2116)); g.drawLine (Line<float> (p1, p2), 1.2f);
            const auto pt = pivot.getPointOnCircumference (len * 0.99f, a);
            g.drawText (db > 0 ? "+" + String ((int) db) : String ((int) db), (int) pt.x - 12, (int) pt.y - 6, 24, 10, Justification::centred);
        }
        g.setColour (Colour (0xff2b2116)); g.setFont (csui::font (12.0f, true));
        g.drawText ("VU", face.withTrimmedTop (face.getHeight() * 0.55f).toNearestInt(), Justification::centredTop, false);
        const float a = angOf (needle);
        g.setColour (Colours::black.withAlpha (0.85f));
        g.drawLine (Line<float> (pivot.getPointOnCircumference (len * 0.42f, a), pivot.getPointOnCircumference (len * 0.95f, a)), 1.6f);
    }
};

// ============================================================================ editor
class ChannelStripEditor : public AudioProcessorEditor, private Timer
{
public:
    static constexpr int kW = 1700, kH = 640;
    ChannelStripEditor (ChannelStripProcessor& p)
        : AudioProcessorEditor (p), proc (p), pre (p), flt (p), gate (p), comp (p), eq (p), lim (p)
    {
        setLookAndFeel (&look);
        addAndMakeVisible (content);
        auto& ap = p.apvts;
        panels = { &pre, &flt, &gate, &comp, &eq, &lim };
        for (auto* m : panels)
        {
            content.addAndMakeVisible (m);
            m->onDrag = [this] (ModulePanel& mp, const MouseEvent& e, int ph) { dragModule (mp, e, ph); };
        }
        inGain.init (content, ap, "inGain", "INPUT", 0.0); outGain.init (content, ap, "outGain", "OUTPUT", 0.0);
        rotDeg.init (content, ap, "rotDeg", "PHASE", 0.0);
        rotDeg.s.textFromValueFunction = [] (double v) { return String ((int) std::lround (v)) + String (CharPointer_UTF8 ("\xc2\xb0")); };
        polL.init (content, ap, "polL", String (CharPointer_UTF8 ("\xc3\x98 L"))); polR.init (content, ap, "polR", String (CharPointer_UTF8 ("\xc3\x98 R")));
        rotOn.init (content, ap, "rotOn", "ROTATE");
        rotOn.b.setTooltip ("Broadband phase rotation: shifts the phase of every frequency by the same angle");
        inMode.init (content, ap, "inMode"); osSel.init (content, ap, "os");
        inBar.get = [&p] { return p.inMeter.load(); }; inBar.reduction = false; inBar.text = "IN"; content.addAndMakeVisible (inBar);
        outBar.get = [&p] { return p.outMeter.load(); }; outBar.reduction = false; outBar.text = "OUT"; content.addAndMakeVisible (outBar);
        vu.get = [&p] { return p.vuMeter.load(); }; content.addAndMakeVisible (vu);
        tooltip.setMillisecondsBeforeTipAppears (600);

        setResizable (true, true);
        setResizeLimits (kW / 2, kH / 2, kW * 2, kH * 2);
        getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
        setSize (1360, (int) std::lround (1360.0 * kH / kW));
        content.ed = this;
        startTimerHz (30);
    }
    ~ChannelStripEditor() override { setLookAndFeel (nullptr); }

    void paint (Graphics& g) override { g.fillAll (Colour (0xff101012)); }

    void resized() override
    {
        content.setBounds (0, 0, kW, kH);
        content.setTransform (AffineTransform::scale ((float) getWidth() / (float) kW));
        layoutContent();
    }

    void layoutContent()
    {
        const int top = 56, h = kH - top - 18;
        // input column
        auto in = Rectangle<int> (34, top, 118, h);
        inGain.setBounds (in.removeFromTop (110).withSizeKeepingCentre (90, 110)); in.removeFromTop (6);
        inMode.c.setBounds (in.removeFromTop (22)); in.removeFromTop (8);
        auto pr = in.removeFromTop (24); polL.b.setBounds (pr.removeFromLeft (57)); polR.b.setBounds (pr.withTrimmedLeft (4)); in.removeFromTop (12);
        rotOn.b.setBounds (in.removeFromTop (22)); in.removeFromTop (2);
        rotDeg.setBounds (in.removeFromTop (96).withSizeKeepingCentre (84, 96)); in.removeFromTop (10);
        inBar.setBounds (in.removeFromTop (14));
        // modules in signal order
        const auto order = cs::orderFromIndex ((int) proc.apvts.getRawParameterValue ("order")->load());
        int x = 160;
        for (int k = 0; k < cs::kNumModules; ++k)
        {
            auto* m = panels[(size_t) order[(size_t) k]];
            const int w = (int) m->width;
            if (! m->dragging) m->setBounds (x, top, w, h);
            slotX[(size_t) k] = x; x += w + 4;
        }
        // output column
        auto out = Rectangle<int> (x + 4, top, kW - x - 38, h);
        vu.setBounds (out.removeFromTop (100)); out.removeFromTop (10);
        outGain.setBounds (out.removeFromTop (110).withSizeKeepingCentre (90, 110)); out.removeFromTop (8);
        outBar.setBounds (out.removeFromTop (14)); out.removeFromTop (20);
        osLabelY = out.getY(); osSel.c.setBounds (out.removeFromTop (22).withTrimmedTop (0).translated (0, 16));
        content.repaint();
    }

    void dragModule (ModulePanel& m, const MouseEvent& e, int phase)
    {
        if (phase == 0) { dragStartX = m.getX(); m.toFront (false); return; }
        const auto ev = e.getEventRelativeTo (&content);
        if (phase == 1) { m.setTopLeftPosition (jlimit (150, kW - (int) m.width - 140, dragStartX + ev.getDistanceFromDragStartX()), m.getY()); return; }
        // drop: new position by the panel's centre against the other panels' centres
        auto order = cs::orderFromIndex ((int) proc.apvts.getRawParameterValue ("order")->load());
        std::vector<int> rest; for (int k = 0; k < cs::kNumModules; ++k) if (order[(size_t) k] != m.module) rest.push_back (order[(size_t) k]);
        const float cx = (float) m.getBounds().getCentreX();
        int pos = 0;
        for (int k = 0; k < (int) rest.size(); ++k) if ((float) panels[(size_t) rest[(size_t) k]]->getBounds().getCentreX() < cx) pos = k + 1;
        rest.insert (rest.begin() + pos, m.module);
        std::array<int, cs::kNumModules> no {}; for (int k = 0; k < cs::kNumModules; ++k) no[(size_t) k] = rest[(size_t) k];
        if (auto* prm = proc.apvts.getParameter ("order"))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) cs::indexFromOrder (no)));
            prm->endChangeGesture();
        }
        layoutContent();
    }

private:
    class Content : public Component
    {
    public:
        ChannelStripEditor* ed = nullptr;
        void paint (Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            // faceplate
            g.setGradientFill (ColourGradient (Colour (0xff2c2d31), 0, 0, Colour (0xff1c1d20), 0, r.getHeight(), false)); g.fillRect (r);
            Random rnd (7);
            g.setColour (Colours::white.withAlpha (0.018f));
            for (int i = 0; i < 260; ++i) { const float y = rnd.nextFloat() * r.getHeight(); g.drawHorizontalLine ((int) y, 0, r.getWidth()); }
            // rack ears + screws
            for (float ex : { 0.0f, r.getWidth() - 26.0f })
            {
                g.setColour (Colour (0xff17181a)); g.fillRect (ex, 0.0f, 26.0f, r.getHeight());
                for (float sy : { 30.0f, r.getHeight() - 30.0f })
                {
                    g.setGradientFill (ColourGradient (Colour (0xffc9c6bd), ex + 13, sy - 7, Colour (0xff5d5b55), ex + 13, sy + 7, false));
                    g.fillEllipse (ex + 6, sy - 7, 14, 14);
                    g.setColour (Colour (0xff2a2a2a)); g.drawLine (ex + 8, sy, ex + 18, sy, 2.0f);
                }
            }
            // title strip
            g.setColour (csui::engrave); g.setFont (csui::font (22.0f, true));
            g.drawText ("CHANNEL STRIP", 40, 12, 400, 30, Justification::centredLeft, false);
            g.setColour (csui::engraveDim); g.setFont (csui::font (12.0f));
            g.drawText ("TUBE  -  IRON  -  VCA  -  OPTO  -  FET", 300, 16, 400, 24, Justification::centredLeft, false);
            g.setFont (csui::font (11.0f, true));
            g.drawText ("drag a module by its title to change the order", (int) r.getWidth() - 470, 18, 300, 20, Justification::centredRight, false);
            if (ed != nullptr)
                g.drawText ("OVERSAMPLING", ed->osSel.c.getX(), ed->osSel.c.getY() - 16, ed->osSel.c.getWidth(), 14, Justification::centredLeft, false);
            // signal-flow arrows between slots
            if (ed != nullptr)
            {
                g.setColour (csui::amber.withAlpha (0.55f));
                for (int k = 1; k < cs::kNumModules; ++k)
                {
                    const float x = (float) ed->slotX[(size_t) k] - 2.0f, y = 70.0f;
                    Path a; a.addTriangle (x - 3, y - 4, x - 3, y + 4, x + 2, y); g.fillPath (a);
                }
            }
        }
    };

    void timerCallback() override
    {
        for (auto* m : panels) m->refresh();
        inBar.repaint(); outBar.repaint(); gate.gr.repaint(); comp.gr.repaint(); lim.gr.repaint(); vu.tick();
        const int ord = (int) proc.apvts.getRawParameterValue ("order")->load();
        bool anyDrag = false; for (auto* m : panels) anyDrag |= m->dragging;
        if (ord != lastOrder && ! anyDrag) { lastOrder = ord; layoutContent(); }
        rotDeg.s.setEnabled (rotOn.b.getToggleState());
    }

    ChannelStripProcessor& proc;
    CSLook look; TooltipWindow tooltip { this };
    Content content;
    PrePanel pre; FilterPanel flt; GatePanel gate; CompPanel comp; EqPanel eq; LimPanel lim;
    std::array<ModulePanel*, cs::kNumModules> panels {};
    std::array<int, cs::kNumModules> slotX {};
    Knob inGain, outGain, rotDeg; Toggle polL, polR, rotOn; Choice inMode, osSel; Bar inBar, outBar; VuMeter vu;
    int dragStartX = 0, lastOrder = -1, osLabelY = 0;
    friend class Content;
};
