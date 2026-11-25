// Flexiverb 480 editor (included by PluginProcessor.cpp).
//
//  Preset bar   the preset menu (click: every preset at once, by category), < > beside it, SAVE.
//  Screen       the early reflections and the low / mid / high decay waterfall of the program as it is set now
//               (rendered in the background from the engine itself), the decay-time curve, and beside them the delay
//               taps. Numbers: right-click and drag up/down, or click and type. Graph handles drag too.
//  Keys         the 480L's algorithms: HALL, RANDOM HALL, ROOM ... Right-click DELAY or CHORUS adds it to the reverb
//               (machine B: beside, before or after it) instead of replacing it.
//  Remote       the LARC: main display with the L/R meters, REG / PROG, keypad, BANK VAR STO CTRL MACH MUTE PAGE 2nd F,
//               the slider display line, six faders and the six slider display keys - one 480L page at a time.
//  Drawer       engine (single / dual), mute, SETUP, output, iron, ANALOG, EQ (with analog models), COMPRESSOR.
//  SETUP        opens beside the plugin: colours, gradient, 3D depth, text sizes, oversampling, 64-bit.
#include "FlexFonts.h"
#include <set>
#include <map>

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
           lowCol { 0xffff3b24 }, midCol { 0xffffb22e }, highCol { 0xff5cc8ff },
           larc { 0xff2c2d30 }, larcRim { 0xff121213 }, larcText { 0xffff3a1e }, larcLegend { 0xffe9e5da };
    float gradient = 0.45f, keyPt = 12.5f, knobPt = 17.0f, displayPt = 15.0f, faderPt = 13.5f, keyHeight = 0.0f, keyPad = 0.0f, knobSize = 1.0f, setupPt = 13.0f, menuPt = 17.0f;
    float stripKnob = 1.0f, stripPt = 17.0f, stripNumPt = 10.0f, gatePt = 17.0f, analogSize = 1.0f, hoverZoom = 20.0f, keyDepth = 0.0f, keyRound = 0.28f;   // keyDepth 0 = flat keys, 1 = full 3D; keyRound: corner radius / height   // channel strip: knob size, names, numbers (at a 56 px knob), gate meter text   // key size: text + extra
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
            { "Channel strip panel", &Theme::stripPanel }, { "VU meter face", &Theme::vuFace },
            { "Remote face", &Theme::larc }, { "Remote rim", &Theme::larcRim }, { "Remote display text", &Theme::larcText }, { "Remote lettering", &Theme::larcLegend } };
        return v;
    }
    String toString() const
    {
        XmlElement x ("LOOK");
        int i = 0; for (auto& it : items()) { x.setAttribute ("c" + String (i), (this->*it.c).toString());
                                               x.setAttribute ("t" + String (i), gTop[i].toString()); x.setAttribute ("b" + String (i), gBot[i].toString()); x.setAttribute ("o" + String (i), gOwn[i]); ++i; }
        x.setAttribute ("grad", gradient); x.setAttribute ("keyPt", keyPt); x.setAttribute ("knobPt17", knobPt); x.setAttribute ("dispPt", displayPt);
        x.setAttribute ("faderPt", faderPt); x.setAttribute ("depth", depth); x.setAttribute ("keyH", keyHeight); x.setAttribute ("keyPad", keyPad); x.setAttribute ("knobSize", knobSize); x.setAttribute ("setupPt", setupPt); x.setAttribute ("menuPt17", menuPt); x.setAttribute ("stripKnob", stripKnob); x.setAttribute ("stripPt", stripPt); x.setAttribute ("stripNumPt", stripNumPt); x.setAttribute ("gatePt", gatePt); x.setAttribute ("analogSize", analogSize); x.setAttribute ("hoverZoom", hoverZoom); x.setAttribute ("keyDepth", keyDepth); x.setAttribute ("keyRound", keyRound); x.setAttribute ("v", 7);
        for (auto& t : texts) { auto* e = x.createNewChildElement ("T"); e->setAttribute ("id", t.first); e->setAttribute ("pt", t.second.pt); e->setAttribute ("col", t.second.col.toString()); e->setAttribute ("hc", t.second.hasCol); }
        return x.toString (XmlElement::TextFormat().singleLine().withoutHeader());
    }
    bool fromString (const String& s)
    {
        auto x = parseXML (s); if (x == nullptr || ! x->hasTagName ("LOOK") || x->getIntAttribute ("v") < 7) return false;   // older looks: start fresh
        int i = 0; for (auto& it : items()) { const auto a = x->getStringAttribute ("c" + String (i)); if (a.isNotEmpty()) this->*it.c = Colour::fromString (a);
                                               if (x->hasAttribute ("t" + String (i))) { gTop[i] = Colour::fromString (x->getStringAttribute ("t" + String (i))); gBot[i] = Colour::fromString (x->getStringAttribute ("b" + String (i))); gOwn[i] = x->getBoolAttribute ("o" + String (i)); }
                                               ++i; }
        gradient = (float) x->getDoubleAttribute ("grad", gradient); keyPt = (float) x->getDoubleAttribute ("keyPt", keyPt);
        knobPt = (float) x->getDoubleAttribute ("knobPt17", knobPt); displayPt = (float) x->getDoubleAttribute ("dispPt", displayPt);
        faderPt = (float) x->getDoubleAttribute ("faderPt", faderPt); depth = jlimit (0, 3, x->getIntAttribute ("depth", depth));
        keyHeight = (float) x->getDoubleAttribute ("keyH", keyHeight); keyPad = (float) x->getDoubleAttribute ("keyPad", keyPad);
        knobSize = (float) x->getDoubleAttribute ("knobSize", knobSize); setupPt = (float) x->getDoubleAttribute ("setupPt", setupPt); menuPt = (float) x->getDoubleAttribute ("menuPt17", menuPt); stripKnob = (float) x->getDoubleAttribute ("stripKnob", stripKnob); stripPt = (float) x->getDoubleAttribute ("stripPt", stripPt); stripNumPt = (float) x->getDoubleAttribute ("stripNumPt", stripNumPt); gatePt = (float) x->getDoubleAttribute ("gatePt", gatePt); analogSize = (float) x->getDoubleAttribute ("analogSize", analogSize); hoverZoom = (float) x->getDoubleAttribute ("hoverZoom", hoverZoom); keyDepth = (float) x->getDoubleAttribute ("keyDepth", keyDepth); keyRound = (float) x->getDoubleAttribute ("keyRound", keyRound);   // new key: the old 13 is not carried over
        texts.clear();
        for (auto* e : x->getChildWithTagNameIterator ("T"))
        { TextStyle t; t.pt = (float) e->getDoubleAttribute ("pt"); t.col = Colour::fromString (e->getStringAttribute ("col")); t.hasCol = e->getBoolAttribute ("hc"); texts[e->getStringAttribute ("id")] = t; }
        return true;
    }
    // your own size / colour for a single text (by its id); 0 / no colour = the usual one
    struct TextStyle { float pt = 0; Colour col; bool hasCol = false; };
    std::map<String, TextStyle> texts;
    float tpt (const String& id, float def) const { auto it = texts.find (id); return it != texts.end() && it->second.pt > 0 ? it->second.pt : def; }
    Colour tcol (const String& id, Colour def) const { auto it = texts.find (id); return it != texts.end() && it->second.hasCol ? it->second.col : def; }
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

using TextHook = std::function<void (const String& id, const String& label, float defPt, Colour defCol, Component* at, Rectangle<int> area)>;
static TextHook& textHook() { static TextHook h; return h; }
static void editText (const String& id, const String& label, float defPt, Colour defCol, Component* at, Rectangle<int> area)
{ if (textHook()) textHook() (id, label, defPt, defCol, at, area); }
static void drawLegend (Graphics& g, const String& s, Rectangle<float> r, const Font& fIn, Colour c, Justification j = Justification::centred)
{
    Font f (fIn);
    { const float w = GlyphArrangement::getStringWidth (f, s); if (w > r.getWidth() && w > 0) f = f.withHeight (f.getHeight() * jmax (0.6f, r.getWidth() / w)); }
    g.setFont (f);
    g.setColour (Colours::black.withAlpha (0.3f)); g.drawText (s, r.translated (0, 1.0f), j, false);
    g.setColour (c); g.drawText (s, r, j, false);
}

// ============================================================================ a key: no lamp - the cap lights itself while on or pressed
// Ctrl+Shift (Cmd+Shift on a Mac) + drag moves any button, knob, word or meter; drag its lower right corner to resize it.
// Ctrl+Shift + double-click puts it back. Controls ignore the mouse while those keys are held.
inline bool arrangeMods (const MouseEvent& e) { return e.mods.isShiftDown() && (e.mods.isCtrlDown() || e.mods.isCommandDown()); }

class LarcKey : public Component, private Timer
{
public:
    LarcKey (const Theme& t, const String& s) : th (t), text (s) { setRepaintsOnMouseActivity (true); }
    Colour Theme::* kc = &Theme::key;            // which object's colours this key uses
    std::function<void()> onClick;
    void setLit (bool b) { if (b != lit) { lit = b; repaint(); } }
    void setText (const String& s) { if (s != text) { text = s; repaint(); } }
    const String& getText() const { return text; }
    String textId() const { return "button:" + text; }
    float textPt() const { return th.tpt (textId(), th.keyPt); }
    bool slim = false;                                   // channel strip keys: little room around the text
    bool textOnly = false;                               // no key at all: the word is the switch, lit yellow when on
    float preferredWidth() const { return textW (ui (textPt(), W::Bold, 0.06f), text) + (textOnly ? 4.0f : slim ? textPt() * 0.7f + 4.0f : textPt() * 1.6f + 8.0f + th.keyPad); }
    std::function<void()> onRightClick, onPress, onRelease;
    void paint (Graphics& g) override
    {
        if (textOnly)
        {
            const bool on = lit || pressed;
            const auto f = ui (textPt(), W::Bold, 0.06f);
            auto r = getLocalBounds().toFloat();
            if (on)
            {   // a tight, sharp yellow glow right around the lit word
                g.setFont (f);
                g.setColour (th.keyLight.withAlpha (0.07f));
                for (float dx = -2.0f; dx <= 2.0f; dx += 1.0f) for (float dy = -2.0f; dy <= 2.0f; dy += 1.0f) if (std::abs (dx) + std::abs (dy) >= 2.5f) g.drawText (text, r.translated (dx, dy), Justification::centred, false);
                g.setColour (th.keyLight.withAlpha (0.28f));
                for (auto d : { Point<float> (-1, 0), Point<float> (1, 0), Point<float> (0, -1), Point<float> (0, 1) }) g.drawText (text, r.translated (d.x, d.y), Justification::centred, false);
            }
            g.setFont (f);
            g.setColour (on ? th.keyLight : (isMouseOver() ? th.panelText : th.panelText.withAlpha (0.6f)));
            g.drawText (text, r, Justification::centred, false);
            return;
        }
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const float rad = jmax (1.0f, getHeight() * jlimit (0.0f, 0.5f, th.keyRound));    // corner radius (SETUP)
        const bool on = lit || pressed || flash;
        if (th.keyDepth < 0.05f)
        {   // completely flat: one colour, a thin edge, the word; lit = the key light colour
            const Colour base = on ? th.keyLight : (th.*kc);
            g.setColour (isMouseOver() && ! on ? base.brighter (0.08f) : base); g.fillRoundedRectangle (r, rad);
            g.setColour (Colours::black.withAlpha (0.45f)); g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
            const bool dark = base.getPerceivedBrightness() < 0.45f;
            g.setFont (ui (jmin (textPt(), r.getHeight() * 0.6f), W::Bold, 0.06f));
            g.setColour (th.tcol (textId(), dark && th.keyText.getPerceivedBrightness() < 0.45f ? Colours::white : (on ? Colours::black : th.keyText)));
            g.drawText (text, r.translated (0, pressed ? 1.0f : 0.0f), Justification::centred, false);
            return;
        }
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
        const Colour ink = th.tcol (textId(), darkCap && th.keyText.getPerceivedBrightness() < 0.45f ? Colours::white : th.keyText);
        g.setFont (ui (jmin (textPt(), face.getHeight() * 0.62f), W::Bold, 0.06f));
        g.setColour (darkCap ? Colours::black.withAlpha (0.5f) : Colours::white.withAlpha (0.4f)); g.drawText (text, face.translated (0, darkCap ? -0.8f : 0.8f), Justification::centred, false);
        g.setColour (ink); g.drawText (text, face, Justification::centred, false);
    }
    void mouseDown (const MouseEvent& e) override { if (arrangeMods (e)) return; pressed = true; repaint(); if (! e.mods.isPopupMenu() && onPress) onPress(); }
    void mouseUp (const MouseEvent& e) override
    {
        if (arrangeMods (e)) return;
        pressed = false; flash = true; startTimer (130); repaint();
        if (! e.mods.isPopupMenu() && onRelease) onRelease();
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); else editText (textId(), "Button " + text, th.keyPt, th.keyText, this, getLocalBounds()); return; }
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
        if (arrangeMods (e)) return;
        if (! enabled) return;
        dragging = true; startShown = shown; startY = (float) e.position.y;
        if (onBegin) onBegin();
        const float bot = getHeight() - 12.0f, top = 12.0f, capY = bot - (bot - top) * shown;
        if (std::abs (e.position.y - capY) > 14) { shown = jlimit (0.0f, 1.0f, (bot - e.position.y) / (bot - top)); startShown = shown; target = shown; if (onMove) onMove (shown); repaint(); }
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (arrangeMods (e)) return;
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
    if (body.getSaturation() < 0.3f)
    {
        // black knobs: flat, no exaggerated light and shadow
        g.setColour (Colours::black.withAlpha (0.25f)); g.fillEllipse (k.translated (0.6f, 1.0f));
        if (skirt)
        {
            g.setColour (body.brighter (0.12f)); g.fillEllipse (k);
            g.setColour (Colours::black.withAlpha (0.12f));
            for (int i = 0; i < 36; ++i) { const float a = i * MathConstants<float>::twoPi / 36.0f; g.drawLine (Line<float> (k.getCentre().getPointOnCircumference (d * 0.43f, a), k.getCentre().getPointOnCircumference (d * 0.5f, a)), 0.6f); }
            k = k.reduced (d * 0.1f);
        }
        g.setColour (body); g.fillEllipse (k);
        g.setColour (Colours::black.withAlpha (0.35f)); g.drawEllipse (k, 0.8f);
        g.setColour (pointer);
        const auto c = k.getCentre(); const float r = k.getWidth() * 0.5f;
        g.drawLine (Line<float> (c.getPointOnCircumference (r * 0.25f, angle), c.getPointOnCircumference (r * 0.95f, angle)), 2.2f);
        return;
    }
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
    bool strip = false;                                  // in the channel strip: its own sizes, numbers tight around the knob
    const float* sizeMul = nullptr;                      // the ANALOG knobs: their own size (SETUP)
    float namePt() const { return strip ? th.stripPt : th.knobPt; }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float lp = th.tpt ("knob:" + name, namePt());
        const auto lf = ui (lp, W::Bold, 0.06f);
        // a name too wide for the cell goes on two lines (split at the middle space) instead of shrinking into its neighbours
        String l1 = name, l2;
        if (GlyphArrangement::getStringWidth (lf, name) > (r.getWidth() - 2.0f) * 1.2f && name.containsChar (' '))
        {
            int best = -1, bd = 1 << 30;
            for (int i = 0; i < name.length(); ++i) if (name[i] == ' ' && std::abs (i * 2 - name.length()) < bd) { bd = std::abs (i * 2 - name.length()); best = i; }
            l1 = name.substring (0, best); l2 = name.substring (best + 1);
        }
        auto lab = r.removeFromBottom ((lp + 3) * (l2.isEmpty() ? 1.0f : 2.0f) + 2);
        float np = th.tpt ("knob numbers:" + name, jmax (7.5f, th.knobPt * 0.6f)), d, rad; Point<float> c;
        if (strip)
        {
            // as big as the cell allows with the numbers close around it; the numbers grow and shrink with the knob
            const float base = th.tpt ("knob numbers:" + name, th.stripNumPt);
            // word labels (TUBE, OPTO ...) need more room at the sides than numbers; the biggest old knob was 56 px, a little more is fine
            int longest = 0; for (auto& m : marks) longest = jmax (longest, (int) std::strlen (m.text));
            const float side = longest > 3 ? 4.6f : 3.6f;
            auto fit = [&r, side] (float n) { return jmin (60.0f, jmin (r.getWidth() - 8.0f - n * side, (r.getHeight() - 5.0f - n * 1.94f) / 0.855f)); };
            np = base; float d0 = fit (np);
            for (int it = 0; it < 4; ++it) { np = base * jlimit (0.7f, 1.6f, d0 / 56.0f); d0 = fit (np); }
            d = jmax (16.0f, d0 * jlimit (0.5f, 2.0f, th.stripKnob)); np = base * jlimit (0.7f, 1.6f, d / 56.0f);
            rad = d * 0.5f + 4.0f + np * 0.95f;
            const float top = rad + np * 0.5f, bot = jmax (d * 0.5f, rad * 0.71f + np * 0.5f);
            c = { r.getCentreX(), r.getY() + jmax (0.0f, (r.getHeight() - top - bot) * 0.5f) + top };
        }
        else
        {
            d = jmax (16.0f, jmin (r.getWidth() - np * 3.4f - 6.0f, r.getHeight() - np * 2.6f - 4.0f) * jlimit (0.5f, 2.0f, sizeMul ? *sizeMul : th.knobSize));
            c = r.getCentre().translated (0, 3); rad = d * 0.5f + 11.0f;
        }
        drawScale (g, param, c, rad, marks, th.tcol ("knob numbers:" + name, th.panelText.withAlpha (0.85f)), np);
        drawKnobBody (g, th, Rectangle<float> (d, d).withCentre (c), knobAngle (param->getValue()), capColour, th.panelText, true);
        if (l2.isEmpty()) drawLegend (g, name, lab, lf, th.tcol ("knob:" + name, th.panelText));
        else
        {
            auto top = lab.removeFromTop (lab.getHeight() * 0.5f);
            drawLegend (g, l1, top, lf, th.tcol ("knob:" + name, th.panelText), Justification::centredBottom);
            drawLegend (g, l2, lab, lf, th.tcol ("knob:" + name, th.panelText), Justification::centredTop);
        }
    }
    void mouseDown (const MouseEvent& e) override
    {
        if (arrangeMods (e)) return;
        styling = e.mods.isPopupMenu();
        if (styling)
        {
            if (e.y > getHeight() - namePt() - 6) editText ("knob:" + name, "Knob name " + name, namePt(), th.panelText, this, getLocalBounds());
            else editText ("knob numbers:" + name, "Numbers around " + name, strip ? th.stripNumPt : jmax (7.5f, th.knobPt * 0.6f), th.panelText, this, getLocalBounds());
            return;
        }
        start = param->getValue(); param->beginChangeGesture(); show();
    }
    bool styling = false;
    void mouseDrag (const MouseEvent& e) override
    {
        if (arrangeMods (e)) return;
        if (styling) return;
        const float fine = e.mods.isShiftDown() || e.mods.isAltDown() ? 0.1f : 1.0f;
        param->setValueNotifyingHost (snapped (param, jlimit (0.0f, 1.0f, start - (float) e.getDistanceFromDragStartY() * 0.005f * fine), step)); show();
    }
    void mouseUp (const MouseEvent&) override { if (styling) { styling = false; return; } param->endChangeGesture(); }
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
    // any two controls nested (the compressor's Threshold + Ratio, Attack + Release): outer and inner marks, their names
    DualKnob (const Theme& t, AudioProcessorValueTreeState& s, const String& outerId, const String& innerId, const String& title,
              std::vector<ScaleMark> outerMarks, std::vector<ScaleMark> innerMarks, const String& outerName, const String& innerName, Colour Theme::* outerCol)
        : DualKnob (t, s, outerId, innerId, title, std::move (outerMarks))
    { eq = false; im = std::move (innerMarks); oName = outerName; iName = innerName; oCol = outerCol; }
    std::function<void (const String&, const String&)> onShow;
    // channel strip sizes: the name, the frequency numbers (they grow with the knob) and the red gain numbers inside them
    float namePt() const { return th.tpt ("knob:" + name, th.stripPt); }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto lab = r.removeFromBottom (namePt() + 5);
        geometry (r);
        static const std::vector<ScaleMark> gmEq = numberScale (-12, 12, 4, 2, true);
        const auto& gm = eq ? gmEq : im;
        const Colour red (0xffff6a4a);
        g.setColour (red.withAlpha (0.6f));
        if (eq)
            for (float db = -14; db <= 14; db += 2)
            {
                const float a = knobAngle (gP->convertTo0to1 (db));
                g.drawLine (Line<float> (c.getPointOnCircumference (rOuter + 1.5f, a), c.getPointOnCircumference (rOuter + (std::fmod (db, 4.0f) == 0 ? 4.5f : 3.2f), a)), 1.0f);
            }
        else
            for (auto& m : im) { const float a = knobAngle (gP->convertTo0to1 (m.value)); g.drawLine (Line<float> (c.getPointOnCircumference (rOuter + 1.5f, a), c.getPointOnCircumference (rOuter + 4.5f, a)), 1.0f); }
        drawScale (g, gP, c, rOuter + 5.0f + gn * 0.6f, gm, red, gn);
        drawScale (g, fP, c, rOuter + rNum, fm, th.tcol ("knob numbers:" + name, th.panelText.withAlpha (0.9f)), np);
        drawKnobBody (g, th, Rectangle<float> (rOuter * 2, rOuter * 2).withCentre (c), knobAngle (fP->getValue()), oCol, th.panelText, true);
        drawKnobBody (g, th, Rectangle<float> (rInner * 2, rInner * 2).withCentre (c), knobAngle (gP->getValue()), &Theme::eqInner, Colours::white, false);
        drawLegend (g, name, lab, ui (namePt(), W::Bold, 0.06f), th.tcol ("knob:" + name, th.panelText));
    }
    bool hitTest (int x, int y) override { geometry (bodyArea()); return Point<float> ((float) x, (float) y).getDistanceFrom (c) < rOuter + 6.0f || y > getHeight() - namePt() - 5; }
    void mouseDown (const MouseEvent& e) override
    {
        if (arrangeMods (e)) return;
        if (e.mods.isPopupMenu())
        {
            active = nullptr;
            if (e.y > getHeight() - namePt() - 6) editText ("knob:" + name, "Knob name " + name, th.stripPt, th.panelText, this, getLocalBounds());
            else editText ("knob numbers:" + name, "Numbers around " + name, th.stripNumPt, th.panelText, this, getLocalBounds());
            return;
        }
        geometry (bodyArea()); active = pick (e); start = active->getValue(); active->beginChangeGesture(); show();
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (arrangeMods (e)) return;
        if (active == nullptr) return;
        const float fine = e.mods.isShiftDown() || e.mods.isAltDown() ? 0.1f : 1.0f;
        active->setValueNotifyingHost (snapped (active, jlimit (0.0f, 1.0f, start - (float) e.getDistanceFromDragStartY() * 0.005f * fine), (eq && active == gP) ? 2.0f : 0.0f)); show();
    }
    void mouseUp (const MouseEvent&) override { if (active) active->endChangeGesture(); }
    void mouseDoubleClick (const MouseEvent& e) override { if (arrangeMods (e)) return; geometry (bodyArea()); auto* p = pick (e); p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); active = p; show(); }
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        geometry (bodyArea()); auto* p = pick (e); p->beginChangeGesture();
        if (eq && p == gP) { const float v = jlimit (-15.0f, 15.0f, p->convertFrom0to1 (p->getValue()) + (w.deltaY > 0 ? 2.0f : -2.0f)); p->setValueNotifyingHost (snapped (p, p->convertTo0to1 (v), 2.0f)); }
        else p->setValueNotifyingHost (jlimit (0.0f, 1.0f, p->getValue() + w.deltaY * 0.03f));
        p->endChangeGesture(); active = p; show();
    }
