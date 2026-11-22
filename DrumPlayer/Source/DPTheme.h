// Drum Player: the user's look settings. Every window part, button and text can get its own
// three-colour gradient, 3D depth and font size. Anything not customised draws exactly as designed.
// Saved to Documents/BD Plugins/Drum Player/Theme.xml, shared by every Drum Player.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>

namespace dp {
using namespace juce;

struct ThemeEl
{
    bool custom = false;                  // use the colours below
    Colour c0 { 0xff3a3e46 }, c1 { 0xff2d3139 }, c2 { 0xff22252b };   // top, middle, bottom
    bool threeD = true;
    float depth = 1.0f;                   // 0 … 2
    float fontSize = 0.0f;                // 0 = as designed
};

class Theme
{
public:
    static Theme& get() { static Theme t; return t; }
    const ThemeEl* find (const String& id) const { auto it = els.find (id); return it == els.end() ? nullptr : &it->second; }
    const ThemeEl* findCustom (const String& id) const { auto* e = find (id); return (e != nullptr && e->custom) ? e : nullptr; }
    float fontFor (const String& id, const String& group, float designed) const
    {
        if (auto* e = find (id); e != nullptr && e->fontSize > 0) return e->fontSize;
        if (auto* e = find (group); e != nullptr && e->fontSize > 0) return e->fontSize;
        return designed;
    }
    ThemeEl& edit (const String& id) { return els[id]; }
    void reset (const String& id) { els.erase (id); changed(); }
    void resetAll() { els.clear(); changed(); }
    void changed() { ++version; save(); }
    int version = 0;

    File file() const
    {
        auto d = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("BD Plugins").getChildFile ("Drum Player");
        d.createDirectory();
        return d.getChildFile ("Theme.xml");
    }
    void save() const
    {
        XmlElement x ("DrumPlayerTheme");
        for (auto& [id, e] : els) {
            auto* c = x.createNewChildElement ("E");
            c->setAttribute ("id", id); c->setAttribute ("custom", e.custom);
            c->setAttribute ("c0", e.c0.toString()); c->setAttribute ("c1", e.c1.toString()); c->setAttribute ("c2", e.c2.toString());
            c->setAttribute ("d3", e.threeD); c->setAttribute ("depth", e.depth); c->setAttribute ("font", e.fontSize);
        }
        x.writeTo (file());
    }
    void load()
    {
        els.clear();
        if (auto x = XmlDocument::parse (file()))
            for (auto* c : x->getChildIterator()) {
                ThemeEl e;
                e.custom = c->getBoolAttribute ("custom");
                e.c0 = Colour::fromString (c->getStringAttribute ("c0", e.c0.toString()));
                e.c1 = Colour::fromString (c->getStringAttribute ("c1", e.c1.toString()));
                e.c2 = Colour::fromString (c->getStringAttribute ("c2", e.c2.toString()));
                e.threeD = c->getBoolAttribute ("d3", true); e.depth = (float) c->getDoubleAttribute ("depth", 1.0);
                e.fontSize = (float) c->getDoubleAttribute ("font", 0.0);
                els[c->getStringAttribute ("id")] = e;
            }
        ++version;
    }
    std::map<String, ThemeEl> els;
private:
    Theme() { load(); }
};

// fill a rounded shape with an element's gradient and 3D shading
inline void fillThemed (Graphics& g, Rectangle<float> r, const ThemeEl& e, float radius)
{
    ColourGradient grad (e.c0, r.getX(), r.getY(), e.c2, r.getX(), r.getBottom(), false);
    grad.addColour (0.5, e.c1);
    if (e.threeD && e.depth > 0.01f) {
        g.setColour (Colours::black.withAlpha (jmin (0.6f, 0.3f * e.depth))); g.fillRoundedRectangle (r.translated (0, 1.5f * e.depth), radius);
    }
    g.setGradientFill (grad); g.fillRoundedRectangle (r, radius);
    if (e.threeD && e.depth > 0.01f) {
        g.setColour (Colours::white.withAlpha (jmin (0.4f, 0.14f * e.depth)));
        g.drawLine (r.getX() + radius, r.getY() + 1.0f, r.getRight() - radius, r.getY() + 1.0f, 1.0f);
        g.setColour (Colours::black.withAlpha (jmin (0.6f, 0.25f * e.depth)));
        g.drawLine (r.getX() + radius, r.getBottom() - 0.5f, r.getRight() - radius, r.getBottom() - 0.5f, 1.0f);
    }
}

// the parts of the window that can be styled (buttons and texts are found on screen)
inline StringArray themePanels()
{
    return { "Window background", "Header", "Keyboard strip", "Parking tray", "Toolbars", "Ruler", "Lane names",
             "Note grid", "Velocity lane", "Roll card", "Chain strip", "Song lane", "Menu bar", "Pages", "Messages", "Buttons", "Lit buttons", "Boxes and menus", "Labels" };
}

} // namespace dp
