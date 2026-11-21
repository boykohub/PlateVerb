// Flexiverb 480 editor (included by PluginProcessor.cpp).
//
//  Preset bar      < preset name >  and SAVE. Click the name for the preset browser (every preset at once, by category).
//  Display         machine A (and B) program names, a status line, and a graph: the decay time against frequency for
//                  reverbs (click to add points, drag, double-click removes) or the delay taps for delay programs (drag a
//                  tap: sideways = time, up/down = level). Right-click the display for the text size.
//  Keys            MACHINE A / B, MACHINES (one, series, parallel, left/right), PAGE, CONTROL, MUTE, SETUP.
//                  Keys have no lamps: a key lights itself while it is on or being pressed.
//  Slider display  the full names and values of the six sliders on this page.
//  Drawer          (tab on the right) ANALOG, EQ (concentric knobs), COMPRESSOR.
//  SETUP           colours with gradient, text sizes, oversampling and 64-bit.
// The window scales from its lower right corner; right-click the panel for fixed sizes.
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

// ============================================================================ the look (SETUP): colours, gradient, text sizes
struct Theme
{
    Colour panel { 0xff2c2d30 }, panelText { 0xffe9e5da }, cheek { 0xff141415 }, key { 0xffbdbab2 }, keyText { 0xff1c1d1f },
           keyLight { 0xffffa63a }, display { 0xff140707 }, displayText { 0xffff3a1e }, fader { 0xff2b2c2f },
           eqKnob { 0xff5a6470 }, eqInner { 0xff8a2a22 }, compKnob { 0xff3a3c40 }, analogKnob { 0xff6b4a2c };
    float gradient = 0.5f, keyPt = 13.0f, knobPt = 11.5f, displayPt = 13.0f;

    struct Item { const char* name; Colour Theme::* c; };
    static const std::vector<Item>& items()
    {
        static const std::vector<Item> v {
            { "Panel", &Theme::panel }, { "Panel text", &Theme::panelText }, { "Side cheeks", &Theme::cheek },
            { "Buttons", &Theme::key }, { "Button text", &Theme::keyText }, { "Button light", &Theme::keyLight },
            { "Screen", &Theme::display }, { "Screen text", &Theme::displayText }, { "Faders", &Theme::fader },
            { "EQ knobs", &Theme::eqKnob }, { "EQ gain caps", &Theme::eqInner }, { "Compressor knobs", &Theme::compKnob },
            { "Analog knobs", &Theme::analogKnob } };
        return v;
    }
    String toString() const
    {
        XmlElement x ("LOOK");
        int i = 0; for (auto& it : items()) x.setAttribute ("c" + String (i++), (this->*it.c).toString());
        x.setAttribute ("grad", gradient); x.setAttribute ("keyPt", keyPt); x.setAttribute ("knobPt", knobPt); x.setAttribute ("dispPt", displayPt);
        return x.toString (XmlElement::TextFormat().singleLine().withoutHeader());
    }
    bool fromString (const String& s)
    {
        auto x = parseXML (s); if (x == nullptr || ! x->hasTagName ("LOOK")) return false;
        int i = 0; for (auto& it : items()) { const auto a = x->getStringAttribute ("c" + String (i++)); if (a.isNotEmpty()) this->*it.c = Colour::fromString (a); }
        gradient = (float) x->getDoubleAttribute ("grad", gradient); keyPt = (float) x->getDoubleAttribute ("keyPt", keyPt);
        knobPt = (float) x->getDoubleAttribute ("knobPt", knobPt); displayPt = (float) x->getDoubleAttribute ("dispPt", displayPt);
        return true;
    }
    Colour hi (Colour c, float k = 1.0f) const { return c.brighter (0.35f * gradient * k); }
    Colour lo (Colour c, float k = 1.0f) const { return c.darker (0.55f * gradient * k); }
    static File defaultFile() { return registerFolder().getParentDirectory().getChildFile ("Look.xml"); }
};

static void drawLegend (Graphics& g, const String& s, Rectangle<float> r, const Font& f, Colour c, Justification j = Justification::centred)
{
    g.setFont (f);
    g.setColour (Colours::black.withAlpha (0.3f)); g.drawText (s, r.translated (0, 1.0f), j, false);
    g.setColour (c); g.drawText (s, r, j, false);
}

// ============================================================================ a key: no lamp - the cap lights itself while on or pressed
class LarcKey : public Component, private Timer
{
public:
    LarcKey (const Theme& t, const String& s) : th (t), text (s) { setRepaintsOnMouseActivity (true); }
    std::function<void()> onClick;
    void setLit (bool b) { if (b != lit) { lit = b; repaint(); } }
    void setText (const String& s) { if (s != text) { text = s; repaint(); } }
    const String& getText() const { return text; }
    float preferredWidth() const { return textW (ui (th.keyPt, W::Bold, 0.06f), text) + th.keyPt * 1.6f + 8.0f; }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const float rad = 3.5f;
        const bool on = lit || pressed || flash;
        g.setColour (Colours::black.withAlpha (0.75f)); g.fillRoundedRectangle (r, rad + 1.5f);
        if (on) { g.setColour (th.keyLight.withAlpha (0.35f)); g.drawRoundedRectangle (r.expanded (0.5f), rad + 2.0f, 2.0f); }
        const float travel = pressed ? 1.2f : 0.0f;
        auto skirt = r.reduced (2.0f).translated (0, travel);
        const Colour c = on ? th.key.interpolatedWith (th.keyLight, 0.62f) : th.key;
        g.setGradientFill (ColourGradient (c.darker (0.12f), skirt.getX(), skirt.getY(), th.lo (c, 1.3f).darker (0.3f), skirt.getX(), skirt.getBottom(), false));
        g.fillRoundedRectangle (skirt, rad);
        auto face = skirt.reduced (2.0f, 1.5f).withTrimmedBottom (pressed ? 1.0f : 2.5f);
        ColourGradient fg (th.hi (c), face.getCentreX(), face.getY(), th.lo (c, 0.4f), face.getCentreX(), face.getBottom(), false);
        g.setGradientFill (fg); g.fillRoundedRectangle (face, rad - 1.0f);
        if (on)
        {   // backlit: a soft glow from inside the cap
            g.setGradientFill (ColourGradient (th.keyLight.brighter (0.4f).withAlpha (0.55f), face.getCentreX(), face.getCentreY(),
                                               th.keyLight.withAlpha (0.0f), face.getCentreX(), face.getY() - face.getHeight() * 0.3f, true));
            g.fillRoundedRectangle (face, rad - 1.0f);
        }
        g.setColour (Colours::white.withAlpha (0.45f * jmax (0.3f, th.gradient))); g.drawLine (face.getX() + 3, face.getY() + 0.7f, face.getRight() - 3, face.getY() + 0.7f, 1.0f);
        g.setColour (Colours::black.withAlpha (0.35f)); g.drawRoundedRectangle (face, rad - 1.0f, 0.6f);
        if (isMouseOver() && ! pressed && ! on) { g.setColour (Colours::white.withAlpha (0.07f)); g.fillRoundedRectangle (face, rad - 1.0f); }
        const bool darkCap = c.getPerceivedBrightness() < 0.45f;
        const Colour ink = darkCap && th.keyText.getPerceivedBrightness() < 0.45f ? Colours::white : th.keyText;
        g.setFont (ui (jmin (th.keyPt, face.getHeight() * 0.62f), W::Bold, 0.06f));
        g.setColour (darkCap ? Colours::black.withAlpha (0.5f) : Colours::white.withAlpha (0.4f)); g.drawText (text, face.translated (0, darkCap ? -0.8f : 0.8f), Justification::centred, false);
        g.setColour (ink); g.drawText (text, face, Justification::centred, false);
    }
    void mouseDown (const MouseEvent&) override { pressed = true; repaint(); }
    void mouseUp (const MouseEvent& e) override
    {
        pressed = false; flash = true; startTimer (130); repaint();
        if (getLocalBounds().contains (e.getPosition()) && onClick) onClick();
    }
private:
    void timerCallback() override { stopTimer(); flash = false; repaint(); }
    const Theme& th; String text; bool lit = false, pressed = false, flash = false;
};

// keys laid out in rows, each as wide as its text; rows wrap and are spread over the width. Returns the next y.
static int flowKeys (const std::vector<LarcKey*>& keys, int x0, int width, int y, int h, int gap)
{
    std::vector<LarcKey*> row; float used = 0;
    auto flush = [&]
    {
        if (row.empty()) return;
        const float extra = jmax (0.0f, (width - used - gap * (float) (row.size() - 1)) / (float) row.size());
        float x = (float) x0;
        for (auto* k : row) { const float w = k->preferredWidth() + extra; k->setBounds ((int) x, y, (int) w, h); x += w + gap; }
        y += h + gap; row.clear(); used = 0;
    };
    for (auto* k : keys)
    {
        if (! k->isVisible()) continue;
        const float w = k->preferredWidth();
        if (! row.empty() && used + gap * (float) row.size() + w > width) flush();
        row.push_back (k); used += w;
    }
    flush();
    return y;
}