private:
    Rectangle<float> bodyArea() const { return getLocalBounds().toFloat().withTrimmedBottom (namePt() + 5); }
    RangedAudioParameter* pick (const MouseEvent& e) const { return e.position.getDistanceFrom (c) <= rInner + 1.0f ? gP : fP; }
    void geometry (Rectangle<float> r)
    {
        // as big as the cell allows; the numbers sit close and grow with the knob (base size = at a 56 px knob)
        const float base = th.tpt ("knob numbers:" + name, th.stripNumPt);
        auto fit = [&] (float n)
        {
            const float g2 = n * 0.7f, off = 5.0f + g2 * 1.2f + n * 0.95f;       // red gain ring, then the frequency numbers
            return jmin (r.getWidth() * 0.5f - off - n * 0.9f - 1.0f, (r.getHeight() - 2.0f - n - 1.71f * off) / 1.71f);
        };
        np = base; float ro = jmin (52.0f, fit (np));
        for (int it = 0; it < 4; ++it) { np = base * jlimit (0.7f, 1.6f, ro * 2.0f / 56.0f); ro = jmin (52.0f, fit (np)); }
        rOuter = jmax (10.0f, ro * jlimit (0.5f, 2.0f, th.stripKnob)); np = base * jlimit (0.7f, 1.6f, rOuter * 2.0f / 56.0f);
        gn = np * 0.7f; rNum = 5.0f + gn * 1.2f + np * 0.95f; rInner = rOuter * 0.62f;
        const float top = rOuter + rNum + np * 0.5f, bot = jmax (rOuter, (rOuter + rNum) * 0.71f + np * 0.5f);
        c = { r.getCentreX(), r.getY() + jmax (0.0f, (r.getHeight() - top - bot) * 0.5f) + top };
    }
    void show() { if (onShow && active) onShow (eq ? name + (active == gP ? " gain" : " frequency") : (active == gP ? iName : oName), active->getCurrentValueAsText()); }
    const Theme& th; RangedAudioParameter *fP, *gP, *active = nullptr; String name; std::vector<ScaleMark> fm;
    bool eq = true; std::vector<ScaleMark> im; String oName, iName; Colour Theme::* oCol = &Theme::eqKnob;
    Point<float> c; float rOuter = 20, rInner = 11, start = 0, np = 10, gn = 7, rNum = 20;
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
        const int m = mach();
        const Colour col = th.displayText;
        if (! proc.hasDecayCurve (m)) return;
        rtMain = jmax (0.05f, proc.mainRt (m));
        yLo = jmax (0.02f, rtMain / 10.0f); yHi = rtMain * 6.0f;
        g.setFont (ui (th.sz (8.5f), W::Medium));
        // fine grid: a faint line at every 1..9 of each decade, labels where they have room
        for (float dec : { 10.0f, 100.0f, 1000.0f, 10000.0f })
            for (int k = 1; k <= 9; ++k) { const float f = dec * (float) k; if (f < 20.0f || f > 20000.0f) continue; g.setColour (col.withAlpha (0.05f)); g.drawVerticalLine ((int) xOf (f), plot.getY(), plot.getBottom()); }
        for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 3000.0f, 4000.0f, 5000.0f, 7000.0f, 10000.0f, 15000.0f })
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
        const bool on = proc.curveOn[m];
        {   // the program's own decay: dashed
            Path pp;
            for (int i = 0; i <= 120; ++i) { const float f = 20.0f * std::pow (1000.0f, i / 120.0f); const Point<float> pt (xOf (f), yOf (rtMain * proc.programDecayMul (m, f))); if (i == 0) pp.startNewSubPath (pt); else pp.lineTo (pt); }
            Path dashed; const float dl[] = { 4.0f, 3.0f }; PathStrokeType (1.0f).createDashedStroke (dashed, pp, dl, 2);
            g.setColour (col.withAlpha (on ? 0.35f : 0.8f)); g.fillPath (dashed);
        }
        const auto& pts = proc.curvePts[m];
        if (on)
        {   // yours: solid, with each point and its width (Q) under it
            Path pc;
            for (int i = 0; i <= 200; ++i) { const float f = 20.0f * std::pow (1000.0f, i / 200.0f); const Point<float> pt (xOf (f), yOf (rtMain * proc.totalDecayMul (m, f))); if (i == 0) pc.startNewSubPath (pt); else pc.lineTo (pt); }
            g.setColour (col.withAlpha (0.25f)); g.strokePath (pc, PathStrokeType (4.0f));
            g.setColour (col); g.strokePath (pc, PathStrokeType (1.8f));
            for (size_t i = 0; i < pts.size(); ++i)
            {
                if (pts[i].q <= 0.001f) continue;
                const Point<float> pt = pointPos (m, pts[i]);
                const float half = 0.6f / pts[i].q;                  // the bell's width, in octaves each side
                const float x0 = xOf (jmax (20.0f, pts[i].f * std::pow (2.0f, -half))), x1 = xOf (jmin (20000.0f, pts[i].f * std::pow (2.0f, half)));
                g.setColour (col.withAlpha ((int) i == drag || (int) i == hover ? 0.7f : 0.35f));
                g.drawLine (x0, pt.y + 10, x1, pt.y + 10, 1.5f); g.drawLine (x0, pt.y + 7, x0, pt.y + 13, 1.0f); g.drawLine (x1, pt.y + 7, x1, pt.y + 13, 1.0f);
                g.setColour (Colours::black); g.fillEllipse (Rectangle<float> (11, 11).withCentre (pt));
                g.setColour ((int) i == drag || (int) i == hover ? Colours::white : col); g.drawEllipse (Rectangle<float> (9, 9).withCentre (pt), 2.0f);
                if ((int) i == drag || (int) i == hover)
                {   // the exact place of this point: its frequency, the decay time there, and its width
                    const float f = pts[i].f, t = rtMain * proc.totalDecayMul (m, f);
                    const String txt = (f >= 1000.0f ? String (f / 1000.0f, f < 10000.0f ? 2 : 1) + " kHz" : String (roundToInt (f)) + " Hz")
                                       + "   " + String (t, t < 10.0f ? 2 : 1) + " s   Q " + String (pts[i].q, pts[i].q < 1.0f ? 2 : 1);
                    const auto fnt = ui (th.sz (11.0f), W::Bold); const float tw = textW (fnt, txt) + 12.0f, tht = th.sz (11.0f) + 8.0f;
                    auto box = Rectangle<float> (tw, tht).withCentre ({ pt.x, pt.y - 14.0f - tht * 0.5f });
                    box = box.withX (jlimit (plot.getX(), plot.getRight() - tw, box.getX())).withY (jmax (plot.getY(), box.getY()));
                    if (box.expanded (6.0f).contains (pt))      // no room above: beside the point instead
                        box = box.withY (jlimit (plot.getY(), plot.getBottom() - tht, pt.y - tht * 0.5f))
                                 .withX (pt.x + 14.0f + tw < plot.getRight() ? pt.x + 14.0f : pt.x - 14.0f - tw);
                    g.setColour (Colours::black.withAlpha (0.85f)); g.fillRoundedRectangle (box, 3.0f);
                    g.setColour (col.withAlpha (0.8f)); g.drawRoundedRectangle (box, 3.0f, 1.0f);
                    g.setColour (Colours::white); g.setFont (fnt); g.drawText (txt, box, Justification::centred, false);
                }
            }
        }
        {   // the big point: lengthens or shortens everything together
            const int gi = globalIndex (pts);
            const float gm = (on && gi >= 0) ? pts[(size_t) gi].m : 1.0f;
            master = { plot.getRight() + th.sz (16.0f), yOf (rtMain * gm) };
            g.setColour (col.withAlpha (0.25f)); g.drawLine (plot.getRight() + 2, master.y, master.x, master.y, 1.0f);
            g.setColour (Colours::black); g.fillEllipse (Rectangle<float> (20, 20).withCentre (master));
            g.setColour (masterDrag || masterHover ? Colours::white : col); g.drawEllipse (Rectangle<float> (17, 17).withCentre (master), 2.6f);
            g.fillEllipse (Rectangle<float> (6, 6).withCentre (master));
        }
        g.setColour (col.withAlpha (0.6f)); g.setFont (ui (th.sz (9.0f), W::Bold, 0.05f));
        g.drawFittedText ("DECAY TIME BY FREQUENCY:  double-click adds or removes one point, drag it, right-click sets its Q (sharp or broad)",
                          (getLocalBounds().toFloat().reduced (th.sz (36.0f), 0).withHeight (th.sz (12.0f) + 2)).toNearestInt(), Justification::topLeft, 1, 0.7f);
    }
    void mouseMove (const MouseEvent& e) override { const int h = hit (e.position); const bool mh = e.position.getDistanceFrom (master) < 12.0f; if (h != hover || mh != masterHover) { hover = h; masterHover = mh; repaint(); } }
    void mouseExit (const MouseEvent&) override { hover = -1; repaint(); }
    void testHover (int i) { hover = i; repaint(); }       // for screenshots
    void mouseDown (const MouseEvent& e) override
    {
        if (arrangeMods (e)) return;
        const int m = mach();
        if (! proc.hasDecayCurve (m)) return;
        if (e.mods.isPopupMenu()) { const int h = hit (e.position); if (h >= 0) qMenu (h); return; }
        if (e.position.getDistanceFrom (master) < 12.0f)
        {
            masterPts = proc.curveOn[m] ? proc.curvePts[m] : std::vector<Flexiverb480Processor::CurvePt>();
            if (globalIndex (masterPts) < 0) masterPts.push_back ({ 1000.0f, 1.0f, 0.0f });
            masterStart = masterPts[(size_t) globalIndex (masterPts)].m;
            masterDrag = true; masterY = e.position.y; proc.setCurve (m, masterPts, true); repaint(); return;
        }
        drag = hit (e.position);
        if (drag >= 0) show();
        repaint();
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (arrangeMods (e)) return;
        const int m = mach();
        if (masterDrag)
        {
            const float k = tOf (e.position.y) / tOf (masterY);
            auto pts = masterPts; const int gi = globalIndex (pts);
            pts[(size_t) gi].m = jlimit (0.05f, 20.0f, masterStart * k);
            proc.setCurve (m, pts, true);
            if (onShow) onShow ("All of the decay", "x " + String (pts[(size_t) gi].m, 2));
            repaint(); return;
        }
        if (drag < 0 || drag >= (int) proc.curvePts[m].size()) return;
        auto pts = proc.curvePts[m];
        auto& p = pts[(size_t) drag];
        p.f = fOf (e.position.x);
        p.m = 1.0f; const float without = proc.programDecayMul (m, p.f) * Flexiverb480Processor::curveEval (pts, p.f);   // the curve here without this point
        p.m = jlimit (0.05f, 20.0f, tOf (jlimit (plot.getY(), plot.getBottom(), e.position.y)) / (rtMain * without));
        proc.setCurve (m, pts, true);
        drag = indexOf (pts, p.f);
        show(); repaint();
    }
    void mouseUp (const MouseEvent&) override { drag = -1; masterDrag = false; repaint(); }
    void mouseDoubleClick (const MouseEvent& e) override
    {
        if (arrangeMods (e)) return;
        const int m = mach();
        if (! proc.hasDecayCurve (m) || e.position.getDistanceFrom (master) < 12.0f) return;
        auto pts = proc.curveOn[m] ? proc.curvePts[m] : std::vector<Flexiverb480Processor::CurvePt>();
        const int h = hit (e.position);
        if (h >= 0) { pts.erase (pts.begin() + h); proc.setCurve (m, pts, ! pts.empty()); drag = hover = -1; repaint(); return; }
        if (! plot.expanded (4).contains (e.position)) return;
        // one new point, exactly where you clicked; everything else stays as it was
        Flexiverb480Processor::CurvePt p { fOf (e.position.x), 1.0f, 1.0f };
        const float here = proc.programDecayMul (m, p.f) * Flexiverb480Processor::curveEval (pts, p.f);
        p.m = jlimit (0.05f, 20.0f, tOf (e.position.y) / (rtMain * here));
        pts.push_back (p);
        proc.setCurve (m, pts, true);
        hover = indexOf (proc.curvePts[m], p.f); repaint();
    }
private:
    int mach() const { return machine ? machine() : 0; }
    static int globalIndex (const std::vector<Flexiverb480Processor::CurvePt>& pts) { for (size_t i = 0; i < pts.size(); ++i) if (pts[i].q <= 0.001f) return (int) i; return -1; }
    static int indexOf (const std::vector<Flexiverb480Processor::CurvePt>& pts, float f) { for (size_t i = 0; i < pts.size(); ++i) if (pts[i].q > 0.001f && pts[i].f == f) return (int) i; return -1; }
    Point<float> pointPos (int m, const Flexiverb480Processor::CurvePt& p) const { return { xOf (p.f), yOf (rtMain * proc.totalDecayMul (m, p.f)) }; }
    void qMenu (int h)
    {
        const int m = mach(); const float q = proc.curvePts[m][(size_t) h].q;
        PopupMenu menu; menu.addSectionHeader ("Width of this point (Q)");
        const float qs[] = { 4.0f, 2.0f, 1.0f, 0.5f, 0.25f };
        const char* nm[] = { "Very sharp", "Sharp", "Medium", "Broad", "Very broad" };
        for (int i = 0; i < 5; ++i) menu.addItem (i + 1, String (nm[i]) + "   (Q " + String (qs[i], qs[i] < 1 ? 2 : 0) + ")", true, std::abs (q - qs[i]) < 0.01f);
        menu.addSeparator(); menu.addItem (10, "Remove this point");
        menu.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [this, m, h, qs] (int r)
        {
            auto pts = proc.curvePts[m]; if (h >= (int) pts.size()) return;
            if (r >= 1 && r <= 5) pts[(size_t) h].q = qs[r - 1];
            else if (r == 10) pts.erase (pts.begin() + h);
            else return;
            proc.setCurve (m, pts, ! pts.empty()); repaint();
        });
    }
    float xOf (float f) const { return plot.getX() + plot.getWidth() * std::log (f / 20.0f) / std::log (1000.0f); }
    float fOf (float x) const { return jlimit (20.0f, 20000.0f, 20.0f * std::pow (1000.0f, (x - plot.getX()) / plot.getWidth())); }
    float yOf (float t) const { return plot.getBottom() - plot.getHeight() * std::log (jlimit (yLo, yHi, t) / yLo) / std::log (yHi / yLo); }
    float tOf (float y) const { return yLo * std::pow (yHi / yLo, (plot.getBottom() - y) / plot.getHeight()); }
    int hit (Point<float> p) const
    {
        const int m = mach(); if (! proc.curveOn[m]) return -1;
        const auto& pts = proc.curvePts[m];
        for (size_t i = 0; i < pts.size(); ++i) if (pts[i].q > 0.001f && p.getDistanceFrom (pointPos (m, pts[i])) < 8.0f) return (int) i;
        return -1;
    }
    void show()
    {
        const int m = mach(); const auto& pts = proc.curvePts[m];
        if (onShow && drag >= 0 && drag < (int) pts.size())
        {
            const float f = pts[(size_t) drag].f, t = rtMain * proc.totalDecayMul (m, f);
            onShow ("Decay at " + (f >= 1000 ? String (f / 1000, 2) + " kHz" : String ((int) f) + " Hz"), String (t, t < 10 ? 2 : 1) + " s   Q " + String (pts[(size_t) drag].q, 2));
        }
    }
    const Theme& th; Flexiverb480Processor& proc; Rectangle<float> plot; float rtMain = 1, yLo = 0.1f, yHi = 10; int drag = -1, hover = -1;
    Point<float> master { -100, -100 }; bool masterDrag = false, masterHover = false; float masterY = 0, masterStart = 1; std::vector<Flexiverb480Processor::CurvePt> masterPts;
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
        g.drawFittedText ("DELAY TAPS:  drag a point - sideways = time, up/down = level", (getLocalBounds().toFloat().reduced (th.sz (36.0f), 0).withHeight (th.sz (12.0f) + 2)).toNearestInt(), Justification::topLeft, 1, 0.8f);
    }
    void mouseMove (const MouseEvent& e) override { collect(); const int h = hit (e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const MouseEvent& e) override
    {
        if (arrangeMods (e)) return;
        collect(); drag = hit (e.position); if (drag < 0) return;
        dragTMax = tMax; begin (taps[(size_t) drag]); show();
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (arrangeMods (e)) return;
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
public:
    bool busy() const { return drag >= 0; }
private:
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
// The Flexiverb presets (each one fitted to its 480L impulse response) first, then Surround (HD Card), then all the 480L
// factory programs. Banks 33-37 (an earlier fit) stay in the table so old sessions still load, but are not offered.
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
        auto banks = [&o] (int b0, int b1) { for (int b = b0; b <= b1; ++b) for (int i = 0; i < l480::numPrograms(); ++i) if (l480::programs()[(size_t) i].bank == b) o.push_back (i); };
        banks (27, 31); banks (22, 26); banks (1, 21);
        return o;
    }();
    return order;
}

// ============================================================================ the reverb EQ (the channel strip's EQ) as a graph
// up to four points, one per band; double-click anywhere adds one, double-click a point removes it, drag to move it
class ReverbEqGraph : public Component
{
public:
    ReverbEqGraph (const Theme& t, Flexiverb480Processor& p) : th (t), proc (p)
    { for (int b = 0; b < 4; ++b) { fP[b] = p.apvts.getParameter ("eqF" + String (b)); gP[b] = p.apvts.getParameter ("eqG" + String (b)); } model = p.apvts.getParameter ("eqModel"); }
    std::function<void (const String&, const String&)> onShow;
    void paint (Graphics& g) override
    {
        plot = getLocalBounds().toFloat().reduced (th.sz (40.0f), 6).withTrimmedRight (th.sz (30.0f)).withTrimmedBottom (th.sz (11.0f)).withTrimmedTop (th.sz (14.0f));
        const Colour col = th.displayText;
        g.setFont (ui (th.sz (8.5f), W::Bold));
        g.setColour (col.withAlpha (0.8f));
        g.drawText ("REVERB EQ:  double-click adds a point (up to 4), double-click a point removes it, drag to move it", getLocalBounds().toFloat().withHeight (th.sz (13.0f)).withTrimmedLeft (th.sz (40.0f)), Justification::centredLeft, false);
        g.setFont (ui (th.sz (8.5f), W::Medium));
        for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            const float x = xOf (f); g.setColour (col.withAlpha (0.10f)); g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (col.withAlpha (0.55f)); g.drawText (f >= 1000 ? String (f / 1000, 0) + "k" : String ((int) f), Rectangle<float> (x - 18, plot.getBottom() + 1, 36, th.sz (12.0f)), Justification::centred, false);
        }
        for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
        {
            const float y = yOf (db); g.setColour (col.withAlpha (db == 0 ? 0.25f : 0.10f)); g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            g.setColour (col.withAlpha (0.55f)); g.drawText ((db > 0 ? "+" : "") + String ((int) db) + " dB", Rectangle<float> (plot.getX() - th.sz (40.0f), y - 6, th.sz (37.0f), 12), Justification::centredRight, false);
        }
        const bool on = proc.apvts.getRawParameterValue ("eqOn")->load() > 0.5f;
        Path pc;
        for (int i = 0; i <= 200; ++i) { const float f = 20.0f * std::pow (1000.0f, i / 200.0f); const Point<float> pt (xOf (f), yOf (magDb (f))); if (i == 0) pc.startNewSubPath (pt); else pc.lineTo (pt); }
        g.setColour (col.withAlpha (on ? 0.25f : 0.1f)); g.strokePath (pc, PathStrokeType (4.0f));
        g.setColour (col.withAlpha (on ? 1.0f : 0.45f)); g.strokePath (pc, PathStrokeType (1.8f));
        for (int b = 0; b < 4; ++b)
        {
            if (! shown (b)) continue;
            const auto pt = pointOf (b);
            g.setColour (Colours::black); g.fillEllipse (Rectangle<float> (11, 11).withCentre (pt));
            g.setColour (b == drag || b == hover ? Colours::white : col); g.drawEllipse (Rectangle<float> (9, 9).withCentre (pt), 2.0f);
        }
    }
    void mouseMove (const MouseEvent& e) override { const int h = hit (e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const MouseEvent& e) override { if (arrangeMods (e)) return; drag = hit (e.position); if (drag >= 0) { fP[drag]->beginChangeGesture(); gP[drag]->beginChangeGesture(); } }
    void mouseDrag (const MouseEvent& e) override
    {
        if (arrangeMods (e)) return;
        if (drag < 0) return;
        set (drag, fOf (e.position.x), dbOf (e.position.y));
    }
    void mouseUp (const MouseEvent&) override { if (drag >= 0) { fP[drag]->endChangeGesture(); gP[drag]->endChangeGesture(); } drag = -1; }
    void mouseDoubleClick (const MouseEvent& e) override
    {
        if (arrangeMods (e)) return;
        const int h = hit (e.position);
        if (h >= 0) { setShown (h, false); gP[h]->beginChangeGesture(); gP[h]->setValueNotifyingHost (gP[h]->convertTo0to1 (0.0f)); gP[h]->endChangeGesture(); repaint(); return; }
        // the free band nearest in frequency takes the new point
        const float f = fOf (e.position.x); int best = -1; float bd = 1e9f;
        for (int b = 0; b < 4; ++b) if (! shown (b)) { const float d = std::abs (std::log2 (f / jmax (20.0f, value (fP[b])))); if (d < bd) { bd = d; best = b; } }
        if (best < 0) { if (onShow) onShow ("Reverb EQ", "all four points are in use"); return; }
        setShown (best, true);
        fP[best]->beginChangeGesture(); gP[best]->beginChangeGesture(); set (best, f, dbOf (e.position.y)); fP[best]->endChangeGesture(); gP[best]->endChangeGesture();
    }
private:
    const Theme& th; Flexiverb480Processor& proc; RangedAudioParameter* fP[4] {}; RangedAudioParameter* gP[4] {}; RangedAudioParameter* model = nullptr;
    Rectangle<float> plot; int drag = -1, hover = -1;
    static float value (RangedAudioParameter* p) { return p->convertFrom0to1 (p->getValue()); }
    bool shown (int b) const { const int m = (int) proc.getUiProperty ("eqPoints", -1); return m < 0 ? std::abs (value (gP[b])) > 0.05f : ((m >> b) & 1) != 0; }
    void setShown (int b, bool v)
    {
        int m = (int) proc.getUiProperty ("eqPoints", -1);
        if (m < 0) { m = 0; for (int k = 0; k < 4; ++k) if (std::abs (value (gP[k])) > 0.05f) m |= 1 << k; }
        m = v ? (m | (1 << b)) : (m & ~(1 << b)); proc.setUiProperty ("eqPoints", m);
    }
    void set (int b, float f, float db)
    {
        fP[b]->setValueNotifyingHost (fP[b]->convertTo0to1 (fP[b]->getNormalisableRange().snapToLegalValue (f)));
        gP[b]->setValueNotifyingHost (gP[b]->convertTo0to1 (jlimit (-15.0f, 15.0f, db)));
        if (onShow) onShow ("Reverb EQ " + String (b + 1), (f >= 1000 ? String (f / 1000, 2) + " kHz" : String (roundToInt (f)) + " Hz") + "   " + String (db, 1) + " dB");
        repaint();
    }
    float magDb (float f) const
    {
        const int mdl = model ? (int) model->convertFrom0to1 (model->getValue()) : 0; double m = 0;
        for (int b = 0; b < 4; ++b)
        {
            cs::BandSettings bs; bs.on = true; bs.shelf = (b == 0 || b == 3); bs.freq = value (fP[b]); bs.gain = value (gP[b]); bs.qScale = 1.0f;
            if (std::abs (bs.gain) > 0.01f) m += cs::EqBank::bandMagDb (mdl, bs, b, f, 96000.0);
        }
        return (float) m;
    }
    Point<float> pointOf (int b) const { return { xOf (value (fP[b])), yOf (value (gP[b])) }; }
    int hit (Point<float> p) const { for (int b = 0; b < 4; ++b) if (shown (b) && pointOf (b).getDistanceFrom (p) < 9.0f) return b; return -1; }
    float xOf (float f) const { return plot.getX() + plot.getWidth() * std::log (jmax (20.0f, f) / 20.0f) / std::log (1000.0f); }
    float fOf (float x) const { return 20.0f * std::pow (1000.0f, jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth())); }
    float yOf (float db) const { return plot.getCentreY() - plot.getHeight() * 0.5f * jlimit (-1.1f, 1.1f, db / 16.0f); }
    float dbOf (float y) const { return jlimit (-15.0f, 15.0f, (plot.getCentreY() - y) / (plot.getHeight() * 0.5f) * 16.0f); }
};

