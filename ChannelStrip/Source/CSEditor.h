// ChannelStrip 2 editor (included by CSPlugin.cpp). Layout: top bar, input rail, large EQ with its four
// bands, signal-chain bar (drag to reorder), module row (PRE, DS2, COMP, GATE, INSERT, LIMIT in chain
// order), output rail with VU. Four colour themes, 3D or flat 2D, window size 50-200 %.

// ============================================================================ themes
struct Theme
{
    String name; Colour bg, bar, panel, inset, field, text, dim, accent, line, screen, grid, grid0, gridText, curve, capHi, capLo, ptr, track, vuFace, vuInk, segOn, segOnText, segOff;
    bool flat = false;
};
static Theme makeTheme (const String& name, bool flat)
{
    Theme t; t.name = name; t.flat = flat;
    auto C = [] (uint32 c) { return Colour (c); };
    if (name == "Cream")
    { t.bg = C (0xffcfc7b5); t.bar = C (0xffe4ddcd); t.panel = C (0xffebe5d7); t.inset = C (0xffe1d9c8); t.field = C (0xfff6f2e8); t.text = C (0xff26231d); t.dim = C (0xff625c50);
      t.accent = C (0xffa8501f); t.line = C (0xffb8af9c); t.screen = C (0xff1b1e1c); t.curve = C (0xfff0b453); t.capHi = C (0xff5a5651); t.capLo = C (0xff1a1918); t.ptr = C (0xfff8f4ea);
      t.track = C (0xffcdc4b1); t.vuFace = C (0xfff6e4b2); t.vuInk = C (0xff2b2116); t.segOn = C (0xffa8501f); t.segOnText = C (0xfffff6ec); t.segOff = C (0xfff6f2e8); t.gridText = C (0xff9a9486); }
    else if (name == "Midnight")
    { t.bg = C (0xff0f1420); t.bar = C (0xff18202f); t.panel = C (0xff1b2435); t.inset = C (0xff161e2c); t.field = C (0xff121926); t.text = C (0xffe2e9f5); t.dim = C (0xff93a0b6);
      t.accent = C (0xff5ec2ff); t.line = C (0xff33405a); t.screen = C (0xff080c14); t.curve = C (0xff7fd4ff); t.capHi = C (0xff3c4a63); t.capLo = C (0xff0b0f17); t.ptr = C (0xffe9f4ff);
      t.track = C (0xff2b3750); t.vuFace = C (0xffdfeefa); t.vuInk = C (0xff14202e); t.segOn = C (0xff1f5a80); t.segOnText = C (0xffdff3ff); t.segOff = C (0xff121926); t.gridText = C (0xff7f8ba1); }
    else if (name == "Forest")
    { t.bg = C (0xff131a16); t.bar = C (0xff1d2620); t.panel = C (0xff212c25); t.inset = C (0xff1b241f); t.field = C (0xff141b17); t.text = C (0xffe6ece2); t.dim = C (0xff9aa797);
      t.accent = C (0xff8fd16a); t.line = C (0xff3a4a3f); t.screen = C (0xff0a0f0c); t.curve = C (0xffa6e07f); t.capHi = C (0xff425246); t.capLo = C (0xff0d120f); t.ptr = C (0xffeef7e8);
      t.track = C (0xff33433a); t.vuFace = C (0xffeef0d0); t.vuInk = C (0xff1f2618); t.segOn = C (0xff3f6a2c); t.segOnText = C (0xffeaffd9); t.segOff = C (0xff141b17); t.gridText = C (0xff859282); }
    else
    { t.name = "Graphite"; t.bg = C (0xff16181b); t.bar = C (0xff24272b); t.panel = C (0xff2a2d32); t.inset = C (0xff222529); t.field = C (0xff1a1c1f); t.text = C (0xffece6d8); t.dim = C (0xffa39d90);
      t.accent = C (0xfff2a93b); t.line = C (0xff474a50); t.screen = C (0xff0d1012); t.curve = C (0xfff6c35a); t.capHi = C (0xff4a4d53); t.capLo = C (0xff121315); t.ptr = C (0xfff4efe2);
      t.track = C (0xff3a3d42); t.vuFace = C (0xfff3e1ad); t.vuInk = C (0xff2b2116); t.segOn = C (0xff6a5228); t.segOnText = C (0xffffe3b0); t.segOff = C (0xff1e2023); t.gridText = C (0xff8d887d); }
    t.grid = Colours::white.withAlpha (0.07f); t.grid0 = Colours::white.withAlpha (0.2f);
    return t;
}
static const Colour kBandCol[4] { Colour (0xffff8a5b), Colour (0xffffd166), Colour (0xff7bd389), Colour (0xff7cc4ff) };
static Font uiFont (float h, bool bold = false) { return Font (FontOptions (h, bold ? Font::bold : Font::plain)); }
static Font monoFont (float h) { return Font (FontOptions (Font::getDefaultMonospacedFontName(), h, Font::bold)); }

class CSLook : public LookAndFeel_V4
{
public:
    Theme th = makeTheme ("Graphite", false);
    void apply (const Theme& t)
    {
        th = t;
        setColour (ComboBox::backgroundColourId, t.field); setColour (ComboBox::textColourId, t.text);
        setColour (ComboBox::outlineColourId, t.line); setColour (ComboBox::arrowColourId, t.text);
        setColour (PopupMenu::backgroundColourId, t.field); setColour (PopupMenu::textColourId, t.text);
        setColour (PopupMenu::highlightedBackgroundColourId, t.segOn); setColour (PopupMenu::highlightedTextColourId, t.segOnText);
        setColour (Slider::textBoxTextColourId, t.text); setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack); setColour (Slider::textBoxHighlightColourId, t.segOn);
        setColour (TextEditor::textColourId, t.text); setColour (TextEditor::backgroundColourId, t.field);
        setColour (Label::textColourId, t.text);
        setColour (TooltipWindow::backgroundColourId, t.field); setColour (TooltipWindow::textColourId, t.text); setColour (TooltipWindow::outlineColourId, t.line);
    }
    Font getComboBoxFont (ComboBox&) override { return uiFont (13.0f, true); }
    Font getPopupMenuFont() override { return uiFont (14.5f); }
    Font getLabelFont (Label& l) override { return l.getFont(); }
    Font getSliderPopupFont (Slider&) override { return monoFont (13.0f); }

    void drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s) override
    {
        auto b = Rectangle<int> (x, y, w, h).toFloat();
        const float S = jmin (b.getWidth(), b.getHeight());
        const auto c = b.getCentre(); const float r = S * 0.5f - 1.0f;
        const float ang = a0 + pos * (a1 - a0);
        const bool bip = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float from = bip ? a0 + (a1 - a0) * (float) s.valueToProportionOfLength (0.0) : a0;
        Path track; track.addCentredArc (c.x, c.y, r - 2.5f, r - 2.5f, 0.0f, a0, a1, true);
        g.setColour (th.track); g.strokePath (track, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::butt));
        Path arc; arc.addCentredArc (c.x, c.y, r - 2.5f, r - 2.5f, 0.0f, jmin (from, ang), jmax (from, ang), true);
        g.setColour (s.isEnabled() ? th.accent : th.dim); g.strokePath (arc, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::butt));
        const float cr = r - 8.0f;
        if (th.flat)
        {
            g.setColour (th.capLo); g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);
            g.setColour (th.capHi); g.drawEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2, 1.5f);
        }
        else
        {
            g.setColour (Colours::black.withAlpha (0.5f)); g.fillEllipse (c.x - cr, c.y - cr + 3.0f, cr * 2, cr * 2);
            g.setGradientFill (ColourGradient (th.capHi, c.x - cr * 0.3f, c.y - cr * 0.4f, th.capLo, c.x + cr * 0.5f, c.y + cr, true));
            g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);
            g.setColour (Colours::black.withAlpha (0.6f)); g.drawEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2, 1.0f);
            g.setColour (Colours::white.withAlpha (0.07f)); g.drawEllipse (c.x - cr + 1.5f, c.y - cr + 1.5f, cr * 2 - 3.0f, cr * 2 - 3.0f, 1.0f);
        }
        g.setColour (s.isEnabled() ? th.ptr : th.dim);
        g.drawLine (Line<float> (c.getPointOnCircumference (cr * 0.18f, ang), c.getPointOnCircumference (cr - 3.0f, ang)), 3.0f);
    }
    void drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        g.setColour (on ? th.segOn : th.segOff); g.fillRoundedRectangle (r, 4.0f);
        if (! th.flat)
        {
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (on ? 0.0f : 0.06f), 0, r.getY(), Colours::black.withAlpha (on ? 0.25f : 0.12f), 0, r.getBottom(), false));
            g.fillRoundedRectangle (r, 4.0f);
        }
        g.setColour (on ? th.accent : th.line); g.drawRoundedRectangle (r, 4.0f, 1.0f);
        if (over || down) { g.setColour ((th.flat ? th.text : Colours::white).withAlpha (down ? 0.12f : 0.06f)); g.fillRoundedRectangle (r, 4.0f); }
    }
    void drawButtonText (Graphics& g, TextButton& b, bool, bool) override
    {
        g.setColour (b.getToggleState() ? th.segOnText : th.text);
        g.setFont (uiFont (12.0f, true));
        g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred, false);
    }
    void drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox&) override
    {
        auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (th.field); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (th.line); g.drawRoundedRectangle (r, 5.0f, 1.0f);
        Path p; const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
        p.startNewSubPath (ax - 4, ay - 2); p.lineTo (ax, ay + 2); p.lineTo (ax + 4, ay - 2);
        g.setColour (th.text); g.strokePath (p, PathStrokeType (1.6f));
    }
};