// ============================================================================ fader
class LarcFader : public Component
{
public:
    explicit LarcFader (const Theme& t) : th (t) {}
    std::function<void (float)> onMove; std::function<void()> onBegin, onEnd, onReset;
    bool enabled = true;
    void setTarget (float t) { target = t; }
    bool animate()
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
            g.setColour (th.panelText.withAlpha (i % 5 == 0 ? 0.5f : 0.28f));
            g.fillRect (cx - 11 - w, y - 0.5f, w, 1.0f); g.fillRect (cx + 11, y - 0.5f, w, 1.0f);
        }
        auto slot = Rectangle<float> (cx - 2.5f, top - 6, 5.0f, bot - top + 12);
        g.setColour (Colours::black.withAlpha (0.9f)); g.fillRoundedRectangle (slot, 2.5f);
        const float y = bot - (bot - top) * shown, cw = jmin (34.0f, r.getWidth() - 8), ch = 24.0f;
        auto cap = Rectangle<float> (cx - cw / 2, y - ch / 2, cw, ch);
        g.setColour (Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (cap.translated (1.5f, 3.0f), 3);
        const Colour base = enabled ? th.fader : th.fader.darker (0.4f);
        g.setGradientFill (ColourGradient (th.hi (base, 1.6f), cap.getX(), cap.getY(), th.lo (base), cap.getX(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 3);
        auto face = cap.reduced (2.5f, 3.0f);
        g.setGradientFill (ColourGradient (th.hi (base), face.getX(), face.getY(), base, face.getX(), face.getBottom(), false));
        g.fillRoundedRectangle (face, 2);
        g.setColour (enabled ? th.panelText : th.panelText.withAlpha (0.2f));
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
    const Theme& th; float shown = 0, target = 0, startShown = 0, startY = 0; bool dragging = false;
};

// one line of 14-segment characters; unlit segments faintly visible
static void segLine (Graphics& g, const String& s, Rectangle<float> r, float h, Colour col)
{
    const auto f = segFont (h);
    g.setFont (f);
    const float cw = textW (f, "8") * 1.08f;
    const int cells = jmax (1, (int) (r.getWidth() / cw));
    const String t = s.toUpperCase();
    std::vector<std::pair<juce_wchar, bool>> cell;
    for (int i = 0; i < t.length() && (int) cell.size() < cells; ++i)
    {
        const juce_wchar c = t[i];
        if ((c == '.' || c == ',') && ! cell.empty() && ! cell.back().second && cell.back().first != ' ') { cell.back().second = true; continue; }
        cell.push_back ({ c, false });
    }
    for (int i = 0; i < cells; ++i)
    {
        auto cr = Rectangle<float> (r.getX() + i * cw, r.getY(), cw, r.getHeight());
        g.setColour (col.withAlpha (0.06f)); g.drawText ("~", cr, Justification::centredLeft, false);
        if (i >= (int) cell.size()) continue;
        const juce_wchar c = cell[(size_t) i].first;
        String ch = (c == ' ') ? String() : String::charToString (c);
        if (cell[(size_t) i].second) ch << ".";
        if (ch.isEmpty()) continue;
        g.setColour (col.withAlpha (0.2f));
        for (auto o : { Point<float> (-0.7f, 0), Point<float> (0.7f, 0) }) g.drawText (ch, cr.translated (o.x, o.y), Justification::centredLeft, false);
        g.setColour (col); g.drawText (ch, cr, Justification::centredLeft, false);
    }
}

// ============================================================================ knobs with printed scales (270 degrees, centred)
static constexpr float kSweep = MathConstants<float>::pi * 0.75f;
static float knobAngle (float norm) { return -kSweep + 2.0f * kSweep * jlimit (0.0f, 1.0f, norm); }
struct ScaleMark { float value; const char* text; };

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
        g.drawText (m.text, Rectangle<float> (60, fontH + 2).withCentre (pt), Justification::centred, false);
    }
}
static void drawKnobBody (Graphics& g, const Theme& th, Rectangle<float> k, float angle, Colour body, Colour pointer, bool skirt)
{
    const float d = k.getWidth();
    g.setColour (Colours::black.withAlpha (0.5f)); g.fillEllipse (k.translated (1.5f, 2.5f));
    if (skirt)
    {
        g.setGradientFill (ColourGradient (th.hi (body), k.getX(), k.getY(), th.lo (body, 1.2f), k.getRight(), k.getBottom(), false));
        g.fillEllipse (k);
        g.setColour (Colours::black.withAlpha (0.25f));
        for (int i = 0; i < 36; ++i) { const float a = i * MathConstants<float>::twoPi / 36.0f; g.drawLine (Line<float> (k.getCentre().getPointOnCircumference (d * 0.43f, a), k.getCentre().getPointOnCircumference (d * 0.5f, a)), 0.8f); }
        k = k.reduced (d * 0.1f);
    }
    g.setGradientFill (ColourGradient (th.hi (body, 0.8f), k.getCentreX(), k.getY(), th.lo (body, 0.7f), k.getCentreX(), k.getBottom(), false));
    g.fillEllipse (k);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.22f * jmax (0.2f, th.gradient)), k.getCentreX(), k.getY(), Colours::transparentWhite, k.getCentreX(), k.getCentreY(), false));
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

class ScaleKnob : public Component
{
public:
    ScaleKnob (const Theme& t, AudioProcessorValueTreeState& s, const String& id, const String& title, std::vector<ScaleMark> m, Colour Theme::* cap)
        : th (t), param (s.getParameter (id)), name (title), marks (std::move (m)), capColour (cap), watch (s, { id }, *this) {}
    std::function<void (const String&, const String&)> onShow;
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto lab = r.removeFromBottom (th.knobPt + 5);
        const float d = jmin (r.getWidth(), r.getHeight()) * 0.5f;
        const auto c = r.getCentre().translated (0, 3);
        drawScale (g, param, c, d * 0.5f + 11.0f, marks, th.panelText.withAlpha (0.85f), jmax (7.5f, th.knobPt * 0.75f));
        drawKnobBody (g, th, Rectangle<float> (d, d).withCentre (c), knobAngle (param->getValue()), th.*capColour, th.panelText, true);
        drawLegend (g, name, lab, ui (th.knobPt, W::Bold, 0.06f), th.panelText);
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
    { param->beginChangeGesture(); param->setValueNotifyingHost (jlimit (0.0f, 1.0f, param->getValue() + w.deltaY * 0.03f)); param->endChangeGesture(); show(); }
private:
    void show() { if (onShow) onShow (name, param->getCurrentValueAsText()); }
    const Theme& th; RangedAudioParameter* param; String name; std::vector<ScaleMark> marks; Colour Theme::* capColour; float start = 0;
    ParamWatcher watch;
};

// concentric EQ knob: outer skirt = frequency (white numbers), inner cap = gain (red numbers)
class DualKnob : public Component
{
public:
    DualKnob (const Theme& t, AudioProcessorValueTreeState& s, const String& freqId, const String& gainId, const String& title, std::vector<ScaleMark> fMarks)
        : th (t), fP (s.getParameter (freqId)), gP (s.getParameter (gainId)), name (title), fm (std::move (fMarks)), watch (s, { freqId, gainId }, *this) {}
    std::function<void (const String&, const String&)> onShow;
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto lab = r.removeFromBottom (th.knobPt + 5);
        geometry (r);
        static const std::vector<ScaleMark> gm { { -15, "-15" }, { -10, "-10" }, { 0, "0" }, { 10, "+10" }, { 15, "+15" } };
        const Colour red (0xffff6a4a);
        g.setColour (red.withAlpha (0.6f));
        for (float db = -15; db <= 15; db += 5)
        {
            const float a = knobAngle (gP->convertTo0to1 (db));
            g.drawLine (Line<float> (c.getPointOnCircumference (rOuter + 2.0f, a), c.getPointOnCircumference (rOuter + 5.5f, a)), 1.2f);
        }
        drawScale (g, fP, c, rOuter + 27.0f, fm, th.panelText.withAlpha (0.9f), jmax (7.5f, th.knobPt * 0.75f));
        drawScale (g, gP, c, rOuter + 12.5f, gm, red, 7.0f);
        drawKnobBody (g, th, Rectangle<float> (rOuter * 2, rOuter * 2).withCentre (c), knobAngle (fP->getValue()), th.eqKnob, th.panelText, true);
        drawKnobBody (g, th, Rectangle<float> (rInner * 2, rInner * 2).withCentre (c), knobAngle (gP->getValue()), th.eqInner, Colours::white, false);
        drawLegend (g, name, lab, ui (th.knobPt, W::Bold, 0.06f), th.panelText);
    }
    bool hitTest (int x, int y) override { geometry (bodyArea()); return Point<float> ((float) x, (float) y).getDistanceFrom (c) < rOuter + 6.0f || y > getHeight() - th.knobPt - 5; }
    void mouseDown (const MouseEvent& e) override { geometry (bodyArea()); active = pick (e); start = active->getValue(); active->beginChangeGesture(); show(); }
    void mouseDrag (const MouseEvent& e) override
    {
        const float fine = e.mods.isShiftDown() || e.mods.isAltDown() ? 0.1f : 1.0f;
        active->setValueNotifyingHost (jlimit (0.0f, 1.0f, start - (float) e.getDistanceFromDragStartY() * 0.005f * fine)); show();
    }
    void mouseUp (const MouseEvent&) override { if (active) active->endChangeGesture(); }
    void mouseDoubleClick (const MouseEvent& e) override { geometry (bodyArea()); auto* p = pick (e); p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); active = p; show(); }
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    { geometry (bodyArea()); auto* p = pick (e); p->beginChangeGesture(); p->setValueNotifyingHost (jlimit (0.0f, 1.0f, p->getValue() + w.deltaY * 0.03f)); p->endChangeGesture(); active = p; show(); }
private:
    Rectangle<float> bodyArea() const { return getLocalBounds().toFloat().withTrimmedBottom (th.knobPt + 5); }
    RangedAudioParameter* pick (const MouseEvent& e) const { return e.position.getDistanceFrom (c) <= rInner + 1.0f ? gP : fP; }
    void geometry (Rectangle<float> r) { c = r.getCentre().translated (0, 4); rOuter = jmin ((r.getWidth() - 28.0f) * 0.22f, r.getHeight() * 0.22f); rInner = rOuter * 0.58f; }
    void show() { if (onShow && active) onShow (name + (active == gP ? " gain" : " frequency"), active->getCurrentValueAsText()); }
    const Theme& th; RangedAudioParameter *fP, *gP, *active = nullptr; String name; std::vector<ScaleMark> fm;
    Point<float> c; float rOuter = 20, rInner = 11, start = 0;
    ParamWatcher watch;
};

// a key bound to an on/off parameter: lights itself while on
class ParamKey : public LarcKey
{
public:
    ParamKey (const Theme& t, AudioProcessorValueTreeState& s, const String& id, const String& txt) : LarcKey (t, txt), param (s.getParameter (id))
    { onClick = [this] { param->beginChangeGesture(); param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f); param->endChangeGesture(); }; }
    void sync() { setLit (param->getValue() > 0.5f); }
private:
    RangedAudioParameter* param;
};

// ============================================================================ graphs drawn inside the screen
// decay time against frequency (reverbs): dashed = the program's own decay, solid = your curve
class DecayGraph : public Component
{
public:
    DecayGraph (const Theme& t, Flexiverb480Processor& p) : th (t), proc (p) {}
    std::function<int()> machine;
    std::function<void (const String&, const String&)> onShow;
    void paint (Graphics& g) override
    {
        plot = getLocalBounds().toFloat().reduced (36, 6).withTrimmedRight (-24).withTrimmedBottom (10);
        const int m = machine ? machine() : 0;
        const Colour col = th.displayText;
        if (! proc.hasDecayCurve (m)) return;
        rtMain = jmax (0.05f, proc.mainRt (m));
        yLo = jmax (0.02f, rtMain / 10.0f); yHi = rtMain * 6.0f;
        g.setFont (ui (8.5f, W::Medium));
        for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            const float x = xOf (f); g.setColour (col.withAlpha (0.10f)); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (col.withAlpha (0.55f)); g.drawText (f >= 1000 ? String (f / 1000, 0) + "k" : String ((int) f), Rectangle<float> (x - 15, plot.getBottom() + 1, 30, 10), Justification::centred, false);
        }
        for (float t : { 0.05f, 0.1f, 0.2f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 50.0f })
        {
            if (t < yLo || t > yHi) continue;
            const float y = yOf (t); g.setColour (col.withAlpha (0.10f)); g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            g.setColour (col.withAlpha (0.55f)); g.drawText (String (t, t < 1 ? 2 : 0) + " s", Rectangle<float> (plot.getX() - 36, y - 5, 33, 10), Justification::centredRight, false);
        }
        {
            Path pp;
            for (int i = 0; i <= 120; ++i) { const float f = 20.0f * std::pow (1000.0f, i / 120.0f); const Point<float> pt (xOf (f), yOf (rtMain * proc.programDecayMul (m, f))); if (i == 0) pp.startNewSubPath (pt); else pp.lineTo (pt); }
            Path dashed; const float dl[] = { 4.0f, 3.0f }; PathStrokeType (1.0f).createDashedStroke (dashed, pp, dl, 2);
            g.setColour (col.withAlpha (proc.curveOn[m] ? 0.35f : 0.8f)); g.fillPath (dashed);
        }
        const auto& pts = proc.curvePts[m];
        if (proc.curveOn[m] && ! pts.empty())
        {
            Path pc;
            for (int i = 0; i <= 160; ++i) { const float f = 20.0f * std::pow (1000.0f, i / 160.0f); const Point<float> pt (xOf (f), yOf (rtMain * Flexiverb480Processor::curveEval (pts, f))); if (i == 0) pc.startNewSubPath (pt); else pc.lineTo (pt); }
            g.setColour (col.withAlpha (0.25f)); g.strokePath (pc, PathStrokeType (4.0f));
            g.setColour (col); g.strokePath (pc, PathStrokeType (1.8f));
            for (size_t i = 0; i < pts.size(); ++i)
            {
                const Point<float> pt (xOf (pts[i].f), yOf (rtMain * pts[i].m));
                g.setColour (Colours::black); g.fillEllipse (Rectangle<float> (11, 11).withCentre (pt));
                g.setColour ((int) i == drag || (int) i == hover ? Colours::white : col); g.drawEllipse (Rectangle<float> (9, 9).withCentre (pt), 2.0f);
            }
        }
        g.setColour (col.withAlpha (0.6f)); g.setFont (ui (9.0f, W::Bold, 0.05f));
        g.drawText (proc.curveOn[m] ? "Decay time: your curve  (double-click a point to remove it)" : "Decay time: click to add a point", getLocalBounds().toFloat().reduced (36, 0).withHeight (12), Justification::topLeft, false);
    }
    void mouseMove (const MouseEvent& e) override { const int h = hit (e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const MouseEvent& e) override
    {
        const int m = machine ? machine() : 0;
        if (e.mods.isPopupMenu() || ! proc.hasDecayCurve (m)) return;
        auto pts = proc.curvePts[m];
        if (! proc.curveOn[m]) pts = proc.programCurve (m);
        drag = hit (e.position);
        if (drag < 0 || ! proc.curveOn[m])
        {
            pts.push_back ({ fOf (e.position.x), jlimit (0.02f, 20.0f, tOf (e.position.y) / rtMain) });
            std::sort (pts.begin(), pts.end(), [] (const auto& a, const auto& b) { return a.f < b.f; });
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
            onShow ("Decay at " + (f >= 1000 ? String (f / 1000, 2) + " kHz" : String ((int) f) + " Hz"), String (t, t < 10 ? 2 : 1) + " s");
        }
    }
    const Theme& th; Flexiverb480Processor& proc; Rectangle<float> plot; float rtMain = 1, yLo = 0.1f, yHi = 10; int drag = -1, hover = -1;
};