// ============================================================================ preset browser: every preset at once, by category
class PresetBrowser : public Component
{
public:
    PresetBrowser (const Theme& t, Flexiverb480Processor& p) : th (t), proc (p) { addAndMakeVisible (view); view.setViewedComponent (&list, false); view.setScrollBarsShown (true, false); }
    std::function<void (int program)> onFactory; std::function<void (const String&)> onUser; std::function<void()> onClose;
    std::function<int()> machine;
    std::vector<std::pair<String, int>> algorithms;       // HALL, RANDOM HALL ...: each loads its default program
    void refresh() { list.build (*this); resized(); repaint(); }
    void resized() override { auto r = getLocalBounds().reduced (16); r.removeFromTop (44); view.setBounds (r); list.setSize (r.getWidth() - 12, list.heightFor (r.getWidth() - 12)); list.layout(); }
    void paint (Graphics& g) override
    {
        g.fillAll (Colours::black.withAlpha (0.82f));
        auto r = getLocalBounds().reduced (16).removeFromTop (30).toFloat();
        const float mp = th.tpt ("preset menu", th.menuPt);
        drawLegend (g, "PRESETS  -  machine " + String (machine && machine() == 1 ? "B" : "A"), r, ui (mp, W::Bold, 0.1f), th.panelText, Justification::centredLeft);
        drawLegend (g, "click a preset to load it", r.withTrimmedRight (60), ui (mp * 0.8f, W::Medium), th.panelText.withAlpha (0.6f), Justification::centredRight);
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
                    for (int i = 0; i < l480::numPrograms(); ++i) if (l480::programs()[(size_t) i].bank == bank) bx.items.push_back ({ l480::programs()[(size_t) i].name, i, {}, {} });
                    if (! bx.items.empty()) s.boxes.push_back (bx);
                }
                sections.push_back (s);
            };
            if (! b.algorithms.empty())
            {
                Section a; a.title = "Algorithms  (each loads its default program)"; Box bx; bx.title = "Algorithms";
                for (auto& al : b.algorithms) bx.items.push_back ({ al.first.substring (0, 1) + al.first.substring (1).toLowerCase(), al.second, {}, {} });
                a.boxes.push_back (bx); sections.push_back (a);
            }
            sec ("Flexiverb  (each fitted to its Flexi 180V impulse response)", 27, 31); sec ("Surround (HD Card)", 22, 26);
            sec ("Flexi 180V  (factory programs)", 1, 21);
            Section u; u.title = "Your presets  (" + Flexiverb480Processor::presetFolder().getFullPathName() + ")";
            Box ub; ub.title = "Saved with SAVE";
            for (auto& n : b.proc.userPresets()) ub.items.push_back ({ n, -1, n, {} });
            if (ub.items.empty()) ub.items.push_back ({ "(none yet)", -2, {}, {} });
            u.boxes.push_back (ub); sections.push_back (u);
        }
        float mp() const { return owner ? owner->th.tpt ("preset menu", owner->th.menuPt) : 17.0f; }
        int kItemH() const { return roundToInt (mp() * 1.45f + 2); }
        int kTitleH() const { return roundToInt (mp() * 1.6f + 4); }
        int kSecH() const { return roundToInt (mp() * 2.1f + 4); }
        int kBoxW() const { return roundToInt (mp() * 13.5f); }
        int cols (int w) const { return jmax (1, w / kBoxW()); }
        int heightFor (int w)
        {
            int y = 0; const int nc = cols (w);
            for (auto& s : sections)
            {
                y += kSecH();
                for (size_t i = 0; i < s.boxes.size(); i += (size_t) nc)
                {
                    int h = 0; for (size_t k = i; k < jmin (s.boxes.size(), i + (size_t) nc); ++k) h = jmax (h, kTitleH() + (int) s.boxes[k].items.size() * kItemH() + 8);
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
                s.y = y; y += kSecH();
                for (size_t i = 0; i < s.boxes.size(); i += (size_t) nc)
                {
                    int h = 0;
                    for (size_t k = i; k < jmin (s.boxes.size(), i + (size_t) nc); ++k)
                    {
                        auto& bx = s.boxes[k]; const int x = (int) (k - i) * bw;
                        const int bh = kTitleH() + (int) bx.items.size() * kItemH() + 8; h = jmax (h, bh);
                        bx.r = { x, y, bw - 8, bh };
                        for (size_t e = 0; e < bx.items.size(); ++e) bx.items[e].r = { x + 6, y + kTitleH() + (int) e * kItemH(), bw - 20, kItemH() };
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
                drawLegend (g, s.title, Rectangle<float> (0, (float) s.y + 4, (float) getWidth(), mp() + 8), ui (mp() + 1, W::Bold, 0.08f), th.keyLight, Justification::centredLeft);
                for (auto& bx : s.boxes)
                {
                    g.setColour (th.panel.withAlpha (0.9f)); g.fillRoundedRectangle (bx.r.toFloat(), 5);
                    g.setColour (th.panelText.withAlpha (0.2f)); g.drawRoundedRectangle (bx.r.toFloat(), 5, 1.0f);
                    drawLegend (g, bx.title, bx.r.toFloat().withHeight ((float) kTitleH()).reduced (6, 0), ui (mp() - 1, W::Bold, 0.06f), th.panelText.withAlpha (0.75f), Justification::centredLeft);
                    for (auto& e : bx.items)
                    {
                        const bool isCur = (e.program >= 0 && e.program == cur) || (e.user.isNotEmpty() && e.user == owner->proc.userPresetName);
                        if (isCur) { g.setColour (th.keyLight.withAlpha (0.85f)); g.fillRoundedRectangle (e.r.toFloat(), 3); }
                        else if (e.r.contains (hoverPt)) { g.setColour (th.panelText.withAlpha (0.12f)); g.fillRoundedRectangle (e.r.toFloat(), 3); }
                        g.setColour (isCur ? Colours::black : th.panelText.withAlpha (e.program == -2 ? 0.4f : 0.95f)); g.setFont (ui (mp(), W::Medium));
                        g.drawText (e.name, e.r.reduced (4, 0), Justification::centredLeft, true);
                    }
                }
            }
        }
        Point<int> hoverPt;
        void mouseMove (const MouseEvent& e) override { hoverPt = e.getPosition(); repaint(); }
        void mouseDown (const MouseEvent& e) override
        {
            if (arrangeMods (e)) return;
            if (e.mods.isPopupMenu()) { editText ("preset menu", "Preset menu text", owner->th.menuPt, owner->th.panelText, this, Rectangle<int> (e.x - 40, e.y - 10, 80, 20)); return; }
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
    static constexpr int kWfBands = 64;
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
            std::vector<double> L ((size_t) len, 0.0), R ((size_t) len, 0.0), a (256, 0.0), b (256, 0.0);
            for (int k = 0; k < 4; ++k) { std::fill (a.begin(), a.end(), 0.0); std::fill (b.begin(), b.end(), 0.0); eng.process (a.data(), b.data(), 256, p); }
            L[0] = R[0] = 0.25f;
            for (int pos = 0; pos < len && ! threadShouldExit(); pos += 256) eng.process (L.data() + pos, R.data() + pos, std::min (256, len - pos), p);
            Result r;
            // early reflections: peak level in 0.5 ms bins over 150 ms, dB re the loudest point of the whole response
            float mx = 1e-9f; for (int i = 0; i < len; ++i) mx = std::max (mx, (float) (0.5 * (std::abs (L[(size_t) i]) + std::abs (R[(size_t) i]))));
            const int eb = std::max (1, (int) (0.0005 * rate));
            for (int k = 0; k < 600; ++k) { float m = 0; for (int i = k * eb; i < (k + 1) * eb && i < len; ++i) m = std::max (m, (float) (0.5 * (std::abs (L[(size_t) i]) + std::abs (R[(size_t) i])))); r.er.push_back (Decibels::gainToDecibels (m / mx, -80.0f)); }
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
            // waterfall: 64 bands, 40 Hz - 16 kHz (about a seventh of an octave apart), energy in 10 ms frames, dB re the loudest
            {
                std::vector<double> m ((size_t) (nf * fr)); for (int i = 0; i < nf * fr; ++i) m[(size_t) i] = 0.5 * (L[(size_t) i] + R[(size_t) i]);
                std::vector<std::vector<double>> w (kWfBands); double wtop = 1e-30;
                for (int bnd = 0; bnd < kWfBands && ! threadShouldExit(); ++bnd)
                {
                    const double fc = 40.0 * std::pow (400.0, bnd / (double) (kWfBands - 1));
                    auto& v = w[(size_t) bnd]; v.assign ((size_t) nf, 0.0);
                    if (fc > 0.45 * rate) continue;
                    Bq q[2];
                    for (auto& qq : q) { const double om = 2 * MathConstants<double>::pi * fc / rate, al = std::sin (om) / (2 * 9.5), a0 = 1 + al;
                        qq.b0 = al / a0; qq.b1 = 0; qq.b2 = -al / a0; qq.a1 = -2 * std::cos (om) / a0; qq.a2 = (1 - al) / a0; }
                    for (int i = 0; i < nf * fr; ++i) { const double y = q[1].p (q[0].p (m[(size_t) i])); v[(size_t) (i / fr)] += y * y; }
                    for (double x : v) wtop = std::max (wtop, x);
                }
                // a light 1-2-1 smoothing across neighbouring bands: fine detail without single-band spikes
                for (int bnd = 0; bnd < kWfBands; ++bnd)
                {
                    const auto& c = w[(size_t) bnd]; const auto& lo = w[(size_t) jmax (0, bnd - 1)]; const auto& hi = w[(size_t) jmin (kWfBands - 1, bnd + 1)];
                    for (size_t i = 0; i < c.size(); ++i) r.wf[bnd].push_back ((float) std::max (-80.0, 10 * std::log10 ((0.25 * lo[i] + 0.5 * c[i] + 0.25 * hi[i]) / wtop + 1e-30)));
                }
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
struct Target { int slot = -1; RangedAudioParameter* param = nullptr; String name; bool trigger = false; String group; int row = 0; String lab; };   // row 0 reverb, 1 delay

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
    // ---- early reflections you can edit: the algorithm's own taps (real) and your own ones
    struct Bar { float ms, db; bool own; int idx; bool off; };
    std::function<std::vector<Bar>()> getBars;
    std::function<void (float ms, float db)> onAdd;
    std::function<void (const Bar&)> onToggle;                         // double-click: yours go, the algorithm's go off / on
    std::function<void (const std::vector<Bar>&)> onDragBegin; std::function<void (float dMs, float dDb)> onDrag; std::function<void()> onDragEnd;
    float erRange = 150.0f;                                              // ms shown in the early-reflection picture
    // what this view shows: 0 the reflections and the tail on one time line (in proportion), 1 the reflections, 2 the tail
    int mode = 0;
    // turning the picture: the tail (and with LINK the reflections) turn with the mouse, and the sound follows
    float yaw = 0, pitch = 0, erYaw = 0, erPitch = 0; bool linkOn = true; int turnWhat = 0;   // 0 both, 1 reflections, 2 tail
    std::function<void (int what, float dx, float dy, bool twist, int phase)> onTurn;     // phase 0 begin, 1 move, 2 end
    std::function<void()> onTurnReset;
    std::function<void (int mode)> onOpenWindow;
    std::vector<std::pair<bool, int>> sel;                               // selected bars: (yours?, number)
    bool isSel (const Bar& b) const { for (auto& q : sel) if (q.first == b.own && q.second == b.idx) return true; return false; }
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
        const float lh = th.sz (12.0f);
        auto head = r.removeFromTop (lh + 4);
        // the reflections take their real share of the time line: the first ~erRange ms of the whole decay
        const float share = mode == 1 ? 1.0f : mode == 2 ? 0.0f : jlimit (0.07f, 0.2f, erRange / (erRange + tMax() * 1000.0f));
        auto erA = r.removeFromLeft (r.getWidth() * share);
        if (mode == 0) r.removeFromLeft (6);
        auto meters = r.removeFromRight (0);
        erArea = mode == 2 ? Rectangle<float>() : erA.withTrimmedBottom (lh + 2).withTrimmedLeft (mode == 1 ? lh * 2.2f : 4.0f);
        decArea = mode == 1 ? Rectangle<float>() : r.withTrimmedBottom (lh + 2).withTrimmedLeft (lh * 2.4f).withTrimmedRight (lh * 3.2f).withTrimmedTop (4);
        g.setFont (ui (th.sz (9.0f), W::Bold, 0.05f)); g.setColour (col.withAlpha (0.7f));
        g.drawText (mode == 1 ? "drag a reflection to move it, double-click adds or removes one" : mode == 2 ? "drag to turn, shift-drag to twist, double-click turns back"
                              : "EARLY REFLECTIONS + TAIL ON ONE TIME LINE    (option-click either to open it big)", head, Justification::left, false);
        // LINK / ER / TAIL: what turns
        {
            const auto f = ui (th.sz (9.5f), W::Bold, 0.05f); g.setFont (f);
            float x = head.getRight();
            const char* nm[3] = { "TAIL", "ER", "LINK" };
            for (int i = 0; i < 3; ++i)
            {
                const float w = textW (f, nm[i]) + 4; x -= w; turnKeys[i] = { x, head.getY(), w, head.getHeight() }; x -= 12;
                const bool on = i == 2 ? linkOn : (! linkOn && turnWhat == (i == 0 ? 2 : 1));
                g.setColour (on ? th.keyLight : col.withAlpha (0.45f)); g.drawText (nm[i], turnKeys[i], Justification::centred, false);
            }
        }
        if (! erArea.isEmpty()) grid (g, erArea, erRange, erRange > 200 ? 100.0f : 50.0f, mode == 1 ? "ms" : "");
        if (shown.er.empty()) return;
        // early reflections as stems
        for (size_t k = 0; k < shown.er.size(); ++k)
        {
            const float db = shown.er[k]; if (db < -60) continue;
            const float prev = k > 1 ? std::max (shown.er[k - 1], shown.er[k - 2]) : -99.0f, next = k + 2 < shown.er.size() ? std::max (shown.er[k + 1], shown.er[k + 2]) : -99.0f;
            if (db < prev || db < next) continue;                     // a distinct reflection: louder than its neighbours
            const float ms = (float) k * 0.5f; if (ms > erRange) break;
            const float x = erX (ms), y = yOf (erArea, db);
            g.setColour (col.withAlpha (0.3f)); g.drawLine (x, erArea.getBottom(), x, y, 1.0f);   // what comes out (a picture)
        }
        const float pd = getPredelayMs ? getPredelayMs() : -1.0f;
        if (pd >= 0)
        {
            const float x = erX (pd);
            Path pth; pth.startNewSubPath (x, erArea.getY()); pth.lineTo (x, erArea.getBottom());
            Path dsh; const float dl[] = { 3.0f, 3.0f }; PathStrokeType (1.2f).createDashedStroke (dsh, pth, dl, 2);
            g.setColour (Colours::white.withAlpha (dragging == 1 ? 0.9f : 0.55f)); g.fillPath (dsh);
            pdAnchor = { x + 3, erArea.getY() };
        }
        else pdAnchor = { -1, -1 };
        // the reflections you can edit, like velocities: a stem with a head; yours in the button-light colour
        if (getBars)
        {
            for (auto& b : getBars())
            {
                if (b.ms > erRange) continue;
                const float x = erX (b.ms), y = b.off ? erArea.getBottom() - 3 : yOf (erArea, b.db);
                const bool s = isSel (b); const Colour c = b.own ? th.keyLight : col;
                g.setColour (c.withAlpha (b.off ? 0.35f : 1.0f)); g.drawLine (x, erArea.getBottom(), x, y, s ? 2.2f : 1.6f);
                const auto head = Rectangle<float> (8, 5).withCentre ({ x, y });
                g.setColour (s ? Colours::white : c.withAlpha (b.off ? 0.35f : 1.0f)); g.fillRect (head);
                if (b.off) { g.setColour (c.withAlpha (0.6f)); g.drawLine (x - 3, y - 6, x + 3, y - 12, 1.0f); g.drawLine (x - 3, y - 12, x + 3, y - 6, 1.0f); }
            }
            if (! band.isEmpty()) { g.setColour (Colours::white.withAlpha (0.12f)); g.fillRect (band); g.setColour (Colours::white.withAlpha (0.5f)); g.drawRect (band, 1.0f); }
        }
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
        ignoreUnused (bc, meters);
    }
    void mouseDown (const MouseEvent& e) override
    {
        if (arrangeMods (e)) return;
        dragging = 0; band = {};
        if (e.mods.isPopupMenu()) return;
        for (int i = 0; i < 3; ++i)
            if (turnKeys[i].contains (e.position))
            {
                if (i == 2) linkOn = ! linkOn; else { linkOn = false; turnWhat = i == 0 ? 2 : 1; }
                if (linkOn) turnWhat = 0;
                if (onShow) onShow ("Turning", linkOn ? "reflections and tail together" : turnWhat == 1 ? "the reflections only" : "the tail only");
                repaint(); return;
            }
        if (e.mods.isAltDown() && onOpenWindow && mode == 0)
        {
            if (erArea.expanded (4).contains (e.position)) { onOpenWindow (1); return; }
            if (decArea.contains (e.position)) { onOpenWindow (2); return; }
        }
        const float pd = getPredelayMs ? getPredelayMs() : -1.0f;
        const auto hb = hitBar (e.position);
        const bool more = e.mods.isShiftDown() || e.mods.isCommandDown();
        if (hb.idx >= 0)
        {
            grabKeyboardFocus();
            if (more) { if (isSel (hb)) sel.erase (std::remove (sel.begin(), sel.end(), std::make_pair (hb.own, hb.idx)), sel.end()); else sel.push_back ({ hb.own, hb.idx }); }
            else if (! isSel (hb)) sel = { { hb.own, hb.idx } };
            dragging = 3; startPos = e.position;
            if (onDragBegin && getBars) { std::vector<Bar> v; for (auto& b : getBars()) if (isSel (b)) v.push_back (b); onDragBegin (v); }
            repaint(); return;
        }
        if (pd >= 0 && erArea.expanded (4).contains (e.position) && std::abs (e.position.x - erX (pd)) < 6) dragging = 1;
        else if (e.position.getDistanceFrom (handle) < 10 && getRt) { dragging = 2; startRt = getRt(); startX = e.position.x; }
        else if (getBars && erArea.expanded (4).contains (e.position)) { grabKeyboardFocus(); if (! more) sel.clear(); dragging = 4; startPos = e.position; repaint(); return; }
        else if (decArea.contains (e.position))
        {   // turn the picture (and the sound with it)
            dragging = 5; startPos = e.position; twisting = e.mods.isShiftDown();
            const int what = linkOn ? 0 : turnWhat; startAngles[0] = yaw; startAngles[1] = pitch; startAngles[2] = erYaw; startAngles[3] = erPitch;
            if (onTurn) onTurn (what, 0, 0, twisting, 0);
            return;
        }
        if (dragging && gesture) gesture (true);
    }
    void mouseMove (const MouseEvent& e) override
    {
        const bool in = erArea.contains (e.position);
        if (in && ! hinted && onShow) onShow ("Reflections:", "drag to move (sideways = time, up/down = level),  shift-click or drag a box to select more,  double-click to add or remove");
        hinted = in;
    }
    void mouseDoubleClick (const MouseEvent& e) override
    {
        if (arrangeMods (e)) return;
        if (decArea.contains (e.position) && e.position.getDistanceFrom (handle) >= 10)
        {   // back to the start: the picture and the sound
            yaw = pitch = erYaw = erPitch = 0; if (onTurnReset) onTurnReset(); dragging = 0; repaint(); return;
        }
        if (! getBars || ! erArea.expanded (4).contains (e.position)) return;
        const auto hb = hitBar (e.position);
        if (hb.idx >= 0) { sel.clear(); if (onToggle) onToggle (hb); }
        else if (onAdd) onAdd (jlimit (0.1f, erRange, (e.position.x - erArea.getX()) / erArea.getWidth() * erRange), jlimit (-60.0f, 0.0f, (erArea.getBottom() - e.position.y) / erArea.getHeight() * 60.0f - 60.0f));
        dragging = 0; repaint();
    }
    bool keyPressed (const KeyPress& k) override
    {
        if ((k == KeyPress::deleteKey || k == KeyPress::backspaceKey) && ! sel.empty() && getBars && onToggle)
        {
            std::vector<Bar> v; for (auto& b : getBars()) if (isSel (b)) v.push_back (b);
            std::sort (v.begin(), v.end(), [] (const Bar& a, const Bar& b) { return a.idx > b.idx; });   // yours from the last, so numbers stay right
            sel.clear(); for (auto& b : v) onToggle (b); repaint(); return true;
        }
        if (k.isKeyCode ('A') && k.getModifiers().isCommandDown() && getBars) { sel.clear(); for (auto& b : getBars()) sel.push_back ({ b.own, b.idx }); repaint(); return true; }
        return false;
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (arrangeMods (e)) return;
        if (dragging == 5)
        {
            const float dx = (e.position.x - startPos.x) / jmax (1.0f, decArea.getWidth()), dy = (e.position.y - startPos.y) / jmax (1.0f, decArea.getHeight());
            const int what = linkOn ? 0 : turnWhat;
            if (! twisting)
            {
                if (what != 1) { yaw = jlimit (-0.6f, 0.4f, startAngles[0] + dx * 0.6f); pitch = jlimit (-0.3f, 0.3f, startAngles[1] - dy * 0.5f); }
                if (what != 2 && ! linkOn) { erYaw = jlimit (-0.6f, 0.4f, startAngles[2] + dx * 0.6f); erPitch = jlimit (-0.3f, 0.3f, startAngles[3] - dy * 0.5f); }
            }
            if (onTurn) onTurn (what, dx, dy, twisting, 1);
            repaint(); return;
        }
        if (dragging == 3 && onDrag)
        {
            const float dMs = (e.position.x - startPos.x) / erArea.getWidth() * erRange, dDb = -(e.position.y - startPos.y) / erArea.getHeight() * 60.0f;
            onDrag (dMs, dDb); repaint(); return;
        }
        if (dragging == 4 && getBars)
        {
            band = Rectangle<float> (startPos, e.position);
            if (! e.mods.isShiftDown() && ! e.mods.isCommandDown()) sel.clear();
            for (auto& b : getBars()) if (band.contains (Point<float> (erX (b.ms), b.off ? erArea.getBottom() - 3 : yOf (erArea, b.db))) && ! isSel (b)) sel.push_back ({ b.own, b.idx });
            repaint(); return;
        }
        if (dragging == 1 && setPredelayMs) { const float ms = jlimit (0.0f, erRange, (e.position.x - erArea.getX()) / erArea.getWidth() * erRange); setPredelayMs (ms); if (onShow) onShow ("Predelay", String (ms, 1) + " ms"); }
        if (dragging == 2 && setRt) { const float k = std::pow (2.0f, (e.position.x - startX) / 120.0f), rt = jlimit (0.05f, 70.0f, startRt * k); setRt (rt); if (onShow) onShow ("Decay", String (rt, 2) + " s"); }
        repaint();
    }
    void mouseUp (const MouseEvent&) override
    {
        if (dragging == 5 && onTurn) onTurn (linkOn ? 0 : turnWhat, 0, 0, twisting, 2);
        if (dragging == 3 && onDragEnd) onDragEnd();
        if ((dragging == 1 || dragging == 2) && gesture) gesture (false);
        dragging = 0; band = {}; repaint();
    }
private:
    float erX (float ms) const { return erArea.getX() + erArea.getWidth() * jlimit (0.0f, 1.0f, ms / erRange); }
    Bar hitBar (Point<float> p) const
    {
        if (! getBars) return { 0, 0, false, -1, false };
        Bar best { 0, 0, false, -1, false }; float bd = 7.0f;
        for (auto& b : getBars())
        {
            if (b.ms > erRange) continue;
            const float x = erX (b.ms), y = b.off ? erArea.getBottom() - 3 : yOf (erArea, b.db);
            const float d = std::abs (p.x - x) + (p.y < y - 6 ? (y - 6 - p.y) : 0.0f);      // on the stem or its head
            if (d < bd && p.y <= erArea.getBottom() + 4) { bd = d; best = b; }
        }
        return best;
    }
    Rectangle<float> band; Point<float> startPos; bool hinted = false, twisting = false; float startAngles[4] {}; Rectangle<float> turnKeys[3];
    // ---- the 3D waterfall: frequency across (log), time going back (up and to the right), level up
    static constexpr float kDepthX = 0.30f, kDepthY = 0.42f;
    static float wfIndex (float f) { return (float) (ResponseRenderer::kWfBands - 1) * std::log (jlimit (40.0f, 16000.0f, f) / 40.0f) / std::log (400.0f); }
    Point<float> P (float band, float t, float db) const { return Pv (band, t, db, yaw, pitch); }
    Point<float> Pv (float band, float t, float db, float yw, float pt) const
    {
        // turned: yaw swings the depth (time) axis left/right, pitch tilts it up/down
        const float dx = jlimit (-0.45f, 0.6f, kDepthX + yw), dy = jlimit (0.08f, 0.75f, kDepthY + pt);
        const float fw = decArea.getWidth() * (1.0f - std::abs (dx)), lh = decArea.getHeight() * (1.0f - dy), k = 1.0f - jlimit (0.0f, 1.0f, t / tMax());   // the start at the back, the tail comes toward you
        const float x0 = decArea.getX() + (dx < 0 ? decArea.getWidth() * -dx : 0.0f);
        return { x0 + fw * band / (float) (ResponseRenderer::kWfBands - 1) + decArea.getWidth() * dx * k,
                 decArea.getBottom() - lh * (jlimit (-60.0f, 0.0f, db) + 60.0f) / 60.0f - decArea.getHeight() * dy * k };
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
        const int ns = 56;
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
                g.setColour (rc[k].withAlpha (0.3f + 0.7f * (1.0f - age))); g.strokePath (top, PathStrokeType (si == 0 ? 1.6f : 0.85f));
            }
        }
        // the early reflections as part of the start: each one a thin line across the frequencies at its time and level,
        // with a stem at the low end (yours in the key-light colour)
        if (getBars)
            for (auto& b : getBars())
            {
                if (b.off) continue;
                const float t = b.ms * 0.001f; if (t > tm) continue;
                const float db = jlimit (-60.0f, 0.0f, b.db);
                const Colour c = b.own ? th.keyLight : col;
                const float ey = linkOn ? yaw : erYaw, ep = linkOn ? pitch : erPitch;
                g.setColour (c.withAlpha (b.own ? 0.6f : 0.3f)); g.drawLine (Line<float> (Pv (0, t, db, ey, ep), Pv ((float) nb - 1, t, db, ey, ep)), b.own ? 1.0f : 0.6f);
                g.setColour (c.withAlpha (0.55f)); g.drawLine (Line<float> (Pv (0, t, -60, ey, ep), Pv (0, t, db, ey, ep)), 0.6f);
            }
        // the live tail: what is coming out right now, glowing across the frequencies at the start of the picture
        {
            Path lp; const float lvl[3] = { Decibels::gainToDecibels (live[0], -60.0f), Decibels::gainToDecibels (live[1], -60.0f), Decibels::gainToDecibels (live[2], -60.0f) };
            const float cb[3] = { wfIndex (120.0f), wfIndex (1000.0f), wfIndex (6000.0f) };
            for (int i = 0; i <= 60; ++i)
            {
                const float bf = (float) (nb - 1) * (float) i / 60.0f;
                const float a = bf <= cb[1] ? jlimit (0.0f, 1.0f, (bf - cb[0]) / jmax (0.001f, cb[1] - cb[0])) : 1.0f + jlimit (0.0f, 1.0f, (bf - cb[1]) / jmax (0.001f, cb[2] - cb[1]));
                const float db = a <= 1.0f ? lvl[0] + (lvl[1] - lvl[0]) * a : lvl[1] + (lvl[2] - lvl[1]) * (a - 1.0f);
                const auto q = P (bf, 0.0f, jmax (-60.0f, db)); if (i == 0) lp.startNewSubPath (q); else lp.lineTo (q);
            }
            const bool any = live[0] + live[1] + live[2] > 0.001f;
            if (any)
            {
                g.setColour (th.keyLight.withAlpha (0.22f)); g.strokePath (lp, PathStrokeType (6.0f));
                g.setColour (th.keyLight.withAlpha (0.95f)); g.strokePath (lp, PathStrokeType (1.8f));
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
            if (unit.isEmpty()) continue;
            g.setColour (col.withAlpha (0.5f)); g.drawText (String (t, unit == "s" && step < 1 ? 1 : 0) + " " + unit, Rectangle<float> (x - th.sz (24.0f), a.getBottom() + 1, th.sz (48.0f), th.sz (12.0f)), Justification::centred, false);
        }
        for (float db : { 0.0f, -20.0f, -40.0f, -60.0f })
        {
            const float y = yOf (a, db); g.setColour (col.withAlpha (0.08f)); g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
            if (unit.isEmpty()) continue;
            g.setColour (col.withAlpha (0.5f)); g.drawText (String ((int) db), Rectangle<float> (a.getX() - th.sz (28.0f), y - th.sz (6.0f), th.sz (25.0f), th.sz (12.0f)), Justification::right, false);
        }
    }
    const Theme& th; ResponseRenderer::Result target, shown; Rectangle<float> erArea, decArea; Point<float> handle { -100, -100 };
    int dragging = 0; float startRt = 1, startX = 0;
public:
    bool busy() const { return dragging != 0; }
private:
};

// ============================================================================ a control you put on the screen: the number drags up/down or types on double-click;
// the name drags it anywhere, and a double-click on the name removes it
class NumberCell : public Component, private TextEditor::Listener
{
public:
    explicit NumberCell (const Theme& t) : th (t) { addChildComponent (ed); ed.addListener (this); ed.setJustification (Justification::centred); }
    std::function<String()> name, value;
    std::function<float()> getNorm; std::function<void (float)> setNorm; std::function<void (bool)> gesture; std::function<void (const String&)> setText;
    std::function<void()> onMoved, onRemove;           // the name part: drag to move, double-click to remove
    bool hot = false;
    float nameSplit() const { return textW (ui (nameH(), W::Medium), name ? name() : String()) + 7.0f; }
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
        if (arrangeMods (e)) return;
        if (ed.isVisible()) return;
        if (e.position.x < nameSplit()) { moving = true; grab = e.getPosition(); setMouseCursor (MouseCursor::DraggingHandCursor); return; }
        dragging = true; start = getNorm ? getNorm() : 0.0f; if (gesture) gesture (true); setMouseCursor (MouseCursor::UpDownResizeCursor);
    }
    void mouseMove (const MouseEvent& e) override { setMouseCursor (e.position.x < nameSplit() ? MouseCursor::DraggingHandCursor : MouseCursor::UpDownResizeCursor); }
    void mouseDoubleClick (const MouseEvent& e) override
    {
        if (arrangeMods (e)) return;
        if (e.position.x < nameSplit()) { moving = false; if (onRemove) onRemove(); return; }
        ed.setFont (ui (th.displayPt, W::Bold)); ed.setColour (TextEditor::backgroundColourId, th.display.brighter (0.15f)); ed.setColour (TextEditor::textColourId, th.displayText);
        ed.setColour (TextEditor::outlineColourId, th.displayText.withAlpha (0.5f));
        ed.setText (value ? value() : String(), false); ed.setVisible (true); ed.grabKeyboardFocus(); ed.selectAll(); repaint();
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (arrangeMods (e)) return;
        if (moving)
        {
            if (auto* p = getParentComponent())
            {
                const auto at = e.getEventRelativeTo (p).getPosition() - grab;
                setTopLeftPosition (jlimit (0, p->getWidth() - getWidth(), at.x), jlimit (0, p->getHeight() - getHeight(), at.y));
            }
            return;
        }
        if (! dragging || ! setNorm) return;
        const float fine = e.mods.isShiftDown() ? 0.1f : 1.0f;
        setNorm (jlimit (0.0f, 1.0f, start - (float) e.getDistanceFromDragStartY() * 0.004f * fine)); repaint();
    }
    void mouseUp (const MouseEvent&) override
    {
        if (moving) { moving = false; if (onMoved) onMoved(); }
        if (dragging) { dragging = false; if (gesture) gesture (false); }
    }
private:
    bool moving = false; Point<int> grab;
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
// the gate's own meter: the level it listens to (a vertical bar), its threshold (a line), and how far it has closed
class GateMeter : public Component
{
public:
    explicit GateMeter (const Theme& t) : th (t) {}
    float keyDb = -120, gainDb = 0, thrDb = -40; bool on = false;
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2, 4);
        const float gp = th.tpt ("knob:GATE", th.gatePt);
        auto lab = r.removeFromBottom (gp + 5);
        // two bars of the same size side by side: the level, and how far the gate has closed
        const float bw = jmin (16.0f, r.getWidth() * 0.26f);
        auto bar = r.withSizeKeepingCentre (bw, r.getHeight()).translated (-(bw + 6) * 0.5f, 0);
        auto gr = bar.translated (bw + 6, 0);
        g.setColour (Colours::black.withAlpha (0.65f)); g.fillRoundedRectangle (bar.expanded (2), 3); g.fillRoundedRectangle (gr.expanded (2), 2);
        auto yOf = [&bar] (float db) { return bar.getBottom() - bar.getHeight() * jlimit (0.0f, 1.0f, (db + 72.0f) / 72.0f); };
        // the level: lit segments
        const int n = 24;
        for (int i = 0; i < n; ++i)
        {
            const float db = -72.0f + 72.0f * (i + 0.5f) / n;
            auto s = Rectangle<float> (bar.getX() + 2, yOf (-72.0f + 72.0f * (i + 1) / n) + 1, bar.getWidth() - 4, bar.getHeight() / n - 2);
            const Colour c = db > -6 ? Colour (0xffff4a2a) : db > -18 ? Colour (0xffffc03a) : Colour (0xff7fe07a);
            g.setColour (keyDb >= db ? c : c.withAlpha (0.10f)); g.fillRect (s);
        }
        // the threshold
        const float ty = yOf (thrDb);
        g.setColour (th.keyLight); g.fillRect (bar.getX() - 3, ty - 1, gr.getRight() - bar.getX() + 6, 2.0f);
        // how far it has closed: a red bar from the top
        const float closed = jlimit (0.0f, 1.0f, -gainDb / 72.0f);
        g.setColour (Colour (0xffff4a2a).withAlpha (on ? 0.9f : 0.25f)); g.fillRect (gr.withHeight (gr.getHeight() * closed));
        drawLegend (g, "GATE", lab, ui (gp, W::Bold, 0.06f), th.tcol ("knob:GATE", th.panelText));
    }
    void mouseDown (const MouseEvent& e) override { if (arrangeMods (e)) return; if (e.mods.isPopupMenu()) editText ("knob:GATE", "Gate meter text", th.gatePt, th.panelText, this, getLocalBounds()); }
private:
    const Theme& th;
};

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
        // taller than wide: a vertical meter (the needle swings up and down, 0 at the top); otherwise the wide arc.
        // Every dB has a tick, every even number its figure: 0 2 4 ... 20
        const bool vert = face.getHeight() > face.getWidth() * 1.15f;
        const float half = 0.80f;                                               // half the swing, radians
        const float fs = jlimit (7.0f, 14.0f, vert ? face.getWidth() * 0.12f : jmin (face.getWidth() * 0.045f, face.getHeight() * 0.13f));
        const float off = 8.0f + fs * 0.9f;
        float rad; Point<float> pv; float aTop;                                 // aTop: angle of 0 dB
        if (! vert)
        {
            rad = jmax (8.0f, jmin (face.getHeight() * 1.25f, (face.getWidth() * 0.5f - fs * 1.3f - 2.0f) / std::sin (half) - off));
            pv = { face.getCentreX(), face.getY() + fs * 0.7f + 3.0f + off + rad };
            aTop = half;                                                        // 0 at the right
        }
        else
        {
            rad = jmax (8.0f, jmin (face.getWidth() * 1.25f, (face.getHeight() * 0.5f - fs - 4.0f) / std::sin (half) - off));
            pv = { face.getRight() - fs * 1.6f - 4.0f - off - rad, face.getCentreY() };
            aTop = MathConstants<float>::halfPi - half;                         // 0 at the top
        }
        auto ang = [&] (float gr) { const float u = jlimit (0.0f, 20.0f, gr) / 20.0f; return vert ? aTop + 2.0f * half * u : aTop - 2.0f * half * u; };
        Path arc; arc.addCentredArc (pv.x, pv.y, rad, rad, 0, jmin (ang (0), ang (20)), jmax (ang (0), ang (20)), true);
        g.setColour (Colour (0xff1b1b1b)); g.strokePath (arc, PathStrokeType (1.3f));
        Path red; red.addCentredArc (pv.x, pv.y, rad + 3, rad + 3, 0, jmin (ang (14), ang (20)), jmax (ang (14), ang (20)), true);
        g.setColour (Colour (0xffc0392b)); g.strokePath (red, PathStrokeType (3.0f));
        g.setFont (ui (fs, W::Bold));
        for (int gr = 0; gr <= 20; ++gr)
        {
            const float a = ang ((float) gr); const bool major = gr % 2 == 0;
            g.setColour (gr >= 14 ? Colour (0xffb02a1e) : Colour (0xff1b1b1b));
            g.drawLine (Line<float> (pv.getPointOnCircumference (rad, a), pv.getPointOnCircumference (rad + (major ? 8.0f : 4.5f), a)), major ? 1.4f : 0.9f);
            if (major) { const auto q = pv.getPointOnCircumference (rad + off, a); g.drawText (String (gr), Rectangle<float> (q.x - fs * 1.2f, q.y - fs * 0.6f, fs * 2.4f, fs * 1.2f), Justification::centred, false); }
        }
        g.setColour (Colour (0xff1b1b1b));
        if (! vert)
        {
            g.setFont (ui (jmin (fs * 1.0f, face.getWidth() / 13.0f), W::Bold, 0.1f));
            g.drawText (face.getWidth() < 140 ? "GR  dB" : "GAIN REDUCTION  dB", face.withTrimmedTop (face.getHeight() * 0.52f).withHeight (face.getHeight() * 0.2f), Justification::centred, false);
            g.setFont (ui (fs * 1.15f, W::Bold, 0.05f));
            g.drawText ("VU", face.withTrimmedTop (face.getHeight() * 0.7f), Justification::centred, false);
        }
        else
        {
            auto t = face.reduced (4).withWidth (jmax (20.0f, pv.x + rad * 0.55f - face.getX()));
            g.setFont (ui (fs * 1.15f, W::Bold, 0.05f)); g.drawText ("VU", t.removeFromBottom (fs * 2.0f), Justification::centredLeft, false);
            g.setFont (ui (fs * 0.9f, W::Bold, 0.1f)); g.drawText ("GR dB", t.removeFromTop (fs * 2.0f), Justification::centredLeft, false);
        }
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

