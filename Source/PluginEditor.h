// Flexiverb editor (included by PluginProcessor.cpp)
// A hardware-style front panel after the classic LARC remote: LED display with a three-band decay graph,
// six long faders with pages, soft keys for the "virtual screens" (Advanced, IR, Hardware, Outputs),
// a fold-away side panel (EQ / Compressor / Delay) and a LOOK panel for colours, gradient, lighting and contrast.

#include <complex>

// ============================================================================ theme
struct Theme
{
    Colour panel { 0xff454441 }, led { 0xffff3b1f };
    float grad = 0.6f, light = 0.6f, contrast = 0.5f; bool flat = false;

    float g() const { return flat ? 0.0f : grad; }
    float l() const { return flat ? 0.12f : light; }
    static Colour shade (Colour c, float k) { return k >= 0.0f ? c.interpolatedWith (Colours::white, jmin (1.0f, k)) : c.interpolatedWith (Colours::black, jmin (1.0f, -k)); }
    float deep() const { return -0.45f - 0.4f * contrast; }
    Colour chassis() const { return shade (panel, -0.35f); }
    Colour rail() const    { return shade (panel, -0.18f); }
    Colour side() const    { return shade (panel, -0.12f); }
    Colour bay() const     { return shade (panel, deep() * 0.8f); }
    Colour bezel() const   { return shade (panel, deep()); }
    Colour screen() const  { return shade (led, -0.93f); }
    Colour ledOff() const  { return shade (led, -0.80f); }
    Colour label() const   { return shade (Colour (0xffd8d2c2), 0.15f * contrast - 0.05f); }
    Colour dim() const     { return shade (Colour (0xffa29c8e), -0.2f + 0.25f * contrast); }
    Colour title() const   { return shade (Colour (0xffece6d8), 0.1f * contrast); }
    Colour scale() const   { return shade (Colour (0xff8d877a), 0.3f * contrast - 0.1f); }
    static Colour bandLo()  { return Colour (0xffff7a45); }
    static Colour bandMid() { return Colour (0xffffc94d); }
    static Colour bandHi()  { return Colour (0xff5ec8ff); }

    // a panel face: vertical gradient (gradient amount) with a light top edge and a dark bottom edge (lighting)
    void fillRaised (Graphics& gr, Rectangle<float> r, Colour base, float corner, float amt = 1.0f) const
    {
        const float lt = l();
        if (lt > 0.01f)
        {
            Path p; p.addRoundedRectangle (r, corner);
            DropShadow (Colours::black.withAlpha (0.25f + 0.45f * lt), roundToInt (2 + 8 * lt), { 0, roundToInt (1 + 3 * lt) }).drawForPath (gr, p);
        }
        gr.setGradientFill (ColourGradient (shade (base, 0.14f * g() * amt), r.getX(), r.getY(), shade (base, -0.18f * g() * amt), r.getX(), r.getBottom(), false));
        gr.fillRoundedRectangle (r, corner);
        gr.setColour (Colours::white.withAlpha (0.04f + 0.27f * lt)); gr.drawHorizontalLine ((int) r.getY() + 1, r.getX() + corner, r.getRight() - corner);
        gr.setColour (Colours::black.withAlpha (0.5f)); gr.drawHorizontalLine ((int) r.getBottom() - 1, r.getX() + corner, r.getRight() - corner);
    }
    // a recess: dark well with an inner shadow along the top and a light lip at the bottom
    void fillSunken (Graphics& gr, Rectangle<float> r, Colour base, float corner) const
    {
        gr.setGradientFill (ColourGradient (shade (base, 0.07f * g()), r.getX(), r.getY(), shade (base, -0.1f * g()), r.getX(), r.getBottom(), false));
        gr.fillRoundedRectangle (r, corner);
        const float d = 2.0f + 5.0f * l();
        gr.setGradientFill (ColourGradient (Colours::black.withAlpha (0.55f), r.getX(), r.getY(), Colours::transparentBlack, r.getX(), r.getY() + d, false));
        gr.fillRoundedRectangle (r.withHeight (d + corner), corner);
        gr.setColour (Colours::white.withAlpha (0.03f + 0.2f * l())); gr.drawHorizontalLine ((int) r.getBottom(), r.getX() + corner, r.getRight() - corner);
    }
    void drawLed (Graphics& gr, Point<float> c, float r, bool on) const
    {
        if (on)
        {
            gr.setColour (led.withAlpha (0.25f + 0.2f * l())); gr.fillEllipse (c.x - r * 2.2f, c.y - r * 2.2f, r * 4.4f, r * 4.4f);
            gr.setColour (led); gr.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
            gr.setColour (Colours::white.withAlpha (0.55f)); gr.fillEllipse (c.x - r * 0.45f, c.y - r * 0.6f, r * 0.7f, r * 0.55f);
        }
        else
        {
            gr.setColour (ledOff()); gr.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
            gr.setColour (Colours::black.withAlpha (0.6f)); gr.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.8f);
        }
    }
    void drawScrew (Graphics& gr, Point<float> c) const
    {
        gr.setGradientFill (ColourGradient (Colour (0xffa5a5a5), c.x - 3, c.y - 3, Colour (0xff3a3a3a), c.x + 4, c.y + 5, true));
        gr.fillEllipse (c.x - 6, c.y - 6, 12, 12);
        gr.setColour (Colour (0xff1a1a1a)); gr.drawEllipse (c.x - 6, c.y - 6, 12, 12, 1.0f);
        gr.fillRect (c.x - 4.0f, c.y - 1.0f, 8.0f, 2.0f);
    }
    // LED-style text with a soft glow
    void ledText (Graphics& gr, const String& t, Rectangle<float> r, Justification j, float h, float alpha = 1.0f, Colour c = {}) const
    {
        const Colour col = c.isTransparent() ? led : c;
        gr.setFont (Font (FontOptions (Font::getDefaultMonospacedFontName(), h, Font::bold)));
        const float gl = 0.10f + 0.14f * l();
        gr.setColour (col.withAlpha (gl * alpha));
        for (auto o : { Point<float> (-1.2f, 0), Point<float> (1.2f, 0), Point<float> (0, -1.2f), Point<float> (0, 1.2f) })
            gr.drawText (t, r.translated (o.x, o.y), j, false);
        gr.setColour (col.withAlpha (alpha)); gr.drawText (t, r, j, false);
    }
};

static Font uiFont (float h, bool bold = false) { return Font (FontOptions (h, bold ? Font::bold : Font::plain)); }
static Font monoFont (float h) { return Font (FontOptions (Font::getDefaultMonospacedFontName(), h, Font::bold)); }

static const Colour kPanelChoices[] = { Colour (0xff454441), Colour (0xff2a2a2b), Colour (0xff8a8578), Colour (0xff1b2430), Colour (0xff5a3a2e), Colour (0xff3c4a3e) };
static const Colour kLedChoices[]   = { Colour (0xffff3b1f), Colour (0xffffb000), Colour (0xff5fe07a), Colour (0xff5ec8ff) };

// ============================================================================ look & feel
class FlexLook : public LookAndFeel_V4
{
public:
    Theme theme;
    FlexLook() { refresh(); }
    void refresh()
    {
        const auto& t = theme;
        setColour (ComboBox::backgroundColourId, t.screen());
        setColour (ComboBox::textColourId, t.led);
        setColour (ComboBox::outlineColourId, Colours::black);
        setColour (ComboBox::arrowColourId, t.led);
        setColour (PopupMenu::backgroundColourId, Theme::shade (t.panel, -0.6f));
        setColour (PopupMenu::textColourId, Colour (0xffe6e1d5));
        setColour (PopupMenu::highlightedBackgroundColourId, t.led.withAlpha (0.85f));
        setColour (PopupMenu::highlightedTextColourId, Colours::black);
        setColour (BubbleComponent::backgroundColourId, t.screen());
        setColour (BubbleComponent::outlineColourId, t.led.withAlpha (0.6f));
        setColour (TooltipWindow::textColourId, t.led);
        setColour (AlertWindow::backgroundColourId, Theme::shade (t.panel, -0.5f));
        setColour (AlertWindow::textColourId, Colour (0xffe6e1d5));
        setColour (TextEditor::backgroundColourId, t.screen());
        setColour (TextEditor::textColourId, t.led);
        setColour (TextButton::textColourOffId, Colour (0xff1d1c1a));
    }
    Font getComboBoxFont (ComboBox&) override { return monoFont (15.0f); }
    Font getPopupMenuFont() override { return uiFont (15.0f); }
    Font getSliderPopupFont (Slider&) override { return monoFont (15.0f); }

    void drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox&) override
    {
        auto r = Rectangle<float> (0, 0, (float) w, (float) h);
        g.setColour (Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r, 3.0f);
        theme.fillSunken (g, r.reduced (1.5f), theme.screen(), 2.0f);
        Path a; const float cx = (float) w - 12.0f, cy = (float) h * 0.5f;
        a.addTriangle (cx - 4.5f, cy - 2.5f, cx + 4.5f, cy - 2.5f, cx, cy + 3.0f);
        g.setColour (theme.led); g.fillPath (a);
    }
    void positionComboBoxText (ComboBox& box, Label& label) override
    {
        label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider&) override
    {
        auto b = Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
        const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        Path track; track.addCentredArc (c.x, c.y, r - 1.5f, r - 1.5f, 0.0f, a0, a1, true);
        g.setColour (Colours::black.withAlpha (0.65f));
        g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        Path val; val.addCentredArc (c.x, c.y, r - 1.5f, r - 1.5f, 0.0f, a0, ang, true);
        g.setColour (theme.led.withAlpha (0.9f));
        g.strokePath (val, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        const float kr = r - 7.0f;
        const float lt = theme.l();
        if (lt > 0.01f) { g.setColour (Colours::black.withAlpha (0.25f + 0.4f * lt)); g.fillEllipse (c.x - kr, c.y - kr + 1.0f + 3.0f * lt, kr * 2, kr * 2); }
        g.setGradientFill (ColourGradient (Theme::shade (Colour (0xff4a4a4c), 0.2f * lt + 0.1f * theme.g()), c.x - kr * 0.3f, c.y - kr * 0.7f,
                                           Colour (0xff141415), c.x + kr * 0.5f, c.y + kr, true));
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
        g.setColour (Colours::white.withAlpha (0.06f + 0.25f * lt)); g.drawEllipse (c.x - kr + 1.0f, c.y - kr + 1.0f, kr * 2 - 2.0f, kr * 2 - 2.0f, 1.0f);
        g.setColour (Colour (0xfff2ede0));
        g.drawLine (Line<float> (c.getPointOnCircumference (kr * 0.25f, ang), c.getPointOnCircumference (kr * 0.9f, ang)), 2.5f);
    }

    void drawLinearSlider (Graphics& g, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           const Slider::SliderStyle style, Slider& s) override
    {
        if (style == Slider::LinearHorizontal)
        {
            const float cy = (float) y + (float) h * 0.5f;
            g.setColour (Colours::black.withAlpha (0.8f)); g.fillRoundedRectangle ((float) x, cy - 2.5f, (float) w, 5.0f, 2.5f);
            g.setColour (theme.led); g.fillRoundedRectangle ((float) x, cy - 2.5f, sliderPos - (float) x, 5.0f, 2.5f);
            g.setColour (Colour (0xffe6e1d5)); g.fillEllipse (sliderPos - 7.0f, cy - 7.0f, 14.0f, 14.0f);
            g.setColour (Colours::black.withAlpha (0.5f)); g.drawEllipse (sliderPos - 7.0f, cy - 7.0f, 14.0f, 14.0f, 1.0f);
            return;
        }
        if (style != Slider::LinearVertical) { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s); return; }
        const float cx = (float) x + (float) w * 0.5f;
        // printed scale on both sides of the slot
        g.setColour (theme.scale());
        for (int i = 0; i <= 10; ++i)
        {
            const float ty = (float) y + (float) h * (float) i / 10.0f;
            const float len = (i % 5 == 0) ? 12.0f : 7.0f;
            g.drawLine (cx - 14.0f - len, ty, cx - 14.0f, ty, 1.0f);
            g.drawLine (cx + 14.0f, ty, cx + 14.0f + len, ty, 1.0f);
        }
        // slot
        g.setColour (Colour (0xff050505)); g.fillRoundedRectangle (cx - 4.0f, (float) y - 6.0f, 8.0f, (float) h + 12.0f, 4.0f);
        g.setColour (Colours::white.withAlpha (0.05f + 0.15f * theme.l())); g.drawVerticalLine ((int) (cx + 4.0f), (float) y - 4.0f, (float) (y + h) + 4.0f);
        // black cap with a white line
        Rectangle<float> cap (cx - 24.0f, sliderPos - 18.0f, 48.0f, 36.0f);
        const float lt = theme.l();
        if (lt > 0.01f) { g.setColour (Colours::black.withAlpha (0.35f + 0.4f * lt)); g.fillRoundedRectangle (cap.translated (0.0f, 2.0f + 4.0f * lt).expanded (1.0f), 4.0f); }
        ColourGradient cg (Theme::shade (Colour (0xff3a3a3c), 0.15f * lt), cap.getX(), cap.getY(), Theme::shade (Colour (0xff333335), 0.1f * lt), cap.getX(), cap.getBottom(), false);
        cg.addColour (0.30, Colour (0xff1a1a1b)); cg.addColour (0.50, Colour (0xff0d0d0e)); cg.addColour (0.70, Colour (0xff1a1a1b));
        if (theme.flat) cg = ColourGradient (Colour (0xff1a1a1b), cap.getX(), cap.getY(), Colour (0xff1a1a1b), cap.getX(), cap.getBottom(), false);
        g.setGradientFill (cg); g.fillRoundedRectangle (cap, 3.0f);
        g.setColour (Colours::white.withAlpha (0.08f + 0.3f * lt)); g.drawHorizontalLine ((int) cap.getY() + 1, cap.getX() + 3.0f, cap.getRight() - 3.0f);
        g.setColour (Colour (0xfff2ede0)); g.fillRect (cap.getX() + 6.0f, cap.getCentreY() - 1.5f, cap.getWidth() - 12.0f, 3.0f);
    }
    int getSliderThumbRadius (Slider& s) override { return s.isVertical() ? 18 : (s.isHorizontal() ? 8 : LookAndFeel_V4::getSliderThumbRadius (s)); }

    // plain TextButtons (popup menus' parents, alert windows): cream keys
    void drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        if (down) r = r.translated (0.0f, 1.0f);
        theme.fillRaised (g, r, Colour (0xffcfc9bb), 3.0f, 1.4f);
        if (over && ! down) { g.setColour (Colours::white.withAlpha (0.12f)); g.fillRoundedRectangle (r, 3.0f); }
    }
    void drawButtonText (Graphics& g, TextButton& b, bool, bool) override
    {
        g.setColour (Colour (0xff1d1c1a)); g.setFont (uiFont (13.0f, true));
        g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred, false);
    }
};
static const Theme& themeOf (Component& c)
{
    if (auto* l = dynamic_cast<FlexLook*> (&c.getLookAndFeel())) return l->theme;
    static Theme t; return t;
}