// ============================================================================ controls
class FineKnob : public Slider
{
public:
    FineKnob() : Slider (Slider::RotaryHorizontalVerticalDrag, Slider::TextBoxBelow)
    {
        setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        setMouseDragSensitivity (260);                         // smooth, unhurried response
        setVelocityBasedMode (false);
    }
    void mouseDown (const MouseEvent& e) override { Slider::mouseDown (e); last = e.position; prop = valueToProportionOfLength (getValue()); }
    void mouseDrag (const MouseEvent& e) override
    {
        if (e.mods.isAltDown())                                // Option / Alt: ten times finer
        {
            const auto d = e.position - last; last = e.position;
            prop = jlimit (0.0, 1.0, prop + (double) (d.x - d.y) / 2600.0);
            setValue (proportionOfLengthToValue (prop), sendNotificationSync); return;
        }
        Slider::mouseDrag (e); last = e.position; prop = valueToProportionOfLength (getValue());
    }
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        auto w2 = w; const float k = e.mods.isAltDown() ? 0.05f : 0.35f; w2.deltaY *= k; w2.deltaX *= k; Slider::mouseWheelMove (e, w2);
    }
private:
    Point<float> last; double prop = 0.0;
};

struct Knob
{
    FineKnob s; Label cap; std::unique_ptr<APVTS::SliderAttachment> att; int size = 64; bool secondary = false;
    void init (Component& parent, APVTS& ap, const String& id, const String& caption, int sz, bool second = false)
    {
        size = sz; secondary = second;
        att = std::make_unique<APVTS::SliderAttachment> (ap, id, s);
        if (auto* p = ap.getParameter (id)) s.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        s.setTextBoxStyle (Slider::TextBoxBelow, false, jmax (70, sz + 12), 16);
        cap.setText (caption, dontSendNotification); cap.setJustificationType (Justification::centred);
        cap.setFont (uiFont (11.0f, true)); cap.setInterceptsMouseClicks (false, false);
        parent.addAndMakeVisible (s); parent.addAndMakeVisible (cap);
    }
    int height() const { return 14 + size + 18; }
    int width() const { return jmax (size + 12, 70); }
    void place (int cx, int y)     // centred at cx, top at y
    {
        cap.setBounds (cx - 45, y, 90, 14);
        s.setBounds (cx - width() / 2, y + 14, width(), size + 18);
    }
    void setVisible (bool v) { s.setVisible (v); cap.setVisible (v); }
    void setCaption (const String& c) { cap.setText (c, dontSendNotification); }
};

struct Toggle
{
    TextButton b; std::unique_ptr<APVTS::ButtonAttachment> att;
    void init (Component& parent, APVTS& ap, const String& id, const String& text)
    {
        b.setButtonText (text); b.setClickingTogglesState (true);
        att = std::make_unique<APVTS::ButtonAttachment> (ap, id, b); parent.addAndMakeVisible (b);
    }
};

// one-click segment buttons for a choice parameter (smoother than a dropdown)
class Segments : public Component, public SettableTooltipClient
{
public:
    std::function<void (int)> onPick;
    Segments() = default;
    void init (APVTS& ap, const String& id, const StringArray& labels = {})
    {
        param = dynamic_cast<AudioParameterChoice*> (ap.getParameter (id));
        const auto items = labels.isEmpty() && param != nullptr ? param->choices : labels;
        build (items);
    }
    void build (const StringArray& items)
    {
        buttons.clear();
        for (int i = 0; i < items.size(); ++i)
        {
            auto* b = buttons.add (new TextButton (items[i]));
            b->onClick = [this, i] { pick (i); };
            addAndMakeVisible (b);
        }
    }
    void pick (int i)
    {
        if (param != nullptr) { param->beginChangeGesture(); param->setValueNotifyingHost (param->convertTo0to1 ((float) i)); param->endChangeGesture(); }
        if (onPick) onPick (i);
        refresh();
    }
    void setSelected (int i) { sel = i; for (int k = 0; k < buttons.size(); ++k) buttons[k]->setToggleState (k == i, dontSendNotification); }
    void refresh() { if (param != nullptr) setSelected (param->getIndex()); }
    void resized() override
    {
        auto r = getLocalBounds(); const int n = buttons.size(); if (n == 0) return;
        const int gap = 3, w = (r.getWidth() - gap * (n - 1)) / n;
        for (int k = 0; k < n; ++k) buttons[k]->setBounds (r.getX() + k * (w + gap), r.getY(), k == n - 1 ? r.getRight() - (r.getX() + k * (w + gap)) : w, r.getHeight());
    }
    AudioParameterChoice* param = nullptr; OwnedArray<TextButton> buttons; int sel = -1;
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

class Lamp : public Button
{
public:
    CSLook* look = nullptr;
    Lamp() : Button ("power") { setClickingTogglesState (true); setTooltip ("Module on / off"); }
    void paintButton (Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f); const float d = jmin (r.getWidth(), r.getHeight());
        auto c = r.withSizeKeepingCentre (d, d); const bool on = getToggleState(); const auto& t = look->th;
        if (on && ! t.flat) { g.setColour (t.accent.withAlpha (0.35f)); g.fillEllipse (c.expanded (3.0f)); }
        if (on) { if (t.flat) g.setColour (t.accent); else g.setGradientFill (ColourGradient (Colours::white, c.getX() + d * 0.35f, c.getY() + d * 0.3f, t.accent, c.getRight(), c.getBottom(), true)); }
        else g.setColour (t.segOff);
        g.fillEllipse (c);
        g.setColour (Colours::black.withAlpha (0.6f)); g.drawEllipse (c, 1.0f);
        if (over) { g.setColour (Colours::white.withAlpha (0.12f)); g.fillEllipse (c); }
    }
};

class GrBar : public Component
{
public:
    std::function<float()> get; float range = 24.0f; CSLook* look = nullptr;
    void paint (Graphics& g) override
    {
        const auto& t = look->th; auto r = getLocalBounds().toFloat();
        g.setColour (t.dim); g.setFont (uiFont (10.5f, true)); g.drawText ("GR", r.removeFromLeft (22), Justification::centredLeft);
        const float v = get ? get() : 0.0f;
        auto txt = r.removeFromRight (56);
        auto bar = r.reduced (0, 3);
        g.setColour (t.screen); g.fillRoundedRectangle (bar, 2.0f);
        const float f = jlimit (0.0f, 1.0f, v / range);
        g.setColour (t.accent); g.fillRect (bar.withLeft (bar.getRight() - bar.getWidth() * f));
        g.setColour (t.text); g.setFont (monoFont (11.0f)); g.drawText ((v > 0.05f ? "-" : "") + String (v, 1) + " dB", txt, Justification::centredRight);
    }
};

class LevelMeter : public Component
{
public:
    std::function<float()> get; CSLook* look = nullptr; float shown = 0;
    void paint (Graphics& g) override
    {
        const auto& t = look->th; auto r = getLocalBounds().toFloat();
        g.setColour (t.screen); g.fillRoundedRectangle (r, 2.0f);
        const float v = get ? get() : 0.0f, db = 20.0f * std::log10 (v + 1e-9f);
        const float f = jlimit (0.0f, 1.0f, (db + 60.0f) / 63.0f);
        shown = jmax (f, shown * 0.92f);
        auto fill = r.reduced (1.5f); fill = fill.withTop (fill.getBottom() - fill.getHeight() * shown);
        g.setGradientFill (ColourGradient (Colour (0xff4caf6a), 0, r.getBottom(), Colour (0xffe5533d), 0, r.getY(), false));
        g.fillRect (fill);
    }
};