// ============================================================================ one text's own size and colour
class TextStyleEditor : public Component
{
public:
    TextStyleEditor (Theme& t, const String& textId, const String& label, float defPt, Colour defCol) : th (t), id (textId), name (label), dPt (defPt), dCol (defCol)
    {
        setSize (320, 132);
        size.setSliderStyle (Slider::LinearHorizontal); size.setTextBoxStyle (Slider::TextBoxRight, false, 48, 20); size.setRange (6, 40, 0.5);
        size.setValue (th.tpt (id, dPt), dontSendNotification); addAndMakeVisible (size);
        size.onValueChange = [this] { th.texts[id].pt = (float) size.getValue(); if (changed) changed(); repaint(); };
        addAndMakeVisible (resetBtn); resetBtn.onClick = [this] { th.texts.erase (id); size.setValue (dPt, dontSendNotification); if (changed) changed(); repaint(); };
    }
    std::function<void()> changed;
    void resized() override { size.setBounds (70, 34, getWidth() - 82, 24); resetBtn.setBounds (12, getHeight() - 34, getWidth() - 24, 26); }
    void paint (Graphics& g) override
    {
        g.fillAll (Colour (0xff1c1d1f));
        drawLegend (g, name, Rectangle<float> (12, 6, (float) getWidth() - 24, 20), ui (12.5f, W::Bold), Colours::white, Justification::centredLeft);
        drawLegend (g, "Size", Rectangle<float> (12, 34, 56, 24), ui (12.0f, W::Medium), Colours::white.withAlpha (0.8f), Justification::centredLeft);
        drawLegend (g, "Colour", Rectangle<float> (12, 64, 56, 26), ui (12.0f, W::Medium), Colours::white.withAlpha (0.8f), Justification::centredLeft);
        g.setColour (th.tcol (id, dCol)); g.fillRoundedRectangle (swatch().toFloat(), 4);
        g.setColour (Colours::white.withAlpha (0.5f)); g.drawRoundedRectangle (swatch().toFloat(), 4, 1.0f);
    }
    void mouseDown (const MouseEvent& e) override
    {
        if (arrangeMods (e)) return;
        if (! swatch().contains (e.getPosition())) return;
        auto sel = std::make_unique<ColourSelector> (ColourSelector::showColourspace | ColourSelector::showSliders | ColourSelector::showColourAtTop);
        sel->setSize (300, 300); sel->setCurrentColour (th.tcol (id, dCol));
        struct Lst : ChangeListener { TextStyleEditor* p; void changeListenerCallback (ChangeBroadcaster* b) override
        { auto& t = p->th.texts[p->id]; t.col = static_cast<ColourSelector*> (b)->getCurrentColour(); t.hasCol = true; p->repaint(); if (p->changed) p->changed(); } };
        auto* l = new Lst(); l->p = this; sel->addChangeListener (l); listener.reset (l);
        CallOutBox::launchAsynchronously (std::move (sel), swatch().translated (getScreenX(), getScreenY()), nullptr);
    }
    Theme& th; String id;
private:
    Rectangle<int> swatch() const { return { 70, 64, getWidth() - 82, 26 }; }
    String name; float dPt; Colour dCol; Slider size; TextButton resetBtn { "BACK TO NORMAL" }; std::unique_ptr<ChangeListener> listener;
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
        if (arrangeMods (e)) return;
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

// ============================================================================ a small round X that closes a panel; lights yellow under the mouse
class CloseX : public Component, private Timer
{
public:
    explicit CloseX (const Theme& t) : th (t) { setRepaintsOnMouseActivity (true); setMouseCursor (MouseCursor::PointingHandCursor); }
    std::function<void()> onClick;
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f);
        const float d = jmin (r.getWidth(), r.getHeight()); r = Rectangle<float> (d, d).withCentre (r.getCentre());
        g.setColour (Colours::black.withAlpha (th.shadow())); g.fillEllipse (r.translated (0.8f * th.d3(), 1.6f * th.d3()));
        const Colour base = th.key.interpolatedWith (th.keyLight, glow);
        g.setGradientFill (ColourGradient (th.hi (base, 1.2f), r.getX(), r.getY(), th.lo (base, 1.4f), r.getX(), r.getBottom(), false));
        g.fillEllipse (r);
        g.setColour (Colours::white.withAlpha (0.35f * th.d3())); g.drawEllipse (r.reduced (0.8f), 0.8f);
        g.setColour (Colours::black.withAlpha (0.5f)); g.drawEllipse (r, 1.0f);
        if (glow > 0.01f) { g.setColour (th.keyLight.withAlpha (0.35f * glow)); g.drawEllipse (r.expanded (1.5f), 2.0f); }
        const auto c = r.getCentre(); const float a = d * 0.2f, w = jmax (1.6f, d * 0.09f) + (down ? 0.3f : 0.0f);
        g.setColour (th.keyText);
        g.drawLine (c.x - a, c.y - a, c.x + a, c.y + a, w); g.drawLine (c.x - a, c.y + a, c.x + a, c.y - a, w);
    }
    void mouseEnter (const MouseEvent&) override { startTimerHz (60); }
    void mouseExit (const MouseEvent&) override { startTimerHz (60); }
    void mouseDown (const MouseEvent&) override { down = true; repaint(); }
    void mouseUp (const MouseEvent& e) override { if (arrangeMods (e)) return; down = false; repaint(); if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
private:
    void timerCallback() override
    {
        const float t = isMouseOver() ? 1.0f : 0.0f; glow += (t - glow) * 0.3f;
        if (std::abs (t - glow) < 0.01f) { glow = t; stopTimer(); }
        repaint();
    }
    const Theme& th; float glow = 0; bool down = false;
};

class SetupPanel : public Component
{
public:
    SetupPanel (Theme& t, Flexiverb480Processor& p) : th (t), proc (p)
    {
        auto addSl = [this] (Slider& s, double lo, double hi, double step)
        { s.setSliderStyle (Slider::LinearHorizontal); s.setTextBoxStyle (Slider::TextBoxRight, false, 44, 20); s.setRange (lo, hi, step); addAndMakeVisible (s); s.onValueChange = [this] { fromSliders(); }; };
        addSl (grad, 0, 1, 0.01); addSl (keyPt, 9, 20, 0.5); addSl (keyHt, -10, 30, 1); addSl (keyWd, 0, 80, 1); addSl (knobPt, 9, 24, 0.5); addSl (dispPt, 9, 22, 0.5); addSl (faderPt, 8, 18, 0.5);
        addSl (knobSz, 0.6, 1.6, 0.05); addSl (setPt, 10, 18, 0.5); addSl (menuPt, 10, 28, 0.5);
        addSl (stKnob, 0.5, 2.0, 0.05); addSl (stPt, 9, 28, 0.5); addSl (stNum, 6, 20, 0.5); addSl (gtPt, 9, 28, 0.5); addSl (anSize, 0.5, 2.0, 0.05); addSl (hvZoom, 0, 60, 1); hvZoom.setTextValueSuffix (" %"); addSl (keyDp, 0, 1, 0.05); addSl (keyRd, 0, 0.5, 0.01);
        anSize.setVisible (false);
        for (auto* c : { &osBox, &precBox, &depthBox, &convBox }) addChildComponent (*c);
        depthBox.setVisible (true);
        osBox.addItemList ({ "Off", "2x", "4x", "6x", "8x" }, 1); precBox.addItemList ({ "32-bit", "64-bit" }, 1); convBox.addItemList ({ "24-bit (clean)", "12-bit", "15-bit ranging", "16-bit ranging" }, 1); depthBox.addItemList ({ "Flat", "Soft", "Deep", "Extra deep" }, 1);
        depthBox.onChange = [this] { th.depth = depthBox.getSelectedItemIndex(); if (onChange) onChange(); };
        osAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "os", osBox);
        precAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "prec", precBox);
        convAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "conv", convBox);
        for (auto* b : { &resetBtn, &saveBtn, &addKeyBtn, &resetPosBtn }) addAndMakeVisible (*b);
        addChildComponent (closeX);
        resetPosBtn.onClick = [this] { if (onResetPositions) onResetPositions(); };
        addKeyBtn.onClick = [this] { if (onAddKey) onAddKey(); };
        resetBtn.onClick = [this] { th = Theme(); sync(); if (onChange) onChange(); };
        saveBtn.onClick = [this] { Theme::defaultFile().getParentDirectory().createDirectory(); Theme::defaultFile().replaceWithText (th.toString()); saveBtn.setButtonText ("SAVED AS DEFAULT"); };
        closeX.onClick = [this] { if (onClose) onClose(); };
        sync();
    }
    std::function<void()> onChange, onClose, onAddKey, onResetPositions;
    // where each PREFERENCES page starts: colours, text and sizes, keys, moved controls
    int sectionY (int page) const { return page <= 0 ? 0 : page == 1 ? jmax (0, lookTop - 10) : page == 2 ? jmax (0, keysTop - 10) : jmax (0, movedTop - 10); }
    void sync()
    {
        grad.setValue (th.gradient, dontSendNotification); keyPt.setValue (th.keyPt, dontSendNotification); keyHt.setValue (th.keyHeight, dontSendNotification); keyWd.setValue (th.keyPad, dontSendNotification); knobPt.setValue (th.knobPt, dontSendNotification);
        dispPt.setValue (th.displayPt, dontSendNotification); faderPt.setValue (th.faderPt, dontSendNotification);
        knobSz.setValue (th.knobSize, dontSendNotification); setPt.setValue (th.setupPt, dontSendNotification); menuPt.setValue (th.menuPt, dontSendNotification);
        stKnob.setValue (th.stripKnob, dontSendNotification); stPt.setValue (th.stripPt, dontSendNotification); stNum.setValue (th.stripNumPt, dontSendNotification); gtPt.setValue (th.gatePt, dontSendNotification); anSize.setValue (th.analogSize, dontSendNotification); hvZoom.setValue (th.hoverZoom, dontSendNotification); keyDp.setValue (th.keyDepth, dontSendNotification); keyRd.setValue (th.keyRound, dontSendNotification);
        depthBox.setSelectedItemIndex (th.depth, dontSendNotification); saveBtn.setButtonText ("SAVE AS DEFAULT FOR NEW WINDOWS"); repaint();
    }
    // every text in SETUP follows one size (13 pt to start with: the slider "Setup text", or right-click any of it)
    float pt() const { return th.tpt ("setup text", th.setupPt); }
    int rowH() const { return roundToInt (pt() * 1.75f + 4); }
    int labW() const { return roundToInt (pt() * 9.0f); }
    void resized() override
    {
        auto r = getLocalBounds().reduced (18); r.removeFromTop (4);
        const int rh = rowH(), lw = labW();
        swatches.clear();
        for (int i = 0; i < (int) Theme::items().size(); ++i) swatches.push_back (Rectangle<int> (r.getX(), r.getY() + rh + i * (rh + 4), r.getWidth(), rh));
        int y = r.getY() + rh + (int) Theme::items().size() * (rh + 4) + rh + 8;
        lookTop = y - rh; int k = 0;
        for (auto* s : sliders()) { if (s == &keyDp) { y += rh + 8; keysTop = y - rh; } s->setBounds (r.getX() + lw, y, r.getWidth() - lw, rh); sliderY[k++] = y; y += rh + 8; }
        depthBox.setBounds (r.getX() + lw, y, 130, rh + 2); depthY = y; y += rh + 12;
        addKeyBtn.setBounds (r.getX(), y, r.getWidth(), rh + 6); y += rh + 6 + rh + 16;
        movedTop = y - rh;
        resetPosBtn.setBounds (r.getX(), y + rh, r.getWidth(), rh + 6); y += 2 * rh + 6 + 24;
        resetBtn.setBounds (r.getX(), y, r.getWidth(), rh + 6); y += rh + 12;
        saveBtn.setBounds (r.getX(), y, r.getWidth(), rh + 6); y += rh + 12;
        prefH = y + 30;
        for (auto* s : sliders()) s->setTextBoxStyle (Slider::TextBoxRight, false, roundToInt (pt() * 4), rh);
    }
    std::vector<Slider*> sliders() { return { &grad, &keyPt, &keyHt, &keyWd, &knobPt, &dispPt, &faderPt, &knobSz, &setPt, &menuPt, &stKnob, &stPt, &stNum, &gtPt, &hvZoom, &keyDp, &keyRd }; }
    int preferredHeight() const { return prefH > 0 ? prefH : 1400; }
    void paint (Graphics& g) override
    {
        g.fillAll (th.cheek.darker (0.3f));
        auto r = getLocalBounds().reduced (18); r.removeFromTop (4);
        const float p = pt(); const int rh = rowH(), lw = labW();
        const Colour tc = th.tcol ("setup text", th.panelText);
        drawLegend (g, "Colours  (click one: top, middle and bottom)", Rectangle<float> ((float) r.getX(), (float) r.getY(), (float) r.getWidth(), (float) rh), ui (p, W::Bold), th.keyLight, Justification::centredLeft);
        int i = 0;
        for (auto& it : Theme::items())
        {
            auto s = swatches[(size_t) i++].toFloat(); auto box = s.removeFromLeft (40);
            g.setGradientFill (th.grad (it.c, box));
            g.fillRoundedRectangle (box, 4); g.setColour (Colours::white.withAlpha (0.4f)); g.drawRoundedRectangle (box, 4, 1.0f);
            drawLegend (g, it.name, s.withTrimmedLeft (8), ui (p, W::Medium), tc, Justification::centredLeft);
        }
        const float x = (float) r.getX();
        drawLegend (g, "Text and sizes", Rectangle<float> (x, (float) lookTop, 300, (float) rh), ui (p, W::Bold), th.keyLight, Justification::centredLeft);
        drawLegend (g, "Keys  (< > SAVE PANEL PAGE MUTE ...)", Rectangle<float> (x, (float) keysTop, 400, (float) rh), ui (p, W::Bold), th.keyLight, Justification::centredLeft);
        const char* lab[] = { "Gradient", "Button text", "Button height +", "Button width +", "Panel text", "Screen text", "Fader text", "System knob size", "Setup text", "Preset menu text",
                              "Strip knob size", "Strip knob names", "Strip knob numbers", "Gate meter text", "Hover zoom (panel)", "Key depth (0 = flat)", "Key corners" };
        for (int k = 0; k < 17; ++k) drawLegend (g, lab[k], Rectangle<float> (x, (float) sliderY[k], (float) lw - 6, (float) rh), ui (p, W::Medium), tc, Justification::centredLeft);
        drawLegend (g, "3D", Rectangle<float> (x, (float) depthY, (float) lw - 6, (float) rh), ui (p, W::Medium), tc, Justification::centredLeft);
        drawLegend (g, "Moved controls", Rectangle<float> (x, (float) movedTop, 300, (float) rh), ui (p, W::Bold), th.keyLight, Justification::centredLeft);
        g.setColour (tc.withAlpha (0.7f)); g.setFont (ui (p * 0.9f, W::Medium));
        g.drawFittedText ("Ctrl+Shift (Cmd+Shift) + drag moves any button, knob, word or meter; drag its lower right corner to resize. It snaps to the line of its neighbours. Ctrl+Shift + double-click puts one back.",
                          Rectangle<int> ((int) x, movedTop + rh, getWidth() - 36, rh), Justification::centredLeft, 2, 0.8f);
    }
    std::function<void (Point<int>)> onRightClick;
    void mouseDown (const MouseEvent& e) override
    {
        if (arrangeMods (e)) return;
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick (e.getPosition()); return; }
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
    void fromSliders() { th.gradient = (float) grad.getValue(); th.keyPt = (float) keyPt.getValue(); th.keyHeight = (float) keyHt.getValue(); th.keyPad = (float) keyWd.getValue(); th.knobPt = (float) knobPt.getValue(); th.displayPt = (float) dispPt.getValue(); th.faderPt = (float) faderPt.getValue();
                         th.knobSize = (float) knobSz.getValue(); th.setupPt = (float) setPt.getValue(); th.menuPt = (float) menuPt.getValue();
                         th.stripKnob = (float) stKnob.getValue(); th.stripPt = (float) stPt.getValue(); th.stripNumPt = (float) stNum.getValue(); th.gatePt = (float) gtPt.getValue(); th.analogSize = (float) anSize.getValue(); th.hoverZoom = (float) hvZoom.getValue();
                         th.keyDepth = (float) keyDp.getValue(); th.keyRound = (float) keyRd.getValue(); resized(); repaint(); if (onChange) onChange(); }
    Flexiverb480Processor& proc;
    Slider grad, keyPt, keyHt, keyWd, knobPt, dispPt, faderPt, knobSz, setPt, menuPt, stKnob, stPt, stNum, gtPt, anSize, hvZoom, keyDp, keyRd; ComboBox osBox, precBox, depthBox, convBox;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> osAtt, precAtt, convAtt;
    TextButton resetBtn { "RESET THE LOOK" }, saveBtn { "SAVE AS DEFAULT FOR NEW WINDOWS" }, addKeyBtn { "MAKE A BUTTON FOR THE CURRENT PRESET" }, resetPosBtn { "PUT EVERY MOVED CONTROL BACK" };
    std::vector<Rectangle<int>> swatches; int lookTop = 0, qualTop = 0, keysTop = 0, movedTop = 0, depthY = 0, prefH = 0; int sliderY[20] = {};
    std::unique_ptr<ChangeListener> listener;
    CloseX closeX { th };
};


// ============================================================================ the gear wheel: opens PREFERENCES
class GearButton : public Component
{
public:
    explicit GearButton (const Theme& t) : th (t) { setRepaintsOnMouseActivity (true); setMouseCursor (MouseCursor::PointingHandCursor); setTooltip ("Preferences"); }
    std::function<void()> onClick; bool lit = false;
    void setTooltip (const String&) {}
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        const float d = jmin (r.getWidth(), r.getHeight()); const auto c = r.getCentre();
        const float ro = d * 0.5f, ri = d * 0.36f, hole = d * 0.15f;
        Path p; const int teeth = 8;
        for (int i = 0; i < teeth * 2; ++i)
        {   // each tooth: flat top, slanted sides
            const float a0 = MathConstants<float>::twoPi * (float) i / (teeth * 2), a1 = MathConstants<float>::twoPi * (float) (i + 1) / (teeth * 2);
            const float rr = (i % 2 == 0) ? ro : ri, sl = (i % 2 == 0) ? 0.06f : -0.06f;
            auto pt = [&] (float a, float rad) { return Point<float> (c.x + rad * std::sin (a), c.y - rad * std::cos (a)); };
            if (i == 0) p.startNewSubPath (pt (a0 + sl, rr)); else p.lineTo (pt (a0 + sl, rr));
            p.lineTo (pt (a1 - sl, rr));
        }
        p.closeSubPath();
        p.addEllipse (c.x - hole, c.y - hole, hole * 2, hole * 2); p.setUsingNonZeroWinding (false);
        const Colour col = lit || down ? th.keyLight : (isMouseOver() ? th.panelText : th.panelText.withAlpha (0.75f));
        g.setColour (col); g.fillPath (p);
    }
    void mouseDown (const MouseEvent& e) override { if (arrangeMods (e)) return; down = true; repaint(); }
    void mouseUp (const MouseEvent& e) override { if (arrangeMods (e)) return; down = false; repaint(); if (getLocalBounds().contains (e.getPosition()) && ! e.mods.isPopupMenu() && onClick) onClick(); }
private:
    const Theme& th; bool down = false;
};

// a combo box that can be moved like everything else
struct MovBox : public ComboBox
{
    void mouseDown (const MouseEvent& e) override { if (arrangeMods (e)) return; ComboBox::mouseDown (e); }
    void mouseDrag (const MouseEvent& e) override { if (arrangeMods (e)) return; ComboBox::mouseDrag (e); }
    void mouseUp (const MouseEvent& e) override { if (arrangeMods (e)) return; ComboBox::mouseUp (e); }
};

// ============================================================================ PREFERENCES: the Flexi 180V hardware page
class HwPage : public Component
{
public:
    HwPage (Theme& t, AudioProcessorValueTreeState& s) : th (t), apvts (s)
    {
        for (auto* c : { &inBox, &outBox, &osBox, &engBox, &precBox, &convBox }) addAndMakeVisible (*c);
        inBox.addItemList ({ "Analog (standard)", "Analog + transformer option", "Digital I/O" }, 1);
        outBox.addItemList ({ "Analog (standard)", "Analog + transformer option", "Digital I/O" }, 1);
        osBox.addItemList ({ "Off", "2x", "4x", "6x", "8x" }, 1);
        convBox.addItemList ({ "24-bit (clean)", "12-bit", "15-bit ranging", "16-bit ranging" }, 1);
        engBox.addItemList ({ "Single", "Dual (machines A and B)" }, 1);
        precBox.addItem ("64-bit everywhere", 1); precBox.setSelectedId (1, dontSendNotification); precBox.setEnabled (false);
        inAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, "hwInSt", inBox);
        outAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, "hwOutSt", outBox);
        osAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, "os", osBox);
        convAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, "conv", convBox);
        addAndMakeVisible (noise); noise.setButtonText ("Converter noise (about -98 dBFS, as the hardware)");
        noiseAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (s, "hwNoise", noise);
        engBox.onChange = [this] { auto* c = apvts.getParameter ("config"); const int v = engBox.getSelectedItemIndex() == 0 ? 0 : 2;
                                   if (((int) apvts.getRawParameterValue ("config")->load() != 0) != (v != 0)) { c->beginChangeGesture(); c->setValueNotifyingHost (c->convertTo0to1 ((float) v)); c->endChangeGesture(); } };
        outKnob = std::make_unique<ScaleKnob> (th, s, "out", "OUTPUT", numberScale (-24, 12, 6, 2, true), &Theme::compKnob);
        depthKnob = std::make_unique<ScaleKnob> (th, s, "loopAmt", "180V DEPTH", std::vector<ScaleMark> { { 0, "0" }, { 0.5f, "50" }, { 1, "100" }, { 1.5f, "150" }, { 2, "200" } }, &Theme::compKnob);
        outKnob->step = 2.0f;
        for (auto* k : { outKnob.get(), depthKnob.get() }) { k->strip = true; addAndMakeVisible (*k); }
    }
    void sync() { engBox.setSelectedItemIndex ((int) apvts.getRawParameterValue ("config")->load() == 0 ? 0 : 1, dontSendNotification); }
    float pt() const { return th.tpt ("setup text", th.setupPt); }
    int rowH() const { return roundToInt (pt() * 1.75f + 6); }
    int preferredHeight() const { return 640 + 8 * rowH(); }
    void resized() override
    {
        const int rh = rowH(), lw = roundToInt (pt() * 10.5f), x = 24, w = getWidth() - 48;
        int y = flowTop() + flowH() + 46;
        auto row = [&] (Component& c, int width) { c.setBounds (x + lw, y, jmin (width, w - lw), rh); y += rh + 10; };
        rows.clear();
        rows.push_back (y); row (inBox, 300);
        rows.push_back (y); row (outBox, 300);
        noise.setBounds (x, y, w, rh); y += rh + 22;
        rows.push_back (y); row (engBox, 300);
        rows.push_back (y); row (osBox, 160);
        rows.push_back (y); row (precBox, 220);
        rows.push_back (y); row (convBox, 220);
        y += 14; knobTop = y;
        const int kw = jmin (180, w / 2 - 10), kh = jmin (190, kw + 30);
        outKnob->setBounds (x + 10, y + 30, kw - 20, kh - 40); depthKnob->setBounds (x + w / 2 + 10, y + 30, kw - 20, kh - 40);
        knobH = kh;
    }
    void paint (Graphics& g) override
    {
        const float p = pt(); const Colour tc = th.tcol ("setup text", th.panelText), dim = tc.withAlpha (0.6f);
        const int x = 24, w = getWidth() - 48;
        drawLegend (g, "FLEXI 180V HARDWARE", Rectangle<float> ((float) x, 14, (float) w, p + 10), ui (p + 3, W::Bold, 0.15f), tc, Justification::centredLeft);
        g.setColour (dim); g.setFont (ui (p * 0.92f, W::Medium));
        g.drawFittedText ("A fixed model of the real unit at its standard load: electronically balanced analog in and out around 18-bit A/D and D/A, "
                          "with the factory transformer option or the digital I/O. No model, frequency or amount controls.",
                          Rectangle<int> (x, 14 + (int) p + 14, w, (int) (p * 3.2f)), Justification::topLeft, 3, 0.9f);
        // signal flow
        const char* box[] = { "INPUT", "180V IN", "180V", "180V OUT", "OUTPUT", "CHANNEL", "CLEAN" };
        const char* sub[] = { "", "stage + A/D", "ENGINE", "D/A + stage", "", "STRIP *", "OUT" };
        const bool hwb[] = { false, true, true, true, false, false, false };
        const int n = 7, gap = 12, bw = (w - gap * (n - 1)) / n, by = flowTop(), bh = flowH();
        for (int i = 0; i < n; ++i)
        {
            auto b = Rectangle<float> ((float) (x + i * (bw + gap)), (float) by, (float) bw, (float) bh);
            g.setColour (hwb[i] ? Colour (0xff3c3220) : Colour (0xff343538)); g.fillRoundedRectangle (b, 5);
            g.setColour (hwb[i] ? th.keyLight.withAlpha (0.55f) : Colours::white.withAlpha (0.25f)); g.drawRoundedRectangle (b, 5, 1.0f);
            g.setColour (tc); g.setFont (ui (jmin (p * 0.9f, bw / 6.5f), W::Bold, 0.04f));
            g.drawText (box[i], b.withTrimmedBottom (b.getHeight() * (sub[i][0] ? 0.45f : 0.0f)).withTrimmedTop (sub[i][0] ? b.getHeight() * 0.12f : 0.0f), Justification::centred, false);
            if (sub[i][0]) { g.setColour (dim); g.setFont (ui (jmin (p * 0.72f, bw / 8.0f), W::Medium)); g.drawText (sub[i], b.withTrimmedTop (b.getHeight() * 0.5f).withTrimmedBottom (b.getHeight() * 0.12f), Justification::centred, false); }
            if (i < n - 1)
            {
                const float ax = b.getRight() + 1, ay = b.getCentreY();
                g.setColour (tc.withAlpha (0.8f)); g.drawLine (ax, ay, ax + gap - 3, ay, 1.5f);
                Path a; a.addTriangle (ax + gap - 2, ay, ax + gap - 7, ay - 4, ax + gap - 7, ay + 4); g.fillPath (a);
            }
        }
        g.setColour (dim); g.setFont (ui (p * 0.82f, W::Medium));
        g.drawFittedText ("* OUTPUT feeds the channel strip whenever the strip, or any of its modules, is on.  CLEAN OUT is always last and clean.",
                          Rectangle<int> (x, by + bh + 6, w, (int) (p * 2.6f)), Justification::topLeft, 2, 0.9f);
        const char* lab[] = { "Input stage", "Output stage", "Engine", "Oversampling", "Precision", "Classic converter" };
        for (int i = 0; i < (int) rows.size() && i < 6; ++i)
            drawLegend (g, lab[i], Rectangle<float> ((float) x, (float) rows[(size_t) i], 200, (float) rowH()), ui (p, W::Medium), tc, Justification::centredLeft);
        for (int i = 0; i < 2; ++i)
        {
            auto b = Rectangle<float> ((float) (x + i * (w / 2)), (float) knobTop, (float) (w / 2 - 10), (float) knobH);
            g.setColour (Colours::black.withAlpha (0.2f)); g.fillRoundedRectangle (b, 8);
            g.setColour (Colours::white.withAlpha (0.12f)); g.drawRoundedRectangle (b, 8, 1.0f);
            drawLegend (g, i == 0 ? "LEVEL INTO THE STRIP" : "LONG RECIRCULATION", b.reduced (12, 8).withHeight (p + 4), ui (p * 0.8f, W::Bold, 0.15f), dim, Justification::centredLeft);
        }
    }
