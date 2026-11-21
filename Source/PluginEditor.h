// Flexiverb 480 editor (included by PluginProcessor.cpp).
//
//  Preset bar   the preset menu (click: every preset at once, by category), < > beside it, SAVE.
//  Screen       REVERB page: the early reflections and the low / mid / high decay of the program as it is set now
//               (rendered in the background from the engine itself), the decay-time curve, and every reverb number.
//               DELAY page: the delay taps and every delay number. Numbers: right-click and drag up/down, or click and
//               type. Graph handles drag too (predelay, decay, taps, curve points).
//  Keys         PAGE (reverb / delay) and the 480L's algorithms: HALL, RANDOM HALL, ROOM, CHAMBER, PLATE, AMBIENCE ...
//  Faders       one per parameter of the page; names above them in their own text size.
//  Drawer       engine (single / dual), mute, SETUP, output, iron, ANALOG, EQ (with analog models), COMPRESSOR.
//  SETUP        opens beside the plugin: colours, gradient, 3D depth, text sizes, oversampling, 64-bit.
#include "FlexFonts.h"
#include <set>

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
           eqKnob { 0xff5c6168 }, eqInner { 0xffc23b2e }, compKnob { 0xff3a3d42 }, analogKnob { 0xffc23b2e },
           stripPanel { 0xff4e6176 }, vuFace { 0xfff0e4c4 },
           keyBar { 0xffbdbab2 }, keyMine { 0xffbdbab2 }, keyPanel { 0xffbdbab2 }, faderSlot { 0xff050505 }, drawerPanel { 0xff2c2d30 },
           lowCol { 0xffff3b24 }, midCol { 0xffffb22e }, highCol { 0xff5cc8ff };
    float gradient = 0.45f, keyPt = 12.5f, knobPt = 11.5f, displayPt = 15.0f, faderPt = 13.5f, keyHeight = 0.0f, keyPad = 0.0f;   // key size: text + extra
    static constexpr int kMaxItems = 48; Colour gTop[kMaxItems], gBot[kMaxItems]; bool gOwn[kMaxItems] = {};
    int depth = 2;                                         // 3D: 0 flat, 1 soft, 2 deep

    struct Item { const char* name; Colour Theme::* c; };
    static const std::vector<Item>& items()
    {
        static const std::vector<Item> v {
            { "Panel", &Theme::panel }, { "Panel text", &Theme::panelText }, { "Side cheeks", &Theme::cheek },
            { "Algorithm buttons", &Theme::key }, { "Preset bar buttons", &Theme::keyBar }, { "Your buttons", &Theme::keyMine },
            { "Panel buttons", &Theme::keyPanel }, { "Button text", &Theme::keyText }, { "Button light", &Theme::keyLight },
            { "Screen", &Theme::display }, { "Screen text", &Theme::displayText }, { "Fader caps", &Theme::fader }, { "Fader slots", &Theme::faderSlot },
            { "Panel drawer", &Theme::drawerPanel },
            { "EQ knobs", &Theme::eqKnob }, { "EQ gain caps", &Theme::eqInner }, { "Compressor knobs", &Theme::compKnob },
            { "Analog knobs", &Theme::analogKnob },
            { "Graph low", &Theme::lowCol }, { "Graph mid", &Theme::midCol }, { "Graph high", &Theme::highCol },
            { "Channel strip panel", &Theme::stripPanel }, { "VU meter face", &Theme::vuFace } };
        return v;
    }
    String toString() const
    {
        XmlElement x ("LOOK");
        int i = 0; for (auto& it : items()) { x.setAttribute ("c" + String (i), (this->*it.c).toString());
                                               x.setAttribute ("t" + String (i), gTop[i].toString()); x.setAttribute ("b" + String (i), gBot[i].toString()); x.setAttribute ("o" + String (i), gOwn[i]); ++i; }
        x.setAttribute ("grad", gradient); x.setAttribute ("keyPt", keyPt); x.setAttribute ("knobPt", knobPt); x.setAttribute ("dispPt", displayPt);
        x.setAttribute ("faderPt", faderPt); x.setAttribute ("depth", depth); x.setAttribute ("keyH", keyHeight); x.setAttribute ("keyPad", keyPad); x.setAttribute ("v", 7);
        return x.toString (XmlElement::TextFormat().singleLine().withoutHeader());
    }
    bool fromString (const String& s)
    {
        auto x = parseXML (s); if (x == nullptr || ! x->hasTagName ("LOOK") || x->getIntAttribute ("v") < 7) return false;   // older looks: start fresh
        int i = 0; for (auto& it : items()) { const auto a = x->getStringAttribute ("c" + String (i)); if (a.isNotEmpty()) this->*it.c = Colour::fromString (a);
                                               if (x->hasAttribute ("t" + String (i))) { gTop[i] = Colour::fromString (x->getStringAttribute ("t" + String (i))); gBot[i] = Colour::fromString (x->getStringAttribute ("b" + String (i))); gOwn[i] = x->getBoolAttribute ("o" + String (i)); }
                                               ++i; }
        gradient = (float) x->getDoubleAttribute ("grad", gradient); keyPt = (float) x->getDoubleAttribute ("keyPt", keyPt);
        knobPt = (float) x->getDoubleAttribute ("knobPt", knobPt); displayPt = (float) x->getDoubleAttribute ("dispPt", displayPt);
        faderPt = (float) x->getDoubleAttribute ("faderPt", faderPt); depth = jlimit (0, 3, x->getIntAttribute ("depth", depth));
        keyHeight = (float) x->getDoubleAttribute ("keyH", keyHeight); keyPad = (float) x->getDoubleAttribute ("keyPad", keyPad);
        return true;
    }
    int indexOf (Colour Theme::* c) const { int i = 0; for (auto& it : items()) { if (it.c == c) return i; ++i; } return -1; }
    // the three colours of an object's gradient: top, middle (its colour), bottom - automatic, or your own
    std::array<Colour, 3> stops (Colour Theme::* c, float k = 1.0f) const
    {
        const Colour m = this->*c; const int i = indexOf (c);
        if (i >= 0 && gOwn[i]) return { gTop[i], m, gBot[i] };
        return { hi (m, k), m, lo (m, k) };
    }
    ColourGradient grad (Colour Theme::* c, Rectangle<float> r, float k = 1.0f, Colour mixWith = {}, float mix = 0.0f) const
    {
        auto st = stops (c, k);
        if (mix > 0) for (auto& x : st) x = x.interpolatedWith (mixWith, mix);
        ColourGradient gr (st[0], r.getX(), r.getY(), st[2], r.getX(), r.getBottom(), false); gr.addColour (0.5, st[1]); return gr;
    }
    float sz (float base) const { return base * displayPt / 12.0f; }   // screen text grows with the screen text size
    float d3() const { return depth == 0 ? 0.15f : depth == 1 ? 0.6f : depth == 2 ? 1.1f : 1.7f; }     // 3D amount
    Colour hi (Colour c, float k = 1.0f) const { return c.brighter (0.35f * gradient * k * (0.4f + 0.6f * d3())); }
    Colour lo (Colour c, float k = 1.0f) const { return c.darker (0.55f * gradient * k * (0.4f + 0.6f * d3())); }
    float shadow() const { return 0.15f + 0.45f * d3(); }
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
    Colour Theme::* kc = &Theme::key;            // which object's colours this key uses
    std::function<void()> onClick;
    void setLit (bool b) { if (b != lit) { lit = b; repaint(); } }
    void setText (const String& s) { if (s != text) { text = s; repaint(); } }
    const String& getText() const { return text; }
    float preferredWidth() const { return textW (ui (th.keyPt, W::Bold, 0.06f), text) + th.keyPt * 1.6f + 8.0f + th.keyPad; }
    std::function<void()> onRightClick;
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const float rad = 3.5f;
        const bool on = lit || pressed || flash;
        g.setColour (Colours::black.withAlpha (0.75f)); g.fillRoundedRectangle (r, rad + 1.5f);
        if (on) { g.setColour (th.keyLight.withAlpha (0.35f)); g.drawRoundedRectangle (r.expanded (0.5f), rad + 2.0f, 2.0f); }
        const float travel = pressed ? 1.2f * th.d3() : 0.0f;
        auto skirt = r.reduced (2.0f).translated (0, travel);
        const Colour c = on ? (th.*kc).interpolatedWith (th.keyLight, 0.62f) : th.*kc;
        const auto st = th.stops (kc, 1.3f);
        g.setGradientFill (ColourGradient ((on ? st[1].interpolatedWith (th.keyLight, 0.62f) : st[1]).darker (0.12f), skirt.getX(), skirt.getY(),
                                           (on ? st[2].interpolatedWith (th.keyLight, 0.62f) : st[2]).darker (0.3f), skirt.getX(), skirt.getBottom(), false));
        g.fillRoundedRectangle (skirt, rad);
        auto face = skirt.reduced (2.0f, 1.5f).withTrimmedBottom (pressed ? 1.0f : 2.5f * th.d3());
        g.setGradientFill (th.grad (kc, face, 0.7f, th.keyLight, on ? 0.62f : 0.0f)); g.fillRoundedRectangle (face, rad - 1.0f);
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
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
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
        float x = (float) x0;                     // each key as wide as its text: rows are not stretched
        for (auto* k : row) { const float w = k->preferredWidth(); k->setBounds ((int) x, y, (int) w, h); x += w + gap; }
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
        g.setGradientFill (th.grad (&Theme::faderSlot, slot, 0.5f)); g.fillRoundedRectangle (slot, 2.5f);
        const float y = bot - (bot - top) * shown, cw = jmin (34.0f, r.getWidth() - 8), ch = 24.0f;
        auto cap = Rectangle<float> (cx - cw / 2, y - ch / 2, cw, ch);
        g.setColour (Colours::black.withAlpha (th.shadow())); g.fillRoundedRectangle (cap.translated (1.5f * th.d3(), 3.0f * th.d3()), 3);
        g.setGradientFill (th.grad (&Theme::fader, cap, 1.6f, Colours::black, enabled ? 0.0f : 0.4f));
        g.fillRoundedRectangle (cap, 3);
        auto face = cap.reduced (2.5f, 3.0f);
        g.setGradientFill (th.grad (&Theme::fader, face, 1.0f, Colours::black, enabled ? 0.0f : 0.4f));
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