// delay taps (delay programs, and the utility delay on its CONTROL pages): a line per tap at its time, as tall as its
// level, with fainter feedback repeats. Drag a tap: sideways = time, up/down = level. Faders follow, and vice versa.
struct TapDef { int timeSlot, levelSlot, fbSlot; const char* label; };
static std::vector<TapDef> delayTaps (int algo)
{
    using namespace l480;
    switch (algo)
    {
        case aTwin:   return { { 6, 7, 8, "1 L" }, { 9, 10, 11, "2 R" }, { 12, 13, 14, "3 L" }, { 15, 16, 17, "4 R" } };
        case aChorus: return { { 6, 12, 2, "1" }, { 7, 13, 2, "2" }, { 8, 14, 2, "3" }, { 9, 15, 2, "4" }, { 10, 16, 2, "5" }, { 11, 17, 2, "6" } };
        case aMBand:  return { { 6, 12, 0, "1" }, { 7, 13, 0, "2" }, { 8, 14, 0, "3" }, { 9, 15, 0, "4" }, { 10, 16, 0, "5" }, { 11, 17, 0, "6" } };
        default:      return {};
    }
}
class DelayGraph : public Component
{
public:
    DelayGraph (const Theme& t, Flexiverb480Processor& p) : th (t), proc (p) {}
    std::function<int()> machine;
    std::function<bool()> utility;                                     // the utility delay's CONTROL page is showing
    std::function<void (const String&, const String&)> onShow;
    std::function<void (int slot)> onSlotTouched;                      // so the editor can flash that fader
    bool hasTaps() const { return (utility && utility()) || ! delayTaps (proc.algoOf (machine ? machine() : 0)).empty(); }
    void paint (Graphics& g) override
    {
        plot = getLocalBounds().toFloat().reduced (36, 6).withTrimmedRight (-24).withTrimmedBottom (10).withTrimmedTop (22);
        collect();
        const Colour col = th.displayText;
        // time grid
        g.setFont (ui (8.5f, W::Medium));
        const float step = tMax <= 200 ? 25 : tMax <= 600 ? 100 : tMax <= 1500 ? 250 : 500;
        for (float t = 0; t <= tMax + 1; t += step)
        {
            const float x = xOf (t); g.setColour (col.withAlpha (0.10f)); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (col.withAlpha (0.55f)); g.drawText (String ((int) t) + " ms", Rectangle<float> (x - 22, plot.getBottom() + 1, 44, 10), Justification::centred, false);
        }
        for (float db : { 0.0f, -12.0f, -24.0f, -36.0f })
        {
            const float y = yOf (db); g.setColour (col.withAlpha (0.10f)); g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            g.setColour (col.withAlpha (0.55f)); g.drawText (String ((int) db) + " dB", Rectangle<float> (plot.getX() - 36, y - 5, 33, 10), Justification::centredRight, false);
        }
        for (size_t i = 0; i < taps.size(); ++i)
        {
            const auto& t = taps[i];
            // feedback repeats
            if (t.fb > 0.01f)
            {
                float lv = t.db; for (int k = 2; k <= 6; ++k) { lv += 20.0f * std::log10 (t.fb); const float x = xOf (t.ms * k); if (x > plot.getRight() || lv < -48) break;
                    g.setColour (col.withAlpha (0.25f)); g.drawLine (x, plot.getBottom(), x, yOf (lv), 1.0f); }
            }
            const float x = xOf (t.ms), y = yOf (t.db);
            g.setColour (col.withAlpha (0.3f)); g.drawLine (x, plot.getBottom(), x, y, 4.0f);
            g.setColour (col); g.drawLine (x, plot.getBottom(), x, y, 1.8f);
            g.setColour (Colours::black); g.fillEllipse (Rectangle<float> (12, 12).withCentre ({ x, y }));
            g.setColour ((int) i == drag || (int) i == hover ? Colours::white : col); g.drawEllipse (Rectangle<float> (10, 10).withCentre ({ x, y }), 2.0f);
            g.setFont (ui (9.0f, W::Bold)); g.setColour (col.withAlpha (0.85f));
            g.drawText (t.label, Rectangle<float> (x + 9, y - 5, 44, 11), Justification::left, false);
        }
        g.setColour (col.withAlpha (0.6f)); g.setFont (ui (9.0f, W::Bold, 0.05f));
        g.drawText ("Delay taps: drag a point - sideways for time, up and down for level", getLocalBounds().toFloat().reduced (36, 0).withHeight (12), Justification::topLeft, false);
    }
    void mouseMove (const MouseEvent& e) override { collect(); const int h = hit (e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const MouseEvent& e) override
    {
        collect(); drag = hit (e.position); if (drag < 0) return;
        dragTMax = tMax; begin (taps[(size_t) drag]); show();
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (drag < 0 || drag >= (int) taps.size()) return;
        const float saveMax = tMax; tMax = dragTMax;                          // the scale holds still while dragging
        const float ms = jmax (0.0f, (e.position.x - plot.getX()) / plot.getWidth() * tMax);
        const float db = jlimit (-48.0f, 0.0f, (plot.getBottom() - e.position.y) / plot.getHeight() * 48.0f - 48.0f);
        set (taps[(size_t) drag], ms, db); tMax = saveMax; show(); repaint();
    }
    void mouseUp (const MouseEvent&) override { if (drag >= 0 && drag < (int) taps.size()) end (taps[(size_t) drag]); drag = -1; repaint(); }
private:
    struct Tap { float ms, db, fb; String label; int timeSlot, levelSlot; bool util; int side; };
    std::vector<Tap> taps; float tMax = 500, dragTMax = 500;
    void collect()
    {
        taps.clear();
        const int m = machine ? machine() : 0;
        if (utility && utility())
        {
            const bool sync = proc.apvts.getRawParameterValue ("dlSync")->load() > 0.5f;
            const float fb = proc.apvts.getRawParameterValue ("dlFb")->load(), mix = proc.apvts.getRawParameterValue ("dlMix")->load();
            const float db = mix > 0.0001f ? jmax (-48.0f, 20.0f * std::log10 (mix)) : -48.0f;
            for (int s = 0; s < 2; ++s)
            {
                float ms = proc.apvts.getRawParameterValue (s == 0 ? "dlTimeL" : "dlTimeR")->load();
                if (sync) { static const float div[] = { 0.0625f, 1.0f / 12, 0.125f, 1.0f / 6, 0.25f, 0.375f, 1.0f / 3, 0.5f, 0.75f, 2.0f / 3, 1.0f, 1.5f, 4.0f / 3, 2.0f, 3.0f, 4.0f };
                            ms = div[jlimit (0, 15, (int) proc.apvts.getRawParameterValue (s == 0 ? "dlDivL" : "dlDivR")->load())] * 500.0f; }   // shown at 120 bpm
                taps.push_back ({ ms, db, fb, s == 0 ? "Left" : "Right", -1, -1, true, s });
            }
        }
        else
            for (auto& d : delayTaps (proc.algoOf (m)))
            {
                const auto& ls = proc.spec (m, d.levelSlot);
                float db = proc.slotValue (m, d.levelSlot); if (ls.loText && db <= ls.lo + 0.01f) db = -48.0f;
                const float fbv = std::abs (proc.slotValue (m, d.fbSlot)) * 0.01f;
                taps.push_back ({ proc.slotValue (m, d.timeSlot), db, jmin (0.95f, fbv), d.label, d.timeSlot, d.levelSlot, false, 0 });
            }
        float mx = 100; for (auto& t : taps) mx = jmax (mx, t.ms);
        tMax = mx < 150 ? 200 : mx < 450 ? 500 : mx < 900 ? 1000 : mx < 1400 ? 1500 : 3000;
    }
    float xOf (float ms) const { return plot.getX() + plot.getWidth() * ms / tMax; }
    float yOf (float db) const { return plot.getBottom() - plot.getHeight() * (jlimit (-48.0f, 0.0f, db) + 48.0f) / 48.0f; }
    int hit (Point<float> p) const { for (size_t i = 0; i < taps.size(); ++i) if (p.getDistanceFrom ({ xOf (taps[i].ms), yOf (taps[i].db) }) < 9.0f) return (int) i; return -1; }
    RangedAudioParameter* utilTime (const Tap& t) const
    {
        const bool sync = proc.apvts.getRawParameterValue ("dlSync")->load() > 0.5f;
        return proc.apvts.getParameter (t.side == 0 ? (sync ? "dlDivL" : "dlTimeL") : (sync ? "dlDivR" : "dlTimeR"));
    }
    void begin (const Tap& t)
    {
        const int m = machine ? machine() : 0;
        if (t.util) { utilTime (t)->beginChangeGesture(); proc.apvts.getParameter ("dlMix")->beginChangeGesture(); }
        else { proc.beginSlot (m, t.timeSlot); proc.beginSlot (m, t.levelSlot); }
    }
    void end (const Tap& t)
    {
        const int m = machine ? machine() : 0;
        if (t.util) { utilTime (t)->endChangeGesture(); proc.apvts.getParameter ("dlMix")->endChangeGesture(); }
        else { proc.endSlot (m, t.timeSlot); proc.endSlot (m, t.levelSlot); }
    }
    void set (Tap& t, float ms, float db)
    {
        const int m = machine ? machine() : 0;
        t.ms = ms; t.db = db;
        if (t.util)
        {
            auto* p = utilTime (t);
            const bool sync = proc.apvts.getRawParameterValue ("dlSync")->load() > 0.5f;
            if (sync) { static const float div[] = { 0.0625f, 1.0f / 12, 0.125f, 1.0f / 6, 0.25f, 0.375f, 1.0f / 3, 0.5f, 0.75f, 2.0f / 3, 1.0f, 1.5f, 4.0f / 3, 2.0f, 3.0f, 4.0f };
                        int best = 0; for (int i = 1; i < 16; ++i) if (std::abs (div[i] * 500 - ms) < std::abs (div[best] * 500 - ms)) best = i;
                        p->setValueNotifyingHost (p->convertTo0to1 ((float) best)); }
            else p->setValueNotifyingHost (p->convertTo0to1 (jlimit (1.0f, 2990.0f, ms)));
            auto* mx = proc.apvts.getParameter ("dlMix"); mx->setValueNotifyingHost (mx->convertTo0to1 (db <= -47.5f ? 0.0f : std::pow (10.0f, db / 20.0f)));
            return;
        }
        proc.setSlot (m, t.timeSlot, l480::toNorm (proc.spec (m, t.timeSlot), ms));
        proc.setSlot (m, t.levelSlot, l480::toNorm (proc.spec (m, t.levelSlot), db <= -47.5f ? proc.spec (m, t.levelSlot).lo : db));
        if (onSlotTouched) { onSlotTouched (t.timeSlot); onSlotTouched (t.levelSlot); }
    }
    void show()
    {
        if (! onShow || drag < 0 || drag >= (int) taps.size()) return;
        const auto& t = taps[(size_t) drag];
        onShow ("Delay " + t.label, String (t.ms, t.ms < 100 ? 1 : 0) + " ms   " + (t.db <= -47.5f ? String ("off") : String (t.db, 1) + " dB"));
    }
    const Theme& th; Flexiverb480Processor& proc; Rectangle<float> plot; int drag = -1, hover = -1;
};

// ============================================================================ the screen: program lines, status line and a graph
class MainDisplay : public Component
{
public:
    MainDisplay (const Theme& t, Flexiverb480Processor& p) : th (t), decay (t, p), delay (t, p) { addAndMakeVisible (decay); addChildComponent (delay); }
    String lines[3]; float mL = 0, mR = 0; bool dim[3] = { false, false, false };
    std::function<void()> onTextClick; std::function<void()> onTextSizeMenu;
    DecayGraph decay; DelayGraph delay;
    void showDelay (bool d) { if (delay.isVisible() != d) { delay.setVisible (d); decay.setVisible (! d); } }
    int textHeight() const { return (int) (th.displayPt * 1.55f) * 3 + 14; }
    void resized() override
    {
        auto r = getLocalBounds().reduced (4);
        r.removeFromTop (textHeight());
        decay.setBounds (r); delay.setBounds (r);
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black.withAlpha (0.7f)); g.fillRoundedRectangle (r, 6);
        auto glass = r.reduced (3.0f);
        g.setGradientFill (ColourGradient (th.hi (th.display, 0.5f), glass.getX(), glass.getY(), th.lo (th.display, 0.8f), glass.getX(), glass.getBottom(), false));
        g.fillRoundedRectangle (glass, 4);
        auto txt = glass.reduced (8, 6).withHeight ((float) textHeight() - 8);
        auto meters = txt.removeFromLeft (26).reduced (2, 2);
        txt.removeFromLeft (8);
        const float lh = txt.getHeight() / 3.0f;
        for (int i = 0; i < 3; ++i)
        {
            const Colour c = dim[i] ? th.displayText.withAlpha (0.42f) : i == 2 ? th.displayText.withAlpha (0.85f) : th.displayText;
            segLine (g, lines[i], txt.withY (txt.getY() + i * lh).withHeight (lh), th.displayPt, c);
        }
        paintMeters (g, meters);
        g.setColour (th.displayText.withAlpha (0.15f)); g.drawHorizontalLine ((int) (glass.getY() + textHeight()), glass.getX() + 8, glass.getRight() - 8);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.05f), r.getX(), r.getY(), Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.3f, false));
        g.fillRoundedRectangle (r.reduced (3.0f).withHeight (r.getHeight() * 0.3f), 4);
    }
    void mouseDown (const MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onTextSizeMenu) onTextSizeMenu(); return; }
        if (e.y < textHeight() && onTextClick) onTextClick();
    }
