// Flexiverb editor (included by PluginProcessor.cpp)
// Three complete looks (Remote, Studio, Ivory) on one layout, each adjustable in ADVANCED > LOOK:
// interface, side panel and lettering colours with their own gradients, display colour, 3D depth and contrast.
#include <complex>
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
static Font ui (float h, W w = W::Medium, float kern = 0.0f)
{
    auto& f = FlexFonts::get();
    auto tf = w == W::Bold ? f.bold : w == W::Semibold ? f.semibold : w == W::Regular ? f.regular : f.medium;
    return Font (FontOptions (tf).withHeight (h).withKerningFactor (kern));
}
static Font segFont (float h) { return Font (FontOptions (FlexFonts::get().seg).withHeight (h)); }

// ============================================================================ theme
enum class Style { Remote = 0, Studio, Ivory };

struct Theme
{
    Style style = Style::Remote;
    Colour iface { 0xff2b2c2e }, side { 0xff232426 }, font { 0xffe9e4d8 }, led { 0xffff3b1f };
    float ifaceGrad = 0.5f, sideGrad = 0.5f, fontGrad = 0.25f, depth = 0.6f, contrast = 0.5f;

    static Colour amber() { return Colour (0xffffb43c); }
    static Colour shade (Colour c, float k) { return k >= 0.0f ? c.interpolatedWith (Colours::white, jmin (1.0f, k)) : c.interpolatedWith (Colours::black, jmin (1.0f, -k)); }
    bool light() const { return iface.getPerceivedBrightness() > 0.55f; }
    Colour well() const     { return shade (iface, light() ? -0.72f - 0.15f * contrast : -0.45f - 0.35f * contrast); }
    Colour screen() const   { return shade (led, -0.94f).interpolatedWith (Colour (0xff0a0a0b), 0.4f); }
    Colour dimFont() const  { return font.withAlpha (0.55f + 0.25f * contrast); }
    Colour keyFace() const
    {
        if (style == Style::Ivory) return shade (iface, -0.06f);
        if (style == Style::Studio) return shade (iface, 0.10f);
        return shade (iface, -0.25f);
    }
    float radius() const { return style == Style::Studio ? 6.0f : style == Style::Ivory ? 4.0f : 3.0f; }

    void defaults (Style s)
    {
        style = s;
        if (s == Style::Remote) { iface = Colour (0xff2b2c2e); side = Colour (0xff222325); font = Colour (0xffe9e4d8); led = Colour (0xffff3b1f); ifaceGrad = 0.45f; sideGrad = 0.45f; fontGrad = 0.2f; depth = 0.65f; contrast = 0.5f; }
        if (s == Style::Studio) { iface = Colour (0xff303338); side = Colour (0xff26292d); font = Colour (0xfff2f2f4); led = Colour (0xffffb43c); ifaceGrad = 0.25f; sideGrad = 0.2f; fontGrad = 0.0f; depth = 0.35f; contrast = 0.55f; }
        if (s == Style::Ivory)  { iface = Colour (0xffd9d1bf); side = Colour (0xffc9bfa9); font = Colour (0xff3a342b); led = Colour (0xffffa23a); ifaceGrad = 0.4f; sideGrad = 0.4f; fontGrad = 0.15f; depth = 0.55f; contrast = 0.5f; }
    }

    // a face lit from above: gradient (amount) and a soft shadow and highlight (depth)
    void face (Graphics& g, Rectangle<float> r, Colour base, float grad, float corner, bool shadow = true) const
    {
        if (shadow && depth > 0.01f)
        {
            Path p; p.addRoundedRectangle (r, corner);
            DropShadow (Colours::black.withAlpha (0.18f + 0.42f * depth), roundToInt (2 + 14 * depth), { 0, roundToInt (1 + 5 * depth) }).drawForPath (g, p);
        }
        g.setGradientFill (ColourGradient (shade (base, 0.10f * grad + 0.03f), r.getX(), r.getY(), shade (base, -0.16f * grad), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, corner);
        if (depth > 0.01f)
        {
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.05f + 0.16f * depth), r.getX(), r.getY(), Colours::transparentWhite, r.getX(), r.getY() + jmin (18.0f, r.getHeight() * 0.5f), false));
            g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
        }
        g.setColour (Colours::black.withAlpha (0.35f)); g.drawRoundedRectangle (r.expanded (0.5f), corner + 0.5f, 0.8f);
    }
    // a recess: inner shadow at the top, a faint light lip at the bottom
    void recess (Graphics& g, Rectangle<float> r, Colour base, float corner) const
    {
        g.setColour (base); g.fillRoundedRectangle (r, corner);
        const float d = 3.0f + 9.0f * depth;
        g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.25f + 0.4f * depth), r.getX(), r.getY(), Colours::transparentBlack, r.getX(), r.getY() + d, false));
        g.fillRoundedRectangle (r, corner);
        g.setColour (Colours::white.withAlpha (0.03f + 0.10f * depth)); g.drawHorizontalLine ((int) r.getBottom(), r.getX() + corner, r.getRight() - corner);
        g.setColour (Colours::black.withAlpha (0.45f)); g.drawRoundedRectangle (r, corner, 0.8f);
    }
    // lettering with its own colour gradient
    void text (Graphics& g, const String& s, Rectangle<float> r, Justification j, const Font& f, Colour c = {}, float alpha = 1.0f) const
    {
        const Colour col = (c.isTransparent() ? font : c).withMultipliedAlpha (alpha);
        g.setFont (f);
        if (fontGrad > 0.01f)
        {
            const float cy = r.getCentreY(), h = f.getHeight() * 0.5f;
            g.setGradientFill (ColourGradient (shade (col, 0.35f * fontGrad), 0, cy - h, shade (col, -0.45f * fontGrad), 0, cy + h, false));
        }
        else g.setColour (col);
        g.drawText (s, r, j, false);
    }
    // LED text: soft glow behind the characters
    void ledText (Graphics& g, const String& s, Rectangle<float> r, Justification j, const Font& f, float alpha = 1.0f, Colour c = {}) const
    {
        const Colour col = c.isTransparent() ? led : c;
        g.setFont (f);
        g.setColour (col.withAlpha (0.10f * alpha));
        for (auto o : { Point<float> (-1.2f, 0), Point<float> (1.2f, 0), Point<float> (0, -1.2f), Point<float> (0, 1.2f) }) g.drawText (s, r.translated (o.x, o.y), j, false);
        g.setColour (col.withAlpha (alpha)); g.drawText (s, r, j, false);
    }
};

// a faint grain so large painted areas don't look like flat plastic
static Image& grainImage()
{
    static Image img = []
    {
        Image im (Image::ARGB, 128, 128, true);
        Random r (4711);
        for (int y = 0; y < 128; ++y) for (int x = 0; x < 128; ++x)
        { const float v = r.nextFloat(); im.setPixelAt (x, y, (v > 0.5f ? Colours::white : Colours::black).withAlpha ((float) std::abs (v - 0.5f) * 0.10f)); }
        return im;
    }();
    return img;
}
static void grain (Graphics& g, Rectangle<int> r, float amount)
{
    if (amount <= 0.01f) return;
    Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (r);
    g.setOpacity (amount);
    g.setTiledImageFill (grainImage(), 0, 0, 1.0f);
    g.fillRect (r);
}

// ============================================================================ look & feel
class FlexLook : public LookAndFeel_V4
{
public:
    Theme t;
    FlexLook() { refresh(); }
    void refresh()
    {
        setColour (ComboBox::textColourId, t.style == Style::Studio ? t.font : t.led);
        setColour (ComboBox::backgroundColourId, t.screen());
        setColour (ComboBox::outlineColourId, Colours::black);
        setColour (PopupMenu::backgroundColourId, Colour (0xff1c1d1f));
        setColour (PopupMenu::textColourId, Colour (0xffe8e6e1));
        setColour (PopupMenu::highlightedBackgroundColourId, Theme::amber());
        setColour (PopupMenu::highlightedTextColourId, Colour (0xff1a1a1a));
        setColour (PopupMenu::headerTextColourId, Colour (0xff9a968e));
        setColour (BubbleComponent::backgroundColourId, Colour (0xff1c1d1f));
        setColour (BubbleComponent::outlineColourId, Colour (0xff3a3b3e));
        setColour (TooltipWindow::textColourId, Colour (0xffe8e6e1));
        setColour (AlertWindow::backgroundColourId, Colour (0xff1c1d1f));
        setColour (AlertWindow::textColourId, Colour (0xffe8e6e1));
        setColour (TextEditor::backgroundColourId, Colour (0xff111113));
        setColour (TextEditor::textColourId, Colour (0xffe8e6e1));
        setColour (TextEditor::outlineColourId, Colour (0xff3a3b3e));
        setColour (TextButton::buttonColourId, Colour (0xff2c2d30));
        setColour (TextButton::textColourOffId, Colour (0xffe8e6e1));
    }
    Font getPopupMenuFont() override { return ui (14.0f, W::Medium); }
    Font getComboBoxFont (ComboBox&) override { return ui (13.0f, W::Semibold); }
    Font getSliderPopupFont (Slider&) override { return ui (13.0f, W::Semibold); }
    Font getAlertWindowMessageFont() override { return ui (14.0f); }
    Font getTextButtonFont (TextButton&, int) override { return ui (13.0f, W::Semibold); }
    void drawPopupMenuBackground (Graphics& g, int w, int h) override
    {
        g.fillAll (Colour (0xff1c1d1f));
        g.setColour (Colour (0xff3a3b3e)); g.drawRect (0, 0, w, h);
    }