// a knob position snapped to whole steps (dB); the ends of the range stay reachable
static float snapped (RangedAudioParameter* p, float norm, float step)
{
    if (step <= 0 || norm <= 0.0f || norm >= 1.0f) return norm;
    const float v = p->convertFrom0to1 (norm);
    return p->convertTo0to1 (p->getNormalisableRange().snapToLegalValue (std::round (v / step) * step));
}
// a scale with a number every 'every' units and a tick at every step between
static std::vector<ScaleMark> numberScale (float lo, float hi, float every, float tick, bool plus = false)
{
    static std::vector<std::unique_ptr<String>> keep;    // the texts live as long as the program
    std::vector<ScaleMark> m;
    for (float v = lo; v <= hi + 1e-3f; v += tick)
    {
        const bool lab = std::abs (std::fmod (v - lo, every)) < 1e-3f || std::abs (std::fmod (v - lo, every) - every) < 1e-3f;
        if (! lab) { m.push_back ({ v, "" }); continue; }
        keep.push_back (std::make_unique<String> ((plus && v > 0 ? "+" : "") + String ((int) std::round (v))));
        m.push_back ({ v, keep.back()->toRawUTF8() });
    }
    return m;
}
static void drawScale (Graphics& g, RangedAudioParameter* p, Point<float> c, float radius, const std::vector<ScaleMark>& marks, Colour col, float fontH)
{
    g.setFont (ui (fontH, W::Semibold));
    for (auto& m : marks)
    {
        const float a = knobAngle (p->convertTo0to1 (m.value));
        const auto pt = c.getPointOnCircumference (radius, a);
        if (m.text[0] == 0)                              // a step without a number: a short tick
        { g.setColour (col.withAlpha (0.4f)); g.drawLine (Line<float> (c.getPointOnCircumference (radius - fontH * 0.9f, a), c.getPointOnCircumference (radius - fontH * 0.65f, a)), 0.8f); continue; }
        g.setColour (col.withAlpha (0.55f));
        g.drawLine (Line<float> (c.getPointOnCircumference (radius - fontH * 0.95f, a), c.getPointOnCircumference (radius - fontH * 0.55f, a)), 1.0f);
        g.setColour (col);
        g.drawText (m.text, Rectangle<float> (60, fontH + 2).withCentre (pt), Justification::centred, false);
    }
}
static void drawKnobBody (Graphics& g, const Theme& th, Rectangle<float> k, float angle, Colour Theme::* item, Colour pointer, bool skirt)
{
    const Colour body = th.*item;
    const float d = k.getWidth();
    g.setColour (Colours::black.withAlpha (th.shadow())); g.fillEllipse (k.translated (1.5f * th.d3(), 2.5f * th.d3()));
    if (skirt)
    {
        { auto st = th.stops (item, 1.2f); ColourGradient gr (st[0], k.getX(), k.getY(), st[2], k.getRight(), k.getBottom(), false); gr.addColour (0.5, st[1]); g.setGradientFill (gr); }
        g.fillEllipse (k);
        g.setColour (Colours::black.withAlpha (0.25f));
        for (int i = 0; i < 36; ++i) { const float a = i * MathConstants<float>::twoPi / 36.0f; g.drawLine (Line<float> (k.getCentre().getPointOnCircumference (d * 0.43f, a), k.getCentre().getPointOnCircumference (d * 0.5f, a)), 0.8f); }
        k = k.reduced (d * 0.1f);
    }
    g.setGradientFill (th.grad (item, k, 0.75f));
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
    float step = 0;                                      // snap to this many units (dB), 0 = free
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto lab = r.removeFromBottom (th.knobPt + 5);
        const float d = jmin (r.getWidth(), r.getHeight()) * 0.5f;
        const auto c = r.getCentre().translated (0, 3);
        drawScale (g, param, c, d * 0.5f + 11.0f, marks, th.panelText.withAlpha (0.85f), jmax (7.5f, th.knobPt * 0.75f));
        drawKnobBody (g, th, Rectangle<float> (d, d).withCentre (c), knobAngle (param->getValue()), capColour, th.panelText, true);
        drawLegend (g, name, lab, ui (th.knobPt, W::Bold, 0.06f), th.panelText);
    }
    void mouseDown (const MouseEvent&) override { start = param->getValue(); param->beginChangeGesture(); show(); }
    void mouseDrag (const MouseEvent& e) override
    {
        const float fine = e.mods.isShiftDown() || e.mods.isAltDown() ? 0.1f : 1.0f;
        param->setValueNotifyingHost (snapped (param, jlimit (0.0f, 1.0f, start - (float) e.getDistanceFromDragStartY() * 0.005f * fine), step)); show();
    }
    void mouseUp (const MouseEvent&) override { param->endChangeGesture(); }
    void mouseDoubleClick (const MouseEvent&) override { param->beginChangeGesture(); param->setValueNotifyingHost (param->getDefaultValue()); param->endChangeGesture(); show(); }
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override
    {
        param->beginChangeGesture();
        if (step > 0) { const float v = param->convertFrom0to1 (param->getValue()) + (w.deltaY > 0 ? step : -step); param->setValueNotifyingHost (snapped (param, param->convertTo0to1 (param->getNormalisableRange().snapToLegalValue (v)), step)); }
        else param->setValueNotifyingHost (jlimit (0.0f, 1.0f, param->getValue() + w.deltaY * 0.03f));
        param->endChangeGesture(); show();
    }
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
        static const std::vector<ScaleMark> gm = numberScale (-12, 12, 4, 2, true);
        const Colour red (0xffff6a4a);
        g.setColour (red.withAlpha (0.6f));
        for (float db = -14; db <= 14; db += 2)
        {
            const float a = knobAngle (gP->convertTo0to1 (db));
            g.drawLine (Line<float> (c.getPointOnCircumference (rOuter + 2.0f, a), c.getPointOnCircumference (rOuter + (std::fmod (db, 4.0f) == 0 ? 5.5f : 3.8f), a)), 1.0f);
        }
        drawScale (g, fP, c, rOuter + 27.0f, fm, th.panelText.withAlpha (0.9f), jmax (7.5f, th.knobPt * 0.75f));
        drawScale (g, gP, c, rOuter + 12.5f, gm, red, 7.0f);
        drawKnobBody (g, th, Rectangle<float> (rOuter * 2, rOuter * 2).withCentre (c), knobAngle (fP->getValue()), &Theme::eqKnob, th.panelText, true);
        drawKnobBody (g, th, Rectangle<float> (rInner * 2, rInner * 2).withCentre (c), knobAngle (gP->getValue()), &Theme::eqInner, Colours::white, false);
        drawLegend (g, name, lab, ui (th.knobPt, W::Bold, 0.06f), th.panelText);
    }
    bool hitTest (int x, int y) override { geometry (bodyArea()); return Point<float> ((float) x, (float) y).getDistanceFrom (c) < rOuter + 6.0f || y > getHeight() - th.knobPt - 5; }
    void mouseDown (const MouseEvent& e) override { geometry (bodyArea()); active = pick (e); start = active->getValue(); active->beginChangeGesture(); show(); }
    void mouseDrag (const MouseEvent& e) override
    {
        const float fine = e.mods.isShiftDown() || e.mods.isAltDown() ? 0.1f : 1.0f;
        active->setValueNotifyingHost (snapped (active, jlimit (0.0f, 1.0f, start - (float) e.getDistanceFromDragStartY() * 0.005f * fine), active == gP ? 2.0f : 0.0f)); show();
    }
    void mouseUp (const MouseEvent&) override { if (active) active->endChangeGesture(); }
    void mouseDoubleClick (const MouseEvent& e) override { geometry (bodyArea()); auto* p = pick (e); p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); active = p; show(); }
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        geometry (bodyArea()); auto* p = pick (e); p->beginChangeGesture();
        if (p == gP) { const float v = jlimit (-15.0f, 15.0f, p->convertFrom0to1 (p->getValue()) + (w.deltaY > 0 ? 2.0f : -2.0f)); p->setValueNotifyingHost (snapped (p, p->convertTo0to1 (v), 2.0f)); }
        else p->setValueNotifyingHost (jlimit (0.0f, 1.0f, p->getValue() + w.deltaY * 0.03f));
        p->endChangeGesture(); active = p; show();
    }
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
        plot = getLocalBounds().toFloat().reduced (th.sz (40.0f), 6).withTrimmedRight (th.sz (30.0f)).withTrimmedBottom (th.sz (11.0f)).withTrimmedTop (th.sz (8.0f));
        const int m = machine ? machine() : 0;
        const Colour col = th.displayText;
        if (! proc.hasDecayCurve (m)) return;
        rtMain = jmax (0.05f, proc.mainRt (m));
        yLo = jmax (0.02f, rtMain / 10.0f); yHi = rtMain * 6.0f;
        g.setFont (ui (th.sz (8.5f), W::Medium));
        for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            const float x = xOf (f); g.setColour (col.withAlpha (0.10f)); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (col.withAlpha (0.55f)); g.drawText (f >= 1000 ? String (f / 1000, 0) + "k" : String ((int) f), Rectangle<float> (x - th.sz (18.0f), plot.getBottom() + 1, th.sz (36.0f), th.sz (12.0f)), Justification::centred, false);
        }
        for (float t : { 0.05f, 0.1f, 0.2f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 50.0f })
        {
            if (t < yLo || t > yHi) continue;
            const float y = yOf (t); g.setColour (col.withAlpha (0.10f)); g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            g.setColour (col.withAlpha (0.55f)); g.drawText (String (t, t < 1 ? 2 : 0) + " s", Rectangle<float> (plot.getX() - th.sz (40.0f), y - th.sz (6.0f), th.sz (37.0f), th.sz (12.0f)), Justification::centredRight, false);
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
        // the big point: moves every point up or down together
        {
            float gm = 0; int n = 0;
            if (proc.curveOn[m]) for (auto& q : pts) { gm += std::log (q.m); ++n; }
            master = { plot.getRight() + th.sz (16.0f), yOf (rtMain * (n > 0 ? std::exp (gm / (float) n) : 1.0f)) };
            g.setColour (col.withAlpha (0.25f)); g.drawLine (plot.getRight() + 2, master.y, master.x, master.y, 1.0f);
            g.setColour (Colours::black); g.fillEllipse (Rectangle<float> (20, 20).withCentre (master));
            g.setColour (masterDrag || masterHover ? Colours::white : col); g.drawEllipse (Rectangle<float> (17, 17).withCentre (master), 2.6f);
            g.fillEllipse (Rectangle<float> (6, 6).withCentre (master));
        }
        g.setColour (col.withAlpha (0.6f)); g.setFont (ui (th.sz (9.0f), W::Bold, 0.05f));
        g.drawText ("Decay time by frequency:  double-click to add or remove a point,  drag a point,  drag the big point to move them all", getLocalBounds().toFloat().reduced (th.sz (36.0f), 0).withHeight (th.sz (12.0f) + 2), Justification::topLeft, false);
    }
    void mouseMove (const MouseEvent& e) override { const int h = hit (e.position); const bool mh = e.position.getDistanceFrom (master) < 12.0f; if (h != hover || mh != masterHover) { hover = h; masterHover = mh; repaint(); } }
    void mouseExit (const MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const MouseEvent& e) override
    {
        const int m = machine ? machine() : 0;
        if (e.mods.isPopupMenu() || ! proc.hasDecayCurve (m)) return;
        if (e.position.getDistanceFrom (master) < 12.0f)
        {
            masterPts = proc.curveOn[m] ? proc.curvePts[m] : proc.programCurve (m);
            masterDrag = true; masterY = e.position.y; proc.setCurve (m, masterPts, true); repaint(); return;
        }
        drag = hit (e.position);
        if (drag >= 0) show();
        repaint();
    }
    void mouseDrag (const MouseEvent& e) override
    {
        const int m = machine ? machine() : 0;
        if (masterDrag)
        {
            const float k = tOf (e.position.y) / tOf (masterY);
            auto pts = masterPts; for (auto& q : pts) q.m = jlimit (0.02f, 20.0f, q.m * k);
            proc.setCurve (m, pts, true);
            if (onShow) onShow ("All points", (k >= 1 ? "x " + String (k, 2) : "x " + String (k, 2)));
            repaint(); return;
        }
        if (drag < 0 || drag >= (int) proc.curvePts[m].size()) return;
        auto pts = proc.curvePts[m];
        const float lo = drag > 0 ? pts[(size_t) drag - 1].f * 1.05f : 20.0f, hi = drag + 1 < (int) pts.size() ? pts[(size_t) drag + 1].f / 1.05f : 20000.0f;
        pts[(size_t) drag] = { jlimit (lo, hi, fOf (e.position.x)), jlimit (0.02f, 20.0f, tOf (jlimit (plot.getY(), plot.getBottom(), e.position.y)) / rtMain) };
        proc.setCurve (m, pts, true); show(); repaint();
    }
    void mouseUp (const MouseEvent&) override { drag = -1; masterDrag = false; repaint(); }
    void mouseDoubleClick (const MouseEvent& e) override
    {
        const int m = machine ? machine() : 0;
        if (! proc.hasDecayCurve (m) || e.position.getDistanceFrom (master) < 12.0f) return;
        const int h = hit (e.position);
        auto pts = proc.curveOn[m] ? proc.curvePts[m] : proc.programCurve (m);
        if (h >= 0) { pts.erase (pts.begin() + h); proc.setCurve (m, pts, ! pts.empty()); drag = hover = -1; repaint(); return; }
        if (! plot.expanded (4).contains (e.position)) return;
        pts.push_back ({ fOf (e.position.x), jlimit (0.02f, 20.0f, tOf (e.position.y) / rtMain) });
        std::sort (pts.begin(), pts.end(), [] (const auto& a, const auto& b) { return a.f < b.f; });
        proc.setCurve (m, pts, true); repaint();
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
    Point<float> master { -100, -100 }; bool masterDrag = false, masterHover = false; float masterY = 0; std::vector<Flexiverb480Processor::CurvePt> masterPts;
public:
    bool ready() const { return ! plot.isEmpty(); }
    Point<float> at (float f, float t) const { return { xOf (jlimit (20.0f, 20000.0f, f)), yOf (t) }; }
    float mainRtShown() const { return rtMain; }
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
        plot = getLocalBounds().toFloat().reduced (th.sz (40.0f), 6).withTrimmedRight (-th.sz (28.0f)).withTrimmedBottom (th.sz (11.0f)).withTrimmedTop (th.sz (22.0f));
        collect();
        const Colour col = th.displayText;
        // time grid
        g.setFont (ui (th.sz (8.5f), W::Medium));
        const float step = tMax <= 200 ? 25 : tMax <= 600 ? 100 : tMax <= 1500 ? 250 : 500;
        for (float t = 0; t <= tMax + 1; t += step)
        {
            const float x = xOf (t); g.setColour (col.withAlpha (0.10f)); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (col.withAlpha (0.55f)); g.drawText (String ((int) t) + " ms", Rectangle<float> (x - th.sz (26.0f), plot.getBottom() + 1, th.sz (52.0f), th.sz (12.0f)), Justification::centred, false);
        }
        for (float db : { 0.0f, -12.0f, -24.0f, -36.0f })
        {
            const float y = yOf (db); g.setColour (col.withAlpha (0.10f)); g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            g.setColour (col.withAlpha (0.55f)); g.drawText (String ((int) db) + " dB", Rectangle<float> (plot.getX() - th.sz (40.0f), y - th.sz (6.0f), th.sz (37.0f), th.sz (12.0f)), Justification::centredRight, false);
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
            g.setFont (ui (th.sz (9.0f), W::Bold)); g.setColour (col.withAlpha (0.85f));
            g.drawText (t.label, Rectangle<float> (x + 9, y - 5, 44, 11), Justification::left, false);
        }
        g.setColour (col.withAlpha (0.6f)); g.setFont (ui (th.sz (9.0f), W::Bold, 0.05f));
        g.drawText ("Delay taps: drag a point - sideways for time, up and down for level", getLocalBounds().toFloat().reduced (th.sz (36.0f), 0).withHeight (th.sz (12.0f) + 2), Justification::topLeft, false);
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
public:
    // where a tap is drawn (for its numbers): by its time or level slot, or the utility delay's side
    Point<float> tapAt (int slot, int utilSide) const
    {
        for (auto& t : taps) if ((slot >= 0 && (t.timeSlot == slot || t.levelSlot == slot)) || (slot < 0 && t.util && t.side == utilSide)) return { xOf (t.ms), yOf (t.db) };
        return { -1, -1 };
    }
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

// ============================================================================ the order presets are offered in
// Extracted (from the 480L IRs) first, then Surround (HD Card), then Matched Presets (the 480L programs matched to the
// extracted ones), then the remaining 480L programs. An original that has a matched version is not offered twice.
static String presetKey (String n)
{
    n = n.toLowerCase().removeCharacters (" +&-'.#").replace ("no2", "2").replace ("sillica", "silica");
    return n.replace ("mediumhallstage", "mediumstage");
}
static bool isMatchedOriginal (int i)
{
    static const std::set<String> matched = [] { std::set<String> m; for (auto& p : l480::programs()) if (p.bank >= 27 && p.bank <= 31) m.insert (presetKey (p.name)); return m; }();
    const auto& p = l480::programs()[(size_t) i];
    return p.bank >= 1 && p.bank <= 21 && matched.count (presetKey (p.name)) > 0;
}
static const std::vector<int>& browseOrder()
{
    static const std::vector<int> order = []
    {
        std::vector<int> o;
        auto banks = [&o] (int b0, int b1) { for (int b = b0; b <= b1; ++b) for (int i = 0; i < l480::numPrograms(); ++i) if (l480::programs()[(size_t) i].bank == b && ! isMatchedOriginal (i)) o.push_back (i); };
        banks (33, 37); banks (22, 26); banks (27, 31); banks (1, 21);
        return o;
    }();
    return order;
}

// ============================================================================ preset browser: every preset at once, by category
class PresetBrowser : public Component
{
public:
    PresetBrowser (const Theme& t, Flexiverb480Processor& p) : th (t), proc (p) { addAndMakeVisible (view); view.setViewedComponent (&list, false); view.setScrollBarsShown (true, false); }
    std::function<void (int program)> onFactory; std::function<void (const String&)> onUser; std::function<void()> onClose;
    std::function<int()> machine;
    void refresh() { list.build (*this); resized(); repaint(); }
    void resized() override { auto r = getLocalBounds().reduced (16); r.removeFromTop (44); view.setBounds (r); list.setSize (r.getWidth() - 12, list.heightFor (r.getWidth() - 12)); list.layout(); }
    void paint (Graphics& g) override
    {
        g.fillAll (Colours::black.withAlpha (0.82f));
        auto r = getLocalBounds().reduced (16).removeFromTop (30).toFloat();
        drawLegend (g, "PRESETS  -  machine " + String (machine && machine() == 1 ? "B" : "A"), r, ui (16.0f, W::Bold, 0.1f), th.panelText, Justification::centredLeft);
        drawLegend (g, "click a preset to load it", r.withTrimmedRight (60), ui (11.0f, W::Medium), th.panelText.withAlpha (0.6f), Justification::centredRight);
        // the close cross
        const auto x = closeArea().toFloat(); const bool ov = closeHover;
        g.setColour (ov ? th.keyLight.withAlpha (0.25f) : Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (x, 6);
        g.setColour (ov ? th.keyLight : th.panelText); g.drawRoundedRectangle (x, 6, 1.5f);
        const auto c = x.reduced (12); g.drawLine (c.getX(), c.getY(), c.getRight(), c.getBottom(), 3.5f); g.drawLine (c.getRight(), c.getY(), c.getX(), c.getBottom(), 3.5f);
    }
    void mouseDown (const MouseEvent&) override { if (onClose) onClose(); }
    void mouseMove (const MouseEvent& e) override { const bool h = closeArea().contains (e.getPosition()); if (h != closeHover) { closeHover = h; repaint (closeArea().expanded (2)); } }
    void mouseExit (const MouseEvent&) override { if (closeHover) { closeHover = false; repaint (closeArea().expanded (2)); } }
private:
    Rectangle<int> closeArea() const { return { getWidth() - 16 - 44, 8, 44, 44 }; }
    bool closeHover = false;
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
                    for (int i = 0; i < l480::numPrograms(); ++i) if (l480::programs()[(size_t) i].bank == bank && ! isMatchedOriginal (i)) bx.items.push_back ({ l480::programs()[(size_t) i].name, i, {}, {} });
                    if (! bx.items.empty()) s.boxes.push_back (bx);
                }
                sections.push_back (s);
            };
            sec ("Extracted  (from the 480L impulse responses, 96 kHz)", 33, 37); sec ("Surround (HD Card)", 22, 26);
            sec ("Matched Presets  (480L programs matched to the extracted ones)", 27, 31); sec ("480L", 1, 21);
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

// ============================================================================ background renderer: the program's own response
// Plays a click through a private copy of the engine whenever a setting changes, then measures it: the early reflections
// (first 150 ms, 0.5 ms bins) and the low / mid / high energy decay (10 ms frames, 2.5 s). The screen draws these.
class ResponseRenderer : public Thread
{
public:
    static constexpr int kWfBands = 28;
    struct Result { std::vector<float> er; std::vector<float> band[3]; std::vector<float> wf[kWfBands]; int version = 0; };
    ResponseRenderer() : Thread ("Flexiverb response") { startThread (Thread::Priority::low); }
    ~ResponseRenderer() override { stopThread (3000); }
    void request (const l480::EngineParams& p, double rate) { { const ScopedLock sl (lock); pending = p; pendingFs = rate; has = true; } notify(); }
    Result get() const { const ScopedLock sl (lock); return result; }
    std::atomic<int> version { 0 };
private:
    struct Bq { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void lp (double fs, double f, bool high) { const double w = 2 * MathConstants<double>::pi * f / fs, c = std::cos (w), al = std::sin (w) / (2 * 0.7071), a0 = 1 + al;
            if (high) { b0 = (1 + c) / 2 / a0; b1 = -(1 + c) / a0; } else { b0 = (1 - c) / 2 / a0; b1 = (1 - c) / a0; } b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0; z1 = z2 = 0; }
        inline double p (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; } };
    void run() override
    {
        while (! threadShouldExit())
        {
            wait (300);
            l480::EngineParams p; double rate;
            { const ScopedLock sl (lock); if (! has) continue; p = pending; rate = pendingFs; has = false; }
            if (rate != engFs) { eng.prepare (rate); engFs = rate; } else eng.resetState();
            const int len = (int) (2.5 * rate);
            std::vector<float> L ((size_t) len, 0.0f), R ((size_t) len, 0.0f), a (256, 0.0f), b (256, 0.0f);
            for (int k = 0; k < 4; ++k) { std::fill (a.begin(), a.end(), 0.0f); std::fill (b.begin(), b.end(), 0.0f); eng.process (a.data(), b.data(), 256, p); }
            L[0] = R[0] = 0.25f;
            for (int pos = 0; pos < len && ! threadShouldExit(); pos += 256) eng.process (L.data() + pos, R.data() + pos, std::min (256, len - pos), p);
            Result r;
            // early reflections: peak level in 0.5 ms bins over 150 ms, dB re the loudest point of the whole response
            float mx = 1e-9f; for (int i = 0; i < len; ++i) mx = std::max (mx, 0.5f * (std::abs (L[(size_t) i]) + std::abs (R[(size_t) i])));
            const int eb = std::max (1, (int) (0.0005 * rate));
            for (int k = 0; k < 300; ++k) { float m = 0; for (int i = k * eb; i < (k + 1) * eb && i < len; ++i) m = std::max (m, 0.5f * (std::abs (L[(size_t) i]) + std::abs (R[(size_t) i]))); r.er.push_back (Decibels::gainToDecibels (m / mx, -80.0f)); }
            // bands: low < 250 Hz, mid 250 Hz - 2.5 kHz, high > 2.5 kHz; energy in 10 ms frames, dB re the loudest band frame
            Bq f[3][4];
            for (int c = 0; c < 2; ++c) { f[0][c].lp (rate, 250, false); f[0][c + 2].lp (rate, 250, false); f[1][c].lp (rate, 250, true); f[1][c + 2].lp (rate, 2500, false); f[2][c].lp (rate, 2500, true); f[2][c + 2].lp (rate, 2500, true); }
            const int fr = (int) (0.01 * rate), nf = len / fr; double top = 1e-30;
            std::vector<double> e[3];
            for (int bnd = 0; bnd < 3; ++bnd)
            {
                e[bnd].assign ((size_t) nf, 0.0);
                for (int i = 0; i < nf * fr; ++i)
                {
                    const double x = f[bnd][2].p (f[bnd][0].p (L[(size_t) i])), y = f[bnd][3].p (f[bnd][1].p (R[(size_t) i]));
                    e[bnd][(size_t) (i / fr)] += x * x + y * y;
                }
                for (double v : e[bnd]) top = std::max (top, v);
            }
            for (int bnd = 0; bnd < 3; ++bnd) for (double v : e[bnd]) r.band[bnd].push_back ((float) std::max (-80.0, 10 * std::log10 (v / top + 1e-30)));
            // waterfall: 28 bands, 40 Hz - 16 kHz (about a third of an octave apart), energy in 10 ms frames, dB re the loudest
            {
                std::vector<double> m ((size_t) (nf * fr)); for (int i = 0; i < nf * fr; ++i) m[(size_t) i] = 0.5 * (L[(size_t) i] + R[(size_t) i]);
                std::vector<std::vector<double>> w (kWfBands); double wtop = 1e-30;
                for (int bnd = 0; bnd < kWfBands && ! threadShouldExit(); ++bnd)
                {
                    const double fc = 40.0 * std::pow (400.0, bnd / (double) (kWfBands - 1));
                    auto& v = w[(size_t) bnd]; v.assign ((size_t) nf, 0.0);
                    if (fc > 0.45 * rate) continue;
                    Bq q[2];
                    for (auto& qq : q) { const double om = 2 * MathConstants<double>::pi * fc / rate, al = std::sin (om) / (2 * 4.3), a0 = 1 + al;
                        qq.b0 = al / a0; qq.b1 = 0; qq.b2 = -al / a0; qq.a1 = -2 * std::cos (om) / a0; qq.a2 = (1 - al) / a0; }
                    for (int i = 0; i < nf * fr; ++i) { const double y = q[1].p (q[0].p (m[(size_t) i])); v[(size_t) (i / fr)] += y * y; }
                    for (double x : v) wtop = std::max (wtop, x);
                }
                for (int bnd = 0; bnd < kWfBands; ++bnd) for (double x : w[(size_t) bnd]) r.wf[bnd].push_back ((float) std::max (-80.0, 10 * std::log10 (x / wtop + 1e-30)));
            }
            r.version = ++counter;
            { const ScopedLock sl (lock); result = r; }
            version.store (counter);
        }
    }
    CriticalSection lock; l480::EngineParams pending; double pendingFs = 48000; bool has = false;
    l480::Engine eng; double engFs = 0; int counter = 0; Result result;
};

// ============================================================================ what a number or fader controls: a program slot or a plugin parameter
struct Target { int slot = -1; RangedAudioParameter* param = nullptr; String name; bool trigger = false; };

// ============================================================================ the reverb picture: early reflections and low / mid / high decay
// Animated: the curves glide to each new response; live bars show the output's low, mid and high level.
// Drag the dotted predelay line, or the decay handle on the mid curve (sideways = decay time).
class ReverbView : public Component
{
public:
    explicit ReverbView (const Theme& t) : th (t) {}
    std::function<float()> getPredelayMs, getRt; std::function<void (float)> setPredelayMs, setRt; std::function<void (bool)> gesture;
    std::function<void (const String&, const String&)> onShow;
    float live[3] = {};
    Point<float> pdAnchor { -1, -1 }, rtAnchor { -1, -1 };   // where the predelay and decay-time numbers go
    Rectangle<float> graphArea() const { return decArea; }
    Point<float> floorAt (float f) const { return decArea.isEmpty() ? Point<float> (-1, -1) : P (wfIndex (f), tMax(), -60.0f); }
    void setResult (const ResponseRenderer::Result& r) { target = r; if (shown.er.empty()) shown = r; }
    bool animate()
    {
        if (target.er.empty()) return false;
        bool moving = false;
        auto glide = [&moving] (std::vector<float>& a, const std::vector<float>& b)
        { if (a.size() != b.size()) { a = b; moving = true; return; } for (size_t i = 0; i < a.size(); ++i) { const float d = b[i] - a[i]; if (std::abs (d) > 0.05f) { a[i] += d * 0.25f; moving = true; } else a[i] = b[i]; } };
        glide (shown.er, target.er); for (int k = 0; k < 3; ++k) glide (shown.band[k], target.band[k]);
        for (int k = 0; k < ResponseRenderer::kWfBands; ++k) glide (shown.wf[k], target.wf[k]);
        return moving;
    }
    void paint (Graphics& g) override
    {
        const Colour col = th.displayText;
        auto r = getLocalBounds().toFloat().reduced (6, 4);
        auto erA = r.removeFromLeft (r.getWidth() * 0.26f).reduced (0, 2); r.removeFromLeft (10);
        auto meters = r.removeFromRight (26); r.removeFromRight (6);
        const float lh = th.sz (12.0f);
        erArea = erA.withTrimmedTop (lh + 4).withTrimmedBottom (lh + 2).withTrimmedLeft (lh * 2.2f); decArea = r.withTrimmedTop (lh + 4).withTrimmedBottom (lh + 2).withTrimmedLeft (lh * 2.4f).withTrimmedRight (lh * 3.2f);
        g.setFont (ui (th.sz (9.0f), W::Bold, 0.05f)); g.setColour (col.withAlpha (0.7f));
        g.drawText ("EARLY REFLECTIONS", erA.withHeight (th.sz (12.0f) + 2), Justification::left, false);
        g.drawText ("DECAY  -  LOW / MID / HIGH,  TIME COMING FORWARD", r.withHeight (th.sz (12.0f) + 2), Justification::left, false);
        grid (g, erArea, 150.0f, 50.0f, "ms");
        if (shown.er.empty()) return;
        // early reflections as stems
        for (size_t k = 0; k < shown.er.size(); ++k)
        {
            const float db = shown.er[k]; if (db < -60) continue;
            const float prev = k > 1 ? std::max (shown.er[k - 1], shown.er[k - 2]) : -99.0f, next = k + 2 < shown.er.size() ? std::max (shown.er[k + 1], shown.er[k + 2]) : -99.0f;
            if (db < prev || db < next) continue;                     // a distinct reflection: louder than its neighbours
            const float x = erArea.getX() + erArea.getWidth() * (float) k / (float) shown.er.size(), y = yOf (erArea, db);
            g.setColour (col.withAlpha (0.85f)); g.drawLine (x, erArea.getBottom(), x, y, 1.2f);
        }
        const float pd = getPredelayMs ? getPredelayMs() : -1.0f;
        if (pd >= 0)
        {
            const float x = erArea.getX() + erArea.getWidth() * jlimit (0.0f, 1.0f, pd / 150.0f);
            Path pth; pth.startNewSubPath (x, erArea.getY()); pth.lineTo (x, erArea.getBottom());
            Path dsh; const float dl[] = { 3.0f, 3.0f }; PathStrokeType (1.2f).createDashedStroke (dsh, pth, dl, 2);
            g.setColour (Colours::white.withAlpha (dragging == 1 ? 0.9f : 0.55f)); g.fillPath (dsh);
            pdAnchor = { x + 3, erArea.getY() };
        }
        else pdAnchor = { -1, -1 };
        waterfall (g);
        const Colour bc[3] = { th.lowCol, th.midCol, th.highCol };
        // decay handle: on the 1 kHz line where it has fallen 30 dB (drag sideways for decay time)
        const float th30 = t30();
        if (th30 > 0)
        {
            handle = P (wfIndex (1000.0f), jmin (th30, tMax()), -30.0f);
            g.setColour (Colours::black); g.fillEllipse (Rectangle<float> (12, 12).withCentre (handle));
            g.setColour (dragging == 2 ? Colours::white : col); g.drawEllipse (Rectangle<float> (10, 10).withCentre (handle), 2.0f);
            rtAnchor = handle.translated (8, -22);
        }
        else rtAnchor = { -1, -1 };
        g.setFont (ui (th.sz (8.0f), W::Bold));
        const char* bn[3] = { "L", "M", "H" };
        for (int b = 0; b < 3; ++b) { g.setColour (bc[b]); g.drawText (bn[b], Rectangle<float> (meters.getX() + b * 9.0f - 1, decArea.getBottom() + 1, 9, th.sz (12.0f)), Justification::centred, false); }
        // live output bars
        for (int b = 0; b < 3; ++b)
        {
            auto bar = Rectangle<float> (meters.getX() + b * 9.0f, decArea.getY(), 7.0f, decArea.getHeight());
            g.setColour (col.withAlpha (0.08f)); g.fillRect (bar);
            const float h = bar.getHeight() * jlimit (0.0f, 1.0f, (Decibels::gainToDecibels (live[b], -60.0f) + 60.0f) / 60.0f);
            g.setColour (bc[b]); g.fillRect (bar.withTop (bar.getBottom() - h));
        }
    }
    void mouseDown (const MouseEvent& e) override
    {
        dragging = 0;
        const float pd = getPredelayMs ? getPredelayMs() : -1.0f;
        if (pd >= 0 && erArea.expanded (4).contains (e.position) && std::abs (e.position.x - (erArea.getX() + erArea.getWidth() * pd / 150.0f)) < 8) dragging = 1;
        else if (e.position.getDistanceFrom (handle) < 10 && getRt) { dragging = 2; startRt = getRt(); startX = e.position.x; }
        if (dragging && gesture) gesture (true);
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (dragging == 1 && setPredelayMs) { const float ms = jlimit (0.0f, 150.0f, (e.position.x - erArea.getX()) / erArea.getWidth() * 150.0f); setPredelayMs (ms); if (onShow) onShow ("Predelay", String (ms, 1) + " ms"); }
        if (dragging == 2 && setRt) { const float k = std::pow (2.0f, (e.position.x - startX) / 120.0f), rt = jlimit (0.05f, 70.0f, startRt * k); setRt (rt); if (onShow) onShow ("Decay", String (rt, 2) + " s"); }
        repaint();
    }
    void mouseUp (const MouseEvent&) override { if (dragging && gesture) gesture (false); dragging = 0; repaint(); }
private:
    // ---- the 3D waterfall: frequency across (log), time going back (up and to the right), level up
    static constexpr float kDepthX = 0.30f, kDepthY = 0.42f;
    static float wfIndex (float f) { return (float) (ResponseRenderer::kWfBands - 1) * std::log (jlimit (40.0f, 16000.0f, f) / 40.0f) / std::log (400.0f); }
    Point<float> P (float band, float t, float db) const
    {
        const float fw = decArea.getWidth() * (1.0f - kDepthX), lh = decArea.getHeight() * (1.0f - kDepthY), k = 1.0f - jlimit (0.0f, 1.0f, t / tMax());   // the start at the back, the tail comes toward you
        return { decArea.getX() + fw * band / (float) (ResponseRenderer::kWfBands - 1) + decArea.getWidth() * kDepthX * k,
                 decArea.getBottom() - lh * (jlimit (-60.0f, 0.0f, db) + 60.0f) / 60.0f - decArea.getHeight() * kDepthY * k };
    }
    float wfLevel (int band, float t) const      // 50 ms average of the band's energy around t
    {
        const auto& e = shown.wf[band]; if (e.empty()) return -80.0f;
        const int c = (int) (t / 0.01f); float sm = 0; int n = 0;
        for (int j = c - 2; j <= c + 2; ++j) if (j >= 0 && j < (int) e.size()) { sm += std::pow (10.0f, e[(size_t) j] / 10.0f); ++n; }
        return 10.0f * std::log10 (sm / (float) jmax (1, n) + 1e-12f);
    }
    void waterfall (Graphics& g)
    {
        const Colour col = th.displayText; const int nb = ResponseRenderer::kWfBands;
        const float tm = tMax();
        // the floor and walls
        g.setColour (col.withAlpha (0.10f));
        for (float f : { 100.0f, 1000.0f, 10000.0f }) g.drawLine (Line<float> (P (wfIndex (f), 0, -60), P (wfIndex (f), tm, -60)), 1.0f);
        const float step = tm > 2.0f ? 0.5f : tm > 1.0f ? 0.25f : 0.1f;
        g.setFont (ui (th.sz (8.0f), W::Medium));
        for (float t = 0; t <= tm + 1e-4f; t += step)
        {
            g.setColour (col.withAlpha (0.10f)); g.drawLine (Line<float> (P (0, t, -60), P ((float) nb - 1, t, -60)), 1.0f);
            const auto q = P ((float) nb - 1, t, -60);
            g.setColour (col.withAlpha (0.5f)); g.drawText (String (t, step < 0.5f ? 2 : 1) + " s", Rectangle<float> (q.x + 3, q.y - th.sz (6.0f), th.sz (48.0f), th.sz (12.0f)), Justification::left, false);
        }
        for (float db : { 0.0f, -20.0f, -40.0f, -60.0f })
        {
            const auto a = P (0, 0, db), b = P (0, tm, db);
            g.setColour (col.withAlpha (0.08f)); g.drawLine (Line<float> (a, b), 1.0f);
            g.setColour (col.withAlpha (0.5f)); g.drawText (String ((int) db), Rectangle<float> (a.x - th.sz (30.0f), a.y - th.sz (6.0f), th.sz (26.0f), th.sz (12.0f)), Justification::right, false);
        }
        for (float f : { 100.0f, 1000.0f, 10000.0f })
        {
            const auto a = P (wfIndex (f), 0, -60);
            g.setColour (col.withAlpha (0.5f)); g.drawText (f >= 1000 ? String ((int) (f / 1000)) + "k" : String ((int) f), Rectangle<float> (a.x - th.sz (18.0f), a.y + 1, th.sz (36.0f), th.sz (12.0f)), Justification::centred, false);
        }
        if (shown.wf[0].empty()) return;
        // slices from the back (the start) to the front (the tail), each hiding what is behind it
        const int ns = 34;
        for (int si = 0; si < ns; ++si)
        {
            const float t = tm * (float) si / (float) (ns - 1), age = (float) si / (float) (ns - 1);
            // one filled piece per range (low < 250 Hz, mid to 2.5 kHz, high above), each hiding what is behind it
            const float edge[4] = { 0.0f, wfIndex (250.0f), wfIndex (2500.0f), (float) nb - 1 };
            const Colour rc[3] = { th.lowCol, th.midCol, th.highCol };
            for (int k = 0; k < 3; ++k)
            {
                Path top, fill; const float b0 = edge[k], b1 = edge[k + 1];
                auto lvl = [&] (float bf) { const int i = jlimit (0, nb - 2, (int) bf); const float fr = bf - (float) i;
                                            return wfLevel (i, t) * (1.0f - fr) + wfLevel (i + 1, t) * fr; };
                fill.startNewSubPath (P (b0, t, -60));
                for (float bf = b0; ; bf = jmin (b1, std::floor (bf) + 1.0f))
                {
                    const auto q = P (bf, t, lvl (bf)); if (bf == b0) top.startNewSubPath (q); else top.lineTo (q); fill.lineTo (q);
                    if (bf >= b1) break;
                }
                fill.lineTo (P (b1, t, -60)); fill.closeSubPath();
                g.setColour (th.display.interpolatedWith (rc[k], 0.06f + 0.16f * (1.0f - age))); g.fillPath (fill);
                g.setColour (rc[k].withAlpha (0.3f + 0.7f * (1.0f - age))); g.strokePath (top, PathStrokeType (si == 0 ? 1.9f : 1.1f));
            }
        }
    }
    float tMax() const { const float t = t30(); return t <= 0 ? 2.5f : jlimit (0.5f, 2.5f, t * 1.5f); }
    float t30() const { const auto& e = shown.band[1]; for (size_t i = 0; i < e.size(); ++i) if (e[i] < -30.0f) return (float) i * 0.01f; return e.empty() ? -1.0f : 2.5f; }
    static float yOf (Rectangle<float> a, float db) { return a.getBottom() - a.getHeight() * (jlimit (-60.0f, 0.0f, db) + 60.0f) / 60.0f; }
    void grid (Graphics& g, Rectangle<float> a, float tm, float step, const String& unit)
    {
        const Colour col = th.displayText; g.setFont (ui (th.sz (8.0f), W::Medium));
        for (float t = 0; t <= tm + 1e-4f; t += step)
        {
            const float x = a.getX() + a.getWidth() * t / tm;
            g.setColour (col.withAlpha (0.08f)); g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
            g.setColour (col.withAlpha (0.5f)); g.drawText (String (t, unit == "s" && step < 1 ? 1 : 0) + " " + unit, Rectangle<float> (x - th.sz (24.0f), a.getBottom() + 1, th.sz (48.0f), th.sz (12.0f)), Justification::centred, false);
        }
        for (float db : { 0.0f, -20.0f, -40.0f, -60.0f })
        {
            const float y = yOf (a, db); g.setColour (col.withAlpha (0.08f)); g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
            g.setColour (col.withAlpha (0.5f)); g.drawText (String ((int) db), Rectangle<float> (a.getX() - th.sz (28.0f), y - th.sz (6.0f), th.sz (25.0f), th.sz (12.0f)), Justification::right, false);
        }
    }
    const Theme& th; ResponseRenderer::Result target, shown; Rectangle<float> erArea, decArea; Point<float> handle { -100, -100 };
    int dragging = 0; float startRt = 1, startX = 0;
};

// ============================================================================ a number on the screen: click and drag up/down, or double-click and type
class NumberCell : public Component, private TextEditor::Listener
{
public:
    explicit NumberCell (const Theme& t) : th (t) { addChildComponent (ed); ed.addListener (this); ed.setJustification (Justification::centred); }
    std::function<String()> name, value;
    std::function<float()> getNorm; std::function<void (float)> setNorm; std::function<void (bool)> gesture; std::function<void (const String&)> setText;
    bool hot = false;
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1, 1);
        g.setColour (th.display.withAlpha (0.72f)); g.fillRoundedRectangle (r, 3);                 // keeps the number readable over the curves
        if (hot || isMouseOver()) { g.setColour (th.displayText.withAlpha (0.14f)); g.fillRoundedRectangle (r, 3); }
        const auto nf = ui (nameH(), W::Medium); const String nm = name ? name() : String();
        g.setColour (th.displayText.withAlpha (0.6f)); g.setFont (nf);
        g.drawText (nm, r.reduced (4, 0), Justification::centredLeft, true);
        if (! ed.isVisible()) { g.setColour (th.displayText); g.setFont (ui (valueH(), W::Bold)); g.drawText (value ? value() : String(), r.reduced (4, 0).withTrimmedLeft (textW (nf, nm) + 7), Justification::centredLeft, true); }
    }
    void resized() override { ed.setBounds (getLocalBounds().reduced (1).withTrimmedLeft (getWidth() / 2)); }
    float nameH() const { return jmax (8.0f, th.displayPt * 0.72f); }
    float valueH() const { return jmax (8.5f, th.displayPt * 0.85f); }
    int preferredWidth() const { return (int) (textW (ui (nameH(), W::Medium), name ? name() : String()) + textW (ui (valueH(), W::Bold), value ? value() + "0" : String ("000 ms"))) + 18; }
    int preferredHeight() const { return (int) (valueH() + 7); }
    void mouseEnter (const MouseEvent&) override { repaint(); }
    void mouseExit (const MouseEvent&) override { repaint(); }
    void mouseDown (const MouseEvent& e) override
    {
        if (ed.isVisible()) return;
        dragging = true; start = getNorm ? getNorm() : 0.0f; if (gesture) gesture (true); setMouseCursor (MouseCursor::UpDownResizeCursor);
    }
    void mouseDoubleClick (const MouseEvent&) override
    {
        ed.setFont (ui (th.displayPt, W::Bold)); ed.setColour (TextEditor::backgroundColourId, th.display.brighter (0.15f)); ed.setColour (TextEditor::textColourId, th.displayText);
        ed.setColour (TextEditor::outlineColourId, th.displayText.withAlpha (0.5f));
        ed.setText (value ? value() : String(), false); ed.setVisible (true); ed.grabKeyboardFocus(); ed.selectAll(); repaint();
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (! dragging || ! setNorm) return;
        const float fine = e.mods.isShiftDown() ? 0.1f : 1.0f;
        setNorm (jlimit (0.0f, 1.0f, start - (float) e.getDistanceFromDragStartY() * 0.004f * fine)); repaint();
    }
    void mouseUp (const MouseEvent&) override { if (dragging) { dragging = false; if (gesture) gesture (false); setMouseCursor (MouseCursor::NormalCursor); } }