private:
    void paintMeters (Graphics& g, Rectangle<float> r)
    {
        const float dbs[] = { -30, -24, -18, -12, -6, -3, 0 };
        const int n = 7; const float colW = r.getWidth() / 2.0f, segH = r.getHeight() / n;
        for (int ch = 0; ch < 2; ++ch)
        {
            const float lv = 20.0f * std::log10 (jmax (1.0e-6f, ch == 0 ? mL : mR));
            for (int i = 0; i < n; ++i)
            {
                auto seg = Rectangle<float> (r.getX() + ch * colW + 1, r.getBottom() - (i + 1) * segH + 1, colW - 2, segH - 2);
                const Colour c = i == n - 1 ? Colour (0xffff4020) : th.displayText;
                g.setColour (lv >= dbs[i] ? c : c.withAlpha (0.09f)); g.fillRect (seg);
            }
        }
    }
    const Theme& th;
};

// slider display: the full name and the value of each of the six sliders
class SliderDisplay : public Component
{
public:
    explicit SliderDisplay (const Theme& t) : th (t) {}
    String names[6], values[6]; int hi = -1;
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::black.withAlpha (0.7f)); g.fillRoundedRectangle (r, 5);
        auto glass = r.reduced (2.5f);
        g.setGradientFill (ColourGradient (th.hi (th.display, 0.5f), glass.getX(), glass.getY(), th.lo (th.display, 0.8f), glass.getX(), glass.getBottom(), false));
        g.fillRoundedRectangle (glass, 4);
        const float w = glass.getWidth() / 6.0f;
        for (int i = 0; i < 6; ++i)
        {
            auto c = Rectangle<float> (glass.getX() + i * w, glass.getY(), w, glass.getHeight()).reduced (3, 3);
            if (i > 0) { g.setColour (th.displayText.withAlpha (0.12f)); g.drawVerticalLine ((int) c.getX() - 3, c.getY() + 2, c.getBottom() - 2); }
            if (i == hi) { g.setColour (th.displayText.withAlpha (0.10f)); g.fillRoundedRectangle (c, 3); }
            if (names[i].isEmpty()) continue;
            auto nr = c.removeFromTop (c.getHeight() * 0.55f);
            g.setColour (th.displayText);
            float fh = jmin (th.displayPt * 0.78f, nr.getHeight() * 0.48f);
            g.setFont (ui (fh, W::Semibold));
            g.drawFittedText (names[i], nr.toNearestInt(), Justification::centred, 2, 0.8f);
            g.setColour (th.displayText.withAlpha (0.85f)); g.setFont (ui (jmin (th.displayPt * 0.8f, c.getHeight() * 0.8f), W::Bold));
            g.drawFittedText (values[i], c.toNearestInt(), Justification::centred, 1, 0.7f);
        }
    }
private:
    const Theme& th;
};

// parameter names in plain words: the 480L manual's abbreviations written out
static String plainName (const String& in)
{
    static const std::map<String, String> word {
        { "L", "Left" }, { "R", "Right" }, { "Ch", "" }, { "Freq", "Frequency" }, { "Hi", "High" }, { "Lo", "Low" }, { "Const", "" },
        { "Pt", "Point" }, { "Ind", "Independent" }, { "X-over", "Crossover" }, { "Xover", "Crossover" }, { "LF", "Bass" }, { "HF", "Treble" },
        { "Q", "Width" }, { "Amp", "Amplitude" }, { "Pch", "Pitch" }, { "DC", "Offset" }, { "Atten", "Attenuation" }, { "Comp", "Compression" },
        { "Max", "Maximum" }, { "Fx", "Effect" }, { "Ref", "Reference" }, { "Rel", "Release" }, { "Atk", "Attack" }, { "Dly", "Delay" } };
    StringArray t; t.addTokens (in, " ", "");
    StringArray out;
    for (auto w : t)
    {
        if (w.length() == 3 && w[1] == '>') { out.add (String (w[0] == 'L' ? "Left" : "Right") + " to " + (w[2] == 'L' ? "Left" : "Right")); continue; }
        if (w == "L-R") { out.add ("Left to Right"); continue; }
        if (w == "R-L") { out.add ("Right to Left"); continue; }
        if (w.startsWith ("DLY")) { out.add ("Delay " + w.substring (3)); continue; }
        if (w.endsWith ("/Pch")) w = w.replace ("/Pch", "/Pitch");
        auto it = word.find (w);
        if (it != word.end()) { if (it->second.isNotEmpty()) out.add (it->second); continue; }
        out.add (w);
    }
    return out.joinIntoString (" ").replace ("Auto Offset Cut", "Auto Offset Removal").replace ("Offset Left Offset", "Left Offset").replace ("Offset Right Offset", "Right Offset");
}

// ============================================================================ CONTROL pages: setup on the sliders
struct CtrlSlot { const char* id; const char* name; };
static constexpr int kCtrlPages = 4;
static const CtrlSlot kCtrl[kCtrlPages][6] = {
    { { "out", "Output Level" }, { "hwOn", "480L Hardware" }, { "hwIn", "Input Type" }, { "hwOut", "Output Type" }, { "hwNoise", "Converter Noise" }, {} },
    { { "hwXfIn", "Input Transformers" }, { "hwXfOut", "Output Transformers" }, { "trOn", "Iron" }, { "trModel", "Iron Model" }, { "trFreq", "Iron Frequency" }, { "trAmt", "Iron Amount" } },
    { { "dlOn", "Delay On" }, { "dlSync", "Tempo Sync" }, { "dlTimeL", "Delay Left" }, { "dlTimeR", "Delay Right" }, { "dlFb", "Feedback" }, { "dlMix", "Delay Level" } },
    { { "dlPing", "Ping-Pong" }, { "dlLow", "Delay Bass" }, { "dlHigh", "Delay Treble" }, {}, {}, {} },
};
static const char* kCtrlPageName[kCtrlPages] = { "Setup", "Iron", "Delay", "Delay 2" };