// ============================================================================ controls
class FineSlider : public Slider
{
public:
    void mouseDown (const MouseEvent& e) override { Slider::mouseDown (e); last = e.position; prop = valueToProportionOfLength (getValue()); }
    void mouseDrag (const MouseEvent& e) override
    {
        if (e.mods.isAltDown())                       // Option (Mac) / Alt (Windows): ten times slower
        {
            const auto d = e.position - last; last = e.position;
            const double full = isVertical() ? (double) getHeight() : isHorizontal() ? (double) getWidth() : 320.0;
            const double delta = isVertical() ? -d.y : isHorizontal() ? d.x : (d.x - d.y);
            prop = jlimit (0.0, 1.0, prop + delta / (full * 10.0));
            setValue (proportionOfLengthToValue (prop), sendNotificationSync);
            return;
        }
        Slider::mouseDrag (e);
        last = e.position; prop = valueToProportionOfLength (getValue());
    }
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        if (e.mods.isAltDown()) { auto w2 = w; w2.deltaY *= 0.1f; w2.deltaX *= 0.1f; Slider::mouseWheelMove (e, w2); }
        else Slider::mouseWheelMove (e, w);
    }
private:
    Point<float> last; double prop = 0.0;
};

static String withUnit (RangedAudioParameter& p)
{
    const auto l = p.getLabel();
    const auto t = p.getCurrentValueAsText();
    const bool num = t.containsAnyOf ("0123456789");
    return t + (l.isEmpty() || l == "%" || l == "x" || ! num ? "" : " ") + (num ? l : String());
}

// a key with an LED above (or beside) it: dark keys for algorithms and tabs, cream keys for functions
class LedKey : public Button
{
public:
    enum Style { Dark, Cream };
    LedKey (const String& text = {}, Style s = Dark, bool ledAbove = true) : Button (text), style (s), above (ledAbove) {}
    Style style; bool above; bool showLed = true, verticalText = false; std::function<bool()> ledState;
    void paintButton (Graphics& g, bool over, bool down) override
    {
        const auto& t = themeOf (*this);
        auto r = getLocalBounds().toFloat();
        Rectangle<float> key = r;
        const bool lit = ledState ? ledState() : getToggleState();
        if (showLed)
        {
            if (above) { t.drawLed (g, { r.getCentreX(), r.getY() + 5.0f }, 4.0f, lit); key = r.withTrimmedTop (13.0f); }
            else { t.drawLed (g, { r.getX() + 5.0f, r.getCentreY() }, 3.6f, lit); key = r.withTrimmedLeft (14.0f); }
        }
        key = key.reduced (1.0f);
        if (down) key = key.translated (0.0f, 1.0f);
        t.fillRaised (g, key, style == Dark ? Colour (0xff262627) : Colour (0xffcfc9bb), 3.0f, 1.4f);
        if (over && ! down) { g.setColour (Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (key, 3.0f); }
        g.setColour (style == Dark ? Colour (0xffece6d8) : Colour (0xff1d1c1a));
        const String txt = getButtonText();
        g.setFont (uiFont (txt.length() > 9 ? 10.5f : 11.5f, true));
        if (verticalText)
        {
            Graphics::ScopedSaveState ss (g);
            g.addTransform (AffineTransform::rotation (MathConstants<float>::halfPi, key.getCentreX(), key.getCentreY()));
            g.drawText (txt, key.withSizeKeepingCentre (key.getHeight(), key.getWidth()), Justification::centred, false);
        }
        else g.drawText (txt, key, Justification::centred, false);
    }
};

// knob with its label above and an LED value readout below
class KnobBox : public Component
{
public:
    KnobBox (const String& lab) : label (lab)
    {
        knob.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        knob.setPopupDisplayEnabled (true, true, nullptr);
        knob.setMouseDragSensitivity (320);
        addAndMakeVisible (knob);
    }
    FineSlider knob; String label, overrideText; RangedAudioParameter* prm = nullptr; bool onScreen = false;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> att;
    void attach (AudioProcessorValueTreeState& st, const String& id)
    {
        att.reset(); prm = st.getParameter (id);
        if (prm == nullptr) return;
        att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (st, id, knob);
        knob.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    }
    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop (16); r.removeFromBottom (20);
        const int d = jmin (r.getWidth(), r.getHeight());
        knob.setBounds (r.withSizeKeepingCentre (d, d));
    }
    void paint (Graphics& g) override
    {
        const auto& t = themeOf (*this);
        const float a = isEnabled() ? 1.0f : 0.35f;
        g.setColour ((onScreen ? t.led : t.label()).withMultipliedAlpha (a)); g.setFont (uiFont (10.5f, true));
        g.drawText (label, getLocalBounds().removeFromTop (15), Justification::centred, false);
        auto vb = getLocalBounds().removeFromBottom (18).toFloat().reduced (2.0f, 0.0f);
        if (! onScreen) { g.setColour (t.screen().withMultipliedAlpha (a)); g.fillRoundedRectangle (vb, 2.0f); }
        t.ledText (g, overrideText.isNotEmpty() ? overrideText : (prm != nullptr ? withUnit (*prm) : String()), vb, Justification::centred, 14.0f, a);
    }
};

// one of the six long faders, with its label strip
class FaderBox : public Component
{
public:
    FaderBox()
    {
        fader.setSliderStyle (Slider::LinearVertical);
        fader.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        fader.setPopupDisplayEnabled (true, true, nullptr);
        fader.setMouseDragSensitivity (360);
        fader.setSliderSnapsToMousePosition (false);
        addAndMakeVisible (fader);
    }
    FineSlider fader; String label; RangedAudioParameter* prm = nullptr;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> att;
    void attach (AudioProcessorValueTreeState& st, const String& id, const String& lab)
    {
        att.reset(); label = lab; prm = st.getParameter (id);
        if (prm == nullptr) return;
        att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (st, id, fader);
        fader.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        repaint();
    }
    void resized() override { auto r = getLocalBounds(); r.removeFromBottom (30); fader.setBounds (r.reduced (0, 4)); }
    void paint (Graphics& g) override
    {
        auto strip = getLocalBounds().removeFromBottom (24).toFloat().withSizeKeepingCentre (jmin (96.0f, (float) getWidth() - 8.0f), 22.0f);
        const auto& t = themeOf (*this);
        t.fillRaised (g, strip, Colour (0xffd9d4c7), 2.0f, 0.6f);
        g.setColour (Colour (0xff1d1c1a)); g.setFont (uiFont (11.5f, true));
        g.drawText (label, strip, Justification::centred, false);
    }
};

// a choice parameter as a row of keys (radio)
class ChoiceKeys : public Component
{
public:
    ChoiceKeys (AudioProcessorValueTreeState& st, const String& id, const StringArray& names, LedKey::Style s = LedKey::Dark, bool ledAbove = true)
    {
        auto* p = st.getParameter (id);
        for (int i = 0; i < names.size(); ++i)
        {
            auto* k = keys.add (new LedKey (names[i], s, ledAbove));
            k->ledState = [this, i] { return current == i; };
            k->onClick = [this, i] { if (pa) pa->setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (k);
        }
        if (p != nullptr)
        {
            pa = std::make_unique<ParameterAttachment> (*p, [this] (float v) { current = roundToInt (v); for (auto* k : keys) k->repaint(); }, nullptr);
            pa->sendInitialUpdate();
        }
    }
    bool vertical = false; int gap = 6;
    void resized() override
    {
        auto r = getLocalBounds();
        const int n = jmax (1, keys.size());
        for (int i = 0; i < n; ++i)
        {
            if (vertical) { const int h = (r.getHeight() - gap * (n - 1)) / n; keys[i]->setBounds (r.getX(), r.getY() + i * (h + gap), r.getWidth(), h); }
            else { const int w = (r.getWidth() - gap * (n - 1)) / n; keys[i]->setBounds (r.getX() + i * (w + gap), r.getY(), w, r.getHeight()); }
        }
    }
    int current = 0;
private:
    OwnedArray<LedKey> keys;
    std::unique_ptr<ParameterAttachment> pa;
};

// a bool parameter on an LED key
class ToggleKey : public LedKey
{
public:
    ToggleKey (AudioProcessorValueTreeState& st, const String& id, const String& text, Style s = Dark, bool ledAbove = true) : LedKey (text, s, ledAbove)
    {
        setClickingTogglesState (true);
        att = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (st, id, *this);
    }
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> att;
};

// ============================================================================ virtual screen (page drawn on the LED display glass)
class VirtualScreen : public Component
{
public:
    String title, footer;
    std::function<void()> onClose;
    VirtualScreen()
    {
        close.setButtonText ("CLOSE"); close.showLed = false; close.style = LedKey::Cream;
        close.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (close);
    }
    int keepClearRight = 0;      // the soft keys sit over the top right corner
    void resized() override { close.setBounds (getWidth() - keepClearRight - 106, 12, 90, 30); }
    void paint (Graphics& g) override
    {
        const auto& t = themeOf (*this);
        auto r = getLocalBounds().toFloat();
        t.fillSunken (g, r, t.bezel(), 4.0f);
        auto in = r.reduced (9.0f);
        g.setColour (t.screen()); g.fillRoundedRectangle (in, 2.0f);
        g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.5f), in.getCentreX(), in.getCentreY(), Colours::transparentBlack, in.getX(), in.getY(), true));
        g.fillRoundedRectangle (in, 2.0f);
        t.ledText (g, title, in.withHeight (40.0f).withTrimmedLeft (16.0f), Justification::centredLeft, 24.0f);
        if (footer.isNotEmpty())
            t.ledText (g, footer, in.removeFromBottom (30.0f).withTrimmedLeft (16.0f).withTrimmedRight (16.0f), Justification::centredLeft, 13.0f, 0.75f);
        g.setColour (t.led.withAlpha (0.25f)); g.drawHorizontalLine ((int) in.getY() + 44, in.getX() + 12.0f, in.getRight() - 12.0f - (float) keepClearRight);
        for (auto& c : captions) t.ledText (g, c.second, c.first.toFloat(), Justification::centredLeft, 13.0f, 0.8f);
    }
    std::vector<std::pair<Rectangle<int>, String>> captions;