private:
    void textEditorReturnKeyPressed (TextEditor&) override { if (setText) setText (ed.getText()); ed.setVisible (false); repaint(); }
    void textEditorEscapeKeyPressed (TextEditor&) override { ed.setVisible (false); repaint(); }
    void textEditorFocusLost (TextEditor&) override { ed.setVisible (false); repaint(); }
    const Theme& th; TextEditor ed; bool dragging = false; float start = 0;
};

// a key that steps through the choices of a parameter (its text shows the current one)
class CycleKey : public LarcKey
{
public:
    CycleKey (const Theme& t, AudioProcessorValueTreeState& s, const String& id, const String& prefix) : LarcKey (t, prefix), param (s.getParameter (id)), pre (prefix)
    {
        onClick = [this] { const int n = param->getNumSteps(); const int v = (int) param->convertFrom0to1 (param->getValue()); param->beginChangeGesture();
                           param->setValueNotifyingHost (param->convertTo0to1 ((float) ((v + 1) % jmax (2, n)))); param->endChangeGesture(); };
    }
    bool sync() { const String t = pre + (pre.isEmpty() ? "" : " ") + param->getCurrentValueAsText().toUpperCase(); if (t == getText()) return false; setText (t); return true; }
private:
    RangedAudioParameter* param; String pre;
};