// ============================================================================ preset browser: every preset at once, by category
class PresetBrowser : public Component
{
public:
    PresetBrowser (const Theme& t, Flexiverb480Processor& p) : th (t), proc (p) { addAndMakeVisible (view); view.setViewedComponent (&list, false); view.setScrollBarsShown (true, false); }
    std::function<void (int program)> onFactory; std::function<void (const String&)> onUser; std::function<void()> onClose;
    std::function<int()> machine;
    void refresh() { list.build (*this); resized(); repaint(); }
    void resized() override { auto r = getLocalBounds().reduced (16); r.removeFromTop (34); view.setBounds (r); list.setSize (r.getWidth() - 12, list.heightFor (r.getWidth() - 12)); list.layout(); }
    void paint (Graphics& g) override
    {
        g.fillAll (Colours::black.withAlpha (0.82f));
        auto r = getLocalBounds().reduced (16).removeFromTop (30).toFloat();
        drawLegend (g, "PRESETS  -  machine " + String (machine && machine() == 1 ? "B" : "A"), r, ui (16.0f, W::Bold, 0.1f), th.panelText, Justification::centredLeft);
        drawLegend (g, "click a preset to load it  -  click here to close", r, ui (11.0f, W::Medium), th.panelText.withAlpha (0.6f), Justification::centredRight);
    }
    void mouseDown (const MouseEvent&) override { if (onClose) onClose(); }
private:
    struct Entry { String name; int program = -1; String user; Rectangle<int> r; };
    struct Box { String title; std::vector<Entry> items; Rectangle<int> r; };
    struct Section { String title; std::vector<Box> boxes; int y = 0; };
    struct List : public Component
    {
        PresetBrowser* owner = nullptr; std::vector<Section> sections;
        void build (PresetBrowser& b)
        {
            owner = &b; sections.clear();
            auto sec = [this] (const String& t, int b0, int b1)
            {
                Section s; s.title = t;
                for (int bank = b0; bank <= b1; ++bank)
                {
                    Box bx; bx.title = l480::bankName (bank);
                    for (int i = 0; i < l480::numPrograms(); ++i) if (l480::programs()[(size_t) i].bank == bank) bx.items.push_back ({ l480::programs()[(size_t) i].name, i, {}, {} });
                    if (! bx.items.empty()) s.boxes.push_back (bx);
                }
                sections.push_back (s);
            };
            sec ("480L", 1, 21); sec ("Surround / HD Cart", 22, 26); sec ("Matched to the 480L impulse responses", 27, 32); sec ("480L II  (the earlier Flexiverb's IR-matched presets)", 33, 37);
            Section u; u.title = "Your presets  (" + Flexiverb480Processor::presetFolder().getFullPathName() + ")";
            Box ub; ub.title = "Saved with SAVE";
            for (auto& n : b.proc.userPresets()) ub.items.push_back ({ n, -1, n, {} });
            if (ub.items.empty()) ub.items.push_back ({ "(none yet)", -2, {}, {} });
            u.boxes.push_back (ub); sections.push_back (u);
        }
        static constexpr int kItemH = 17, kTitleH = 22, kSecH = 30, kBoxW = 168;
        int cols (int w) const { return jmax (1, w / kBoxW); }
        int heightFor (int w)
        {
            int y = 0; const int nc = cols (w);
            for (auto& s : sections)
            {
                y += kSecH;
                for (size_t i = 0; i < s.boxes.size(); i += (size_t) nc)
                {
                    int h = 0; for (size_t k = i; k < jmin (s.boxes.size(), i + (size_t) nc); ++k) h = jmax (h, kTitleH + (int) s.boxes[k].items.size() * kItemH + 8);
                    y += h + 8;
                }
            }
            return y + 10;
        }
        void layout()
        {
            const int w = getWidth(), nc = cols (w), bw = w / nc; int y = 0;
            for (auto& s : sections)
            {
                s.y = y; y += kSecH;
                for (size_t i = 0; i < s.boxes.size(); i += (size_t) nc)
                {
                    int h = 0;
                    for (size_t k = i; k < jmin (s.boxes.size(), i + (size_t) nc); ++k)
                    {
                        auto& bx = s.boxes[k]; const int x = (int) (k - i) * bw;
                        const int bh = kTitleH + (int) bx.items.size() * kItemH + 8; h = jmax (h, bh);
                        bx.r = { x, y, bw - 8, bh };
                        for (size_t e = 0; e < bx.items.size(); ++e) bx.items[e].r = { x + 6, y + kTitleH + (int) e * kItemH, bw - 20, kItemH };
                    }
                    y += h + 8;
                }
            }
        }
        void paint (Graphics& g) override
        {
            const auto& th = owner->th; const int m = owner->machine ? owner->machine() : 0;
            const int cur = owner->proc.userPresetName.isEmpty() ? owner->proc.programOf (m) : -1;
            for (auto& s : sections)
            {
                drawLegend (g, s.title, Rectangle<float> (0, (float) s.y + 4, (float) getWidth(), 20), ui (13.0f, W::Bold, 0.08f), th.keyLight, Justification::centredLeft);
                for (auto& bx : s.boxes)
                {
                    g.setColour (th.panel.withAlpha (0.9f)); g.fillRoundedRectangle (bx.r.toFloat(), 5);
                    g.setColour (th.panelText.withAlpha (0.2f)); g.drawRoundedRectangle (bx.r.toFloat(), 5, 1.0f);
                    drawLegend (g, bx.title, bx.r.toFloat().withHeight ((float) kTitleH).reduced (6, 0), ui (11.0f, W::Bold, 0.06f), th.panelText.withAlpha (0.75f), Justification::centredLeft);
                    for (auto& e : bx.items)
                    {
                        const bool isCur = (e.program >= 0 && e.program == cur) || (e.user.isNotEmpty() && e.user == owner->proc.userPresetName);
                        if (isCur) { g.setColour (th.keyLight.withAlpha (0.85f)); g.fillRoundedRectangle (e.r.toFloat(), 3); }
                        else if (e.r.contains (hoverPt)) { g.setColour (th.panelText.withAlpha (0.12f)); g.fillRoundedRectangle (e.r.toFloat(), 3); }
                        g.setColour (isCur ? Colours::black : th.panelText.withAlpha (e.program == -2 ? 0.4f : 0.95f)); g.setFont (ui (12.0f, W::Medium));
                        g.drawText (e.name, e.r.reduced (4, 0), Justification::centredLeft, true);
                    }
                }
            }
        }
        Point<int> hoverPt;
        void mouseMove (const MouseEvent& e) override { hoverPt = e.getPosition(); repaint(); }
        void mouseDown (const MouseEvent& e) override
        {
            for (auto& s : sections) for (auto& bx : s.boxes) for (auto& it : bx.items)
                if (it.r.contains (e.getPosition()))
                {
                    if (it.program >= 0 && owner->onFactory) owner->onFactory (it.program);
                    else if (it.user.isNotEmpty() && owner->onUser) owner->onUser (it.user);
                    return;
                }
        }
    };
    const Theme& th; Flexiverb480Processor& proc; Viewport view; List list;
};

// ============================================================================ SETUP: colours, gradient, text sizes, quality
class SetupPanel : public Component
{
public:
    SetupPanel (Theme& t, Flexiverb480Processor& p) : th (t), proc (p)
    {
        auto addSl = [this] (Slider& s, double lo, double hi, double step)
        { s.setSliderStyle (Slider::LinearHorizontal); s.setTextBoxStyle (Slider::TextBoxRight, false, 50, 20); s.setRange (lo, hi, step); addAndMakeVisible (s); s.onValueChange = [this] { fromSliders(); }; };
        addSl (grad, 0, 1, 0.01); addSl (keyPt, 9, 20, 0.5); addSl (knobPt, 8, 18, 0.5); addSl (dispPt, 10, 26, 0.5);
        for (auto* c : { &osBox, &precBox }) addAndMakeVisible (*c);
        osBox.addItemList ({ "Off", "2x", "4x", "6x", "8x" }, 1); precBox.addItemList ({ "32-bit", "64-bit" }, 1);
        osAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "os", osBox);
        precAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "prec", precBox);
        for (auto* b : { &resetBtn, &saveBtn, &closeBtn }) addAndMakeVisible (*b);
        resetBtn.onClick = [this] { th = Theme(); sync(); if (onChange) onChange(); };
        saveBtn.onClick = [this] { Theme::defaultFile().getParentDirectory().createDirectory(); Theme::defaultFile().replaceWithText (th.toString()); saveBtn.setButtonText ("SAVED AS DEFAULT"); };
        closeBtn.onClick = [this] { if (onClose) onClose(); };
        sync();
    }
    std::function<void()> onChange, onClose;
    void sync()
    {
        grad.setValue (th.gradient, dontSendNotification); keyPt.setValue (th.keyPt, dontSendNotification);
        knobPt.setValue (th.knobPt, dontSendNotification); dispPt.setValue (th.displayPt, dontSendNotification);
        saveBtn.setButtonText ("SAVE AS DEFAULT FOR NEW WINDOWS"); repaint();
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (30); r.removeFromTop (44);
        auto left = r.removeFromLeft (jmin (260, r.getWidth() / 2 - 10)), right = r.withTrimmedLeft (20);
        const int n = (int) Theme::items().size();
        swatches.clear();
        for (int i = 0; i < n; ++i) swatches.push_back (Rectangle<int> (left.getX(), left.getY() + 24 + i * 36, left.getWidth() - 12, 30));
        int y = right.getY() + 24;
        for (auto* s : { &grad, &keyPt, &knobPt, &dispPt }) { s->setBounds (right.getX() + 110, y, right.getWidth() - 110, 24); y += 36; }
        y += 34;
        osBox.setBounds (right.getX() + 120, y, jmin (160, right.getWidth() - 120), 26); y += 36;
        precBox.setBounds (right.getX() + 120, y, jmin (160, right.getWidth() - 120), 26); y += 60;
        resetBtn.setBounds (right.getX(), y, right.getWidth(), 30); y += 38;
        saveBtn.setBounds (right.getX(), y, right.getWidth(), 30); y += 38;
        closeBtn.setBounds (right.getX(), y, right.getWidth(), 30);
        rightX = right.getX(); rightTop = right.getY();
    }
    void paint (Graphics& g) override
    {
        g.fillAll (Colours::black.withAlpha (0.88f));
        auto r = getLocalBounds().reduced (30);
        drawLegend (g, "SETUP", r.removeFromTop (34).toFloat(), ui (20.0f, W::Bold, 0.12f), th.panelText, Justification::centredLeft);
        drawLegend (g, "Colours  (click one to change it)", Rectangle<float> ((float) r.getX(), (float) r.getY() + 10, 300, 18), ui (12.0f, W::Bold), th.keyLight, Justification::centredLeft);
        int i = 0;
        for (auto& it : Theme::items())
        {
            auto s = swatches[(size_t) i++].toFloat();
            auto box = s.removeFromLeft (44);
            g.setGradientFill (ColourGradient (th.hi (th.*it.c), box.getX(), box.getY(), th.lo (th.*it.c), box.getX(), box.getBottom(), false));
            g.fillRoundedRectangle (box, 4); g.setColour (Colours::white.withAlpha (0.4f)); g.drawRoundedRectangle (box, 4, 1.0f);
            drawLegend (g, it.name, s.withTrimmedLeft (8), ui (12.0f, W::Medium), th.panelText, Justification::centredLeft);
        }
        const float x = (float) rightX; float y = (float) rightTop;
        drawLegend (g, "Look", Rectangle<float> (x, y + 10, 200, 18), ui (12.0f, W::Bold), th.keyLight, Justification::centredLeft);
        const char* lab[] = { "Gradient", "Button text", "Knob text", "Screen text" };
        for (int k = 0; k < 4; ++k) drawLegend (g, lab[k], Rectangle<float> (x, y + 24 + k * 36, 108, 24), ui (12.0f, W::Medium), th.panelText, Justification::centredLeft);
        y += 24 + 4 * 36 + 10;
        drawLegend (g, "Quality  (analog, EQ, compressor)", Rectangle<float> (x, y, 300, 18), ui (12.0f, W::Bold), th.keyLight, Justification::centredLeft);
        drawLegend (g, "Oversampling", Rectangle<float> (x, y + 24, 145, 26), ui (12.0f, W::Medium), th.panelText, Justification::centredLeft);
        drawLegend (g, "Precision", Rectangle<float> (x, y + 60, 145, 26), ui (12.0f, W::Medium), th.panelText, Justification::centredLeft);
    }
    void mouseDown (const MouseEvent& e) override
    {
        int i = 0;
        for (auto& it : Theme::items())
        {
            if (swatches[(size_t) i++].contains (e.getPosition()))
            {
                auto sel = std::make_unique<ColourSelector> (ColourSelector::showColourspace | ColourSelector::showSliders | ColourSelector::showColourAtTop);
                sel->setSize (300, 300); sel->setCurrentColour (th.*it.c);
                struct L : ChangeListener { SetupPanel* p; Colour Theme::* c; void changeListenerCallback (ChangeBroadcaster* b) override { p->th.*c = static_cast<ColourSelector*> (b)->getCurrentColour(); p->repaint(); if (p->onChange) p->onChange(); } };
                auto* l = new L(); l->p = this; l->c = it.c; sel->addChangeListener (l); listener.reset (l);
                CallOutBox::launchAsynchronously (std::move (sel), swatches[(size_t) i - 1], this);
                return;
            }
        }
    }
    Theme& th;