// ============================================================================ module panels
class Panel : public Component
{
public:
    Panel (ChannelStripProcessor& p, CSLook& l, int mod, const String& title, const String& onId, const String& routeId = {})
        : proc (p), look (l), module (mod), name (title)
    {
        lamp.look = &look;
        if (onId.isNotEmpty()) { lampAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, onId, lamp); addAndMakeVisible (lamp); }
        if (routeId.isNotEmpty())
        {
            route.init (p.apvts, routeId); addAndMakeVisible (route);
            route.setTooltip ("ST = linked stereo, DUO = left and right separately, MS = mid / side");
        }
    }
    static constexpr int kHeader = 28;
    void paint (Graphics& g) override
    {
        const auto& t = look.th; auto r = getLocalBounds().toFloat();
        if (t.flat) { g.setColour (t.panel); g.fillRoundedRectangle (r, 8.0f); }
        else
        {
            g.setColour (Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (r.translated (0, 3), 8.0f);
            g.setGradientFill (ColourGradient (t.panel, 0, 0, t.inset, 0, r.getHeight(), false)); g.fillRoundedRectangle (r, 8.0f);
            g.setColour (Colours::white.withAlpha (0.05f)); g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        }
        g.setColour (lamp.getToggleState() || ! lamp.isVisible() ? t.text : t.dim);
        g.setFont (uiFont (14.0f, true));
        g.drawText (name, 34, 6, 200, 20, Justification::centredLeft, false);
        paintBody (g);
    }
    virtual void paintBody (Graphics&) {}
    virtual void layout (Rectangle<int>) = 0;
    virtual void refresh (bool /*focus*/) {}
    void resized() override
    {
        lamp.setBounds (8, 6, 20, 20);
        route.setBounds (getWidth() - 56, 6, 48, 20);
        layout (getLocalBounds().withTrimmedTop (kHeader + 4).reduced (10, 6));
    }
    // knob grid helper: n columns centred in r
    static void grid (std::vector<Knob*> ks, Rectangle<int> r, int cols)
    {
        std::vector<Knob*> vis; for (auto* k : ks) if (k->s.isVisible()) vis.push_back (k);
        const int cw = r.getWidth() / jmax (1, cols); int y = r.getY(), rowH = 0;
        for (size_t i = 0; i < vis.size(); ++i)
        {
            const int c = (int) i % cols; if (c == 0 && i > 0) { y += rowH + 6; rowH = 0; }
            vis[i]->place (r.getX() + cw * c + cw / 2, y); rowH = jmax (rowH, vis[i]->height());
        }
    }
    ChannelStripProcessor& proc; CSLook& look; const int module; String name;
    Lamp lamp; std::unique_ptr<APVTS::ButtonAttachment> lampAtt; Segments route;
    float width = 200;
};

class PrePanel : public Panel
{
public:
    PrePanel (ChannelStripProcessor& p, CSLook& l) : Panel (p, l, cs::ModPre, "PRE", "preOn", "preRoute")
    {
        width = 268; auto& ap = p.apvts;
        model.init (*this, ap, "preModel");
        colour.init (ap, "preColor"); addAndMakeVisible (colour);
        thump.init (*this, ap, "preThump", "THUMP");
        hpSlope.init (ap, "hpSlope", { "6", "12", "18", "24" }); addAndMakeVisible (hpSlope);
        hpOn.init (*this, ap, "hpOn", "LOW CUT"); lpOn.init (*this, ap, "lpOn", "HIGH CUT");
        drive.init (*this, ap, "preDrive", "DRIVE", 64); out.init (*this, ap, "preOut", "OUTPUT", 64, true);
        hpF.init (*this, ap, "hpFreq", "LOW CUT", 64, true); lpF.init (*this, ap, "lpFreq", "HIGH CUT", 64, true);
        hpR.init (*this, ap, "hpRes", "RESO", 64, true);
        lpSlopeHidden.init (ap, "lpSlope", { "6", "12", "18", "24" }); addAndMakeVisible (lpSlopeHidden);
        thump.b.setTooltip ("Gentle low-end resonance just above the low cut");
        auto hz = [] (double v) { return v >= 1000.0 ? String (v / 1000.0, 1) + " kHz" : String ((int) std::lround (v)) + " Hz"; };
        hpF.s.textFromValueFunction = hz; lpF.s.textFromValueFunction = hz;
    }
    void layout (Rectangle<int> r) override
    {
        model.c.setBounds (r.removeFromTop (26)); r.removeFromTop (6);
        colour.setBounds (r.removeFromTop (24)); r.removeFromTop (5);
        auto row = r.removeFromTop (24);
        thump.b.setBounds (row.removeFromLeft (62)); row.removeFromLeft (4);
        hpSlope.setBounds (row); r.removeFromTop (5);
        auto row2 = r.removeFromTop (22);
        hpOn.b.setBounds (row2.removeFromLeft (row2.getWidth() / 2 - 2)); lpOn.b.setBounds (row2.withTrimmedLeft (4));
        lpSlopeHidden.setBounds (0, 0, 0, 0);
        r.removeFromTop (6);
        grid ({ &drive, &out, &hpR, &hpF, &lpF }, r, 3);
    }
    void refresh (bool focus) override
    {
        colour.refresh(); hpSlope.refresh(); route.refresh();
        for (auto* k : { &out, &hpF, &lpF, &hpR }) k->setVisible (! focus);
        hpSlope.setVisible (! focus); hpOn.b.setVisible (! focus); lpOn.b.setVisible (! focus); thump.b.setVisible (! focus);
        hpF.s.updateText(); lpF.s.updateText();
    }
    Choice model; Segments colour, hpSlope, lpSlopeHidden; Toggle thump, hpOn, lpOn; Knob drive, out, hpF, lpF, hpR;
};

class DsPanel : public Panel
{
public:
    DsPanel (ChannelStripProcessor& p, CSLook& l) : Panel (p, l, cs::ModDs, "DS2", "dsOn", "dsRoute")
    {
        width = 168; auto& ap = p.apvts;
        mode.init (ap, "dsMode"); addAndMakeVisible (mode);
        listen.init (*this, ap, "dsListen", "LISTEN");
        f1.init (*this, ap, "ds1Freq", "FREQ 1", 64, true); t1.init (*this, ap, "ds1Thr", "THRESH 1", 64);
        f2.init (*this, ap, "ds2Freq", "FREQ 2", 64, true); t2.init (*this, ap, "ds2Thr", "THRESH 2", 64);
        auto hz = [] (double v) { return String (v / 1000.0, 1) + " kHz"; };
        f1.s.textFromValueFunction = hz; f2.s.textFromValueFunction = hz;
        gr.look = &l; gr.get = [&p] { return p.dsGr.load(); }; addAndMakeVisible (gr);
    }
    void layout (Rectangle<int> r) override
    {
        mode.setBounds (r.removeFromTop (24)); r.removeFromTop (5);
        listen.b.setBounds (r.removeFromTop (22)); r.removeFromTop (6);
        gr.setBounds (r.removeFromBottom (16));
        grid ({ &f1, &t1, &f2, &t2 }, r, 2);
    }
    void refresh (bool focus) override { mode.refresh(); route.refresh(); f1.setVisible (! focus); f2.setVisible (! focus); listen.b.setVisible (! focus); gr.repaint(); }
    Segments mode; Toggle listen; Knob f1, t1, f2, t2; GrBar gr;
};

class CompPanel : public Panel
{
public:
    CompPanel (ChannelStripProcessor& p, CSLook& l) : Panel (p, l, cs::ModComp, "COMP", "cpOn", "cpRoute")
    {
        width = 300; auto& ap = p.apvts;
        type.init (ap, "cpType"); addAndMakeVisible (type);
        tone.init (ap, "cpTone", { "CLEAN", "VINTAGE" }); addAndMakeVisible (tone);
        key.init (ap, "cpKey", { "KEY INT", "KEY EXT" }); addAndMakeVisible (key);
        thr.init (*this, ap, "cpThr", "THRESH", 64); ratio.init (*this, ap, "cpRatio", "RATIO", 64); mk.init (*this, ap, "cpMakeup", "MAKEUP", 64);
        mix.init (*this, ap, "cpMix", "MIX", 64); atk.init (*this, ap, "cpAtk", "ATTACK", 64, true); rel.init (*this, ap, "cpRel", "RELEASE", 64, true);
        sc.init (*this, ap, "cpScHp", "SC HPF", 64, true);
        atk.s.textFromValueFunction = [] (double v) { return v < 0.001 ? String (v * 1.0e6, 0) + " us" : String (v * 1000.0, v < 0.01 ? 2 : 1) + " ms"; };
        rel.s.textFromValueFunction = [] (double v) { return v < 1.0 ? String (v * 1000.0, 0) + " ms" : String (v, 2) + " s"; };
        sc.s.textFromValueFunction = [] (double v) { return v < 1.0 ? String ("OFF") : String ((int) std::lround (v)) + " Hz"; };
        ratio.s.textFromValueFunction = [] (double v) { return String (v, 1) + ":1"; };
        sc.s.setTooltip ("Side-chain high-pass: lows below this frequency are not compressed (the audio itself is not filtered)");
        gr.look = &l; gr.get = [&p] { return p.compGr.load(); }; addAndMakeVisible (gr);
    }
    void layout (Rectangle<int> r) override
    {
        type.setBounds (r.removeFromTop (26)); r.removeFromTop (5);
        auto row = r.removeFromTop (22); tone.setBounds (row.removeFromLeft (row.getWidth() / 2 - 2)); key.setBounds (row.withTrimmedLeft (4));
        r.removeFromTop (6); gr.setBounds (r.removeFromBottom (16));
        grid ({ &thr, &ratio, &mk, &mix, &atk, &rel, &sc }, r, 4);
    }
    void refresh (bool focus) override
    {
        type.refresh(); tone.refresh(); key.refresh(); route.refresh();
        const bool opto = type.sel == pv::CompOpto;          // LA-2A style: its own attack and programme release
        atk.setVisible (! opto && ! focus); rel.setVisible (! opto && ! focus); sc.setVisible (! focus); key.setVisible (! focus);
        gr.repaint();
    }
    Segments type, tone, key; Knob thr, ratio, mk, mix, atk, rel, sc; GrBar gr;
};