// ============================================================================ SETUP: beside the plugin, so changes show live
// ============================================================================ the compressor's VU meter: gain reduction, VU ballistics
class VuMeter : public Component
{
public:
    explicit VuMeter (const Theme& t) : th (t) {}
    void setGr (float grDb, float dtMs)
    {
        const float k = 1.0f - std::exp (-dtMs / 65.0f);                // about 300 ms to 99 %: a VU needle
        const float old = shown; shown += (jlimit (0.0f, 30.0f, grDb) - shown) * k;
        if (std::abs (old - shown) > 0.01f) repaint();
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1);
        // housing
        g.setColour (Colours::black.withAlpha (th.shadow())); g.fillRoundedRectangle (r.translated (1.5f * th.d3(), 2.5f * th.d3()), 6);
        g.setGradientFill (ColourGradient (Colour (0xff2a2b2e), r.getX(), r.getY(), Colour (0xff0e0f10), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 6);
        auto face = r.reduced (8, 8);
        g.setGradientFill (th.grad (&Theme::vuFace, face, 0.35f));
        g.fillRoundedRectangle (face, 3);
        // scale: 0 at the right, gain reduction to the left
        const Point<float> pv (face.getCentreX(), face.getBottom() + face.getHeight() * 0.55f);
        const float rad = face.getHeight() * 1.18f, a0 = -0.78f, a1 = 0.78f;
        auto ang = [&] (float gr) { const float u = 1.0f - std::pow (jlimit (0.0f, 20.0f, gr) / 20.0f, 0.62f); return a0 + (a1 - a0) * u; };
        Path arc; arc.addCentredArc (pv.x, pv.y, rad, rad, 0, a0, a1, true);
        g.setColour (Colour (0xff1b1b1b)); g.strokePath (arc, PathStrokeType (1.3f));
        Path red; red.addCentredArc (pv.x, pv.y, rad + 3, rad + 3, 0, ang (6.0f), ang (20.0f), true);
        g.setColour (Colour (0xffc0392b)); g.strokePath (red, PathStrokeType (3.0f));
        g.setFont (ui (jmax (8.0f, face.getHeight() * 0.13f), W::Bold));
        for (float gr : { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 10.0f, 15.0f, 20.0f })
        {
            const float a = ang (gr); const bool major = gr == 0 || gr == 2 || gr == 4 || gr == 6 || gr == 10 || gr == 20;
            g.setColour (gr >= 6 ? Colour (0xffb02a1e) : Colour (0xff1b1b1b));
            g.drawLine (Line<float> (pv.getPointOnCircumference (rad, a), pv.getPointOnCircumference (rad + (major ? 8.0f : 5.0f), a)), major ? 1.4f : 1.0f);
            if (major) { const auto q = pv.getPointOnCircumference (rad + 16.0f, a); g.drawText (gr == 0 ? "0" : "-" + String ((int) gr), Rectangle<float> (q.x - 14, q.y - 7, 28, 14), Justification::centred, false); }
        }
        g.setColour (Colour (0xff1b1b1b)); g.setFont (ui (jmax (8.0f, face.getHeight() * 0.12f), W::Bold, 0.1f));
        g.drawText ("GAIN REDUCTION  dB", face.withTrimmedTop (face.getHeight() * 0.52f).withHeight (face.getHeight() * 0.2f), Justification::centred, false);
        g.setFont (ui (jmax (9.0f, face.getHeight() * 0.16f), W::Bold, 0.05f));
        g.drawText ("VU", face.withTrimmedTop (face.getHeight() * 0.7f), Justification::centred, false);
        // needle
        g.saveState(); g.reduceClipRegion (face.toNearestInt());
        const float a = ang (shown);
        g.setColour (Colours::black.withAlpha (0.18f)); g.drawLine (Line<float> (pv.translated (2, 2), pv.getPointOnCircumference (rad + 6, a).translated (2, 2)), 1.6f);
        g.setColour (Colour (0xff111111)); g.drawLine (Line<float> (pv, pv.getPointOnCircumference (rad + 6, a)), 1.4f);
        g.restoreState();
        // glass
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.28f * jmax (0.3f, th.gradient)), face.getX(), face.getY(), Colours::transparentWhite, face.getX(), face.getCentreY(), false));
        g.fillRoundedRectangle (face.withHeight (face.getHeight() * 0.5f), 3);
        g.setColour (Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (face, 3, 1.0f);
    }
private:
    const Theme& th; float shown = 0;
};

// ============================================================================ an object's three colours: top, middle, bottom
class GradEditor : public Component
{
public:
    GradEditor (Theme& t, int index, const String& nm) : th (t), idx (index), name (nm)
    {
        setSize (330, 150);
        addAndMakeVisible (autoBtn); autoBtn.onClick = [this] { th.gOwn[idx] = false; if (changed) changed(); repaint(); };
    }
    std::function<void()> changed;
    void resized() override { autoBtn.setBounds (12, getHeight() - 36, getWidth() - 24, 26); }
    void paint (Graphics& g) override
    {
        g.fillAll (Colour (0xff1c1d1f));
        drawLegend (g, name + "  -  three colours", Rectangle<float> (12, 6, (float) getWidth() - 24, 18), ui (12.5f, W::Bold), Colours::white, Justification::centredLeft);
        const auto st = th.stops (item());
        const char* lab[] = { "Top", "Middle", "Bottom" };
        for (int k = 0; k < 3; ++k)
        {
            auto r = box (k).toFloat();
            g.setColour (st[(size_t) k]); g.fillRoundedRectangle (r, 4); g.setColour (Colours::white.withAlpha (0.5f)); g.drawRoundedRectangle (r, 4, 1.0f);
            drawLegend (g, lab[k], r.translated (0, r.getHeight() + 2).withHeight (14), ui (11.0f, W::Medium), Colours::white.withAlpha (0.8f));
        }
        auto pr = Rectangle<float> ((float) getWidth() - 60, 30, 46, 64);
        g.setGradientFill (th.grad (item(), pr)); g.fillRoundedRectangle (pr, 4);
        drawLegend (g, th.gOwn[idx] ? "your own" : "automatic", pr.translated (0, 66).withHeight (14), ui (10.0f, W::Medium), Colours::white.withAlpha (0.6f));
    }
    void mouseDown (const MouseEvent& e) override
    {
        for (int k = 0; k < 3; ++k)
            if (box (k).contains (e.getPosition()))
            {
                if (! th.gOwn[idx]) { const auto st = th.stops (item()); th.gTop[idx] = st[0]; th.gBot[idx] = st[2]; }
                auto sel = std::make_unique<ColourSelector> (ColourSelector::showColourspace | ColourSelector::showSliders | ColourSelector::showColourAtTop);
                sel->setSize (300, 300); sel->setCurrentColour (k == 0 ? th.gTop[idx] : k == 1 ? th.*item() : th.gBot[idx]);
                struct Lst : ChangeListener { GradEditor* p; int k; void changeListenerCallback (ChangeBroadcaster* b) override
                { const auto c = static_cast<ColourSelector*> (b)->getCurrentColour();
                  if (k == 1) p->th.*(p->item()) = c; else { p->th.gOwn[p->idx] = true; (k == 0 ? p->th.gTop : p->th.gBot)[p->idx] = c; }
                  p->repaint(); if (p->changed) p->changed(); } };
                auto* l = new Lst(); l->p = this; l->k = k; sel->addChangeListener (l); listener.reset (l);
                CallOutBox::launchAsynchronously (std::move (sel), box (k).translated (getScreenX(), getScreenY()), nullptr);
                return;
            }
    }
    Theme& th; int idx;
private:
    Colour Theme::* item() const { return Theme::items()[(size_t) idx].c; }
    Rectangle<int> box (int k) const { return { 12 + k * 82, 30, 70, 50 }; }
    String name; TextButton autoBtn { "AUTOMATIC  (from the middle colour)" }; std::unique_ptr<ChangeListener> listener;
};