    void drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox&) override
    {
        auto r = Rectangle<float> (0, 0, (float) w, (float) h);
        t.recess (g, r.reduced (0.5f), t.style == Style::Studio ? Theme::shade (t.iface, -0.35f) : t.screen(), t.radius());
        Path a; const float cx = (float) w - 13.0f, cy = (float) h * 0.5f;
        a.startNewSubPath (cx - 4.0f, cy - 2.0f); a.lineTo (cx, cy + 2.5f); a.lineTo (cx + 4.0f, cy - 2.0f);
        g.setColour (t.style == Style::Studio ? t.font.withAlpha (0.7f) : t.led); g.strokePath (a, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    void positionComboBoxText (ComboBox& box, Label& label) override
    {
        label.setBounds (8, 0, box.getWidth() - 28, box.getHeight());
        label.setFont (getComboBoxFont (box));
    }

    // realistic knob: printed scale, knurled skirt, machined cap with a highlight, pointer line
    void drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider&) override
    {
        auto b = Rectangle<int> (x, y, w, h).toFloat();
        const float R = jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        // scale dots
        for (int i = 0; i <= 10; ++i)
        {
            const float a = a0 + (a1 - a0) * (float) i / 10.0f;
            const auto p = c.getPointOnCircumference (R - 2.0f, a);
            const float d = (i == 0 || i == 5 || i == 10) ? 2.4f : 1.6f;
            g.setColour (t.font.withAlpha (0.55f)); g.fillEllipse (p.x - d * 0.5f, p.y - d * 0.5f, d, d);
        }
        const float sk = R - 7.0f;          // skirt radius
        const float cp = sk * 0.74f;        // cap radius
        // shadow
        if (t.depth > 0.01f)
        {
            Path s; s.addEllipse (c.x - sk, c.y - sk, sk * 2, sk * 2);
            DropShadow (Colours::black.withAlpha (0.35f + 0.45f * t.depth), roundToInt (3 + 9 * t.depth), { 0, roundToInt (2 + 4 * t.depth) }).drawForPath (g, s);
        }
        const bool aluminium = t.style != Style::Remote;
        // skirt with knurling
        const Colour skBase = aluminium ? Colour (0xff3a3b3d) : Colour (0xff141415);
        g.setGradientFill (ColourGradient (Theme::shade (skBase, 0.18f), c.x, c.y - sk, Theme::shade (skBase, -0.35f), c.x, c.y + sk, false));
        g.fillEllipse (c.x - sk, c.y - sk, sk * 2, sk * 2);
        for (int i = 0; i < 60; ++i)
        {
            const float a = ang + MathConstants<float>::twoPi * (float) i / 60.0f;
            g.setColour ((i % 2 ? Colours::black : Colours::white).withAlpha (i % 2 ? 0.30f : 0.07f));
            g.drawLine (Line<float> (c.getPointOnCircumference (cp + 1.0f, a), c.getPointOnCircumference (sk - 0.5f, a)), 1.0f);
        }
        // pointer on the skirt
        g.setColour (Colour (0xfff4f1ea));
        g.drawLine (Line<float> (c.getPointOnCircumference (cp - 1.0f, ang), c.getPointOnCircumference (sk - 1.0f, ang)), 2.2f);
        // cap: machined face, conical sheen
        const Colour capBase = aluminium ? Colour (0xffb9bcc0) : Colour (0xff2a2b2d);
        ColourGradient cg (Theme::shade (capBase, 0.35f), c.x - cp * 0.6f, c.y - cp * 0.8f, Theme::shade (capBase, -0.35f), c.x + cp * 0.6f, c.y + cp * 0.9f, false);
        g.setGradientFill (cg); g.fillEllipse (c.x - cp, c.y - cp, cp * 2, cp * 2);
        // concentric machining rings
        for (float rr = cp * 0.2f; rr < cp; rr += 1.6f)
        { g.setColour ((aluminium ? Colours::white : Colours::white).withAlpha (aluminium ? 0.035f : 0.025f)); g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 0.6f); }
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (aluminium ? 0.45f : 0.18f), c.x, c.y - cp, Colours::transparentWhite, c.x, c.y, false));
        g.drawEllipse (c.x - cp + 0.6f, c.y - cp + 0.6f, cp * 2 - 1.2f, cp * 2 - 1.2f, 1.0f);
        g.setColour (Colours::black.withAlpha (0.5f)); g.drawEllipse (c.x - cp, c.y - cp, cp * 2, cp * 2, 0.8f);
        // pointer line on the cap
        g.setColour (aluminium ? Colour (0xff1d1d1f) : Colour (0xfff4f1ea));
        g.drawLine (Line<float> (c.getPointOnCircumference (cp * 0.25f, ang), c.getPointOnCircumference (cp - 2.0f, ang)), 2.0f);
    }

    void drawLinearSlider (Graphics& g, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           const Slider::SliderStyle style, Slider& s) override
    {
        if (style == Slider::LinearHorizontal)
        {
            const float cy = (float) y + (float) h * 0.5f;
            t.recess (g, { (float) x, cy - 3.0f, (float) w, 6.0f }, Colour (0xff0d0d0e), 3.0f);
            g.setColour (Theme::amber()); g.fillRoundedRectangle ((float) x + 1.0f, cy - 2.0f, sliderPos - (float) x - 1.0f, 4.0f, 2.0f);
            Rectangle<float> cap (sliderPos - 8.0f, cy - 8.0f, 16.0f, 16.0f);
            if (t.depth > 0.01f) { Path p; p.addEllipse (cap); DropShadow (Colours::black.withAlpha (0.5f), 4, { 0, 2 }).drawForPath (g, p); }
            g.setGradientFill (ColourGradient (Colour (0xfff2f2f2), cap.getX(), cap.getY(), Colour (0xffb9b9bb), cap.getX(), cap.getBottom(), false));
            g.fillEllipse (cap);
            return;
        }
        if (style != Slider::LinearVertical) { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s); return; }
        const float cx = (float) x + (float) w * 0.5f;
        // printed scale
        for (int i = 0; i <= 20; ++i)
        {
            const float ty = (float) y + (float) h * (float) i / 20.0f;
            const bool major = i % 5 == 0;
            g.setColour (t.font.withAlpha (major ? 0.55f : 0.28f));
            g.fillRect (cx - 21.0f - (major ? 8.0f : 4.0f), ty - 0.5f, major ? 8.0f : 4.0f, 1.0f);
            g.fillRect (cx + 21.0f, ty - 0.5f, major ? 8.0f : 4.0f, 1.0f);
        }
        // slot
        t.recess (g, { cx - 3.5f, (float) y - 8.0f, 7.0f, (float) h + 16.0f }, Colour (0xff050505), 3.5f);
        // cap
        const bool slim = t.style == Style::Studio;
        Rectangle<float> cap = slim ? Rectangle<float> (cx - 15.0f, sliderPos - 11.0f, 30.0f, 22.0f) : Rectangle<float> (cx - 19.0f, sliderPos - 15.0f, 38.0f, 30.0f);
        if (t.depth > 0.01f)
        {
            Path p; p.addRoundedRectangle (cap, slim ? 5.0f : 2.5f);
            DropShadow (Colours::black.withAlpha (0.45f + 0.35f * t.depth), roundToInt (3 + 8 * t.depth), { 0, roundToInt (2 + 6 * t.depth) }).drawForPath (g, p);
        }
        Colour capCol = t.style == Style::Ivory ? Colour (0xffe9e2d2) : slim ? Colour (0xffc9ccd0) : Colour (0xff1e1f20);
        ColourGradient cg (Theme::shade (capCol, 0.22f), cap.getX(), cap.getY(), Theme::shade (capCol, -0.28f), cap.getX(), cap.getBottom(), false);
        if (! slim) { cg.addColour (0.48, Theme::shade (capCol, -0.12f)); cg.addColour (0.52, Theme::shade (capCol, 0.06f)); }
        g.setGradientFill (cg); g.fillRoundedRectangle (cap, slim ? 5.0f : 2.5f);
        g.setColour (Colours::white.withAlpha (0.10f + 0.25f * t.depth)); g.drawHorizontalLine ((int) cap.getY() + 1, cap.getX() + 3.0f, cap.getRight() - 3.0f);
        g.setColour (Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (cap, slim ? 5.0f : 2.5f, 0.8f);
        g.setColour (t.style == Style::Remote ? Colour (0xfff4f1ea) : Colour (0xff1d1d1f));
        g.fillRect (cap.getX() + 5.0f, cap.getCentreY() - 1.0f, cap.getWidth() - 10.0f, 2.0f);
    }
    int getSliderThumbRadius (Slider& s) override { return s.isVertical() ? 15 : (s.isHorizontal() ? 8 : LookAndFeel_V4::getSliderThumbRadius (s)); }
};
static const Theme& themeOf (Component& c)
{
    if (auto* l = dynamic_cast<FlexLook*> (&c.getLookAndFeel())) return l->t;
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

// every key in the plugin: one height (26 px), as wide as its text, lit amber when on
static constexpr int kKeyH = 26;
class Key : public Button
{
public:
    explicit Key (const String& text = {}) : Button (text) {}
    std::function<bool()> lit;                 // lit state, if not the toggle state
    std::function<void()> onRightClick;
    bool marked = false;                       // a thin amber underline (the algorithm being edited)
    int idealWidth() const { return jmax (28, roundToInt (ui (12.0f, W::Semibold, 0.04f).getStringWidthFloat (getButtonText())) + 26); }
    void mouseDown (const MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && onRightClick) { onRightClick(); return; }
        Button::mouseDown (e);
    }
    void paintButton (Graphics& g, bool over, bool down) override
    {
        const auto& t = themeOf (*this);
        auto r = getLocalBounds().toFloat().reduced (0.5f, 0.5f);
        r = r.withSizeKeepingCentre (r.getWidth(), (float) kKeyH - 1.0f);
        const bool on = lit ? lit() : getToggleState();
        const float rad = t.radius();
        if (down) r = r.translated (0.0f, 0.6f);
        if (on)
        {
            g.setColour (Theme::amber().withAlpha (0.18f + 0.18f * t.depth)); g.fillRoundedRectangle (r.expanded (2.5f), rad + 2.0f);
            g.setGradientFill (ColourGradient (Colour (0xffffd27a), r.getX(), r.getY(), Colour (0xffe8961f), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, rad);
            g.setColour (Colours::white.withAlpha (0.35f)); g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + rad, r.getRight() - rad);
            g.setColour (Colour (0xff7a4a08).withAlpha (0.8f)); g.drawRoundedRectangle (r, rad, 0.8f);
        }
        else
        {
            t.face (g, r, t.keyFace(), 1.0f, rad, ! down);
            if (over) { g.setColour (Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (r, rad); }
        }
        if (getProperties().contains ("swatch"))
        {
            auto chip = r.reduced (5.0f, 5.0f);
            g.setColour (Colour ((uint32) (int) getProperties()["swatch"])); g.fillRoundedRectangle (chip, 2.5f);
            g.setColour (Colours::black.withAlpha (0.45f)); g.drawRoundedRectangle (chip, 2.5f, 0.8f);
            return;
        }
        const Colour tc = on ? Colour (0xff2a1a04) : t.font;
        g.setColour (tc); g.setFont (ui (12.0f, W::Semibold, 0.04f));
        g.drawText (getButtonText(), r, Justification::centred, false);
        if (marked) { g.setColour (on ? Colour (0xff7a4a08) : Theme::amber()); g.fillRoundedRectangle (r.getX() + 8.0f, r.getBottom() - 4.0f, r.getWidth() - 16.0f, 2.0f, 1.0f); }
    }
};

// a bool parameter on a key
class ParamKey : public Key
{
public:
    ParamKey (AudioProcessorValueTreeState& st, const String& id, const String& text) : Key (text)
    {
        setClickingTogglesState (true);
        att = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (st, id, *this);
    }
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> att;
};

// a row of keys that behaves as one control (segmented)
class Segmented : public Component
{
public:
    std::function<void (int)> onPick; std::function<int()> current;
    void build (const StringArray& names)
    {
        keys.clear();
        for (int i = 0; i < names.size(); ++i)
        {
            auto* k = keys.add (new Key (names[i]));
            k->lit = [this, i] { return current && current() == i; };
            k->onClick = [this, i] { if (onPick) onPick (i); repaintAll(); };
            addAndMakeVisible (k);
        }
    }
    int idealWidth() const { int w = 0; for (auto* k : keys) w += k->idealWidth() + 4; return w; }
    void resized() override { int x = 0; for (auto* k : keys) { k->setBounds (x, 0, k->idealWidth(), getHeight()); x += k->idealWidth() + 4; } }
    void repaintAll() { for (auto* k : keys) k->repaint(); }
    OwnedArray<Key> keys;
};
// a choice parameter as a segmented control
class ParamSegmented : public Segmented
{
public:
    ParamSegmented (AudioProcessorValueTreeState& st, const String& id, const StringArray& names)
    {
        build (names);
        current = [this] { return value; };
        if (auto* p = st.getParameter (id))
        {
            pa = std::make_unique<ParameterAttachment> (*p, [this] (float v) { value = roundToInt (v); repaintAll(); }, nullptr);
            pa->sendInitialUpdate();
            onPick = [this] (int i) { pa->setValueAsCompleteGesture ((float) i); };
        }
    }
    int value = 0;
private:
    std::unique_ptr<ParameterAttachment> pa;
};

// knob with its label above and its value below
class KnobBox : public Component
{
public:
    explicit KnobBox (const String& lab) : label (lab)
    {
        knob.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        knob.setPopupDisplayEnabled (true, true, nullptr);
        knob.setMouseDragSensitivity (300);
        knob.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        addAndMakeVisible (knob);
    }
    FineSlider knob; String label; RangedAudioParameter* prm = nullptr; bool onScreen = false;
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
        auto r = getLocalBounds(); r.removeFromTop (16); r.removeFromBottom (18);
        const int d = jmin (r.getWidth(), r.getHeight());
        knob.setBounds (r.withSizeKeepingCentre (d, d));
    }
    void paint (Graphics& g) override
    {
        const auto& t = themeOf (*this);
        const float a = isEnabled() ? 1.0f : 0.35f;
        t.text (g, label, getLocalBounds().removeFromTop (15).toFloat(), Justification::centred, ui (10.5f, W::Semibold, 0.06f), onScreen ? t.led : Colour(), a);
        t.text (g, prm != nullptr ? withUnit (*prm) : String(), getLocalBounds().removeFromBottom (16).toFloat(), Justification::centred, ui (11.5f, W::Medium),
                onScreen || t.style != Style::Studio ? (t.style == Style::Ivory && ! onScreen ? t.font : t.led) : t.font.withAlpha (0.8f), a);
    }
};