private:
    int flowTop() const { return 14 + (int) pt() + 14 + (int) (pt() * 3.2f) + 10; }
    int flowH() const { return roundToInt (pt() * 3.6f); }
    Theme& th; AudioProcessorValueTreeState& apvts;
    ComboBox inBox, outBox, osBox, engBox, precBox, convBox; ToggleButton noise;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> inAtt, outAtt, osAtt, convAtt; std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> noiseAtt;
    std::unique_ptr<ScaleKnob> outKnob, depthKnob;
    std::vector<int> rows; int knobTop = 0, knobH = 0;
};

// ============================================================================ PREFERENCES: a window over the panel, its pages listed at the left
class PrefsWindow : public Component
{
public:
    PrefsWindow (Theme& t, SetupPanel& look, HwPage& hw) : th (t), lookPage (look), hwPage (hw)
    {
        addAndMakeVisible (view); view.setScrollBarsShown (true, false);
        addAndMakeVisible (closeX); closeX.onClick = [this] { setVisible (false); };
        items = { { "LOOK", -1 }, { "Colours", 0 }, { "Text and sizes", 1 }, { "Keys", 2 }, { "Moved controls", 3 },
                  { "MODELING", -1 }, { "Flexi 180V hardware", 4 } };
    }
    std::function<void()> onSync;
    void open (int page) { if (onSync) onSync(); show (page); setVisible (true); toFront (true); }
    void show (int page)
    {
        cur = page;
        Component* c = page == 4 ? (Component*) &hwPage : (Component*) &lookPage;
        if (view.getViewedComponent() != c) view.setViewedComponent (c, false);
        layoutPage();
        if (page < 4) view.setViewPosition (0, lookPage.sectionY (page));
        repaint();
    }
    Rectangle<int> box() const { const int w = jmin (getWidth() - 60, 980), h = jmin (getHeight() - 60, 760); return Rectangle<int> (w, h).withCentre (getLocalBounds().getCentre()); }
    int sideW() const { return roundToInt (th.tpt ("setup text", th.setupPt) * 14.0f); }
    void resized() override
    {
        const auto b = box();
        closeX.setBounds (b.getRight() - 40, b.getY() + 14, 26, 26);
        view.setBounds (b.getX() + sideW() + 1, b.getY() + 56, b.getWidth() - sideW() - 2, b.getHeight() - 57);
        layoutPage();
    }
    void layoutPage()
    {
        const int w = view.getWidth() - view.getScrollBarThickness();
        if (cur == 4) hwPage.setSize (w, jmax (view.getHeight(), hwPage.preferredHeight()));
        else lookPage.setSize (w, jmax (view.getHeight(), lookPage.preferredHeight()));
    }
    void paint (Graphics& g) override
    {
        g.fillAll (Colours::black.withAlpha (0.55f));
        const auto b = box().toFloat(); const float p = th.tpt ("setup text", th.setupPt);
        g.setColour (Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (b.translated (5, 7), 12);
        g.setColour (th.cheek.darker (0.3f)); g.fillRoundedRectangle (b, 12);
        g.setColour (Colours::white.withAlpha (0.18f)); g.drawRoundedRectangle (b, 12, 1.0f);
        const Colour tc = th.tcol ("setup text", th.panelText);
        drawLegend (g, "PREFERENCES", Rectangle<float> (b.getX() + 22, b.getY() + 10, 300, 36), ui (p + 5, W::Bold, 0.2f), tc, Justification::centredLeft);
        g.setColour (Colours::white.withAlpha (0.12f)); g.drawHorizontalLine ((int) b.getY() + 55, b.getX(), b.getRight());
        auto side = Rectangle<float> (b.getX(), b.getY() + 56, (float) sideW(), b.getHeight() - 56 - 12);
        g.setColour (Colours::black.withAlpha (0.18f)); g.fillRect (side);
        itemR.clear(); float y = side.getY() + 16;
        for (auto& it : items)
        {
            if (it.second < 0) { y += 8; drawLegend (g, it.first, Rectangle<float> (side.getX() + 18, y, side.getWidth() - 24, p + 6), ui (p * 0.85f, W::Bold, 0.2f), tc.withAlpha (0.55f), Justification::centredLeft); y += p + 12; itemR.push_back ({}); continue; }
            auto r = Rectangle<float> (side.getX() + 10, y, side.getWidth() - 20, p * 1.9f);
            const bool sel = it.second == cur;
            if (sel) { g.setColour (th.keyLight.withAlpha (0.16f)); g.fillRoundedRectangle (r, 5); }
            const auto f = ui (p, sel ? W::Bold : W::Medium);
            if (sel) { g.setFont (f); g.setColour (th.keyLight.withAlpha (0.28f)); for (auto d : { Point<float> (-1, 0), Point<float> (1, 0), Point<float> (0, -1), Point<float> (0, 1) }) g.drawText (it.first, r.withTrimmedLeft (12).translated (d.x, d.y), Justification::centredLeft, false); }
            drawLegend (g, it.first, r.withTrimmedLeft (12), f, sel ? th.keyLight : tc.withAlpha (0.85f), Justification::centredLeft);
            itemR.push_back (r.toNearestInt()); y += r.getHeight() + 4;
        }
    }
    void mouseDown (const MouseEvent& e) override
    {
        if (arrangeMods (e)) return;
        if (! box().contains (e.getPosition())) { setVisible (false); return; }
        for (size_t i = 0; i < itemR.size(); ++i) if (items[i].second >= 0 && itemR[i].contains (e.getPosition())) { show (items[i].second); return; }
    }
    int page() const { return cur; }
private:
    Theme& th; SetupPanel& lookPage; HwPage& hwPage; Viewport view; CloseX closeX { th };
    std::vector<std::pair<String, int>> items; std::vector<Rectangle<int>> itemR; int cur = 0;
};

struct FlexLnf : public LookAndFeel_V4
{
    const Theme* th = nullptr;
    float sp() const { return th ? th->tpt ("setup text", th->setupPt) : 13.0f; }
    Font getPopupMenuFont() override { return ui (jmax (13.5f, sp()), W::Medium); }
    Font getComboBoxFont (ComboBox&) override { return ui (sp(), W::Medium); }
    Font getTextButtonFont (TextButton&, int) override { return ui (sp(), W::Bold); }
    Font getLabelFont (Label&) override { return ui (sp(), W::Medium); }
};

// ============================================================================ the editor
class Flexiverb480Editor : public AudioProcessorEditor, private Timer
{
public:
    friend int main (int, char**);          // the screenshot test renders the windows and the hover zoom
    static constexpr int kL = 780, kH = 1060, kMinL = 640, kMinH = 840, kDrawer = 470, kSetup = 340, kMaxFaders = 56;
    int mainW = kL, mainH = kH;          // the main panel, in layout units: free to resize either way

    explicit Flexiverb480Editor (Flexiverb480Processor& p) : AudioProcessorEditor (p), proc (p), decay (theme, p), delayView (theme, p), reverbView (theme), browser (theme, p), setup (theme, p)
    {
        if (! theme.fromString (proc.getUiProperty ("themeData", String()).toString())) theme.fromString (Theme::defaultFile().loadFileAsString());
        scale = (float) (double) proc.getUiProperty ("uiScale", 0.8);
        drawerOpen = (bool) proc.getUiProperty ("uiDrawer", false);
        mainW = jmax (kMinL, (int) proc.getUiProperty ("uiW", kL)); mainH = jmax (kMinH, (int) proc.getUiProperty ("uiH", kH));

        addAndMakeVisible (content); content.ed = this;
        // screen
        content.addAndMakeVisible (screen); screen.ed = this;
        screen.addAndMakeVisible (reverbView); screen.addAndMakeVisible (decay); screen.addChildComponent (delayView);
        screen.addAndMakeVisible (eqGraph); eqGraph.onShow = [this] (const String& a, const String& b) { showMessage (a, b); };
        // dividers: drag them to give the waterfall, the decay graph, the EQ graph and the faders more or less room
        for (int i = 0; i < 2; ++i) { screen.addAndMakeVisible (scrDiv[i]); scrDiv[i].onDrag = [this, i] (int dy) { dragScreenDiv (i, dy); }; }
        content.addAndMakeVisible (faderDiv); faderDiv.onDrag = [this] (int dy) { const int h = (int) proc.getUiProperty ("uiScrH", 0) > 0 ? (int) proc.getUiProperty ("uiScrH", 0) : screen.getHeight();
                                                                               proc.setUiProperty ("uiScrH", jlimit (200, mainH - 330, h + dy)); layout(); content.repaint(); };
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
        textHook() = [sp = Component::SafePointer<Flexiverb480Editor> (this)] (const String& id, const String& label, float defPt, Colour defCol, Component* at, Rectangle<int> area)
        {
            if (sp == nullptr) return;
            auto ed = std::make_unique<TextStyleEditor> (sp->theme, id, label, defPt, defCol);
            ed->changed = [sp] { if (sp != nullptr) sp->lookChanged(); };
            CallOutBox::launchAsynchronously (std::move (ed), at->localAreaToGlobal (area), nullptr);
        };
        reverbView.getBars = [this]
        {
            std::vector<ReverbView::Bar> v;
            float ms[64], db[64]; const int n = proc.baseTaps (machine, ms, db, 64);
            const auto& ed = proc.tapEd[machine];
            for (int k = 0; k < n; ++k)
            {
                if (db[k] < -100.0f) continue;                       // a slot this program does not use
                const auto e = k < (int) ed.size() ? ed[(size_t) k] : Flexiverb480Processor::TapEd {};
                v.push_back ({ ms[k] + e.ms, db[k] + (e.db <= -79.0f ? 0.0f : e.db), false, k, e.db <= -79.0f });
            }
            const auto& r = proc.refl[machine];
            for (size_t j = 0; j < r.size(); ++j) v.push_back ({ r[j].ms, r[j].db, true, (int) j, false });
            return v;
        };
        reverbView.onAdd = [this] (float ms, float db)
        {
            auto r = proc.refl[machine]; if (r.size() >= 32) return;
            r.push_back ({ ms, db, (r.size() % 2 == 0) ? -0.7f : 0.7f }); proc.setRefl (machine, r);
            showMessage ("Your reflection", String (ms, 1) + " ms   " + String (db, 1) + " dB");
        };
        reverbView.onToggle = [this] (const ReverbView::Bar& b)
        {
            if (b.own) { auto r = proc.refl[machine]; if (b.idx < (int) r.size()) { r.erase (r.begin() + b.idx); proc.setRefl (machine, r); } return; }
            auto e = proc.tapEd[machine]; if ((int) e.size() <= b.idx) e.resize ((size_t) b.idx + 1);
            e[(size_t) b.idx].db = e[(size_t) b.idx].db <= -79.0f ? 0.0f : -80.0f; proc.setTapEdits (machine, e);
            showMessage ("Reflection " + String (b.idx + 1), e[(size_t) b.idx].db <= -79.0f ? "off" : "on");
        };
        reverbView.onDragBegin = [this] (const std::vector<ReverbView::Bar>& v) { dragBars = v; dragTaps = proc.tapEd[machine]; dragRefl = proc.refl[machine]; };
        reverbView.onDrag = [this] (float dMs, float dDb)
        {
            auto e = dragTaps; auto r = dragRefl;
            for (auto& b : dragBars)
            {
                if (b.own) { if (b.idx < (int) r.size()) { r[(size_t) b.idx].ms = jlimit (0.1f, 300.0f, dragRefl[(size_t) b.idx].ms + dMs); r[(size_t) b.idx].db = jlimit (-60.0f, 6.0f, dragRefl[(size_t) b.idx].db + dDb); } continue; }
                if ((int) e.size() <= b.idx) e.resize ((size_t) b.idx + 1);
                const auto o = b.idx < (int) dragTaps.size() ? dragTaps[(size_t) b.idx] : Flexiverb480Processor::TapEd {};
                e[(size_t) b.idx].ms = jlimit (-100.0f, 200.0f, o.ms + dMs);
                if (o.db > -79.0f) e[(size_t) b.idx].db = jlimit (-40.0f, 24.0f, o.db + dDb);
            }
            proc.setTapEdits (machine, e); proc.setRefl (machine, r);
            if (dragBars.size() == 1) showMessage (dragBars[0].own ? "Your reflection" : "Reflection " + String (dragBars[0].idx + 1),
                                                   String (dragBars[0].ms + dMs, 1) + " ms   " + String (dragBars[0].db + dDb, 1) + " dB");
            else showMessage (String ((int) dragBars.size()) + " reflections", (dMs >= 0 ? "+" : "") + String (dMs, 1) + " ms   " + (dDb >= 0 ? "+" : "") + String (dDb, 1) + " dB");
        };
        reverbView.onDragEnd = [this] { dragBars.clear(); };
        // turning the picture changes the sound: tail left/right = decay time, tilt = brighter/darker,
        // twist (shift) = room size; the reflections alone: left/right = predelay, tilt = density (diffusion)
        reverbView.onTurn = [this] (int what, float dx, float dy, bool twist, int phase) { turnSound (what, dx, dy, twist, phase); };
        reverbView.onTurnReset = [this] { resetTurn(); };
        reverbView.onOpenWindow = [this] (int m) { openViewWindow (m); };
        reverbView.setWantsKeyboardFocus (true);
        holdRev.ed = this; holdRev.busy = [this] { return reverbView.busy(); }; reverbView.addMouseListener (&holdRev, false);
        holdDelay.ed = this; holdDelay.busy = [this] { return delayView.busy(); }; delayView.addMouseListener (&holdDelay, false);

        // preset bar, page and algorithm keys
        auto mk = [this] (const String& t) { auto k = std::make_unique<LarcKey> (theme, t); k->setComponentID ("mv:main:" + t); content.addAndMakeVisible (*k); return k; };
        content.addAndMakeVisible (presetName); presetName.ed = this; presetName.setComponentID ("mv:presetname");
        prevKey = mk ("<"); nextKey = mk (">"); saveKey = mk ("SAVE"); panelKey = mk ("PANEL >");
        for (auto* k : { prevKey.get(), nextKey.get(), saveKey.get(), panelKey.get() }) k->kc = &Theme::keyBar;
        prevKey->onClick = [this] { stepPreset (-1); }; nextKey->onClick = [this] { stepPreset (1); }; saveKey->onClick = [this] { savePreset(); };
        panelKey->onClick = [this] { togglePanel (drawerOpen, kDrawer); proc.setUiProperty ("uiDrawer", drawerOpen); };
        pageKey = mk ("REVERB"); pageKey->onClick = [this] { page = 1 - page; refreshPage (true); };
        struct Alg { const char* name; int bank, num; };
        static const Alg algs[] = { { "HALL", 1, 1 }, { "RANDOM HALL", 11, 1 }, { "ROOM", 2, 2 }, { "CHAMBER", 2, 8 }, { "PLATE", 4, 1 }, { "AMBIENCE", 13, 2 },
                                    { "INVERSE", 3, 8 }, { "DELAY", 6, 2 }, { "CHORUS", 20, 2 }, { "PITCH", 8, 2 } };
        for (auto& a : algs) { auto k = mk (a.name); int pi = l480::findProgram (a.bank, a.num);
                                if (isMatchedOriginal (pi)) for (int i = 0; i < l480::numPrograms(); ++i)   // the matched version of it
                                { const auto& q = l480::programs()[(size_t) i]; if (q.bank >= 27 && q.bank <= 31 && presetKey (q.name) == presetKey (l480::programs()[(size_t) pi].name)) { pi = i; break; } } k->onClick = [this, pi] { proc.loadProgram (machine, pi); page = 0; refreshPage (true); };
                                auto* kp = k.get(); k->onRightClick = [this, kp] { keyMenu (-1, kp); }; browser.algorithms.push_back ({ a.name, pi }); algKeys.push_back (std::move (k)); }
        pageKey->onRightClick = [this] { keyMenu (-1, pageKey.get()); };
        loadMyKeys();
        // DELAY and CHORUS: right-click adds them to the reverb (machine B) instead of replacing it
        for (auto& k : algKeys)
            if (k->getText() == "DELAY" || k->getText() == "CHORUS")
            {
                auto* kp = k.get(); const int pi = l480::findProgram (k->getText() == "DELAY" ? 6 : 20, 2);
                kp->onRightClick = [this, kp, pi] { addOnMenu (pi, kp); };
            }

        // the remote: display, REG / PROG, keypad, function keys, slider display keys - the 480L's own
        content.addAndMakeVisible (larcMain); larcMain.ed = this;
        regKey = mk ("REG"); progKey = mk ("PROG");
        regKey->onClick = [this] { pressReg(); }; progKey->onClick = [this] { pressProg(); };
        for (int i = 0; i < 10; ++i) { numKey[i] = mk (String ((i + 1) % 10)); numKey[i]->onClick = [this, i] { pressNumber (i == 9 ? 0 : i + 1); }; }
        const char* fn[] = { "BANK", "VAR", "STO", "CTRL", "MACH", "MUTE", "PAGE", "2nd F" };
        for (int i = 0; i < 8; ++i) fnKey[i] = mk (fn[i]);
        fnKey[0]->onClick = [this] { pressBank(); };
        fnKey[1]->onClick = [this] { setMode (Mode::Normal); showMessage (progText (machine), String (l480::algoDef (proc.algoOf (machine)).name) + "  PAGE " + String (page + 1) + " OF " + String (pages())); };
        fnKey[2]->onClick = [this] { pressSto(); };
        fnKey[3]->onClick = [this] { togglePanel (drawerOpen, kDrawer); proc.setUiProperty ("uiDrawer", drawerOpen); };
        fnKey[4]->onClick = [this] { pressMach(); };
        fnKey[5]->onPress = [this] { proc.mute.store (true); }; fnKey[5]->onRelease = [this] { proc.mute.store (false); };
        fnKey[6]->onClick = [this] { pressPage(); };
        fnKey[7]->onClick = [this] { showMessage ("2ND F", "NOT USED ON THE FLEXI 180V"); };
        for (int i = 0; i < 6; ++i) { sdKey[i] = mk (""); sdKey[i]->onClick = [this, i] { pressSlotKey (i); }; }

        for (int i = 0; i < kMaxFaders; ++i)
        {
            auto f = std::make_unique<LarcFader> (theme); content.addChildComponent (*f);
            f->onBegin = [this, i] { faderBegin (i); }; f->onMove = [this, i] (float t) { faderMove (i, t); };
            f->onEnd = [this, i] { faderEnd (i); }; f->onReset = [this, i] { faderReset (i); };
            faders.push_back (std::move (f));
        }
        mixFader = std::make_unique<LarcFader> (theme); content.addAndMakeVisible (*mixFader);
        mixFader->onBegin = [this] { targetGesture (mixTg, true); };
        mixFader->onMove = [this] (float t) { setTargetNorm (mixTg, t); showMessage (mixTg.name, targetText (mixTg)); };
        mixFader->onEnd = [this] { targetGesture (mixTg, false); };
        mixFader->onReset = [this]
        {
            if (mixTg.param) { mixTg.param->beginChangeGesture(); mixTg.param->setValueNotifyingHost (1.0f); mixTg.param->endChangeGesture(); return; }
            if (mixTg.slot < 0) return;
            const auto& p = proc.prog (machine); const auto& sp = proc.spec (machine, mixTg.slot);
            proc.beginSlot (machine, mixTg.slot); proc.setSlot (machine, mixTg.slot, l480::toNorm (sp, l480::programValue (p, l480::realIndex (p.algo, mixTg.slot)))); proc.endSlot (machine, mixTg.slot);
        };

        // drawer (scrolls when longer than the window)
        drawer.addAndMakeVisible (vu); vu.setComponentID ("mv:vu"); gateMeter.setComponentID ("mv:gatemeter");
        content.addAndMakeVisible (drawerView); drawerView.setViewedComponent (&drawer, false); drawerView.setScrollBarsShown (false, false);
        drawer.ed = this;
        auto dk = [this] (const String& t) { auto k = std::make_unique<LarcKey> (theme, t); k->kc = &Theme::keyPanel; k->setComponentID ("mv:key:" + t); drawer.addAndMakeVisible (*k); return k; };
        auto pk = [this] (const String& id, const String& t) { auto k = std::make_unique<ParamKey> (theme, proc.apvts, id, t); k->kc = &Theme::keyPanel; k->setComponentID ("mv:key:" + id); drawer.addAndMakeVisible (*k); return k; };
        auto ck = [this] (const String& id, const String& t) { auto k = std::make_unique<CycleKey> (theme, proc.apvts, id, t); k->kc = &Theme::keyPanel; k->setComponentID ("mv:key:" + id); drawer.addAndMakeVisible (*k); return k; };
        // ENGINE: single (machine A) or dual (machines A and B side by side)
        drawer.addAndMakeVisible (engineBox); engineBox.addItemList ({ "Single", "Dual" }, 1);
        drawer.addAndMakeVisible (anOsBox); anOsBox.addItemList ({ "Oversampling Off", "Oversampling 2x", "Oversampling 4x", "Oversampling 6x", "Oversampling 8x" }, 1);
        anOsAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "os", anOsBox);
        engineBox.onChange = [this] { auto* c = proc.apvts.getParameter ("config"); const int v = engineBox.getSelectedItemIndex() == 0 ? 0 : 2;
                                      if ((config() != 0) != (v != 0)) { c->beginChangeGesture(); c->setValueNotifyingHost (c->convertTo0to1 ((float) v)); c->endChangeGesture(); } };
        drawer.addAndMakeVisible (drawerX); drawerX.onClick = [this] { if (drawerOpen) togglePanel (drawerOpen, kDrawer); proc.setUiProperty ("uiDrawer", false); };
        machAKey = dk ("MACHINE A"); machBKey = dk ("MACHINE B"); muteKey = dk ("MUTE"); setupKey = dk ("SETUP");
        machAKey->onClick = [this] { selectMachine (0); }; machBKey->onClick = [this] { selectMachine (1); };
        muteKey->onClick = [this] { proc.mute.store (! proc.mute.load()); };
        setupKey->onClick = [this] { openPrefs(); };
        ironKey = pk ("trOn", "IRON");
        anOnKey = pk ("anOn", "ANALOG"); eqOnKey = pk ("eqOn", "EQ"); cpOnKey = pk ("cpOn", "COMP"); satKey = pk ("cpColor", "SAT"); duckKey = pk ("cpDuck", "DUCK");
        gtOnKey = pk ("gtOn", "GATE"); gtKeyKey = pk ("gtKey", "KEY: INPUT");
        orderKey = std::make_unique<LarcKey> (theme, "EQ > COMP"); orderKey->kc = &Theme::keyPanel; drawer.addAndMakeVisible (*orderKey);
        orderKey->onClick = [this] { auto* p = proc.apvts.getParameter ("stripOrder"); p->beginChangeGesture(); p->setValueNotifyingHost (stripOrder() == 1 ? 0.0f : 1.0f); p->endChangeGesture(); layoutDrawer(); drawer.repaint(); };
        drawer.addAndMakeVisible (gateMeter);
        auto knob = [this, shower] (const String& id, const String& t, std::vector<ScaleMark> m, Colour Theme::* c)
        { auto k = std::make_unique<ScaleKnob> (theme, proc.apvts, id, t, std::move (m), c); k->onShow = shower; k->setComponentID ("mv:knob:" + id); drawer.addAndMakeVisible (*k); return k; };
        sysKnob[0] = knob ("out", "OUTPUT", numberScale (-24, 12, 6, 2, true), &Theme::compKnob);
        sysKnob[1] = knob ("trModel", "IRON MODEL", [] { auto m = numberScale (1, (float) pv::kNumTransformers, 1, 1); for (auto& x : m) x.value -= 1; return m; }(), &Theme::compKnob);
        sysKnob[2] = knob ("trFreq", "IRON FREQ", { { 20, "20" }, { 30, "30" }, { 50, "50" }, { 80, "80" }, { 120, "120" }, { 200, "200" }, { 400, "400" } }, &Theme::compKnob);
        depthKnob = knob ("loopAmt", "180V DEPTH", { { 0, "0" }, { 0.5f, "50" }, { 1, "100" }, { 1.5f, "150" }, { 2, "200" } }, &Theme::compKnob);
        sysKnob[3] = knob ("trAmt", "IRON AMOUNT", { { 0, "0" }, { 0.2f, "20" }, { 0.4f, "40" }, { 0.6f, "60" }, { 0.8f, "80" }, { 1, "100" } }, &Theme::compKnob);
        for (auto* k : { sysKnob[0].get(), sysKnob[1].get(), sysKnob[2].get(), sysKnob[3].get(), depthKnob.get() }) k->strip = true;
        anKnob[0] = knob ("anDrive", "DRIVE", numberScale (0, 30, 4, 2), &Theme::analogKnob);
        anKnob[1] = knob ("anOut", "OUTPUT", numberScale (-12, 12, 4, 2, true), &Theme::analogKnob);
        anKnob[2] = knob ("anModel", "MODEL", { { 0, "CLEAN" }, { 1, "TUBE" }, { 2, "1073" }, { 3, "1080" }, { 4, "212" } }, &Theme::analogKnob);
        anKnob[3] = knob ("anColour", "CHARACTER", { { 0, "ODD" }, { 1, "EVEN" }, { 2, "HEAVY" }, { 3, "CRUSH" } }, &Theme::analogKnob);
        anKnob[4] = knob ("anTarget", "AFFECTS", { { 0, "REVERB" }, { 1, "EQ" }, { 2, "BOTH" } }, &Theme::analogKnob);
        for (auto& k : anKnob) { k->sizeMul = &theme.analogSize; k->strip = true; }
        eqKnob2[0] = knob ("eqModel", "EQ MODEL", { { 0, "CLEAN" }, { 1, "73" }, { 2, "73/84" }, { 3, "4K" }, { 4, "4K U" }, { 5, "TUBE" }, { 6, "550" } }, &Theme::eqKnob);
        eqKnob2[1] = knob ("eqDrive", "EQ COLOUR", { { 0, "0" }, { 0.2f, "20" }, { 0.4f, "40" }, { 0.6f, "60" }, { 0.8f, "80" }, { 1, "100" } }, &Theme::eqKnob);
        const char* bn[] = { "LOW", "LOW MID", "HIGH MID", "HIGH" };
        const std::vector<ScaleMark> fAll { { 20, "20" }, { 50, "50" }, { 100, "100" }, { 200, "200" }, { 500, "500" }, { 1000, "1k" }, { 2000, "2k" }, { 5000, "5k" }, { 10000, "10k" }, { 20000, "20k" } };
        const std::vector<ScaleMark> fm[4] = { fAll, fAll, fAll, fAll };     // every band is free over the whole range
        for (int b = 0; b < 4; ++b) { eqKnob[b] = std::make_unique<DualKnob> (theme, proc.apvts, "eqF" + String (b), "eqG" + String (b), bn[b], fm[b]); eqKnob[b]->onShow = shower; eqKnob[b]->setComponentID ("mv:eq" + String (b)); drawer.addAndMakeVisible (*eqKnob[b]); }
        gtKnob[0] = knob ("gtThr", "THRESHOLD", numberScale (-80, 0, 20, 2), &Theme::compKnob);
        gtKnob[1] = knob ("gtRange", "RANGE", numberScale (0, 80, 20, 2), &Theme::compKnob);
        gtKnob[2] = knob ("gtRel", "RELEASE", { { 5, "5" }, { 20, "20" }, { 50, "50" }, { 150, "150" }, { 500, "500" }, { 2000, "2k" } }, &Theme::compKnob);
        cpKnob[0] = knob ("cpType", "TYPE", { { 0, "VCA" }, { 1, "FET" }, { 2, "OPTO" } }, &Theme::compKnob);
        cpKnob[1] = knob ("cpThr", "THRESHOLD", numberScale (-60, 0, 10, 2), &Theme::compKnob);
        cpKnob[2] = knob ("cpRatio", "RATIO", { { 1, "1" }, { 1.5f, "1.5" }, { 2, "2" }, { 3, "3" }, { 4, "4" }, { 6, "6" }, { 10, "10" }, { 20, "20" } }, &Theme::compKnob);
        cpKnob[3] = knob ("cpAtk", "ATTACK", { { 0.05f, ".05" }, { 0.2f, ".2" }, { 1, "1" }, { 3, "3" }, { 10, "10" }, { 30, "30" }, { 100, "100" }, { 300, "300" } }, &Theme::compKnob);
        cpKnob[4] = knob ("cpRel", "RELEASE", { { 20, "20" }, { 50, "50" }, { 100, "100" }, { 200, "200" }, { 500, "500" }, { 1000, "1k" }, { 2000, "2k" }, { 5000, "5k" } }, &Theme::compKnob);
        cpKnob[5] = knob ("cpMakeup", "MAKEUP", numberScale (0, 24, 4, 2), &Theme::compKnob);
        for (auto* k : { anKnob[0].get(), anKnob[1].get(), sysKnob[0].get() }) k->step = 2.0f;
        cpKnob[6] = knob ("cpMix", "MIX", { { 0, "0" }, { 0.2f, "20" }, { 0.4f, "40" }, { 0.6f, "60" }, { 0.8f, "80" }, { 1, "100" } }, &Theme::compKnob);
        for (auto* k : { gtKnob[0].get(), gtKnob[1].get(), gtKnob[2].get(), eqKnob2[0].get(), eqKnob2[1].get(), cpKnob[0].get(), cpKnob[1].get(), cpKnob[2].get(), cpKnob[3].get(), cpKnob[4].get(), cpKnob[5].get(), cpKnob[6].get() }) k->strip = true;
        cpDual[0] = std::make_unique<DualKnob> (theme, proc.apvts, "cpThr", "cpRatio", "THRESHOLD / RATIO", numberScale (-60, 0, 10, 2),
                        std::vector<ScaleMark> { { 1, "1" }, { 2, "2" }, { 4, "4" }, { 10, "10" }, { 20, "20" } }, "Comp threshold", "Comp ratio", &Theme::compKnob);
        cpDual[1] = std::make_unique<DualKnob> (theme, proc.apvts, "cpAtk", "cpRel", "ATTACK / RELEASE",
                        std::vector<ScaleMark> { { 0.05f, ".05" }, { 0.2f, ".2" }, { 1, "1" }, { 3, "3" }, { 10, "10" }, { 30, "30" }, { 100, "100" }, { 300, "300" } },
                        std::vector<ScaleMark> { { 20, "20" }, { 100, "100" }, { 500, "500" }, { 2000, "2k" }, { 5000, "5k" } }, "Comp attack", "Comp release", &Theme::compKnob);
        for (auto& d : cpDual) { d->onShow = shower; drawer.addAndMakeVisible (*d); }
        for (int i = 1; i <= 4; ++i) cpKnob[i]->setVisible (false);   // now inside the two nested knobs
        for (LarcKey* k : std::initializer_list<LarcKey*> { gtOnKey.get(), gtKeyKey.get(), eqOnKey.get(), cpOnKey.get(), satKey.get(), duckKey.get(), orderKey.get() }) k->slim = true;