class GatePanel : public Panel
{
public:
    GatePanel (ChannelStripProcessor& p, CSLook& l) : Panel (p, l, cs::ModGate, "GATE", "gtOn", "gtRoute")
    {
        width = 222; auto& ap = p.apvts;
        mode.init (ap, "gtMode"); addAndMakeVisible (mode);
        key.init (ap, "gtKey", { "INT", "EXT" }); addAndMakeVisible (key);
        listen.init (*this, ap, "gtListen", "LISTEN");
        thr.init (*this, ap, "gtThr", "THRESH", 64); range.init (*this, ap, "gtRange", "RANGE", 64); atk.init (*this, ap, "gtAtk", "ATTACK", 64, true);
        hold.init (*this, ap, "gtHold", "HOLD", 64, true); rel.init (*this, ap, "gtRel", "RELEASE", 64, true); khp.init (*this, ap, "gtKeyHp", "KEY HPF", 64, true);
        gr.look = &l; gr.get = [&p] { return p.gateGr.load(); }; gr.range = 40.0f; addAndMakeVisible (gr);
    }
    void layout (Rectangle<int> r) override
    {
        mode.setBounds (r.removeFromTop (24)); r.removeFromTop (5);
        auto row = r.removeFromTop (22); key.setBounds (row.removeFromLeft (row.getWidth() - 74)); listen.b.setBounds (row.withTrimmedLeft (4));
        r.removeFromTop (6); gr.setBounds (r.removeFromBottom (16));
        grid ({ &thr, &range, &atk, &hold, &rel, &khp }, r, 3);
    }
    void refresh (bool focus) override
    {
        mode.refresh(); key.refresh(); route.refresh();
        for (auto* k : { &atk, &hold, &rel, &khp }) k->setVisible (! focus);
        key.setVisible (! focus); listen.b.setVisible (! focus); gr.repaint();
    }
    Segments mode, key; Toggle listen; Knob thr, range, atk, hold, rel, khp; GrBar gr;
};

class HostWindow : public DocumentWindow
{
public:
    HostWindow (AudioProcessorEditor* ed, const String& title)
        : DocumentWindow (title, Colours::black, DocumentWindow::closeButton)
    { setUsingNativeTitleBar (true); setContentOwned (ed, true); setResizable (ed->isResizable(), false); centreWithSize (getWidth(), getHeight()); setVisible (true); }
    void closeButtonPressed() override { setVisible (false); }
};

class InsertPanel : public Panel, private ChangeListener
{
public:
    InsertPanel (ChannelStripProcessor& p, CSLook& l) : Panel (p, l, cs::ModInsert, "INSERT", "insOn")
    {
        width = 116;
        load.setButtonText ("+ VST3"); edit.setButtonText ("EDIT"); remove.setButtonText ("REMOVE");
        for (auto* b : { &load, &edit, &remove }) addAndMakeVisible (b);
        nameLbl.setJustificationType (Justification::centred); nameLbl.setFont (uiFont (12.0f, true)); addAndMakeVisible (nameLbl);
        load.onClick = [this] {
            chooser = std::make_unique<FileChooser> ("Choose a VST3 plugin", File ("C:\\Program Files\\Common Files\\VST3"), "*.vst3");
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::canSelectDirectories,
                [this] (const FileChooser& fc) {
                    const auto f = fc.getResult(); if (f == File()) return;
                    String err; window.reset();
                    if (! proc.loadInsert (f.getFullPathName(), err)) AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Insert", err.isEmpty() ? "Could not load the plugin." : err);
                });
        };
        edit.onClick = [this] {
            if (window != nullptr) { window->setVisible (true); window->toFront (true); return; }
            if (auto* pl = proc.insertPlugin()) if (auto* ed = pl->createEditorIfNeeded()) window = std::make_unique<HostWindow> (ed, pl->getName());
        };
        remove.onClick = [this] { window.reset(); proc.removeInsert(); };
        proc.insertChanged.addChangeListener (this);
        update();
    }
    ~InsertPanel() override { proc.insertChanged.removeChangeListener (this); window.reset(); }
    void changeListenerCallback (ChangeBroadcaster*) override { window.reset(); update(); }
    void update()
    {
        const auto n = proc.insertName();
        nameLbl.setText (n.isEmpty() ? "empty" : n, dontSendNotification);
        edit.setEnabled (n.isNotEmpty()); remove.setEnabled (n.isNotEmpty());
    }
    void paintBody (Graphics& g) override
    {
        const auto& t = look.th; g.setColour (t.line);
        const float d[] = { 5.0f, 4.0f };
        auto r = Rectangle<float> (10, (float) kHeader + 8, (float) getWidth() - 20, (float) getHeight() - kHeader - 18);
        Path p; p.addRoundedRectangle (r, 6.0f); Path dashed; PathStrokeType (1.5f).createDashedStroke (dashed, p, d, 2); g.fillPath (dashed);
        g.setColour (t.dim); g.setFont (uiFont (11.0f));
        g.drawFittedText ("any of your plugins, inside the chain", r.reduced (8).withTrimmedTop (r.getHeight() * 0.62f).toNearestInt(), Justification::centred, 3);
    }
    void layout (Rectangle<int> r) override
    {
        r.removeFromTop (10);
        load.setBounds (r.removeFromTop (30).reduced (6, 0)); r.removeFromTop (8);
        nameLbl.setBounds (r.removeFromTop (34)); r.removeFromTop (6);
        edit.setBounds (r.removeFromTop (24).reduced (6, 0)); r.removeFromTop (6);
        remove.setBounds (r.removeFromTop (24).reduced (6, 0));
    }
    void refresh (bool) override { nameLbl.setColour (Label::textColourId, look.th.text); }
    TextButton load, edit, remove; Label nameLbl; std::unique_ptr<FileChooser> chooser; std::unique_ptr<HostWindow> window;
};

class LimPanel : public Panel
{
public:
    LimPanel (ChannelStripProcessor& p, CSLook& l) : Panel (p, l, cs::ModLim, "LIMIT", "lmOn")
    {
        width = 116; auto& ap = p.apvts;
        drv.init (*this, ap, "lmDrive", "DRIVE", 64, true); ceil.init (*this, ap, "lmCeil", "CEILING", 64); rel.init (*this, ap, "lmRel", "RELEASE", 64, true);
        gr.look = &l; gr.get = [&p] { return p.limGr.load(); }; gr.range = 12.0f; addAndMakeVisible (gr);
    }
    void layout (Rectangle<int> r) override { gr.setBounds (r.removeFromBottom (16)); grid ({ &drv, &ceil, &rel }, r, 1); }
    void refresh (bool focus) override { drv.setVisible (! focus); rel.setVisible (! focus); gr.repaint(); }
    Knob drv, ceil, rel; GrBar gr;
};