private:
    void fromSliders() { th.gradient = (float) grad.getValue(); th.keyPt = (float) keyPt.getValue(); th.knobPt = (float) knobPt.getValue(); th.displayPt = (float) dispPt.getValue(); repaint(); if (onChange) onChange(); }
    Flexiverb480Processor& proc;
    Slider grad, keyPt, knobPt, dispPt; ComboBox osBox, precBox;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> osAtt, precAtt;
    TextButton resetBtn { "RESET THE LOOK" }, saveBtn { "SAVE AS DEFAULT FOR NEW WINDOWS" }, closeBtn { "CLOSE" };
    std::vector<Rectangle<int>> swatches; int rightX = 0, rightTop = 0;
    std::unique_ptr<ChangeListener> listener;
};

struct FlexLnf : public LookAndFeel_V4
{
    Font getPopupMenuFont() override { return ui (13.5f, W::Medium); }
    Font getComboBoxFont (ComboBox&) override { return ui (13.0f, W::Medium); }
    Font getTextButtonFont (TextButton&, int) override { return ui (12.5f, W::Bold); }
};

// ============================================================================ the editor
class Flexiverb480Editor : public AudioProcessorEditor, private Timer
{
public:
    static constexpr int kL = 540, kH = 860, kTab = 24, kPanel = 440;

    explicit Flexiverb480Editor (Flexiverb480Processor& p) : AudioProcessorEditor (p), proc (p), display (theme, p), sliderDisplay (theme), browser (theme, p), setup (theme, p)
    {
        // the look: this session's, else your saved default, else the built-in one
        if (! theme.fromString (proc.getUiProperty ("themeData", String()).toString())) theme.fromString (Theme::defaultFile().loadFileAsString());
        scale = (float) (double) proc.getUiProperty ("uiScale", 0.85);
        drawerOpen = (bool) proc.getUiProperty ("uiDrawer", false);

        addAndMakeVisible (content); content.ed = this;
        content.addAndMakeVisible (display);
        display.onTextClick = [this] { openBrowser(); };
        display.onTextSizeMenu = [this] { textSizeMenu(); };
        display.decay.machine = [this] { return machine; };
        display.delay.machine = [this] { return machine; };
        display.delay.utility = [this] { return ctrl && (ctrlPage == 2 || ctrlPage == 3); };
        auto shower = [this] (const String& a, const String& b) { showMessage (a, b, 1500); };
        display.decay.onShow = shower; display.delay.onShow = shower;
        content.addAndMakeVisible (sliderDisplay);

        auto mk = [this] (const String& t) { auto k = std::make_unique<LarcKey> (theme, t); content.addAndMakeVisible (*k); return k; };
        prevKey = mk ("<"); nextKey = mk (">"); saveKey = mk ("SAVE");
        prevKey->onClick = [this] { stepPreset (-1); }; nextKey->onClick = [this] { stepPreset (1); }; saveKey->onClick = [this] { savePreset(); };
        machAKey = mk ("MACHINE A"); machBKey = mk ("MACHINE B"); combineKey = mk ("ONE MACHINE");
        machAKey->onClick = [this] { selectMachine (0); }; machBKey->onClick = [this] { selectMachine (1); };
        combineKey->onClick = [this] { auto* c = proc.apvts.getParameter ("config"); const int v = (config() + 1) % 4; c->beginChangeGesture(); c->setValueNotifyingHost (c->convertTo0to1 ((float) v)); c->endChangeGesture(); };
        pageKey = mk ("PAGE"); ctrlKey = mk ("CONTROL"); muteKey = mk ("MUTE"); setupKey = mk ("SETUP");
        pageKey->onClick = [this] { pressPage(); }; ctrlKey->onClick = [this] { ctrl = ! ctrl; refreshPage (true); msgUntil = 0; };
        muteKey->onClick = [this] { proc.mute.store (! proc.mute.load()); };
        setupKey->onClick = [this] { setup.setVisible (! setup.isVisible()); if (setup.isVisible()) { setup.sync(); setup.toFront (true); } };
        content.addAndMakeVisible (presetName);
        presetName.ed = this;

        for (int i = 0; i < 6; ++i)
        {
            fader[i] = std::make_unique<LarcFader> (theme); content.addAndMakeVisible (*fader[i]);
            fader[i]->onBegin = [this, i] { faderBegin (i); };
            fader[i]->onMove  = [this, i] (float t) { faderMove (i, t); };
            fader[i]->onEnd   = [this, i] { faderEnd (i); };
            fader[i]->onReset = [this, i] { faderReset (i); };
        }

        // drawer: ANALOG, EQ, COMPRESSOR
        content.addAndMakeVisible (drawer); drawer.ed = this;
        auto pk = [this] (const String& id, const String& t) { auto k = std::make_unique<ParamKey> (theme, proc.apvts, id, t); drawer.addAndMakeVisible (*k); return k; };
        anOnKey = pk ("anOn", "ANALOG"); eqOnKey = pk ("eqOn", "EQ"); cpOnKey = pk ("cpOn", "COMPRESSOR"); satKey = pk ("cpColor", "SATURATE"); duckKey = pk ("cpDuck", "DUCKING");
        auto knob = [this, shower] (const String& id, const String& t, std::vector<ScaleMark> m, Colour Theme::* c)
        { auto k = std::make_unique<ScaleKnob> (theme, proc.apvts, id, t, std::move (m), c); k->onShow = shower; drawer.addAndMakeVisible (*k); return k; };
        anKnob[0] = knob ("anDrive", "DRIVE", { { 0, "0" }, { 6, "6" }, { 12, "12" }, { 18, "18" }, { 24, "24" }, { 30, "30" } }, &Theme::analogKnob);
        anKnob[1] = knob ("anOut", "OUTPUT", { { -12, "-12" }, { -6, "-6" }, { 0, "0" }, { 6, "+6" }, { 12, "+12" } }, &Theme::analogKnob);
        anKnob[2] = knob ("anModel", "MODEL", { { 0, "CLEAN" }, { 1, "TUBE" }, { 2, "1073" }, { 3, "1080" }, { 4, "212" } }, &Theme::analogKnob);
        anKnob[3] = knob ("anColour", "CHARACTER", { { 0, "ODD" }, { 1, "EVEN" }, { 2, "HEAVY" }, { 3, "CRUSH" } }, &Theme::analogKnob);
        anKnob[4] = knob ("anTarget", "AFFECTS", { { 0, "REVERB" }, { 1, "EQ" }, { 2, "BOTH" } }, &Theme::analogKnob);
        const char* bn[] = { "LOW", "LOW MID", "HIGH MID", "HIGH" };
        const std::vector<ScaleMark> fm[4] = {
            { { 20, "20" }, { 50, "50" }, { 100, "100" }, { 200, "200" }, { 500, "500" } },
            { { 100, "100" }, { 200, "200" }, { 500, "500" }, { 1000, "1k" }, { 2000, "2k" } },
            { { 800, ".8k" }, { 1500, "1.5k" }, { 3000, "3k" }, { 5000, "5k" }, { 8000, "8k" } },
            { { 2000, "2k" }, { 4000, "4k" }, { 8000, "8k" }, { 12000, "12k" }, { 20000, "20k" } } };
        for (int b = 0; b < 4; ++b) { eqKnob[b] = std::make_unique<DualKnob> (theme, proc.apvts, "eqF" + String (b), "eqG" + String (b), bn[b], fm[b]); eqKnob[b]->onShow = shower; drawer.addAndMakeVisible (*eqKnob[b]); }
        cpKnob[0] = knob ("cpType", "TYPE", { { 0, "VCA" }, { 1, "FET" }, { 2, "OPTO" } }, &Theme::compKnob);
        cpKnob[1] = knob ("cpThr", "THRESHOLD", { { -60, "-60" }, { -48, "-48" }, { -36, "-36" }, { -24, "-24" }, { -12, "-12" }, { 0, "0" } }, &Theme::compKnob);
        cpKnob[2] = knob ("cpRatio", "RATIO", { { 1, "1" }, { 2, "2" }, { 4, "4" }, { 8, "8" }, { 20, "20" } }, &Theme::compKnob);
        cpKnob[3] = knob ("cpAtk", "ATTACK", { { 0.05f, ".05" }, { 1, "1" }, { 10, "10" }, { 100, "100" }, { 300, "300" } }, &Theme::compKnob);
        cpKnob[4] = knob ("cpRel", "RELEASE", { { 20, "20" }, { 100, "100" }, { 500, "500" }, { 2000, "2k" }, { 5000, "5k" } }, &Theme::compKnob);
        cpKnob[5] = knob ("cpMakeup", "MAKEUP", { { 0, "0" }, { 6, "6" }, { 12, "12" }, { 18, "18" }, { 24, "24" } }, &Theme::compKnob);
        cpKnob[6] = knob ("cpMix", "MIX", { { 0, "0" }, { 0.25f, "25" }, { 0.5f, "50" }, { 0.75f, "75" }, { 1, "100" } }, &Theme::compKnob);

        // overlays
        content.addChildComponent (browser);
        browser.machine = [this] { return machine; };
        browser.onClose = [this] { browser.setVisible (false); };
        browser.onFactory = [this] (int i) { proc.loadProgram (machine, i); page = 0; ctrl = false; refreshPage (true); browser.setVisible (false); msgUntil = 0; };
        browser.onUser = [this] (const String& n) { proc.loadUserPreset (n); page = 0; ctrl = false; refreshPage (true); browser.setVisible (false); msgUntil = 0; };
        content.addChildComponent (setup);
        setup.onClose = [this] { setup.setVisible (false); };
        setup.onChange = [this] { lookChanged(); };

        lnf.setColour (PopupMenu::backgroundColourId, Colour (0xff1c1d1f)); lnf.setColour (PopupMenu::textColourId, Colour (0xffe8e6e1));
        lnf.setColour (PopupMenu::highlightedBackgroundColourId, theme.keyLight.darker (0.2f)); lnf.setColour (PopupMenu::highlightedTextColourId, Colours::black);
        lnf.setColour (PopupMenu::headerTextColourId, Colour (0xff9a968e));
        lnf.setDefaultSansSerifTypeface (FlexFonts::get().medium);
        setLookAndFeel (&lnf);
        layout();
        setResizable (true, true);
        applySize();
        refreshPage (true);
        startTimerHz (30);
    }
    ~Flexiverb480Editor() override { stopTimer(); setLookAndFeel (nullptr); }

    void paint (Graphics& g) override { g.fillAll (theme.cheek.darker (0.5f)); }
    void resized() override
    {
        // fit both ways and centre: if the host does not give exactly the asked size, everything still shows whole
        const float sx = getWidth() / (float) baseWidth(), sy = getHeight() / (float) kH;
        const float sc = jlimit (0.3f, 3.0f, jmin (sx, sy));
        const float dx = jmax (0.0f, (getWidth() - baseWidth() * sc) * 0.5f), dy = jmax (0.0f, (getHeight() - kH * sc) * 0.5f);
        content.setTransform (AffineTransform::scale (sc).translated (dx, dy));
        if (! applying) { scale = sc; proc.setUiProperty ("uiScale", scale); }
    }