        cpKnob[1]->step = 2.0f; cpKnob[5]->step = 2.0f;
        // CLEAN OUT: plain gain, always last
        cleanKnob = knob ("cleanOut", "CLEAN OUT", numberScale (-24, 12, 6, 2, true), &Theme::compKnob); cleanKnob->strip = true; cleanKnob->step = 2.0f;
        // EQ MODEL: a menu; each model at its own hardware colour
        eqModelLab = dk ("EQ MODEL"); eqModelLab->textOnly = true; eqModelLab->setComponentID ("mv:label:eqmodel");
        drawer.addAndMakeVisible (eqModelBox); eqModelBox.setComponentID ("mv:eqmodel");
        for (int i = 1; i < cs::kNumEqModels; ++i) eqModelBox.addItem (cs::eqModelName (i), i + 1);
        eqModelBox.onChange = [this] { const int m = eqModelBox.getSelectedId() - 1; if (m < 1) return; auto* p = proc.apvts.getParameter ("eqModel");
                                       if ((int) proc.apvts.getRawParameterValue ("eqModel")->load() != m) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 ((float) m)); p->endChangeGesture(); } };
        // the gear wheels: PREFERENCES (always visible at the top, and on the strip)
        content.addAndMakeVisible (topGear); topGear.setComponentID ("mv:gear"); topGear.onClick = [this] { openPrefs(); };
        drawer.addAndMakeVisible (stripGear); stripGear.setComponentID ("mv:gear2"); stripGear.onClick = [this] { openPrefs(); };
        content.addChildComponent (prefs);
        prefs.onSync = [this] { setup.sync(); hwPage.sync(); };
        setup.onResetPositions = [this] { resetMoves(); };
        content.addChildComponent (setupView);
        setup.onAddKey = [this] { addMyKey(); }; setup.onClose = [this] { if (setupOpen) togglePanel (setupOpen, kSetup); }; setup.onChange = [this] { lookChanged(); };
        setup.onRightClick = [this] (Point<int> p) { editText ("setup text", "All the text in SETUP", theme.setupPt, theme.panelText, &setup, Rectangle<int> (p.x - 40, p.y - 10, 80, 20)); };
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
        lnf.th = &theme; setLookAndFeel (&lnf);
        setResizable (true, true);
        layout(); applySize(); refreshPage (true);
        startTimerHz (30);
        addMouseListener (&arranger, true);
    }
    ~Flexiverb480Editor() override { removeMouseListener (&arranger); textHook() = nullptr; stopTimer(); setLookAndFeel (nullptr); }

    void paint (Graphics& g) override { g.setGradientFill (theme.grad (&Theme::cheek, getLocalBounds().toFloat(), 0.6f)); g.fillAll(); }
    void resized() override
    {
        // dragging the corner changes the panel's width and height (more faders, a longer waterfall);
        // the size menu changes the scale of everything
        if (! applying)
        {
            const int w = jmax (kMinL, roundToInt (getWidth() / scale) - (drawerOpen ? kDrawer : 0) - (setupOpen ? kSetup : 0));
            const int h = jmax (kMinH, roundToInt (getHeight() / scale));
            if (w != mainW || h != mainH)
            {
                const bool cols = pagesPerView() != jlimit (1, pages(), jmax (6, (w - 88) / jmax (82, roundToInt (theme.faderPt * 6.6f))) / 6);
                mainW = w; mainH = h; proc.setUiProperty ("uiW", mainW); proc.setUiProperty ("uiH", mainH);
                placePanels();
                if (cols) refreshPage (true); else layout();
            }
        }
        content.setTransform (AffineTransform::scale (scale));
    }
    void placePanels()
    {
        const int w = baseWidth();
        content.setBounds (0, 0, w, mainH);
        drawerView.setBounds (mainW, 0, kDrawer, mainH); drawerView.setVisible (drawerOpen);
        for (int i = 0; i < 3; ++i)
        {
            auto& h = holders[i]; h.ed = this; h.idx = i;
            if (h.getParentComponent() == nullptr) content.addChildComponent (h);
            h.setTransform ({}); h.zoomed = false;
            if (i < (int) sections.size()) h.setBounds (sections[(size_t) i].area + drawerView.getPosition());
            h.setVisible (drawerOpen && i < (int) sections.size()); h.toFront (false);
        }
        setupView.setVisible (false);
        prefs.setBounds (0, 0, w, mainH);
        browser.setBounds (0, 0, w, mainH); browser.resized();
    }
    // opening the panel or SETUP takes room from the main panel while it can; closing gives it back
    void togglePanel (bool& flag, int width)
    {
        flag = ! flag;
        if (flag && mainW - width >= kMinL) mainW -= width;
        else if (! flag && getWidth() > 0) mainW += width;
        proc.setUiProperty ("uiW", mainW);
        refreshPage (true); applySize();
    }

    // ---- the three modules of the panel: each grows (hover zoom, SETUP) while the mouse is over it
    struct ModuleHolder : public Component, private Timer
    {
        Flexiverb480Editor* ed = nullptr; int idx = 0; bool zoomed = false;
        ModuleHolder() { startTimerHz (15); }
        void paint (Graphics& g) override { if (ed) ed->paintModule (g, idx, zoomed); }
        void timerCallback() override
        {
            if (ed == nullptr || ! isVisible()) return;
            const bool over = isMouseOverOrDragging (true) || isMouseButtonDown (true);
            if (over != zoomed) ed->setZoom (*this, over);
        }
    };
    ModuleHolder holders[3];
    void setZoom (ModuleHolder& h, bool on)
    {
        h.zoomed = on;
        const float sc = 1.0f + jlimit (0.0f, 60.0f, theme.hoverZoom) / 100.0f;
        if (! on || sc <= 1.001f || h.getHeight() > mainH * 0.7f) { h.setTransform ({}); h.repaint(); return; }
        // grow to the left (into the main window), downwards for the top modules, upwards for the strip
        const float px = (float) h.getRight(), py = h.idx == 2 ? (float) h.getBottom() : (float) h.getY();
        h.setTransform (AffineTransform::scale (sc, sc, px, py));
        h.toFront (false); h.repaint();
    }
    void paintModule (Graphics& g, int i, bool zoomed)
    {
        if (i >= (int) sections.size()) return;
        const auto& s = sections[(size_t) i];
        auto b = s.area.withZeroOrigin().toFloat();
        if (zoomed) { g.setColour (Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (b, 12); }
        g.setGradientFill (theme.grad (&Theme::larcRim, b, 0.8f)); g.fillRoundedRectangle (b, 12);
        auto f2 = b.reduced (5);
        g.setGradientFill (theme.grad (&Theme::larc, f2, 0.5f)); g.fillRoundedRectangle (f2, 8);
        if (zoomed) { g.setColour (theme.keyLight.withAlpha (0.8f)); g.drawRoundedRectangle (b.reduced (1), 12, 1.5f); }
        if (s.title != "ANALOG")
        {
            const String tid = "section:" + s.title;
            const auto f = ui (theme.tpt (tid, jmax (10.0f, theme.knobPt * 1.0f)), W::Bold, 0.18f);
            drawLegend (g, s.title, Rectangle<float> (10, 6, textW (f, s.title) + 10, (float) keyH()), f, theme.tcol (tid, theme.panelText.withAlpha (0.9f)), Justification::centredLeft);
        }
        if (s.title == "SYSTEM")
            drawLegend (g, "ENGINE", (engineLabelR - s.area.getPosition()).toFloat(), ui (theme.tpt ("label:ENGINE", jmax (9.0f, theme.keyPt * 0.9f)), W::Bold, 0.08f), theme.tcol ("label:ENGINE", theme.panelText), Justification::centredLeft);
    }
    struct Content : public Component
    {
        Flexiverb480Editor* ed = nullptr;
        void paint (Graphics& g) override { if (ed) ed->paintPanel (g); }
        void mouseDown (const MouseEvent& e) override { if (ed && e.mods.isPopupMenu()) ed->panelRightClick (e.getPosition()); }
    };
    struct Drawer : public Component
    {
        Flexiverb480Editor* ed = nullptr;
        void paint (Graphics& g) override { if (ed) ed->paintDrawer (g); }
        void mouseDown (const MouseEvent& e) override { if (ed && e.mods.isPopupMenu()) ed->drawerRightClick (e.getPosition()); }
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
            if (false)      // the remote's display shows the status now
            {
                g.setColour (th.displayText); g.setFont (ui (th.displayPt, W::Bold));
                g.drawText (ed->msg1 + "   " + ed->msg2, in.reduced (12, 6).withHeight (th.displayPt + 6), Justification::centredLeft, true);
            }
        }
    };
    // the remote's main display: two lines of 14-segment characters and the L / R headroom meters
    struct LarcMain : public Component
    {
        Flexiverb480Editor* ed = nullptr; String l1, l2; float mL = 0, mR = 0;
        void paint (Graphics& g) override { ed->paintLarcMain (g, *this); }
        void mouseDown (const MouseEvent& e) override
        {
            if (e.mods.isPopupMenu()) editText ("larc display", "Remote display", ed->larcPt(), ed->theme.larcText, this, getLocalBounds());
            else if (e.x < getWidth() - 70) ed->openBrowser();
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
            g.setColour (th.tcol ("preset category", th.displayText.withAlpha (0.6f))); g.setFont (ui (th.tpt ("preset category", jmin (12.0f, th.keyPt)), W::Medium));
            g.drawText (sub + "   v", t, Justification::centredRight, true);
            g.setColour (th.tcol ("preset name", th.displayText)); g.setFont (ui (th.tpt ("preset name", jmin (17.0f, th.keyPt * 1.3f)), W::Bold));
            g.drawText (text, t, Justification::centredLeft, true);
        }
        void mouseEnter (const MouseEvent&) override { repaint(); }
        void mouseExit (const MouseEvent&) override { repaint(); }
        void mouseDown (const MouseEvent& e) override
        {
            if (! e.mods.isPopupMenu()) { ed->openBrowser(); return; }
            const auto& th = ed->theme;
            if (e.x > getWidth() * 0.6f) editText ("preset category", "Preset category", jmin (12.0f, th.keyPt), th.displayText, this, getLocalBounds());
            else editText ("preset name", "Preset name", jmin (17.0f, th.keyPt * 1.3f), th.displayText, this, getLocalBounds());
        }
    };