// ---------------------------------------------------------------- EQ (top)
class EqGraph : public Component, private Timer
{
public:
    EqGraph (ChannelStripProcessor& p, CSLook& l) : proc (p), look (l) { startTimerHz (30); }
    static constexpr float fMin = 20.0f, fMax = 20000.0f, gMax = 20.0f;
    float xOf (float f) const { return (float) getWidth() * std::log (f / fMin) / std::log (fMax / fMin); }
    float fOf (float x) const { return fMin * std::pow (fMax / fMin, jlimit (0.0f, 1.0f, x / (float) getWidth())); }
    float yOf (float g) const { return (float) getHeight() * (0.5f - g / (2.0f * gMax) * 0.9f); }
    float gOf (float y) const { return (0.5f - y / (float) getHeight()) * 2.0f * gMax / 0.9f; }
    float pv_ (const String& id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    cs::BandSettings band (int b) const
    {
        cs::BandSettings s; s.on = pv_ (bid (b, "On")) > 0.5f; s.gain = pv_ (bid (b, "Gain")); s.freq = pv_ (bid (b, "Freq")); s.qScale = pv_ (bid (b, "Q"));
        s.shelf = pv_ (bid (b, "Shape")) > 0.5f; s.aux = pv_ (bid (b, "Aux")); s.auxSel = (int) pv_ (bid (b, "AuxSel")); return s;
    }
    int model() const { return (int) pv_ ("eqModel"); }
    void rebuild()
    {
        const int W = getWidth(); if (W <= 0) return;
        const double fsD = 96000.0; const int m = model();
        cs::CutFilter hp, lp; static const int sl[] = { 6, 12, 18, 24 };
        const bool preOn = pv_ ("preOn") > 0.5f;
        hp.set (true, preOn && pv_ ("hpOn") > 0.5f, fsD, pv_ ("hpFreq"), sl[jlimit (0, 3, (int) pv_ ("hpSlope"))], pv_ ("hpRes") * 0.01);
        lp.set (false, preOn && pv_ ("lpOn") > 0.5f, fsD, pv_ ("lpFreq"), sl[jlimit (0, 3, (int) pv_ ("lpSlope"))], pv_ ("lpRes") * 0.01);
        cs::BandSettings bs[4]; for (int b = 0; b < 4; ++b) bs[b] = band (b);
        const bool eqOn = pv_ ("eqOn") > 0.5f;
        total.clear(); for (auto& p : bandPath) p.clear();
        const int N = 600;
        for (int i = 0; i <= N; ++i)
        {
            const float x = (float) W * (float) i / (float) N; const double f = fOf (x);
            double t = hp.magDb (f, fsD) + lp.magDb (f, fsD);
            for (int b = 0; b < 4; ++b)
            {
                const double mdb = cs::EqBank::bandMagDb (m, bs[b], b, f, fsD);
                if (eqOn) t += mdb;
                const float y = yOf ((float) mdb);
                if (i == 0) bandPath[b].startNewSubPath (x, y); else bandPath[b].lineTo (x, y);
            }
            const float y = yOf ((float) jlimit (-40.0, 40.0, t));
            if (i == 0) total.startNewSubPath (x, y); else total.lineTo (x, y);
        }
        fill = total; fill.lineTo ((float) W, yOf (0)); fill.lineTo (0, yOf (0)); fill.closeSubPath();
        repaint();
    }
    void paint (Graphics& g) override
    {
        const auto& t = look.th; auto r = getLocalBounds().toFloat();
        g.setColour (t.screen); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (t.grid);
        for (float f : { 30.f, 50.f, 100.f, 200.f, 300.f, 500.f, 1000.f, 2000.f, 3000.f, 5000.f, 10000.f }) g.drawVerticalLine ((int) xOf (f), 0, r.getHeight());
        for (float db : { -12.f, -6.f, 6.f, 12.f }) g.drawHorizontalLine ((int) yOf (db), 0, r.getWidth());
        g.setColour (t.grid0); g.drawHorizontalLine ((int) yOf (0), 0, r.getWidth());
        g.setColour (t.gridText); g.setFont (uiFont (10.5f));
        for (auto [f, s] : { std::pair<float, const char*> { 50.f, "50" }, { 100.f, "100" }, { 500.f, "500" }, { 1000.f, "1k" }, { 5000.f, "5k" }, { 10000.f, "10k" } })
            g.drawText (s, (int) xOf (f) + 3, (int) r.getBottom() - 14, 34, 12, Justification::left);
        for (float db : { 12.f, 6.f, -6.f, -12.f }) g.drawText ((db > 0 ? "+" : "") + String ((int) db), 6, (int) yOf (db) - 13, 30, 12, Justification::left);
        for (int b = 0; b < 4; ++b) { g.setColour (kBandCol[b].withAlpha (0.45f)); g.strokePath (bandPath[b], PathStrokeType (1.2f)); }
        g.setColour (t.curve.withAlpha (0.12f)); g.fillPath (fill);
        g.setColour (pv_ ("eqOn") > 0.5f ? t.curve : t.dim); g.strokePath (total, PathStrokeType (2.4f, PathStrokeType::curved, PathStrokeType::rounded));
        for (int b = 0; b < 4; ++b)
        {
            const auto s = band (b); const float x = xOf (s.freq), y = yOf (jlimit (-gMax, gMax, s.gain));
            g.setColour (kBandCol[b].withAlpha (s.on ? 1.0f : 0.35f)); g.fillEllipse (x - 10, y - 10, 20, 20);
            g.setColour (Colours::black); g.setFont (uiFont (11.0f, true)); g.drawText (String (b + 1), (int) x - 10, (int) y - 10, 20, 20, Justification::centred);
        }
        g.setColour (t.gridText); g.setFont (uiFont (10.5f));
        g.drawText ("drag a dot: frequency + gain  -  wheel: Q  -  double-click: 0 dB", r.reduced (10, 6).toNearestInt(), Justification::topRight);
    }
    void resized() override { rebuild(); }
    void timerCallback() override
    {
        String h; for (auto* p : proc.getParameters()) h << String (p->getValue(), 5) << ",";
        const auto hash = h.hashCode64();
        if (hash != lastHash) { lastHash = hash; rebuild(); }
    }
    int hit (Point<float> p) const
    {
        int best = -1; float bd = 16.0f;
        for (int b = 0; b < 4; ++b) { const auto s = band (b); const float d = p.getDistanceFrom ({ xOf (s.freq), yOf (jlimit (-gMax, gMax, s.gain)) }); if (d < bd) { bd = d; best = b; } }
        return best;
    }
    RangedAudioParameter* prm (int b, const char* k) const { return proc.apvts.getParameter (bid (b, k)); }
    void mouseDown (const MouseEvent& e) override { drag = hit (e.position); if (drag >= 0) for (auto* k : { "Freq", "Gain" }) prm (drag, k)->beginChangeGesture(); }
    void mouseDrag (const MouseEvent& e) override
    {
        if (drag < 0) return;
        auto* pf = prm (drag, "Freq"); auto* pg = prm (drag, "Gain");
        const float fine = e.mods.isAltDown() ? 0.1f : 1.0f;
        const auto d = e.position - lastPos; lastPos = e.position;
        if (! e.mods.isAltDown()) { pf->setValueNotifyingHost (pf->convertTo0to1 (jlimit (20.0f, 20000.0f, fOf (e.position.x)))); if (! e.mods.isShiftDown()) pg->setValueNotifyingHost (pg->convertTo0to1 (jlimit (-20.0f, 20.0f, gOf (e.position.y))));
        }
        else
        {
            const float f = proc.apvts.getRawParameterValue (bid (drag, "Freq"))->load() * std::pow (2.0f, d.x * fine / 120.0f);
            const float gn = proc.apvts.getRawParameterValue (bid (drag, "Gain"))->load() - d.y * fine * 0.1f;
            pf->setValueNotifyingHost (pf->convertTo0to1 (jlimit (20.0f, 20000.0f, f))); pg->setValueNotifyingHost (pg->convertTo0to1 (jlimit (-20.0f, 20.0f, gn)));
        }
    }
    void mouseUp (const MouseEvent&) override { if (drag >= 0) for (auto* k : { "Freq", "Gain" }) prm (drag, k)->endChangeGesture(); drag = -1; }
    void mouseMove (const MouseEvent& e) override { lastPos = e.position; setMouseCursor (hit (e.position) >= 0 ? MouseCursor::DraggingHandCursor : MouseCursor::NormalCursor); }
    void mouseDoubleClick (const MouseEvent& e) override { const int b = hit (e.position); if (b >= 0) { auto* p = prm (b, "Gain"); p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (0.0f)); p->endChangeGesture(); } }
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        const int b = hit (e.position); if (b < 0) return;
        auto* p = prm (b, "Q"); const float q = proc.apvts.getRawParameterValue (bid (b, "Q"))->load() * std::pow (2.0f, w.deltaY * (e.mods.isAltDown() ? 0.1f : 0.6f));
        p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (jlimit (0.25f, 4.0f, q))); p->endChangeGesture();
    }
private:
    ChannelStripProcessor& proc; CSLook& look; Path total, fill, bandPath[4]; int64 lastHash = 0; int drag = -1; Point<float> lastPos;
};