private:
    LedKey close;
};

// ============================================================================ EQ curve of the edited slot
class EqGraph : public Component
{
public:
    explicit EqGraph (AudioProcessorValueTreeState& s) : st (s) {}
    int slot = 0;
    void paint (Graphics& g) override
    {
        const auto& t = themeOf (*this);
        auto r = getLocalBounds().toFloat();
        t.fillSunken (g, r, t.bezel(), 3.0f);
        auto in = r.reduced (7.0f);
        g.setColour (t.screen()); g.fillRect (in);
        const double fs = 48000.0;
        pv::Biquad b[4];
        auto raw = [this] (const String& id) { auto* v = st.getRawParameterValue (id); return v ? v->load() : 0.0f; };
        float f[4], gn[4];
        for (int i = 0; i < 4; ++i) { f[i] = raw (sid (slot, nid ("eqF", i))); gn[i] = raw (sid (slot, nid ("eqG", i))); }
        b[0].lowShelf ((float) fs, f[0], gn[0]); b[1].peak ((float) fs, f[1], 0.9f, gn[1]); b[2].peak ((float) fs, f[2], 0.9f, gn[2]); b[3].highShelf ((float) fs, f[3], gn[3]);
        auto xOf = [&] (double hz) { return in.getX() + (float) (std::log (hz / 20.0) / std::log (1000.0)) * in.getWidth(); };
        auto yOf = [&] (double db) { return in.getCentreY() - (float) (db / 18.0) * in.getHeight() * 0.5f; };
        for (double hz : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
        { g.setColour (Colours::white.withAlpha (0.06f)); g.drawVerticalLine ((int) xOf (hz), in.getY(), in.getBottom()); }
        g.setColour (Colours::white.withAlpha (0.15f)); g.drawHorizontalLine ((int) yOf (0), in.getX(), in.getRight());
        Path p; const int N = (int) in.getWidth() * 2;
        for (int i = 0; i <= N; ++i)
        {
            const double hz = 20.0 * std::pow (1000.0, (double) i / N);
            const std::complex<double> z = std::polar (1.0, -2.0 * MathConstants<double>::pi * hz / fs);
            double db = 0;
            for (auto& q : b)
            {
                const auto num = (double) q.b0 + (double) q.b1 * z + (double) q.b2 * z * z, den = 1.0 + (double) q.a1 * z + (double) q.a2 * z * z;
                db += 20.0 * std::log10 (std::abs (num / den) + 1e-12);
            }
            const float y = jlimit (in.getY(), in.getBottom(), yOf (db));
            if (i == 0) p.startNewSubPath (xOf (hz), y); else p.lineTo (xOf (hz), y);
        }
        Path fill (p); fill.lineTo (in.getRight(), yOf (0)); fill.lineTo (in.getX(), yOf (0)); fill.closeSubPath();
        g.setColour (t.led.withAlpha (0.13f)); g.fillPath (fill);
        g.setColour (t.led); g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }
private:
    AudioProcessorValueTreeState& st;
};

// ============================================================================ main panel (fixed design size, scaled by the editor)
class MainPanel : public Component, private Timer
{
public:
    static constexpr int H = 820, WFull = 1440, WShort = 1124;
    enum { kDefaultId = 1, kBuiltinBase = 2, kUserBase = 2000, kSaveId = 9000, kDeleteId = 9001, kFolderId = 9002 };
    enum Page { PageNone = 0, PageAdvanced, PageIr, PageHardware, PageOutputs };
    std::function<void()> onSizeChanged;
    std::function<void (float)> onScaleChosen;
    void showScale (float sc)
    {
        static const float v[] = { 0.75f, 0.9f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };
        int best = 0; for (int i = 0; i < 7; ++i) if (std::abs (v[i] - sc) < std::abs (v[best] - sc)) best = i;
        sizeBox.setSelectedItemIndex (best, dontSendNotification);
    }

    explicit MainPanel (PlateVerbProcessor& p) : proc (p), eqGraph (p.apvts)
    {
        auto& st = proc.apvts;
        loadLook();
        editSlot = jlimit (0, pv::kNumSlots - 1, (int) st.state.getProperty ("editSlot", 0));
        sideOpen = (bool) st.state.getProperty ("sideOpen", true);
        sideTab = jlimit (0, 2, (int) st.state.getProperty ("sideTab", 0));

        // ---- rail
        presetBox.onChange = [this] { presetChosen(); };
        presetBox.setTextWhenNothingSelected ("Default");
        addAndMakeVisible (presetBox);
        for (auto* b : { &presetPrev, &presetNext, &presetSave, &lookBtn }) { b->style = LedKey::Cream; addAndMakeVisible (*b); }
        presetPrev.setButtonText ("<"); presetNext.setButtonText (">"); presetSave.setButtonText ("SAVE"); lookBtn.setButtonText ("LOOK");
        presetPrev.showLed = presetNext.showLed = presetSave.showLed = false;
        presetPrev.onClick = [this] { stepPreset (-1); };
        presetNext.onClick = [this] { stepPreset (1); };
        presetSave.onClick = [this] { savePreset(); };
        lookBtn.ledState = [this] { return lookPanel.isVisible(); };
        lookBtn.onClick = [this] { lookPanel.setVisible (! lookPanel.isVisible()); if (lookPanel.isVisible()) lookPanel.toFront (false); lookBtn.repaint(); };
        analogKey = std::make_unique<ToggleKey> (st, "hwOn", "ANALOG", LedKey::Cream);
        addAndMakeVisible (*analogKey);

        // ---- algorithms: the key switches the algorithm on / off and opens it for editing
        const int order[] = { pv::Plate, pv::Room, pv::Chamber, pv::Hall, pv::Lex480, pv::IR };
        for (int i = 0; i < pv::kNumSlots; ++i)
        {
            const int s = order[i];
            auto* k = algoKeys.add (new ToggleKey (st, sid (s, "on"), kSlotNames[s], LedKey::Dark));
            k->onClick = [this, s] { selectSlot (s); };
            addAndMakeVisible (k);
            algoOrder[i] = s;
        }

        // ---- soft keys
        const char* softNames[] = { "PAGE 1/2", "ADVANCED", "HARDWARE", "OUTPUTS" };
        for (int i = 0; i < 4; ++i)
        {
            auto* k = softKeys.add (new LedKey (softNames[i], LedKey::Cream, false));
            addAndMakeVisible (k);
        }
        softKeys[0]->ledState = [this] { return page2; };
        softKeys[0]->onClick = [this] { page2 = ! page2; bindFaders(); repaint(); };
        softKeys[1]->ledState = [this] { return screenPage == PageAdvanced || screenPage == PageIr; };
        softKeys[1]->onClick = [this] { showPage (editSlot == pv::IR ? PageIr : PageAdvanced); };
        softKeys[2]->ledState = [this] { return screenPage == PageHardware; };
        softKeys[2]->onClick = [this] { showPage (PageHardware); };
        softKeys[3]->ledState = [this] { return screenPage == PageOutputs; };
        softKeys[3]->onClick = [this] { showPage (PageOutputs); };

        // ---- faders
        for (int i = 0; i < 6; ++i) addAndMakeVisible (faders.add (new FaderBox()));

        // ---- bottom row
        outType = std::make_unique<ChoiceKeys> (st, "hwOut", StringArray { "ANALOG", "DIGITAL" }, LedKey::Dark, false);
        addAndMakeVisible (*outType);
        outKnob = addKnob (*this, "out", "OUTPUT", false);

        // ---- side panel: tabs, EQ, compressor, delay
        const char* tabNames[] = { "EQ", "COMP", "DELAY" };
        for (int i = 0; i < 3; ++i)
        {
            auto* k = tabKeys.add (new LedKey (tabNames[i], LedKey::Dark));
            k->ledState = [this, i] { return sideTab == i; };
            k->onClick = [this, i] { sideTab = i; proc.apvts.state.setProperty ("sideTab", i, nullptr); updateSide(); };
            addAndMakeVisible (k);
            auto* v = vTabs.add (new LedKey (tabNames[i], LedKey::Dark, true)); v->verticalText = true;
            v->ledState = [this, i] { return sideTab == i; };
            v->onClick = [this, i] { sideTab = i; proc.apvts.state.setProperty ("sideTab", i, nullptr); setSideOpen (true); };
            addChildComponent (v);
        }
        foldBtn.style = LedKey::Dark; foldBtn.showLed = false;
        foldBtn.onClick = [this] { setSideOpen (! sideOpen); };
        addAndMakeVisible (foldBtn);
        addAndMakeVisible (eqGraph);
        const char* bn[] = { "LOW", "LO-MID", "HI-MID", "HIGH" };
        for (int b = 0; b < 4; ++b) { eqF[b] = addKnob (*this, nid ("eqF", b), String (bn[b]) + " HZ", true); eqG[b] = addKnob (*this, nid ("eqG", b), String (bn[b]) + " dB", true); }
        compOn = std::make_unique<LedKey> ("COMP", LedKey::Dark); compOn->setClickingTogglesState (true); addAndMakeVisible (*compOn);
        compDuck = std::make_unique<LedKey> ("DUCK", LedKey::Cream); compDuck->setClickingTogglesState (true); addAndMakeVisible (*compDuck);
        compType.addItemList (compTypeNames(), 1); addAndMakeVisible (compType);
        compTone.addItemList (StringArray { "Clean", "Vintage" }, 1); addAndMakeVisible (compTone);
        const char* cpn[][2] = { { "cpThr", "THRESH" }, { "cpRatio", "RATIO" }, { "cpAtk", "ATTACK" }, { "cpRel", "RELEASE" }, { "cpMakeup", "MAKEUP" }, { "cpMix", "MIX" } };
        for (int i = 0; i < 6; ++i) compK[i] = addKnob (*this, cpn[i][0], cpn[i][1], true);
        dlOn = std::make_unique<ToggleKey> (st, "dlOn", "DELAY", LedKey::Dark); addAndMakeVisible (*dlOn);
        dlSync = std::make_unique<ToggleKey> (st, "dlSync", "SYNC", LedKey::Cream); addAndMakeVisible (*dlSync);
        dlPing = std::make_unique<ToggleKey> (st, "dlPing", "PING", LedKey::Cream); addAndMakeVisible (*dlPing);
        const char* dln[][2] = { { "dlTimeL", "TIME L" }, { "dlTimeR", "TIME R" }, { "dlFb", "FEEDBACK" }, { "dlMix", "MIX" },
                                 { "dlAtk", "ATTACK" }, { "dlPanL", "PAN L" }, { "dlPanR", "PAN R" }, { "dlWidth", "WIDTH" },
                                 { "dlLow", "LOW" }, { "dlMid", "MID" }, { "dlMidF", "MID HZ" }, { "dlHigh", "HIGH" } };
        for (int i = 0; i < 12; ++i) dlK[i] = addKnob (*this, dln[i][0], dln[i][1], false);
        dlDivL.addItemList (divNames(), 1); dlDivR.addItemList (divNames(), 1);
        addAndMakeVisible (dlDivL); addAndMakeVisible (dlDivR);
        dlDivLAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, "dlDivL", dlDivL);
        dlDivRAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, "dlDivR", dlDivR);

        // ---- virtual screens
        buildAdvanced(); buildIr(); buildHardware(); buildOutputs();
        for (auto* v : { &scrAdvanced, &scrIr, &scrHardware, &scrOutputs }) { v->onClose = [this] { showPage (PageNone); }; addChildComponent (*v); }

        // ---- LOOK popover
        buildLook();
        addChildComponent (lookPanel);