class SetupPanel : public Component
{
public:
    SetupPanel (Theme& t, Flexiverb480Processor& p) : th (t), proc (p)
    {
        auto addSl = [this] (Slider& s, double lo, double hi, double step)
        { s.setSliderStyle (Slider::LinearHorizontal); s.setTextBoxStyle (Slider::TextBoxRight, false, 44, 20); s.setRange (lo, hi, step); addAndMakeVisible (s); s.onValueChange = [this] { fromSliders(); }; };
        addSl (grad, 0, 1, 0.01); addSl (keyPt, 9, 20, 0.5); addSl (keyHt, -10, 30, 1); addSl (keyWd, 0, 80, 1); addSl (knobPt, 8, 18, 0.5); addSl (dispPt, 9, 22, 0.5); addSl (faderPt, 8, 18, 0.5);
        for (auto* c : { &osBox, &precBox, &depthBox }) addAndMakeVisible (*c);
        osBox.addItemList ({ "Off", "2x", "4x", "6x", "8x" }, 1); precBox.addItemList ({ "32-bit", "64-bit" }, 1); depthBox.addItemList ({ "Flat", "Soft", "Deep", "Extra deep" }, 1);
        depthBox.onChange = [this] { th.depth = depthBox.getSelectedItemIndex(); if (onChange) onChange(); };
        osAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "os", osBox);
        precAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "prec", precBox);
        for (auto* b : { &resetBtn, &saveBtn, &closeBtn, &addKeyBtn }) addAndMakeVisible (*b);
        addKeyBtn.onClick = [this] { if (onAddKey) onAddKey(); };
        resetBtn.onClick = [this] { th = Theme(); sync(); if (onChange) onChange(); };
        saveBtn.onClick = [this] { Theme::defaultFile().getParentDirectory().createDirectory(); Theme::defaultFile().replaceWithText (th.toString()); saveBtn.setButtonText ("SAVED AS DEFAULT"); };
        closeBtn.onClick = [this] { if (onClose) onClose(); };
        sync();
    }
    std::function<void()> onChange, onClose, onAddKey;
    void sync()
    {
        grad.setValue (th.gradient, dontSendNotification); keyPt.setValue (th.keyPt, dontSendNotification); keyHt.setValue (th.keyHeight, dontSendNotification); keyWd.setValue (th.keyPad, dontSendNotification); knobPt.setValue (th.knobPt, dontSendNotification);
        dispPt.setValue (th.displayPt, dontSendNotification); faderPt.setValue (th.faderPt, dontSendNotification);
        depthBox.setSelectedItemIndex (th.depth, dontSendNotification); saveBtn.setButtonText ("SAVE AS DEFAULT FOR NEW WINDOWS"); repaint();
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (18); r.removeFromTop (40);
        swatches.clear();
        for (int i = 0; i < (int) Theme::items().size(); ++i) swatches.push_back (Rectangle<int> (r.getX(), r.getY() + 22 + i * 26, r.getWidth(), 22));
        int y = r.getY() + 22 + (int) Theme::items().size() * 26 + 30;
        lookTop = y - 22;
        for (auto* s : { &grad, &keyPt, &keyHt, &keyWd, &knobPt, &dispPt, &faderPt }) { s->setBounds (r.getX() + 110, y, r.getWidth() - 110, 22); y += 30; }
        depthBox.setBounds (r.getX() + 110, y, 120, 24); y += 34;
        addKeyBtn.setBounds (r.getX(), y, r.getWidth(), 28); y += 52;
        qualTop = y - 22;
        osBox.setBounds (r.getX() + 110, y, 120, 24); y += 30;
        precBox.setBounds (r.getX() + 110, y, 120, 24); y += 44;
        resetBtn.setBounds (r.getX(), y, r.getWidth(), 28); y += 34;
        saveBtn.setBounds (r.getX(), y, r.getWidth(), 28); y += 34;
        closeBtn.setBounds (r.getX(), y, r.getWidth(), 28);
    }
    void paint (Graphics& g) override
    {
        g.fillAll (th.cheek.darker (0.3f));
        auto r = getLocalBounds().reduced (18);
        drawLegend (g, "SETUP", r.removeFromTop (34).toFloat(), ui (18.0f, W::Bold, 0.12f), th.panelText, Justification::centredLeft);
        drawLegend (g, "Colours  (click one: top, middle and bottom)", Rectangle<float> ((float) r.getX(), (float) r.getY() + 4, 300, 16), ui (11.5f, W::Bold), th.keyLight, Justification::centredLeft);
        int i = 0;
        for (auto& it : Theme::items())
        {
            auto s = swatches[(size_t) i++].toFloat(); auto box = s.removeFromLeft (40);
            g.setGradientFill (th.grad (it.c, box));
            g.fillRoundedRectangle (box, 4); g.setColour (Colours::white.withAlpha (0.4f)); g.drawRoundedRectangle (box, 4, 1.0f);
            drawLegend (g, it.name, s.withTrimmedLeft (8), ui (12.0f, W::Medium), th.panelText, Justification::centredLeft);
        }
        const float x = (float) r.getX();
        drawLegend (g, "Look", Rectangle<float> (x, (float) lookTop, 200, 16), ui (11.5f, W::Bold), th.keyLight, Justification::centredLeft);
        const char* lab[] = { "Gradient", "Button text", "Button height +", "Button width +", "Knob text", "Screen text", "Fader text", "3D" };
        for (int k = 0; k < 8; ++k) drawLegend (g, lab[k], Rectangle<float> (x, (float) lookTop + 22 + k * 30, 105, 22), ui (12.0f, W::Medium), th.panelText, Justification::centredLeft);
        drawLegend (g, "Quality  (analog, EQ, compressor)", Rectangle<float> (x, (float) qualTop, 300, 16), ui (11.5f, W::Bold), th.keyLight, Justification::centredLeft);
        drawLegend (g, "Oversampling", Rectangle<float> (x, (float) qualTop + 22, 105, 24), ui (12.0f, W::Medium), th.panelText, Justification::centredLeft);
        drawLegend (g, "Precision", Rectangle<float> (x, (float) qualTop + 52, 105, 24), ui (12.0f, W::Medium), th.panelText, Justification::centredLeft);
    }
    void mouseDown (const MouseEvent& e) override
    {
        int i = 0;
        for (auto& it : Theme::items())
            if (swatches[(size_t) i++].contains (e.getPosition()))
            {
                auto ge = std::make_unique<GradEditor> (th, i - 1, it.name);
                ge->changed = [this] { repaint(); if (onChange) onChange(); };
                CallOutBox::launchAsynchronously (std::move (ge), swatches[(size_t) i - 1], this);
                return;
            }
    }
    Theme& th;