class EqPanel : public Panel
{
public:
    EqPanel (ChannelStripProcessor& p, CSLook& l) : Panel (p, l, cs::ModEq, "EQ", "eqOn", "eqRoute"), graph (p, l)
    {
        width = 1290; auto& ap = p.apvts;
        model.init (*this, ap, "eqModel");
        model.c.setTooltip ("One model for all bands. Choosing a model sets its classic frequencies; every band stays free from 20 Hz to 20 kHz.");
        addAndMakeVisible (graph);
        drive.init (*this, ap, "eqDrive", "DRIVE", 44, true);
        drive.s.setTooltip ("Model colour (transformers, tubes, VCAs). 50 % = as measured on the reference plugins");
        static const char* const sub[] = { "shelf / bell", "wide", "focused", "shelf / bell" };
        for (int b = 0; b < 4; ++b)
        {
            auto& c = col[b];
            c.name = cs::bandName (b); c.sub = sub[b];
            c.on.init (*this, ap, bid (b, "On"), c.name);
            c.gain.init (*this, ap, bid (b, "Gain"), "GAIN", 76);
            c.freq.init (*this, ap, bid (b, "Freq"), "FREQ", 56);
            c.q.init (*this, ap, bid (b, "Q"), "Q", 50, true);
            c.aux.init (*this, ap, bid (b, "Aux"), "ATTEN", 50);
            c.shape.init (ap, bid (b, "Shape"), { "BELL", "SHELF" }); addAndMakeVisible (c.shape);
            c.auxSel.init (ap, bid (b, "AuxSel"), { "5k", "10k", "20k" }); addAndMakeVisible (c.auxSel);
            c.gain.s.textFromValueFunction = [] (double v) { return (v > 0.05 ? "+" : "") + String (v, 1) + " dB"; };
            c.freq.s.textFromValueFunction = [] (double v) { return v >= 1000.0 ? String (v / 1000.0, v >= 10000.0 ? 1 : 2) + " kHz" : String (v, v < 100.0 ? 1 : 0) + " Hz"; };
            const int bb = b;
            c.q.s.textFromValueFunction = [this, bb] (double v) {
                const auto s = graph.band (bb); const int m = graph.model();
                if (cs::bandIsShelf (m, s, bb)) return String (v, 2) + "x";
                return "Q " + String (cs::bandQ (m, bb, s.gain) * (float) v, 2); };
            c.aux.s.textFromValueFunction = [] (double v) { return String (v, 1) + " dB"; };
            c.freq.s.setTooltip ("Free 20 Hz - 20 kHz. Type a value, or Option/Alt-drag for fine steps.");
        }
    }
    void layout (Rectangle<int>) override
    {
        model.c.setBounds (76, 4, 250, 24);
        drive.place (getWidth() - 110, -2); drive.cap.setBounds (getWidth() - 190, 8, 70, 14); drive.s.setBounds (getWidth() - 132, 2, 44, 30);
        drive.s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        graph.setBounds (10, kHeader + 8, 780, getHeight() - kHeader - 18);
        auto r = Rectangle<int> (802, kHeader + 8, getWidth() - 812, getHeight() - kHeader - 18);
        const int w = r.getWidth() / 4;
        for (int b = 0; b < 4; ++b)
        {
            auto c = Rectangle<int> (r.getX() + b * w, r.getY(), w - 6, r.getHeight());
            auto& k = col[b]; k.area = c;
            k.on.b.setBounds (c.getX() + 8, c.getY() + 6, c.getWidth() - 16, 22);
            const int cx = c.getCentreX();
            k.gain.place (cx, c.getY() + 46);
            const int y2 = c.getY() + 46 + k.gain.height() + 2;
            k.freq.place (cx - 29, y2); k.q.place (cx + 30, y2); k.aux.place (cx + 30, y2);
            const int y3 = y2 + k.freq.height() + 4;
            k.shape.setBounds (c.getX() + 8, y3, c.getWidth() - 16, 22);
            k.auxSel.setBounds (c.getX() + 8, y3, c.getWidth() - 16, 22);
        }
    }
    void paintBody (Graphics& g) override
    {
        const auto& t = look.th;
        g.setColour (t.dim); g.setFont (uiFont (11.0f));
        g.drawText ("one model for all bands  -  the model sets its classic frequencies, every band is free 20 Hz - 20 kHz", 340, 8, 560, 18, Justification::centredLeft);
        for (int b = 0; b < 4; ++b)
        {
            auto r = col[b].area.toFloat();
            g.setColour (t.inset.withAlpha (t.flat ? 1.0f : 0.85f)); g.fillRoundedRectangle (r, 6.0f);
            g.setColour (kBandCol[b]); g.fillRoundedRectangle (r.getX() + 8, r.getY() + 30, r.getWidth() - 16, 3.0f, 1.5f);
            g.setColour (t.dim); g.setFont (uiFont (10.5f)); g.drawText (col[b].sub, r.withTrimmedTop (32).withHeight (14).toNearestInt(), Justification::centred);
        }
    }
    void refresh (bool focus) override
    {
        route.refresh(); const int m = graph.model();
        for (int b = 0; b < 4; ++b)
        {
            auto& k = col[b]; const bool pultecAux = cs::eqHasAux (m, b);
            k.shape.refresh(); k.auxSel.refresh();
            const bool shelfChoice = (b == cs::BandLow || b == cs::BandHigh) && m != cs::EqPultec;
            k.shape.setVisible (shelfChoice && ! focus);
            k.auxSel.setVisible (pultecAux && b == cs::BandHigh && ! focus);
            // Pultec low: boost + atten (1st-order, no Q); Pultec high: boost, bandwidth, atten + select
            k.aux.setVisible (pultecAux && (b == cs::BandLow || ! focus) && b == cs::BandLow);
            k.q.setVisible (! (pultecAux && b == cs::BandLow) && ! focus);
            if (pultecAux && b == cs::BandHigh) { k.aux.setVisible (! focus); k.q.place (k.area.getCentreX() + 30, k.freq.cap.getY()); }
            k.gain.setCaption (m == cs::EqPultec && (b == cs::BandLow || b == cs::BandHigh) ? "BOOST" : "GAIN");
            k.q.setCaption (m == cs::EqPultec && b == cs::BandHigh ? "BANDWIDTH" : (cs::bandIsShelf (m, graph.band (b), b) ? "SLOPE" : "Q"));
            if (pultecAux && b == cs::BandHigh)
            {
                // atten knob takes the shape row's place; atten select sits under it
                k.aux.place (k.area.getCentreX(), k.freq.cap.getY() + k.freq.height() + 2);
                k.auxSel.setBounds (k.area.getX() + 8, k.aux.s.getBottom() + 2, k.area.getWidth() - 16, 20);
            }
            k.q.s.updateText();
        }
    }
    struct Col { String name, sub; Toggle on; Knob gain, freq, q, aux; Segments shape, auxSel; Rectangle<int> area; };
    Choice model; EqGraph graph; Knob drive; Col col[4];
};

// ============================================================================ VU
class VuMeter : public Component
{
public:
    std::function<float()> get; CSLook* look = nullptr; float needle = -20.0f;
    void tick() { const float v = get ? get() : 0.0f; const float db = jlimit (-22.0f, 4.0f, 20.0f * std::log10 (v + 1e-9f) + 18.0f); needle += (db - needle) * 0.18f; repaint(); }
    void paint (Graphics& g) override
    {
        const auto& t = look->th; auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (r, 6.0f);
        auto face = r.reduced (3.0f);
        if (t.flat) g.setColour (t.vuFace); else g.setGradientFill (ColourGradient (t.vuFace.brighter (0.1f), 0, face.getY(), t.vuFace.darker (0.15f), 0, face.getBottom(), false));
        g.fillRoundedRectangle (face, 4.0f);
        const Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.3f);
        const float len = face.getHeight() * 1.08f;
        auto angOf = [] (float db) { return -0.8f + 1.6f * (db + 20.0f) / 23.0f; };
        g.setFont (uiFont (9.0f, true));
        for (float db : { -20.f, -10.f, -7.f, -5.f, -3.f, -1.f, 0.f, 1.f, 2.f, 3.f })
        {
            const float a = angOf (db);
            g.setColour (db > 0 ? Colour (0xffc0392b) : t.vuInk);
            g.drawLine (Line<float> (pivot.getPointOnCircumference (len * 0.84f, a), pivot.getPointOnCircumference (len * 0.92f, a)), 1.2f);
            const auto pt = pivot.getPointOnCircumference (len * 0.99f, a);
            g.drawText (db > 0 ? "+" + String ((int) db) : String ((int) db), (int) pt.x - 12, (int) pt.y - 6, 24, 10, Justification::centred);
        }
        g.setColour (t.vuInk); g.setFont (uiFont (12.0f, true)); g.drawText ("VU", face.withTrimmedTop (face.getHeight() * 0.55f).toNearestInt(), Justification::centredTop);
        g.setColour (Colours::black.withAlpha (0.85f)); const float a = angOf (needle);
        g.drawLine (Line<float> (pivot.getPointOnCircumference (len * 0.4f, a), pivot.getPointOnCircumference (len * 0.95f, a)), 1.6f);
    }
};