        for (auto* prm : proc.getParameters()) lastValues.add (prm->getValue());
        buildPresetMenu();
        bindSlot (editSlot);
        setSize (designWidth(), H);
        updateSide();
        startTimerHz (25);
    }
    ~MainPanel() override { stopTimer(); }
    int designWidth() const { return sideOpen ? WFull : WShort; }
    FlexLook& getLook() { return look; }

    // ---------------------------------------------------------------- painting
    void paint (Graphics& g) override
    {
        const auto& t = look.theme;
        auto all = getLocalBounds().toFloat();
        g.setGradientFill (ColourGradient (Theme::shade (t.chassis(), 0.14f * t.g()), 0, 0, Theme::shade (t.chassis(), -0.18f * t.g()), 0, all.getBottom(), false));
        g.fillAll();
        // rail
        t.fillRaised (g, railArea.toFloat(), t.rail(), 4.0f, 1.2f);
        t.drawScrew (g, { (float) railArea.getX() + 16.0f, (float) railArea.getCentreY() });
        t.drawScrew (g, { (float) railArea.getRight() - 16.0f, (float) railArea.getCentreY() });
        g.setColour (t.title()); g.setFont (uiFont (19.0f, true));
        g.drawText ("F L E X I V E R B", railArea.getX() + 34, railArea.getY(), 230, railArea.getHeight(), Justification::centredLeft);
        g.setColour (t.dim()); g.setFont (uiFont (10.5f, true));
        if (sideOpen) g.drawText ("DIGITAL EFFECTS SYSTEM", railArea.getX() + 250, railArea.getY(), 200, railArea.getHeight(), Justification::centredLeft);
        g.setColour (shownDsp > 70.0f ? Colour (0xffff8a65) : t.dim()); g.setFont (uiFont (10.5f, true));
        g.drawText ("DSP " + String (shownDsp, 1) + " %", dspArea, Justification::centredLeft);
        // control surface
        t.fillRaised (g, surfArea.toFloat(), t.panel, 4.0f);
        g.setColour (t.label()); g.setFont (uiFont (10.5f, true));
        g.drawText ("ALGORITHM", surfArea.getX() + 18, algoKeys[0]->getY() + 13, 100, algoKeys[0]->getHeight() - 13, Justification::centredLeft);
        // display
        paintDisplay (g);
        // fader bay
        t.fillSunken (g, bayArea.toFloat(), t.bay(), 4.0f);
        // bottom row: meters
        paintMeters (g);
        g.setColour (t.label()); g.setFont (uiFont (10.5f, true));
        g.drawText ("OUTPUT", outType->getX() - 64, outType->getY(), 58, outType->getHeight(), Justification::centredRight);
        // side panel
        if (sideOpen)
        {
            t.fillRaised (g, sideArea.toFloat(), t.side(), 4.0f);
            if (sideTab == 1) paintCompMeters (g);
            if (sideTab == 2)
            {
                g.setColour (t.label()); g.setFont (uiFont (10.5f, true));
                g.drawText ("ATTACK / PAN / WIDTH", sideArea.getX() + 16, dlK[5]->getY() - 20, 200, 14, Justification::centredLeft);
                g.drawText ("ANALOG EQ", sideArea.getX() + 16, dlK[8]->getY() - 20, 160, 14, Justification::centredLeft);
                g.setColour (Colours::black.withAlpha (0.35f));
                g.drawHorizontalLine (dlK[5]->getY() - 26, (float) sideArea.getX() + 12.0f, (float) sideArea.getRight() - 12.0f);
                g.drawHorizontalLine (dlK[8]->getY() - 26, (float) sideArea.getX() + 12.0f, (float) sideArea.getRight() - 12.0f);
            }
        }
        else t.fillRaised (g, sideArea.toFloat(), t.side(), 4.0f);
    }

    void paintDisplay (Graphics& g)
    {
        const auto& t = look.theme;
        t.fillSunken (g, dispArea.toFloat(), t.bezel(), 4.0f);
        auto in = dispArea.toFloat().reduced (9.0f);
        g.setColour (t.screen()); g.fillRoundedRectangle (in, 2.0f);
        g.setGradientFill (ColourGradient (Colours::transparentBlack, in.getCentreX(), in.getCentreY(), Colours::black.withAlpha (0.45f), in.getX(), in.getY(), true));
        g.fillRoundedRectangle (in, 2.0f);
        if (screenPage != PageNone) return;                     // a virtual screen covers it
        auto txt = in.reduced (14.0f, 8.0f);
        auto graph = txt.removeFromRight (jmin (430.0f, txt.getWidth() * 0.48f));
        txt.removeFromRight (16.0f);
        // header line
        const int cfg = (int) raw ("config");
        const int mA = (int) raw ("machA"), mB = (int) raw ("machB");
        String mach = cfg == pv::CfgSingle ? String ("SINGLE") : (editSlot == mA ? String ("MACH A") : editSlot == mB ? String ("MACH B") : String ("NOT IN USE"));
        t.ledText (g, mach + "  " + String (kSlotNames[editSlot]) + "  PG " + String (page2 ? 2 : 1) + "/2", txt.removeFromTop (22.0f), Justification::centredLeft, 17.0f, 0.8f);
        // name
        t.ledText (g, displayName(), txt.removeFromTop (46.0f), Justification::centredLeft, 38.0f);
        // the six fader values
        txt.removeFromTop (6.0f);
        const float cw = txt.getWidth() / 3.0f, rh = jmin (24.0f, txt.getHeight() / 2.0f);
        for (int i = 0; i < 6; ++i)
        {
            Rectangle<float> c (txt.getX() + (float) (i % 3) * cw, txt.getY() + (float) (i / 3) * rh, cw - 12.0f, rh);
            t.ledText (g, faders[i]->label, c, Justification::centredLeft, 16.0f, 0.65f);
            t.ledText (g, faders[i]->prm != nullptr ? withUnit (*faders[i]->prm) : String(), c, Justification::centredRight, 16.0f);
        }
        paintDecay (g, graph);
    }

    // three-band decay (low / mid / high) of the edited slot: smooth curves with a soft fill, like the 1081's EQ graph
    void paintDecay (Graphics& g, Rectangle<float> r)
    {
        const auto& t = look.theme;
        const Colour bc[3] = { Theme::bandLo(), Theme::bandMid(), Theme::bandHi() };
        const char* nm[] = { "LOW", "MID", "HIGH" };
        auto legend = r.removeFromTop (16.0f);
        float rt[3] = { 0, 0, 0 };
        if (editSlot == pv::IR) { rt[1] = wave.rt; rt[0] = wave.rt * raw (sid (editSlot, "lowMult")); rt[2] = wave.rt * raw (sid (editSlot, "highMult")); }
        else { const float d = raw (sid (editSlot, "decay")); rt[0] = d * raw (sid (editSlot, "lowMult")); rt[1] = d; rt[2] = d * raw (sid (editSlot, "highMult")); }
        g.setFont (uiFont (10.5f, true));
        for (int i = 0; i < 3; ++i)
        {
            auto c = legend.removeFromLeft (legend.getWidth() / (float) (3 - i));
            g.setColour (bc[i]); g.fillEllipse (c.getX(), c.getCentreY() - 3.5f, 7.0f, 7.0f);
            g.drawText (String (nm[i]) + " " + (rt[i] > 0.0f ? String (rt[i], 2) + " s" : String ("--")), c.withTrimmedLeft (11.0f), Justification::centredLeft, false);
        }
        auto axis = r.removeFromBottom (13.0f);
        auto plot = r.reduced (0.0f, 3.0f);
        g.setColour (Colours::white.withAlpha (0.05f));
        for (int k = 1; k < 4; ++k) g.drawHorizontalLine ((int) (plot.getY() + plot.getHeight() * (float) k / 4.0f), plot.getX(), plot.getRight());
        if (! wave.valid || wave.lo.empty())
        {
            t.ledText (g, editSlot == pv::IR ? "NO IR LOADED" : "...", plot, Justification::centred, 14.0f, 0.6f);
            return;
        }
        const float total = (float) wave.lo.size() * wave.binMs;
        const float floorDb = -80.0f;
        const float step = total > 6000 ? 2000.0f : total > 2500 ? 1000.0f : 500.0f;
        g.setFont (uiFont (9.5f));
        for (float ms = 0; ms <= total; ms += step)
        {
            const float x = plot.getX() + plot.getWidth() * ms / total;
            g.setColour (Colours::white.withAlpha (0.05f)); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (Colours::white.withAlpha (0.35f));
            g.drawText (ms >= 1000 ? String (ms / 1000.0f, 0) + " s" : String ((int) ms), (int) x - 20, (int) axis.getY(), 40, 12, Justification::centred);
        }
        const std::vector<float>* bv[3] = { &wave.lo, &wave.mid, &wave.hi };
        const int N = (int) plot.getWidth();
        for (int b = 0; b < 3; ++b)
        {
            // peak per column, then a short moving average in dB: smooth like an analyser, not jagged
            std::vector<float> y ((size_t) N + 1);
            const auto& v = *bv[b];
            for (int k = 0; k <= N; ++k)
            {
                const float m0 = total * (float) k / (float) N, m1 = total * (float) (k + 1) / (float) N;
                int a = jlimit (0, (int) v.size() - 1, (int) (m0 / wave.binMs)), e = jlimit (a, (int) v.size() - 1, (int) (m1 / wave.binMs));
                float pk = 0; for (int i = a; i <= e; ++i) pk = jmax (pk, v[(size_t) i]);
                y[(size_t) k] = jmax (floorDb, 20.0f * std::log10 (pk + 1e-9f));
            }
            std::vector<float> s (y.size());
            for (int k = 0; k <= N; ++k) { float acc = 0; int c = 0; for (int d = -3; d <= 3; ++d) { const int q = k + d; if (q >= 0 && q <= N) { acc += y[(size_t) q]; ++c; } } s[(size_t) k] = acc / (float) c; }
            Path p;
            for (int k = 0; k <= N; ++k)
            {
                const float px = plot.getX() + (float) k, py = plot.getY() + plot.getHeight() * s[(size_t) k] / floorDb;
                if (k == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
            }
            Path f (p); f.lineTo (plot.getRight(), plot.getBottom()); f.lineTo (plot.getX(), plot.getBottom()); f.closeSubPath();
            g.setColour (bc[b].withAlpha (0.12f)); g.fillPath (f);
            g.setColour (bc[b]); g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
        }
    }

    void paintMeters (Graphics& g)
    {
        const float lv[3] = { proc.meterIn.load(), proc.meterMain.load(), proc.meterAux.load() };
        const char* nm[] = { "IN", "MAIN", "AUX" };
        const auto& t = look.theme;
        for (int m = 0; m < 3; ++m)
        {
            auto r = meterArea[m].toFloat();
            g.setColour (t.label()); g.setFont (uiFont (10.5f, true));
            g.drawText (nm[m], r.removeFromLeft (40.0f), Justification::centredLeft);
            r = r.withSizeKeepingCentre (r.getWidth(), 14.0f);
            g.setColour (Colour (0xff0b0b0b)); g.fillRoundedRectangle (r, 2.0f);
            const float db = 20.0f * std::log10 (lv[m] + 1e-9f);
            const int segs = 14; const float sw = (r.getWidth() - 4.0f) / (float) segs;
            for (int i = 0; i < segs; ++i)
            {
                const float thr = -42.0f + 3.0f * (float) i;                 // -42 ... -3 dBFS, the last one = clip
                const Colour c = i >= segs - 1 ? Colour (0xffff3b1f) : i >= segs - 4 ? Colour (0xffffb000) : Colour (0xff5fd068);
                auto sr = Rectangle<float> (r.getX() + 2.0f + (float) i * sw, r.getY() + 2.5f, sw - 2.0f, r.getHeight() - 5.0f);
                g.setColour (db >= thr ? c : Colour (0xff1d1d1d)); g.fillRoundedRectangle (sr, 1.0f);
            }
        }
    }

    void paintCompMeters (Graphics& g)
    {
        const auto& t = look.theme;
        const float lo = -80.0f;
        const float inDb = jlimit (lo, 0.0f, proc.compIn[editSlot].load());
        const float grDb = jlimit (0.0f, 30.0f, proc.compGr[editSlot].load());
        const float thr  = raw (sid (editSlot, "cpThr"));
        auto in = inMeter.toFloat(), gr = grMeter.toFloat();
        g.setColour (t.screen()); g.fillRoundedRectangle (in, 2.0f); g.fillRoundedRectangle (gr, 2.0f);
        const float inH = in.getHeight() * (inDb - lo) / -lo;
        g.setColour (t.led.withAlpha (0.85f)); g.fillRect (in.getX() + 3.0f, in.getBottom() - inH, in.getWidth() - 6.0f, inH);
        const float ty = in.getBottom() - in.getHeight() * (jlimit (lo, 0.0f, thr) - lo) / -lo;
        g.setColour (Colours::white); g.fillRect (in.getX() - 4.0f, ty - 1.0f, in.getWidth() + 8.0f, 2.0f);
        const float grH = gr.getHeight() * grDb / 30.0f;
        g.setColour (Theme::bandHi()); g.fillRect (gr.getX() + 3.0f, gr.getY(), gr.getWidth() - 6.0f, grH);
        g.setColour (t.label()); g.setFont (uiFont (10.5f, true));
        g.drawText (raw (sid (editSlot, "cpDuck")) > 0.5f ? "DRY" : "IN", inMeter.getX() - 14, inMeter.getBottom() + 3, inMeter.getWidth() + 28, 14, Justification::centred);
        g.drawText ("GR", grMeter.getX() - 14, grMeter.getBottom() + 3, grMeter.getWidth() + 28, 14, Justification::centred);
        t.ledText (g, inDb <= lo + 0.1f ? String ("-inf") : String (inDb, 0), Rectangle<int> (inMeter.getX() - 16, inMeter.getY() - 18, inMeter.getWidth() + 32, 15).toFloat(), Justification::centred, 12.0f);
        t.ledText (g, grDb < 0.05f ? String ("0") : String (-grDb, 1), Rectangle<int> (grMeter.getX() - 16, grMeter.getY() - 18, grMeter.getWidth() + 32, 15).toFloat(), Justification::centred, 12.0f);
    }

    // ---------------------------------------------------------------- layout
    void resized() override
    {
        const int W = getWidth();
        railArea = { 14, 14, W - 28, 46 };
        surfArea = { 14, 72, 1048, 734 };
        sideArea = sideOpen ? Rectangle<int> (1074, 72, 352, 734) : Rectangle<int> (1074, 72, 36, 734);
        // rail (right to left)
        int x = railArea.getRight() - 34;
        lookBtn.setBounds (x - 74, 19, 74, 40); x -= 84;
        analogKey->setBounds (x - 86, 19, 86, 40); x -= 100;
        presetSave.setBounds (x - 64, 24, 64, 30); x -= 70;
        presetNext.setBounds (x - 32, 24, 32, 30); x -= 36;
        presetPrev.setBounds (x - 32, 24, 32, 30); x -= 40;
        presetBox.setBounds (x - 300, 24, 300, 30);
        dspArea = { presetBox.getX() - 110, 24, 100, 30 };
        // algorithm keys
        for (int i = 0; i < algoKeys.size(); ++i) algoKeys[i]->setBounds (surfArea.getX() + 118 + i * 100, 84, 92, 48);
        // display + soft keys
        dispArea = { surfArea.getX() + 18, 142, surfArea.getWidth() - 36 - 142, 192 };
        for (int i = 0; i < softKeys.size(); ++i) softKeys[i]->setBounds (dispArea.getRight() + 14, dispArea.getY() + 2 + i * 48, 128, 40);
        // fader bay
        bayArea = { surfArea.getX() + 18, 348, surfArea.getWidth() - 36, 400 };
        const int fw = bayArea.getWidth() / 6;
        for (int i = 0; i < 6; ++i) faders[i]->setBounds (bayArea.getX() + i * fw + 4, bayArea.getY() + 14, fw - 8, bayArea.getHeight() - 22);
        // bottom row
        const int by = surfArea.getBottom() - 46;
        for (int m = 0; m < 3; ++m) meterArea[m] = { surfArea.getX() + 18 + m * 210, by, 196, 34 };
        outType->setBounds (surfArea.getX() + 720, by + 2, 200, 30);
        outKnob->setBounds (surfArea.getRight() - 110, by - 26, 90, 64);
        outKnob->setVisible (false);    // output level lives on the OUTPUTS screen (keeps the row tidy)
        // virtual screens cover the display and the fader bay, not the soft keys
        const Rectangle<int> scr (dispArea.getX(), dispArea.getY(), bayArea.getWidth(), bayArea.getBottom() - dispArea.getY());
        for (auto* v : { &scrAdvanced, &scrIr, &scrHardware, &scrOutputs }) { v->keepClearRight = bayArea.getRight() - dispArea.getRight(); v->setBounds (scr); v->resized(); }
        layoutAdvanced(); layoutIr(); layoutHardware(); layoutOutputs();
        // side panel
        layoutSide();
        lookPanel.setBounds (W - 350, 64, 330, 370);
        layoutLook();
    }

    void layoutSide()
    {
        const int sx = sideArea.getX();
        foldBtn.setButtonText (sideOpen ? ">>" : "<<");
        if (! sideOpen)
        {
            foldBtn.setBounds (sx + 3, sideArea.getY() + 8, 30, 30);
            for (int i = 0; i < 3; ++i) vTabs[i]->setBounds (sx + 3, sideArea.getY() + 50 + i * 90, 30, 80);
            return;
        }
        for (int i = 0; i < 3; ++i) tabKeys[i]->setBounds (sx + 12 + i * 98, sideArea.getY() + 10, 92, 42);
        foldBtn.setBounds (sideArea.getRight() - 42, sideArea.getY() + 22, 32, 30);
        const int y0 = sideArea.getY() + 66;
        // EQ
        eqGraph.setBounds (sx + 12, y0, 328, 150);
        for (int b = 0; b < 4; ++b)
        {
            eqF[b]->setBounds (sx + 12 + b * 82, y0 + 166, 82, 96);
            eqG[b]->setBounds (sx + 12 + b * 82, y0 + 276, 82, 96);
        }
        // compressor
        compOn->setBounds (sx + 12, y0, 92, 46);
        compDuck->setBounds (sx + 112, y0, 92, 46);
        compType.setBounds (sx + 12, y0 + 64, 200, 28);
        compTone.setBounds (sx + 220, y0 + 64, 120, 28);
        for (int i = 0; i < 6; ++i) compK[i]->setBounds (sx + 12 + (i % 3) * 82, y0 + 110 + (i / 3) * 110, 82, 100);
        inMeter = { sx + 274, y0 + 128, 16, 200 };
        grMeter = { sx + 314, y0 + 128, 16, 200 };
        // delay
        dlOn->setBounds (sx + 12, y0, 100, 46); dlSync->setBounds (sx + 120, y0, 90, 46); dlPing->setBounds (sx + 218, y0, 90, 46);
        for (int i = 0; i < 4; ++i) dlK[i]->setBounds (sx + 12 + i * 82, y0 + 60, 82, 96);
        dlDivL.setBounds (sx + 16, y0 + 92, 74, 28); dlDivR.setBounds (sx + 98, y0 + 92, 74, 28);
        for (int i = 0; i < 4; ++i) dlK[4 + i]->setBounds (sx + 12 + i * 82, y0 + 200, 82, 96);
        for (int i = 0; i < 4; ++i) dlK[8 + i]->setBounds (sx + 12 + i * 82, y0 + 336, 82, 96);
    }
    void updateSide()
    {
        const bool open = sideOpen;
        for (auto* k : tabKeys) k->setVisible (open);
        for (auto* k : vTabs) k->setVisible (! open);
        const bool eq = open && sideTab == 0, cp = open && sideTab == 1, dl = open && sideTab == 2;
        eqGraph.setVisible (eq);
        for (int b = 0; b < 4; ++b) { eqF[b]->setVisible (eq); eqG[b]->setVisible (eq); }
        compOn->setVisible (cp); compDuck->setVisible (cp); compType.setVisible (cp); compTone.setVisible (cp);
        for (auto* k : compK) k->setVisible (cp);
        dlOn->setVisible (dl); dlSync->setVisible (dl); dlPing->setVisible (dl);
        const bool sync = raw ("dlSync") > 0.5f;
        for (int i = 0; i < 12; ++i) dlK[i]->setVisible (dl && ! (sync && i < 2));
        dlDivL.setVisible (dl && sync); dlDivR.setVisible (dl && sync);
        for (auto* k : tabKeys) k->repaint();
        repaint();
    }
    void setSideOpen (bool o)
    {
        sideOpen = o;
        proc.apvts.state.setProperty ("sideOpen", o, nullptr);
        setSize (designWidth(), H);
        resized(); updateSide();
        if (onSizeChanged) onSizeChanged();
    }

   #if FLEX_TEST
    void debugShow (int scr, int tab, int slot, bool lookOpen, bool open, int lookPreset)
    {
        selectSlot (slot); sideTab = tab; setSideOpen (open);
        if (scr > 0) showPage ((Page) scr);
        lookPanel.setVisible (lookOpen);
        if (lookPreset == 1) { look.theme.panel = kPanelChoices[2]; look.theme.led = kLedChoices[1]; look.theme.grad = 0.9f; look.theme.light = 0.8f; lookChanged(); }
        if (lookPreset == 2) { look.theme.panel = kPanelChoices[3]; look.theme.led = kLedChoices[3]; look.theme.flat = true; lookChanged(); }
        if (lookPreset == 3) { look.theme.panel = kPanelChoices[1]; look.theme.led = kLedChoices[2]; look.theme.contrast = 1.0f; look.theme.light = 1.0f; lookChanged(); }
        wave = proc.getWave (editSlot);
    }
    void debugWave() { wave = proc.getWave (editSlot); repaint(); }
   #endif
private:
    // ---------------------------------------------------------------- helpers
    float raw (const String& id) const { auto* v = proc.apvts.getRawParameterValue (id); return v != nullptr ? v->load() : 0.0f; }
    struct PerSlot { KnobBox* box; String base; };
    std::vector<PerSlot> perSlotKnobs;
    KnobBox* addKnob (Component& parent, const String& id, const String& label, bool perSlot)
    {
        auto* k = knobs.add (new KnobBox (label));
        parent.addAndMakeVisible (k);
        if (perSlot) perSlotKnobs.push_back ({ k, id }); else k->attach (proc.apvts, id);
        return k;
    }
    KnobBox* addScreenKnob (VirtualScreen& v, const String& id, const String& label, bool perSlot)
    {
        auto* k = addKnob (v, id, label, perSlot); k->onScreen = true; return k;
    }

    String displayName()
    {
        if (editSlot == pv::IR)
        {
            if (proc.irLayout() == pv::IrNone) return "IR - NO FILE";
            return proc.irInfo().upToLastOccurrenceOf (".", false, false).toUpperCase().substring (0, 22);
        }
        if (proc.userPresetName.isNotEmpty()) return proc.userPresetName.toUpperCase().substring (0, 22);
        const int cp = proc.getCurrentProgram();
        if (cp > 0 && kPresets[cp - 1].slot == editSlot) return String (kPresets[cp - 1].name).toUpperCase().substring (0, 22);
        const char* names[] = { "PLATE", "ROOM", "CHAMBER", "HALL", "IMPULSE RESPONSE", "RANDOM HALL" };
        return names[editSlot];
    }

    // fader pages for the edited slot
    void bindFaders()
    {
        struct F { const char* id; const char* lab; bool global; };
        const bool is480 = editSlot == pv::Lex480, isIr = editSlot == pv::IR;
        std::vector<F> f;
        if (isIr)
            f = page2 ? std::vector<F> { { "lowMult", "LOW DEC", false }, { "highMult", "HIGH DEC", false }, { "size", "SIZE", false },
                                         { "build", "FADE IN", false }, { "damping", "DAMPING", false }, { "tone", "TONE", false } }
                      : std::vector<F> { { "decay", "DECAY", false }, { "predelay", "PRE-DLY", false }, { "early", "EARLY", false },
                                         { "inWidth", "WIDTH", false }, { "lv", "LEVEL", false }, { "mix", "MIX", true } };
        else
            f = page2 ? std::vector<F> { { "diff", "DIFFUSE", false }, { "early", "EARLY", false }, { "build", is480 ? "SPREAD" : "BUILD", false },
                                         { "mod", is480 ? "WANDER" : "MOD", false }, { is480 ? "spin" : "damping", is480 ? "SPIN" : "DAMPING", false }, { "lv", "LEVEL", false } }
                      : std::vector<F> { { "decay", "DECAY", false }, { "size", "SIZE", false }, { "predelay", "PRE-DLY", false },
                                         { "lowMult", "BASS X", false }, { "highMult", "TREBLE X", false }, { "mix", "MIX", true } };
        for (int i = 0; i < 6; ++i) faders[i]->attach (proc.apvts, f[(size_t) i].global ? String (f[(size_t) i].id) : sid (editSlot, f[(size_t) i].id), f[(size_t) i].lab);
        repaint();
    }

    void bindSlot (int s)
    {
        editSlot = s;
        for (auto& k : perSlotKnobs) { k.box->attach (proc.apvts, sid (s, k.base)); k.box->repaint(); }
        auto bindCombo = [this, s] (ComboBox& b, std::unique_ptr<APVTS::ComboBoxAttachment>& a, const String& base)
        {
            a.reset();
            if (base == "matA" || base == "matB") { b.clear (dontSendNotification); b.addItemList (materialNames (s), 1); }
            a = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, sid (s, base), b);
        };
        bindCombo (compType, compTypeAtt, "cpType"); bindCombo (compTone, compToneAtt, "cpTone");
        bindCombo (matA, matAAtt, "matA"); bindCombo (matB, matBAtt, "matB"); bindCombo (conv, convAtt, "conv");
        bindCombo (irMatA, irMatAAtt, "matA"); bindCombo (irMatB, irMatBAtt, "matB");
        compOnAtt.reset(); compOnAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, sid (s, "cpOn"), *compOn);
        compDuckAtt.reset(); compDuckAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, sid (s, "cpDuck"), *compDuck);
        matMixAtt.reset(); matMixAtt = std::make_unique<APVTS::SliderAttachment> (proc.apvts, sid (s, "matMix"), matMix);
        irMatMixAtt.reset(); irMatMixAtt = std::make_unique<APVTS::SliderAttachment> (proc.apvts, sid (s, "matMix"), irMatMix);
        eqGraph.slot = s; eqGraph.repaint();
        proc.viewSlot.store (s);
        proc.apvts.state.setProperty ("editSlot", s, nullptr);
        lastWaveVersion = -1;
        if (screenPage == PageAdvanced && s == pv::IR) showPage (PageIr);
        else if (screenPage == PageIr && s != pv::IR) showPage (PageAdvanced);
        spinK->setEnabled (s == pv::Lex480);
        bindFaders();
        for (auto* k : algoKeys) k->repaint();
        repaint();
    }
    void selectSlot (int s) { if (s != editSlot) bindSlot (s); }

    void showPage (Page pg)
    {
        screenPage = (screenPage == pg) ? PageNone : pg;
        scrAdvanced.setVisible (screenPage == PageAdvanced); scrIr.setVisible (screenPage == PageIr);
        scrHardware.setVisible (screenPage == PageHardware); scrOutputs.setVisible (screenPage == PageOutputs);
        for (auto* k : softKeys) { k->repaint(); k->toFront (false); }
        if (lookPanel.isVisible()) lookPanel.toFront (false);
        repaint();
    }

    // ---------------------------------------------------------------- virtual screens
    ComboBox matA, matB, conv, irMatA, irMatB, osBox, trModel;
    Slider matMix, irMatMix;
    std::unique_ptr<APVTS::ComboBoxAttachment> matAAtt, matBAtt, convAtt, irMatAAtt, irMatBAtt, osAtt, trModelAtt, compTypeAtt, compToneAtt, dlDivLAtt, dlDivRAtt;
    std::unique_ptr<APVTS::SliderAttachment> matMixAtt, irMatMixAtt;
    std::unique_ptr<APVTS::ButtonAttachment> compOnAtt, compDuckAtt;
    KnobBox* advK[9] {}; KnobBox* spinK = nullptr; KnobBox* irK[8] {}; KnobBox* trK[2] {}; KnobBox* outScreenK = nullptr;
    LedKey irLoad { "LOAD", LedKey::Cream }, irPrev { "<", LedKey::Cream }, irNext { ">", LedKey::Cream }, irClear { "CLEAR", LedKey::Cream };
    std::unique_ptr<ToggleKey> irReverse, irPitch, hwXfIn, hwXfOut, hwNoise, hwOn2, trOn, auxMain;
    std::unique_ptr<ChoiceKeys> hwIn, hwOut2, cfgKeys;
    ComboBox machA, machB; std::unique_ptr<APVTS::ComboBoxAttachment> machAAtt, machBAtt;

    void buildAdvanced()
    {
        auto& v = scrAdvanced; v.title = "ADVANCED";
        const char* ids[][2] = { { "damping", "DAMPING" }, { "tone", "INPUT TONE" }, { "hiCut", "HI CUT" }, { "inWidth", "WIDTH" },
                                 { "xover", "BASS XOVER" }, { "dirt", "DIRT" }, { "rich", "RICHNESS" }, { "mod", "MOD/WANDER" }, { "diff", "DIFFUSE" } };
        for (int i = 0; i < 9; ++i) advK[i] = addScreenKnob (v, ids[i][0], ids[i][1], true);
        spinK = addScreenKnob (v, "spin", "SPIN (480)", true);
        for (auto* b : { &matA, &matB, &conv }) v.addAndMakeVisible (*b);
        conv.addItemList (kConverterNames, 1);
        matMix.setSliderStyle (Slider::LinearHorizontal); matMix.setTextBoxStyle (Slider::NoTextBox, false, 0, 0); v.addAndMakeVisible (matMix);
        v.footer = "Materials, converter and dirt shape the algorithm only. The IR player stays clean.";
    }
    void layoutAdvanced()
    {
        auto& v = scrAdvanced; v.captions.clear();
        const int w = v.getWidth() - v.keepClearRight;
        for (int i = 0; i < 10; ++i)
        {
            KnobBox* k = i < 9 ? advK[i] : spinK;
            k->setBounds (24 + (i % 5) * ((w - 48) / 5), 64 + (i / 5) * 128, (w - 48) / 5, 112);
        }
        const int y = 330;
        v.captions.push_back ({ { 24, y, 200, 18 }, "MATERIAL A" }); matA.setBounds (24, y + 22, 240, 30);
        v.captions.push_back ({ { 280, y, 200, 18 }, "BLEND A-B" }); matMix.setBounds (280, y + 22, 170, 30);
        v.captions.push_back ({ { 466, y, 200, 18 }, "MATERIAL B" }); matB.setBounds (466, y + 22, 240, 30);
        v.captions.push_back ({ { 24, y + 70, 200, 18 }, "CONVERTER" }); conv.setBounds (24, y + 92, 260, 30);
    }
    void buildIr()
    {
        auto& v = scrIr; v.title = "IMPULSE RESPONSE";
        for (auto* b : { &irLoad, &irPrev, &irNext, &irClear }) { b->showLed = false; v.addAndMakeVisible (*b); }
        irLoad.onClick = [this] { chooseIr(); };
        irPrev.onClick = [this] { stepIr (-1); };
        irNext.onClick = [this] { stepIr (1); };
        irClear.onClick = [this] { proc.clearIr(); };
        const char* irk[][2] = { { "irStart", "START" }, { "irXover", "ER/LR X" }, { "irFadeOut", "FADE OUT" }, { "irXfeed", "X-FEED" },
                                 { "irXdelay", "X-DELAY" }, { "irAsym", "ASYM" } };
        for (int i = 0; i < 6; ++i) irK[i] = addScreenKnob (v, irk[i][0], irk[i][1], false);
        irK[6] = addScreenKnob (v, "diff", "DIFFUSE", true);
        irK[7] = addScreenKnob (v, "mod", "MOD", true);
        irReverse = std::make_unique<ToggleKey> (proc.apvts, "irReverse", "REVERSE", LedKey::Cream, false); v.addAndMakeVisible (*irReverse);
        irPitch = std::make_unique<ToggleKey> (proc.apvts, "irPitch", "STRETCH PITCH", LedKey::Cream, false); v.addAndMakeVisible (*irPitch);
        for (auto* b : { &irMatA, &irMatB }) v.addAndMakeVisible (*b);
        irMatMix.setSliderStyle (Slider::LinearHorizontal); irMatMix.setTextBoxStyle (Slider::NoTextBox, false, 0, 0); v.addAndMakeVisible (irMatMix);
        v.footer = "Clean path: no converter, dirt, richness or hardware colouring - only the IR and your EQ / comp.";
    }
    void layoutIr()
    {
        auto& v = scrIr; v.captions.clear();
        const int w = v.getWidth() - v.keepClearRight;
        irLoad.setBounds (24, 56, 90, 32); irPrev.setBounds (120, 56, 36, 32); irNext.setBounds (160, 56, 36, 32); irClear.setBounds (204, 56, 90, 32);
        v.captions.push_back ({ { 310, 56, w - 420, 32 }, {} });
        for (int i = 0; i < 8; ++i) irK[i]->setBounds (24 + (i % 4) * ((w - 48) / 4), 104 + (i / 4) * 120, (w - 48) / 4, 108);
        irReverse->setBounds (24, 352, 150, 32); irPitch->setBounds (186, 352, 190, 32);
        v.captions.push_back ({ { 24, 398, 200, 18 }, "MATERIAL A" }); irMatA.setBounds (24, 420, 220, 30);
        v.captions.push_back ({ { 260, 398, 200, 18 }, "BLEND" }); irMatMix.setBounds (260, 420, 150, 30);
        v.captions.push_back ({ { 426, 398, 200, 18 }, "MATERIAL B" }); irMatB.setBounds (426, 420, 220, 30);
    }
    void buildHardware()
    {
        auto& v = scrHardware; v.title = "HARDWARE  -  480L SIGNAL CHAIN";
        auto& st = proc.apvts;
        hwOn2 = std::make_unique<ToggleKey> (st, "hwOn", "ANALOG", LedKey::Cream, false); v.addAndMakeVisible (*hwOn2);
        hwIn = std::make_unique<ChoiceKeys> (st, "hwIn", StringArray { "IN ANALOG", "IN DIGITAL" }, LedKey::Dark, false); v.addAndMakeVisible (*hwIn);
        hwOut2 = std::make_unique<ChoiceKeys> (st, "hwOut", StringArray { "OUT ANALOG", "OUT DIGITAL" }, LedKey::Dark, false); v.addAndMakeVisible (*hwOut2);
        hwXfIn = std::make_unique<ToggleKey> (st, "hwXfIn", "XFMR IN", LedKey::Cream, false); v.addAndMakeVisible (*hwXfIn);
        hwXfOut = std::make_unique<ToggleKey> (st, "hwXfOut", "XFMR OUT", LedKey::Cream, false); v.addAndMakeVisible (*hwXfOut);
        hwNoise = std::make_unique<ToggleKey> (st, "hwNoise", "NOISE -98 dB", LedKey::Cream, false); v.addAndMakeVisible (*hwNoise);
        osBox.addItemList (osNames(), 1); v.addAndMakeVisible (osBox); osAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, "os", osBox);
        trOn = std::make_unique<ToggleKey> (st, "trOn", "OUTPUT IRON", LedKey::Cream, false); v.addAndMakeVisible (*trOn);
        trModel.addItemList (trNames(), 1); v.addAndMakeVisible (trModel); trModelAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, "trModel", trModel);
        trK[0] = addScreenKnob (v, "trFreq", "IRON FREQ", false);
        trK[1] = addScreenKnob (v, "trAmt", "IRON AMOUNT", false);
        v.footer = "Applies to the algorithms only. The dry signal and the IR player never pass through it.";
    }
    void layoutHardware()
    {
        auto& v = scrHardware; v.captions.clear();
        hwOn2->setBounds (24, 60, 130, 34);
        hwIn->setBounds (170, 60, 300, 34);
        hwOut2->setBounds (486, 60, 300, 34);
        hwXfIn->setBounds (24, 110, 130, 34); hwXfOut->setBounds (170, 110, 130, 34); hwNoise->setBounds (316, 110, 154, 34);
        v.captions.push_back ({ { 486, 110, 140, 34 }, "OVERSAMPLING" }); osBox.setBounds (636, 112, 90, 30);
        v.captions.push_back ({ { 24, 162, 860, 18 }, "IN:  XLR > input transformer (option) > LM833 receiver > 9-pole 20 kHz filter > 18-bit gain-ranging A/D" });
        v.captions.push_back ({ { 24, 184, 860, 18 }, "OUT: 18-bit D/A (PCM53 + 2 LSB) > sample & hold > 9-pole 20 kHz filter > LLA5200 line driver > transformer (option)" });
        v.captions.push_back ({ { 24, 222, 300, 18 }, "EXTRA: OUTPUT TRANSFORMER (whole output)" });
        trOn->setBounds (24, 246, 160, 34); trModel.setBounds (196, 248, 300, 30);
        trK[0]->setBounds (520, 226, 100, 110); trK[1]->setBounds (630, 226, 100, 110);
    }
    void buildOutputs()
    {
        auto& v = scrOutputs; v.title = "OUTPUTS  -  CONFIGURATION";
        auto& st = proc.apvts;
        cfgKeys = std::make_unique<ChoiceKeys> (st, "config", StringArray { "SINGLE", "CASCADE", "STEREO SPLIT", "MONO SPLIT" }, LedKey::Dark, true);
        v.addAndMakeVisible (*cfgKeys);
        machA.addItemList (slotChoiceNames(), 1); machB.addItemList (slotChoiceNames(), 1);
        v.addAndMakeVisible (machA); v.addAndMakeVisible (machB);
        machAAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, "machA", machA);
        machBAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, "machB", machB);
        auxMain = std::make_unique<ToggleKey> (st, "auxToMain", "AUX > MAIN", LedKey::Cream, false); v.addAndMakeVisible (*auxMain);
        outScreenK = addScreenKnob (v, "out", "OUTPUT", false);
        v.footer = "Studio One: enable the plugin's Aux output (outputs 3-4) to get the Aux pair on its own channel.";
    }
    void layoutOutputs()
    {
        auto& v = scrOutputs; v.captions.clear();
        cfgKeys->setBounds (24, 58, 640, 52);
        v.captions.push_back ({ { 24, 126, 200, 18 }, "MACHINE A" }); machA.setBounds (24, 148, 200, 30);
        v.captions.push_back ({ { 240, 126, 200, 18 }, "MACHINE B" }); machB.setBounds (240, 148, 200, 30);
        auxMain->setBounds (460, 146, 150, 34);
        outScreenK->setBounds (690, 54, 110, 120);
        const int y = 204;
        v.captions.push_back ({ { 24, y, 860, 18 },      "SINGLE:        all switched-on algorithms, the same on MAIN and AUX" });
        v.captions.push_back ({ { 24, y + 22, 860, 18 }, "CASCADE:       A feeds B.  MAIN = A > B,  AUX = A only" });
        v.captions.push_back ({ { 24, y + 44, 860, 18 }, "STEREO SPLIT:  both get the stereo input.  MAIN = A,  AUX = B" });
        v.captions.push_back ({ { 24, y + 66, 860, 18 }, "MONO SPLIT:    left in > A,  right in > B.  MAIN = A,  AUX = B" });
        v.captions.push_back ({ { 24, y + 104, 860, 18 }, "AUX > MAIN: with no Aux bus in the host, machine B is mixed into MAIN so you hear both." });
    }

    // ---------------------------------------------------------------- LOOK
    Component lookPanel;
    struct Swatch : public Button
    {
        Swatch (Colour c, bool round) : Button ({}), col (c), circle (round) {}
        Colour col; bool circle; bool selected = false;
        void paintButton (Graphics& g, bool over, bool) override
        {
            auto r = getLocalBounds().toFloat().reduced (3.0f);
            g.setColour (col);
            if (circle) g.fillEllipse (r); else g.fillRoundedRectangle (r, 4.0f);
            g.setColour (selected ? Colour (0xffe6e1d5) : Colours::white.withAlpha (over ? 0.5f : 0.15f));
            if (circle) g.drawEllipse (r.expanded (selected ? 2.0f : 0.0f), selected ? 2.0f : 1.0f);
            else g.drawRoundedRectangle (r.expanded (selected ? 2.0f : 0.0f), 4.0f, selected ? 2.0f : 1.0f);
        }
    };
    OwnedArray<Swatch> panelSw, ledSw;
    Slider lkGrad, lkLight, lkContrast; LedKey lk3D { "3D", LedKey::Dark, false }, lk2D { "2D", LedKey::Dark, false }, lkClose { "CLOSE", LedKey::Cream };
    ComboBox sizeBox; LedKey lkPanelMore { "...", LedKey::Cream }, lkLedMore { "...", LedKey::Cream };
    std::unique_ptr<ColourSelector> picker;

    struct LookBg : Component
    {
        MainPanel& m; explicit LookBg (MainPanel& p) : m (p) { setInterceptsMouseClicks (false, false); }
        void paint (Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            DropShadow (Colours::black.withAlpha (0.7f), 18, { 0, 8 }).drawForRectangle (g, getLocalBounds());
            g.setColour (Theme::shade (m.look.theme.panel, -0.55f)); g.fillRoundedRectangle (r, 6.0f);
            g.setColour (Colours::white.withAlpha (0.12f)); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
            g.setColour (Colour (0xffe6e1d5)); g.setFont (uiFont (13.0f, true));
            g.drawText ("LOOK", 16, 10, 100, 22, Justification::centredLeft);
            g.setColour (Colour (0xffa29c8e)); g.setFont (uiFont (11.0f, true));
            g.drawText ("PANEL", 16, 40, 100, 16, Justification::centredLeft);
            g.drawText ("DISPLAY", 16, 108, 100, 16, Justification::centredLeft);
            g.drawText ("GRADIENT", 16, 178, 90, 20, Justification::centredLeft);
            g.drawText ("LIGHTING", 16, 210, 90, 20, Justification::centredLeft);
            g.drawText ("CONTRAST", 16, 242, 90, 20, Justification::centredLeft);
            g.drawText ("STYLE", 16, 284, 60, 20, Justification::centredLeft);
            g.drawText ("SIZE", 16, 326, 60, 20, Justification::centredLeft);
        }
    } lookBg { *this };

    void buildLook()
    {
        lookPanel.addAndMakeVisible (lookBg);
        for (auto c : kPanelChoices) { auto* s = panelSw.add (new Swatch (c, false)); s->onClick = [this, c] { look.theme.panel = c; lookChanged(); }; lookPanel.addAndMakeVisible (s); }
        for (auto c : kLedChoices)   { auto* s = ledSw.add (new Swatch (c, true));    s->onClick = [this, c] { look.theme.led = c; lookChanged(); }; lookPanel.addAndMakeVisible (s); }
        for (auto* b : { &lkPanelMore, &lkLedMore }) { b->showLed = false; lookPanel.addAndMakeVisible (*b); }
        lkPanelMore.onClick = [this] { pickColour (true); };
        lkLedMore.onClick = [this] { pickColour (false); };
        auto setup = [this] (Slider& s, float v, std::function<void (float)> fn)
        {
            s.setSliderStyle (Slider::LinearHorizontal); s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
            s.setRange (0.0, 1.0, 0.01); s.setValue (v, dontSendNotification);
            s.onValueChange = [this, &s, fn] { fn ((float) s.getValue()); lookChanged(); };
            lookPanel.addAndMakeVisible (s);
        };
        setup (lkGrad, look.theme.grad, [this] (float v) { look.theme.grad = v; });
        setup (lkLight, look.theme.light, [this] (float v) { look.theme.light = v; });
        setup (lkContrast, look.theme.contrast, [this] (float v) { look.theme.contrast = v; });
        lk3D.ledState = [this] { return ! look.theme.flat; }; lk2D.ledState = [this] { return look.theme.flat; };
        lk3D.onClick = [this] { look.theme.flat = false; lookChanged(); };
        lk2D.onClick = [this] { look.theme.flat = true; lookChanged(); };
        lookPanel.addAndMakeVisible (lk3D); lookPanel.addAndMakeVisible (lk2D);
        lkClose.showLed = false; lkClose.onClick = [this] { lookPanel.setVisible (false); lookBtn.repaint(); };
        lookPanel.addAndMakeVisible (lkClose);
        sizeBox.addItemList (StringArray { "75 %", "90 %", "100 %", "125 %", "150 %", "175 %", "200 %" }, 1);
        sizeBox.onChange = [this] { static const float sc[] = { 0.75f, 0.9f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f }; const int i = sizeBox.getSelectedItemIndex(); if (i >= 0 && onScaleChosen) onScaleChosen (sc[i]); };
        lookPanel.addAndMakeVisible (sizeBox);
        updateSwatches();
    }
    void layoutLook()
    {
        lookBg.setBounds (lookPanel.getLocalBounds());
        lkClose.setBounds (lookPanel.getWidth() - 84, 8, 72, 26);
        for (int i = 0; i < panelSw.size(); ++i) panelSw[i]->setBounds (14 + i * 42, 58, 38, 38);
        lkPanelMore.setBounds (14 + panelSw.size() * 42 + 2, 62, 34, 30);
        for (int i = 0; i < ledSw.size(); ++i) ledSw[i]->setBounds (14 + i * 42, 126, 38, 38);
        lkLedMore.setBounds (14 + ledSw.size() * 42 + 2, 130, 34, 30);
        lkGrad.setBounds (110, 178, 200, 20); lkLight.setBounds (110, 210, 200, 20); lkContrast.setBounds (110, 242, 200, 20);
        lk3D.setBounds (110, 280, 80, 30); lk2D.setBounds (200, 280, 80, 30);
        sizeBox.setBounds (110, 322, 120, 28);
    }
    void updateSwatches()
    {
        for (auto* s : panelSw) { s->selected = s->col == look.theme.panel; s->repaint(); }
        for (auto* s : ledSw) { s->selected = s->col == look.theme.led; s->repaint(); }
    }
    void pickColour (bool panelColour)
    {
        picker = std::make_unique<ColourSelector> (ColourSelector::showColourspace | ColourSelector::showSliders);
        picker->setCurrentColour (panelColour ? look.theme.panel : look.theme.led, dontSendNotification);
        picker->setSize (300, 300);
        struct L : ChangeListener { MainPanel& m; bool p; L (MainPanel& mm, bool pp) : m (mm), p (pp) {} void changeListenerCallback (ChangeBroadcaster* b) override
            { if (auto* cs = dynamic_cast<ColourSelector*> (b)) { (p ? m.look.theme.panel : m.look.theme.led) = cs->getCurrentColour().withAlpha (1.0f); m.lookChanged(); } } };
        pickListener = std::make_unique<L> (*this, panelColour);
        picker->addChangeListener (pickListener.get());
        CallOutBox::launchAsynchronously (std::move (picker), (panelColour ? lkPanelMore : lkLedMore).getScreenBounds(), nullptr);
    }
    std::unique_ptr<ChangeListener> pickListener;

    void lookChanged()
    {
        look.refresh(); updateSwatches();
        auto& s = proc.apvts.state;
        s.setProperty ("lookPanel", look.theme.panel.toString(), nullptr); s.setProperty ("lookLed", look.theme.led.toString(), nullptr);
        s.setProperty ("lookGrad", look.theme.grad, nullptr); s.setProperty ("lookLight", look.theme.light, nullptr);
        s.setProperty ("lookContrast", look.theme.contrast, nullptr); s.setProperty ("lookFlat", look.theme.flat, nullptr);
        // also the default for new instances
        XmlElement x ("FlexiverbLook");
        x.setAttribute ("panel", look.theme.panel.toString()); x.setAttribute ("led", look.theme.led.toString());
        x.setAttribute ("grad", look.theme.grad); x.setAttribute ("light", look.theme.light); x.setAttribute ("contrast", look.theme.contrast); x.setAttribute ("flat", look.theme.flat);
        lookFile().getParentDirectory().createDirectory(); x.writeTo (lookFile());
        repaintAll (*this);
    }
    static void repaintAll (Component& c) { c.repaint(); for (auto* ch : c.getChildren()) repaintAll (*ch); }
    static File lookFile() { return userPresetFolder().getParentDirectory().getChildFile ("FlexiverbLook.xml"); }
    void loadLook()
    {
        auto& s = proc.apvts.state; auto& t = look.theme;
        if (s.hasProperty ("lookPanel"))
        {
            t.panel = Colour::fromString (s.getProperty ("lookPanel").toString()); t.led = Colour::fromString (s.getProperty ("lookLed").toString());
            t.grad = (float) s.getProperty ("lookGrad", 0.6f); t.light = (float) s.getProperty ("lookLight", 0.6f);
            t.contrast = (float) s.getProperty ("lookContrast", 0.5f); t.flat = (bool) s.getProperty ("lookFlat", false);
        }
        else if (auto x = parseXML (lookFile()))
        {
            t.panel = Colour::fromString (x->getStringAttribute ("panel", t.panel.toString())); t.led = Colour::fromString (x->getStringAttribute ("led", t.led.toString()));
            t.grad = (float) x->getDoubleAttribute ("grad", 0.6); t.light = (float) x->getDoubleAttribute ("light", 0.6);
            t.contrast = (float) x->getDoubleAttribute ("contrast", 0.5); t.flat = x->getBoolAttribute ("flat", false);
        }
        look.refresh();
    }

    // ---------------------------------------------------------------- presets
    Array<File> userFiles;
    void buildPresetMenu()
    {
        auto* root = presetBox.getRootMenu();
        root->clear();
        root->addItem (kDefaultId, "Default");
        String group; PopupMenu sub;
        for (int i = 0; i < kNumPresets; ++i)
        {
            if (group != kPresets[i].group && group.isNotEmpty()) { root->addSubMenu (group, sub); sub = PopupMenu(); }
            group = kPresets[i].group;
            sub.addItem (kBuiltinBase + i, kPresets[i].name);
        }
        if (group.isNotEmpty()) root->addSubMenu (group, sub);
        userFiles = userPresetFolder().findChildFiles (File::findFiles, false, "*.pvpreset");
        std::sort (userFiles.begin(), userFiles.end(), [] (const File& a, const File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
        PopupMenu u;
        for (int i = 0; i < userFiles.size(); ++i) u.addItem (kUserBase + i, userFiles[i].getFileNameWithoutExtension());
        if (userFiles.isEmpty()) u.addItem (8999, "(no user presets yet)", false);
        root->addSeparator();
        root->addSubMenu ("User", u);
        root->addItem (kSaveId, "Save User Preset...");
        root->addItem (kDeleteId, "Delete Current User Preset", proc.userPresetName.isNotEmpty());
        root->addItem (kFolderId, "Show User Preset Folder");
        syncPresetBox();
    }
    void syncPresetBox()
    {
        int id = 0;
        if (proc.userPresetName.isNotEmpty())
        {
            for (int i = 0; i < userFiles.size(); ++i) if (userFiles[i].getFileNameWithoutExtension() == proc.userPresetName) id = kUserBase + i;
        }
        else id = proc.getCurrentProgram() == 0 ? kDefaultId : kBuiltinBase + proc.getCurrentProgram() - 1;
        if (id != 0 && presetBox.getSelectedId() != id) presetBox.setSelectedId (id, dontSendNotification);
    }
    void presetChosen()
    {
        const int id = presetBox.getSelectedId();
        if (id == kSaveId) { syncPresetBox(); savePreset(); return; }
        if (id == kDeleteId)
        {
            syncPresetBox();
            const File f = userPresetFolder().getChildFile (proc.userPresetName + ".pvpreset");
            if (f.existsAsFile()) f.moveToTrash();
            proc.userPresetName = {};
            buildPresetMenu(); return;
        }
        if (id == kFolderId) { syncPresetBox(); auto d = userPresetFolder(); d.createDirectory(); d.revealToUser(); return; }
        if (id == kDefaultId) proc.setCurrentProgram (0);
        else if (id >= kBuiltinBase && id < kBuiltinBase + kNumPresets) proc.setCurrentProgram (id - kBuiltinBase + 1);
        else if (id >= kUserBase && id - kUserBase < userFiles.size()) proc.loadUserPreset (userFiles[id - kUserBase]);
        buildPresetMenu();
    }
    void stepPreset (int d)
    {
        if (proc.userPresetName.isNotEmpty() && ! userFiles.isEmpty())
        {
            int i = 0; for (int k = 0; k < userFiles.size(); ++k) if (userFiles[k].getFileNameWithoutExtension() == proc.userPresetName) i = k;
            proc.loadUserPreset (userFiles[(i + d + userFiles.size()) % userFiles.size()]);
        }
        else proc.setCurrentProgram ((proc.getCurrentProgram() + d + kNumPresets + 1) % (kNumPresets + 1));
        buildPresetMenu();
    }
    std::unique_ptr<AlertWindow> alert;
    void savePreset()
    {
        alert = std::make_unique<AlertWindow> ("Save User Preset", "Name:", MessageBoxIconType::NoIcon, this);
        alert->addTextEditor ("name", proc.userPresetName.isNotEmpty() ? proc.userPresetName : String ("My Preset"));
        alert->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
        alert->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        alert->enterModalState (true, ModalCallbackFunction::create ([this] (int r)
        {
            if (r == 1 && alert != nullptr)
            {
                const String name = alert->getTextEditorContents ("name").trim();
                if (name.isNotEmpty()) proc.saveUserPreset (name);
                buildPresetMenu();
            }
            alert.reset();
        }), false);
    }

    // ---------------------------------------------------------------- IR files
    std::unique_ptr<FileChooser> chooser;
    void chooseIr()
    {
        File start = proc.irFileA().existsAsFile() ? proc.irFileA().getParentDirectory() : File::getSpecialLocation (File::userDocumentsDirectory);
        chooser = std::make_unique<FileChooser> ("Load impulse response (select one file, or the L and R files together)",
                                                 start, "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::canSelectMultipleItems,
                              [this] (const FileChooser& fc)
        {
            Array<File> fs;
            for (auto& f : fc.getResults()) if (f.existsAsFile()) fs.add (f);
            if (fs.isEmpty()) return;
            if (fs.size() >= 2)
            {
                File a = fs[0], b = fs[1];
                if (irSide (a) == 1 || irSide (b) == 0) std::swap (a, b);     // put the left file first
                proc.requestIrLoad (a, b, true);
            }
            else loadAuto (fs[0]);
            selectSlot (pv::IR);
            if (raw (sid (pv::IR, "on")) < 0.5f) proc.setParam (sid (pv::IR, "on"), 1.0f);
        });
    }
    void loadAuto (const File& f)
    {
        bool isLeft = true;
        const File partner = findPartner (f, isLeft);
        if (partner.existsAsFile()) proc.requestIrLoad (isLeft ? f : partner, isLeft ? partner : f, true);
        else proc.requestIrLoad (f, File(), true);
    }
    void stepIr (int d)
    {
        const File cur = proc.irFileA();
        if (! cur.existsAsFile()) return;
        auto files = cur.getParentDirectory().findChildFiles (File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac");
        std::sort (files.begin(), files.end(), [] (const File& a, const File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
        Array<File> list;
        for (auto& f : files) { bool left = true; const File p = findPartner (f, left); if (p.existsAsFile() && ! left) continue; list.add (f); }
        int i = list.indexOf (cur); if (i < 0) i = 0;
        if (list.isEmpty()) return;
        loadAuto (list[(i + d + list.size()) % list.size()]);
    }

    // ---------------------------------------------------------------- timer
    void timerCallback() override
    {
        const int req = proc.presetSlotRequest.exchange (-1);
        if (req >= 0 && req < pv::kNumSlots) { bindSlot (req); buildPresetMenu(); }
        if (++dspTick >= 10)
        {
            dspTick = 0;
            const float d = std::round (proc.dspLoad.load() * 10.0f) / 10.0f;
            if (d != shownDsp) { shownDsp = d; repaint (dspArea); }
        }
        auto& params = proc.getParameters();
        bool changed = false;
        for (int i = 0; i < params.size() && i < lastValues.size(); ++i)
        {
            const float v = params[i]->getValue();
            if (v != lastValues[i]) { lastValues.set (i, v); changed = true; }
        }
        syncPresetBox();
        const bool sync = raw ("dlSync") > 0.5f;
        if (sync != lastSync) { lastSync = sync; updateSide(); }
        const bool optoOnly = (int) raw (sid (editSlot, "cpType")) == pv::CompOpto;
        for (int i : { 2, 3 }) compK[i]->setEnabled (! optoOnly);
        const int ver = proc.waveVersion.load();
        if (ver != lastWaveVersion)
        {
            lastWaveVersion = ver;
            wave = proc.getWave (editSlot);
            repaint (dispArea);
        }
        if (screenPage == PageIr) { scrIr.captions[0].second = proc.irLayout() == pv::IrNone ? String ("no file loaded") : proc.irInfo(); }
        if (changed)
        {
            repaint (dispArea); for (auto* k : knobs) k->repaint();
            for (auto* k : algoKeys) k->repaint();
            if (sideOpen && sideTab == 0) eqGraph.repaint();
            if (screenPage != PageNone) for (auto* v : { &scrAdvanced, &scrIr, &scrHardware, &scrOutputs }) if (v->isVisible()) v->repaint();
        }
        repaint (meterArea[0].getUnion (meterArea[2]).expanded (4));
        if (sideOpen && sideTab == 1) repaint (inMeter.getUnion (grMeter).expanded (40, 24));
    }

    PlateVerbProcessor& proc;
    FlexLook look;
    ComboBox presetBox;
    LedKey presetPrev, presetNext, presetSave, lookBtn, foldBtn;
    std::unique_ptr<ToggleKey> analogKey, dlOn, dlSync, dlPing;
    std::unique_ptr<LedKey> compOn, compDuck;
    ComboBox compType, compTone, dlDivL, dlDivR;
    OwnedArray<ToggleKey> algoKeys; int algoOrder[pv::kNumSlots] {};
    OwnedArray<LedKey> softKeys, tabKeys, vTabs;
    OwnedArray<FaderBox> faders;
    OwnedArray<KnobBox> knobs;
    KnobBox* eqF[4] {}; KnobBox* eqG[4] {}; KnobBox* compK[6] {}; KnobBox* dlK[12] {}; KnobBox* outKnob = nullptr;
    std::unique_ptr<ChoiceKeys> outType;
    EqGraph eqGraph;
    VirtualScreen scrAdvanced, scrIr, scrHardware, scrOutputs;
    pv::WaveData wave;
    Rectangle<int> railArea, surfArea, sideArea, dispArea, bayArea, meterArea[3], inMeter, grMeter, dspArea;
    float shownDsp = 0.0f; int dspTick = 0;
    Array<float> lastValues; int editSlot = 0, lastWaveVersion = -1, sideTab = 0;
    bool sideOpen = true, page2 = false, lastSync = false; Page screenPage = PageNone;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};

// ============================================================================ editor: scales the panel to the window
class PlateVerbEditor : public AudioProcessorEditor
{
public:
    explicit PlateVerbEditor (PlateVerbProcessor& p) : AudioProcessorEditor (&p), procRef (p), panel (p)
    {
        setLookAndFeel (&panel.getLook());
        addAndMakeVisible (panel);
        scale = (float) (double) p.apvts.state.getProperty ("uiScale3", 0.9);
        panel.onSizeChanged = [this] { applySize(); };
        panel.onScaleChosen = [this] (float s) { scale = s; procRef.apvts.state.setProperty ("uiScale3", scale, nullptr); applySize(); };
        setResizable (true, true);
        applySize();
        panel.showScale (scale);
    }
    ~PlateVerbEditor() override { setLookAndFeel (nullptr); }
    void resized() override
    {
        if (applying) return;
        scale = jlimit (0.5f, 2.0f, (float) getWidth() / (float) panel.designWidth());
        procRef.apvts.state.setProperty ("uiScale3", scale, nullptr);
        panel.setTransform (AffineTransform::scale (scale));
        panel.setBounds (0, 0, panel.designWidth(), MainPanel::H);
    }

   #if FLEX_TEST
    MainPanel& getPanel() { return panel; }
   #endif
private:
    void applySize()
    {
        const int w = panel.designWidth(), h = MainPanel::H;
        applying = true;
        setResizeLimits (w / 2, h / 2, w * 2, h * 2);
        getConstrainer()->setFixedAspectRatio ((double) w / h);
        setSize (roundToInt (w * scale), roundToInt (h * scale));
        applying = false;
        panel.setTransform (AffineTransform::scale (scale));
        panel.setBounds (0, 0, w, h);
    }
    PlateVerbProcessor& procRef;
    MainPanel panel;
    float scale = 0.9f; bool applying = false;
};

AudioProcessorEditor* PlateVerbProcessor::createEditor() { return new PlateVerbEditor (*this); }
