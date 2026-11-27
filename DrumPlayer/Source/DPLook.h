// Drum Player look: the Music Practice app's dark theme (charcoal cards, sky blue, warm amber lights),
// embedded Inter font drawn as vector outlines so text stays sharp at every zoom.
#pragma once
#include "DPProcessor.h"
#include "FlexFonts.h"
#include "DPTheme.h"

namespace dp {
namespace th {
    static const Colour bg { 0xff121316 }, panel { 0xff1a1c21 }, surface { 0xff22252b }, raised { 0xff2d3139 },
                        line { 0x1cffffff }, text { 0xeaffffff }, dim { 0x8cffffff }, faint { 0x40ffffff },
                        blue { 0xff7fd1ff }, amber { 0xffffc15e }, red { 0xffff8fa3 }, green { 0xffb8f28b },
                        violet { 0xffc9a7ff }, teal { 0xff6ee7d8 }, orange { 0xffffa36e }, lemon { 0xffe3e37a },
                        ink { 0xff201800 };
    inline Colour pal (int i)
    {
        static const uint32 c[] = { 0xFFFFC15E, 0xFF7FD1FF, 0xFFB8F28B, 0xFFFF8FA3, 0xFFC9A7FF, 0xFF6EE7D8, 0xFFFFA36E, 0xFFE3E37A };
        return Colour (c[((i % 8) + 8) % 8]);
    }
    inline Colour typeColour (PatternType t)
    {
        switch (t) { case PatternType::Groove: return blue; case PatternType::Fill: return amber; case PatternType::Intro: return green; case PatternType::Ending: return red; }
        return blue;
    }
    // one colour per note value for the adaptive grid
    inline Colour valueColour (int vi)
    {
        static const uint32 c[] = { 0xFFFFFFFF, 0xFF7FD1FF, 0xFFC9A7FF, 0xFF6EE7D8, 0xFFFF8FA3, 0xFFB8F28B, 0xFFFFA36E, 0xFFE3E37A, 0xFFFF6FD8, 0xFFFFC15E, 0xFF9AA8FF };
        return Colour (c[jlimit (0, numNoteValues - 1, vi)]);
    }
}

struct Fonts
{
    Typeface::Ptr reg, med, semi, bold;
    static Fonts& get() { static Fonts f; return f; }
    Fonts()
    {
        reg  = Typeface::createSystemTypefaceFor (flexfonts::InterRegular,  (size_t) flexfonts::InterRegularSize);
        med  = Typeface::createSystemTypefaceFor (flexfonts::InterMedium,   (size_t) flexfonts::InterMediumSize);
        semi = Typeface::createSystemTypefaceFor (flexfonts::InterSemiBold, (size_t) flexfonts::InterSemiBoldSize);
        bold = Typeface::createSystemTypefaceFor (flexfonts::InterBold,     (size_t) flexfonts::InterBoldSize);
    }
};
// weight: 0 regular, 1 medium, 2 semibold, 3 bold
inline Font font (float h, int weight = 1)
{
    auto& f = Fonts::get();
    auto tf = weight <= 0 ? f.reg : weight == 1 ? f.med : weight == 2 ? f.semi : f.bold;
    return Font (FontOptions (tf).withHeight (h * Theme::get().textScale));     // "All text" size applies everywhere
}
// a pattern's colour: its group's, or else by type (groove, fill...)
inline Colour patternColour (const Song& s, const Pattern& p)
{
    if (p.group >= 0 && p.group < (int) s.groups.size()) return Colour (s.groups[(size_t) p.group].colour);
    return th::typeColour (p.type);
}
inline float textWidth (const Font& f, const String& s) { return GlyphArrangement::getStringWidth (f, s); }

// button height follows the "All buttons" size in the Look editor
struct ButtonHeight { operator int() const { return (int) std::round (22.0f * Theme::get().buttonScale); } };
inline const ButtonHeight btnH {};

class DPLnF : public LookAndFeel_V4
{
public:
    DPLnF()
    {
        setColour (ResizableWindow::backgroundColourId, th::bg);
        setColour (PopupMenu::backgroundColourId, th::panel);
        setColour (PopupMenu::textColourId, th::text);
        setColour (PopupMenu::highlightedBackgroundColourId, th::blue.withAlpha (0.22f));
        setColour (PopupMenu::highlightedTextColourId, th::text);
        setColour (PopupMenu::headerTextColourId, th::amber);
        setColour (ComboBox::backgroundColourId, th::surface);
        setColour (ComboBox::textColourId, th::text);
        setColour (ComboBox::outlineColourId, th::line);
        setColour (ComboBox::arrowColourId, th::dim);
        setColour (Label::textColourId, th::text);
        setColour (TextEditor::backgroundColourId, th::surface);
        setColour (TextEditor::textColourId, th::text);
        setColour (TextEditor::outlineColourId, th::line);
        setColour (TextEditor::focusedOutlineColourId, th::blue.withAlpha (0.7f));
        setColour (TextEditor::highlightColourId, th::blue.withAlpha (0.35f));
        setColour (CaretComponent::caretColourId, th::amber);
        setColour (ScrollBar::thumbColourId, Colour (0x40ffffff));
        setColour (ListBox::backgroundColourId, th::panel);
        setColour (ListBox::outlineColourId, th::line);
        setColour (Slider::textBoxTextColourId, th::text);
        setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (TooltipWindow::backgroundColourId, th::raised);
        setColour (TooltipWindow::textColourId, th::text);
        setColour (AlertWindow::backgroundColourId, th::panel);
        setColour (AlertWindow::textColourId, th::text);
        setColour (TextButton::buttonColourId, th::surface);
        setColour (TextButton::textColourOffId, th::text);
    }
    Typeface::Ptr getTypefaceForFont (const Font& f) override
    {
        auto& F = Fonts::get();
        return f.isBold() ? F.semi : F.med;
    }
    void drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        const float cr = 5.0f;
        {   // the user's look, if set: this button's own style, else the lit / all-buttons style
            auto& T = Theme::get();
            const ThemeEl* e = on ? T.findCustom ("Lit buttons") : nullptr;
            if (e == nullptr) e = T.findCustom ("btn:" + b.getComponentID());
            if (e == nullptr && ! on) e = T.findCustom ("Buttons");
            if (e != nullptr) {
                fillThemed (g, r, *e, cr);
                if (on && T.findCustom ("Lit buttons") == nullptr) { g.setColour (th::amber.withAlpha (0.55f)); g.fillRoundedRectangle (r, cr); }
                if (over || down) { g.setColour (Colours::white.withAlpha (down ? 0.14f : 0.06f)); g.fillRoundedRectangle (r, cr); }
                if (! b.isEnabled()) { g.setColour (Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r, cr); }
                if (b.getProperties().contains ("rmenu")) {
                    const float t = jlimit (4.0f, 6.0f, r.getHeight() * 0.22f);
                    Path p; p.addTriangle (r.getX() + 2, r.getBottom() - 2 - t, r.getX() + 2, r.getBottom() - 2, r.getX() + 2 + t, r.getBottom() - 2);
                    g.setColour (on ? th::ink.withAlpha (0.55f) : th::faint); g.fillPath (p);
                }
                return;
            }
        }
        // soft drop shadow so the key looks raised
        g.setColour (Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (r.translated (0, 1.0f), cr);
        // a button can carry its own lamp colour (Play green, Stop and Rec red); otherwise amber
        const Colour lamp = b.getProperties().contains ("lit") ? Colour ((uint32) (int) b.getProperties()["lit"]) : th::amber;
        // a button can also carry a dim colour for when it's off (Rec and Play green, Stop blue)
        const bool hasIdle = b.getProperties().contains ("idle");
        const Colour idle = hasIdle ? Colour ((uint32) (int) b.getProperties()["idle"]) : th::raised;
        Colour top = on ? lamp.brighter (0.15f) : hasIdle ? idle.brighter (0.12f) : th::raised.brighter (0.06f);
        const bool rmenu = b.getProperties().contains ("rmenu");
        Colour bot = on ? lamp.darker (0.25f) : hasIdle ? idle.darker (0.35f) : th::surface;
        if (! b.isEnabled()) { top = top.withMultipliedAlpha (0.45f); bot = bot.withMultipliedAlpha (0.45f); }
        g.setGradientFill (ColourGradient (top, r.getTopLeft(), bot, r.getBottomLeft(), false));
        g.fillRoundedRectangle (r, cr);
        if (on) {                                                   // lamp glow
            g.setColour (lamp.withAlpha (0.25f)); g.drawRoundedRectangle (r.expanded (1.0f), cr + 1, 2.0f);
        }
        g.setColour (Colours::white.withAlpha (on ? 0.35f : 0.08f)); g.drawLine (r.getX() + cr, r.getY() + 1.0f, r.getRight() - cr, r.getY() + 1.0f, 1.0f);
        g.setColour (on ? lamp.darker (0.4f) : Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (r, cr, 0.8f);
        if (over || down) { g.setColour (Colours::white.withAlpha (down ? 0.14f : 0.06f)); g.fillRoundedRectangle (r, cr); }
        if (rmenu) {                                                // a little corner: this one has a right-click menu
            const float t = jlimit (4.0f, 6.0f, r.getHeight() * 0.22f);
            Path p; p.addTriangle (r.getX() + 2, r.getBottom() - 2 - t, r.getX() + 2, r.getBottom() - 2, r.getX() + 2 + t, r.getBottom() - 2);
            g.setColour (on ? th::ink.withAlpha (0.55f) : th::faint); g.fillPath (p);
        }
    }
    Font getTextButtonFont (TextButton&, int) override { return font (12.0f, 2); }
    void drawButtonText (Graphics& g, TextButton& b, bool, bool) override
    {
        const String id = "btn:" + b.getComponentID();
        g.setFont (font (Theme::get().fontFor (id, "Buttons", 12.0f), Theme::get().boldFor (id, "Buttons", true) ? 2 : 1));
        g.setColour (b.getToggleState() ? th::ink : (b.isEnabled() ? Theme::get().textFor (id, "Buttons", th::text) : th::faint));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), Justification::centred, 1);
    }
    void drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box) override
    {
        auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        if (auto* e = Theme::get().findCustom ("Boxes and menus")) {
            fillThemed (g, r, *e, 5);
            Path p; const float ax = (float) w - 10.0f, ay = (float) h * 0.5f;
            p.startNewSubPath (ax - 3.5f, ay - 1.8f); p.lineTo (ax, ay + 1.8f); p.lineTo (ax + 3.5f, ay - 1.8f);
            g.setColour (th::dim); g.strokePath (p, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
            return;
        }
        g.setColour (Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (r.translated (0, 1.0f), 5);
        g.setGradientFill (ColourGradient (th::raised, r.getTopLeft(), th::surface, r.getBottomLeft(), false));
        g.fillRoundedRectangle (r, 5);
        g.setColour (Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (r, 5, 0.8f);
        Path p; const float ax = (float) w - 10.0f, ay = (float) h * 0.5f;
        p.startNewSubPath (ax - 3.5f, ay - 1.8f); p.lineTo (ax, ay + 1.8f); p.lineTo (ax + 3.5f, ay - 1.8f);
        g.setColour (box.isEnabled() ? th::dim : th::faint); g.strokePath (p, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    Font getComboBoxFont (ComboBox&) override { return font (Theme::get().fontFor ("", "Boxes and menus", 12.0f), Theme::get().boldFor ("Boxes and menus", "", true) ? 2 : 1); }
    Font getLabelFont (Label& l) override
    {
        auto f = l.getFont();
        const float base = (float) (double) l.getProperties().getWithDefault ("baseH", f.getHeight() / Theme::get().textScale);
        const float h = Theme::get().fontFor ("txt:" + l.getComponentID(), "Labels", 0.0f);
        return f.withHeight ((h > 0 ? h : base) * Theme::get().textScale);
    }
    void positionComboBoxText (ComboBox& b, Label& l) override
    {
        l.setBounds (6, 0, b.getWidth() - 20, b.getHeight());
        l.setFont (getComboBoxFont (b)); l.setJustificationType (Justification::centredLeft);
    }
    // menus open outside the scaled window, so they get the window's zoom here; as big as the buttons' text (or your size)
    static inline float menuScale = 1.0f;
    float menuSize() const { return Theme::get().fontFor ("Popup menus", "", 14.0f) * menuScale * Theme::get().textScale; }
    Font getPopupMenuFont() override { return font (menuSize(), Theme::get().boldFor ("Popup menus", "", false) ? 2 : 1); }
    void drawPopupMenuItem (Graphics& g, const Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted, bool isTicked, bool hasSubMenu,
                            const String& text, const String& shortcutKeyText, const Drawable* icon, const Colour* textColour) override
    {
        Colour own = Theme::get().textFor ("Popup menus", "", Colour());
        LookAndFeel_V4::drawPopupMenuItem (g, area, isSeparator, isActive, isHighlighted, isTicked, hasSubMenu, text, shortcutKeyText, icon,
                                           textColour != nullptr ? textColour : (own != Colour() ? &own : nullptr));
    }
    // headings in menus (the key's name, its bars and type): big, bold, easy to see
    void drawPopupMenuSectionHeader (Graphics& g, const Rectangle<int>& area, const String& text) override
    {
        const float sz = Theme::get().fontFor ("Menu headings", "", 15.0f) * menuScale * Theme::get().textScale;
        g.setFont (font (sz, Theme::get().boldFor ("Menu headings", "", true) ? 3 : 1));
        g.setColour (Theme::get().textFor ("Menu headings", "", th::amber));
        g.drawFittedText (text, area.getX() + 12, area.getY(), area.getWidth() - 16, area.getHeight(), Justification::centredLeft, 1, 0.8f);
    }
    void drawPopupMenuBackground (Graphics& g, int w, int h) override
    {
        g.fillAll (th::panel);
        g.setColour (th::line); g.drawRect (0, 0, w, h);
    }
    void drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle st, Slider& s) override
    {
        if (st != Slider::LinearHorizontal) { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, st, s); return; }
        const float cy = y + h * 0.5f;
        auto track = Rectangle<float> ((float) x, cy - 2.5f, (float) w, 5.0f);
        g.setColour (Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (track, 2.5f);
        const auto fillC = s.findColour (Slider::trackColourId);
        g.setColour (fillC); g.fillRoundedRectangle (track.withRight (pos), 2.5f);
        auto thumb = Rectangle<float> (13, 13).withCentre ({ pos, cy });
        g.setColour (Colours::black.withAlpha (0.4f)); g.fillEllipse (thumb.translated (0, 1));
        g.setGradientFill (ColourGradient (Colours::white, thumb.getTopLeft(), Colour (0xffc8ccd4), thumb.getBottomLeft(), false));
        g.fillEllipse (thumb);
    }
    Label* createSliderTextBox (Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        l->setFont (font (11.5f, 1));
        l->setColour (Label::textColourId, th::text);
        l->setColour (Label::outlineColourId, Colours::transparentBlack);
        l->setColour (Label::backgroundColourId, Colours::transparentBlack);
        return l;
    }
    void drawScrollbar (Graphics& g, ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbPos, int thumbSize, bool over, bool) override
    {
        auto r = vertical ? Rectangle<float> ((float) x + 2, (float) thumbPos, (float) w - 4, (float) thumbSize)
                          : Rectangle<float> ((float) thumbPos, (float) y + 2, (float) thumbSize, (float) h - 4);
        g.setColour (Colours::white.withAlpha (over ? 0.28f : 0.16f)); g.fillRoundedRectangle (r, 3);
    }
    int getDefaultScrollbarWidth() override { return 10; }
    void drawMenuBarBackground (Graphics& g, int w, int h, bool, MenuBarComponent&) override
    {
        if (auto* e = Theme::get().findCustom ("Menu bar")) { fillThemed (g, Rectangle<float> (0, 0, (float) w, (float) h), *e, 4); return; }
        g.setColour (th::panel); g.fillRoundedRectangle (Rectangle<float> (0, 0, (float) w, (float) h), 4);
    }
    void drawMenuBarItem (Graphics& g, int w, int h, int, const String& text, bool over, bool open, bool, MenuBarComponent&) override
    {
        if (over || open) { g.setColour (th::blue.withAlpha (0.22f)); g.fillRoundedRectangle (Rectangle<float> (1, 2, (float) w - 2, (float) h - 4), 3); }
        g.setFont (menuBarFont()); g.setColour (open ? th::amber : Theme::get().textFor ("Menu bar", "", th::text));
        g.drawFittedText (text, 0, 0, w, h, Justification::centred, 1);
    }
    Font menuBarFont() const { return font (Theme::get().fontFor ("txt:Menu bar", "Menu bar", 14.0f), Theme::get().boldFor ("Menu bar", "", true) ? 2 : 1); }
    Font getMenuBarFont (MenuBarComponent&, int, const String&) override { return menuBarFont(); }
    int getMenuBarItemWidth (MenuBarComponent&, int, const String& t) override { return (int) textWidth (menuBarFont(), t) + 24; }
    void fillTextEditorBackground (Graphics& g, int w, int h, TextEditor&) override
    {
        g.setColour (Colours::black.withAlpha (0.25f)); g.fillRoundedRectangle (Rectangle<float> (0, 0, (float) w, (float) h), 5);
    }
    void drawTextEditorOutline (Graphics& g, int w, int h, TextEditor& t) override
    {
        g.setColour (t.hasKeyboardFocus (true) ? th::blue.withAlpha (0.6f) : th::line);
        g.drawRoundedRectangle (Rectangle<float> (0.5f, 0.5f, (float) w - 1, (float) h - 1), 5, 1.0f);
    }
};

// ---------------------------------------------------------------- small helpers
inline int fitW (const String& t, int minW = 0) { return jmax (minW, (int) std::ceil (textWidth (font (12.0f, 2), t) + 16.0f * Theme::get().buttonScale)); }

inline void setupButton (Component& parent, TextButton& b, const String& text, bool toggle = false, const String& tip = {})
{
    b.setButtonText (text);
    if (b.getComponentID().isEmpty()) b.setComponentID (text);       // the name the Look editor knows it by
    b.setClickingTogglesState (toggle);
    if (tip.isNotEmpty()) b.setTooltip (tip);
    b.setSize (fitW (text), btnH);
    b.setWantsKeyboardFocus (false);
    parent.addAndMakeVisible (b);
}
inline void setupCombo (Component& parent, ComboBox& c, const StringArray& items, int firstId = 1, const String& tip = {})
{
    c.clear (dontSendNotification);
    c.addItemList (items, firstId);
    if (tip.isNotEmpty()) c.setTooltip (tip);
    c.setWantsKeyboardFocus (false);
    parent.addAndMakeVisible (c);
}
inline void setupLabel (Component& parent, Label& l, const String& text, float size = 11.5f, Colour c = th::dim)
{
    l.setText (text, dontSendNotification); l.setFont (font (size, 2)); l.setColour (Label::textColourId, c);
    l.getProperties().set ("baseH", size);
    if (l.getComponentID().isEmpty()) l.setComponentID (text.upToFirstOccurrenceOf ("\n", false, false).substring (0, 40));
    l.setJustificationType (Justification::centredLeft); l.setInterceptsMouseClicks (false, false);
    parent.addAndMakeVisible (l);
}
inline void setupSlider (Component& parent, Slider& s, Colour c = th::blue)
{
    s.setSliderStyle (Slider::LinearHorizontal);
    s.setTextBoxStyle (Slider::TextBoxRight, false, 52, btnH);
    s.setColour (Slider::trackColourId, c);
    s.setWantsKeyboardFocus (false);
    parent.addAndMakeVisible (s);
}

// lays components out left to right at their own widths
struct RowPlacer
{
    int x, y, h, gap;
    RowPlacer (Rectangle<int> r, int g = 5) : x (r.getX()), y (r.getY() + (r.getHeight() - btnH) / 2), h (btnH), gap (g) {}
    void place (Component& c, int w = -1) { if (w < 0) w = c.getWidth(); c.setBounds (x, y, w, h); x += w + gap; }
    void skip (int w) { x += w; }
};

// card background like the practice app: rounded charcoal panel with a hairline
inline void drawCard (Graphics& g, Rectangle<float> r, Colour c = th::panel, float radius = 10.0f, const String& themeId = {})
{
    if (themeId.isNotEmpty()) if (auto* e = Theme::get().findCustom (themeId)) { fillThemed (g, r, *e, radius); return; }
    g.setColour (Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (r.translated (0, 2), radius);
    g.setColour (c); g.fillRoundedRectangle (r, radius);
    g.setColour (th::line); g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}


// background of a styled area: the user's gradient if set, else the designed colour
inline void fillArea (Graphics& g, Rectangle<int> r, const String& themeId, Colour designed)
{
    if (auto* e = Theme::get().findCustom (themeId)) { fillThemed (g, r.toFloat(), *e, 0.0f); return; }
    g.setColour (designed); g.fillRect (r);
}
inline bool themed (const String& id) { return Theme::get().findCustom (id) != nullptr; }

} // namespace dp