// one of the six long faders: name and live value as plain text below
class FaderBox : public Component
{
public:
    FaderBox()
    {
        fader.setSliderStyle (Slider::LinearVertical);
        fader.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        fader.setPopupDisplayEnabled (false, false, nullptr);
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
    void resized() override { auto r = getLocalBounds(); r.removeFromBottom (40); fader.setBounds (r.reduced (0, 10)); }
    void paint (Graphics& g) override
    {
        const auto& t = themeOf (*this);
        auto r = getLocalBounds().removeFromBottom (38).toFloat();
        t.text (g, label, r.removeFromTop (18.0f), Justification::centred, ui (11.0f, W::Semibold, 0.08f));
        t.text (g, prm != nullptr ? withUnit (*prm) : String(), r.removeFromTop (18.0f), Justification::centred, ui (12.5f, W::Medium),
                t.style == Style::Ivory ? Theme::shade (t.font, 0.1f) : t.led);
    }
};

// ============================================================================ main panel
class MainPanel : public Component, private Timer
{
public:
    static constexpr int H = 820, WFull = 1440, WShort = 1092;
    enum { kDefaultId = 1, kBuiltinBase = 2, kUserBase = 2000, kSaveId = 9000, kDeleteId = 9001, kFolderId = 9002 };
    enum Mode { Live = 0, Decay, Screen480 };
    std::function<void()> onSizeChanged;
    std::function<void (float)> onScaleChosen;

    explicit MainPanel (PlateVerbProcessor& p) : proc (p), fft (12)
    {
        auto& st = proc.apvts;
        loadLook();
        editSlot = jlimit (0, pv::kNumSlots - 1, (int) st.state.getProperty ("editSlot", 0));
        drawerOpen = (bool) st.state.getProperty ("drawerOpen", true);
        drawerTab = jlimit (0, 2, (int) st.state.getProperty ("drawerTab", 0));
        mode = jlimit (0, 2, (int) st.state.getProperty ("dispMode", 0));

        // ---- header
        presetBox.onChange = [this] { presetChosen(); };
        presetBox.setTextWhenNothingSelected ("Default");
        addAndMakeVisible (presetBox);
        for (auto* k : { &presetPrev, &presetNext, &presetSave, &advKey, &drawerKey }) addAndMakeVisible (*k);
        presetPrev.setButtonText ("<"); presetNext.setButtonText (">"); presetSave.setButtonText ("SAVE");
        advKey.setButtonText ("ADVANCED"); drawerKey.setButtonText ("EQ  COMP  DELAY");
        presetPrev.onClick = [this] { stepPreset (-1); };
        presetNext.onClick = [this] { stepPreset (1); };
        presetSave.onClick = [this] { savePreset(); };
        advKey.lit = [this] { return advOpen; };
        advKey.onClick = [this] { advOpen = ! advOpen; updateVisibility(); };
        drawerKey.lit = [this] { return drawerOpen; };
        drawerKey.onClick = [this] { setDrawer (! drawerOpen); };

        // ---- algorithms: click = this one only, right-click = add / remove alongside the others
        const int order[] = { pv::Plate, pv::Room, pv::Chamber, pv::Hall, pv::IR };
        const char* names[] = { "PLATE", "ROOM", "CHAMBER", "HALL", "IR PLAYER" };
        for (int i = 0; i < 5; ++i)
        {
            const int s = order[i];
            auto* k = algoKeys.add (new Key (names[i]));
            k->lit = [this, s] { return raw (sid (s, "on")) > 0.5f; };
            k->onClick = [this, s] { soloSlot (s); };
            k->onRightClick = [this, s] { proc.setParam (sid (s, "on"), raw (sid (s, "on")) > 0.5f ? 0.0f : 1.0f); selectSlot (s); repaintKeys(); };
            k->setTooltip ("Click: use this algorithm only.  Right-click: add it to the others (or remove it).");
            addAndMakeVisible (k);
            algoSlot[i] = s;
        }
        // ---- display mode, pages
        modeSeg.build ({ "LIVE", "DECAY", "480L" });
        modeSeg.current = [this] { return mode; };
        modeSeg.onPick = [this] (int m) { mode = m; proc.apvts.state.setProperty ("dispMode", m, nullptr); repaint (dispArea); };
        addAndMakeVisible (modeSeg);
        pageSeg.build ({ "PAGE 1", "PAGE 2" });
        pageSeg.current = [this] { return page2 ? 1 : 0; };
        pageSeg.onPick = [this] (int pg) { page2 = pg == 1; bindFaders(); };
        addAndMakeVisible (pageSeg);

        // ---- faders + output
        for (int i = 0; i < 6; ++i) addAndMakeVisible (faders.add (new FaderBox()));
        outKnob = addKnob (*this, "out", "OUTPUT", false);

        // ---- drawer: EQ / compressor / delay
        drawerSeg.build ({ "EQ", "COMP", "DELAY" });
        drawerSeg.current = [this] { return drawerTab; };
        drawerSeg.onPick = [this] (int i) { drawerTab = i; proc.apvts.state.setProperty ("drawerTab", i, nullptr); updateVisibility(); };
        addAndMakeVisible (drawerSeg);
        const char* bn[] = { "LOW", "LO-MID", "HI-MID", "HIGH" };
        for (int b = 0; b < 4; ++b) { eqF[b] = addKnob (*this, nid ("eqF", b), String (bn[b]) + " FREQ", true); eqG[b] = addKnob (*this, nid ("eqG", b), String (bn[b]) + " GAIN", true); }
        compOn.setButtonText ("COMP"); compOn.setClickingTogglesState (true); addAndMakeVisible (compOn);
        compDuck.setButtonText ("DUCK"); compDuck.setClickingTogglesState (true); addAndMakeVisible (compDuck);
        compType.addItemList (compTypeNames(), 1); addAndMakeVisible (compType);
        compTone.addItemList (StringArray { "Clean", "Vintage" }, 1); addAndMakeVisible (compTone);
        const char* cpn[][2] = { { "cpThr", "THRESHOLD" }, { "cpRatio", "RATIO" }, { "cpAtk", "ATTACK" }, { "cpRel", "RELEASE" }, { "cpMakeup", "MAKEUP" }, { "cpMix", "MIX" } };
        for (int i = 0; i < 6; ++i) compK[i] = addKnob (*this, cpn[i][0], cpn[i][1], true);
        dlOn = std::make_unique<ParamKey> (st, "dlOn", "DELAY"); addAndMakeVisible (*dlOn);
        dlSync = std::make_unique<ParamKey> (st, "dlSync", "SYNC"); addAndMakeVisible (*dlSync);
        dlPing = std::make_unique<ParamKey> (st, "dlPing", "PING-PONG"); addAndMakeVisible (*dlPing);
        const char* dln[][2] = { { "dlTimeL", "TIME L" }, { "dlTimeR", "TIME R" }, { "dlFb", "FEEDBACK" }, { "dlMix", "MIX" },
                                 { "dlAtk", "ATTACK" }, { "dlPanL", "PAN L" }, { "dlPanR", "PAN R" }, { "dlWidth", "WIDTH" },
                                 { "dlLow", "LOW" }, { "dlMid", "MID" }, { "dlMidF", "MID FREQ" }, { "dlHigh", "HIGH" } };
        for (int i = 0; i < 12; ++i) dlK[i] = addKnob (*this, dln[i][0], dln[i][1], false);
        dlDivL.addItemList (divNames(), 1); dlDivR.addItemList (divNames(), 1);
        addAndMakeVisible (dlDivL); addAndMakeVisible (dlDivR);
        dlDivLAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, "dlDivL", dlDivL);
        dlDivRAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, "dlDivR", dlDivR);

        buildIr(); buildAdvanced();