// ============================================================================ editor
class ChannelStripEditor : public AudioProcessorEditor, private Timer
{
public:
    static constexpr int kW = 1600, kH = 840;
    ChannelStripEditor (ChannelStripProcessor& p)
        : AudioProcessorEditor (p), proc (p), pre (p, look), ds (p, look), comp (p, look), gate (p, look), ins (p, look), lim (p, look), eq (p, look)
    {
        auto& st = p.apvts.state;
        look.apply (makeTheme (st.getProperty ("theme", "Graphite").toString(), st.getProperty ("depth", "3D").toString() == "2D"));
        setLookAndFeel (&look);
        addAndMakeVisible (content); content.ed = this;
        auto& ap = p.apvts;
        panels = { &pre, &eq, &ds, &comp, &gate, &ins, &lim };
        for (auto* m : panels) content.addAndMakeVisible (m);

        // top bar
        presetBtn.setButtonText (p.presetName); presetBtn.onClick = [this] { showPresetMenu(); }; content.addAndMakeVisible (presetBtn);
        slots.build ({ "A", "B", "C", "D" }); slots.onPick = [this] (int k) { proc.selectSlot (k); }; slots.setSelected (p.currentSlot()); content.addAndMakeVisible (slots);
        undoBtn.setButtonText ("UNDO"); redoBtn.setButtonText ("REDO");
        undoBtn.onClick = [this] { proc.undo.undo(); }; redoBtn.onClick = [this] { proc.undo.redo(); };
        content.addAndMakeVisible (undoBtn); content.addAndMakeVisible (redoBtn);
        focusBtn.setButtonText ("FOCUS"); focusBtn.setClickingTogglesState (true); focusBtn.setTooltip ("Show only the key controls of each module");
        focusBtn.onClick = [this] { refreshAll(); layoutContent(); }; content.addAndMakeVisible (focusBtn);
        themeBox.addItemList ({ "Graphite", "Cream", "Midnight", "Forest" }, 1);
        themeBox.setText (look.th.name, dontSendNotification);
        themeBox.onChange = [this] { setTheme (themeBox.getText(), look.th.flat); }; content.addAndMakeVisible (themeBox);
        depth.build ({ "3D", "2D" }); depth.setSelected (look.th.flat ? 1 : 0); depth.onPick = [this] (int k) { setTheme (look.th.name, k == 1); depth.setSelected (k); };
        content.addAndMakeVisible (depth);
        osSel.init (ap, "os"); content.addAndMakeVisible (osSel);
        zeroLat.init (content, ap, "zeroLat", "ZERO LATENCY");
        zeroLat.b.setTooltip ("No oversampling, no limiter look-ahead: no added latency (for tracking and live)");
        sizes.build ({ "75%", "100%", "125%", "150%", "200%" });
        sizes.onPick = [this] (int k) { static const float s[] = { 0.75f, 1.0f, 1.25f, 1.5f, 2.0f }; setSize ((int) std::lround (kW * s[k]), (int) std::lround (kH * s[k])); };
        content.addAndMakeVisible (sizes);

        // rails
        inGain.init (content, ap, "inGain", "INPUT", 72); rotDeg.init (content, ap, "rotDeg", "PHASE", 64);
        rotDeg.s.textFromValueFunction = [] (double v) { return String ((int) std::lround (v)) + String (CharPointer_UTF8 ("\xc2\xb0")); };
        inGain.s.textFromValueFunction = [] (double v) { return (v > 0.05 ? "+" : "") + String (v, 1) + " dB"; };
        polL.init (content, ap, "polL", String (CharPointer_UTF8 ("\xc3\x98 L"))); polR.init (content, ap, "polR", String (CharPointer_UTF8 ("\xc3\x98 R")));
        rotOn.init (content, ap, "rotOn", "ROTATE"); rotOn.b.setTooltip ("Broadband phase rotation: the same phase shift at every frequency");
        inMode.init (content, ap, "inMode");
        outGain.init (content, ap, "outGain", "OUTPUT", 76);
        outGain.s.textFromValueFunction = [] (double v) { return (v > 0.05 ? "+" : "") + String (v, 1) + " dB"; };
        for (auto* m : { &inL, &inR, &outL, &outR }) { m->look = &look; content.addAndMakeVisible (m); }
        inL.get = [&p] { return p.inMeter.load(); }; inR.get = inL.get; outL.get = [&p] { return p.outMeter.load(); }; outR.get = outL.get;
        vu.look = &look; vu.get = [&p] { return p.vuMeter.load(); }; content.addAndMakeVisible (vu);

        tooltip.setMillisecondsBeforeTipAppears (700);
        refreshAll();
        const double sc = jlimit (0.5, 2.0, (double) st.getProperty ("scale", 1.0));
        setResizable (true, true);
        setResizeLimits (kW / 2, kH / 2, kW * 2, kH * 2);
        getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
        setSize ((int) std::lround (kW * sc), (int) std::lround (kH * sc));
        startTimerHz (30);
    }
    ~ChannelStripEditor() override { setLookAndFeel (nullptr); }

    void setTheme (const String& name, bool flat)
    {
        look.apply (makeTheme (name, flat));
        proc.apvts.state.setProperty ("theme", look.th.name, nullptr); proc.apvts.state.setProperty ("depth", flat ? "2D" : "3D", nullptr);
        sendLookAndFeelChange(); repaint(); content.repaint(); for (auto* m : panels) m->repaint();
    }
    void paint (Graphics& g) override { g.fillAll (look.th.bg); }
    void resized() override
    {
        const float s = (float) getWidth() / (float) kW;
        content.setBounds (0, 0, kW, kH); content.setTransform (AffineTransform::scale (s));
        proc.apvts.state.setProperty ("scale", s, nullptr);
        static const float steps[] = { 0.75f, 1.0f, 1.25f, 1.5f, 2.0f }; int sel = -1;
        for (int k = 0; k < 5; ++k) if (std::abs (steps[k] - s) < 0.01f) sel = k;
        sizes.setSelected (sel);
        layoutContent();
    }

    void layoutContent()
    {
        // top bar
        int x = 220;
        presetBtn.setBounds (x, 18, 240, 28); x += 250;
        slots.setBounds (x, 18, 128, 28); x += 140;
        undoBtn.setBounds (x, 18, 56, 28); redoBtn.setBounds (x + 60, 18, 56, 28); x += 128;
        focusBtn.setBounds (x, 18, 70, 28);
        x = kW - 14 - 12;
        sizes.setBounds (x - 230, 18, 230, 28); x -= 230 + 60;
        zeroLat.b.setBounds (x - 110, 18, 110, 28); x -= 118;
        osSel.setBounds (x - 104, 18, 104, 28); x -= 104 + 104;
        depth.setBounds (x - 76, 18, 76, 28); x -= 84;
        themeBox.setBounds (x - 120, 18, 120, 28);
        themeX = themeBox.getX(); osX = osSel.getX(); sizeX = sizes.getX();

        const int top = 66, bottom = kH - 12;
        // input rail
        auto in = Rectangle<int> (14, top, 112, bottom - top); inRail = in;
        inGain.place (in.getCentreX(), in.getY() + 34);
        int y = in.getY() + 34 + inGain.height() + 6;
        inMode.c.setBounds (in.getX() + 10, y, in.getWidth() - 20, 26); y += 34;
        polL.b.setBounds (in.getX() + 10, y, 44, 26); polR.b.setBounds (in.getRight() - 54, y, 44, 26); y += 36;
        rotOn.b.setBounds (in.getX() + 10, y, in.getWidth() - 20, 26); y += 32;
        rotDeg.place (in.getCentreX(), y); y += rotDeg.height() + 10;
        inL.setBounds (in.getCentreX() - 12, y, 9, bottom - 16 - y); inR.setBounds (in.getCentreX() + 3, y, 9, bottom - 16 - y);
        // output rail
        auto out = Rectangle<int> (kW - 14 - 150, top, 150, bottom - top); outRail = out;
        vu.setBounds (out.getX() + 9, out.getY() + 32, 132, 86);
        outGain.place (out.getCentreX(), out.getY() + 132);
        y = out.getY() + 132 + outGain.height() + 12;
        outL.setBounds (out.getCentreX() - 12, y, 9, bottom - 16 - y); outR.setBounds (out.getCentreX() + 3, y, 9, bottom - 16 - y);
        // centre: EQ on top, chain bar, module row in chain order
        const int cx0 = 136, cw = kW - 14 - 150 - 10 - cx0;
        eq.setBounds (cx0, top, cw, 350);
        chainY = top + 350 + 8;
        const int rowY = chainY + 38, rowH = bottom - rowY;
        const auto order = cs::orderFromIndex ((int) proc.apvts.getRawParameterValue ("order")->load());
        int xx = cx0;
        for (int k = 0; k < cs::kNumModules; ++k)
        {
            const int m = order[(size_t) k]; if (m == cs::ModEq) continue;
            auto* pnl = panels[(size_t) m];
            pnl->setBounds (xx, rowY, (int) pnl->width, rowH); xx += (int) pnl->width + 10;
        }
        // chain chips
        chips.clear(); int chx = cx0 + 110;
        for (int k = 0; k < cs::kNumModules; ++k)
        {
            const int m = order[(size_t) k]; const int w = 30 + (int) String (cs::moduleName (m)).length() * 9;
            chips.push_back ({ m, Rectangle<int> (chx, chainY + 2, w, 26) }); chx += w + 26;
        }
        content.repaint();
    }