private:
    // ---------------------------------------------------------------- painting
    void paintPanel (Graphics& g)
    {
        auto face = Rectangle<float> (0, 0, (float) mainW, (float) mainH);
        g.setGradientFill (theme.grad (&Theme::panel, face, 0.45f));
        g.fillRect (face);
        for (auto sr : { face.withWidth (12.0f), face.withLeft (face.getRight() - 12.0f) })
        {
            g.setGradientFill (theme.grad (&Theme::cheek, sr, 0.8f)); g.fillRect (sr);
            g.setColour (Colours::white.withAlpha (0.06f * theme.d3())); g.drawVerticalLine ((int) sr.getX() + 1, sr.getY(), sr.getBottom());
            g.setColour (Colours::black.withAlpha (0.5f)); g.drawVerticalLine ((int) sr.getRight() - 1, sr.getY(), sr.getBottom());
        }
        // the fader section: the remote's face in its rim
        const auto lr = larcR.toFloat();
        g.setColour (Colours::black.withAlpha (theme.shadow())); g.fillRoundedRectangle (lr.translated (0, 3.0f * theme.d3()), 12);
        g.setGradientFill (theme.grad (&Theme::larcRim, lr, 0.8f)); g.fillRoundedRectangle (lr, 12);
        const auto lf = lr.reduced (6.0f);
        g.setGradientFill (theme.grad (&Theme::larc, lf, 0.5f)); g.fillRoundedRectangle (lf, 8);
        g.setColour (Colours::white.withAlpha (0.08f * theme.d3())); g.drawRoundedRectangle (lf.translated (0, 1), 8, 1.0f);
        g.setColour (Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (lf, 8, 1.0f);
        // one recessed plate per page of six, its number small at the top
        for (auto& bay : bays)
        {
            const auto b = bay.area.toFloat();
            g.setColour (Colours::black.withAlpha (0.18f)); g.fillRoundedRectangle (b, 6);
            g.setColour (Colours::black.withAlpha (0.35f)); g.drawRoundedRectangle (b, 6, 1.0f);
            if (bays.size() > 1)
                drawLegend (g, bay.title, b.withHeight (12).translated (0, -13), ui (theme.tpt ("page label", 9.0f), W::Bold, 0.18f), theme.tcol ("page label", theme.larcLegend.withAlpha (0.45f)));
        }
        // REVERB MIX: its own bay at the right
        {
            const auto b = mixBay.toFloat();
            g.setColour (Colours::black.withAlpha (0.18f)); g.fillRoundedRectangle (b, 6);
            g.setColour (Colours::black.withAlpha (0.35f)); g.drawRoundedRectangle (b, 6, 1.0f);
            const String nid = "fader name:Reverb Mix", vid = "fader value:Reverb Mix";
            g.setColour (theme.tcol (nid, theme.larcLegend)); g.setFont (ui (theme.tpt (nid, theme.faderPt), W::Semibold));
            g.drawFittedText ("Reverb Mix", mixNameR, Justification::centredBottom, 2, 0.8f);
            const auto vr = mixValueR.toFloat();
            g.setColour (Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (vr, 3);
            g.setGradientFill (theme.grad (&Theme::display, vr.reduced (1.5f), 0.5f)); g.fillRoundedRectangle (vr.reduced (1.5f), 2);
            g.setColour (theme.tcol (vid, theme.larcText)); g.setFont (ui (theme.tpt (vid, theme.faderPt), W::Bold));
            g.drawFittedText (targetText (mixTg), mixValueR.reduced (3, 0), Justification::centred, 1, 0.6f);
        }
        // each fader: its full name, then its value in a little red window
        const bool hot = focus >= 0 && Time::getMillisecondCounter() - focusTime < 2500;
        for (int i = 0; i < nShown; ++i)
        {
            const auto& tg = pageTargets[(size_t) i];
            if (! tg.param && tg.slot < 0) continue;
            const String nid = "fader name:" + tg.name, vid = "fader value:" + tg.name;
            g.setColour (theme.tcol (nid, theme.larcLegend)); g.setFont (ui (theme.tpt (nid, theme.faderPt), W::Semibold));
            g.drawFittedText (tg.name, nameR[(size_t) i], Justification::centredBottom, 2, 0.8f);
            const auto vr = valueR[(size_t) i].toFloat();
            g.setColour (Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (vr, 3);
            g.setGradientFill (theme.grad (&Theme::display, vr.reduced (1.5f), 0.5f)); g.fillRoundedRectangle (vr.reduced (1.5f), 2);
            const Colour vc = theme.tcol (vid, theme.larcText);
            g.setColour (hot && focus == i ? vc.brighter (0.35f) : vc); g.setFont (ui (theme.tpt (vid, theme.faderPt), W::Bold));
            g.drawFittedText (tg.trigger ? String ("PRESS") : targetText (tg), valueR[(size_t) i].reduced (3, 0), Justification::centred, 1, 0.6f);
        }
    }
    float larcPt() const { return theme.tpt ("larc display", theme.displayPt * 1.45f); }
    float sdPt() const { return theme.tpt ("slider line", theme.faderPt * 1.1f); }
    void paintLarcMain (Graphics& g, LarcMain& c)
    {
        auto r = c.getLocalBounds().toFloat();
        g.setColour (Colours::black.withAlpha (0.6f + 0.3f * jmin (1.0f, theme.d3()))); g.fillRoundedRectangle (r, 6);
        auto in = r.reduced (3.0f); g.setGradientFill (theme.grad (&Theme::display, in, 0.5f)); g.fillRoundedRectangle (in, 4);
        auto meters = in.removeFromRight (62).reduced (6, 10);
        auto txt = in.reduced (12, 8);
        const float h = jmin (larcPt(), txt.getHeight() * 0.42f);
        const Colour col = theme.tcol ("larc display", theme.larcText);
        segLine (g, c.l1, txt.removeFromTop (txt.getHeight() * 0.5f), h, col);
        segLine (g, c.l2, txt, h, col);
        for (int ch = 0; ch < 2; ++ch)       // L / R headroom, 8 segments of 3 dB, the top one red
        {
            const float lv = 20.0f * std::log10 (jmax (1.0e-6f, ch == 0 ? c.mL : c.mR)) + 24.0f;
            auto colR = meters.withWidth (meters.getWidth() * 0.5f).translated (ch * meters.getWidth() * 0.5f, 0);
            const float segH = colR.getHeight() / 8.0f;
            for (int i = 0; i < 8; ++i)
            {
                auto sr = Rectangle<float> (colR.getX() + 4, colR.getBottom() - (i + 1) * segH + 1, colR.getWidth() - 8, segH - 2);
                const bool on = lv >= i * 3.0f; const Colour cc = i >= 7 ? Colour (0xffff2a1a) : col;
                g.setColour (on ? cc : cc.withAlpha (0.08f)); g.fillRect (sr);
                if (on) { g.setColour (cc.withAlpha (0.25f)); g.fillRect (sr.expanded (1.0f)); }
            }
        }
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.06f), r.getX(), r.getY(), Colours::transparentWhite, r.getX(), r.getY() + r.getHeight() * 0.45f, false));
        g.fillRoundedRectangle (r.reduced (3.0f).withHeight (r.getHeight() * 0.45f), 4);
    }
    String machineLegend() const
    {
        static const char* cfg[] = { "SINGLE", "CASCADE", "STEREO SPLIT", "MONO SPLIT" };
        static const char* add[] = { "", "+ B BESIDE", "+ B BEFORE", "+ B AFTER" };
        const String c = addMode() > 0 ? String (add[addMode()]) : String (cfg[jlimit (0, 3, config())]);
        return c + "    MACH " + (machine == 0 ? "A" : "B");
    }
    void paintDrawer (Graphics& g)
    {
        auto r = drawer.getLocalBounds().toFloat();
        g.setGradientFill (theme.grad (&Theme::panel, r.withHeight ((float) mainH), 0.45f));
        g.fillRect (r);
        titleR.clear();
    }

    // ---------------------------------------------------------------- layout
    struct Sec { String title; Rectangle<int> area; };
    std::vector<Sec> sections; Rectangle<int> faderArea; float labelH = 40;
    int keyH() const { return roundToInt (theme.keyPt * 2.0f + 12.0f + theme.keyHeight); }   // follows the text, plus your extra
    Rectangle<int> larcR, sdLineR, brandR;
    int faderCols() const { return jmax (6, (mainW - 48 - 40 - mixColW()) / jmax (82, roundToInt (theme.faderPt * 6.6f))); }
    int pagesPerView() const { return jlimit (1, pages(), faderCols() / 6); }
    void layout()
    {
        const int W = mainW, H = mainH, x0 = 24, wAll = W - 48, gap = 6, kh = keyH();
        int y = 14;
        // preset bar: the menu, then < > side by side, SAVE and the drawer key
        const int pw = (int) panelKey->preferredWidth(), sw = (int) saveKey->preferredWidth();
        topGear.setBounds (x0 + wAll - kh, y, kh, kh);
        panelKey->setBounds (topGear.getX() - gap - pw, y, pw, kh); saveKey->setBounds (panelKey->getX() - gap - sw, y, sw, kh);
        nextKey->setBounds (saveKey->getX() - gap - kh, y, kh, kh); prevKey->setBounds (nextKey->getX() - 2 - kh, y, kh, kh);
        presetName.setBounds (x0, y, prevKey->getX() - gap - x0, kh);
        y += kh + 10;
        // the screen: waterfall, decay time and the reverb EQ, one under the other; drag the line under it to resize
        const int scrDef = jlimit (260, H - 330, (int) (H * 0.47f)), scrSet = (int) proc.getUiProperty ("uiScrH", 0);
        const int scrH = jlimit (200, jmax (200, H - 330), scrSet > 0 ? scrSet : scrDef);
        screen.setBounds (x0, y, wAll, scrH); layoutScreen(); y += scrH;
        faderDiv.setBounds (x0, y, wAll, 10); y += 10;
        pageKey->setVisible (false);
        for (auto& k : algKeys) k->setVisible (false);
        for (auto& k : myKeys) k->setVisible (false);
        larcMain.setVisible (false);
        for (auto* k : { regKey.get(), progKey.get(), fnKey[0].get(), fnKey[1].get(), fnKey[2].get(), fnKey[3].get(), fnKey[4].get(), fnKey[5].get(), fnKey[6].get(), fnKey[7].get() }) k->setVisible (false);
        for (auto& k : numKey) k->setVisible (false);
        for (auto& k : sdKey) k->setVisible (false);
        // the shortcut bar: the first keys at the left, MUTE at the right (above Reverb Mix)
        {
            if (scKeys.empty() && ! shortcutIds().isEmpty()) buildShortcuts();
            scBarR = Rectangle<int> (x0, y, wAll, kh);
            int lx = x0, rx = x0 + wAll;
            for (size_t k = 0; k < scKeys.size(); ++k)
            {
                auto& key = *scKeys[k]; const int w = jmax (70, (int) key.preferredWidth());
                if (key.getText() == "MUTE") { rx -= w; key.setBounds (rx, y, w, kh); rx -= gap; }
                else { key.setBounds (lx, y, w, kh); lx += w + gap; }
            }
            y += kh + 8;
        }
        // the faders: full names, the value in a red window, then the fader
        larcR = Rectangle<int> (x0 - 8, y, wAll + 16, H - 12 - y);
        const float np = theme.faderPt, vp = theme.faderPt;
        nameLines = 2;
        labelH = np * 2.5f + 8 + vp + 14;
        faderArea = Rectangle<int> (larcR.getX() + 16, y + 12 + (int) labelH, larcR.getWidth() - 32 - mixColW(), larcR.getBottom() - 14 - (y + 12 + (int) labelH));
        {
            const int mx = faderArea.getRight() + 16, mw = mixColW() - 16;
            mixFader->setBounds (mx, faderArea.getY(), mw, faderArea.getHeight());
            const int top = faderArea.getY() - (int) labelH;
            mixNameR = Rectangle<int> (mx + 2, top, mw - 4, (int) (np * 2.5f + 4));
            mixValueR = Rectangle<int> (mx + 4, mixNameR.getBottom() + 3, mw - 8, (int) (theme.faderPt + 10));
            mixBay = Rectangle<int> (mx - 6, faderArea.getY() - (int) labelH - 8, mw + 12, faderArea.getHeight() + (int) labelH + 14);
        }
        sdLineR = {}; brandR = {};
        layoutFaders();
        layoutDrawer();
        applyMoves (content);
    }
    void layoutScreen()
    {
        auto r = screen.getLocalBounds().reduced (8);
        delayView.setBounds (r);                       // shown instead of the reverb picture for the delay programs
        const float a = (float) (double) proc.getUiProperty ("uiFrA", 0.56), b = (float) (double) proc.getUiProperty ("uiFrB", 0.22);
        const int hA = roundToInt ((r.getHeight() - 16) * a), hB = roundToInt ((r.getHeight() - 16) * b);
        reverbView.setBounds (r.removeFromTop (hA)); scrDiv[0].setBounds (r.removeFromTop (8));
        decay.setBounds (r.removeFromTop (hB)); scrDiv[1].setBounds (r.removeFromTop (8));
        eqGraph.setBounds (r);
        placeCells();
    }
    // the numbers sit on the picture: predelay at its line, decay time at the decay handle, the rest written across the
    // top of the lower graph (decay page) or of the delay picture. Click to type, or right-click and drag up/down.
    int slotPd() const { return findSlot ({ "PDL", "RPD" }); }
    // ---- your controls on the screen: click and hold the picture to add one; positions are kept per algorithm and page
    struct Pin { String key; float x, y; };
    String pinsId() const { return "pins_" + String (proc.algoOf (machine)) + "_" + String (page); }
    std::vector<Pin> loadPins() const
    {
        std::vector<Pin> v; StringArray t; t.addTokens (proc.getUiProperty (pinsId(), String()).toString(), ";", "");
        for (auto& e : t) { StringArray f; f.addTokens (e, "|", ""); if (f.size() == 3) v.push_back ({ f[0], f[1].getFloatValue(), f[2].getFloatValue() }); }
        return v;
    }
    void savePins (const std::vector<Pin>& v)
    {
        String s; for (auto& p : v) s << p.key << "|" << String (p.x, 4) << "|" << String (p.y, 4) << ";";
        proc.setUiProperty (pinsId(), s);
    }
    String targetKey (const Target& t) const
    {
        if (t.param) if (auto* w = dynamic_cast<AudioProcessorParameterWithID*> (t.param)) return "p:" + w->getParameterID();
        return "s:" + String (t.slot);
    }
    int targetIndex (const String& key) const { for (size_t i = 0; i < pageTargets.size(); ++i) if (targetKey (pageTargets[i]) == key) return (int) i; return -1; }
    void placeCells()
    {
        const auto pins = loadPins();
        const auto sb = screen.getLocalBounds();
        size_t ci = 0;
        for (auto& pin : pins)
        {
            const int ti = targetIndex (pin.key);
            if (ti < 0 || ci >= cells.size()) continue;
            auto& c = *cells[ci++];
            const String key = pin.key;
            c.name = [this, key] { const int i = targetIndex (key); return i >= 0 ? targetName (pageTargets[(size_t) i]) : String(); };
            c.value = [this, key] { const int i = targetIndex (key); return i >= 0 ? targetText (pageTargets[(size_t) i]) : String(); };
            c.getNorm = [this, key] { const int i = targetIndex (key); return i >= 0 ? targetNorm (pageTargets[(size_t) i]) : 0.0f; };
            c.setNorm = [this, key] (float v) { const int i = targetIndex (key); if (i >= 0) setTargetNorm (pageTargets[(size_t) i], v); };
            c.gesture = [this, key] (bool b) { const int i = targetIndex (key); if (i >= 0) targetGesture (pageTargets[(size_t) i], b); };
            c.setText = [this, key] (const String& t) { const int i = targetIndex (key); if (i >= 0) setTargetText (pageTargets[(size_t) i], t); };
            auto* cp = &c;
            c.onMoved = [this, key, cp]
            {
                auto v = loadPins(); const auto b = screen.getLocalBounds();
                for (auto& p : v) if (p.key == key) { p.x = cp->getX() / (float) jmax (1, b.getWidth()); p.y = cp->getY() / (float) jmax (1, b.getHeight()); }
                savePins (v);
            };
            c.onRemove = [this, key]
            {
                auto v = loadPins(); v.erase (std::remove_if (v.begin(), v.end(), [&key] (const Pin& p) { return p.key == key; }), v.end());
                savePins (v); Component::SafePointer<Flexiverb480Editor> sp (this); MessageManager::callAsync ([sp] { if (sp != nullptr) sp->placeCells(); });
            };
            const int w = c.preferredWidth(), h = c.preferredHeight();
            c.setBounds (jlimit (0, sb.getWidth() - w, roundToInt (pin.x * sb.getWidth())), jlimit (0, sb.getHeight() - h, roundToInt (pin.y * sb.getHeight())), w, h);
            c.setVisible (true); c.toFront (false);
        }
        for (; ci < cells.size(); ++ci) cells[ci]->setVisible (false);
    }
    // the function a control belongs to (for the menu, and the faders)
    static String groupOf (const String& n, int pg)
    {
        auto has = [&n] (std::initializer_list<const char*> w) { for (auto* x : w) if (n.containsIgnoreCase (x)) return true; return false; };
        if (pg == 1)
        {
            if (has ({ "Feedback", "Ping" })) return "REPEATS";
            if (has ({ "Level", "Pan", "Dry", "Mix" })) return "LEVEL";
            if (has ({ "Rolloff", "Bass", "Treble", "Cutoff" })) return "TONE";
            if (has ({ "Attack" })) return "SHAPE";
            if (has ({ "Delay", "Time", "Value", "Sync", "Multiplier", "Fine", "Echo" })) return "TIME";
            return "OTHER";
        }
        if (has ({ "Predelay", "Pre-Delay", "Reflection", "Echo", "Attack", "Early" })) return "EARLY";
        if (has ({ "Reverb Time", "RT ", "Multiply", "Crossover", "Decay", "Infinite" })) return "DECAY";
        if (has ({ "Mix", "Level", "Wet", "Dry", "Balance" })) return "MIX";
        if (has ({ "Cutoff", "Frequency", "Bass", "Treble", "EQ", "Filter" })) return "TONE";
        if (has ({ "Size", "Shape", "Spread", "Diffusion", "Density", "Definition", "Spin", "Wander", "Chorus", "Width", "Depth", "Rate", "Modulation" })) return "SPACE";
        return "OTHER";
    }
    // click and hold on the picture: choose a control to put there
    void addControlMenu (Point<int> atInScreen)
    {
        const auto pins = loadPins();
        const StringArray order = groupOrder();
        PopupMenu m; m.addSectionHeader ("Put a control here");
        for (auto& gname : order)
        {
            PopupMenu sub; bool any = false;
            for (size_t i = 0; i < pageTargets.size(); ++i)
            {
                if (pageTargets[i].group != gname) continue;
                const String key = targetKey (pageTargets[i]);
                bool on = false; for (auto& p : pins) on = on || p.key == key;
                sub.addItem (1 + (int) i, pageTargets[i].name, ! on, on); any = true;
            }
            if (any) m.addSubMenu (groupTitle (gname).substring (0, 1) + groupTitle (gname).substring (1).toLowerCase(), sub);
        }
        const auto b = screen.getLocalBounds();
        m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (Rectangle<int> (screen.localPointToGlobal (atInScreen), screen.localPointToGlobal (atInScreen)).expanded (1)),
            [this, atInScreen, b] (int r)
            {
                if (r <= 0 || r > (int) pageTargets.size()) return;
                auto v = loadPins(); v.push_back ({ targetKey (pageTargets[(size_t) r - 1]), atInScreen.x / (float) jmax (1, b.getWidth()), atInScreen.y / (float) jmax (1, b.getHeight()) });
                savePins (v); placeCells();
            });
    }
    struct HoldWatch : public MouseListener, private Timer
    {
        Flexiverb480Editor* ed = nullptr; std::function<bool()> busy; Component* src = nullptr; Point<int> down; bool armed = false;
        void mouseDown (const MouseEvent& e) override { if (e.mods.isPopupMenu() || e.getNumberOfClicks() > 1) return; src = e.eventComponent; down = e.getEventRelativeTo (&ed->screen).getPosition(); armed = true; startTimer (450); }
        void mouseDrag (const MouseEvent& e) override { if (armed && e.getDistanceFromDragStart() > 4) { armed = false; stopTimer(); } }
        void mouseUp (const MouseEvent&) override { armed = false; stopTimer(); }
        void timerCallback() override { stopTimer(); if (armed && ! (busy && busy())) { armed = false; ed->addControlMenu (down); } }
    };
    HoldWatch holdRev, holdDelay;
    std::vector<ReverbView::Bar> dragBars; std::vector<Flexiverb480Processor::TapEd> dragTaps; std::vector<Flexiverb480Processor::Refl> dragRefl;
    // faders in groups (a heading over each, a gap between); one row, or two when there are many - a group never splits
    struct GroupBox { String group; Rectangle<int> head; };
    std::vector<GroupBox> groupBoxes;
    std::vector<Rectangle<int>> nameR, valueR; int nameLines = 2;
    float groupPtFor (const String& g) const { return theme.tpt ("group:" + g, theme.faderPt * 0.95f); }
    struct Bay { String title; Rectangle<int> area; };
    std::vector<Bay> bays;
    void layoutFaders()
    {
        // as many faders as the window is wide, in groups of six (one 480L page each)
        nShown = jmin ((int) pageTargets.size(), kMaxFaders);
        groupBoxes.clear(); bays.clear(); nameR.assign ((size_t) kMaxFaders, {}); valueR.assign ((size_t) kMaxFaders, {});
        const int groups = jmax (1, (nShown + 5) / 6), gg = 16;
        const float cw = (faderArea.getWidth() - (groups - 1) * gg) / (float) jmax (6, nShown);
        const float np = theme.faderPt;
        for (int i = 0; i < kMaxFaders; ++i)
        {
            if (i >= nShown) { faders[(size_t) i]->setVisible (false); continue; }
            const int x = faderArea.getX() + (int) (i * cw) + (i / 6) * gg;
            faders[(size_t) i]->setBounds (x, faderArea.getY(), (int) cw, faderArea.getHeight()); faders[(size_t) i]->setVisible (true);
            const int top = faderArea.getY() - (int) labelH;
            nameR[(size_t) i] = Rectangle<int> (x + 2, top, (int) cw - 4, (int) (np * 2.5f + 4));
            valueR[(size_t) i] = Rectangle<int> (x + 4, nameR[(size_t) i].getBottom() + 3, (int) cw - 8, (int) (theme.faderPt + 10));
        }
        for (int gi = 0; gi < groups; ++gi)
        {
            const int i0 = gi * 6, i1 = jmin (nShown, i0 + 6) - 1;
            if (i1 < i0) continue;
            const int x0 = faders[(size_t) i0]->getX(), x1 = faders[(size_t) i1]->getRight();
            bays.push_back ({ "PAGE " + String (page + gi + 1), Rectangle<int> (x0 - 6, faderArea.getY() - (int) labelH - 8, x1 - x0 + 12, faderArea.getHeight() + (int) labelH + 14) });
        }
        content.repaint();
    }
    void layoutDrawer()
    {
        // the side panel: the channel strip only (the system and analog settings live in PREFERENCES)
        sections.clear();
        for (auto& h : holders) while (h.getNumChildComponents() > 0) drawer.addChildComponent (h.getChildComponent (0));
        const int fx = 6, fw = kDrawer - 12, pad = 10, x0 = fx + pad, wAll = fw - 2 * pad, kh = keyH();
        const int th = roundToInt (theme.keyPt * 1.5f + 6);                   // a row of word switches
        const int A = presetName.getY(), H = mainH - 10;
        for (auto* k : std::initializer_list<LarcKey*> { muteKey.get(), setupKey.get(), ironKey.get(), machAKey.get(), machBKey.get(), anOnKey.get(), gtOnKey.get(), gtKeyKey.get(),
                         eqOnKey.get(), cpOnKey.get(), satKey.get(), duckKey.get(), eqModelLab.get() }) k->textOnly = true;
        // gone from the panel
        for (Component* c : std::initializer_list<Component*> { orderKey.get(), cpDual[0].get(), cpDual[1].get(), muteKey.get(), setupKey.get(), ironKey.get(), machAKey.get(), machBKey.get(),
                         anOnKey.get(), &anOsBox, &engineBox, depthKnob.get(), eqKnob2[0].get(), eqKnob2[1].get() }) c->setVisible (false);
        for (auto& k : sysKnob) k->setVisible (false);
        for (auto& k : anKnob) k->setVisible (false);
        auto words = [&] (std::vector<LarcKey*> ks, int x, int y, int gapW) { for (auto* k : ks) { k->setVisible (true); const int w = (int) k->preferredWidth(); k->setBounds (x, y, w, th); x += w + gapW; } return x; };
        const int titleH = jmax (kh, roundToInt (theme.knobPt) + 12);
        sections.push_back ({ "CHANNEL STRIP", { fx, A, fw, H - A } });
        drawerX.setBounds (fx + fw - 8 - 26, A + 7, 26, 26);
        stripGear.setBounds (drawerX.getX() - 12 - 28, A + 6, 28, 28); stripGear.setVisible (true);
        int y = A + titleH;
        words ({ gtOnKey.get(), gtKeyKey.get() }, x0 + 2, y, 14);
        const int wEq = (int) (eqOnKey->preferredWidth() + cpOnKey->preferredWidth() + satKey->preferredWidth() + duckKey->preferredWidth()) + 3 * 14;
        words ({ eqOnKey.get(), cpOnKey.get(), satKey.get(), duckKey.get() }, x0 + wAll - wEq - 2, y, 14);
        y += th + 4;
        // rows: gate, EQ (2 rows), the EQ model menu, the VU, the compressor (2 rows)
        const int menuH = kh + 6;
        enum { rGate, rEq1, rEq2, rVu, rCp1, rCp2 };
        const float wtOf[6] = { 0.95f, 1.3f, 1.3f, 1.05f, 1.0f, 1.0f };
        const int avail = H - 6 - y - menuH;
        int top[6], bot[6]; { float tot = 0; for (float w : wtOf) tot += w; float acc = 0; int yy = y;
            for (int r = 0; r < 6; ++r) { if (r == rVu) yy += menuH; top[r] = yy + roundToInt ((float) avail * acc / tot); acc += wtOf[r]; bot[r] = yy + roundToInt ((float) avail * acc / tot); } }
        auto cell = [&] (Component& c, int row, float col, float span, int cols) { const float w = (float) wAll / (float) cols;
            c.setBounds (x0 + roundToInt (col * w), top[row], roundToInt (span * w), bot[row] - top[row]); c.setVisible (true); };
        cell (*gtKnob[0], rGate, 0, 1, 4); cell (*gtKnob[1], rGate, 1, 1, 4); cell (*gtKnob[2], rGate, 2, 1, 4);
        { const int w = wAll / 4; gateMeter.setBounds (x0 + 3 * w + 4, top[rGate] + 2, w - 8, bot[rGate] - top[rGate] - 4); }
        cell (*eqKnob[0], rEq1, 0, 1, 2); cell (*eqKnob[1], rEq1, 1, 1, 2);
        cell (*eqKnob[2], rEq2, 0, 1, 2); cell (*eqKnob[3], rEq2, 1, 1, 2);
        {   // EQ MODEL menu under the EQ
            const int my = bot[rEq2] + 3, lw = (int) eqModelLab->preferredWidth();
            eqModelLab->setBounds (x0 + 4, my + (kh - th) / 2, lw, th); eqModelLab->setVisible (true);
            eqModelBox.setBounds (x0 + lw + 18, my, wAll - lw - 22, kh); eqModelBox.setVisible (true);
        }
        vu.setBounds (x0 + 2, top[rVu] + 4, wAll - 4, bot[rVu] - top[rVu] - 8);
        for (int k = 1; k <= 4; ++k) cell (*cpKnob[k], rCp1, (float) (k - 1), 1, 4);
        cell (*cpKnob[0], rCp2, 0, 1, 4); cell (*cpKnob[5], rCp2, 1, 1, 4); cell (*cpKnob[6], rCp2, 2, 1, 4); cell (*cleanKnob, rCp2, 3, 1, 4);
        drawer.setSize (kDrawer, mainH);
        engineLabelR = {};
        // every control moves into its module (so the module can grow with all its controls)
        for (int c = drawer.getNumChildComponents(); --c >= 0;)
        {
            auto* comp = drawer.getChildComponent (c);
            for (int i = 0; i < (int) sections.size() && i < 3; ++i)
                if (sections[(size_t) i].area.contains (comp->getBounds().getCentre()))
                { const auto bb = comp->getBounds() - sections[(size_t) i].area.getPosition(); holders[i].addChildComponent (comp); comp->setBounds (bb); comp->toFront (false); break; }
        }
        for (int i = 0; i < 3; ++i)
        {
            auto& h = holders[i]; h.ed = this; h.idx = i;
            if (h.getParentComponent() == nullptr) content.addChildComponent (h);
            if (! h.zoomed && i < (int) sections.size()) h.setBounds (sections[(size_t) i].area + Point<int> (mainW, 0));
            h.setVisible (drawerOpen && i < (int) sections.size()); h.repaint();
            applyMoves (h);
        }
    }
    int baseWidth() const { return mainW + (drawerOpen ? kDrawer : 0) + (setupOpen ? kSetup : 0); }
    bool applying = false;
    void applySize()
    {
        applying = true;
        const int w = baseWidth();
        placePanels();
        panelKey->setText (drawerOpen ? "< PANEL" : "PANEL >");
        const int extra = (drawerOpen ? kDrawer : 0) + (setupOpen ? kSetup : 0);
        setResizeLimits (roundToInt ((kMinL + extra) * scale), roundToInt (kMinH * scale), roundToInt ((3000 + extra) * scale), roundToInt (2000 * scale));
        if (auto* c = getConstrainer()) c->setFixedAspectRatio (0.0);
        setSize (roundToInt (w * scale), roundToInt (mainH * scale));
        applying = false;
        resized(); repaint(); content.repaint();
    }
    void panelRightClick (Point<int> p)
    {
        if (scBarR.contains (p)) { shortcutMenu(); return; }
        if (sdLineR.contains (p)) { editText ("slider line", "Slider display line", sdPt(), theme.larcText, &content, sdLineR); return; }
        if (brandR.contains (p))
        {
            if (p.y > brandR.getY() + brandR.getHeight() * 0.55f) editText ("machine legend", "Configuration line", 9.0f, theme.larcLegend.withAlpha (0.6f), &content, brandR);
            else editText ("brand", "Name on the remote", 18.0f, theme.larcLegend, &content, brandR);
            return;
        }
        for (auto& gb : groupBoxes) if (gb.head.expanded (0, 4).contains (p)) { editText ("group:" + gb.group, "Group heading " + groupTitle (gb.group), theme.faderPt * 0.95f, theme.keyLight, &content, gb.head); return; }
        for (int i = 0; i < nShown; ++i)
        {
            const auto& tg = pageTargets[(size_t) i];
            if (nameR[(size_t) i].contains (p)) { editText ("fader name:" + tg.name, "Fader name " + tg.name, theme.faderPt, theme.panelText, &content, nameR[(size_t) i]); return; }
            if (valueR[(size_t) i].contains (p)) { editText ("fader value:" + tg.name, "Fader value " + tg.name, theme.faderPt, theme.keyLight, &content, valueR[(size_t) i]); return; }
        }
        showSizeMenu();
    }
    void drawerRightClick (Point<int> p)
    {
        if (engineLabelR.contains (p)) { editText ("label:ENGINE", "ENGINE", jmax (9.0f, theme.keyPt * 0.9f), theme.panelText, &drawer, engineLabelR); return; }
        for (auto& t : titleR) if (t.second.contains (p)) { editText ("section:" + t.first, "Title " + t.first, jmax (10.0f, theme.knobPt), t.first == "SYSTEM" ? theme.panelText : Colours::white, &drawer, t.second); return; }
        showSizeMenu();
    }
    void showSizeMenu()
    {
        PopupMenu m; m.addSectionHeader ("Window size");
        const float sizes[] = { 0.5f, 0.6f, 0.7f, 0.8f, 1.0f, 1.25f, 1.5f };
        for (int i = 0; i < 7; ++i) m.addItem (i + 1, String (roundToInt (sizes[i] * 100)) + "%", true, std::abs (sizes[i] - scale) < 0.01f);
        m.addSeparator(); m.addItem (100, "Drag the lower right corner to scale freely", false);
        m.addSeparator(); m.addItem (200, "Preferences (look, Flexi 180V hardware)");
        m.showMenuAsync (PopupMenu::Options(), [this, sizes] (int r)
        {
            if (r > 0 && r < 100) { scale = sizes[r - 1]; proc.setUiProperty ("uiScale", scale); applySize(); }
            if (r == 200) openPrefs();
        });
    }
    void lookChanged()
    {
        proc.setUiProperty ("themeData", theme.toString());
        lnf.setColour (PopupMenu::highlightedBackgroundColourId, theme.keyLight.darker (0.2f));
        layout(); applySize(); setup.resized(); browser.resized();
        content.repaint(); for (auto* c : content.getChildren()) c->repaint(); for (auto* c : drawer.getChildren()) c->repaint(); for (auto* c : screen.getChildren()) c->repaint();
    }

    // ---------------------------------------------------------------- state
    int machine = 0, page = 0, lastConfig = -1, nShown = 0, lastAlgo = -1, lastPage = -1, lastRv = -1;
    bool drawerOpen = false, setupOpen = false;
    uint32 msgUntil = 0; String msg1, msg2;
    int config() const { return (int) proc.apvts.getRawParameterValue ("config")->load(); }
    int addMode() const { return jlimit (0, 3, (int) proc.apvts.getRawParameterValue ("addOn")->load()); }

    // ---------------------------------------------------------------- the remote's keys (as on the 480L's LARC)
    enum class Mode { Normal, Bank, Prog, Page, Reg, Sto };
    Mode mode = Mode::Normal; int browseBank = 1, regBank = 1, focus = -1; uint32 focusTime = 0;
    LarcMain larcMain;
    std::unique_ptr<LarcKey> regKey, progKey, numKey[10], fnKey[8], sdKey[6];
    String progText (int m) const
    {
        if (m == 0 && proc.userPresetName.isNotEmpty()) return proc.userPresetName;
        const auto& p = proc.prog (m);
        if (proc.registerName[m].isNotEmpty()) return "R" + proc.registerName[m] + " " + p.name;
        return String (p.bank) + "." + String (p.num) + " " + p.name;
    }
    void setMode (Mode m)
    {
        mode = m;
        fnKey[0]->setLit (m == Mode::Bank); progKey->setLit (m == Mode::Prog); fnKey[6]->setLit (m == Mode::Page);
        regKey->setLit (m == Mode::Reg); fnKey[2]->setLit (m == Mode::Sto);
    }
    void pressBank()
    {
        if (mode == Mode::Reg || mode == Mode::Sto) { regBank = regBank % 10 + 1; showMessage ((mode == Mode::Reg ? "REG BANK " : "STORE IN BANK ") + String (regBank), "PRESS 1-0", 6000); return; }
        if (mode != Mode::Bank) browseBank = proc.prog (machine).bank; else browseBank = browseBank % l480::kNumBanks + 1;
        setMode (Mode::Bank); showMessage ("BANK " + String (browseBank), l480::bankName (browseBank), 6000);
    }
    void pressProg()
    {
        if (mode != Mode::Bank) browseBank = proc.prog (machine).bank;
        setMode (Mode::Prog); showMessage ("PROG  BANK " + String (browseBank), "1-0 LOADS A PROGRAM", 6000);
    }
    void pressReg() { setMode (Mode::Reg); showMessage ("REG BANK " + String (regBank), "1-0 RECALLS  BANK = NEXT", 6000); }
    void pressSto() { setMode (Mode::Sto); showMessage ("STORE IN REG " + String (regBank) + ".?", "PRESS 1-0", 6000); }
    void pressPage()
    {
        const int ppv = pagesPerView();
        page = (page + ppv) % pages(); page -= page % ppv; refreshPage (true);
        const int last = jmin (pages(), page + ppv);
        showMessage (ppv > 1 ? "PAGES " + String (page + 1) + "-" + String (last) + " OF " + String (pages()) : "PAGE " + String (page + 1) + " OF " + String (pages()), l480::algoDef (proc.algoOf (machine)).name, 2000);
    }
    void pressNumber (int n)
    {
        const int k = n == 0 ? 10 : n;
        if (mode == Mode::Page) { setMode (Mode::Normal); if (k <= pages()) { page = k - 1; refreshPage (true); } msgUntil = 0; return; }
        if (mode == Mode::Reg)
        {
            setMode (Mode::Normal);
            if (proc.recallRegister (machine, regBank, n)) { page = 0; refreshPage (true); showMessage ("REGISTER " + String (regBank) + "." + String (n), proc.registerTitle (regBank, n)); }
            else showMessage ("REGISTER " + String (regBank) + "." + String (n), "EMPTY");
            return;
        }
        if (mode == Mode::Sto) { setMode (Mode::Normal); if (proc.storeRegister (machine, regBank, n)) showMessage ("STORED IN REGISTER", String (regBank) + "." + String (n)); return; }
        const int bank = (mode == Mode::Bank || mode == Mode::Prog) ? browseBank : proc.prog (machine).bank;
        const int pi = l480::findProgram (bank, n);
        setMode (Mode::Normal);
        if (pi >= 0) { proc.loadProgram (machine, pi); page = 0; refreshPage (true); msgUntil = 0; }
        else showMessage ("NO PROGRAM", String (bank) + "." + String (n));
    }
    void pressMach()
    {
        if (config() == 0 && addMode() == 0) { showMessage ("SINGLE", "MACHINE A ONLY"); return; }
        selectMachine (1 - machine); showMessage ("MACHINE " + String (machine == 0 ? "A" : "B"), progText (machine));
    }
    void pressSlotKey (int i)
    {
        if (i >= (int) pageTargets.size() || pageTargets[(size_t) i].slot < 0) return;
        const auto t = pageTargets[(size_t) i];
        if (t.trigger) { proc.pressKey (machine, t.slot); showMessage (t.name.toUpperCase(), proc.machineStatus (machine)); return; }
        focus = i; focusTime = Time::getMillisecondCounter();
        showMessage (t.name.toUpperCase(), t.lab + " " + targetText (t), 2500);
    }
    // DELAY / CHORUS with the reverb: machine B holds it, beside, before or after machine A
    void addOnMenu (int pi, LarcKey* kp)
    {
        auto* a = proc.apvts.getParameter ("addOn");
        auto setAdd = [a] (int v) { a->beginChangeGesture(); a->setValueNotifyingHost (a->convertTo0to1 ((float) v)); a->endChangeGesture(); };
        if (pi < 0) return;
        const bool on = addMode() > 0 && proc.programOf (1) == pi;
        if (! on)
        {
            proc.loadProgram (1, pi); setAdd (addMode() > 0 ? addMode() : 1);
            showMessage (String (l480::programs()[(size_t) pi].name).toUpperCase() + " ADDED", "MACH SHOWS ITS PAGES", 3000);
            return;
        }
        PopupMenu m; const int cur = addMode();
        m.addItem (1, "Beside the reverb (both hear the input)", true, cur == 1);
        m.addItem (2, "Before the reverb (the echoes get reverb)", true, cur == 2);
        m.addItem (3, "After the reverb (the reverb gets echoes)", true, cur == 3);
        m.addSeparator(); m.addItem (4, "Take it off");
        m.addSeparator(); m.addItem (5, "Text size and colour of this button...");
        Component::SafePointer<LarcKey> sp (kp);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (kp), [this, setAdd, sp] (int r)
        {
            if (r >= 1 && r <= 3) setAdd (r);
            else if (r == 4) { setAdd (0); selectMachine (0); }
            else if (r == 5 && sp != nullptr) editText (sp->textId(), "Button " + sp->getText(), theme.keyPt, theme.keyText, sp.getComponent(), sp->getLocalBounds());
        });
    }
    void showMessage (const String& a, const String& b, int ms = 1800) { msg1 = a; msg2 = b; msgUntil = Time::getMillisecondCounter() + (uint32) ms; screen.repaint(); }
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
    int pages() const { return jmax (1, l480::algoDef (proc.algoOf (machine)).pages); }
    void buildPage()
    {
        // the six controls of the current page, exactly as the 480L lays them out
        pageTargets.clear();
        const int algo = proc.algoOf (machine), ppv = pagesPerView();
        if (page >= pages()) page = 0;
        page -= page % ppv;
        for (int i = 0; i < jmin (jmax (6, faderCols()), 6 * (pages() - page)); ++i)
        {
            const int slot = page * 6 + i; Target t;
            const auto& sp = proc.spec (machine, slot);
            if (l480::algoDef (algo).pages > 0 && sp.lab != nullptr && String (sp.lab) != "CONV") { /* the converter is a SETUP option */ t.slot = slot; t.name = plainName (sp.name); t.lab = sp.lab; t.trigger = sp.unit == l480::uKey; }
            t.group = "PAGE";
            pageTargets.push_back (t);
        }
    }
    static String rowGroup (const String& g, int row)
    {
        if (row == 0) return g;
        if (g == "TIME") return "DELAY TIME";
        if (g == "LEVEL") return "ECHO LEVEL";
        if (g == "TONE") return "DELAY TONE";
        if (g == "SHAPE") return "DELAY SHAPE";
        if (g == "OTHER") return "DELAY MORE";
        return g;
    }
    StringArray groupOrder() const { return { "EARLY", "DECAY", "SPACE", "TONE", "MIX", "OTHER", "DELAY TIME", "REPEATS", "ECHO LEVEL", "DELAY TONE", "DELAY SHAPE", "DELAY MORE" }; }
    static String groupTitle (const String& g) { return g == "OTHER" || g == "DELAY MORE" ? String ("MORE") : g == "ECHO LEVEL" ? String ("ECHOES") : g.startsWith ("DELAY ") ? g.substring (6) : g; }
    String targetName (const Target& t) const { return t.name; }
    float targetNorm (const Target& t) const { if (t.param) return t.param->getValue(); if (t.slot < 0) return 0.0f; const auto& s = proc.spec (machine, t.slot); return (s.unit == l480::uKey) ? 0.0f : proc.slotNorm (machine, t.slot); }
    String targetText (const Target& t) const
    {
        if (! t.param && t.slot < 0) return {};
        if (t.param) return t.param->getCurrentValueAsText();
        const auto& s = proc.spec (machine, t.slot);
        if (s.unit == l480::uKey) return "push up";
        return String (l480::formatValue (s, proc.slotValue (machine, t.slot)));
    }
    void targetGesture (const Target& t, bool begin) { if (! t.param && t.slot < 0) return; if (t.param) { if (begin) t.param->beginChangeGesture(); else t.param->endChangeGesture(); } else if (! t.trigger) { if (begin) proc.beginSlot (machine, t.slot); else proc.endSlot (machine, t.slot); } }
    void setTargetNorm (const Target& t, float v) { if (! t.param && t.slot < 0) return; if (t.param) t.param->setValueNotifyingHost (v); else if (! t.trigger) proc.setSlot (machine, t.slot, v); }
    void setTargetText (const Target& t, const String& text)
    {
        if (! t.param && t.slot < 0) return;
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
        focus = i; focusTime = Time::getMillisecondCounter();
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
        buildPage(); layout();            // the label height follows the page's texts
        pageKey->setText (page == 0 ? "REVERB" : "DELAY");
        for (int i = 0; i < nShown; ++i)
        {
            const auto& tg = pageTargets[(size_t) i];
            faders[(size_t) i]->enabled = tg.param != nullptr || tg.slot >= 0;
            faders[(size_t) i]->setTarget (pageTargets[(size_t) i].trigger ? 0.0f : targetNorm (pageTargets[(size_t) i]));
            if (snap) faders[(size_t) i]->snap();
        }
        findMix(); mixFader->setTarget (targetNorm (mixTg)); if (snap) mixFader->snap();
        const int al = proc.algoOf (machine);
        const bool dl = al == l480::aTwin || al == l480::aMBand || al == l480::aPrime || al == l480::aChorus || al == l480::aFx;
        delayView.setVisible (dl); reverbView.setVisible (! dl); decay.setVisible (! dl); eqGraph.setVisible (! dl); for (auto& d : scrDiv) d.setVisible (! dl);
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
    void selectMachine (int m) { if (config() == 0 && addMode() == 0) m = 0; if (m != machine) { machine = m; page = 0; refreshPage (true); } }

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
        { const auto& r = proc.refl[machine]; p.reflN[0] = (int) jmin ((size_t) 32, r.size()); for (int k = 0; k < p.reflN[0]; ++k) { p.reflMs[0][k] = r[(size_t) k].ms; p.reflDb[0][k] = r[(size_t) k].db; p.reflPan[0][k] = r[(size_t) k].pan; } }
        { const auto& e = proc.tapEd[machine]; p.tapN[0] = (int) jmin ((size_t) 64, e.size()); for (int k = 0; k < p.tapN[0]; ++k) { p.tapMs[0][k] = e[(size_t) k].ms; p.tapDb[0][k] = e[(size_t) k].db; } }
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
        for (auto& w : viewWin) if (w != nullptr) for (int b = 0; b < 3; ++b) w->view->live[b] = reverbView.live[b];
    }

    // ---------------------------------------------------------------- 30 Hz
    void timerCallback() override
    {
        if (config() != lastConfig) { lastConfig = config(); if (lastConfig == 0 && addMode() == 0 && machine == 1) machine = 0; engineBox.setSelectedItemIndex (lastConfig == 0 ? 0 : 1, dontSendNotification); layoutDrawer(); refreshPage (true); drawer.repaint(); }
        if (proc.programLoaded.exchange (false) || proc.algoOf (machine) != lastAlgo) { lastAlgo = proc.algoOf (machine); refreshPage (false); }
        // preset bar
        {
            String t = programTitle (machine), sub = proc.userPresetName.isNotEmpty() ? String ("Your preset") : String (l480::bankName (proc.prog (machine).bank));
            if (config() != 0) t = String (machine == 0 ? "A  " : "B  ") + t;
            if (t != presetName.text || sub != presetName.sub) { presetName.text = t; presetName.sub = sub; presetName.repaint(); }
        }
        // the remote's display: the program and what was just touched; the meters
        {
            const uint32 now = Time::getMillisecondCounter();
            String l1, l2;
            if (now < msgUntil) { l1 = msg1; l2 = msg2; }
            else
            {
                if (mode != Mode::Normal) setMode (Mode::Normal);
                l1 = ((config() != 0 || addMode() > 0) ? String (machine == 0 ? "A " : "B ") : String()) + progText (machine) + (proc.isEdited (machine) ? "*" : "");
                const auto st = proc.machineStatus (machine);
                if (focus >= 0 && focus < (int) pageTargets.size() && now - focusTime < 2500 && pageTargets[(size_t) focus].slot >= 0)
                    l2 = pageTargets[(size_t) focus].name.toUpperCase() + " " + targetText (pageTargets[(size_t) focus]);
                else if (st.isNotEmpty()) l2 = st;
                else l2 = String (l480::algoDef (proc.algoOf (machine)).name).toUpperCase();
            }
            if (l1 != larcMain.l1 || l2 != larcMain.l2) { larcMain.l1 = l1; larcMain.l2 = l2; larcMain.repaint(); }
            const float mo = proc.meterOut.load(); larcMain.mL = mo > larcMain.mL ? mo : larcMain.mL * 0.88f; larcMain.mR = larcMain.mL;
            larcMain.repaint (larcMain.getLocalBounds().removeFromRight (66));
            for (int i = 0; i < nShown; ++i) content.repaint (valueR[(size_t) i]);
            fnKey[4]->setLit (machine == 1); fnKey[5]->setLit (proc.mute.load()); fnKey[3]->setLit (drawerOpen);
        }
        // ask for a new response picture when anything that shapes the reverb changed
        {
            int sig = proc.programOf (machine) * 7919 + machine * 31 + (proc.curveOn[machine] ? 1 : 0) + (int) (proc.apvts.getRawParameterValue ("erAtk")->load() * 10);
            for (int s = 0; s < l480::kMaxParams; ++s) sig = sig * 31 + (int) (proc.slotNorm (machine, s) * 1000.0f);
            for (int b = 0; b < pv::DecayEq::N; ++b) sig = sig * 31 + (int) (proc.aCurveMul[machine][b].load() * 1000.0f);
            for (auto& r : proc.refl[machine]) sig = sig * 31 + (int) (r.ms * 10.0f) * 7 + (int) (r.db * 10.0f);
            for (auto& e : proc.tapEd[machine]) sig = sig * 31 + (int) (e.ms * 10.0f) * 7 + (int) (e.db * 10.0f);
            if (sig != lastSig) { lastSig = sig; requestResponse(); }
            const int v = renderer.version.load(); if (v != lastRv) { lastRv = v; reverbView.setResult (renderer.get()); for (auto& w : viewWin) if (w != nullptr) w->view->setResult (renderer.get()); }
        }
        {
            float mx = 100.0f; const float pd = reverbView.getPredelayMs ? reverbView.getPredelayMs() : 0.0f; mx = jmax (mx, pd);
            for (auto& b : reverbView.getBars()) mx = jmax (mx, b.ms);
            const float rng = jlimit (100.0f, 500.0f, std::ceil (mx * 1.15f / 50.0f) * 50.0f);
            if (rng != reverbView.erRange && ! reverbView.busy()) { reverbView.erRange = rng; reverbView.repaint(); }
        }
        liveBands();
        if (reverbView.isVisible() && (reverbView.animate() || true)) reverbView.repaint();
        for (auto& w : viewWin) if (w != nullptr) { w->view->erRange = reverbView.erRange; w->view->animate(); w->view->repaint(); }
        if (decay.isVisible() && sigChanged()) decay.repaint();
        if (delayView.isVisible()) delayView.repaint();
        if (Time::getMillisecondCounter() < msgUntil + 100) screen.repaint (screen.getLocalBounds().withHeight ((int) theme.displayPt + 14));
        for (auto& c : cells) if (c->isVisible()) { const int w = c->preferredWidth(); if (w != c->getWidth()) c->setSize (w, c->getHeight()); c->repaint(); }
        // faders follow their targets (and the screen's edits)
        bool labels = false;
        for (int i = 0; i < nShown; ++i)
        {
            const auto& tg = pageTargets[(size_t) i];
            if (! tg.trigger) faders[(size_t) i]->setTarget (targetNorm (tg));
            if (faders[(size_t) i]->animate()) { faders[(size_t) i]->repaint(); labels = true; }
        }
        { mixFader->setTarget (targetNorm (mixTg)); if (mixFader->animate()) { mixFader->repaint(); labels = true; } }
        if (labels || ++labelTick % 6 == 0) { content.repaint (faderArea.withTrimmedTop (-(int) labelH * 2 - 10)); content.repaint (mixBay.expanded (2)); }
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
            if (addMode() > 0 && machine == 0)
            {
                const int a1 = proc.algoOf (1);
                if (a == "DELAY") lit = lit || a1 == l480::aTwin || a1 == l480::aMBand;
                if (a == "CHORUS") lit = lit || a1 == l480::aChorus || a1 == l480::aPrime;
            }
            algKeys[k]->setLit (lit);
        }
        pageKey->setLit (page == 1);
        for (size_t k = 0; k < myKeys.size() && k < myDefs.size(); ++k)
        {
            const auto& d = myDefs[k];
            myKeys[k]->setLit (d.user.isNotEmpty() ? proc.userPresetName == d.user : (proc.userPresetName.isEmpty() && proc.programOf (machine) == d.program));
        }
        machAKey->setLit (config() != 0 && machine == 0); machBKey->setLit (config() != 0 && machine == 1);
        muteKey->setLit (proc.mute.load()); setupKey->setLit (prefs.isVisible()); topGear.lit = stripGear.lit = prefs.isVisible();
        { const int m = (int) proc.apvts.getRawParameterValue ("eqModel")->load(); if (m >= 1 && eqModelBox.getSelectedId() != m + 1) eqModelBox.setSelectedId (m + 1, dontSendNotification); } panelKey->setLit (drawerOpen);
        if (drawerOpen) vu.setGr (proc.compGr.load(), 1000.0f / 30.0f);
        if (drawerOpen)
        {
            for (auto* k : { ironKey.get(), anOnKey.get(), eqOnKey.get(), cpOnKey.get(), satKey.get(), duckKey.get(), gtOnKey.get(), gtKeyKey.get() }) k->sync();
            gateMeter.on = proc.apvts.getRawParameterValue ("gtOn")->load() > 0.5f; gateMeter.thrDb = proc.apvts.getRawParameterValue ("gtThr")->load();
            gateMeter.keyDb = jmax (proc.gateKey.load(), gateMeter.keyDb - 2.0f); gateMeter.gainDb = proc.gateGain.load(); gateMeter.repaint();
            if (orderKey->getText() != (stripOrder() == 1 ? "COMP > EQ" : "EQ > COMP")) { layoutDrawer(); drawer.repaint(); }
        }
    }
    bool sigChanged() { const int s = lastSig ^ (proc.curveOn[machine] ? 77 : 0); if (s != lastDecaySig) { lastDecaySig = s; return true; } return false; }
    int lastSig = 0, lastDecaySig = 0, labelTick = 0;

    // ---------------------------------------------------------------- members
    Flexiverb480Processor& proc;
    Theme theme;
    FlexLnf lnf;
    float scale = 0.8f;
    Content content; Screen screen; PresetName presetName; CloseX drawerX { theme };
    DecayGraph decay; DelayGraph delayView; ReverbView reverbView; ResponseRenderer renderer;
    std::vector<std::unique_ptr<NumberCell>> cells;
    std::unique_ptr<LarcKey> prevKey, nextKey, saveKey, panelKey, pageKey, machAKey, machBKey, muteKey, setupKey;
    // ---- a thin bar you drag up or down
    struct DragDivider : public Component
    {
        std::function<void (int)> onDrag; int last = 0;
        DragDivider() { setMouseCursor (MouseCursor::UpDownResizeCursor); setRepaintsOnMouseActivity (true); }
        void paint (Graphics& g) override { g.setColour (Colours::white.withAlpha (isMouseOver() || isMouseButtonDown() ? 0.28f : 0.06f)); g.fillRect (getLocalBounds().toFloat().withSizeKeepingCentre ((float) getWidth() - 20.0f, isMouseOver() ? 3.0f : 1.0f)); }
        void mouseDown (const MouseEvent& e) override { last = e.getScreenY(); }
        void mouseDrag (const MouseEvent& e) override { const int dy = e.getScreenY() - last; if (dy != 0 && onDrag) { last = e.getScreenY(); onDrag (dy); } }
    };
    DragDivider scrDiv[2], faderDiv;
    ReverbEqGraph eqGraph { theme, proc };
    // ---- turning: the slots it moves, as they were when the turning started (double-click brings them back)
    struct TurnSlot { int slot = -1; float start = 0, home = -1; };
    TurnSlot tRt, tHf, tSz, tPd, tDf; bool turnHomeSet = false;
    void turnSound (int what, float dx, float dy, bool twist, int phase)
    {
        auto grab = [this] (TurnSlot& t, std::initializer_list<const char*> labs)
        { t.slot = findSlot (labs); if (t.slot >= 0) { t.start = proc.slotNorm (machine, t.slot); if (! turnHomeSet) t.home = t.start; proc.beginSlot (machine, t.slot); } };
        auto put = [this] (TurnSlot& t, float norm) { if (t.slot >= 0) proc.setSlot (machine, t.slot, jlimit (0.0f, 1.0f, norm)); };
        auto end = [this] (TurnSlot& t) { if (t.slot >= 0) proc.endSlot (machine, t.slot); };
        if (phase == 0)
        {
            tRt.slot = l480::algoMainRtSlot (proc.algoOf (machine));
            if (tRt.slot >= 0) { tRt.start = proc.slotNorm (machine, tRt.slot); if (! turnHomeSet) tRt.home = tRt.start; proc.beginSlot (machine, tRt.slot); }
            grab (tHf, { "HFC", "RTC", "ROL" }); grab (tSz, { "SIZ", "SIZE" }); grab (tPd, { "PDL", "RPD" }); grab (tDf, { "DIF", "DIFF" });
            turnHomeSet = true; return;
        }
        if (phase == 2) { for (auto* t : { &tRt, &tHf, &tSz, &tPd, &tDf }) end (*t); refreshPage (false); return; }
        if (what != 1)
        {
            if (twist) put (tSz, tSz.start + dx * 0.6f);
            else { put (tRt, tRt.start + dx * 0.5f); put (tHf, tHf.start - dy * 0.6f); }
        }
        if (what == 1 || (what == 0 && twist))
        {
            put (tPd, tPd.start + dx * 0.5f); put (tDf, tDf.start - dy * 0.6f);
        }
        String t;
        if (tRt.slot >= 0) t << "decay " << String (proc.mainRt (machine), 2) << " s";
        if (tHf.slot >= 0) t << "   " << proc.spec (machine, tHf.slot).name << " " << String (proc.slotValue (machine, tHf.slot), 0);
        showMessage ("Turning", t);
    }
    void resetTurn()
    {
        if (! turnHomeSet) return;
        for (auto* t : { &tRt, &tHf, &tSz, &tPd, &tDf })
            if (t->slot >= 0 && t->home >= 0) { proc.beginSlot (machine, t->slot); proc.setSlot (machine, t->slot, t->home); proc.endSlot (machine, t->slot); }
        turnHomeSet = false; refreshPage (false); showMessage ("Turning", "back to where it started");
    }
    // ---- the reflections or the tail in a window of their own: move it by its title, resize it from any edge or corner
    struct ViewWindow : public Component
    {
        Flexiverb480Editor* ed = nullptr; int mode = 1; std::unique_ptr<ReverbView> view;
        ComponentDragger dragger; ComponentBoundsConstrainer limits; std::unique_ptr<ResizableBorderComponent> border;
        ViewWindow (Flexiverb480Editor* e, int m) : ed (e), mode (m)
        {
            view = std::make_unique<ReverbView> (e->theme); view->mode = m; addAndMakeVisible (*view);
            limits.setMinimumSize (260, 180); limits.setMinimumOnscreenAmounts (40, 60, 40, 60);
            border = std::make_unique<ResizableBorderComponent> (this, &limits); border->setBorderThickness (BorderSize<int> (6)); addAndMakeVisible (*border);
        }
        Rectangle<int> titleR() const { return { 0, 0, getWidth(), 26 }; }
        Rectangle<int> closeR() const { return { getWidth() - 30, 3, 22, 22 }; }
        void paint (Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (b, 10);
            g.setColour (ed->theme.display); g.fillRoundedRectangle (b.reduced (1), 9);
            g.setColour (ed->theme.displayText.withAlpha (0.12f)); g.fillRoundedRectangle (titleR().toFloat().reduced (2, 2), 8);
            g.setColour (ed->theme.displayText.withAlpha (0.5f)); g.drawRoundedRectangle (b.reduced (1), 9, 1.2f);
            g.setFont (ui (12.0f, W::Bold, 0.08f)); g.setColour (ed->theme.displayText);
            g.drawText (mode == 1 ? "EARLY REFLECTIONS" : "THE TAIL", titleR().withTrimmedLeft (12), Justification::centredLeft, false);
            const auto c = closeR().toFloat();
            g.setColour (Colours::white.withAlpha (closeHover ? 0.25f : 0.1f)); g.fillEllipse (c);
            g.setColour (ed->theme.displayText); g.drawLine (c.getX() + 6, c.getY() + 6, c.getRight() - 6, c.getBottom() - 6, 2.0f); g.drawLine (c.getRight() - 6, c.getY() + 6, c.getX() + 6, c.getBottom() - 6, 2.0f);
        }
        void resized() override { view->setBounds (getLocalBounds().reduced (8).withTrimmedTop (24)); border->setBounds (getLocalBounds()); ed->proc.setUiProperty ("win" + String (mode), getBounds().toString()); }
        void moved() override { ed->proc.setUiProperty ("win" + String (mode), getBounds().toString()); }
        bool closeHover = false;
        void mouseMove (const MouseEvent& e) override { const bool h = closeR().contains (e.getPosition()); if (h != closeHover) { closeHover = h; repaint(); } }
        void mouseDown (const MouseEvent& e) override { if (closeR().contains (e.getPosition())) { auto* ep = ed; const int m = mode; MessageManager::callAsync ([ep, m] { ep->closeViewWindow (m); }); return; } dragger.startDraggingComponent (this, e); toFront (true); }
        void mouseDrag (const MouseEvent& e) override { dragger.dragComponent (this, e, &limits); }
    };
    std::unique_ptr<ViewWindow> viewWin[2];
    void openViewWindow (int m)
    {
        auto& w = viewWin[m - 1];
        if (w != nullptr) { w->toFront (true); return; }
        w = std::make_unique<ViewWindow> (this, m);
        auto& v = *w->view;
        for (auto* src : { &reverbView })
        {
            v.onShow = src->onShow; v.getPredelayMs = src->getPredelayMs; v.setPredelayMs = src->setPredelayMs; v.getRt = src->getRt; v.setRt = src->setRt; v.gesture = src->gesture;
            v.getBars = src->getBars; v.onAdd = src->onAdd; v.onToggle = src->onToggle; v.onDragBegin = src->onDragBegin; v.onDrag = src->onDrag; v.onDragEnd = src->onDragEnd;
            v.onTurn = src->onTurn; v.onTurnReset = src->onTurnReset; v.erRange = src->erRange; v.linkOn = src->linkOn; v.turnWhat = src->turnWhat;
        }
        v.setResult (renderer.get());
        content.addAndMakeVisible (*w);
        const auto saved = Rectangle<int>::fromString (proc.getUiProperty ("win" + String (m), String()).toString());
        w->setBounds (saved.getWidth() > 100 ? saved : Rectangle<int> (screen.getX() + (m == 1 ? 10 : 140), screen.getY() + 30, m == 1 ? 520 : 640, m == 1 ? 340 : 400));
        w->toFront (true);
    }
    void closeViewWindow (int m) { viewWin[m - 1].reset(); }
    void dragScreenDiv (int i, int dy)
    {
        const float h = (float) jmax (1, screen.getHeight() - 16);
        float a = (float) (double) proc.getUiProperty ("uiFrA", 0.56), b = (float) (double) proc.getUiProperty ("uiFrB", 0.22);
        if (i == 0) { a = jlimit (0.2f, 0.8f, a + dy / h); b = jlimit (0.08f, 0.9f - a, b); }
        else b = jlimit (0.08f, 0.92f - a, b + dy / h);
        proc.setUiProperty ("uiFrA", a); proc.setUiProperty ("uiFrB", b); layoutScreen();
    }
    // ---- the shortcut bar over the faders: right-click it to choose its keys
    struct Shortcut { String id; LarcKey* target; };
    std::vector<Shortcut> shortcutDefs()
    {
        std::vector<Shortcut> v = { { "PAGE", fnKey[6].get() }, { "MUTE", fnKey[5].get() }, { "MACH", fnKey[4].get() }, { "SAVE", saveKey.get() }, { "SETUP", setupKey.get() },
                                    { "GATE", gtOnKey.get() }, { "EQ", eqOnKey.get() }, { "COMP", cpOnKey.get() },
                                    { "SAT", satKey.get() }, { "DUCK", duckKey.get() } };
        return v;
    }
    StringArray shortcutIds() const { auto t = StringArray::fromTokens (proc.getUiProperty ("uiShortcuts", "PAGE,MUTE").toString(), ",", ""); t.removeEmptyStrings(); return t; }
    std::vector<std::unique_ptr<LarcKey>> scKeys; std::vector<LarcKey*> scTargets; Rectangle<int> scBarR;
    void buildShortcuts()
    {
        for (auto& k : scKeys) content.removeChildComponent (k.get());
        scKeys.clear(); scTargets.clear();
        const auto defs = shortcutDefs();
        for (auto& id : shortcutIds())
            for (auto& d : defs)
                if (d.id == id && d.target != nullptr)
                {
                    auto k = std::make_unique<LarcKey> (theme, id); k->kc = &Theme::keyBar; k->setComponentID ("mv:sc:" + id); content.addAndMakeVisible (*k);
                    auto* t = d.target;
                    k->onClick = [t] { if (t->onClick) t->onClick(); };
                    k->onPress = [t] { if (t->onPress) t->onPress(); };
                    k->onRelease = [t] { if (t->onRelease) t->onRelease(); };
                    k->onRightClick = [this] { shortcutMenu(); };
                    scTargets.push_back (t); scKeys.push_back (std::move (k));
                }
    }
    void shortcutMenu()
    {
        PopupMenu m; m.addSectionHeader ("Keys on this bar");
        const auto ids = shortcutIds(); const auto defs = shortcutDefs();
        for (int i = 0; i < (int) defs.size(); ++i) m.addItem (i + 1, defs[(size_t) i].id, true, ids.contains (defs[(size_t) i].id));
        m.showMenuAsync (PopupMenu::Options(), [this, defs] (int r)
        {
            if (r <= 0) return;
            auto ids = shortcutIds(); const auto& id = defs[(size_t) r - 1].id;
            if (ids.contains (id)) ids.removeString (id); else ids.add (id);
            proc.setUiProperty ("uiShortcuts", ids.joinIntoString (","));
            buildShortcuts(); layout(); content.repaint();
        });
    }
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
            auto* kp = k.get(); k->onRightClick = [this, i, kp] { keyMenu ((int) i, kp); };
            myKeys.push_back (std::move (k));
        }
    }
    void addMyKey()
    {
        MyDef d; d.program = proc.programOf (machine); d.user = proc.userPresetName;
        d.label = d.user.isNotEmpty() ? d.user : String (proc.prog (machine).name);
        myDefs.push_back (d); saveMyKeys(); rebuildMyKeys(); layout(); content.repaint();
    }
    void keyMenu (int mine, LarcKey* key = nullptr)
    {
        PopupMenu m;
        const String cur = proc.userPresetName.isNotEmpty() ? proc.userPresetName : String (proc.prog (machine).name);
        m.addItem (1, "Make a button for the current preset  (" + cur + ")");
        if (mine >= 0) { m.addSeparator(); m.addItem (2, "Rename this button..."); m.addItem (3, "Remove this button"); }
        if (key != nullptr) { m.addSeparator(); m.addItem (4, "Text size and colour of this button..."); }
        Component::SafePointer<LarcKey> kp (key);
        m.showMenuAsync (PopupMenu::Options(), [this, mine, kp] (int r)
        {
            if (r == 4 && kp != nullptr) { editText (kp->textId(), "Button " + kp->getText(), theme.keyPt, theme.keyText, kp.getComponent(), kp->getLocalBounds()); return; }
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
    // REVERB MIX: always at the right of the faders, whatever the page and the window size (the program's own Wet/Dry Mix)
    std::unique_ptr<LarcFader> mixFader; Target mixTg; Rectangle<int> mixNameR, mixValueR, mixBay;
    int mixColW() const { return jmax (82, roundToInt (theme.faderPt * 6.6f)) + 20; }
    void findMix()
    {
        mixTg = {}; mixTg.name = "Reverb Mix";
        const int algo = proc.algoOf (machine);
        for (int k = 0; k < l480::kMaxParams; ++k) { const auto& sp = proc.spec (machine, k); if (sp.lab != nullptr && String (sp.lab) == "MIX") { mixTg.slot = k; mixTg.lab = sp.lab; break; } }
        (void) algo;
        if (mixTg.slot < 0) mixTg.param = proc.apvts.getParameter ("revMix");   // the Classic engine has no mix of its own
        mixFader->enabled = true;
    }
    Viewport drawerView; Drawer drawer;
    ComboBox anOsBox; std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> anOsAtt;   // ANALOG: oversampling (same setting as in SETUP)
    ComboBox engineBox; int engineLabelY = 0, engineLabelX = 18; Rectangle<int> engineLabelR; std::vector<std::pair<String, Rectangle<int>>> titleR;
    VuMeter vu { theme };
    Viewport setupView;
    std::unique_ptr<ParamKey> ironKey, anOnKey, eqOnKey, cpOnKey, satKey, duckKey, gtOnKey, gtKeyKey;
    std::unique_ptr<LarcKey> orderKey; std::unique_ptr<ScaleKnob> gtKnob[3]; GateMeter gateMeter { theme };
    int stripOrder() const { return (int) proc.apvts.getRawParameterValue ("stripOrder")->load(); }
    std::unique_ptr<ScaleKnob> depthKnob, sysKnob[4], anKnob[5], eqKnob2[2], cpKnob[7];
    std::unique_ptr<DualKnob> eqKnob[4], cpDual[2];     // the compressor's Threshold + Ratio and Attack + Release, nested
    PresetBrowser browser; SetupPanel setup;
    GearButton topGear { theme }, stripGear { theme }; MovBox eqModelBox; std::unique_ptr<LarcKey> eqModelLab; std::unique_ptr<ScaleKnob> cleanKnob;
    HwPage hwPage { theme, proc.apvts }; PrefsWindow prefs { theme, setup, hwPage };
    void openPrefs (int page = -1) { prefs.setBounds (0, 0, baseWidth(), mainH); prefs.open (page < 0 ? prefs.page() : page); }

    // ---- moving controls: Ctrl+Shift + drag (Cmd+Shift on a Mac); the corner resizes; snaps to the neighbours' lines
    static String posKey (const String& id) { String k = "pos_"; for (auto c : id) k << (CharacterFunctions::isLetterOrDigit (c) ? String::charToString (c) : String ("_")); return k; }
    void savePos (Component& c)
    {
        const auto b = c.getBounds();
        proc.setUiProperty (posKey (c.getComponentID()), String (b.getX()) + " " + String (b.getY()) + " " + String (b.getWidth()) + " " + String (b.getHeight()));
    }
    void applyMoves (Component& par)
    {
        for (auto* c : par.getChildren())
        {
            const auto id = c->getComponentID();
            if (! id.startsWith ("mv:")) continue;
            const auto v = StringArray::fromTokens (proc.getUiProperty (posKey (id), "").toString(), " ", "");
            if (v.size() == 4) c->setBounds (v[0].getIntValue(), v[1].getIntValue(), jmax (12, v[2].getIntValue()), jmax (10, v[3].getIntValue()));
        }
    }
    void resetMoves()
    {
        auto& st = proc.apvts.state; StringArray gone;
        for (int i = 0; i < st.getNumProperties(); ++i) if (st.getPropertyName (i).toString().startsWith ("pos_")) gone.add (st.getPropertyName (i).toString());
        for (auto& g : gone) st.removeProperty (Identifier (g), nullptr);
        layout(); content.repaint();
    }
    struct Arranger : public MouseListener
    {
        Flexiverb480Editor& ed; Component* tgt = nullptr; Rectangle<int> start; bool resizing = false;
        explicit Arranger (Flexiverb480Editor& e) : ed (e) {}
        static Component* movable (Component* c) { while (c != nullptr && ! c->getComponentID().startsWith ("mv:")) c = c->getParentComponent(); return c; }
        void mouseDown (const MouseEvent& e) override
        {
            tgt = nullptr;
            if (! arrangeMods (e)) return;
            auto* c = movable (e.eventComponent);
            if (c == nullptr || c->getParentComponent() == nullptr) return;
            tgt = c; start = c->getBounds();
            const auto lp = e.getEventRelativeTo (c).position;
            resizing = lp.x > c->getWidth() - 16 && lp.y > c->getHeight() - 16;
            c->toFront (false);
        }
        void mouseDrag (const MouseEvent& e) override
        {
            if (tgt == nullptr) return;
            auto* par = tgt->getParentComponent();
            const auto d = e.getEventRelativeTo (par).getOffsetFromDragStart();
            auto b = start;
            if (resizing) b.setSize (jmax (16, start.getWidth() + d.x), jmax (12, start.getHeight() + d.y));
            else b.setPosition (start.getX() + d.x, start.getY() + d.y);
            // snap to the edges and centre lines of the neighbours (the same line)
            const int tol = 6; int bx = tol + 1, by = tol + 1;
            for (auto* o : par->getChildren())
            {
                if (o == tgt || ! o->isVisible() || ! o->getComponentID().startsWith ("mv:")) continue;
                const auto r = o->getBounds();
                const int xs[] = { r.getX(), r.getCentreX(), r.getRight() }, ys[] = { r.getY(), r.getCentreY(), r.getBottom() };
                std::vector<int> mx, my;
                if (resizing) { mx = { b.getRight() }; my = { b.getBottom() }; }
                else { mx = { b.getX(), b.getCentreX(), b.getRight() }; my = { b.getY(), b.getCentreY(), b.getBottom() }; }
                for (int a : mx) for (int t : xs) if (std::abs (t - a) < std::abs (bx)) bx = t - a;
                for (int a : my) for (int t : ys) if (std::abs (t - a) < std::abs (by)) by = t - a;
            }
            if (std::abs (bx) <= tol) { if (resizing) b.setWidth (b.getWidth() + bx); else b.translate (bx, 0); }
            if (std::abs (by) <= tol) { if (resizing) b.setHeight (b.getHeight() + by); else b.translate (0, by); }
            tgt->setBounds (b);
        }
        void mouseUp (const MouseEvent&) override { if (tgt != nullptr) ed.savePos (*tgt); tgt = nullptr; }
        void mouseDoubleClick (const MouseEvent& e) override
        {
            if (! arrangeMods (e)) return;
            if (auto* c = movable (e.eventComponent)) { ed.proc.setUiProperty (posKey (c->getComponentID()), ""); ed.layout(); }
        }
    } arranger { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Flexiverb480Editor)
};