private:
    void fromSliders() { th.gradient = (float) grad.getValue(); th.keyPt = (float) keyPt.getValue(); th.keyHeight = (float) keyHt.getValue(); th.keyPad = (float) keyWd.getValue(); th.knobPt = (float) knobPt.getValue(); th.displayPt = (float) dispPt.getValue(); th.faderPt = (float) faderPt.getValue(); repaint(); if (onChange) onChange(); }
    Flexiverb480Processor& proc;
    Slider grad, keyPt, keyHt, keyWd, knobPt, dispPt, faderPt; ComboBox osBox, precBox, depthBox;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> osAtt, precAtt;
    TextButton resetBtn { "RESET THE LOOK" }, saveBtn { "SAVE AS DEFAULT FOR NEW WINDOWS" }, closeBtn { "CLOSE" }, addKeyBtn { "MAKE A BUTTON FOR THE CURRENT PRESET" };
    std::vector<Rectangle<int>> swatches; int lookTop = 0, qualTop = 0;
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
    static constexpr int kL = 780, kH = 920, kDrawer = 390, kSetup = 340, kMaxFaders = 40;

    explicit Flexiverb480Editor (Flexiverb480Processor& p) : AudioProcessorEditor (p), proc (p), decay (theme, p), delayView (theme, p), reverbView (theme), browser (theme, p), setup (theme, p)
    {
        if (! theme.fromString (proc.getUiProperty ("themeData", String()).toString())) theme.fromString (Theme::defaultFile().loadFileAsString());
        scale = (float) (double) proc.getUiProperty ("uiScale", 0.8);
        drawerOpen = (bool) proc.getUiProperty ("uiDrawer", false);

        addAndMakeVisible (content); content.ed = this;
        // screen
        content.addAndMakeVisible (screen); screen.ed = this;
        screen.addAndMakeVisible (reverbView); screen.addAndMakeVisible (decay); screen.addChildComponent (delayView);
        decay.machine = [this] { return machine; }; delayView.machine = [this] { return machine; };
        delayView.utility = [this] { return delayTaps (proc.algoOf (machine)).empty(); };
        auto shower = [this] (const String& a, const String& b) { showMessage (a, b); };
        decay.onShow = shower; delayView.onShow = shower; reverbView.onShow = shower;
        reverbView.getPredelayMs = [this] { const int s = findSlot ({ "PDL", "RPD" }); return s < 0 ? -1.0f : proc.slotValue (machine, s); };
        reverbView.setPredelayMs = [this] (float ms) { const int s = findSlot ({ "PDL", "RPD" }); if (s >= 0) proc.setSlot (machine, s, l480::toNorm (proc.spec (machine, s), ms)); };
        reverbView.getRt = [this] { return proc.mainRt (machine); };
        reverbView.setRt = [this] (float rt) { const int s = l480::algoMainRtSlot (proc.algoOf (machine)); proc.setSlot (machine, s, l480::toNorm (proc.spec (machine, s), rt)); };
        reverbView.gesture = [this] (bool b) { for (int s : { findSlot ({ "PDL", "RPD" }), l480::algoMainRtSlot (proc.algoOf (machine)) }) if (s >= 0) { if (b) proc.beginSlot (machine, s); else proc.endSlot (machine, s); } };
        for (int i = 0; i < 40; ++i) { cells.push_back (std::make_unique<NumberCell> (theme)); screen.addChildComponent (*cells.back()); }

        // preset bar, page and algorithm keys
        auto mk = [this] (const String& t) { auto k = std::make_unique<LarcKey> (theme, t); content.addAndMakeVisible (*k); return k; };
        content.addAndMakeVisible (presetName); presetName.ed = this;
        prevKey = mk ("<"); nextKey = mk (">"); saveKey = mk ("SAVE"); panelKey = mk ("PANEL >");
        for (auto* k : { prevKey.get(), nextKey.get(), saveKey.get(), panelKey.get() }) k->kc = &Theme::keyBar;
        prevKey->onClick = [this] { stepPreset (-1); }; nextKey->onClick = [this] { stepPreset (1); }; saveKey->onClick = [this] { savePreset(); };
        panelKey->onClick = [this] { drawerOpen = ! drawerOpen; proc.setUiProperty ("uiDrawer", drawerOpen); applySize(); };
        pageKey = mk ("REVERB"); pageKey->onClick = [this] { page = 1 - page; refreshPage (true); };
        struct Alg { const char* name; int bank, num; };
        static const Alg algs[] = { { "HALL", 1, 1 }, { "RANDOM HALL", 11, 1 }, { "ROOM", 2, 2 }, { "CHAMBER", 2, 8 }, { "PLATE", 4, 1 }, { "AMBIENCE", 13, 2 },
                                    { "INVERSE", 3, 8 }, { "DELAY", 6, 2 }, { "CHORUS", 20, 2 }, { "PITCH", 8, 2 } };
        for (auto& a : algs) { auto k = mk (a.name); int pi = l480::findProgram (a.bank, a.num);
                                if (isMatchedOriginal (pi)) for (int i = 0; i < l480::numPrograms(); ++i)   // the matched version of it
                                { const auto& q = l480::programs()[(size_t) i]; if (q.bank >= 27 && q.bank <= 31 && presetKey (q.name) == presetKey (l480::programs()[(size_t) pi].name)) { pi = i; break; } } k->onClick = [this, pi] { proc.loadProgram (machine, pi); page = 0; refreshPage (true); };
                                k->onRightClick = [this] { keyMenu (-1); }; algKeys.push_back (std::move (k)); }
        pageKey->onRightClick = [this] { keyMenu (-1); };
        loadMyKeys();

        for (int i = 0; i < kMaxFaders; ++i)
        {
            auto f = std::make_unique<LarcFader> (theme); content.addChildComponent (*f);
            f->onBegin = [this, i] { faderBegin (i); }; f->onMove = [this, i] (float t) { faderMove (i, t); };
            f->onEnd = [this, i] { faderEnd (i); }; f->onReset = [this, i] { faderReset (i); };
            faders.push_back (std::move (f));
        }

        // drawer (scrolls when longer than the window)
        drawer.addAndMakeVisible (vu);
        content.addAndMakeVisible (drawerView); drawerView.setViewedComponent (&drawer, false); drawerView.setScrollBarsShown (false, false);
        drawer.ed = this;
        auto dk = [this] (const String& t) { auto k = std::make_unique<LarcKey> (theme, t); k->kc = &Theme::keyPanel; drawer.addAndMakeVisible (*k); return k; };
        auto pk = [this] (const String& id, const String& t) { auto k = std::make_unique<ParamKey> (theme, proc.apvts, id, t); k->kc = &Theme::keyPanel; drawer.addAndMakeVisible (*k); return k; };
        auto ck = [this] (const String& id, const String& t) { auto k = std::make_unique<CycleKey> (theme, proc.apvts, id, t); k->kc = &Theme::keyPanel; drawer.addAndMakeVisible (*k); return k; };
        // ENGINE: single (machine A) or dual (machines A and B side by side)
        drawer.addAndMakeVisible (engineBox); engineBox.addItemList ({ "Single", "Dual" }, 1);
        engineBox.onChange = [this] { auto* c = proc.apvts.getParameter ("config"); const int v = engineBox.getSelectedItemIndex() == 0 ? 0 : 2;
                                      if ((config() != 0) != (v != 0)) { c->beginChangeGesture(); c->setValueNotifyingHost (c->convertTo0to1 ((float) v)); c->endChangeGesture(); } };
        machAKey = dk ("MACHINE A"); machBKey = dk ("MACHINE B"); muteKey = dk ("MUTE"); setupKey = dk ("SETUP");
        machAKey->onClick = [this] { selectMachine (0); }; machBKey->onClick = [this] { selectMachine (1); };
        muteKey->onClick = [this] { proc.mute.store (! proc.mute.load()); };
        setupKey->onClick = [this] { setupOpen = ! setupOpen; if (setupOpen) setup.sync(); applySize(); };
        ironKey = pk ("trOn", "IRON");
        anOnKey = pk ("anOn", "ANALOG"); eqOnKey = pk ("eqOn", "EQ"); cpOnKey = pk ("cpOn", "ON"); satKey = pk ("cpColor", "SAT"); duckKey = pk ("cpDuck", "DUCK");
        auto knob = [this, shower] (const String& id, const String& t, std::vector<ScaleMark> m, Colour Theme::* c)
        { auto k = std::make_unique<ScaleKnob> (theme, proc.apvts, id, t, std::move (m), c); k->onShow = shower; drawer.addAndMakeVisible (*k); return k; };
        sysKnob[0] = knob ("out", "OUTPUT", numberScale (-24, 12, 6, 2, true), &Theme::compKnob);
        sysKnob[1] = knob ("trModel", "IRON MODEL", [] { auto m = numberScale (1, (float) pv::kNumTransformers, 1, 1); for (auto& x : m) x.value -= 1; return m; }(), &Theme::compKnob);
        sysKnob[2] = knob ("trFreq", "IRON FREQ", { { 20, "20" }, { 30, "30" }, { 50, "50" }, { 80, "80" }, { 120, "120" }, { 200, "200" }, { 400, "400" } }, &Theme::compKnob);
        sysKnob[3] = knob ("trAmt", "IRON AMOUNT", { { 0, "0" }, { 0.2f, "20" }, { 0.4f, "40" }, { 0.6f, "60" }, { 0.8f, "80" }, { 1, "100" } }, &Theme::compKnob);
        anKnob[0] = knob ("anDrive", "DRIVE", numberScale (0, 30, 4, 2), &Theme::analogKnob);
        anKnob[1] = knob ("anOut", "OUTPUT", numberScale (-12, 12, 4, 2, true), &Theme::analogKnob);
        anKnob[2] = knob ("anModel", "MODEL", { { 0, "CLEAN" }, { 1, "TUBE" }, { 2, "1073" }, { 3, "1080" }, { 4, "212" } }, &Theme::analogKnob);
        anKnob[3] = knob ("anColour", "CHARACTER", { { 0, "ODD" }, { 1, "EVEN" }, { 2, "HEAVY" }, { 3, "CRUSH" } }, &Theme::analogKnob);
        anKnob[4] = knob ("anTarget", "AFFECTS", { { 0, "REVERB" }, { 1, "EQ" }, { 2, "BOTH" } }, &Theme::analogKnob);
        eqKnob2[0] = knob ("eqModel", "EQ MODEL", { { 0, "CLEAN" }, { 1, "73" }, { 2, "73/84" }, { 3, "4K" }, { 4, "4K U" }, { 5, "TUBE" }, { 6, "550" } }, &Theme::eqKnob);
        eqKnob2[1] = knob ("eqDrive", "EQ COLOUR", { { 0, "0" }, { 0.2f, "20" }, { 0.4f, "40" }, { 0.6f, "60" }, { 0.8f, "80" }, { 1, "100" } }, &Theme::eqKnob);
        const char* bn[] = { "LOW", "LOW MID", "HIGH MID", "HIGH" };
        const std::vector<ScaleMark> fm[4] = {
            { { 20, "20" }, { 40, "40" }, { 70, "70" }, { 120, "120" }, { 200, "200" }, { 300, "300" }, { 500, "500" } },
            { { 100, "100" }, { 200, "200" }, { 350, "350" }, { 600, "600" }, { 1000, "1k" }, { 2000, "2k" } },
            { { 800, ".8k" }, { 1200, "1.2k" }, { 2000, "2k" }, { 3000, "3k" }, { 5000, "5k" }, { 8000, "8k" } },
            { { 2000, "2k" }, { 3500, "3.5k" }, { 5000, "5k" }, { 8000, "8k" }, { 12000, "12k" }, { 20000, "20k" } } };
        for (int b = 0; b < 4; ++b) { eqKnob[b] = std::make_unique<DualKnob> (theme, proc.apvts, "eqF" + String (b), "eqG" + String (b), bn[b], fm[b]); eqKnob[b]->onShow = shower; drawer.addAndMakeVisible (*eqKnob[b]); }
        cpKnob[0] = knob ("cpType", "TYPE", { { 0, "VCA" }, { 1, "FET" }, { 2, "OPTO" } }, &Theme::compKnob);
        cpKnob[1] = knob ("cpThr", "THRESHOLD", numberScale (-60, 0, 10, 2), &Theme::compKnob);
        cpKnob[2] = knob ("cpRatio", "RATIO", { { 1, "1" }, { 1.5f, "1.5" }, { 2, "2" }, { 3, "3" }, { 4, "4" }, { 6, "6" }, { 10, "10" }, { 20, "20" } }, &Theme::compKnob);
        cpKnob[3] = knob ("cpAtk", "ATTACK", { { 0.05f, ".05" }, { 0.2f, ".2" }, { 1, "1" }, { 3, "3" }, { 10, "10" }, { 30, "30" }, { 100, "100" }, { 300, "300" } }, &Theme::compKnob);
        cpKnob[4] = knob ("cpRel", "RELEASE", { { 20, "20" }, { 50, "50" }, { 100, "100" }, { 200, "200" }, { 500, "500" }, { 1000, "1k" }, { 2000, "2k" }, { 5000, "5k" } }, &Theme::compKnob);
        cpKnob[5] = knob ("cpMakeup", "MAKEUP", numberScale (0, 24, 4, 2), &Theme::compKnob);
        for (auto* k : { anKnob[0].get(), anKnob[1].get(), sysKnob[0].get() }) k->step = 2.0f;
        cpKnob[6] = knob ("cpMix", "MIX", { { 0, "0" }, { 0.2f, "20" }, { 0.4f, "40" }, { 0.6f, "60" }, { 0.8f, "80" }, { 1, "100" } }, &Theme::compKnob);

        cpKnob[1]->step = 2.0f; cpKnob[5]->step = 2.0f;
        content.addAndMakeVisible (setup); setup.onAddKey = [this] { addMyKey(); }; setup.onClose = [this] { setupOpen = false; applySize(); }; setup.onChange = [this] { lookChanged(); };
        content.addChildComponent (browser);
        browser.machine = [this] { return machine; };
        browser.onClose = [this] { browser.setVisible (false); };
        browser.onFactory = [this] (int i) { proc.loadProgram (machine, i); page = 0; refreshPage (true); browser.setVisible (false); };
        browser.onUser = [this] (const String& n) { proc.loadUserPreset (n); page = 0; refreshPage (true); browser.setVisible (false); };

        lnf.setColour (PopupMenu::backgroundColourId, Colour (0xff1c1d1f)); lnf.setColour (PopupMenu::textColourId, Colour (0xffe8e6e1));
        lnf.setColour (PopupMenu::highlightedBackgroundColourId, theme.keyLight.darker (0.2f)); lnf.setColour (PopupMenu::highlightedTextColourId, Colours::black);
        lnf.setColour (ComboBox::backgroundColourId, theme.display); lnf.setColour (ComboBox::textColourId, theme.displayText);
        lnf.setColour (ComboBox::outlineColourId, Colours::black); lnf.setColour (ComboBox::arrowColourId, theme.keyLight);
        lnf.setDefaultSansSerifTypeface (FlexFonts::get().medium);
        setLookAndFeel (&lnf);
        setResizable (true, true);
        layout(); applySize(); refreshPage (true);
        startTimerHz (30);
    }
    ~Flexiverb480Editor() override { stopTimer(); setLookAndFeel (nullptr); }

    void paint (Graphics& g) override { g.setGradientFill (theme.grad (&Theme::cheek, getLocalBounds().toFloat(), 0.6f)); g.fillAll(); }
    void resized() override
    {
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
        void mouseDown (const MouseEvent& e) override { if (ed && e.mods.isPopupMenu()) ed->showSizeMenu(); }
    };
    struct Drawer : public Component
    {
        Flexiverb480Editor* ed = nullptr;
        void paint (Graphics& g) override { if (ed) ed->paintDrawer (g); }
        void mouseDown (const MouseEvent& e) override { if (ed && e.mods.isPopupMenu()) ed->showSizeMenu(); }
    };
    struct Screen : public Component
    {
        Flexiverb480Editor* ed = nullptr;
        void paint (Graphics& g) override
        {
            const auto& th = ed->theme; auto r = getLocalBounds().toFloat();
            g.setColour (Colours::black.withAlpha (0.5f + 0.3f * th.d3())); g.fillRoundedRectangle (r, 6);
            auto in = r.reduced (3.0f);
            g.setGradientFill (th.grad (&Theme::display, in, 0.5f));
            g.fillRoundedRectangle (in, 4);
            // the status line (what was just changed)
            if (Time::getMillisecondCounter() < ed->msgUntil)
            {
                g.setColour (th.displayText); g.setFont (ui (th.displayPt, W::Bold));
                g.drawText (ed->msg1 + "   " + ed->msg2, in.reduced (12, 6).withHeight (th.displayPt + 6), Justification::centredLeft, true);
            }
        }
    };
    struct PresetName : public Component
    {
        Flexiverb480Editor* ed = nullptr; String text, sub;
        void paint (Graphics& g) override
        {
            const auto& th = ed->theme; auto r = getLocalBounds().toFloat();
            g.setColour (Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r, 5);
            auto in = r.reduced (2.5f);
            g.setGradientFill (th.grad (&Theme::display, in, 0.5f));
            g.fillRoundedRectangle (in, 4);
            if (isMouseOver()) { g.setColour (th.displayText.withAlpha (0.08f)); g.fillRoundedRectangle (in, 4); }
            auto t = in.reduced (12, 0);
            g.setColour (th.displayText.withAlpha (0.6f)); g.setFont (ui (jmin (12.0f, th.keyPt), W::Medium));
            g.drawText (sub + "   v", t, Justification::centredRight, true);
            g.setColour (th.displayText); g.setFont (ui (jmin (17.0f, th.keyPt * 1.3f), W::Bold));
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
        auto face = Rectangle<float> (0, 0, (float) kL, (float) kH);
        g.setGradientFill (theme.grad (&Theme::panel, face, 0.45f));
        g.fillRect (face);
        // names and values above the faders, in their own text size
        for (int i = 0; i < nShown; ++i)
        {
            auto b = faders[(size_t) i]->getBounds().toFloat();
            auto nr = Rectangle<float> (b.getX(), b.getY() - labelH, b.getWidth(), labelH - theme.faderPt - 4);
            g.setColour (theme.panelText); g.setFont (ui (theme.faderPt, W::Semibold));
            g.drawFittedText (targetName (pageTargets[(size_t) i]), nr.toNearestInt(), Justification::centredBottom, 3, 0.75f);
            g.setColour (theme.keyLight); g.setFont (ui (theme.faderPt, W::Bold));
            g.drawFittedText (targetText (pageTargets[(size_t) i]), Rectangle<float> (b.getX(), nr.getBottom() + 1, b.getWidth(), theme.faderPt + 3).toNearestInt(), Justification::centred, 1, 0.7f);
        }
        if (nShown == 0) drawLegend (g, "nothing on this page", faderArea.toFloat(), ui (13.0f, W::Medium), theme.panelText.withAlpha (0.5f));
        const auto f1 = ui (18.0f, W::Bold, -0.02f), f2 = ui (18.0f, W::Regular);
        const float w1 = textW (f1, "flexiverb"), w2 = textW (f2, " 480"), x = kL * 0.5f - (w1 + w2) / 2;
        drawLegend (g, "flexiverb", Rectangle<float> (x, (float) kH - 34, w1 + 2, 22), f1, theme.panelText, Justification::centredLeft);
        drawLegend (g, " 480", Rectangle<float> (x + w1, (float) kH - 34, w2 + 4, 22), f2, theme.panelText.withAlpha (0.6f), Justification::centredLeft);
    }
    void paintDrawer (Graphics& g)
    {
        auto r = drawer.getLocalBounds().toFloat();
        g.setGradientFill (theme.grad (&Theme::drawerPanel, r.withHeight ((float) kH), 0.45f));
        g.fillRect (r);
        drawLegend (g, "ENGINE", Rectangle<float> ((float) engineLabelX, (float) engineLabelY, 90, (float) keyH()), ui (jmax (9.0f, theme.keyPt * 0.9f), W::Bold, 0.08f), theme.panelText, Justification::centredLeft);
        for (auto& s : sections)
        {
            auto b = s.area.toFloat();
            if (s.title != "SYSTEM")
            {   // the channel strip: a blue-grey console panel with screws
                const auto sp = theme.stripPanel;
                g.setColour (Colours::black.withAlpha (theme.shadow())); g.fillRoundedRectangle (b.translated (1.5f * theme.d3(), 3.0f * theme.d3()), 5);
                g.setGradientFill (theme.grad (&Theme::stripPanel, b, 0.7f));
                g.fillRoundedRectangle (b, 5);
                g.setColour (Colours::white.withAlpha (0.025f));
                for (float yy = b.getY() + 2; yy < b.getBottom(); yy += 3) g.drawHorizontalLine ((int) yy, b.getX() + 2, b.getRight() - 2);
                g.setColour (Colours::white.withAlpha (0.18f)); g.drawRoundedRectangle (b, 5, 1.0f);
                for (auto c : { b.getTopLeft().translated (9, 9), b.getTopRight().translated (-9, 9), b.getBottomLeft().translated (9, -9), b.getBottomRight().translated (-9, -9) })
                {
                    const auto sr = Rectangle<float> (9, 9).withCentre (c);
                    g.setGradientFill (ColourGradient (Colour (0xffe2e4e6), sr.getX(), sr.getY(), Colour (0xff6d7277), sr.getRight(), sr.getBottom(), false)); g.fillEllipse (sr);
                    g.setColour (Colours::black.withAlpha (0.55f)); g.drawLine (sr.getX() + 2, sr.getCentreY(), sr.getRight() - 2, sr.getCentreY(), 1.2f);
                }
                const auto f = ui (jmax (10.0f, theme.knobPt * 1.0f), W::Bold, 0.18f);
                drawLegend (g, s.title, Rectangle<float> (b.getX() + 20, b.getY() + 4, b.getWidth() - 40, (float) keyH()), f, Colours::white.withAlpha (0.92f), Justification::centredLeft);
                continue;
            }
            g.setColour (theme.panelText.withAlpha (0.25f)); g.drawRoundedRectangle (b, 4, 1.0f);
            const auto f = ui (jmax (10.0f, theme.knobPt * 1.0f), W::Bold, 0.18f);
            drawLegend (g, s.title, Rectangle<float> (b.getX() + 10, b.getY() + 6, b.getWidth() - 20, (float) keyH()), f, theme.panelText.withAlpha (0.85f), Justification::centredLeft);
        }
    }

    // ---------------------------------------------------------------- layout
    struct Sec { String title; Rectangle<int> area; };
    std::vector<Sec> sections; Rectangle<int> faderArea; float labelH = 40;
    int keyH() const { return roundToInt (theme.keyPt * 2.0f + 12.0f + theme.keyHeight); }   // follows the text, plus your extra
    void layout()
    {
        const int x0 = 24, wAll = kL - 48, gap = 6, kh = keyH();
        int y = 14;
        // preset bar: the menu, then < > side by side, SAVE and the drawer key
        const int pw = (int) panelKey->preferredWidth(), sw = (int) saveKey->preferredWidth();
        panelKey->setBounds (x0 + wAll - pw, y, pw, kh); saveKey->setBounds (panelKey->getX() - gap - sw, y, sw, kh);
        nextKey->setBounds (saveKey->getX() - gap - kh, y, kh, kh); prevKey->setBounds (nextKey->getX() - 2 - kh, y, kh, kh);
        presetName.setBounds (x0, y, prevKey->getX() - gap - x0, kh);
        y += kh + 10;
        screen.setBounds (x0, y, wAll, 360); layoutScreen(); y += 370;
        std::vector<LarcKey*> keys { pageKey.get() }; for (auto& k : algKeys) keys.push_back (k.get()); for (auto& k : myKeys) keys.push_back (k.get());
        y = flowKeys (keys, x0, wAll, y, kh, gap) + 4;
        labelH = theme.faderPt * 3.6f + 14;
        faderArea = Rectangle<int> (x0, y + (int) labelH, wAll, kH - 46 - (y + (int) labelH));
        layoutFaders();
        layoutDrawer();
    }
    void layoutScreen()
    {
        auto r = screen.getLocalBounds().reduced (8);
        r.removeFromTop ((int) theme.displayPt + 8);                      // the status line
        reverbView.setBounds (r.withHeight (r.getHeight() * 2 / 3 - 3));
        decay.setBounds (r.withTrimmedTop (r.getHeight() * 2 / 3 + 3));
        delayView.setBounds (r);
        placeCells();
    }
    // the numbers sit on the picture: predelay at its line, decay time at the decay handle, the rest written across the
    // top of the lower graph (decay page) or of the delay picture. Click to type, or right-click and drag up/down.
    int slotPd() const { return findSlot ({ "PDL", "RPD" }); }
    void placeCells()
    {
        const int n = (int) jmin (pageTargets.size(), cells.size());
        const int algo = proc.algoOf (machine), rtSlot = l480::algoMainRtSlot (algo), pd = slotPd();
        const int xov = findSlot ({ "XOV" }), bas = findSlot ({ "BAS" }), rtc = findSlot ({ "RTC" });
        const bool rev = page == 0 && reverbView.isVisible();
        const float rt = decay.mainRtShown();
        // anchor of target i on the picture (screen coordinates), or -1 when it has no place of its own
        auto anchor = [&] (const Target& tg) -> Point<float>
        {
            auto in = [] (const Component& v, Point<float> a, float dx = 0, float dy = 0) { return a.x < 0 ? a : a + v.getPosition().toFloat() + Point<float> (dx, dy); };
            if (rev && tg.param == nullptr)
            {
                if (tg.slot == pd) return in (reverbView, reverbView.pdAnchor);
                if (tg.slot == rtSlot) return in (reverbView, reverbView.rtAnchor);
                if (tg.slot == xov) return in (reverbView, reverbView.floorAt (proc.slotValue (machine, xov)), -20, -22);
                if (tg.slot == bas) return in (reverbView, reverbView.floorAt (60.0f), 0, -22);
                if (tg.slot == rtc) return in (reverbView, reverbView.floorAt (jmin (12000.0f, proc.slotValue (machine, rtc))), -60, -22);
            }
            if (! rev && delayView.isVisible())
            {
                if (tg.param == nullptr && tg.slot >= 0) { const auto a = delayView.tapAt (tg.slot, -1); const String nm (proc.spec (machine, tg.slot).name);
                    const bool lvl = nm.contains ("Level"); return in (delayView, a, 6, lvl ? -18.0f : -36.0f); }
                if (tg.param != nullptr && (tg.name == "Delay Left" || tg.name == "Delay Right"))
                    return in (delayView, delayView.tapAt (-1, tg.name == "Delay Left" ? 0 : 1), 6, -36);
            }
            return { -1, -1 };
        };
        auto area = rev ? (reverbView.graphArea() + reverbView.getPosition().toFloat()).toNearestInt().withTrimmedTop (2)
                        : delayView.getBounds().reduced ((int) theme.sz (40.0f), 0).withTrimmedTop ((int) theme.sz (16.0f));
        std::vector<Rectangle<int>> used;
        int x = area.getX(), y = area.getY();
        if (! rev)      // delay picture: the taps rise from the bottom up to their level, so the list goes under them, at the bottom
        {
            int rows = 1, xx = area.getX(), hh = 16;
            for (int i = 0; i < n; ++i)
            {
                if (anchor (pageTargets[(size_t) i]).x >= 0) continue;
                const int w = cells[(size_t) i]->preferredWidth(); hh = cells[(size_t) i]->preferredHeight();
                if (xx + w > area.getRight() && xx > area.getX()) { xx = area.getX(); ++rows; }
                xx += w + 4;
            }
            y = jmax (area.getY(), area.getBottom() - 16 - rows * (hh + 2));
        }
        std::vector<int> anchored;
        for (int i = 0; i < (int) cells.size(); ++i)
        {
            auto& c = *cells[(size_t) i];
            if (i >= n) { c.setVisible (false); continue; }
            const auto& tg = pageTargets[(size_t) i];
            if (anchor (tg).x >= 0) { anchored.push_back (i); continue; }
            const int w = c.preferredWidth(), h = c.preferredHeight();
            if (x + w > area.getRight() && x > area.getX()) { x = area.getX(); y += h + 2; }
            c.setBounds (x, y, w, h); c.setVisible (true); used.push_back (c.getBounds()); x += w + 10;
        }
        const auto sb = screen.getLocalBounds().reduced (6);
        for (int i : anchored)
        {
            auto& c = *cells[(size_t) i]; const auto a = anchor (pageTargets[(size_t) i]);
            const int w = c.preferredWidth(), h = c.preferredHeight();
            Rectangle<int> rc (jlimit (sb.getX(), sb.getRight() - w, (int) a.x), jlimit (sb.getY(), sb.getBottom() - h, (int) a.y), w, h);
            const int dir = rc.getCentreY() > area.getCentreY() ? -1 : 1;
            for (int k = 0; k < 10; ++k)                      // step clear of numbers already placed
            {
                bool hitAny = false; for (auto& u : used) if (u.intersects (rc)) { hitAny = true; break; }
                if (! hitAny) break;
                rc.setY (jlimit (sb.getY(), sb.getBottom() - h, rc.getY() + dir * (h + 2)));
            }
            c.setBounds (rc); c.setVisible (true); used.push_back (rc);
        }
    }
    void layoutFaders()
    {
        nShown = jmin ((int) pageTargets.size(), kMaxFaders);
        const int rows = nShown > 16 ? 2 : 1, perRow = (nShown + rows - 1) / jmax (1, rows);
        const float colW = perRow > 0 ? faderArea.getWidth() / (float) perRow : 0;
        const int rowH = (faderArea.getHeight() - (rows - 1) * (int) labelH) / rows;
        for (int i = 0; i < kMaxFaders; ++i)
        {
            auto& f = *faders[(size_t) i];
            if (i >= nShown) { f.setVisible (false); continue; }
            const int row = i / perRow, col = i % perRow;
            f.setBounds ((int) (faderArea.getX() + col * colW), faderArea.getY() + row * (rowH + (int) labelH), (int) colW, rowH); f.setVisible (true);
        }
        content.repaint();
    }
    void layoutDrawer()
    {
        // No scrolling: every box edge sits on a line of the main window.
        //   SYSTEM      preset bar top        -> waterfall / decay line
        //   ANALOG      waterfall / decay line -> screen bottom
        //   EQUALISER   buttons top           -> a fader scale line
        //   COMPRESSOR  that fader line       -> faders bottom
        sections.clear();
        const int fx = 10, fw = kDrawer - 20, pad = 10, x0 = fx + pad, wAll = fw - 2 * pad, gap = 5, kh = keyH(), kTxt = roundToInt (theme.knobPt) + 5;
        const int A = presetName.getY(), S = screen.getY() + decay.getY() - 3, D = screen.getBottom();
        const int E = screen.getBottom() + 10;
        const int fTop = faderArea.getY() + 12, fBot = faderArea.getBottom() - 12, H = fBot + 6;
        int T = fBot; { int best = 1 << 30; for (int k = 0; k <= 10; ++k) { const int t = fBot - (fBot - fTop) * k / 10; if (std::abs (t - (E + 215)) < best) { best = std::abs (t - (E + 215)); T = t; } } }
        auto frame = [&] (const String& t, int top, int bot) { sections.push_back ({ t, { fx, top, fw, bot - top } }); };
        const int titleH = jmax (kh, roundToInt (theme.knobPt) + 10);
        // SYSTEM
        frame ("SYSTEM", A, S - 4);
        {
            int y = A + 6;
            machAKey->setVisible (config() != 0); machBKey->setVisible (config() != 0);
            const int lw = (int) textW (ui (jmax (9.0f, theme.keyPt * 0.9f), W::Bold, 0.08f), "ENGINE") + 14;
            engineLabelY = y; engineLabelX = x0 + (int) textW (ui (jmax (10.0f, theme.knobPt), W::Bold, 0.18f), "SYSTEM") + 24;
            engineBox.setBounds (engineLabelX + lw, y, 120, kh); y += kh + gap;
            y = flowKeys ({ machAKey.get(), machBKey.get(), muteKey.get(), setupKey.get(), ironKey.get() }, x0, wAll, y, kh, gap);
            const int qw = wAll / 4, kb = S - 4 - 6 - y;
            for (int i = 0; i < 4; ++i) sysKnob[i]->setBounds (x0 + i * qw, y, qw, jmax (40, kb));
        }
        // ANALOG
        frame ("ANALOG", S + 4, D);
        {
            const int y = S + 4 + 4;
            anOnKey->setBounds (x0 + wAll - (int) anOnKey->preferredWidth(), y, (int) anOnKey->preferredWidth(), kh);
            const int ky = y + titleH, kb = D - 4 - ky, w5 = wAll / 5;
            const int order[5] = { 0, 1, 4, 2, 3 };
            for (int i = 0; i < 5; ++i) anKnob[order[i]]->setBounds (x0 + i * w5, ky, w5, jmax (40, kb));
        }
        // EQUALISER
        frame ("EQUALISER", E, T - 4);
        {
            const int y = E + 4;
            eqOnKey->setBounds (x0 + wAll - (int) eqOnKey->preferredWidth(), y, (int) eqOnKey->preferredWidth(), kh);
            const int ky = y + titleH, avail = T - 4 - 4 - ky, r1 = avail * 38 / 100, w4 = wAll / 4;
            eqKnob2[0]->setBounds (x0 + wAll / 6, ky, wAll / 3, r1); eqKnob2[1]->setBounds (x0 + wAll / 2, ky, wAll / 3, r1);
            for (int b = 0; b < 4; ++b) eqKnob[b]->setBounds (x0 + b * w4, ky + r1, w4, avail - r1);
        }
        // COMPRESSOR, with the VU meter
        frame ("COMPRESSOR", T + 4, H);
        {
            const int y = T + 4 + 4;
            int kx = x0 + wAll;
            for (auto* k : { duckKey.get(), satKey.get(), cpOnKey.get() }) { const int w = (int) k->preferredWidth(); kx -= w; k->setBounds (kx, y, w, kh); kx -= gap; }
            const int ky = y + titleH, avail = H - 4 - ky, r1 = avail * 52 / 100, w5 = wAll / 5;
            const int vw = jmin (wAll * 52 / 100, (int) (r1 * 1.9f));
            vu.setBounds (x0, ky + 2, vw, r1 - 4);
            const int rw = (wAll - vw) / 2;
            cpKnob[1]->setBounds (x0 + vw, ky, rw, r1); cpKnob[2]->setBounds (x0 + vw + rw, ky, rw, r1);
            const int order[5] = { 0, 3, 4, 5, 6 };
            for (int i = 0; i < 5; ++i) cpKnob[order[i]]->setBounds (x0 + i * w5, ky + r1, w5, avail - r1);
        }
        drawer.setSize (kDrawer, kH);
    }
    int baseWidth() const { return kL + (drawerOpen ? kDrawer : 0) + (setupOpen ? kSetup : 0); }
    bool applying = false;
    void applySize()
    {
        applying = true;
        const int w = baseWidth();
        content.setBounds (0, 0, w, kH);
        drawerView.setBounds (kL, 0, kDrawer, kH); drawerView.setVisible (drawerOpen);
        setup.setBounds (kL + (drawerOpen ? kDrawer : 0), 0, kSetup, kH); setup.setVisible (setupOpen);
        browser.setBounds (0, 0, w, kH);
        panelKey->setText (drawerOpen ? "< PANEL" : "PANEL >");
        setResizeLimits (roundToInt (w * 0.3f), roundToInt (kH * 0.3f), roundToInt (w * 3.0f), roundToInt (kH * 3.0f));
        if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) w / kH);
        setSize (roundToInt (w * scale), roundToInt (kH * scale));
        applying = false;
        resized(); repaint(); content.repaint();
    }
    void showSizeMenu()
    {
        PopupMenu m; m.addSectionHeader ("Window size");
        const float sizes[] = { 0.5f, 0.6f, 0.7f, 0.8f, 1.0f, 1.25f, 1.5f };
        for (int i = 0; i < 7; ++i) m.addItem (i + 1, String (roundToInt (sizes[i] * 100)) + "%", true, std::abs (sizes[i] - scale) < 0.01f);
        m.addSeparator(); m.addItem (100, "Drag the lower right corner to scale freely", false);
        m.showMenuAsync (PopupMenu::Options(), [this, sizes] (int r) { if (r > 0 && r < 100) { scale = sizes[r - 1]; proc.setUiProperty ("uiScale", scale); applySize(); } });
    }
    void lookChanged()
    {
        proc.setUiProperty ("themeData", theme.toString());
        lnf.setColour (PopupMenu::highlightedBackgroundColourId, theme.keyLight.darker (0.2f));
        layout(); applySize();
        content.repaint(); for (auto* c : content.getChildren()) c->repaint(); for (auto* c : drawer.getChildren()) c->repaint(); for (auto* c : screen.getChildren()) c->repaint();
    }

    // ---------------------------------------------------------------- state
    int machine = 0, page = 0, lastConfig = -1, nShown = 0, lastAlgo = -1, lastPage = -1, lastRv = -1;
    bool drawerOpen = false, setupOpen = false;
    uint32 msgUntil = 0; String msg1, msg2;
    int config() const { return (int) proc.apvts.getRawParameterValue ("config")->load(); }
    void showMessage (const String& a, const String& b) { msg1 = a; msg2 = b; msgUntil = Time::getMillisecondCounter() + 1800; screen.repaint(); }
    int findSlot (std::initializer_list<const char*> labs) const
    {
        for (int i = 0; i < l480::kMaxParams; ++i) { const auto& s = proc.spec (machine, i); if (! s.lab || s.alias >= 0) continue; for (auto* l : labs) if (String (s.lab) == l) return i; }
        return -1;
    }

    // ---------------------------------------------------------------- pages: REVERB (the program) and DELAY (taps, echoes, the utility delay)
    std::vector<Target> pageTargets;
    static bool isDelaySpec (const l480::PSpec& s, int algo)
    {
        using namespace l480;
        if (algo == aTwin || algo == aChorus || algo == aMBand || algo == aFx || algo == aPrime) return true;
        const String n (s.name);
        return n.contains ("Echo") || n.contains ("Preecho") || n.contains ("Pre-Echo");
    }
    void buildPage()
    {
        pageTargets.clear();
        const int algo = proc.algoOf (machine);
        for (int i = 0; i < l480::kMaxParams; ++i)
        {
            const auto& s = proc.spec (machine, i);
            if (! s.lab || s.alias >= 0 || i >= l480::algoDef (algo).pages * 6) continue;
            if (isDelaySpec (s, algo) != (page == 1)) continue;
            pageTargets.push_back ({ i, nullptr, plainName (s.name), s.unit == l480::uKey });
        }
        auto addP = [this] (const char* id, const char* nm) { pageTargets.push_back ({ -1, proc.apvts.getParameter (id), nm, false }); };
        if (page == 0) addP ("erAtk", "Reflection Attack");
        else
        {
            const bool sync = proc.apvts.getRawParameterValue ("dlSync")->load() > 0.5f;
            addP ("dlOn", "Delay On"); addP ("dlSync", "Tempo Sync"); addP (sync ? "dlDivL" : "dlTimeL", "Delay Left"); addP (sync ? "dlDivR" : "dlTimeR", "Delay Right");
            addP ("dlFb", "Feedback"); addP ("dlMix", "Delay Level"); addP ("dlAtk", "Delay Attack"); addP ("dlPing", "Ping-Pong"); addP ("dlLow", "Delay Bass"); addP ("dlHigh", "Delay Treble");
        }
        // the screen's number cells mirror the faders
        for (size_t i = 0; i < cells.size(); ++i)
        {
            auto& c = *cells[i];
            if (i >= pageTargets.size()) { c.name = nullptr; c.value = nullptr; continue; }
            c.name = [this, i] { return i < pageTargets.size() ? targetName (pageTargets[i]) : String(); };
            c.value = [this, i] { return i < pageTargets.size() ? targetText (pageTargets[i]) : String(); };
            c.getNorm = [this, i] { return i < pageTargets.size() ? targetNorm (pageTargets[i]) : 0.0f; };
            c.setNorm = [this, i] (float v) { if (i < pageTargets.size()) setTargetNorm (pageTargets[i], v); };
            c.gesture = [this, i] (bool b) { if (i < pageTargets.size()) targetGesture (pageTargets[i], b); };
            c.setText = [this, i] (const String& t) { if (i < pageTargets.size()) setTargetText (pageTargets[i], t); };
        }
    }
    String targetName (const Target& t) const { return t.name; }
    float targetNorm (const Target& t) const { if (t.param) return t.param->getValue(); const auto& s = proc.spec (machine, t.slot); return (s.unit == l480::uKey) ? 0.0f : proc.slotNorm (machine, t.slot); }
    String targetText (const Target& t) const
    {
        if (t.param) return t.param->getCurrentValueAsText();
        const auto& s = proc.spec (machine, t.slot);
        if (s.unit == l480::uKey) return "push up";
        return String (l480::formatValue (s, proc.slotValue (machine, t.slot)));
    }
    void targetGesture (const Target& t, bool begin) { if (t.param) { if (begin) t.param->beginChangeGesture(); else t.param->endChangeGesture(); } else if (! t.trigger) { if (begin) proc.beginSlot (machine, t.slot); else proc.endSlot (machine, t.slot); } }
    void setTargetNorm (const Target& t, float v) { if (t.param) t.param->setValueNotifyingHost (v); else if (! t.trigger) proc.setSlot (machine, t.slot, v); }
    void setTargetText (const Target& t, const String& text)
    {
        targetGesture (t, true);
        if (t.param) t.param->setValueNotifyingHost (t.param->getValueForText (text));
        else
        {
            const auto& s = proc.spec (machine, t.slot);
            float v = text.retainCharacters ("0123456789.-").getFloatValue();
            if (s.unit == l480::uChoice) { const int n = l480::numChoices (s); for (int k = 0; k < n; ++k) if (String (l480::choiceText (s, k)).equalsIgnoreCase (text.trim())) v = (float) k; }
            else if (s.unit == l480::uHz && text.containsIgnoreCase ("k")) v *= 1000.0f;
            else if (s.unit == l480::uSec && text.containsIgnoreCase ("ms")) v *= 0.001f;
            proc.setSlot (machine, t.slot, l480::toNorm (s, v));
        }
        targetGesture (t, false);
    }

    // ---------------------------------------------------------------- faders
    bool triggered[kMaxFaders] = {};
    void faderBegin (int i) { if (i < nShown) { triggered[i] = false; targetGesture (pageTargets[(size_t) i], true); } }
    void faderMove (int i, float t)
    {
        if (i >= nShown) return;
        const auto& tg = pageTargets[(size_t) i];
        if (tg.trigger) { if (t > 0.6f && ! triggered[i]) { triggered[i] = true; proc.pressKey (machine, tg.slot); } return; }
        setTargetNorm (tg, t); showMessage (tg.name, targetText (tg));
    }
    void faderEnd (int i) { if (i >= nShown) return; const auto& tg = pageTargets[(size_t) i]; targetGesture (tg, false); if (tg.trigger) faders[(size_t) i]->setTarget (0.0f); }
    void faderReset (int i)
    {
        if (i >= nShown) return;
        const auto& tg = pageTargets[(size_t) i];
        if (tg.param) { tg.param->beginChangeGesture(); tg.param->setValueNotifyingHost (tg.param->getDefaultValue()); tg.param->endChangeGesture(); return; }
        const auto& p = proc.prog (machine); const auto& s = proc.spec (machine, tg.slot);
        if (s.unit == l480::uKey) return;
        proc.beginSlot (machine, tg.slot); proc.setSlot (machine, tg.slot, l480::toNorm (s, l480::programValue (p, l480::realIndex (p.algo, tg.slot)))); proc.endSlot (machine, tg.slot);
    }
    void refreshPage (bool snap)
    {
        buildPage(); layoutScreen(); layoutFaders();
        pageKey->setText (page == 0 ? "REVERB" : "DELAY");
        for (int i = 0; i < nShown; ++i)
        {
            faders[(size_t) i]->enabled = true;
            faders[(size_t) i]->setTarget (pageTargets[(size_t) i].trigger ? 0.0f : targetNorm (pageTargets[(size_t) i]));
            if (snap) faders[(size_t) i]->snap();
        }
        const bool delayPage = page == 1;
        delayView.setVisible (delayPage); reverbView.setVisible (! delayPage); decay.setVisible (! delayPage);
        lastRv = -1; screen.repaint();
    }

    // ---------------------------------------------------------------- presets
    String programTitle (int m) const { if (proc.userPresetName.isNotEmpty()) return proc.userPresetName; return String (proc.prog (m).name) + (proc.isEdited (m) ? " *" : ""); }
    void openBrowser() { browser.refresh(); browser.setVisible (true); browser.toFront (true); }
    void stepPreset (int dir)
    {
        const auto user = proc.userPresets();
        const auto& order = browseOrder();
        const int nf = (int) order.size(), total = nf + user.size();
        int cur = proc.userPresetName.isNotEmpty() ? nf + user.indexOf (proc.userPresetName) : (int) (std::find (order.begin(), order.end(), proc.programOf (machine)) - order.begin());
        if (cur < 0 || cur >= total) cur = 0;
        const int next = ((cur + dir) % total + total) % total;
        if (next < nf) proc.loadProgram (machine, order[(size_t) next]); else proc.loadUserPreset (user[next - nf]);
        page = 0; refreshPage (true);
    }
    void savePreset()
    {
        auto* w = new AlertWindow ("Save preset", "Name for this preset (both machines, EQ, analog, compressor):", MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", proc.userPresetName.isNotEmpty() ? proc.userPresetName : String (proc.prog (machine).name));
        w->addButton ("Save", 1, KeyPress (KeyPress::returnKey)); w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        w->enterModalState (true, ModalCallbackFunction::create ([this, w] (int r)
        { if (r == 1) { const auto n = w->getTextEditorContents ("name").trim(); if (n.isNotEmpty() && proc.saveUserPreset (n)) showMessage ("Saved", n); } }), true);
    }
    void selectMachine (int m) { if (config() == 0) m = 0; if (m != machine) { machine = m; page = 0; refreshPage (true); } }

    // ---------------------------------------------------------------- the response picture
    void requestResponse()
    {
        l480::EngineParams p; const int pr = proc.programOf (machine);
        p.prog[0] = p.prog[1] = pr; p.config = l480::kSingle;
        for (int i = 0; i < l480::kMaxParams; ++i) { const auto& s = proc.spec (machine, i); p.val[0][i] = p.val[1][i] = s.lab ? proc.slotValue (machine, i) : 0.0f; }
        p.hw.on = false;
        p.curveOn[0] = proc.curveOn[machine];
        for (int b = 0; b < pv::DecayEq::N; ++b) p.curveMul[0][b] = proc.aCurveMul[machine][b].load();
        p.erAtkMs = proc.apvts.getRawParameterValue ("erAtk")->load();
        renderer.request (p, proc.sampleRateUi.load());
    }
    void liveBands()
    {
        // low / mid / high level of the output, from the processor's scope buffer
        const int pos = proc.scopeWrite.load (std::memory_order_acquire); const double fsr = proc.sampleRateUi.load();
        const float aL = 1.0f - std::exp (-2.0f * MathConstants<float>::pi * 250.0f / (float) fsr), aH = 1.0f - std::exp (-2.0f * MathConstants<float>::pi * 2500.0f / (float) fsr);
        float l1 = 0, h1 = 0; double e[3] = {};
        for (int i = 0; i < 1024; ++i)
        {
            const float x = proc.scope[(size_t) ((pos - 1024 + i) & (Flexiverb480Processor::kScopeSize - 1))];
            l1 += aL * (x - l1); h1 += aH * (x - h1);
            e[0] += l1 * l1; e[1] += (h1 - l1) * (h1 - l1); e[2] += (x - h1) * (x - h1);
        }
        for (int b = 0; b < 3; ++b) { const float v = (float) std::sqrt (e[b] / 1024.0) * 2.0f; reverbView.live[b] = v > reverbView.live[b] ? v : reverbView.live[b] * 0.85f; }
    }

    // ---------------------------------------------------------------- 30 Hz
    void timerCallback() override
    {
        if (config() != lastConfig) { lastConfig = config(); if (lastConfig == 0 && machine == 1) machine = 0; engineBox.setSelectedItemIndex (lastConfig == 0 ? 0 : 1, dontSendNotification); layoutDrawer(); refreshPage (true); drawer.repaint(); }
        if (proc.programLoaded.exchange (false) || proc.algoOf (machine) != lastAlgo) { lastAlgo = proc.algoOf (machine); refreshPage (false); }
        // preset bar
        {
            String t = programTitle (machine), sub = proc.userPresetName.isNotEmpty() ? String ("Your preset") : String (l480::bankName (proc.prog (machine).bank));
            if (config() != 0) t = String (machine == 0 ? "A  " : "B  ") + t;
            if (t != presetName.text || sub != presetName.sub) { presetName.text = t; presetName.sub = sub; presetName.repaint(); }
        }
        // ask for a new response picture when anything that shapes the reverb changed
        {
            int sig = proc.programOf (machine) * 7919 + machine * 31 + (proc.curveOn[machine] ? 1 : 0) + (int) (proc.apvts.getRawParameterValue ("erAtk")->load() * 10);
            for (int s = 0; s < l480::kMaxParams; ++s) sig = sig * 31 + (int) (proc.slotNorm (machine, s) * 1000.0f);
            for (int b = 0; b < pv::DecayEq::N; ++b) sig = sig * 31 + (int) (proc.aCurveMul[machine][b].load() * 1000.0f);
            if (sig != lastSig) { lastSig = sig; requestResponse(); }
            const int v = renderer.version.load(); if (v != lastRv) { lastRv = v; reverbView.setResult (renderer.get()); }
        }
        liveBands();
        if (reverbView.isVisible() && (reverbView.animate() || true)) reverbView.repaint();
        if (decay.isVisible() && sigChanged()) decay.repaint();
        if (delayView.isVisible()) delayView.repaint();
        if (Time::getMillisecondCounter() < msgUntil + 100) screen.repaint (screen.getLocalBounds().withHeight ((int) theme.displayPt + 14));
        placeCells();
        for (auto& c : cells) if (c->isVisible()) c->repaint();
        // faders follow their targets (and the screen's edits)
        bool labels = false;
        for (int i = 0; i < nShown; ++i)
        {
            const auto& tg = pageTargets[(size_t) i];
            if (! tg.trigger) faders[(size_t) i]->setTarget (targetNorm (tg));
            if (faders[(size_t) i]->animate()) { faders[(size_t) i]->repaint(); labels = true; }
        }
        if (labels || ++labelTick % 6 == 0) content.repaint (faderArea.withTrimmedTop (-(int) labelH));
        // keys light themselves while on
        const int algo = proc.algoOf (machine), bank = proc.prog (machine).bank;
        static const char* algName[] = { "HALL", "RANDOM HALL", "ROOM", "CHAMBER", "PLATE", "AMBIENCE", "INVERSE", "DELAY", "CHORUS", "PITCH" };
        const String nm (proc.prog (machine).name);
        for (size_t k = 0; k < algKeys.size(); ++k)
        {
            const String a (algName[k]); bool lit = false;
            if (a == "HALL") lit = (bank == 1 || bank == 18 || bank == 27 || bank == 33) && ! nm.containsIgnoreCase ("church") ;
            else if (a == "RANDOM HALL") lit = algo == l480::aRHall;
            else if (a == "ROOM") lit = (bank == 2 || bank == 28 || bank == 34) && ! nm.containsIgnoreCase ("chamber");
            else if (a == "CHAMBER") lit = nm.containsIgnoreCase ("chamber");
            else if (a == "PLATE") lit = bank == 4 || bank == 19 || bank == 30 || bank == 31 || bank == 36 || bank == 37;
            else if (a == "AMBIENCE") lit = algo == l480::aAmb || bank == 29 || bank == 35;
            else if (a == "INVERSE") lit = nm.containsIgnoreCase ("inside out");
            else if (a == "DELAY") lit = algo == l480::aTwin || algo == l480::aMBand;
            else if (a == "CHORUS") lit = algo == l480::aChorus || algo == l480::aPrime;
            else if (a == "PITCH") lit = algo == l480::aPitch || algo == l480::aDopp;
            algKeys[k]->setLit (lit);
        }
        pageKey->setLit (page == 1);
        for (size_t k = 0; k < myKeys.size() && k < myDefs.size(); ++k)
        {
            const auto& d = myDefs[k];
            myKeys[k]->setLit (d.user.isNotEmpty() ? proc.userPresetName == d.user : (proc.userPresetName.isEmpty() && proc.programOf (machine) == d.program));
        }
        machAKey->setLit (config() != 0 && machine == 0); machBKey->setLit (config() != 0 && machine == 1);
        muteKey->setLit (proc.mute.load()); setupKey->setLit (setupOpen); panelKey->setLit (drawerOpen);
        if (drawerOpen) vu.setGr (proc.compGr.load(), 1000.0f / 30.0f);
        if (drawerOpen)
        {
            for (auto* k : { ironKey.get(), anOnKey.get(), eqOnKey.get(), cpOnKey.get(), satKey.get(), duckKey.get() }) k->sync();
        }
    }
    bool sigChanged() { const int s = lastSig ^ (proc.curveOn[machine] ? 77 : 0); if (s != lastDecaySig) { lastDecaySig = s; return true; } return false; }
    int lastSig = 0, lastDecaySig = 0, labelTick = 0;

    // ---------------------------------------------------------------- members
    Flexiverb480Processor& proc;
    Theme theme;
    FlexLnf lnf;
    float scale = 0.8f;
    Content content; Screen screen; PresetName presetName;
    DecayGraph decay; DelayGraph delayView; ReverbView reverbView; ResponseRenderer renderer;
    std::vector<std::unique_ptr<NumberCell>> cells;
    std::unique_ptr<LarcKey> prevKey, nextKey, saveKey, panelKey, pageKey, machAKey, machBKey, muteKey, setupKey;
    std::vector<std::unique_ptr<LarcKey>> algKeys;
    // ---- your own buttons: each loads a preset; made from SETUP or by right-clicking a button
    struct MyDef { String label; int program = 0; String user; };
    std::vector<MyDef> myDefs; std::vector<std::unique_ptr<LarcKey>> myKeys;
    void loadMyKeys()
    {
        myDefs.clear();
        StringArray lines; lines.addLines (proc.getUiProperty ("myKeys", String()).toString());
        for (auto& l : lines) { StringArray t; t.addTokens (l, "\t", ""); if (t.size() >= 2 && t[0].isNotEmpty()) myDefs.push_back ({ t[0], t[1].getIntValue(), t[2] }); }
        rebuildMyKeys();
    }
    void saveMyKeys()
    {
        StringArray lines; for (auto& d : myDefs) lines.add (d.label + "\t" + String (d.program) + "\t" + d.user);
        proc.setUiProperty ("myKeys", lines.joinIntoString ("\n"));
    }
    void rebuildMyKeys()
    {
        for (auto& k : myKeys) content.removeChildComponent (k.get());
        myKeys.clear();
        for (size_t i = 0; i < myDefs.size(); ++i)
        {
            auto k = std::make_unique<LarcKey> (theme, myDefs[i].label.toUpperCase()); k->kc = &Theme::keyMine; content.addAndMakeVisible (*k);
            k->onClick = [this, i] { if (i >= myDefs.size()) return; const auto d = myDefs[i];
                                     if (d.user.isNotEmpty()) proc.loadUserPreset (d.user); else proc.loadProgram (machine, d.program); page = 0; refreshPage (true); };
            k->onRightClick = [this, i] { keyMenu ((int) i); };
            myKeys.push_back (std::move (k));
        }
    }
    void addMyKey()
    {
        MyDef d; d.program = proc.programOf (machine); d.user = proc.userPresetName;
        d.label = d.user.isNotEmpty() ? d.user : String (proc.prog (machine).name);
        myDefs.push_back (d); saveMyKeys(); rebuildMyKeys(); layout(); content.repaint();
    }
    void keyMenu (int mine)
    {
        PopupMenu m;
        const String cur = proc.userPresetName.isNotEmpty() ? proc.userPresetName : String (proc.prog (machine).name);
        m.addItem (1, "Make a button for the current preset  (" + cur + ")");
        if (mine >= 0) { m.addSeparator(); m.addItem (2, "Rename this button..."); m.addItem (3, "Remove this button"); }
        m.showMenuAsync (PopupMenu::Options(), [this, mine] (int r)
        {
            if (r == 1) addMyKey();
            else if (r == 3 && mine >= 0 && mine < (int) myDefs.size()) { myDefs.erase (myDefs.begin() + mine); saveMyKeys(); rebuildMyKeys(); layout(); content.repaint(); }
            else if (r == 2 && mine >= 0 && mine < (int) myDefs.size())
            {
                auto w = std::make_shared<AlertWindow> ("Button name", "", MessageBoxIconType::NoIcon);
                w->addTextEditor ("n", myDefs[(size_t) mine].label); w->addButton ("OK", 1, KeyPress (KeyPress::returnKey)); w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
                w->enterModalState (true, ModalCallbackFunction::create ([this, w, mine] (int res)
                {
                    if (res == 1 && mine < (int) myDefs.size()) { const auto n = w->getTextEditorContents ("n").trim(); if (n.isNotEmpty()) { myDefs[(size_t) mine].label = n; saveMyKeys(); rebuildMyKeys(); layout(); content.repaint(); } }
                }), false);
            }
        });
    }
    std::vector<std::unique_ptr<LarcFader>> faders;
    Viewport drawerView; Drawer drawer;
    ComboBox engineBox; int engineLabelY = 0, engineLabelX = 18;
    VuMeter vu { theme };
    std::unique_ptr<ParamKey> ironKey, anOnKey, eqOnKey, cpOnKey, satKey, duckKey;
    std::unique_ptr<ScaleKnob> sysKnob[4], anKnob[5], eqKnob2[2], cpKnob[7];
    std::unique_ptr<DualKnob> eqKnob[4];
    PresetBrowser browser; SetupPanel setup;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Flexiverb480Editor)
};