    void refreshAll()
    {
        const bool f = focusBtn.getToggleState();
        for (auto* m : panels) m->refresh (f);
        osSel.refresh(); presetBtn.setButtonText (proc.presetName); slots.setSelected (proc.currentSlot());
    }

    void showPresetMenu()
    {
        PopupMenu m;
        const auto& list = factoryPresets();
        for (int i = 0; i < (int) list.size(); ++i) m.addItem (1 + i, list[(size_t) i].name);
        m.addSeparator();
        auto folder = ChannelStripProcessor::userPresetFolder();
        auto files = folder.findChildFiles (File::findFiles, false, "*.csp");
        PopupMenu user; for (int i = 0; i < files.size(); ++i) user.addItem (500 + i, files[i].getFileNameWithoutExtension());
        if (files.size() > 0) m.addSubMenu ("My presets", user);
        m.addItem (900, "Save preset...");
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (presetBtn), [this, files] (int r) {
            if (r <= 0) return;
            if (r < 500) proc.applyFactory (r - 1);
            else if (r < 900) proc.loadUserPreset (files[r - 500]);
            else
            {
                chooser = std::make_unique<FileChooser> ("Save preset", ChannelStripProcessor::userPresetFolder().getChildFile ("My preset.csp"), "*.csp");
                ChannelStripProcessor::userPresetFolder().createDirectory();
                chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting, [this] (const FileChooser& fc) {
                    auto f = fc.getResult(); if (f == File()) return;
                    proc.saveUserPreset (f.withFileExtension ("csp")); presetBtn.setButtonText (proc.presetName); });
            }
            presetBtn.setButtonText (proc.presetName);
        });
    }

    // chain bar drag
    void chainMouse (const MouseEvent& e, int phase)
    {
        const auto p = e.getEventRelativeTo (&content).position.toInt();
        if (phase == 0) { dragChip = -1; for (size_t i = 0; i < chips.size(); ++i) if (chips[i].r.contains (p)) { dragChip = (int) i; dragX = p.x; } return; }
        if (dragChip < 0) return;
        if (phase == 1) { dragOffset = p.x - dragX; content.repaint(); return; }
        const int cxm = chips[(size_t) dragChip].r.getCentreX() + dragOffset;
        std::vector<int> rest; for (size_t i = 0; i < chips.size(); ++i) if ((int) i != dragChip) rest.push_back (chips[i].m);
        int pos = 0; for (size_t i = 0, j = 0; i < chips.size(); ++i) { if ((int) i == dragChip) continue; if (chips[i].r.getCentreX() < cxm) pos = (int) j + 1; ++j; }
        rest.insert (rest.begin() + pos, chips[(size_t) dragChip].m);
        std::array<int, cs::kNumModules> no {}; for (int k = 0; k < cs::kNumModules; ++k) no[(size_t) k] = rest[(size_t) k];
        if (auto* prm = proc.apvts.getParameter ("order")) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 ((float) cs::indexFromOrder (no))); prm->endChangeGesture(); }
        dragChip = -1; dragOffset = 0; layoutContent();
    }

    class Content : public Component
    {
    public:
        ChannelStripEditor* ed = nullptr;
        void paint (Graphics& g) override
        {
            const auto& t = ed->look.th; auto r = getLocalBounds().toFloat();
            g.fillAll (t.bg);
            auto bar = Rectangle<float> (14, 12, r.getWidth() - 28, 40);
            if (t.flat) g.setColour (t.bar); else g.setGradientFill (ColourGradient (t.bar, 0, bar.getY(), t.panel, 0, bar.getBottom(), false));
            g.fillRoundedRectangle (bar, 8.0f);
            g.setColour (t.text); g.setFont (uiFont (18.0f, true));
            g.drawText ("CHANNEL STRIP", 30, 12, 160, 40, Justification::centredLeft);
            g.setColour (t.accent); g.drawText ("2", 178, 12, 20, 40, Justification::centredLeft);
            g.setColour (t.dim); g.setFont (uiFont (11.0f, true));
            g.drawText ("THEME", ed->themeX - 52, 12, 48, 40, Justification::centredRight);
            g.drawText ("OVERSAMPLING", ed->osX - 100, 12, 96, 40, Justification::centredRight);
            g.drawText ("SIZE", ed->sizeX - 40, 12, 36, 40, Justification::centredRight);
            for (auto rr : { ed->inRail, ed->outRail })
            {
                auto f = rr.toFloat();
                if (t.flat) g.setColour (t.panel); else { g.setColour (Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (f.translated (0, 3), 8.0f); g.setGradientFill (ColourGradient (t.panel, 0, f.getY(), t.inset, 0, f.getBottom(), false)); }
                g.fillRoundedRectangle (f, 8.0f);
            }
            g.setColour (t.text); g.setFont (uiFont (13.0f, true));
            g.drawText ("INPUT", ed->inRail.withHeight (30), Justification::centred);
            g.drawText ("OUTPUT", ed->outRail.withHeight (30), Justification::centred);
            // chain bar
            g.setColour (t.dim); g.setFont (uiFont (11.0f, true));
            g.drawText ("SIGNAL CHAIN", 136, ed->chainY, 104, 30, Justification::centredLeft);
            for (size_t i = 0; i < ed->chips.size(); ++i)
            {
                auto cr = ed->chips[i].r.toFloat(); if ((int) i == ed->dragChip) cr = cr.translated ((float) ed->dragOffset, 0);
                if (i > 0)
                {
                    const float ax = ed->chips[i].r.getX() - 20.0f, ay = cr.getCentreY();
                    Path a; a.startNewSubPath (ax, ay); a.lineTo (ax + 12, ay); a.startNewSubPath (ax + 8, ay - 4); a.lineTo (ax + 12, ay); a.lineTo (ax + 8, ay + 4);
                    g.setColour (t.accent); g.strokePath (a, PathStrokeType (1.6f));
                }
                const bool on = ed->proc.apvts.getRawParameterValue (kOnIds[ed->chips[i].m])->load() > 0.5f;
                g.setColour (t.field); g.fillRoundedRectangle (cr, 13.0f);
                g.setColour ((int) i == ed->dragChip ? t.accent : t.line); g.drawRoundedRectangle (cr, 13.0f, 1.0f);
                g.setColour (on ? t.text : t.dim); g.setFont (uiFont (12.0f, true));
                g.drawText (cs::moduleName (ed->chips[i].m), cr.withTrimmedLeft (14).toNearestInt(), Justification::centred);
                g.setColour (t.dim); for (int dy = 0; dy < 3; ++dy) for (int dx = 0; dx < 2; ++dx) g.fillEllipse (cr.getX() + 10 + dx * 4, cr.getCentreY() - 5 + dy * 4, 2.2f, 2.2f);
            }
            g.setColour (t.dim); g.setFont (uiFont (11.0f));
            if (! ed->chips.empty()) g.drawText ("drag to reorder", ed->chips.back().r.getRight() + 16, ed->chainY, 200, 30, Justification::centredLeft);
        }
        void mouseDown (const MouseEvent& e) override { if (e.y >= ed->chainY && e.y < ed->chainY + 30) ed->chainMouse (e, 0); }
        void mouseDrag (const MouseEvent& e) override { ed->chainMouse (e, 1); }
        void mouseUp (const MouseEvent& e) override { ed->chainMouse (e, 2); }
    };

    void timerCallback() override
    {
        refreshAll();
        for (auto* m : { &inL, &inR, &outL, &outR }) m->repaint();
        vu.tick();
        const int ord = (int) proc.apvts.getRawParameterValue ("order")->load();
        if (ord != lastOrder && dragChip < 0) { lastOrder = ord; layoutContent(); }
        rotDeg.s.setEnabled (rotOn.b.getToggleState());
    }

    ChannelStripProcessor& proc;
    CSLook look; TooltipWindow tooltip { this };
    Content content;
    PrePanel pre; DsPanel ds; CompPanel comp; GatePanel gate; InsertPanel ins; LimPanel lim; EqPanel eq;
    std::array<Panel*, cs::kNumModules> panels {};
    TextButton presetBtn, undoBtn, redoBtn, focusBtn; Segments slots, depth, osSel, sizes; ComboBox themeBox; Toggle zeroLat;
    Knob inGain, rotDeg, outGain; Toggle polL, polR, rotOn; Choice inMode; LevelMeter inL, inR, outL, outR; VuMeter vu;
    struct Chip { int m; Rectangle<int> r; }; std::vector<Chip> chips;
    Rectangle<int> inRail, outRail; int chainY = 0, dragChip = -1, dragX = 0, dragOffset = 0, lastOrder = -1, themeX = 0, osX = 0, sizeX = 0;
    std::unique_ptr<FileChooser> chooser;
};