    struct Content : public Component
    {
        Flexiverb480Editor* ed = nullptr;
        void paint (Graphics& g) override { if (ed) ed->paintPanel (g); }
        void mouseDown (const MouseEvent& e) override
        {
            if (! ed) return;
            if (e.mods.isPopupMenu()) ed->showSizeMenu();
            else if (e.x >= kL && e.x < kL + kTab) ed->toggleDrawer();
        }
    };
    struct Drawer : public Component
    {
        Flexiverb480Editor* ed = nullptr;
        void paint (Graphics& g) override { if (ed) ed->paintDrawer (g); }
        void mouseDown (const MouseEvent& e) override { if (ed && e.mods.isPopupMenu()) ed->showSizeMenu(); }
    };
    // the preset name in the preset bar: click for the browser
    struct PresetName : public Component
    {
        Flexiverb480Editor* ed = nullptr; String text, sub;
        void paint (Graphics& g) override
        {
            const auto& th = ed->theme; auto r = getLocalBounds().toFloat();
            g.setColour (Colours::black.withAlpha (0.7f)); g.fillRoundedRectangle (r, 5);
            auto in = r.reduced (2.5f);
            g.setGradientFill (ColourGradient (th.hi (th.display, 0.5f), in.getX(), in.getY(), th.lo (th.display, 0.8f), in.getX(), in.getBottom(), false));
            g.fillRoundedRectangle (in, 4);
            if (isMouseOver()) { g.setColour (th.displayText.withAlpha (0.08f)); g.fillRoundedRectangle (in, 4); }
            auto t = in.reduced (10, 0);
            g.setColour (th.displayText.withAlpha (0.6f)); g.setFont (ui (jmin (12.0f, th.keyPt * 0.85f), W::Medium));
            g.drawText (sub + "   v", t, Justification::centredRight, true);
            g.setColour (th.displayText); g.setFont (ui (jmin (17.0f, th.keyPt * 1.2f), W::Bold));
            g.drawText (text, t, Justification::centredLeft, true);
        }
        void mouseEnter (const MouseEvent&) override { repaint(); }
        void mouseExit (const MouseEvent&) override { repaint(); }
        void mouseDown (const MouseEvent&) override { ed->openBrowser(); }
    };

private:
    // ---------------------------------------------------------------- painting
    void paintPanel (Graphics& g)
    {
        auto r = Rectangle<float> (0, 0, (float) kL, (float) kH);
        const float sw = 14.0f;
        for (auto sr : { r.withWidth (sw), r.withLeft (r.getRight() - sw) })
        {
            g.setGradientFill (ColourGradient (theme.hi (theme.cheek), sr.getX(), sr.getY(), theme.lo (theme.cheek), sr.getRight(), sr.getBottom(), false));
            g.fillRect (sr);
        }
        auto face = r.reduced (sw, 0);
        g.setGradientFill (ColourGradient (theme.hi (theme.panel, 0.4f), face.getX(), face.getY(), theme.lo (theme.panel, 0.5f), face.getX(), face.getBottom(), false));
        g.fillRect (face);
        for (int i = 0; i < 6; ++i)
        {
            auto b = fader[i]->getBounds().toFloat();
            drawLegend (g, String (i + 1), Rectangle<float> (b.getX(), b.getBottom() + 1, b.getWidth(), 12), ui (9.0f, W::Bold), theme.panelText.withAlpha (0.55f));
        }
        auto np = namePlate;
        const auto f1 = ui (19.0f, W::Bold, -0.02f), f2 = ui (19.0f, W::Regular);
        const float w1 = textW (f1, "flexiverb"), w2 = textW (f2, " 480"), x = np.getCentreX() - (w1 + w2) / 2;
        drawLegend (g, "flexiverb", Rectangle<float> (x, np.getY(), w1 + 2, 24), f1, theme.panelText, Justification::centredLeft);
        drawLegend (g, " 480", Rectangle<float> (x + w1, np.getY(), w2 + 4, 24), f2, theme.panelText.withAlpha (0.6f), Justification::centredLeft);
        // drawer tab
        auto tab = Rectangle<float> ((float) kL, 0, (float) kTab, (float) kH);
        g.setGradientFill (ColourGradient (theme.hi (theme.cheek), tab.getX(), 0, theme.lo (theme.cheek), tab.getRight(), 0, false));
        g.fillRect (tab);
        g.setColour (Colours::black.withAlpha (0.6f)); g.drawVerticalLine (kL, 0, (float) kH);
        {
            Graphics::ScopedSaveState ss (g);
            g.addTransform (AffineTransform::rotation (-MathConstants<float>::halfPi, tab.getCentreX(), tab.getCentreY()));
            auto t = Rectangle<float> (300, 16).withCentre (tab.getCentre());
            g.setColour (drawerOpen ? theme.keyLight : theme.panelText.withAlpha (0.85f)); g.setFont (ui (10.0f, W::Bold, 0.18f));
            g.drawText (drawerOpen ? "CLOSE   ANALOG  -  EQ  -  COMPRESSOR" : "OPEN   ANALOG  -  EQ  -  COMPRESSOR", t, Justification::centred, false);
        }
    }
    void paintDrawer (Graphics& g)
    {
        auto r = drawer.getLocalBounds().toFloat();
        g.setGradientFill (ColourGradient (theme.hi (theme.panel, 0.4f), 0, 0, theme.lo (theme.panel, 0.5f), 0, r.getBottom(), false));
        g.fillRect (r);
        auto sr = r.withLeft (r.getRight() - 14);
        g.setGradientFill (ColourGradient (theme.hi (theme.cheek), sr.getX(), 0, theme.lo (theme.cheek), sr.getRight(), 0, false)); g.fillRect (sr);
        for (auto& s : sections)
        {
            auto b = s.area.toFloat().expanded (8, 6).withTrimmedTop (-10);
            g.setColour (theme.panelText.withAlpha (0.28f)); g.drawRoundedRectangle (b, 4, 1.0f);
            const auto f = ui (jmax (9.0f, theme.knobPt * 0.85f), W::Bold, 0.14f); const float tw = textW (f, s.title) + 12;
            auto tr = Rectangle<float> (b.getX() + 10, b.getY() - 7, tw, 14);
            g.setColour (theme.panel); g.fillRect (tr);
            drawLegend (g, s.title, tr, f, theme.panelText.withAlpha (0.85f));
        }
    }