        for (auto* prm : proc.getParameters()) lastValues.add (prm->getValue());
        buildPresetMenu();
        bindSlot (editSlot);
        setSize (designWidth(), H);
        updateVisibility();
        startTimerHz (30);
    }
    ~MainPanel() override { stopTimer(); }
    int designWidth() const { return drawerOpen ? WFull : WShort; }
    FlexLook& getLook() { return look; }
    void showScale (float sc)
    {
        static const float v[] = { 0.75f, 0.9f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };
        int best = 0; for (int i = 0; i < 7; ++i) if (std::abs (v[i] - sc) < std::abs (v[best] - sc)) best = i;
        sizeBox.setSelectedItemIndex (best, dontSendNotification);
    }

    // ---------------------------------------------------------------- painting
    void paint (Graphics& g) override
    {
        const auto& t = look.t;
        // chassis
        g.setGradientFill (ColourGradient (Theme::shade (t.iface, -0.30f), 0, 0, Theme::shade (t.iface, -0.45f), 0, (float) getHeight(), false));
        g.fillAll();
        // header
        t.face (g, headArea.toFloat(), Theme::shade (t.iface, -0.12f), t.ifaceGrad, t.radius() + 2.0f);
        t.text (g, "FLEXIVERB", headArea.toFloat().withTrimmedLeft (22.0f).withWidth (200.0f), Justification::centredLeft, ui (17.0f, W::Bold, 0.32f));
        t.text (g, "DSP " + String (shownDsp, 1) + "%", dspArea.toFloat(), Justification::centredRight, ui (11.0f, W::Medium), t.dimFont());
        // control surface
        t.face (g, surfArea.toFloat(), t.iface, t.ifaceGrad, t.radius() + 2.0f);
        grain (g, surfArea.reduced (2), t.style == Style::Studio ? 0.25f : 0.8f);
        // display
        paintDisplay (g);
        // fader bay
        t.recess (g, bayArea.toFloat(), Theme::shade (t.iface, t.light() ? -0.10f : -0.22f), t.radius() + 1.0f);
        paintMeters (g);
        // drawer
        if (drawerOpen)
        {
            t.face (g, drawerArea.toFloat(), t.side, t.sideGrad, t.radius() + 2.0f);
            grain (g, drawerArea.reduced (2), t.style == Style::Studio ? 0.25f : 0.8f);
            if (drawerTab == 1) paintCompMeters (g);
            if (drawerTab == 0)
                for (int b = 0; b < 3; ++b)
                { g.setColour (Colours::black.withAlpha (0.18f)); g.drawVerticalLine (eqF[b]->getRight() + 1, (float) eqF[b]->getY() + 8.0f, (float) eqG[b]->getBottom() - 8.0f); }
        }
    }

    void paintDisplay (Graphics& g)
    {
        const auto& t = look.t;
        t.recess (g, dispArea.toFloat(), Colour (0xff050505), t.radius() + 1.0f);
        auto in = dispArea.toFloat().reduced (6.0f);
        g.setColour (t.screen()); g.fillRoundedRectangle (in, t.radius());
        if (advOpen || editSlot == pv::IR) { glass (g, in); return; }          // the IR window or ADVANCED covers it
        if (mode == Live) paintLive (g, in.reduced (14.0f, 10.0f));
        else if (mode == Decay) paintDecay (g, in.reduced (14.0f, 10.0f));
        else paint480 (g, in.reduced (18.0f, 12.0f));
        glass (g, in);
    }
    static void glass (Graphics& g, Rectangle<float> r)
    {
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.045f), r.getX(), r.getY(), Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.45f, false));
        g.fillRoundedRectangle (r, 3.0f);
    }

    // ---- LIVE: scrolling three-band level history (left) and a 1/3-octave spectrum (right), from the plugin's output
    void paintLive (Graphics& g, Rectangle<float> r)
    {
        const auto& t = look.t;
        auto head = r.removeFromTop (20.0f);
        t.ledText (g, programName(), head, Justification::centredLeft, ui (13.0f, W::Semibold, 0.06f));
        const Colour bc[3] = { Colour (0xffff7a45), Colour (0xffffc94d), Colour (0xff5ec8ff) };
        const char* nm[] = { "LOW", "MID", "HIGH" };
        for (int i = 0; i < 3; ++i)
        {
            Rectangle<float> c (head.getRight() - 200.0f + (float) i * 66.0f, head.getY(), 64.0f, head.getHeight());
            g.setColour (bc[i]); g.fillEllipse (c.getX(), c.getCentreY() - 3.0f, 6.0f, 6.0f);
            g.setFont (ui (10.5f, W::Semibold, 0.06f)); g.setColour (bc[i].withAlpha (0.9f)); g.drawText (nm[i], c.withTrimmedLeft (10.0f), Justification::centredLeft);
        }
        r.removeFromTop (6.0f);
        auto spec = r.removeFromRight (r.getWidth() * 0.36f);
        r.removeFromRight (18.0f);
        // history
        const float floorDb = -66.0f;
        g.setColour (Colours::white.withAlpha (0.05f));
        for (int k = 1; k < 4; ++k) g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) k / 4.0f), r.getX(), r.getRight());
        const int N = (int) histLen;
        for (int b = 2; b >= 0; --b)
        {
            Path p; bool started = false;
            for (int i = 0; i < N; ++i)
            {
                const float v = hist[b][(size_t) ((histPos + i) % N)];
                const float x = r.getX() + r.getWidth() * (float) i / (float) (N - 1);
                const float y = r.getBottom() - r.getHeight() * jlimit (0.0f, 1.0f, (v - floorDb) / -floorDb);
                if (! started) { p.startNewSubPath (x, y); started = true; } else p.lineTo (x, y);
            }
            Path f (p); f.lineTo (r.getRight(), r.getBottom()); f.lineTo (r.getX(), r.getBottom()); f.closeSubPath();
            g.setGradientFill (ColourGradient (bc[b].withAlpha (0.30f), 0, r.getY(), bc[b].withAlpha (0.02f), 0, r.getBottom(), false));
            g.fillPath (f);
            g.setColour (bc[b]); g.strokePath (p, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        // spectrum bars
        const int nb = (int) bars.size();
        const float bw = spec.getWidth() / (float) nb;
        for (int i = 0; i < nb; ++i)
        {
            const float v = jlimit (0.0f, 1.0f, (bars[(size_t) i] - floorDb) / -floorDb), pk = jlimit (0.0f, 1.0f, (peaks[(size_t) i] - floorDb) / -floorDb);
            auto bar = Rectangle<float> (spec.getX() + (float) i * bw + 1.0f, spec.getBottom() - spec.getHeight() * v, bw - 2.0f, spec.getHeight() * v);
            const Colour c = i < nb / 4 ? bc[0] : i < (nb * 3) / 4 ? bc[1] : bc[2];
            g.setGradientFill (ColourGradient (c.withAlpha (0.95f), 0, spec.getY(), c.withAlpha (0.25f), 0, spec.getBottom(), false));
            g.fillRoundedRectangle (bar, 1.0f);
            g.setColour (c); g.fillRect (bar.getX(), spec.getBottom() - spec.getHeight() * pk - 1.0f, bar.getWidth(), 1.5f);
        }
        g.setFont (ui (9.5f, W::Medium)); g.setColour (Colours::white.withAlpha (0.35f));
        for (auto lab : { std::pair<float, const char*> (0.05f, "50"), { 0.33f, "500" }, { 0.66f, "5k" }, { 0.95f, "20k" } })
            g.drawText (lab.second, (int) (spec.getX() + spec.getWidth() * lab.first) - 15, (int) spec.getBottom() + 1, 30, 11, Justification::centred);
    }

    // ---- DECAY: the edited algorithm's decay in three bands (static impulse response)
    void paintDecay (Graphics& g, Rectangle<float> r, bool withName = true)
    {
        const auto& t = look.t;
        auto head = r.removeFromTop (20.0f);
        if (withName) t.ledText (g, programName(), head, Justification::centredLeft, ui (13.0f, W::Semibold, 0.06f));
        const Colour bc[3] = { Colour (0xffff7a45), Colour (0xffffc94d), Colour (0xff5ec8ff) };
        const char* nm[] = { "LOW", "MID", "HIGH" };
        const float d = raw (sid (editSlot, "decay"));
        const float rt[3] = { d * raw (sid (editSlot, "lowMult")), d, d * raw (sid (editSlot, "highMult")) };
        for (int i = 0; i < 3; ++i)
        {
            Rectangle<float> c (head.getRight() - 330.0f + (float) i * 110.0f, head.getY(), 108.0f, head.getHeight());
            g.setColour (bc[i]); g.fillEllipse (c.getX(), c.getCentreY() - 3.0f, 6.0f, 6.0f);
            g.setFont (ui (10.5f, W::Semibold, 0.06f)); g.drawText (String (nm[i]) + "  " + String (rt[i], 2) + " s", c.withTrimmedLeft (10.0f), Justification::centredLeft);
        }
        r.removeFromTop (6.0f);
        auto axis = r.removeFromBottom (14.0f);
        if (! wave.valid || wave.lo.empty()) { t.ledText (g, "...", r, Justification::centred, ui (14.0f), 0.5f); return; }
        const float total = (float) wave.lo.size() * wave.binMs, floorDb = -80.0f;
        g.setColour (Colours::white.withAlpha (0.05f));
        for (int k = 1; k < 4; ++k) g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) k / 4.0f), r.getX(), r.getRight());
        const float step = total > 6000 ? 2000.0f : total > 2500 ? 1000.0f : 500.0f;
        g.setFont (ui (9.5f, W::Medium));
        for (float ms = 0; ms <= total; ms += step)
        {
            const float x = r.getX() + r.getWidth() * ms / total;
            g.setColour (Colours::white.withAlpha (0.05f)); g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
            g.setColour (Colours::white.withAlpha (0.35f));
            g.drawText (ms >= 1000 ? String (ms / 1000.0f, 0) + " s" : String ((int) ms) + " ms", (int) x - 22, (int) axis.getY() + 1, 44, 12, Justification::centred);
        }
        const std::vector<float>* bv[3] = { &wave.lo, &wave.mid, &wave.hi };
        const int N = (int) r.getWidth();
        for (int b = 0; b < 3; ++b)
        {
            std::vector<float> y ((size_t) N + 1);
            const auto& v = *bv[b];
            for (int k = 0; k <= N; ++k)
            {
                const float m0 = total * (float) k / (float) N, m1 = total * (float) (k + 1) / (float) N;
                int a = jlimit (0, (int) v.size() - 1, (int) (m0 / wave.binMs)), e = jlimit (a, (int) v.size() - 1, (int) (m1 / wave.binMs));
                float pk = 0; for (int i = a; i <= e; ++i) pk = jmax (pk, v[(size_t) i]);
                y[(size_t) k] = jmax (floorDb, 20.0f * std::log10 (pk + 1e-9f));
            }
            Path p;
            for (int k = 0; k <= N; ++k)
            {
                float acc = 0; int c = 0; for (int dd = -4; dd <= 4; ++dd) { const int q = k + dd; if (q >= 0 && q <= N) { acc += y[(size_t) q]; ++c; } }
                const float px = r.getX() + (float) k, py = r.getY() + r.getHeight() * (acc / (float) c) / floorDb;
                if (k == 0) p.startNewSubPath (px, py); else p.lineTo (px, py);
            }
            Path f (p); f.lineTo (r.getRight(), r.getBottom()); f.lineTo (r.getX(), r.getBottom()); f.closeSubPath();
            g.setGradientFill (ColourGradient (bc[b].withAlpha (0.24f), 0, r.getY(), bc[b].withAlpha (0.02f), 0, r.getBottom(), false));
            g.fillPath (f);
            g.setColour (bc[b]); g.strokePath (p, PathStrokeType (1.8f, PathStrokeType::curved, PathStrokeType::rounded));
        }
    }

    // ---- 480L: the remote's alphanumeric display, 14-segment characters with unlit segments showing
    void segLine (Graphics& g, const String& s, Rectangle<float> r, float h, int chars)
    {
        const auto& t = look.t;
        String ghost; for (int i = 0; i < chars; ++i) ghost << "~";
        String txt = s.toUpperCase().replace (" ", "!").substring (0, chars); while (txt.length() < chars) txt << "!";   // '!' = a blank cell in DSEG
        g.setFont (segFont (h));
        g.setColour (t.led.withAlpha (0.07f)); g.drawText (ghost, r, Justification::centredLeft, false);
        t.ledText (g, txt, r, Justification::centredLeft, segFont (h));
    }
    void paint480 (Graphics& g, Rectangle<float> r)
    {
        const auto& t = look.t;
        const int cfg = (int) raw ("config");
        const String mach = cfg == pv::CfgSingle ? "A" : (editSlot == (int) raw ("machB") ? "B" : "A");
        auto top = r.removeFromTop (r.getHeight() * 0.36f);
        segLine (g, mach + " " + programName(), top, top.getHeight() * 0.62f, 24);
        r.removeFromTop (8.0f);
        g.setColour (t.led.withAlpha (0.18f)); g.drawHorizontalLine ((int) r.getY(), r.getX(), r.getRight());
        r.removeFromTop (10.0f);
        const float cw = r.getWidth() / 6.0f;
        for (int i = 0; i < 6; ++i)
        {
            auto c = Rectangle<float> (r.getX() + (float) i * cw, r.getY(), cw - 10.0f, r.getHeight());
            t.ledText (g, abbrev[i], c.removeFromTop (22.0f), Justification::centredLeft, ui (12.0f, W::Semibold, 0.1f), 0.7f);
            String v = faders[i]->prm != nullptr ? faders[i]->prm->getCurrentValueAsText() : String();
            v = v.replace (" ", "").replace ("%", "").substring (0, 5);
            segLine (g, v, c.removeFromTop (c.getHeight() * 0.7f), jmin (34.0f, cw * 0.19f), 5);
        }
    }

    void paintMeters (Graphics& g)
    {
        const auto& t = look.t;
        const float lv[3] = { proc.meterIn.load(), proc.meterMain.load(), proc.meterAux.load() };
        const char* nm[] = { "IN", "MAIN", "AUX" };
        for (int m = 0; m < 3; ++m)
        {
            auto r = meterArea[m].toFloat();
            t.text (g, nm[m], r.removeFromBottom (18.0f), Justification::centred, ui (10.5f, W::Semibold, 0.08f));
            r = r.withSizeKeepingCentre (14.0f, r.getHeight());
            t.recess (g, r, Colour (0xff070707), 2.0f);
            const float db = 20.0f * std::log10 (lv[m] + 1e-9f);
            const int segs = 24; const float sh = (r.getHeight() - 4.0f) / (float) segs;
            for (int i = 0; i < segs; ++i)
            {
                const float thr = -48.0f + 2.0f * (float) i;
                const Colour c = i >= segs - 1 ? Colour (0xffff3b1f) : i >= segs - 5 ? Colour (0xffffb43c) : Colour (0xff4fd06a);
                auto sr = Rectangle<float> (r.getX() + 3.0f, r.getBottom() - 2.0f - (float) (i + 1) * sh + 1.0f, r.getWidth() - 6.0f, sh - 1.5f);
                g.setColour (db >= thr ? c : c.withAlpha (0.08f)); g.fillRoundedRectangle (sr, 0.8f);
            }
        }
        if (! auxBusNote.isEmpty()) t.text (g, auxBusNote, Rectangle<int> (meterArea[0].getX() - 10, meterArea[0].getY() - 22, 200, 16).toFloat(), Justification::centredLeft, ui (10.0f), t.dimFont());
    }

    void paintCompMeters (Graphics& g)
    {
        const auto& t = look.t;
        const float lo = -80.0f;
        const float inDb = jlimit (lo, 0.0f, proc.compIn[editSlot].load());
        const float grDb = jlimit (0.0f, 30.0f, proc.compGr[editSlot].load());
        const float thr  = raw (sid (editSlot, "cpThr"));
        auto in = inMeter.toFloat(), gr = grMeter.toFloat();
        t.recess (g, in, Colour (0xff070707), 2.0f); t.recess (g, gr, Colour (0xff070707), 2.0f);
        const float inH = (in.getHeight() - 4.0f) * (inDb - lo) / -lo;
        g.setColour (Colour (0xff4fd06a)); g.fillRect (in.getX() + 3.0f, in.getBottom() - 2.0f - inH, in.getWidth() - 6.0f, inH);
        const float ty = in.getBottom() - in.getHeight() * (jlimit (lo, 0.0f, thr) - lo) / -lo;
        g.setColour (Theme::amber()); g.fillRect (in.getX() - 4.0f, ty - 1.0f, in.getWidth() + 8.0f, 2.0f);
        const float grH = (gr.getHeight() - 4.0f) * grDb / 30.0f;
        g.setColour (Theme::amber()); g.fillRect (gr.getX() + 3.0f, gr.getY() + 2.0f, gr.getWidth() - 6.0f, grH);
        t.text (g, raw (sid (editSlot, "cpDuck")) > 0.5f ? "DRY" : "IN", Rectangle<int> (inMeter.getX() - 14, inMeter.getBottom() + 4, inMeter.getWidth() + 28, 14).toFloat(), Justification::centred, ui (10.0f, W::Semibold));
        t.text (g, "GR", Rectangle<int> (grMeter.getX() - 14, grMeter.getBottom() + 4, grMeter.getWidth() + 28, 14).toFloat(), Justification::centred, ui (10.0f, W::Semibold));
    }

    // ---------------------------------------------------------------- layout
    void resized() override
    {
        const int Wd = getWidth();
        headArea = { 14, 12, Wd - 28, 44 };
        surfArea = { 14, 66, 1064, 742 };
        drawerArea = { 1090, 66, 336, 742 };
        // header (right to left): ADVANCED, drawer key, then the preset row in the middle
        const int hy = headArea.getCentreY() - kKeyH / 2;
        int x = headArea.getRight() - 14;
        advKey.setBounds (x - advKey.idealWidth(), hy, advKey.idealWidth(), kKeyH); x -= advKey.idealWidth() + 8;
        drawerKey.setBounds (x - drawerKey.idealWidth(), hy, drawerKey.idealWidth(), kKeyH); x -= drawerKey.idealWidth() + 18;
        dspArea = { x - 80, hy, 80, kKeyH }; x -= 96;
        presetSave.setBounds (x - presetSave.idealWidth(), hy, presetSave.idealWidth(), kKeyH); x -= presetSave.idealWidth() + 6;
        presetNext.setBounds (x - 30, hy, 30, kKeyH); x -= 34;
        presetPrev.setBounds (x - 30, hy, 30, kKeyH); x -= 36;
        const int pw = jmin (380, x - (headArea.getX() + 200));
        presetBox.setBounds (x - pw, hy, pw, kKeyH);
        // algorithm row + display mode
        int ax = surfArea.getX() + 16; const int ay = surfArea.getY() + 14;
        for (auto* k : algoKeys) { k->setBounds (ax, ay, k->idealWidth(), kKeyH); ax += k->idealWidth() + 6; }
        modeSeg.setBounds (surfArea.getRight() - 16 - modeSeg.idealWidth(), ay, modeSeg.idealWidth(), kKeyH);
        // display
        dispArea = { surfArea.getX() + 16, ay + kKeyH + 14, surfArea.getWidth() - 32, 300 };
        // page row
        const int py = dispArea.getBottom() + 12;
        pageSeg.setBounds (surfArea.getX() + 16, py, pageSeg.idealWidth(), kKeyH);
        // fader bay
        bayArea = { surfArea.getX() + 16, py + kKeyH + 10, surfArea.getWidth() - 32, surfArea.getBottom() - 16 - (py + kKeyH + 10) };
        const int fw = 104;
        for (int i = 0; i < 6; ++i) faders[i]->setBounds (bayArea.getX() + 14 + i * fw, bayArea.getY() + 8, fw - 6, bayArea.getHeight() - 14);
        const int mx = bayArea.getX() + 14 + 6 * fw + 40;
        for (int m = 0; m < 3; ++m) meterArea[m] = { mx + m * 44, bayArea.getY() + 30, 40, bayArea.getHeight() - 50 };
        outKnob->setBounds (mx + 160, bayArea.getY() + 40, 110, 128);
        // IR window and ADVANCED cover the display (and ADVANCED also the faders)
        irWin.setBounds (dispArea.reduced (6));
        advWin.setBounds (dispArea.getX(), dispArea.getY(), dispArea.getWidth(), bayArea.getBottom() - dispArea.getY());
        layoutIr(); layoutAdvanced();
        // drawer
        layoutDrawer();
    }

    void layoutDrawer()
    {
        const int dx = drawerArea.getX(), dy = drawerArea.getY();
        drawerSeg.setBounds (dx + 16, dy + 14, drawerSeg.idealWidth(), kKeyH);
        const int y0 = dy + 62, cw = (drawerArea.getWidth() - 24) / 4;
        for (int b = 0; b < 4; ++b) { eqF[b]->setBounds (dx + 12 + b * cw, y0, cw, 110); eqG[b]->setBounds (dx + 12 + b * cw, y0 + 130, cw, 110); }
        compOn.setBounds (dx + 16, y0, compOn.idealWidth(), kKeyH);
        compDuck.setBounds (compOn.getRight() + 6, y0, compDuck.idealWidth(), kKeyH);
        compType.setBounds (dx + 16, y0 + 40, 180, kKeyH);
        compTone.setBounds (dx + 204, y0 + 40, 110, kKeyH);
        const int kw = 82;
        for (int i = 0; i < 6; ++i) compK[i]->setBounds (dx + 12 + (i % 3) * kw, y0 + 84 + (i / 3) * 120, kw, 110);
        inMeter = { dx + 268, y0 + 96, 14, 200 }; grMeter = { dx + 300, y0 + 96, 14, 200 };
        dlOn->setBounds (dx + 16, y0, dlOn->idealWidth(), kKeyH);
        dlSync->setBounds (dlOn->getRight() + 6, y0, dlSync->idealWidth(), kKeyH);
        dlPing->setBounds (dlSync->getRight() + 6, y0, dlPing->idealWidth(), kKeyH);
        for (int i = 0; i < 4; ++i) dlK[i]->setBounds (dx + 12 + i * cw, y0 + 44, cw, 104);
        dlDivL.setBounds (dx + 16, y0 + 84, cw - 8, kKeyH); dlDivR.setBounds (dx + 12 + cw + 4, y0 + 84, cw - 8, kKeyH);
        for (int i = 0; i < 4; ++i) dlK[4 + i]->setBounds (dx + 12 + i * cw, y0 + 168, cw, 104);
        for (int i = 0; i < 4; ++i) dlK[8 + i]->setBounds (dx + 12 + i * cw, y0 + 292, cw, 104);
    }

    void updateVisibility()
    {
        const bool open = drawerOpen;
        drawerSeg.setVisible (open);
        const bool eq = open && drawerTab == 0, cp = open && drawerTab == 1, dl = open && drawerTab == 2;
        for (int b = 0; b < 4; ++b) { eqF[b]->setVisible (eq); eqG[b]->setVisible (eq); }
        compOn.setVisible (cp); compDuck.setVisible (cp); compType.setVisible (cp); compTone.setVisible (cp);
        for (auto* k : compK) k->setVisible (cp);
        dlOn->setVisible (dl); dlSync->setVisible (dl); dlPing->setVisible (dl);
        const bool sync = raw ("dlSync") > 0.5f;
        for (int i = 0; i < 12; ++i) dlK[i]->setVisible (dl && ! (sync && i < 2));
        dlDivL.setVisible (dl && sync); dlDivR.setVisible (dl && sync);
        irWin.setVisible (editSlot == pv::IR && ! advOpen);
        advWin.setVisible (advOpen);
        const bool facesVisible = ! advOpen;
        for (auto* f : faders) f->setVisible (facesVisible);
        pageSeg.setVisible (facesVisible);
        outKnob->setVisible (facesVisible);
        modeSeg.setVisible (editSlot != pv::IR && ! advOpen);
        if (advOpen) advWin.toFront (false);
        advKey.repaint(); drawerKey.repaint(); drawerSeg.repaintAll();
        repaint();
    }
    void setDrawer (bool o)
    {
        drawerOpen = o;
        proc.apvts.state.setProperty ("drawerOpen", o, nullptr);
        setSize (designWidth(), H);
        resized(); updateVisibility();
        if (onSizeChanged) onSizeChanged();
    }

   #if FLEX_TEST
    void debugShow (int style, int slot, int dmode, bool adv, int advTab, bool drawer, int dtab, bool sig)
    {
        look.t.defaults ((Style) style); lookChanged (false);
        selectSlot (slot); mode = dmode; advOpen = adv; advTabIdx = advTab; drawerTab = dtab; setDrawer (drawer);
        updateAdvTabs(); updateVisibility();
        if (sig)
        {   // fill the live display with a believable history
            Random rnd (3);
            for (int i = 0; i < (int) histLen; ++i)
            {
                const float ph = (float) i / (float) histLen;
                const float env = -10.0f - 40.0f * std::fmod (ph * 3.0f, 1.0f);
                hist[0][(size_t) i] = env + 4.0f; hist[1][(size_t) i] = env + 1.0f + rnd.nextFloat(); hist[2][(size_t) i] = env - 8.0f + 2.0f * rnd.nextFloat();
            }
            histPos = 0;
            for (size_t i = 0; i < bars.size(); ++i) { bars[i] = -12.0f - 30.0f * (float) i / (float) bars.size() + 6.0f * std::sin ((float) i * 0.9f); peaks[i] = bars[i] + 4.0f; }
        }
        wave = proc.getWave (editSlot);
        repaint();
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
    void repaintKeys() { for (auto* k : algoKeys) k->repaint(); }

    String programName()
    {
        if (proc.userPresetName.isNotEmpty()) return proc.userPresetName.toUpperCase();
        const int cp = proc.getCurrentProgram();
        if (cp > 0 && kPresets[cp - 1].slot == editSlot) return String (kPresets[cp - 1].name).toUpperCase();
        const char* names[] = { "PLATE", "ROOM", "CHAMBER", "HALL", "IR PLAYER", "RANDOM HALL" };
        return names[editSlot];
    }

    // click: this algorithm only (everything else off); right-click adds it instead
    void soloSlot (int s)
    {
        for (int k = 0; k < pv::kNumSlots; ++k) proc.setParam (sid (k, "on"), k == s ? 1.0f : 0.0f);
        selectSlot (s);
        repaintKeys();
    }

    void bindFaders()
    {
        struct F { const char* id; const char* lab; const char* ab; bool global; };
        const bool is480 = editSlot == pv::Lex480, isIr = editSlot == pv::IR;
        std::vector<F> f;
        if (isIr)
            f = page2 ? std::vector<F> { { "lowMult", "LOW DECAY", "LOW", false }, { "highMult", "HIGH DECAY", "HIGH", false }, { "size", "SIZE", "SIZ", false },
                                         { "build", "FADE IN", "FIN", false }, { "damping", "DAMPING", "DMP", false }, { "tone", "TONE", "TON", false } }
                      : std::vector<F> { { "decay", "DECAY", "RTM", false }, { "predelay", "PRE-DELAY", "PDL", false }, { "early", "EARLY", "ERL", false },
                                         { "inWidth", "WIDTH", "WID", false }, { "lv", "LEVEL", "LVL", false }, { "mix", "MIX", "MIX", true } };
        else
            f = page2 ? std::vector<F> { { "early", "SHAPE", "SHP", false }, { "build", "SPREAD", "SPR", false }, { "diff", "DIFFUSION", "DIF", false },
                                         { is480 ? "spin" : "damping", is480 ? "SPIN" : "DAMPING", is480 ? "SPN" : "DMP", false },
                                         { "mod", is480 ? "WANDER" : "MODULATION", is480 ? "WAN" : "MOD", false }, { "lv", "LEVEL", "LVL", false } }
                      : std::vector<F> { { "decay", "RT MID", "RTM", false }, { "size", "SIZE", "SIZ", false }, { "predelay", "PREDELAY", "PDL", false },
                                         { "lowMult", "BASS MULT", "BAS", false }, { "highMult", "TREBLE MULT", "TRB", false }, { "mix", "WET/DRY MIX", "MIX", true } };
        for (int i = 0; i < 6; ++i)
        {
            faders[i]->attach (proc.apvts, f[(size_t) i].global ? String (f[(size_t) i].id) : sid (editSlot, f[(size_t) i].id), f[(size_t) i].lab);
            abbrev[i] = f[(size_t) i].ab;
        }
        pageSeg.repaintAll();
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
        compOnAtt.reset(); compOnAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, sid (s, "cpOn"), compOn);
        compDuckAtt.reset(); compDuckAtt = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, sid (s, "cpDuck"), compDuck);
        matMixAtt.reset(); matMixAtt = std::make_unique<APVTS::SliderAttachment> (proc.apvts, sid (s, "matMix"), matMix);
        proc.viewSlot.store (s);
        proc.apvts.state.setProperty ("editSlot", s, nullptr);
        lastWaveVersion = -1;
        for (int i = 0; i < algoKeys.size(); ++i) algoKeys[i]->marked = algoSlot[i] == s;
        if (spinK != nullptr) spinK->setEnabled (s == pv::Lex480);
        bindFaders();
        updateVisibility();
        repaintKeys();
    }
    void selectSlot (int s) { if (s != editSlot) bindSlot (s); }

    // ---------------------------------------------------------------- IR window (its own window in the display)
    struct Panel : Component
    {
        MainPanel& m; bool overlay; explicit Panel (MainPanel& mp, bool ov) : m (mp), overlay (ov) {}
        std::vector<std::pair<Rectangle<int>, String>> captions; String title;
        std::function<void (Graphics&)> extra;
        void paint (Graphics& g) override
        {
            const auto& t = m.look.t;
            auto r = getLocalBounds().toFloat();
            if (overlay)
            {
                t.face (g, r, Theme::shade (t.iface, t.light() ? -0.04f : 0.04f), t.ifaceGrad, t.radius() + 1.0f, false);
                grain (g, getLocalBounds().reduced (2), t.style == Style::Studio ? 0.25f : 0.8f);
            }
            else { g.setColour (t.screen()); g.fillRoundedRectangle (r, t.radius()); }
            if (title.isNotEmpty())
                (overlay ? t.text (g, title, r.reduced (18.0f, 0.0f).removeFromTop (44.0f), Justification::centredLeft, ui (13.0f, W::Bold, 0.18f))
                         : t.ledText (g, title, r.reduced (16.0f, 0.0f).removeFromTop (38.0f), Justification::centredLeft, ui (13.0f, W::Bold, 0.12f)));
            for (auto& c : captions)
                (overlay ? t.text (g, c.second, c.first.toFloat(), Justification::centredLeft, ui (11.0f, W::Medium, 0.02f), t.dimFont())
                         : t.ledText (g, c.second, c.first.toFloat(), Justification::centredLeft, ui (11.5f, W::Medium), 0.85f));
            if (extra) extra (g);
        }
    };
    Panel irWin { *this, false }, advWin { *this, true };
    Key irLoad { "LOAD" }, irPrev { "<" }, irNext { ">" }, irClear { "CLEAR" };
    std::unique_ptr<ParamKey> irReverse, irPitch;
    KnobBox* irK[8] {};

    void buildIr()
    {
        irWin.title = "IR PLAYER";
        for (auto* b : { &irLoad, &irPrev, &irNext, &irClear }) irWin.addAndMakeVisible (*b);
        irLoad.onClick = [this] { chooseIr(); };
        irPrev.onClick = [this] { stepIr (-1); };
        irNext.onClick = [this] { stepIr (1); };
        irClear.onClick = [this] { proc.clearIr(); };
        const char* irk[][2] = { { "irStart", "START" }, { "irXover", "ER / LR" }, { "irFadeOut", "FADE OUT" }, { "irXfeed", "X-FEED" },
                                 { "irXdelay", "X-DELAY" }, { "irAsym", "ASYMMETRY" } };
        for (int i = 0; i < 6; ++i) { irK[i] = addKnob (irWin, irk[i][0], irk[i][1], false); irK[i]->onScreen = true; }
        irK[6] = addKnob (irWin, "diff", "DIFFUSION", true); irK[6]->onScreen = true;
        irK[7] = addKnob (irWin, "mod", "MODULATION", true); irK[7]->onScreen = true;
        irReverse = std::make_unique<ParamKey> (proc.apvts, "irReverse", "REVERSE"); irWin.addAndMakeVisible (*irReverse);
        irPitch = std::make_unique<ParamKey> (proc.apvts, "irPitch", "STRETCH PITCH"); irWin.addAndMakeVisible (*irPitch);
        irWin.extra = [this] (Graphics& g) { paintIrDecay (g); };
        addChildComponent (irWin);
    }
    void layoutIr()
    {
        irWin.captions.clear();
        int x = 16; const int y = 44;
        for (auto* b : { &irLoad, &irPrev, &irNext, &irClear }) { b->setBounds (x, y, b->idealWidth(), kKeyH); x += b->idealWidth() + 6; }
        irWin.captions.push_back ({ { x + 12, y, 420, kKeyH }, {} });
        for (int i = 0; i < 8; ++i) irK[i]->setBounds (10 + (i % 4) * 92, 86 + (i / 4) * 96, 92, 92);
        irReverse->setBounds (16, irWin.getHeight() - 8 - kKeyH, irReverse->idealWidth(), kKeyH);
        irPitch->setBounds (irReverse->getRight() + 6, irWin.getHeight() - 8 - kKeyH, irPitch->idealWidth(), kKeyH);
    }
    void paintIrDecay (Graphics& g)
    {
        auto r = irWin.getLocalBounds().toFloat().withTrimmedLeft (400.0f).reduced (16.0f, 12.0f).withTrimmedTop (32.0f);
        paintDecay (g, r, false);
    }

    // ---------------------------------------------------------------- ADVANCED (reverb details, hardware, outputs, look)
    Segmented advTabs; int advTabIdx = 0;
    ComboBox matA, matB, conv, osBox, trModel, machA, machB, sizeBox;
    Slider matMix;
    std::unique_ptr<APVTS::ComboBoxAttachment> matAAtt, matBAtt, convAtt, osAtt, trModelAtt, compTypeAtt, compToneAtt, dlDivLAtt, dlDivRAtt, machAAtt, machBAtt;
    std::unique_ptr<APVTS::SliderAttachment> matMixAtt;
    std::unique_ptr<APVTS::ButtonAttachment> compOnAtt, compDuckAtt;
    KnobBox* advK[9] {}; KnobBox* spinK = nullptr; KnobBox* trK[2] {};
    std::unique_ptr<ParamKey> hwOn, hwXfIn, hwXfOut, hwNoise, trOn, auxMain;
    std::unique_ptr<ParamSegmented> hwIn, hwOut, cfgSeg;
    Segmented styleSeg;
    struct ColourRow { String name; std::function<Colour&()> col; std::function<float&()> grad; OwnedArray<Key> sw; Key more { "..." }; Slider gradS; };
    OwnedArray<ColourRow> colourRows;
    Slider depthS, contrastS;
    std::vector<Component*> tabComps[4];

    void buildAdvanced()
    {
        auto& st = proc.apvts; auto& w = advWin;
        advTabs.build ({ "REVERB", "HARDWARE", "OUTPUTS", "LOOK" });
        advTabs.current = [this] { return advTabIdx; };
        advTabs.onPick = [this] (int i) { advTabIdx = i; updateAdvTabs(); };
        w.addAndMakeVisible (advTabs);
        // REVERB
        const char* ids[][2] = { { "damping", "DAMPING" }, { "tone", "INPUT TONE" }, { "hiCut", "HIGH CUT" }, { "inWidth", "WIDTH" },
                                 { "xover", "BASS XOVER" }, { "dirt", "DIRT" }, { "rich", "RICHNESS" }, { "trim", "TRIM" }, { "diff", "DIFFUSION" } };
        for (int i = 0; i < 9; ++i) { advK[i] = addKnob (w, ids[i][0], ids[i][1], true); tabComps[0].push_back (advK[i]); }
        spinK = addKnob (w, "spin", "SPIN", true); tabComps[0].push_back (spinK);
        conv.addItemList (kConverterNames, 1);
        for (auto* c : { (Component*) &matA, (Component*) &matB, (Component*) &conv }) { w.addAndMakeVisible (c); tabComps[0].push_back (c); }
        matMix.setSliderStyle (Slider::LinearHorizontal); matMix.setTextBoxStyle (Slider::NoTextBox, false, 0, 0); w.addAndMakeVisible (matMix); tabComps[0].push_back (&matMix);
        // HARDWARE
        hwOn = std::make_unique<ParamKey> (st, "hwOn", "ANALOG HARDWARE");
        hwIn = std::make_unique<ParamSegmented> (st, "hwIn", StringArray { "IN ANALOG", "IN DIGITAL" });
        hwOut = std::make_unique<ParamSegmented> (st, "hwOut", StringArray { "OUT ANALOG", "OUT DIGITAL" });
        hwXfIn = std::make_unique<ParamKey> (st, "hwXfIn", "INPUT TRANSFORMERS");
        hwXfOut = std::make_unique<ParamKey> (st, "hwXfOut", "OUTPUT TRANSFORMERS");
        hwNoise = std::make_unique<ParamKey> (st, "hwNoise", "CONVERTER NOISE");
        trOn = std::make_unique<ParamKey> (st, "trOn", "OUTPUT IRON");
        osBox.addItemList (osNames(), 1); osAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, "os", osBox);
        trModel.addItemList (trNames(), 1); trModelAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, "trModel", trModel);
        trK[0] = addKnob (w, "trFreq", "IRON FREQ", false); trK[1] = addKnob (w, "trAmt", "IRON AMOUNT", false);
        for (auto* c : std::initializer_list<Component*> { hwOn.get(), hwIn.get(), hwOut.get(), hwXfIn.get(), hwXfOut.get(), hwNoise.get(), trOn.get(), &osBox, &trModel, trK[0], trK[1] })
        { w.addAndMakeVisible (c); tabComps[1].push_back (c); }
        // OUTPUTS
        cfgSeg = std::make_unique<ParamSegmented> (st, "config", StringArray { "SINGLE", "CASCADE", "STEREO SPLIT", "MONO SPLIT" });
        machA.addItemList (slotChoiceNames(), 1); machB.addItemList (slotChoiceNames(), 1);
        machAAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, "machA", machA);
        machBAtt = std::make_unique<APVTS::ComboBoxAttachment> (st, "machB", machB);
        auxMain = std::make_unique<ParamKey> (st, "auxToMain", "AUX INTO MAIN");
        for (auto* c : std::initializer_list<Component*> { cfgSeg.get(), &machA, &machB, auxMain.get() }) { w.addAndMakeVisible (c); tabComps[2].push_back (c); }
        // LOOK
        styleSeg.build ({ "REMOTE", "STUDIO", "IVORY" });
        styleSeg.current = [this] { return (int) look.t.style; };
        styleSeg.onPick = [this] (int s) { look.t.defaults ((Style) s); syncLookControls(); lookChanged(); };
        w.addAndMakeVisible (styleSeg); tabComps[3].push_back (&styleSeg);
        auto addRow = [this, &w] (const String& name, std::function<Colour&()> col, std::function<float&()> grad, std::initializer_list<uint32> sw)
        {
            auto* r = colourRows.add (new ColourRow());
            r->name = name; r->col = col; r->grad = grad;
            for (auto c : sw)
            {
                auto* k = r->sw.add (new Key());
                const Colour cc (c);
                k->lit = [r, cc] { return r->col() == cc; };
                k->onClick = [this, r, cc] { r->col() = cc; lookChanged(); };
                k->getProperties().set ("swatch", (int) c);
                w.addAndMakeVisible (k); tabComps[3].push_back (k);
            }
            r->more.onClick = [this, r] { pickColour (r); };
            w.addAndMakeVisible (r->more); tabComps[3].push_back (&r->more);
            if (grad)
            {
                r->gradS.setSliderStyle (Slider::LinearHorizontal); r->gradS.setTextBoxStyle (Slider::NoTextBox, false, 0, 0); r->gradS.setRange (0.0, 1.0, 0.01);
                r->gradS.onValueChange = [this, r] { r->grad() = (float) r->gradS.getValue(); lookChanged(); };
                w.addAndMakeVisible (r->gradS); tabComps[3].push_back (&r->gradS);
            }
        };
        addRow ("INTERFACE", [this]() -> Colour& { return look.t.iface; }, [this]() -> float& { return look.t.ifaceGrad; },
                { 0xff2b2c2e, 0xff1e1f22, 0xff3d3a35, 0xff1d2733, 0xffd9d1bf, 0xffb8b2a6 });
        addRow ("SIDE PANEL", [this]() -> Colour& { return look.t.side; }, [this]() -> float& { return look.t.sideGrad; },
                { 0xff222325, 0xff17181a, 0xff4a2e22, 0xff2a3a2e, 0xffc9bfa9, 0xff5b5f66 });
        addRow ("LETTERING", [this]() -> Colour& { return look.t.font; }, [this]() -> float& { return look.t.fontGrad; },
                { 0xffe9e4d8, 0xfff2f2f4, 0xffd8b66a, 0xffc0c4c8, 0xff3a342b, 0xff141414 });
        addRow ("DISPLAY", [this]() -> Colour& { return look.t.led; }, nullptr,
                { 0xffff3b1f, 0xffffb43c, 0xff5fe07a, 0xff5ec8ff, 0xfff2f2f4, 0xffff5ea8 });
        for (auto* s : { &depthS, &contrastS })
        {
            s->setSliderStyle (Slider::LinearHorizontal); s->setTextBoxStyle (Slider::NoTextBox, false, 0, 0); s->setRange (0.0, 1.0, 0.01);
            w.addAndMakeVisible (s); tabComps[3].push_back (s);
        }
        depthS.onValueChange = [this] { look.t.depth = (float) depthS.getValue(); lookChanged(); };
        contrastS.onValueChange = [this] { look.t.contrast = (float) contrastS.getValue(); lookChanged(); };
        sizeBox.addItemList (StringArray { "75 %", "90 %", "100 %", "125 %", "150 %", "175 %", "200 %" }, 1);
        sizeBox.onChange = [this] { static const float sc[] = { 0.75f, 0.9f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f }; const int i = sizeBox.getSelectedItemIndex(); if (i >= 0 && onScaleChosen) onScaleChosen (sc[i]); };
        w.addAndMakeVisible (sizeBox); tabComps[3].push_back (&sizeBox);
        syncLookControls();
        addChildComponent (advWin);
        updateAdvTabs();
    }
    void syncLookControls()
    {
        for (auto* r : colourRows) if (r->grad) r->gradS.setValue (r->grad(), dontSendNotification);
        depthS.setValue (look.t.depth, dontSendNotification); contrastS.setValue (look.t.contrast, dontSendNotification);
    }
    void updateAdvTabs()
    {
        for (int i = 0; i < 4; ++i) for (auto* c : tabComps[i]) c->setVisible (i == advTabIdx);
        layoutAdvanced(); advTabs.repaintAll(); advWin.repaint();
    }
    void layoutAdvanced()
    {
        auto& w = advWin; w.captions.clear(); w.title = {};
        w.extra = nullptr;
        advTabs.setBounds (18, 14, advTabs.idealWidth(), kKeyH);
        const int W0 = w.getWidth();
        const int y0 = 64;
        if (advTabIdx == 0)
        {
            const int cw = (W0 - 36) / 5;
            for (int i = 0; i < 10; ++i) { KnobBox* k = i < 9 ? advK[i] : spinK; k->setBounds (18 + (i % 5) * cw, y0 + (i / 5) * 140, cw, 124); }
            const int y = y0 + 300;
            w.captions.push_back ({ { 24, y, 240, 16 }, "MATERIAL A" });  matA.setBounds (24, y + 20, 260, kKeyH);
            w.captions.push_back ({ { 300, y, 240, 16 }, "BLEND A / B" }); matMix.setBounds (300, y + 20, 200, kKeyH);
            w.captions.push_back ({ { 516, y, 240, 16 }, "MATERIAL B" });  matB.setBounds (516, y + 20, 260, kKeyH);
            w.captions.push_back ({ { 24, y + 66, 240, 16 }, "CONVERTER" }); conv.setBounds (24, y + 86, 260, kKeyH);
            w.captions.push_back ({ { 24, w.getHeight() - 34, W0 - 48, 16 }, "Spin applies to the random hall behind the 480L programs. The IR player never passes through any of this." });
        }
        else if (advTabIdx == 1)
        {
            hwOn->setBounds (24, y0, hwOn->idealWidth(), kKeyH);
            hwIn->setBounds (24, y0 + 46, hwIn->idealWidth(), kKeyH);
            hwOut->setBounds (hwIn->getRight() + 24, y0 + 46, hwOut->idealWidth(), kKeyH);
            hwXfIn->setBounds (24, y0 + 92, hwXfIn->idealWidth(), kKeyH);
            hwXfOut->setBounds (hwXfIn->getRight() + 8, y0 + 92, hwXfOut->idealWidth(), kKeyH);
            hwNoise->setBounds (hwXfOut->getRight() + 8, y0 + 92, hwNoise->idealWidth(), kKeyH);
            w.captions.push_back ({ { 24, y0 + 138, 140, kKeyH }, "OVERSAMPLING" }); osBox.setBounds (140, y0 + 138, 90, kKeyH);
            w.captions.push_back ({ { 24, y0 + 190, W0 - 48, 16 }, "Input:  XLR > transformer (option) > LM833 receiver > 9-pole 20 kHz filter > 18-bit gain-ranging A/D" });
            w.captions.push_back ({ { 24, y0 + 210, W0 - 48, 16 }, "Output: 18-bit D/A (PCM53 + 2 LSB) > sample & hold > 9-pole 20 kHz filter > LLA5200 line driver > transformer (option)" });
            w.captions.push_back ({ { 24, y0 + 230, W0 - 48, 16 }, "Applies to the algorithms only. The dry signal and the IR player never pass through it." });
            trOn->setBounds (24, y0 + 290, trOn->idealWidth(), kKeyH);
            trModel.setBounds (trOn->getRight() + 12, y0 + 290, 280, kKeyH);
            trK[0]->setBounds (trModel.getRight() + 30, y0 + 250, 100, 120); trK[1]->setBounds (trModel.getRight() + 140, y0 + 250, 100, 120);
            w.captions.push_back ({ { 24, y0 + 324, W0 - 48, 16 }, "Output iron: a transformer on the whole output (dry and wet), off by default." });
        }
        else if (advTabIdx == 2)
        {
            cfgSeg->setBounds (24, y0, cfgSeg->idealWidth(), kKeyH);
            w.captions.push_back ({ { 24, y0 + 46, 120, kKeyH }, "MACHINE A" }); machA.setBounds (120, y0 + 46, 170, kKeyH);
            w.captions.push_back ({ { 310, y0 + 46, 120, kKeyH }, "MACHINE B" }); machB.setBounds (406, y0 + 46, 170, kKeyH);
            auxMain->setBounds (600, y0 + 46, auxMain->idealWidth(), kKeyH);
            const char* lines[] = { "Single:  every algorithm that is on, the same on Main and Aux.",
                                    "Cascade:  A feeds B.  Main = A into B,  Aux = A only.",
                                    "Stereo Split:  both machines get the stereo input.  Main = A,  Aux = B.",
                                    "Mono Split:  left input > A,  right input > B.  Main = A,  Aux = B.",
                                    "Aux is the plugin's second stereo output (3-4). Enable it in Studio One to give Aux its own channel;",
                                    "without it, Aux into Main mixes machine B into the main output." };
            for (int i = 0; i < 6; ++i) w.captions.push_back ({ { 24, y0 + 104 + i * 22, W0 - 48, 18 }, lines[i] });
        }
        else
        {
            styleSeg.setBounds (140, y0, styleSeg.idealWidth(), kKeyH);
            w.captions.push_back ({ { 24, y0, 110, kKeyH }, "STYLE" });
            int y = y0 + 50;
            for (auto* r : colourRows)
            {
                w.captions.push_back ({ { 24, y, 110, kKeyH }, r->name });
                int x = 140;
                for (auto* k : r->sw) { k->setBounds (x, y, 34, kKeyH); x += 40; }
                r->more.setBounds (x, y, 34, kKeyH);
                if (r->grad) { w.captions.push_back ({ { x + 54, y, 80, kKeyH }, "GRADIENT" }); r->gradS.setBounds (x + 134, y, 180, kKeyH); }
                y += 44;
            }
            w.captions.push_back ({ { 24, y, 110, kKeyH }, "3D DEPTH" }); depthS.setBounds (140, y, 220, kKeyH);
            w.captions.push_back ({ { 400, y, 110, kKeyH }, "CONTRAST" }); contrastS.setBounds (500, y, 220, kKeyH);
            y += 44;
            w.captions.push_back ({ { 24, y, 110, kKeyH }, "SIZE" }); sizeBox.setBounds (140, y, 110, kKeyH);
        }
    }
    void pickColour (ColourRow* r)
    {
        auto cs = std::make_unique<ColourSelector> (ColourSelector::showColourspace | ColourSelector::showSliders);
        cs->setCurrentColour (r->col(), dontSendNotification);
        cs->setSize (300, 300);
        struct L : ChangeListener { MainPanel& m; ColourRow* row; L (MainPanel& mm, ColourRow* rr) : m (mm), row (rr) {}
            void changeListenerCallback (ChangeBroadcaster* b) override { if (auto* c = dynamic_cast<ColourSelector*> (b)) { row->col() = c->getCurrentColour().withAlpha (1.0f); m.lookChanged(); } } };
        pickListener = std::make_unique<L> (*this, r);
        cs->addChangeListener (pickListener.get());
        CallOutBox::launchAsynchronously (std::move (cs), r->more.getScreenBounds(), nullptr);
    }
    std::unique_ptr<ChangeListener> pickListener;

    void lookChanged (bool save = true)
    {
        look.refresh();
        auto& s = proc.apvts.state; const auto& t = look.t;
        s.setProperty ("lkStyle", (int) t.style, nullptr);
        s.setProperty ("lkIface", t.iface.toString(), nullptr); s.setProperty ("lkSide", t.side.toString(), nullptr);
        s.setProperty ("lkFont", t.font.toString(), nullptr); s.setProperty ("lkLed", t.led.toString(), nullptr);
        s.setProperty ("lkIfaceGrad", t.ifaceGrad, nullptr); s.setProperty ("lkSideGrad", t.sideGrad, nullptr); s.setProperty ("lkFontGrad", t.fontGrad, nullptr);
        s.setProperty ("lkDepth", t.depth, nullptr); s.setProperty ("lkContrast", t.contrast, nullptr);
        if (save)
        {
            XmlElement x ("FlexiverbLook2");
            for (int i = 0; i < s.getNumProperties(); ++i) { const auto n = s.getPropertyName (i).toString(); if (n.startsWith ("lk")) x.setAttribute (n, s.getProperty (n).toString()); }
            lookFile().getParentDirectory().createDirectory(); x.writeTo (lookFile());
        }
        styleSeg.repaintAll();
        repaintAll (*this);
    }
    static void repaintAll (Component& c) { c.repaint(); for (auto* ch : c.getChildren()) repaintAll (*ch); }
    static File lookFile() { return userPresetFolder().getParentDirectory().getChildFile ("FlexiverbLook2.xml"); }
    void loadLook()
    {
        auto& s = proc.apvts.state; auto& t = look.t;
        auto apply = [&t] (std::function<String (const char*, const String&)> get)
        {
            t.defaults ((Style) jlimit (0, 2, get ("lkStyle", "0").getIntValue()));
            t.iface = Colour::fromString (get ("lkIface", t.iface.toString())); t.side = Colour::fromString (get ("lkSide", t.side.toString()));
            t.font = Colour::fromString (get ("lkFont", t.font.toString())); t.led = Colour::fromString (get ("lkLed", t.led.toString()));
            t.ifaceGrad = get ("lkIfaceGrad", String (t.ifaceGrad)).getFloatValue(); t.sideGrad = get ("lkSideGrad", String (t.sideGrad)).getFloatValue();
            t.fontGrad = get ("lkFontGrad", String (t.fontGrad)).getFloatValue(); t.depth = get ("lkDepth", String (t.depth)).getFloatValue();
            t.contrast = get ("lkContrast", String (t.contrast)).getFloatValue();
        };
        if (s.hasProperty ("lkStyle")) apply ([&s] (const char* n, const String& d) { return s.getProperty (n, d).toString(); });
        else if (auto x = parseXML (lookFile())) { auto* xp = x.get(); apply ([xp] (const char* n, const String& d) { return xp->getStringAttribute (n, d); }); }
        look.refresh();
    }

    // ---------------------------------------------------------------- live analyser
    void updateAnalyser()
    {
        const int N = 1 << 12;
        proc.readScope (fftBuf.data(), N);
        window.multiplyWithWindowingTable (fftBuf.data(), (size_t) N);
        std::fill (fftBuf.begin() + N, fftBuf.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (fftBuf.data());
        const double fs = jmax (8000.0, proc.getSampleRateHz());
        auto bandDb = [&] (double f0, double f1)
        {
            const int b0 = jmax (1, (int) (f0 / fs * N)), b1 = jmin (N / 2 - 1, jmax (b0 + 1, (int) (f1 / fs * N)));
            double e = 0; for (int b = b0; b < b1; ++b) e += (double) fftBuf[(size_t) b] * fftBuf[(size_t) b];
            return (float) (10.0 * std::log10 (e / (double) (N * N) * 16.0 + 1e-12));
        };
        const float lo = bandDb (20, 300), mid = bandDb (300, 4000), hi = bandDb (4000, 20000);
        const float vals[3] = { lo, mid, hi };
        for (int b = 0; b < 3; ++b) { auto& sm = histSmooth[b]; sm += (jmax (sm - 6.0f, vals[b]) - sm) * 0.5f; hist[b][(size_t) histPos] = sm; }
        histPos = (histPos + 1) % (int) histLen;
        const size_t nb = bars.size();
        for (size_t i = 0; i < nb; ++i)
        {
            const double f0 = 20.0 * std::pow (1000.0, (double) i / nb), f1 = 20.0 * std::pow (1000.0, (double) (i + 1) / nb);
            const float v = bandDb (f0, f1) + 4.0f;
            bars[i] = v > bars[i] ? bars[i] + (v - bars[i]) * 0.6f : bars[i] - 1.2f;
            peaks[i] = v > peaks[i] ? v : peaks[i] - 0.35f;
        }
    }

    // ---------------------------------------------------------------- presets (nested menus: "480L/1 Halls")
    Array<File> userFiles;
    void buildPresetMenu()
    {
        auto* root = presetBox.getRootMenu();
        root->clear();
        root->addItem (kDefaultId, "Default");
        // groups in list order, "A/B" nests B inside A
        StringArray tops; std::map<String, StringArray> subs; std::map<String, std::vector<int>> items;
        for (int i = 0; i < kNumPresets; ++i)
        {
            const String g (kPresets[i].group);
            const String top = g.upToFirstOccurrenceOf ("/", false, false), sub = g.fromFirstOccurrenceOf ("/", false, false);
            if (! tops.contains (top)) tops.add (top);
            if (sub.isNotEmpty() && ! subs[top].contains (sub)) subs[top].add (sub);
            items[g].push_back (i);
        }
        for (auto& top : tops)
        {
            PopupMenu tm;
            for (int i : items[top]) tm.addItem (kBuiltinBase + i, kPresets[i].name);
            for (auto& sub : subs[top])
            {
                PopupMenu sm;
                for (int i : items[top + "/" + sub]) sm.addItem (kBuiltinBase + i, kPresets[i].name);
                tm.addSubMenu (sub, sm);
            }
            root->addSubMenu (top, tm);
        }
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
                if (irSide (a) == 1 || irSide (b) == 0) std::swap (a, b);
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
        if (++dspTick >= 15)
        {
            dspTick = 0;
            const float d = std::round (proc.dspLoad.load() * 10.0f) / 10.0f;
            if (d != shownDsp) { shownDsp = d; repaint (headArea); }
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
        if (sync != lastSync) { lastSync = sync; updateVisibility(); }
        const bool optoOnly = (int) raw (sid (editSlot, "cpType")) == pv::CompOpto;
        for (int i : { 2, 3 }) compK[i]->setEnabled (! optoOnly);
        const int ver = proc.waveVersion.load();
        if (ver != lastWaveVersion) { lastWaveVersion = ver; wave = proc.getWave (editSlot); repaint (dispArea); if (irWin.isVisible()) irWin.repaint(); }
        if (irWin.isVisible())
        {
            const String info = proc.irLayout() == pv::IrNone ? String ("no file loaded - press LOAD") : proc.irInfo();
            if (! irWin.captions.empty() && irWin.captions[0].second != info) { irWin.captions[0].second = info; irWin.repaint(); }
        }
        if (mode == Live && ! advOpen && editSlot != pv::IR) { updateAnalyser(); repaint (dispArea); }
        if (changed)
        {
            if (mode != Live) repaint (dispArea);
            for (auto* k : knobs) k->repaint();
            for (auto* f : faders) f->repaint();
            repaintKeys();
        }
        repaint (meterArea[0].getUnion (meterArea[2]).expanded (4));
        if (drawerOpen && drawerTab == 1) repaint (inMeter.getUnion (grMeter).expanded (30, 24));
    }

    PlateVerbProcessor& proc;
    FlexLook look;
    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window { (size_t) (1 << 12), juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> fftBuf = std::vector<float> ((size_t) (1 << 13), 0.0f);
    static constexpr size_t histLen = 180;
    std::vector<float> hist[3] = { std::vector<float> (histLen, -90.0f), std::vector<float> (histLen, -90.0f), std::vector<float> (histLen, -90.0f) };
    float histSmooth[3] = { -90.0f, -90.0f, -90.0f }; int histPos = 0;
    std::vector<float> bars = std::vector<float> (31, -90.0f), peaks = std::vector<float> (31, -90.0f);
    ComboBox presetBox;
    Key presetPrev, presetNext, presetSave, advKey, drawerKey;
    Key compOn, compDuck;
    std::unique_ptr<ParamKey> dlOn, dlSync, dlPing;
    ComboBox compType, compTone, dlDivL, dlDivR;
    OwnedArray<Key> algoKeys; int algoSlot[5] {};
    Segmented modeSeg, pageSeg, drawerSeg;
    OwnedArray<FaderBox> faders; String abbrev[6];
    OwnedArray<KnobBox> knobs;
    KnobBox* eqF[4] {}; KnobBox* eqG[4] {}; KnobBox* compK[6] {}; KnobBox* dlK[12] {}; KnobBox* outKnob = nullptr;
    pv::WaveData wave;
    Rectangle<int> headArea, surfArea, drawerArea, dispArea, bayArea, meterArea[3], inMeter, grMeter, dspArea;
    String auxBusNote;
    float shownDsp = 0.0f; int dspTick = 0;
    Array<float> lastValues; int editSlot = 0, lastWaveVersion = -1, drawerTab = 0, mode = 0;
    bool drawerOpen = true, page2 = false, lastSync = false, advOpen = false;

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
        scale = (float) (double) p.apvts.state.getProperty ("uiScale4", 0.85);
        panel.onSizeChanged = [this] { applySize(); };
        panel.onScaleChosen = [this] (float s) { scale = s; procRef.apvts.state.setProperty ("uiScale4", scale, nullptr); applySize(); };
        setResizable (true, true);
        applySize();
        panel.showScale (scale);
    }
    ~PlateVerbEditor() override { setLookAndFeel (nullptr); }
    void resized() override
    {
        if (applying) return;
        scale = jlimit (0.5f, 2.0f, (float) getWidth() / (float) panel.designWidth());
        procRef.apvts.state.setProperty ("uiScale4", scale, nullptr);
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
    float scale = 0.85f; bool applying = false;
};

AudioProcessorEditor* PlateVerbProcessor::createEditor() { return new PlateVerbEditor (*this); }