    // ---------------------------------------------------------------- layout
    Rectangle<float> namePlate;
    struct Sec { String title; Rectangle<int> area; };
    std::vector<Sec> sections;
    int keyH() const { return roundToInt (theme.keyPt * 2.0f + 12.0f); }
    void layout()
    {
        const int x0 = 28, wAll = kL - 56, gap = 6, kh = keyH();
        int y = 14;
        // preset bar
        prevKey->setBounds (x0, y, kh + 4, kh); nextKey->setBounds (x0 + wAll - (int) saveKey->preferredWidth() - gap - kh - 4, y, kh + 4, kh);
        saveKey->setBounds (x0 + wAll - (int) saveKey->preferredWidth(), y, (int) saveKey->preferredWidth(), kh);
        presetName.setBounds (prevKey->getRight() + gap, y, nextKey->getX() - gap - (prevKey->getRight() + gap), kh);
        y += kh + 12;
        // the screen
        const int dispH = display.textHeight() + 190;
        display.setBounds (x0, y, wAll, dispH); y += dispH + 12;
        // keys
        machAKey->setVisible (config() != 0); machBKey->setVisible (config() != 0);
        y = flowKeys ({ machAKey.get(), machBKey.get(), combineKey.get() }, x0, wAll, y, kh, gap);
        y = flowKeys ({ pageKey.get(), ctrlKey.get(), muteKey.get(), setupKey.get() }, x0, wAll, y, kh, gap);
        y += 6;
        const int sdH = roundToInt (theme.displayPt * 2.6f + 14);
        sliderDisplay.setBounds (x0, y, wAll, sdH); y += sdH + 8;
        const int fh = jmax (120, kH - y - 64), colW = wAll / 6;
        for (int i = 0; i < 6; ++i) fader[i]->setBounds (x0 + i * colW, y, colW, fh);
        namePlate = Rectangle<float> ((float) x0, (float) (kH - 44), (float) wAll, 30);
        browser.setBounds (0, 0, baseWidth(), kH); setup.setBounds (0, 0, baseWidth(), kH);
        layoutDrawer();
    }
    void layoutDrawer()
    {
        sections.clear();
        const int x0 = 26, wAll = kPanel - 26 - 30, gap = 6, kh = keyH(), kTxt = roundToInt (theme.knobPt) + 5;
        int y = 30;
        // ANALOG (the ChannelStrip drive), above the EQ
        int top = y;
        anOnKey->setBounds (x0 + wAll - (int) anOnKey->preferredWidth(), y - 2, (int) anOnKey->preferredWidth(), kh); y += kh + 8;
        const int aw = wAll / 3;
        anKnob[0]->setBounds (x0, y, aw, 92 + kTxt); anKnob[1]->setBounds (x0 + aw, y, aw, 92 + kTxt); anKnob[4]->setBounds (x0 + 2 * aw, y, aw, 92 + kTxt);
        y += 96 + kTxt;
        anKnob[2]->setBounds (x0 + aw / 2, y, aw, 92 + kTxt); anKnob[3]->setBounds (x0 + aw / 2 + aw, y, aw, 92 + kTxt);
        y += 92 + kTxt; sections.push_back ({ "ANALOG", { x0, top, wAll, y - top } });
        y += 30;
        // EQ
        top = y;
        eqOnKey->setBounds (x0 + wAll - (int) eqOnKey->preferredWidth(), y - 2, (int) eqOnKey->preferredWidth(), kh); y += kh + 6;
        const int ew = wAll / 4;
        for (int b = 0; b < 4; ++b) eqKnob[b]->setBounds (x0 + b * ew - 14, y, ew + 28, 120 + kTxt);
        y += 120 + kTxt; sections.push_back ({ "EQUALISER", { x0, top, wAll, y - top } });
        y += 30;
        // COMPRESSOR
        top = y;
        float kx = (float) (x0 + wAll);
        for (auto* k : { duckKey.get(), satKey.get(), cpOnKey.get() }) { kx -= k->preferredWidth(); k->setBounds ((int) kx, y - 2, (int) k->preferredWidth(), kh); kx -= gap; }
        y += kh + 6;
        const int cw = wAll / 4;
        for (int i = 0; i < 4; ++i) cpKnob[i]->setBounds (x0 + i * cw - 4, y, cw + 8, 80 + kTxt);
        y += 82 + kTxt;
        for (int i = 4; i < 7; ++i) cpKnob[i]->setBounds (x0 + (i - 4) * cw + cw / 2 - 4, y, cw + 8, 80 + kTxt);
        y += 80 + kTxt; sections.push_back ({ "COMPRESSOR", { x0, top, wAll, y - top } });
    }
    int baseWidth() const { return kL + kTab + (drawerOpen ? kPanel : 0); }
    bool applying = false;
    void applySize()
    {
        applying = true;
        const int w = baseWidth();
        content.setBounds (0, 0, w, kH);
        browser.setBounds (0, 0, w, kH); setup.setBounds (0, 0, w, kH);
        drawer.setBounds (kL + kTab, 0, kPanel, kH); drawer.setVisible (drawerOpen);
        setResizeLimits (roundToInt (w * 0.3f), roundToInt (kH * 0.3f), roundToInt (w * 3.0f), roundToInt (kH * 3.0f));
        if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) w / kH);
        setSize (roundToInt (w * scale), roundToInt (kH * scale));
        applying = false;
        resized();
        repaint(); content.repaint();
    }
    void toggleDrawer() { drawerOpen = ! drawerOpen; proc.setUiProperty ("uiDrawer", drawerOpen); applySize(); }
    void showSizeMenu()
    {
        PopupMenu m; m.addSectionHeader ("Window size");
        const float sizes[] = { 0.6f, 0.75f, 0.85f, 1.0f, 1.25f, 1.5f, 2.0f };
        for (int i = 0; i < 7; ++i) m.addItem (i + 1, String (roundToInt (sizes[i] * 100)) + "%", true, std::abs (sizes[i] - scale) < 0.01f);
        m.addSeparator(); m.addItem (100, "Drag the lower right corner to scale freely", false);
        m.showMenuAsync (PopupMenu::Options(), [this, sizes] (int r) { if (r > 0 && r < 100) { scale = sizes[r - 1]; proc.setUiProperty ("uiScale", scale); applySize(); } });
    }
    void textSizeMenu()
    {
        PopupMenu m; m.addSectionHeader ("Screen text size");
        const float pts[] = { 11.0f, 13.0f, 17.0f };
        const char* nm[] = { "Small", "Medium", "Large" };
        for (int i = 0; i < 3; ++i) m.addItem (i + 1, nm[i], true, std::abs (theme.displayPt - pts[i]) < 0.6f);
        m.showMenuAsync (PopupMenu::Options(), [this, pts] (int r) { if (r > 0) { theme.displayPt = pts[r - 1]; lookChanged(); setup.sync(); } });
    }
    void lookChanged()
    {
        proc.setUiProperty ("themeData", theme.toString());
        lnf.setColour (PopupMenu::highlightedBackgroundColourId, theme.keyLight.darker (0.2f));
        layout(); display.resized(); refreshPage (false);
        content.repaint(); for (auto* c : content.getChildren()) c->repaint(); for (auto* c : drawer.getChildren()) c->repaint();
    }

    // ---------------------------------------------------------------- state
    bool ctrl = false;
    int machine = 0, page = 0, ctrlPage = 0, focus = -1, lastConfig = -1;
    uint32 focusTime = 0, msgUntil = 0; String msg1, msg2;
    int config() const { return (int) proc.apvts.getRawParameterValue ("config")->load(); }
    int pages() const { return ctrl ? kCtrlPages : l480::algoDef (proc.algoOf (machine)).pages; }
    void showMessage (const String& a, const String& b, int ms = 1800) { msg1 = a; msg2 = b; msgUntil = Time::getMillisecondCounter() + (uint32) ms; }
    static String configName (int c) { static const char* n[] = { "ONE MACHINE", "SERIES", "PARALLEL", "LEFT / RIGHT" }; return n[jlimit (0, 3, c)]; }
    String programTitle (int m) const
    {
        if (proc.userPresetName.isNotEmpty()) return proc.userPresetName;
        return String (proc.prog (m).name) + (proc.isEdited (m) ? " *" : "");
    }

    // ---------------------------------------------------------------- presets
    void openBrowser() { setup.setVisible (false); browser.refresh(); browser.setVisible (true); browser.toFront (true); }
    void stepPreset (int dir)
    {
        // every factory program in menu order, then your presets
        const auto user = proc.userPresets();
        const int nf = l480::numPrograms(), total = nf + user.size();
        int cur = proc.userPresetName.isNotEmpty() ? nf + user.indexOf (proc.userPresetName) : proc.programOf (machine);
        if (cur < 0) cur = 0;
        const int next = ((cur + dir) % total + total) % total;
        if (next < nf) proc.loadProgram (machine, next); else proc.loadUserPreset (user[next - nf]);
        page = 0; ctrl = false; refreshPage (true); msgUntil = 0;
    }
    void savePreset()
    {
        auto* w = new AlertWindow ("Save preset", "Name for this preset (both machines, EQ, analog, compressor):", MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", proc.userPresetName.isNotEmpty() ? proc.userPresetName : String (proc.prog (machine).name));
        w->addButton ("Save", 1, KeyPress (KeyPress::returnKey)); w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        w->enterModalState (true, ModalCallbackFunction::create ([this, w] (int r)
        {
            if (r == 1) { const auto n = w->getTextEditorContents ("name").trim(); if (n.isNotEmpty() && proc.saveUserPreset (n)) showMessage ("Saved", n); }
        }), true);
    }
    void selectMachine (int m)
    {
        if (config() == 0) m = 0;
        if (m != machine) { machine = m; page = 0; refreshPage (true); }
    }
    void pressPage()
    {
        const int np = jmax (1, pages());
        if (ctrl) ctrlPage = (ctrlPage + 1) % np; else page = (page + 1) % np;
        refreshPage (true); msgUntil = 0;
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
        if (isTrigger (i)) { if (t > 0.6f && ! triggered[i]) { triggered[i] = true; proc.pressKey (machine, page * 6 + i); } return; }
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
    String faderName (int i) const
    {
        if (ctrl) return ctrlParam (i) ? String (kCtrl[ctrlPage][i].name) : String();
        const auto& s = proc.spec (machine, page * 6 + i);
        return (pages() > 0 && s.lab) ? plainName (s.name) : String();
    }
    String faderValueText (int i) const
    {
        if (ctrl) { auto* p = ctrlParam (i); return p ? p->getCurrentValueAsText() : String(); }
        const int slot = page * 6 + i; const auto& s = proc.spec (machine, slot);
        if (! s.lab) return {};
        if (s.unit == l480::uKey) return "push up";
        return String (l480::formatValue (s, proc.slotValue (machine, slot)));
    }
    void refreshPage (bool snap)
    {
        const int np = jmax (1, pages());
        if (ctrl) { if (ctrlPage >= np) ctrlPage = 0; } else if (page >= np) page = 0;
        for (int i = 0; i < 6; ++i)
        {
            const bool has = faderName (i).isNotEmpty();
            fader[i]->enabled = has;
            fader[i]->setTarget (has ? faderValue (i) : 0.0f);
            if (snap) fader[i]->snap();
            fader[i]->repaint();
        }
        focus = -1;
    }

    // ---------------------------------------------------------------- 30 Hz
    void timerCallback() override
    {
        const uint32 now = Time::getMillisecondCounter();
        if (config() != lastConfig) { lastConfig = config(); if (lastConfig == 0 && machine == 1) machine = 0; combineKey->setText (configName (lastConfig)); layout(); refreshPage (true); content.repaint(); }
        if (proc.programLoaded.exchange (false)) refreshPage (false);
        const bool dual = config() != 0;
        // preset bar
        {
            String t = programTitle (machine), sub = proc.userPresetName.isNotEmpty() ? String ("Your preset") : String (l480::bankName (proc.prog (machine).bank));
            if (t != presetName.text || sub != presetName.sub) { presetName.text = t; presetName.sub = sub; presetName.repaint(); }
        }
        // screen text
        String l[3];
        if (dual) { l[0] = "A  " + programTitle (0); l[1] = "B  " + programTitle (1); }
        else { l[0] = programTitle (0); l[1] = proc.userPresetName.isNotEmpty() ? String ("your preset") : String (l480::bankName (proc.prog (0).bank)); }
        const bool focused = focus >= 0 && now - focusTime < 2500;
        if (now < msgUntil) l[2] = msg1 + "  " + msg2;
        else if (focused) l[2] = faderName (focus) + "  " + faderValueText (focus);
        else if (ctrl) l[2] = "Control: " + String (kCtrlPageName[ctrlPage]) + "   page " + String (ctrlPage + 1) + " of " + String (kCtrlPages);
        else
        {
            const auto st = proc.machineStatus (machine);
            l[2] = st.isNotEmpty() ? st : String (l480::algoDef (proc.algoOf (machine)).name) + (pages() > 1 ? "   page " + String (page + 1) + " of " + String (pages()) : String());
        }
        bool changed = false;
        for (int i = 0; i < 3; ++i) if (display.lines[i] != l[i]) { display.lines[i] = l[i]; changed = true; }
        { const bool d0 = dual && machine == 1, d1 = ! dual || machine == 0; if (d0 != display.dim[0] || d1 != display.dim[1]) { display.dim[0] = d0; display.dim[1] = d1; changed = true; } }
        if (changed) display.repaint (display.getLocalBounds().withHeight (display.textHeight()));
        // meters
        const float mo = proc.meterOut.load();
        display.mL = mo > display.mL ? mo : display.mL * 0.86f; display.mR = display.mL;
        display.repaint (Rectangle<int> (0, 0, 44, display.textHeight()));
        // graph: delay taps for delay programs (and the utility delay's CONTROL pages), otherwise the decay
        const bool showDelay = display.delay.hasTaps();
        display.showDelay (showDelay);
        {
            const int sig = proc.programOf (machine) * 31 + machine * 7 + (proc.curveOn[machine] ? 1 : 0) + (int) proc.curvePts[machine].size() * 1000 + (ctrl ? ctrlPage * 100000 + 50000 : 0);
            float v = proc.mainRt (machine);
            for (int s = 0; s < l480::kMaxParams; ++s) v += proc.slotNorm (machine, s) * (float) (s + 1);
            if (showDelay) for (auto* id : { "dlTimeL", "dlTimeR", "dlDivL", "dlDivR", "dlFb", "dlMix", "dlSync" }) v += proc.apvts.getRawParameterValue (id)->load();
            if (v != lastGraphV || sig != lastSig) { lastGraphV = v; lastSig = sig; display.decay.repaint(); display.delay.repaint(); }
        }
        // slider display
        bool sdChanged = false;
        for (int i = 0; i < 6; ++i)
        {
            const String n = faderName (i), v = n.isNotEmpty() ? faderValueText (i) : String();
            if (n != sliderDisplay.names[i] || v != sliderDisplay.values[i]) { sliderDisplay.names[i] = n; sliderDisplay.values[i] = v; sdChanged = true; }
        }
        const int hi = focused ? focus : -1;
        if (hi != sliderDisplay.hi) { sliderDisplay.hi = hi; sdChanged = true; }
        if (sdChanged) sliderDisplay.repaint();
        // faders follow their parameters (and the graph's edits)
        for (int i = 0; i < 6; ++i)
        {
            if (fader[i]->enabled && ! (! ctrl && isTrigger (i))) fader[i]->setTarget (faderValue (i));
            if (fader[i]->animate()) fader[i]->repaint();
        }
        // keys light themselves while on
        machAKey->setLit (dual && machine == 0); machBKey->setLit (dual && machine == 1);
        ctrlKey->setLit (ctrl); muteKey->setLit (proc.mute.load()); setupKey->setLit (setup.isVisible());
        if (drawerOpen) for (auto* k : { anOnKey.get(), eqOnKey.get(), cpOnKey.get(), satKey.get(), duckKey.get() }) k->sync();
    }
    float lastGraphV = -1; int lastSig = -1;

    // ---------------------------------------------------------------- members
    Flexiverb480Processor& proc;
    Theme theme;
    FlexLnf lnf;
    Content content; Drawer drawer; bool drawerOpen = false; float scale = 0.85f;
    MainDisplay display; SliderDisplay sliderDisplay; PresetName presetName;
    std::unique_ptr<LarcKey> prevKey, nextKey, saveKey, machAKey, machBKey, combineKey, pageKey, ctrlKey, muteKey, setupKey;
    std::unique_ptr<LarcFader> fader[6];
    bool triggered[6] = {};
    std::unique_ptr<ParamKey> anOnKey, eqOnKey, cpOnKey, satKey, duckKey;
    std::unique_ptr<ScaleKnob> anKnob[5], cpKnob[7];
    std::unique_ptr<DualKnob> eqKnob[4];
    PresetBrowser browser; SetupPanel setup;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Flexiverb480Editor)
};
