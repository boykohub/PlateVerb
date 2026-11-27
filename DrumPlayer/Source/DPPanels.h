// Drum Player: keyboard strip, parking tray, and the pop-over pages (quantize, humanize, chains, settings, presets).
#pragma once
#include "DPRoll.h"

namespace dp {

// fine slider: drag = normal, Shift or Alt = ten times finer, wheel = small steps, double-click = default
class FineSlider : public Slider
{
public:
    FineSlider() { setVelocityBasedMode (false); }
    void mouseDown (const MouseEvent& e) override
    {
        if (e.mods.isShiftDown() || e.mods.isAltDown()) { fine = true; startVal = getValue(); startX = e.position.x; return; }
        fine = false; Slider::mouseDown (e);
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (! fine) { Slider::mouseDrag (e); return; }
        const double range = getMaximum() - getMinimum();
        const double w = jmax (40, getWidth() - getTextBoxWidth());
        setValue (startVal + (e.position.x - startX) / w * range / 10.0, sendNotificationSync);
    }
    void mouseUp (const MouseEvent& e) override { if (! fine) Slider::mouseUp (e); fine = false; }
private:
    bool fine = false; double startVal = 0; float startX = 0;
};

// ---------------------------------------------------------------- base for the pop-over pages
class Page : public Component
{
public:
    explicit Page (String t) : title (std::move (t)) {}
    bool embedded = false;                    // shown inside another page (as a tab): no card or title of its own
    void paint (Graphics& g) override
    {
        if (embedded) return;
        drawCard (g, getLocalBounds().toFloat().reduced (2), th::panel, 12, "Pages");
        g.setFont (font (13.0f, 3)); g.setColour (th::amber);
        g.drawText (title, 16, 8, getWidth() - 32, 22, Justification::centredLeft);
        g.setFont (font (10.5f, 1)); g.setColour (th::faint);
        g.drawText ("press its button again to close", 16, 8, getWidth() - 56, 22, Justification::centredRight);
    }
    virtual void refresh() {}
    String title;
protected:
    Rectangle<int> content() const { return embedded ? getLocalBounds() : getLocalBounds().reduced (16, 0).withTrimmedTop (38).withTrimmedBottom (12); }
    // a labelled row
    Rectangle<int> row (Rectangle<int>& area, Label& l, int labelW = 130)
    {
        auto r = area.removeFromTop (28); area.removeFromTop (4);
        l.setBounds (r.removeFromLeft (labelW));
        return r;
    }
};

// ---------------------------------------------------------------- keyboard strip
class KeyStrip : public Component, public DragAndDropTarget, public FileDragAndDropTarget, public SettableTooltipClient
{
public:
    KeyStrip (EditState& e, bool big) : st (e), large (big)
    {
        ignoreUnused (big);      // no pop-up text over the keys
    }
    std::function<void (const StringArray&, int key)> onFiles;
    std::function<void (int key)> onKeyMenu;
    int dropHover = -1;

    void paint (Graphics& g) override
    {
        auto& s = st.song();
        updateView();
        const int lo = viewLow, hi = viewEnd;
        const int play = st.proc.playingKey.load(), queued = st.proc.queuedKey.load();
        const bool blink = (Time::getMillisecondCounter() / 250) % 2 == 0;
        fillArea (g, getLocalBounds(), "Keyboard strip", th::bg);
        // white keys
        for (int k = lo; k <= hi; ++k) {
            if (isBlackKey (k)) continue;
            auto r = keyRect (k);
            const auto& p = s.keys[(size_t) k];
            const bool usable = isPatternKey (k) || s.settings.isStop (k);
            g.setGradientFill (ColourGradient (Colour (0xffe9eaee).withMultipliedBrightness (usable ? 1.0f : 0.55f), r.getTopLeft(),
                                               Colour (0xffb9bcc4).withMultipliedBrightness (usable ? 1.0f : 0.55f), r.getBottomLeft(), false));
            g.fillRoundedRectangle (r.reduced (0.5f, 0), 3);
            if (p.group >= 0 && p.group < (int) s.groups.size()) { g.setColour (Colour (s.groups[(size_t) p.group].colour).withAlpha (0.28f)); g.fillRoundedRectangle (r.reduced (0.5f, 0), 3); }
            g.setColour (Colours::black.withAlpha (0.55f)); g.drawRoundedRectangle (r.reduced (0.5f, 0), 3, 1.0f);
            drawTile (g, r.withTrimmedTop (r.getHeight() * 0.56f).reduced (2.5f, 3), k, p, false);
            decorate (g, r, k, play, queued, blink);
        }
        // black keys on top
        for (int k = lo; k <= hi; ++k) {
            if (! isBlackKey (k)) continue;
            auto r = keyRect (k);
            if (r.isEmpty()) continue;
            const auto& p = s.keys[(size_t) k];
            g.setColour (Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (r.translated (1.5f, 2), 3);
            g.setGradientFill (ColourGradient (Colour (0xff3a3e46), r.getTopLeft(), Colour (0xff101114), r.getBottomLeft(), false));
            g.fillRoundedRectangle (r, 3);
            if (p.group >= 0 && p.group < (int) s.groups.size()) { g.setColour (Colour (s.groups[(size_t) p.group].colour).withAlpha (0.35f)); g.fillRoundedRectangle (r, 3); }
            drawTile (g, r.withTrimmedTop (r.getHeight() * 0.3f).reduced (2, 3), k, p, true);
            decorate (g, r, k, play, queued, blink);
        }
        // octave labels
        g.setFont (font (9.0f * Theme::get().keyTextScale, 2)); g.setColour (Colours::black.withAlpha (0.55f));
        for (int k = lo; k <= hi; ++k) if (k % 12 == 0) { auto r = keyRect (k); g.drawText (keyName (k), r.withHeight (14).translated (0, (int) (r.getHeight() * 0.42f)), Justification::centred); }
        // while dragging, a tag says exactly which key the groove will land on
        if (dropHover >= 0) {
            auto kr = keyRect (dropHover);
            const String t = "Drop on " + keyName (dropHover) + (s.keys[(size_t) dropHover].isEmpty() ? String() : "  (replaces " + s.keys[(size_t) dropHover].name + ")");
            g.setFont (font (11.5f, 3));
            const float tw = textWidth (g.getCurrentFont(), t) + 14;
            auto tag = Rectangle<float> (jlimit (0.0f, (float) getWidth() - tw, kr.getCentreX() - tw / 2), 2.0f, tw, 18.0f);
            g.setColour (th::ink.withAlpha (0.9f)); g.fillRoundedRectangle (tag, 4);
            g.setColour (th::green); g.drawText (t, tag, Justification::centred);
        }
        // scroll arrows: the whole keyboard is there, an octave per click
        drawArrow (g, leftArrow(), true, viewLow > firstKey());
        drawArrow (g, rightArrow(), false, viewEnd < s.settings.zoneHigh);
    }

    // ------------------------------------------------------------ geometry (a scrolling window onto the whole keyboard)
    int firstKey() const
    {
        const auto& o = st.song().settings;
        int f = o.zoneLow;
        for (int stop : { o.resolveStop(), o.stopNote2 }) if (stop >= 0 && stop < f) f = stop;
        while (f > 0 && isBlackKey (f)) --f;
        return f;
    }
    bool isPatternKey (int k) const { const auto& o = st.song().settings; return k >= o.zoneLow && k <= o.zoneHigh && ! o.isStop (k); }
    int visibleWhites() const { return jmax (5, (int) ((getWidth() - 2 * arrowW) / ((large ? 30.0f : 24.0f) * Theme::get().keyScale))); }
    void updateView()
    {
        const int hi = st.song().settings.zoneHigh;
        if (viewLow < 0) viewLow = firstKey();
        viewLow = jlimit (firstKey(), jmax (firstKey(), hi), viewLow);
        while (viewLow > firstKey() && isBlackKey (viewLow)) --viewLow;
        // pull back if the window runs past the top
        auto endFor = [this, hi] (int from) { int w = 0, k = from; for (; k <= hi; ++k) { if (! isBlackKey (k)) { if (w == visibleWhites()) break; ++w; } } return k - 1; };
        viewEnd = endFor (viewLow);
        while (viewEnd >= hi && viewLow - 12 >= firstKey() && endFor (viewLow - 12) >= hi) { viewLow -= 12; viewEnd = endFor (viewLow); }
    }
    void scrollOctaves (int n) { viewLow += 12 * n; updateView(); repaint(); }
    // the key you just pressed on your keyboard: jump to it and light it for a moment
    void flash (int note)
    {
        if (note < firstKey() || note > st.song().settings.zoneHigh) return;
        flashNote = note; flashUntil = Time::getMillisecondCounter() + 350;
        ensureVisible (note);
        Timer::callAfterDelay (400, [sp = Component::SafePointer<KeyStrip> (this)] { if (sp != nullptr) sp->repaint(); });
    }
    int flashNote = -1; uint32 flashUntil = 0;
    void ensureVisible (int k)
    {
        updateView();
        if (k < viewLow) { while (k < viewLow && viewLow > firstKey()) viewLow -= 12; }
        else if (k > viewEnd) { while (k > viewEnd && viewEnd < st.song().settings.zoneHigh) { viewLow += 12; updateView(); } }
        updateView(); repaint();
    }
    Rectangle<float> keyRect (int k) const
    {
        if (k < viewLow || k > viewEnd) return {};
        const float ww = (float) (getWidth() - 2 * arrowW) / visibleWhites();
        int wi = 0; for (int i = viewLow; i < k; ++i) if (! isBlackKey (i)) ++wi;
        const float x0 = (float) arrowW;
        if (! isBlackKey (k)) return { x0 + wi * ww, 0.0f, ww, (float) getHeight() };
        return { x0 + wi * ww - ww * 0.31f, 0.0f, ww * 0.62f, getHeight() * 0.58f };
    }
    int keyAt (Point<float> pt) const
    {
        for (int k = viewLow; k <= viewEnd; ++k) if (isBlackKey (k) && keyRect (k).contains (pt)) return k;
        for (int k = viewLow; k <= viewEnd; ++k) if (! isBlackKey (k) && keyRect (k).contains (pt)) return k;
        return -1;
    }
    Rectangle<float> leftArrow() const { return { 0, 0, (float) arrowW - 2, (float) getHeight() }; }
    Rectangle<float> rightArrow() const { return { (float) getWidth() - arrowW + 2, 0, (float) arrowW - 2, (float) getHeight() }; }
    static void drawArrow (Graphics& g, Rectangle<float> r, bool left, bool enabled)
    {
        g.setColour (th::surface.withAlpha (enabled ? 1.0f : 0.4f)); g.fillRoundedRectangle (r, 4);
        Path p; const auto c = r.getCentre();
        if (left) p.addTriangle (c.x + 4, c.y - 7, c.x + 4, c.y + 7, c.x - 4, c.y);
        else p.addTriangle (c.x - 4, c.y - 7, c.x - 4, c.y + 7, c.x + 4, c.y);
        g.setColour (enabled ? th::text : th::faint); g.fillPath (p);
    }
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override
    {
        const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
        wheelAcc += d;
        if (std::abs (wheelAcc) > 0.12f) { scrollOctaves (wheelAcc > 0 ? -1 : 1); wheelAcc = 0; }
    }

    // ------------------------------------------------------------ mouse
    void mouseDown (const MouseEvent& e) override
    {
        if (leftArrow().contains (e.position)) { scrollOctaves (-1); return; }
        if (rightArrow().contains (e.position)) { scrollOctaves (1); return; }
        const int k = keyAt (e.position);
        if (k < 0) return;
        if (! isPatternKey (k) && ! st.song().settings.isStop (k)) return;
        if (e.mods.isPopupMenu()) { if (onKeyMenu) onKeyMenu (k); return; }
        if (! large && st.song().settings.isStop (k)) { st.proc.uiStop(); pressedStop = true; repaint(); return; }
        if (e.mods.isAltDown()) {                                  // Option/Alt+click: pick several keys (to colour them together)
            if (st.keySel.count (k)) st.keySel.erase (k); else st.keySel.insert (k);
            repaint(); return;
        }
        if (e.mods.isShiftDown()) {                                 // Shift+click: pick every key from the last one to here
            const int from = st.keySel.empty() ? st.song().editKey : (k > *st.keySel.rbegin() ? *st.keySel.rbegin() : *st.keySel.begin());
            const bool blackFirst = MidiMessage::isMidiNoteBlack (from);         // white keys or black keys, like the first one
            for (int q = jmin (from, k); q <= jmax (from, k); ++q) if (isPatternKey (q) && MidiMessage::isMidiNoteBlack (q) == blackFirst) st.keySel.insert (q);
            repaint(); return;
        }
        if (! st.keySel.empty()) { st.keySel.clear(); repaint(); }
        st.song().editKey = k;
        st.focusLane = -1;
        pendingPlay = (! large && ! e.mods.isCommandDown()) ? k : -1;   // it plays when you let go, unless you drag it somewhere
        if (st.changed) st.changed();
        dragKey = k;
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (dragKey < 0 || e.getDistanceFromDragStart() < 6) return;
        pendingPlay = -1;
        if (st.song().keys[(size_t) dragKey].isEmpty()) return;
        if (auto* c = DragAndDropContainer::findParentDragContainerFor (this))
            if (! c->isDragAndDropActive()) {
                // only the key being dragged travels under the mouse
                const auto r = keyRect (dragKey).toNearestInt();
                const auto img = createComponentSnapshot (r, true);
                const Point<int> off = r.getPosition() - e.getMouseDownPosition();
                c->startDragging ("key:" + String (dragKey), this, ScaledImage (img), false, &off);
            }
    }
    void mouseUp (const MouseEvent& e) override
    {
        ignoreUnused (e);
        if (pendingPlay >= 0) { st.proc.uiTrigger (pendingPlay); playedKey = pendingPlay; pendingPlay = -1; }
        if (! st.song().settings.latch && playedKey >= 0) st.proc.uiRelease (playedKey);
        playedKey = -1; dragKey = -1;
        if (pressedStop) { pressedStop = false; repaint(); }
    }

    // ------------------------------------------------------------ internal drag (swap / unpark)
    bool isInterestedInDragSource (const SourceDetails& d) override { const auto s = d.description.toString(); return s.startsWith ("key:") || s.startsWith ("park:") || s.startsWith ("groove:"); }
    void itemDragMove (const SourceDetails& d) override { dropHover = keyAt (d.localPosition.toFloat()); repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHover = -1; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        const int to = keyAt (d.localPosition.toFloat());
        dropHover = -1;
        if (to < 0 || ! isPatternKey (to)) { repaint(); return; }
        const auto s = d.description.toString();
        if (s.startsWith ("groove:")) { if (onFiles) onFiles ({ s.fromFirstOccurrenceOf ("groove:", false, false) }, to); repaint(); return; }
        if (s.startsWith ("key:")) {
            const int from = s.fromFirstOccurrenceOf ("key:", false, false).getIntValue();
            if (from == to) return;
            st.applySong ("Swap patterns", [from, to] (Song& x) {
                std::swap (x.keys[(size_t) from], x.keys[(size_t) to]);
                for (auto& c : x.chains) for (auto& step : c.steps) { if (step.key == from) step.key = to; else if (step.key == to) step.key = from; }
                if (x.editKey == from) x.editKey = to;
            });
        } else {
            const int slot = s.fromFirstOccurrenceOf ("park:", false, false).getIntValue();
            st.applySong ("Unpark pattern", [slot, to] (Song& x) { std::swap (x.park[(size_t) slot], x.keys[(size_t) to]); x.editKey = to; });
        }
    }

    // ------------------------------------------------------------ file drops
    bool isInterestedInFileDrag (const StringArray& f) override
    {
        for (auto& p : f) if (p.endsWithIgnoreCase (".mid") || p.endsWithIgnoreCase (".midi") || p.endsWithIgnoreCase (".smf")) return true;
        return false;
    }
    void fileDragMove (const StringArray&, int x, int y) override { dropHover = keyAt ({ (float) x, (float) y }); repaint(); }
    void fileDragExit (const StringArray&) override { dropHover = -1; repaint(); }
    void filesDropped (const StringArray& f, int x, int y) override
    {
        int k = keyAt ({ (float) x, (float) y });
        dropHover = -1;
        if (k < 0) k = st.song().editKey;
        if (onFiles) onFiles (f, k);
    }

private:
    EditState& st;
    bool large;
    int dragKey = -1, playedKey = -1, pendingPlay = -1;
    int viewLow = -1, viewEnd = 0; float wheelAcc = 0;
    static constexpr int arrowW = 18;
    bool pressedStop = false;

    void drawTile (Graphics& g, Rectangle<float> r, int k, const Pattern& p, bool black)
    {
        if (st.song().settings.isStop (k)) {                       // a STOP key
            g.setColour (th::red.withAlpha (pressedStop ? 1.0f : 0.85f)); g.fillRoundedRectangle (r, 3);
            g.setColour (th::ink); g.setFont (font ((large ? 11.0f : 9.5f) * Theme::get().keyTextScale, 3));
            g.drawFittedText ("STOP", r.reduced (1).toNearestInt(), Justification::centred, 1, 0.7f);
            return;
        }
        if (p.notes.empty() && p.name.isEmpty()) return;
        const auto c = patternColour (st.song(), p);
        g.setColour (c.withAlpha (black ? 0.85f : 0.9f));
        g.fillRoundedRectangle (r, 3);
        g.setColour (th::ink);
        const float fs = (large ? 11.0f : 9.5f) * Theme::get().keyTextScale;
        g.setFont (font (fs, 2));
        String label = p.name.isNotEmpty() ? p.name : String (typeName (p.type));
        if (black && ! large && Theme::get().keyScale < 1.5f) label = label.substring (0, 3);
        const int lines = jmax (1, (int) (r.getHeight() / (fs * Theme::get().textScale * 1.15f)));
        g.drawFittedText (label, r.reduced (2, 1).toNearestInt(), Justification::centred, lines, 0.85f);
        ignoreUnused (k);
    }
    void decorate (Graphics& g, Rectangle<float> r, int k, int play, int queued, bool blink)
    {
        if (k == st.song().editKey) { g.setColour (th::blue); g.drawRoundedRectangle (r.reduced (1.5f), 3, 2.2f); }
        if (k == play) {
            g.setColour (th::amber.withAlpha (0.35f)); g.fillRoundedRectangle (r.reduced (1.0f), 3);
            g.setColour (th::amber); g.drawRoundedRectangle (r.reduced (1.0f), 3, 2.0f);
        }
        if (k == queued && blink) { g.setColour (th::blue.withAlpha (0.45f)); g.fillRoundedRectangle (r.reduced (1.0f), 3); }
        if (k == dropHover) {
            g.setColour (th::green.withAlpha (0.55f)); g.fillRoundedRectangle (r.reduced (1.0f), 3);
            g.setColour (th::green); g.drawRoundedRectangle (r.reduced (1.0f), 3, 2.5f);
        }
        if (k == flashNote && Time::getMillisecondCounter() < flashUntil) { g.setColour (th::text.withAlpha (0.45f)); g.fillRoundedRectangle (r.reduced (2.0f), 3); }
        if (st.keySel.count (k)) {                                   // picked with Option/Alt+click
            g.setColour (th::green.withAlpha (0.3f)); g.fillRoundedRectangle (r.reduced (1.0f), 3);
            g.setColour (th::green); g.drawRoundedRectangle (r.reduced (1.0f), 3, 2.5f);
        }
        if (const auto& kp = st.song().keys[(size_t) k]; kp.hasOriginal()) {   // edited (amber corner) or playing the original (blue corner)
            const float t = jlimit (6.0f, 11.0f, r.getWidth() * 0.32f);
            Path tri; tri.addTriangle (r.getRight() - 1 - t, r.getY() + 1, r.getRight() - 1, r.getY() + 1, r.getRight() - 1, r.getY() + 1 + t);
            g.setColour (kp.showingOriginal ? th::blue : th::amber); g.fillPath (tri);
            g.setColour (Colours::black.withAlpha (0.5f)); g.strokePath (tri, PathStrokeType (0.8f));
        }
        if (st.song().keys[(size_t) k].fav) {                         // your star: at the bottom of the key, over the text
            const float sz = jlimit (8.0f, 16.0f, r.getWidth() * 0.42f);
            drawStar (g, { r.getCentreX(), r.getBottom() - sz * 0.75f }, sz);
        }
    }
public:
    // a gold star with a dark rim: visible on white and black keys alike
    static void drawStar (Graphics& g, Point<float> c, float size)
    {
        Path p; p.addStar (c, 5, size * 0.22f, size * 0.52f, -MathConstants<float>::halfPi);
        g.setColour (Colours::black.withAlpha (0.85f)); g.strokePath (p, PathStrokeType (2.2f));
        g.setColour (Colour (0xffffcc33)); g.fillPath (p);
    }
private:
};

// ---------------------------------------------------------------- parking tray: hold patterns aside while rearranging
class ParkTray : public Component, public DragAndDropTarget, public SettableTooltipClient
{
public:
    explicit ParkTray (EditState& e) : st (e) { setTooltip ("Parking tray: drag a key here to set its pattern aside, drag it back onto any key"); }
    void paint (Graphics& g) override
    {
        drawCard (g, getLocalBounds().toFloat().reduced (1), th::panel, 8, "Parking tray");
        g.setFont (font (9.5f, 3)); g.setColour (th::faint);
        g.drawText ("PARKED", 8, 2, 60, 12, Justification::centredLeft);
        for (int i = 0; i < 8; ++i) {
            auto r = slotRect (i);
            const auto& p = st.song().park[(size_t) i];
            g.setColour (i == hover ? th::green.withAlpha (0.35f) : Colours::black.withAlpha (0.3f)); g.fillRoundedRectangle (r, 4);
            if (! p.isEmpty()) {
                g.setColour (patternColour (st.song(), p)); g.fillRoundedRectangle (r.reduced (1.5f), 4);
                g.setColour (th::ink); g.setFont (font (9.5f, 2));
                g.drawFittedText (p.name.isNotEmpty() ? p.name : String (typeName (p.type)), r.reduced (3, 1).toNearestInt(), Justification::centred, 2, 0.8f);
            }
        }
    }
    Rectangle<float> slotRect (int i) const
    {
        const float w = (getWidth() - 16.0f) / 4.0f, h = (getHeight() - 22.0f) / 2.0f;
        return { 8 + (i % 4) * w, 16 + (i / 4) * (h + 2), w - 3, h };
    }
    int slotAt (Point<float> p) const { for (int i = 0; i < 8; ++i) if (slotRect (i).contains (p)) return i; return -1; }
    void mouseDown (const MouseEvent& e) override { dragSlot = slotAt (e.position); }
    void mouseDrag (const MouseEvent& e) override
    {
        if (dragSlot < 0 || e.getDistanceFromDragStart() < 6 || st.song().park[(size_t) dragSlot].isEmpty()) return;
        if (auto* c = DragAndDropContainer::findParentDragContainerFor (this))
            if (! c->isDragAndDropActive()) {
                const auto r = slotRect (dragSlot).toNearestInt();
                const auto img = createComponentSnapshot (r, true);
                const Point<int> off = r.getPosition() - e.getMouseDownPosition();
                c->startDragging ("park:" + String (dragSlot), this, ScaledImage (img), false, &off);
            }
    }
    bool isInterestedInDragSource (const SourceDetails& d) override { const auto s = d.description.toString(); return s.startsWith ("key:") || s.startsWith ("park:"); }
    void itemDragMove (const SourceDetails& d) override { hover = slotAt (d.localPosition.toFloat()); repaint(); }
    void itemDragExit (const SourceDetails&) override { hover = -1; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        const int slot = slotAt (d.localPosition.toFloat()); hover = -1;
        if (slot < 0) { repaint(); return; }
        const auto s = d.description.toString();
        if (s.startsWith ("key:")) {
            const int k = s.fromFirstOccurrenceOf ("key:", false, false).getIntValue();
            st.applySong ("Park pattern", [k, slot] (Song& x) { std::swap (x.park[(size_t) slot], x.keys[(size_t) k]); });
        } else {
            const int from = s.fromFirstOccurrenceOf ("park:", false, false).getIntValue();
            if (from != slot) st.applySong ("Move parked", [from, slot] (Song& x) { std::swap (x.park[(size_t) from], x.park[(size_t) slot]); });
        }
    }
private:
    EditState& st; int dragSlot = -1, hover = -1;
};

// ---------------------------------------------------------------- drag MIDI out of the plugin
class DragMidiButton : public TextButton
{
public:
    std::function<std::vector<const Pattern*>()> getParts;
    std::function<int()> getChannel;
    DragMidiButton() { setTooltip ("Drag this onto a Studio One track (or anywhere) to get the pattern as MIDI"); }
    void mouseDrag (const MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 5 || ! getParts) return;
        auto parts = getParts();
        if (parts.empty()) return;
        dragging = true;
        auto dir = dataFolder().getChildFile ("Temp"); dir.createDirectory();
        String name = File::createLegalFileName (parts.front()->name.isNotEmpty() ? parts.front()->name : String ("Drum Player"));
        auto f = dir.getChildFile (name + ".mid");
        writeMidiFile (parts, f, getChannel ? getChannel() : 10);
        DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { dragging = false; });
    }
    void mouseUp (const MouseEvent& e) override { dragging = false; TextButton::mouseUp (e); }
private:
    bool dragging = false;
};


// ---------------------------------------------------------------- chain strip on the main window
class ChainStrip : public Component, public DragAndDropTarget, public SettableTooltipClient
{
public:
    explicit ChainStrip (EditState& e) : st (e)
    {
        setTooltip ("Drag keys here to chain them. Drag a tile to reorder, right-click it for repeats. Click a tile to edit its pattern. Press and hold a tile to loop it; click it again to let the chain go on");
        setupCombo (*this, sel, {}, 1, "Which chain");
        sel.onChange = [this] {
            const int i = sel.getSelectedItemIndex();
            if (i == (int) st.song().chains.size()) {                       // "+ New chain"
                st.applySong ("New chain", [] (Song& s) { s.chains.push_back ({ "Chain " + String ((int) s.chains.size() + 1), {}, -1, true }); });
                st.currentChain = (int) st.song().chains.size() - 1;
            } else if (i >= 0) st.currentChain = i;
            refresh(); if (st.changed) st.changed();
        };
        setupButton (*this, play, "Play", false, "Play this chain"); play.onClick = [this] { st.proc.uiPlayChain (cur()); };
        setupButton (*this, loop, "Loop", true); loop.onClick = [this] { const bool on = loop.getToggleState(); edit ("Chain loop", [on] (Chain& c) { c.loop = on; }); };
        setupButton (*this, keyBtn, "Key: none", false, "Start this chain from a key on your keyboard");
        keyBtn.onClick = [this] { keyMenu(); };
        dragOut.setButtonText ("MIDI"); dragOut.setSize (fitW ("MIDI"), btnH); dragOut.setComponentID ("Chain MIDI"); addAndMakeVisible (dragOut);
        dragOut.setTooltip ("Drag the whole chain out as one MIDI file");
        dragOut.getParts = [this] {
            std::vector<const Pattern*> v;
            for (auto& s2 : chain().steps) if (s2.key >= 0) for (int r = 0; r < s2.repeats; ++r) v.push_back (&st.song().keys[(size_t) s2.key]);
            return v;
        };
        dragOut.getChannel = [this] { return st.song().settings.outChannel; };
    }
    void refresh()
    {
        auto& chains = st.song().chains;
        st.currentChain = cur();
        sel.clear (dontSendNotification);
        for (int i = 0; i < (int) chains.size(); ++i) sel.addItem (chains[(size_t) i].name.isNotEmpty() ? chains[(size_t) i].name : "Chain " + String (i + 1), i + 1);
        sel.addItem ("+ New chain", (int) chains.size() + 1);
        sel.setSelectedItemIndex (st.currentChain, dontSendNotification);
        loop.setToggleState (chain().loop, dontSendNotification);
        const int tn = chain().triggerNote;
        keyBtn.setButtonText (tn >= 0 ? "Key: " + keyName (tn) : String ("Key: none"));
        repaint();
    }
    void paint (Graphics& g) override
    {
        drawCard (g, getLocalBounds().toFloat().reduced (1), th::panel, 8, "Chain strip");
        g.setFont (font (9.5f, 3)); g.setColour (th::faint);
        g.drawText ("CHAIN", 10, 0, 44, getHeight(), Justification::centredLeft);
        const auto& c = chain();
        const bool playingThis = st.proc.chainPlaying.load() == cur();
        const int playStep = st.proc.chainStep.load();
        g.saveState(); g.reduceClipRegion (tilesArea());
        for (int i = 0; i <= (int) c.steps.size(); ++i) {
            auto r = tileRect (i);
            if (i == (int) c.steps.size()) {                                 // the drop hint at the end
                float dash[] = { 4, 3 };
                g.setColour (dropAt == i ? th::green : th::faint);
                Path p; p.addRoundedRectangle (r, 5);
                PathStrokeType (1.2f).createDashedStroke (p, p, dash, 2);
                g.fillPath (p);
                g.setFont (font (10.5f, 2)); g.drawText (c.steps.empty() ? "drag keys here" : "+", r.toNearestInt(), Justification::centred);
                break;
            }
            const auto& step = c.steps[(size_t) i];
            const auto& p = st.song().keys[(size_t) jlimit (0, 127, step.key)];
            const bool on = playingThis && playStep == i;
            g.setColour (Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (r.translated (0, 1), 5);
            g.setColour (patternColour (st.song(), p).withAlpha (p.notes.empty() ? 0.35f : 0.9f)); g.fillRoundedRectangle (r, 5);
            if (on) { g.setColour (th::amber); g.drawRoundedRectangle (r.expanded (1), 5, 2.2f); }
            if (i == dragStep) { g.setColour (Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (r, 5); }
            g.setColour (th::ink); g.setFont (font (10.5f, 2));
            g.drawFittedText ((p.name.isNotEmpty() ? p.name : keyName (step.key)), r.reduced (6, 1).withTrimmedRight (22).toNearestInt(), Justification::centredLeft, 1, 0.8f);
            g.setFont (font (10.5f, 3));
            g.drawText (step.looping ? String ("LOOP") : "x" + String (step.repeats), r.reduced (5, 0).toNearestInt(), Justification::centredRight);
            if (step.looping) {                                                   // a looping step pulses
                const float a = 0.5f + 0.5f * (float) std::sin (Time::getMillisecondCounter() * 0.008);
                g.setColour (th::amber.withAlpha (0.4f + 0.6f * a)); g.drawRoundedRectangle (r.expanded (2), 6, 2.5f);
            }
            if (dropAt == i) { g.setColour (th::green); g.fillRect (r.getX() - 3.0f, r.getY(), 2.0f, r.getHeight()); }
        }
        g.restoreState();
    }
    void resized() override
    {
        RowPlacer rp (getLocalBounds().withTrimmedLeft (52), 5);
        play.setVisible (false);
        rp.place (sel, 120); rp.place (loop); rp.place (keyBtn, 86); rp.place (dragOut);
        tilesX = rp.x + 6;
    }
    void mouseDown (const MouseEvent& e) override
    {
        const int i = stepAt (e.position);
        downStep = i;
        if (i < 0) return;
        if (e.mods.isPopupMenu()) { stepMenu (i); return; }
        downTime = Time::getMillisecondCounter(); tileDragged = false;
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (downStep < 0 || e.getDistanceFromDragStart() < 6) return;
        if (auto* c = DragAndDropContainer::findParentDragContainerFor (this))
            if (! c->isDragAndDropActive()) {
                const auto r = tileRect (downStep).toNearestInt();
                const auto img = createComponentSnapshot (r, true);
                const Point<int> off = r.getPosition() - e.getMouseDownPosition();
                dragStep = downStep; tileDragged = true;
                c->startDragging ("chainstep:" + String (downStep), this, ScaledImage (img), false, &off);
            }
    }
    void mouseUp (const MouseEvent& e) override
    {
        const int i = downStep;
        if (i >= 0 && i < (int) chain().steps.size() && ! tileDragged && ! e.mods.isPopupMenu()) {
            if (Time::getMillisecondCounter() - downTime >= 350 || chain().steps[(size_t) i].looping)   // press and hold: loop; click a looping tile: release it
                edit (chain().steps[(size_t) i].looping ? "Stop looping" : "Loop chain step", [i] (Chain& c) { c.steps[(size_t) i].looping = ! c.steps[(size_t) i].looping; });
            else { st.song().editKey = chain().steps[(size_t) i].key; if (st.changed) st.changed(); }
        }
        downStep = -1; dragStep = -1; repaint();
    }
    uint32 downTime = 0; bool tileDragged = false;
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override
    {
        scroll = jlimit (0, jmax (0, ((int) chain().steps.size() + 1) * (tileW + gap) - tilesArea().getWidth()), scroll - (int) ((w.deltaY + w.deltaX) * 300));
        repaint();
    }

    bool isInterestedInDragSource (const SourceDetails& d) override { const auto s2 = d.description.toString(); return s2.startsWith ("key:") || s2.startsWith ("chainstep:"); }
    void itemDragMove (const SourceDetails& d) override { dropAt = insertAt (d.localPosition.toFloat()); repaint(); }
    void itemDragExit (const SourceDetails&) override { dropAt = -1; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        const int at = insertAt (d.localPosition.toFloat());
        dropAt = -1; dragStep = -1;
        const auto s2 = d.description.toString();
        if (s2.startsWith ("key:")) {
            const int k = s2.fromFirstOccurrenceOf ("key:", false, false).getIntValue();
            edit ("Add to chain", [k, at] (Chain& c) { c.steps.insert (c.steps.begin() + jlimit (0, (int) c.steps.size(), at), ChainStep { k, 1 }); });
        } else {
            const int from = s2.fromFirstOccurrenceOf ("chainstep:", false, false).getIntValue();
            edit ("Move chain step", [from, at] (Chain& c) {
                if (from < 0 || from >= (int) c.steps.size()) return;
                auto step = c.steps[(size_t) from];
                c.steps.erase (c.steps.begin() + from);
                const int to = jlimit (0, (int) c.steps.size(), at > from ? at - 1 : at);
                c.steps.insert (c.steps.begin() + to, step);
            });
        }
    }
private:
    EditState& st;
    ComboBox sel; TextButton play, loop, keyBtn; DragMidiButton dragOut;
    int tilesX = 400, scroll = 0, dropAt = -1, downStep = -1, dragStep = -1;
    static constexpr int tileW = 104, gap = 5;

    int cur() const { return jlimit (0, jmax (0, (int) st.song().chains.size() - 1), st.currentChain); }
    Chain& chain() { if (st.song().chains.empty()) st.song().chains.push_back ({ "Chain 1", {}, -1, true }); return st.song().chains[(size_t) cur()]; }
    Rectangle<int> tilesArea() const { return getLocalBounds().withTrimmedLeft (tilesX).reduced (0, 4).withTrimmedRight (6); }
    Rectangle<float> tileRect (int i) const
    {
        const auto a = tilesArea();
        return { (float) (a.getX() + i * (tileW + gap) - scroll), (float) a.getY() + 1, (float) tileW, (float) a.getHeight() - 2 };
    }
    int stepAt (Point<float> p)
    {
        if (! tilesArea().toFloat().contains (p)) return -1;
        for (int i = 0; i < (int) chain().steps.size(); ++i) if (tileRect (i).contains (p)) return i;
        return -1;
    }
    int insertAt (Point<float> p)
    {
        const int n = (int) chain().steps.size();
        const float rel = p.x - tilesArea().getX() + scroll + tileW * 0.5f;
        return jlimit (0, n, (int) (rel / (tileW + gap)));
    }
    void edit (const String& name, std::function<void (Chain&)> f)
    {
        const int c = cur();
        st.applySong (name, [c, f] (Song& s) { if (c < (int) s.chains.size()) f (s.chains[(size_t) c]); });
        refresh();
    }
    void stepMenu (int i)
    {
        PopupMenu m;
        PopupMenu reps; for (int r : { 1, 2, 3, 4, 6, 8, 12, 16, 32 }) reps.addItem (100 + r, String (r) + (r == 1 ? " time" : " times"), true, chain().steps[(size_t) i].repeats == r);
        m.addSubMenu ("Repeats", reps);
        if (i > 0) m.addItem (1, "Move left");
        if (i < (int) chain().steps.size() - 1) m.addItem (2, "Move right");
        m.addItem (3, "Duplicate"); m.addItem (4, "Remove");
        m.addSeparator(); m.addItem (5, "Edit this pattern");
        m.showMenuAsync (PopupMenu::Options().withMousePosition(), [this, i] (int r) {
            if (r >= 100) { const int n = r - 100; edit ("Chain repeats", [i, n] (Chain& c) { c.steps[(size_t) i].repeats = n; }); }
            else if (r == 1) edit ("Move chain step", [i] (Chain& c) { std::swap (c.steps[(size_t) i], c.steps[(size_t) i - 1]); });
            else if (r == 2) edit ("Move chain step", [i] (Chain& c) { std::swap (c.steps[(size_t) i], c.steps[(size_t) i + 1]); });
            else if (r == 3) edit ("Duplicate chain step", [i] (Chain& c) { auto s2 = c.steps[(size_t) i]; c.steps.insert (c.steps.begin() + i + 1, s2); });
            else if (r == 4) edit ("Remove chain step", [i] (Chain& c) { c.steps.erase (c.steps.begin() + i); });
            else if (r == 5) { st.song().editKey = chain().steps[(size_t) i].key; if (st.changed) st.changed(); }
        });
    }
    void keyMenu()
    {
        PopupMenu m;
        m.addItem (1, "None (Play button only)", true, chain().triggerNote < 0);
        PopupMenu keys;
        for (int n = 0; n < 128; ++n) keys.addItem (1000 + n, keyName (n) + "  (" + String (n) + ")", true, chain().triggerNote == n);
        m.addSubMenu ("Start from key", keys);
        m.addSeparator();
        m.addItem (2, "Rename this chain...");
        if (st.song().chains.size() > 1) m.addItem (3, "Delete this chain");
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&keyBtn), [this] (int r) {
            if (r == 1) edit ("Chain key", [] (Chain& c) { c.triggerNote = -1; });
            else if (r >= 1000) { const int n = r - 1000; edit ("Chain key", [n] (Chain& c) { c.triggerNote = n; }); }
            else if (r == 2) {
                auto* aw = new AlertWindow ("Rename chain", "", MessageBoxIconType::NoIcon);
                aw->addTextEditor ("n", chain().name);
                aw->addButton ("OK", 1, KeyPress (KeyPress::returnKey)); aw->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
                aw->enterModalState (true, ModalCallbackFunction::create ([this, aw] (int res) { if (res == 1) { const auto t = aw->getTextEditorContents ("n"); edit ("Rename chain", [t] (Chain& c) { c.name = t; }); } }), true);
            }
            else if (r == 3) {
                const int c = cur();
                st.applySong ("Delete chain", [c] (Song& s) { if (c < (int) s.chains.size()) s.chains.erase (s.chains.begin() + c); if (s.chains.empty()) s.chains.push_back ({ "Chain 1", {}, -1, true }); });
                st.currentChain = jmax (0, c - 1); refresh();
            }
        });
    }
};


// ---------------------------------------------------------------- song lane: patterns as chunks on a timeline, like a Studio One track
class SongLane : public Component, public DragAndDropTarget, public SettableTooltipClient
{
public:
    explicit SongLane (EditState& e) : st (e)
    {
        setWantsKeyboardFocus (true);
        setTooltip ("Drag keys here to arrange a song. Drag chunks to move (Alt: by beat), their right edge to make them longer or shorter, "
                    "their left edge to trim. Ctrl-drag copies. D duplicates, Alt+X splits at the cursor, M mutes, Del deletes. Double-click: edit the pattern. "
                    "Right-click + drag up/down: zoom. The loop bar on the bar numbers: drag to make one, drag its ends to resize, click to turn it on, double-click to turn it off. "
                    "Clicking a chunk loops it (Settings). Double-click the bar numbers outside the loop: play from there. Where chunks overlap, the one on top plays");
    }
    std::function<void()> onFocus;
    float pxPerBeat = 12.0f; int scrollX = 0; double cursorBeat = 0; int snap = 0;   // snap: an index into snapBeats(), the last = off
    static const std::vector<std::pair<const char*, double>>& snapBeats()
    {
        static const std::vector<std::pair<const char*, double>> v { { "Snap: bar", 4.0 }, { "Snap: beat", 1.0 }, { "Snap: 1/8", 0.5 }, { "Snap: 1/16", 0.25 },
            { "Snap: 1/32", 0.125 }, { "Snap: 1/64", 0.0625 }, { "Snap: 1/128", 0.03125 }, { "Snap: 1/8T", 1.0 / 3 }, { "Snap: 1/16T", 1.0 / 6 },
            { "Snap: 1/32T", 1.0 / 12 }, { "Snap: 1/64T", 1.0 / 24 }, { "Snap: 1/128T", 1.0 / 48 }, { "Snap: off", 0.0 } };
        return v;
    }
    std::function<void (const String&, double beat)> onDropPattern;      // a parked pattern dropped here
    static constexpr int rulerH = 16;

    double beatToX (double b) const { return b * pxPerBeat - scrollX; }
    double xToBeat (double x) const { return (x + scrollX) / pxPerBeat; }
    double snapBeat (double b, bool alt) const
    {
        const auto& v = snapBeats();
        const double g = v[(size_t) jlimit (0, (int) v.size() - 1, snap)].second;
        if (alt || g <= 0.0) return jmax (0.0, std::round (b * 96) / 96);   // free (fine steps)
        return jmax (0.0, std::round (b / g) * g);
    }
    // chunks click onto the edges of the other chunks (unless Alt)
    double magnet (double b, bool alt) const
    {
        if (alt) return b;
        double best = b, dist = 9.0 / pxPerBeat;
        for (auto& c : st.song().clips) {
            if (c.selected) continue;
            for (double e : { c.start, c.start + c.length }) if (std::abs (e - b) < dist) { dist = std::abs (e - b); best = e; }
        }
        return best;
    }

    void paint (Graphics& g) override
    {
        fillArea (g, getLocalBounds(), "Song lane", th::panel);
        // ruler
        g.setColour (Colours::black.withAlpha (0.25f)); g.fillRect (0, 0, getWidth(), rulerH);
        g.setFont (font (9.5f, 2));
        const int firstBar = jmax (0, (int) (xToBeat (0) / 4));
        const int barStep = pxPerBeat * 4 < 26 ? 4 : (pxPerBeat * 4 < 50 ? 2 : 1);
        for (int bar = firstBar; ; ++bar) {
            const float x = (float) beatToX (bar * 4.0);
            if (x > getWidth()) break;
            g.setColour (Colours::white.withAlpha (0.10f)); g.drawVerticalLine ((int) x, (float) rulerH, (float) getHeight());
            if (bar % barStep == 0) { g.setColour (th::dim); g.drawText (String (bar + 1), (int) x + 3, 1, 30, rulerH - 2, Justification::centredLeft); }
        }
        // chunks
        auto& s = st.song();
        for (auto& c : s.clips) {
            auto r = clipRect (c);
            if (r.getRight() < 0 || r.getX() > getWidth()) continue;
            const auto& p = s.keys[(size_t) jlimit (0, 127, c.key)];
            const auto col = patternColour (s, p).withMultipliedAlpha (c.muted ? 0.3f : 1.0f);
            g.setColour (Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (r.translated (0, 1), 4);
            g.setGradientFill (ColourGradient (col.brighter (0.15f), r.getTopLeft(), col.darker (0.25f), r.getBottomLeft(), false));
            g.fillRoundedRectangle (r, 4);
            // where the pattern repeats inside the chunk
            const double L = p.lengthTicks() / (double) PPQ;
            if (L > 0) {
                g.setColour (Colours::black.withAlpha (0.3f));
                for (double t = std::ceil (c.offset / L + 1e-9) * L; t < c.offset + c.length - 1e-9; t += L) {
                    const float x = (float) beatToX (c.start + t - c.offset);
                    g.drawVerticalLine ((int) x, r.getY() + 2, r.getBottom() - 2);
                }
            }
            // a sketch of the notes
            if (L > 0 && r.getHeight() > 20) {
                // one row per drum used (cymbals on top, kick at the bottom): a small drum grid that grows with the lane
                std::vector<int> used; for (auto& n : p.notes) if (std::find (used.begin(), used.end(), n.note) == used.end()) used.push_back (n.note);
                std::sort (used.begin(), used.end(), std::greater<int>());
                const float top = r.getY() + 16, avail = r.getHeight() - 20;
                const float rowH = used.empty() ? 0 : jmin (40.0f, avail / (float) used.size());
                const float dotH = jlimit (2.0f, 14.0f, rowH * 0.5f), dotW = jlimit (2.0f, 5.0f, rowH * 0.3f);
                if (rowH >= 6) {                                       // faint row lines when there's room
                    g.setColour (th::ink.withAlpha (0.08f));
                    for (size_t i = 0; i < used.size(); ++i) g.drawHorizontalLine ((int) (top + rowH * (float) i + rowH * 0.5f), r.getX() + 2, r.getRight() - 2);
                }
                for (auto& n : p.notes) {
                    if (n.tick >= p.lengthTicks()) continue;
                    const double nt = n.tick / (double) PPQ;
                    const auto idx = (float) (std::find (used.begin(), used.end(), n.note) - used.begin());
                    const float y = top + rowH * idx + (rowH - dotH) * 0.5f;
                    g.setColour (th::ink.withAlpha (0.25f + 0.45f * (float) n.velocity / 127.0f));
                    for (double base = std::floor (c.offset / L) * L; base < c.offset + c.length; base += L) {
                        const double t = base + nt;
                        if (t < c.offset || t >= c.offset + c.length) continue;
                        g.fillRect ((float) beatToX (c.start + t - c.offset), y, dotW, dotH);
                    }
                }
            }
            g.setColour (th::ink); g.setFont (font (10.5f, 3));
            g.drawText ((p.name.isNotEmpty() ? p.name : keyName (c.key)) + (c.muted ? "  (muted)" : ""), r.reduced (5, 1).withHeight (13).toNearestInt(), Justification::centredLeft, true);
            if (c.selected) { g.setColour (th::amber); g.drawRoundedRectangle (r.expanded (1), 4, 2.0f); }
        }
        if (dropBeat >= 0) { g.setColour (th::green); g.fillRect ((float) beatToX (dropBeat) - 1, (float) rulerH, 2.0f, (float) getHeight()); }
        if (st.song().settings.followHost && st.proc.hostLooping.load()) {       // synced: Studio One's loop, so both agree on where you are
            const float x0 = (float) beatToX (st.proc.hostLoopA.load()), x1 = (float) beatToX (st.proc.hostLoopB.load());
            g.setColour (th::blue.withAlpha (0.9f)); g.fillRect (x0, (float) rulerH - 3, jmax (2.0f, x1 - x0), 2.0f);
            g.setFont (font (9.0f, 2)); if (x1 - x0 > 90) g.drawText ("Studio One loop", Rectangle<float> (x0 + 4, (float) rulerH - 13, x1 - x0 - 8, 10).toNearestInt(), Justification::centredRight);
        }
        {   // the loop: a bar on the ruler, amber when on
            const auto& sg = st.song();
            if (sg.loopEnd > sg.loopStart) {
                const float x0 = (float) beatToX (sg.loopStart), x1 = (float) beatToX (sg.loopEnd);
                if (sg.loopOn) { g.setColour (th::amber.withAlpha (0.06f)); g.fillRect (x0, (float) rulerH, x1 - x0, (float) getHeight() - rulerH); }
                g.setColour (sg.loopOn ? th::amber.withAlpha (0.85f) : Colours::white.withAlpha (0.18f));
                g.fillRoundedRectangle (x0, 2.0f, jmax (3.0f, x1 - x0), (float) rulerH - 4, 3.0f);
                g.setColour (sg.loopOn ? th::ink : th::dim); g.setFont (font (9.0f, 3));
                if (x1 - x0 > 40) g.drawText (sg.loopOn ? "LOOP" : "loop off", Rectangle<float> (x0 + 4, 2.0f, x1 - x0 - 8, (float) rulerH - 4).toNearestInt(), Justification::centredLeft);
            }
        }
        // cursor and playhead
        g.setColour (th::blue.withAlpha (0.8f)); g.drawVerticalLine ((int) beatToX (cursorBeat), 0, (float) getHeight());
        const double ph = st.proc.songBeatUi.load();
        if (ph >= 0) { g.setColour (th::text); g.drawVerticalLine ((int) beatToX (ph), 0, (float) getHeight()); }
        if (! band.isEmpty()) { g.setColour (th::blue.withAlpha (0.15f)); g.fillRect (band); g.setColour (th::blue); g.drawRect (band, 1.0f); }
        if (s.clips.empty()) { g.setFont (font (11.0f, 1)); g.setColour (th::faint); g.drawText ("Drag keys here to build a song", getLocalBounds().withTrimmedTop (rulerH), Justification::centred); }
    }

    // ------------------------------------------------------------ mouse
    void mouseDown (const MouseEvent& e) override
    {
        grabKeyboardFocus(); if (onFocus) onFocus();
        auto& clips = st.song().clips;
        const int i = clipAt (e.position);
        if (e.mods.isPopupMenu()) { rzDown = true; rzZoom = false; rzY = e.position.y; rzX = e.position.x; rzPx = pxPerBeat; rzBeat = xToBeat (e.x); rzClip = i; return; }   // menu on release, zoom on drag
        if (e.y < rulerH) {                                       // the loop bar: click turns it on, double-click off, drag its ends or move it
            auto& sg = st.song();
            const float x0 = (float) beatToX (sg.loopStart), x1 = (float) beatToX (sg.loopEnd);
            loopOrigA = sg.loopStart; loopOrigB = sg.loopEnd; downPos = e.position;
            if (sg.loopEnd > sg.loopStart && std::abs (e.position.x - x1) < 6) { mode = LoopR; return; }
            if (sg.loopEnd > sg.loopStart && std::abs (e.position.x - x0) < 6) { mode = LoopL; return; }
            if (sg.loopEnd > sg.loopStart && e.position.x > x0 && e.position.x < x1) {
                if (e.getNumberOfClicks() > 1) { sg.loopOn = false; st.proc.publish(); repaint(); return; }
                if (! sg.loopOn) { sg.loopOn = true; st.proc.publish(); }
                mode = LoopMove; repaint(); return;
            }
            if (e.getNumberOfClicks() < 2) { mode = LoopNew; cursorBeat = snapBeat (xToBeat (e.x), e.mods.isAltDown()); repaint(); return; }
        }
        cursorBeat = snapBeat (xToBeat (e.x), e.mods.isAltDown());          // the cursor goes where you click, on the lane's grid
        if (e.y < rulerH && e.getNumberOfClicks() > 1) {        // double-click in the ruler: play the song from there
            if (! st.song().songMode) st.applySong ("Song mode", [] (Song& s) { s.songMode = true; });
            st.proc.uiSongPlay (true, cursorBeat); repaint(); return;
        }
        if (i >= 0 && st.tool == Tool::Split) { splitAt (snapBeat (xToBeat (e.x), e.mods.isAltDown()), i); return; }   // the scissors cut on the grid
        if (i < 0) {
            if (! e.mods.isShiftDown()) for (auto& c : clips) c.selected = false;
            mode = Band; downPos = e.position; repaint(); return;
        }
        auto& c = clips[(size_t) i];
        if (e.getNumberOfClicks() > 1) { st.song().editKey = c.key; if (st.changed) st.changed(); return; }
        if (e.mods.isShiftDown()) c.selected = ! c.selected;
        else if (! c.selected) { for (auto& x : clips) x.selected = false; c.selected = true; }
        if (st.song().settings.loopSelected) {                      // the loop goes around what you clicked
            double lo = 1e9, hi = 0; int n = 0; uint32 id = 0;
            for (auto& x : clips) if (x.selected) { lo = jmin (lo, x.start); hi = jmax (hi, x.start + x.length); ++n; id = x.id; }
            if (hi > lo) { auto& sg = st.song(); sg.loopStart = lo; sg.loopEnd = hi; sg.loopOn = true; sg.loopClip = n == 1 ? id : 0; }
        }
        const auto r = clipRect (c);
        mode = e.position.x > r.getRight() - 6 ? ResizeR : (e.position.x < r.getX() + 6 ? ResizeL : Move);
        st.proc.beginEdit();
        orig = clips; anchor = i; downPos = e.position;
        copying = e.mods.isCommandDown() && mode == Move;
        repaint();
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (rzDown) {
            const float dy = e.position.y - rzY;
            if (! rzZoom && std::abs (dy) < 4.0f) return;
            rzZoom = true;
            pxPerBeat = jlimit (2.0f, 240.0f, rzPx * std::exp (-dy * 0.012f));
            scrollX = jmax (0, (int) (rzBeat * pxPerBeat - rzX));
            repaint(); return;
        }
        if (mode == LoopL || mode == LoopR || mode == LoopMove || mode == LoopNew) {
            auto& sg = st.song(); const bool alt = e.mods.isAltDown();
            const double d = (e.position.x - downPos.x) / pxPerBeat;
            if (mode == LoopR) sg.loopEnd = jmax (sg.loopStart + 0.25, snapBeat (loopOrigB + d, alt));
            else if (mode == LoopL) sg.loopStart = jlimit (0.0, sg.loopEnd - 0.25, snapBeat (loopOrigA + d, alt));
            else if (mode == LoopMove) { const double ns = snapBeat (loopOrigA + d, alt); sg.loopStart = ns; sg.loopEnd = ns + (loopOrigB - loopOrigA); }
            else if (std::abs (e.position.x - downPos.x) > 4) {
                const double a = snapBeat (xToBeat (downPos.x), alt), b = snapBeat (xToBeat (e.position.x), alt);
                sg.loopStart = jmin (a, b); sg.loopEnd = jmax (jmax (a, b), sg.loopStart + 0.25); sg.loopOn = true;
            }
            if (mode != LoopNew || std::abs (e.position.x - downPos.x) > 4) sg.loopClip = 0;      // a loop you shape yourself stays where you put it
            st.proc.publish(); repaint(); return;
        }
        auto& clips = st.song().clips;
        if (mode == Band) {
            band = Rectangle<float> (downPos, e.position);
            for (auto& c : clips) c.selected = clipRect (c).intersects (band) || (e.mods.isShiftDown() && c.selected);
            repaint(); return;
        }
        if (mode == None || anchor < 0) return;
        if (mode == Move && ! copying) {
            // dragged out of the Drum Player window: the selected chunks go to Studio One as MIDI (gaps kept)
            auto* top = getTopLevelComponent();
            if (top != nullptr && ! top->getScreenBounds().contains (e.getScreenPosition())) {
                clips = orig; st.proc.publish(); st.proc.endEdit ("Drag out");
                mode = None; anchor = -1; repaint();
                auto dir = dataFolder().getChildFile ("Temp"); dir.createDirectory();
                String name = "Drum Player chunks";
                for (auto& c : clips) if (c.selected) { name = st.song().keys[(size_t) jlimit (0, 127, c.key)].name; break; }
                auto f = dir.getChildFile (File::createLegalFileName (name.isNotEmpty() ? name : String ("Drum Player chunks")) + ".mid");
                writeSongMidi (st.song(), f, st.song().settings.outChannel, true);
                DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
                return;
            }
        }
        const double d = (e.position.x - downPos.x) / pxPerBeat;
        clips = orig;
        if (copying) for (auto& c : orig) if (c.selected) { auto x = c; x.selected = false; clips.push_back (x); }
        const auto& a = orig[(size_t) anchor];
        if (mode == Move) {
            const bool alt = e.mods.isAltDown();
            double nb = snapBeat (a.start + d, alt);
            const double ms = magnet (a.start + d, alt), me = magnet (a.start + d + a.length, alt) - a.length;   // start or end onto a neighbour
            if (std::abs (ms - (a.start + d)) < std::abs (nb - (a.start + d)) && ms != a.start + d) nb = ms;
            else if (me != a.start + d && std::abs (me - (a.start + d)) < std::abs (nb - (a.start + d))) nb = me;
            const double dd = nb - a.start;
            for (size_t k = 0; k < orig.size(); ++k) if (orig[k].selected) clips[k].start = jmax (0.0, orig[k].start + dd);
        } else if (mode == ResizeR) {
            double ne = snapBeat (a.start + a.length + d, e.mods.isAltDown());
            if (const double m = magnet (a.start + a.length + d, e.mods.isAltDown()); m != a.start + a.length + d) ne = m;
            const double dd = ne - (a.start + a.length);
            for (size_t k = 0; k < orig.size(); ++k) if (orig[k].selected) clips[k].length = jmax (0.25, orig[k].length + dd);
        } else if (mode == ResizeL) {
            const double ns = snapBeat (a.start + d, e.mods.isAltDown()), dd = ns - a.start;
            for (size_t k = 0; k < orig.size(); ++k) if (orig[k].selected) {
                auto& c = clips[k];
                const double move = jlimit (-orig[k].start, orig[k].length - 0.25, dd);
                c.start = orig[k].start + move; c.length = orig[k].length - move; c.offset = jmax (0.0, orig[k].offset + move);
            }
        }
        followLoop (st.song());                                    // the loop moves with its chunk while you drag
        st.proc.publish(); repaint();
    }
    void mouseUp (const MouseEvent&) override
    {
        if (rzDown) {
            rzDown = false;
            if (! rzZoom && rzY < rulerH) { loopMenu(); return; }
            if (! rzZoom) {
                auto& clips = st.song().clips; const int i = rzClip;
                cursorBeat = snapBeat (rzBeat, false);
                if (i >= 0 && i < (int) clips.size() && ! clips[(size_t) i].selected) { for (auto& c : clips) c.selected = false; clips[(size_t) i].selected = true; }
                menu (i); repaint();
            }
            return;
        }
        if (mode == Move || mode == ResizeL || mode == ResizeR) st.proc.endEdit (copying ? "Copy chunks" : mode == Move ? "Move chunks" : "Chunk length");
        mode = None; band = {}; anchor = -1; repaint();
    }
    void mouseMove (const MouseEvent& e) override
    {
        if (e.y < rulerH) {
            const auto& sg = st.song();
            const bool edge = std::abs (e.position.x - (float) beatToX (sg.loopEnd)) < 6 || std::abs (e.position.x - (float) beatToX (sg.loopStart)) < 6;
            setMouseCursor (edge ? MouseCursor::LeftRightResizeCursor : MouseCursor::NormalCursor); return;
        }
        const int i = clipAt (e.position);
        if (i < 0) { setMouseCursor (MouseCursor::NormalCursor); return; }
        const auto r = clipRect (st.song().clips[(size_t) i]);
        setMouseCursor (e.position.x > r.getRight() - 6 || e.position.x < r.getX() + 6 ? MouseCursor::LeftRightResizeCursor : MouseCursor::NormalCursor);
    }
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        if (e.mods.isCommandDown()) {
            const double b = xToBeat (e.x);
            pxPerBeat = jlimit (2.0f, 120.0f, pxPerBeat * (w.deltaY > 0 ? 1.15f : 1.0f / 1.15f));
            scrollX = jmax (0, (int) (b * pxPerBeat - e.x));
        } else scrollX = jmax (0, scrollX - (int) ((std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY) * 400));
        repaint();
    }

    // ------------------------------------------------------------ drops from the keyboard
    bool isInterestedInDragSource (const SourceDetails& d) override { const auto s = d.description.toString(); return s.startsWith ("key:") || s.startsWith ("park:"); }
    void itemDragMove (const SourceDetails& d) override { dropBeat = snapBeat (xToBeat (d.localPosition.x), false); repaint(); }
    void itemDragExit (const SourceDetails&) override { dropBeat = -1; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        const double at = snapBeat (xToBeat (d.localPosition.x), false);
        dropBeat = -1;
        if (d.description.toString().startsWith ("park:")) { if (onDropPattern) onDropPattern (d.description.toString(), at); repaint(); return; }
        const int k = d.description.toString().fromFirstOccurrenceOf ("key:", false, false).getIntValue();
        const double len = jmax (1.0, st.song().keys[(size_t) jlimit (0, 127, k)].lengthTicks() / (double) PPQ);
        st.applySong ("Add chunk", [k, at, len] (Song& s) {
            if (s.clips.empty()) s.songMode = true;               // the first chunk: the lane plays with Studio One from now on (Settings)
            for (auto& c : s.clips) c.selected = false; Clip c; c.start = at; c.length = len; c.key = k; c.selected = true; s.clips.push_back (c);
            s.loopStart = c.start; s.loopEnd = c.start + c.length; s.loopOn = true; s.loopClip = c.id; });     // inserting a pattern makes the loop
        cursorBeat = at + len;
    }

    // ------------------------------------------------------------ actions (also on the keyboard, Studio One style)
    void duplicate()
    {
        st.applySong ("Duplicate chunks", [] (Song& s) {
            double lo = 1e9, hi = 0; for (auto& c : s.clips) if (c.selected) { lo = jmin (lo, c.start); hi = jmax (hi, c.start + c.length); }
            if (lo > hi) return;
            std::vector<Clip> add;
            for (auto& c : s.clips) if (c.selected) { auto x = c; x.start += hi - lo; add.push_back (x); c.selected = false; }
            for (auto& x : add) s.clips.push_back (x);
        });
    }
    // cut one chunk (or the selection, when the chunk is in it) at a spot
    void splitAt (double at, int clipIndex)
    {
        st.applySong ("Cut chunk", [at, clipIndex] (Song& s) {
            if (clipIndex < 0 || clipIndex >= (int) s.clips.size()) return;
            const bool many = s.clips[(size_t) clipIndex].selected;
            std::vector<Clip> add;
            for (int i = 0; i < (int) s.clips.size(); ++i) {
                auto& c = s.clips[(size_t) i];
                if (! (i == clipIndex || (many && c.selected)) || at <= c.start + 1e-6 || at >= c.start + c.length - 1e-6) continue;
                Clip r = c; const double cut = at - c.start;
                c.length = cut; r.start = at; r.length -= cut; r.offset += cut; r.selected = false;
                add.push_back (r);
            }
            for (auto& x : add) s.clips.push_back (x);
        });
        cursorBeat = at; repaint();
    }
    void splitAtCursor()
    {
        const double at = cursorBeat;
        st.applySong ("Split chunks", [at] (Song& s) {
            bool any = false; for (auto& c : s.clips) any |= c.selected;
            std::vector<Clip> add;
            for (auto& c : s.clips) {
                if ((any && ! c.selected) || at <= c.start + 1e-6 || at >= c.start + c.length - 1e-6) continue;
                Clip r = c; const double cut = at - c.start;
                c.length = cut; r.start = at; r.length -= cut; r.offset += cut; r.selected = false;
                add.push_back (r);
            }
            for (auto& x : add) s.clips.push_back (x);
        });
    }
    // the loop's own menu (right-click the loop bar)
    void loopMenu()
    {
        auto& sg = st.song();
        PopupMenu m; m.addSectionHeader ("LOOP");
        m.addItem (1, "Loop on", true, sg.loopOn);
        m.addItem (2, "Loop follows the pattern", true, sg.loopFollows);
        bool anySel = false; for (auto& c : sg.clips) anySel |= c.selected;
        m.addItem (3, "Loop around the selected chunks", anySel);
        m.addItem (4, "Loop the whole song", ! sg.clips.empty());
        m.addItem (5, "Loop from the cursor, 4 bars");
        m.showMenuAsync (PopupMenu::Options().withMousePosition(), [this] (int r) {
            auto& s2 = st.song();
            if (r == 1) s2.loopOn = ! s2.loopOn;
            else if (r == 2) { s2.loopFollows = ! s2.loopFollows; followLoop (s2); }
            else if (r == 3 || r == 4) {
                double lo = 1e9, hi = 0; int n = 0; uint32 id = 0;
                for (auto& c : s2.clips) if (r == 4 || c.selected) { lo = jmin (lo, c.start); hi = jmax (hi, c.start + c.length); ++n; id = c.id; }
                if (hi > lo) { s2.loopStart = lo; s2.loopEnd = hi; s2.loopOn = true; s2.loopClip = n == 1 ? id : 0; }
            }
            else if (r == 5) { s2.loopStart = cursorBeat; s2.loopEnd = cursorBeat + 4 * st.pat().beatsPerBar; s2.loopOn = true; s2.loopClip = 0; }
            else return;
            st.proc.publish(); repaint();
        });
    }
    void deleteSelected() { st.applySong ("Delete chunks", [] (Song& s) { s.clips.erase (std::remove_if (s.clips.begin(), s.clips.end(), [] (const Clip& c) { return c.selected; }), s.clips.end()); }); }
    void muteSelected() { st.applySong ("Mute chunks", [] (Song& s) { for (auto& c : s.clips) if (c.selected) c.muted = ! c.muted; }); }
    void selectAll() { for (auto& c : st.song().clips) c.selected = true; repaint(); }
    void copy() { clipboard.clear(); double lo = 1e9; for (auto& c : st.song().clips) if (c.selected) { clipboard.push_back (c); lo = jmin (lo, c.start); } for (auto& c : clipboard) c.start -= lo; }
    void paste() { auto clip = clipboard; const double at = cursorBeat; st.applySong ("Paste chunks", [clip, at] (Song& s) { for (auto& c : s.clips) c.selected = false; for (auto c : clip) { c.start += at; c.selected = true; s.clips.push_back (c); } }); }
    void makeUnique()
    {
        st.applySong ("Make unique", [] (Song& s) {
            for (auto& c : s.clips) if (c.selected) {
                int free = -1; for (int k = s.settings.zoneLow; k <= s.settings.zoneHigh; ++k) if (s.keys[(size_t) k].isEmpty() && ! s.settings.isStop (k)) { free = k; break; }
                if (free < 0) return;
                s.keys[(size_t) free] = s.keys[(size_t) c.key];
                s.keys[(size_t) free].name = s.keys[(size_t) c.key].name + " (unique)";
                c.key = free;
            }
        });
    }
    bool anySelected() const { for (auto& c : st.song().clips) if (c.selected) return true; return false; }

private:
    enum Mode { None, Move, ResizeL, ResizeR, Band, LoopL, LoopR, LoopMove, LoopNew };
    double loopOrigA = 0, loopOrigB = 4;
    EditState& st;
    Mode mode = None; Point<float> downPos; std::vector<Clip> orig, clipboard; int anchor = -1; bool copying = false;
    Rectangle<float> band; double dropBeat = -1;
    bool rzDown = false, rzZoom = false; float rzY = 0, rzX = 0, rzPx = 12; double rzBeat = 0; int rzClip = -1;

    Rectangle<float> clipRect (const Clip& c) const
    {
        const float x0 = (float) beatToX (c.start), x1 = (float) beatToX (c.start + c.length);
        return { x0, (float) rulerH + 3, jmax (4.0f, x1 - x0 - 1), (float) getHeight() - rulerH - 6 };
    }
    int clipAt (Point<float> p) const
    {
        auto& clips = st.song().clips;
        for (int i = (int) clips.size() - 1; i >= 0; --i) if (clipRect (clips[(size_t) i]).contains (p)) return i;
        return -1;
    }
    void menu (int i)
    {
        PopupMenu m;
        if (i >= 0) {
            m.addItem (1, "Edit this pattern");
            m.addItem (2, "Duplicate  (D)"); m.addItem (4, st.song().clips[(size_t) i].muted ? "Unmute  (M)" : "Mute  (M)");
            m.addItem (5, "Delete  (Del)"); m.addItem (6, "Copy  (Ctrl+C)");
            const auto& c = st.song().clips[(size_t) i];
            m.addItem (3, "Scissors: split here, at the cursor  (Alt+X)", cursorBeat > c.start + 1e-6 && cursorBeat < c.start + c.length - 1e-6);
            PopupMenu reps; for (int r : { 1, 2, 3, 4, 8 }) reps.addItem (100 + r, String (r) + (r == 1 ? " time" : " times"));
            m.addSubMenu ("Length: pattern repeated", reps);
            m.addItem (8, "Make unique (its own copy of the pattern)");
        } else {
            if (! clipboard.empty()) m.addItem (7, "Paste at the cursor  (Ctrl+V)");
            if (! st.song().clips.empty()) m.addItem (9, "Select all  (Ctrl+A)");
            bool cut = false; for (auto& c : st.song().clips) cut |= cursorBeat > c.start + 1e-6 && cursorBeat < c.start + c.length - 1e-6;
            if (cut) m.addItem (3, "Split chunks at the cursor  (Alt+X)");
            if (m.getNumItems() == 0) { m.addItem (-1, "Drag keys from the keyboard here to arrange a song", false); }
        }
        const int key = i >= 0 ? st.song().clips[(size_t) i].key : -1;
        m.showMenuAsync (PopupMenu::Options().withMousePosition(), [this, key] (int r) {
            switch (r) {
                case 1: if (key >= 0) { st.song().editKey = key; if (st.changed) st.changed(); } break;
                case 2: duplicate(); break; case 3: splitAtCursor(); break; case 4: muteSelected(); break;
                case 5: deleteSelected(); break; case 6: copy(); break; case 7: paste(); break;
                case 8: makeUnique(); break; case 9: selectAll(); break;
                default:
                    if (r > 100) { const int n = r - 100; st.applySong ("Chunk length", [n] (Song& s) {
                        for (auto& c : s.clips) if (c.selected) c.length = jmax (0.25, n * s.keys[(size_t) c.key].lengthTicks() / (double) PPQ); }); }
                    break;
            }
            repaint();
        });
    }
};

// the song lane's controls, sitting under the lane names column
class SongHeader : public Component
{
public:
    SongHeader (EditState& e, SongLane& l) : st (e), lane (l)
    {
        setupButton (*this, onBtn, "Play song", true, "On: the song lane plays, with Studio One's transport or the Play button here. Off: your keys play patterns as usual");
        onBtn.onClick = [this] { const bool on = onBtn.getToggleState(); st.applySong ("Song mode", [on] (Song& s) { s.songMode = on; }); };
        setupButton (*this, play, "Play"); play.onClick = [this] { if (! st.song().songMode) st.applySong ("Song mode", [] (Song& s) { s.songMode = true; }); st.proc.uiSongPlay (true, lane.cursorBeat); };
        setupButton (*this, stop, "Stop"); stop.onClick = [this] { st.proc.uiSongPlay (false, 0); };
        setupButton (*this, rec, "Rec", true, "Record what you play with the pattern keys onto this lane, at Studio One's position (or from the cursor when Studio One is stopped). Press again to finish");
        rec.getProperties().set ("lit", (int) 0xffff5a5a);
        rec.onClick = [this] {
            const bool on = rec.getToggleState();
            if (on) { st.song().songMode = false; st.proc.songRecCursor = lane.cursorBeat; st.proc.publish(); }
            st.proc.setSongRecord (on);
            if (st.toast) st.toast (on ? "Recording: play your pattern keys" : "Recording finished");
        };
        { StringArray sn; for (auto& [n, b] : SongLane::snapBeats()) sn.add (n);
          setupCombo (*this, snap, sn, 1, "Chunks also click onto each other's edges. Alt while dragging = free. With the scissors (3), clicking a chunk cuts it on this grid"); }
        snap.setSelectedItemIndex (0, dontSendNotification);
        snap.onChange = [this] { lane.snap = snap.getSelectedItemIndex(); };
        dragOut.setButtonText ("Drag lane to DAW"); dragOut.setSize (fitW ("Drag lane to DAW"), btnH); dragOut.setComponentID ("Song MIDI"); addAndMakeVisible (dragOut);
        dragOut.setTooltip ("Drag into Studio One: the selected chunks (gaps kept, starting where you drop it), or the whole lane from bar 1 if none are selected");
    }
    void refresh() { onBtn.setToggleState (st.song().songMode, dontSendNotification); rec.setToggleState (st.proc.songRecArmed.load(), dontSendNotification); }
    void paint (Graphics& g) override
    {
        fillArea (g, getLocalBounds(), "Lane names", th::panel);
        g.setFont (font (10.0f, 3)); g.setColour (th::faint); g.drawText ("SONG", 8, 2, 50, 14, Justification::centredLeft);
        g.setColour (th::line); g.drawVerticalLine (getWidth() - 1, 0, (float) getHeight());
    }
    void resized() override
    {
        // the transport at the bottom plays and records the song; here: the lane's grid and the drag to Studio One
        for (auto* b : { &onBtn, &play, &stop, &rec }) b->setVisible (false);
        auto a = getLocalBounds().reduced (6, 3);
        auto r1 = a.removeFromTop (btnH); r1.removeFromLeft (44);
        dragOut.setBounds (r1.removeFromLeft (jmin (r1.getWidth(), dragOut.getWidth())));
        a.removeFromTop (4);
        snap.setBounds (a.removeFromTop (btnH).removeFromLeft (jmin (a.getWidth(), 120)));
    }
private:
    struct DragSong : public TextButton
    {
        EditState* st = nullptr; bool dragging = false;
        void mouseDrag (const MouseEvent& e) override
        {
            if (dragging || st == nullptr || e.getDistanceFromDragStart() < 5 || st->song().clips.empty()) return;
            dragging = true;
            auto dir = dataFolder().getChildFile ("Temp"); dir.createDirectory();
            bool anySel = false; for (auto& c : st->song().clips) anySel |= c.selected;
            auto f = dir.getChildFile (anySel ? "Drum Player selection.mid" : "Drum Player Song.mid");
            writeSongMidi (st->song(), f, st->song().settings.outChannel, anySel);
            DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { dragging = false; });
        }
    };
    EditState& st; SongLane& lane;
    TextButton onBtn, play, stop, rec; ComboBox snap;
    DragSong dragOut;
public:
    void attach() { dragOut.st = &st; }
};


// ---------------------------------------------------------------- velocity curve: a sine (or other wave) drawn over the velocities, live
class CurvePanel : public Component
{
public:
    explicit CurvePanel (EditState& e) : st (e)
    {
        auto& p = st.pat();
        bool anySel = false; for (auto& n : p.notes) anySel |= n.selected;
        for (int i = 0; i < (int) p.notes.size(); ++i) {
            const auto& n = p.notes[(size_t) i];
            if (anySel ? n.selected : (st.focusLane < 0 || n.note == st.focusLane)) { idx.push_back (i); orig.push_back (n.velocity); lo = jmin (lo, n.tick); }
        }
        if (lo == INT_MAX) lo = 0;
        st.proc.beginEdit();
        setupLabel (*this, title, anySel ? "Velocity curve on the selected notes" : "Velocity curve on the notes shown", 12.5f, th::amber);
        setupLabel (*this, lShape, "Shape"); setupCombo (*this, shape, { "Sine wave", "Triangle", "Ramp up", "Ramp down", "Saw (up each cycle)", "Random" });
        setupLabel (*this, lCycle, "One cycle"); setupCombo (*this, cycle, { "1/8", "1/4 (a beat)", "1/2 bar", "1 bar", "2 bars", "All the notes" });
        setupLabel (*this, lMode, "Mode"); setupCombo (*this, mode, { "Set the velocity", "Add to the velocity it has" });
        setupLabel (*this, lCentre, "Middle"); setupSlider (*this, centre, th::blue); centre.setRange (1, 127, 1);
        setupLabel (*this, lDepth, "Up and down"); setupSlider (*this, depth, th::amber); depth.setRange (0, 63, 1);
        setupLabel (*this, lPhase, "Start point"); setupSlider (*this, phase, th::blue); phase.setRange (0, 360, 1); phase.setTextValueSuffix (" deg");
        shape.setSelectedItemIndex (0, dontSendNotification); cycle.setSelectedItemIndex (1, dontSendNotification); mode.setSelectedItemIndex (0, dontSendNotification);
        centre.setValue (90, dontSendNotification); depth.setValue (25, dontSendNotification); phase.setValue (90, dontSendNotification);
        for (auto* c : { &shape, &cycle, &mode }) c->onChange = [this] { apply(); };
        for (auto* sl : { &centre, &depth, &phase }) sl->onValueChange = [this] { apply(); };
        setupLabel (*this, help, "Changes show live. Click outside to keep it; Ctrl+Z undoes it.", 10.5f, th::faint);
        setSize (380, 252);
        apply();
    }
    ~CurvePanel() override { st.proc.endEdit ("Velocity curve"); if (st.changed) st.changed(); }
    void paint (Graphics& g) override { g.fillAll (th::panel); }
    void resized() override
    {
        auto a = getLocalBounds().reduced (12, 8);
        title.setBounds (a.removeFromTop (22)); a.removeFromTop (4);
        auto rowOf = [&a] (Label& l, Component& c, int w) { auto r = a.removeFromTop (26); a.removeFromTop (4); l.setBounds (r.removeFromLeft (90)); c.setBounds (w > 0 ? r.removeFromLeft (w) : r); };
        rowOf (lShape, shape, 180); rowOf (lCycle, cycle, 140); rowOf (lMode, mode, 200);
        rowOf (lCentre, centre, 0); rowOf (lDepth, depth, 0); rowOf (lPhase, phase, 0);
        help.setBounds (a.removeFromTop (20));
    }
private:
    EditState& st;
    std::vector<int> idx, orig; int lo = INT_MAX;
    Label title, lShape, lCycle, lMode, lCentre, lDepth, lPhase, help;
    ComboBox shape, cycle, mode;
    FineSlider centre, depth, phase;

    void apply()
    {
        auto& p = st.pat();
        if (idx.empty()) return;
        int hi = lo; for (int i : idx) if (i < (int) p.notes.size()) hi = jmax (hi, p.notes[(size_t) i].tick);
        static const int cyc[] = { 480, 960, 0, 0, 0, 0 };
        const int bar = p.beatsPerBar * PPQ;
        const int ci = cycle.getSelectedItemIndex();
        double len = ci <= 1 ? cyc[ci] : ci == 2 ? bar / 2.0 : ci == 3 ? bar : ci == 4 ? bar * 2.0 : jmax (1, hi - lo + 1);
        const double ph = phase.getValue() / 360.0;
        std::mt19937 rng ((uint32) (centre.getValue() * 131 + depth.getValue() * 7 + ph * 1000));
        std::uniform_real_distribution<double> rnd (-1.0, 1.0);
        for (size_t k = 0; k < idx.size(); ++k) {
            if (idx[k] >= (int) p.notes.size()) continue;
            auto& n = p.notes[(size_t) idx[k]];
            double x = std::fmod ((n.tick - lo) / len + ph, 1.0); if (x < 0) x += 1.0;
            double w = 0;
            switch (shape.getSelectedItemIndex()) {
                case 0: w = std::sin (x * MathConstants<double>::twoPi); break;
                case 1: w = x < 0.5 ? x * 4 - 1 : 3 - x * 4; break;
                case 2: w = (n.tick - lo) / (double) jmax (1, hi - lo) * 2 - 1; break;
                case 3: w = 1 - (n.tick - lo) / (double) jmax (1, hi - lo) * 2; break;
                case 4: w = x * 2 - 1; break;
                default: w = rnd (rng); break;
            }
            const double base = mode.getSelectedItemIndex() == 0 ? centre.getValue() : orig[k] + (centre.getValue() - 64);
            n.velocity = jlimit (1, 127, (int) std::round (base + depth.getValue() * w));
        }
        st.publish();
    }
};

// ---------------------------------------------------------------- quantize page
class QuantizePage : public Page
{
public:
    QuantizePage (EditState& e) : Page ("QUANTIZE SETUP"), st (e)
    {
        StringArray vals; for (auto& v : noteValues) vals.add (v.name);
        setupLabel (*this, lGrid, "Quantize to"); setupCombo (*this, grid, vals); grid.setSelectedItemIndex (3, dontSendNotification);
        setupLabel (*this, lMode, "Mode");
        setupCombo (*this, mode, { "Normal: everything to the grid", "Catch: only notes near the grid", "Dual: straight and triplets each to their own grid" });
        mode.setSelectedItemIndex (0, dontSendNotification);
        setupLabel (*this, lCatch, "Catch range");
        setupSlider (*this, catchR); catchR.setRange (5, 50, 1); catchR.setValue (25); catchR.setTextValueSuffix (" %");
        setupLabel (*this, lStr, "Strength");
        setupSlider (*this, strength); strength.setRange (0, 100, 1); strength.setValue (100); strength.setTextValueSuffix (" %");
        setupButton (*this, selOnly, "Only selected notes", true); selOnly.setToggleState (true, dontSendNotification);

        // ---- keep the swing: open it to see how it works and set it
        setupButton (*this, swingHead, "Keep the swing", true, "Open to see how it works and set it");
        swingHead.onClick = [this] { swingOpen = swingHead.getToggleState(); resized(); save(); };
        setupButton (*this, keepSwing, "On", true, "Notes snap to the swung grid, so a swung groove stays swung");
        setupLabel (*this, lSwAmt, "Swing amount");
        setupCombo (*this, swingFrom, { "The Swing setting in the toolbar", "My own amount" });
        swingFrom.setSelectedItemIndex (0, dontSendNotification);
        setupSlider (*this, ownSwing); ownSwing.setRange (50, 75, 0.5); ownSwing.setValue (58, dontSendNotification); ownSwing.setTextValueSuffix (" %");
        setupLabel (*this, lSwOn, "Swing on");
        StringArray sv { "The quantize grid" }; for (auto& v : noteValues) if (! v.triplet) sv.add (v.name);
        setupCombo (*this, swingOn, sv); swingOn.setSelectedItemIndex (0, dontSendNotification);
        setupLabel (*this, swingHelp, {}, 10.5f, th::dim); swingHelp.setJustificationType (Justification::topLeft);

        // ---- a rule for each kind of note
        setupButton (*this, rulesHead, "Rules for each note value", true, "Open to decide, for each kind of note, whether it goes to the grid, to its own value or stays, and how far it moves");
        rulesHead.onClick = [this] { rulesOpen = rulesHead.getToggleState(); resized(); save(); };
        setupButton (*this, useRules, "Use these rules", true, "On: every note is first recognised (1/16, 1/16 triplet, 1/32...), then moved by its own rule");
        setupButton (*this, presetsBtn, "Presets"); presetsBtn.onClick = [this] { presetMenu(); };
        for (int c = 0; c < numQClasses; ++c) {
            auto* r = rules.add (new RuleRow());
            setupLabel (*this, r->name, qClassName (c));
            setupCombo (*this, r->action, { "To the grid", "To its own value", "Leave alone" });
            r->action.setSelectedItemIndex (0, dontSendNotification);
            setupSlider (*this, r->amount); r->amount.setRange (0, 100, 1); r->amount.setValue (100, dontSendNotification); r->amount.setTextValueSuffix (" %");
            r->action.onChange = [this] { updateHelp(); save(); };
            r->amount.onValueChange = [this] { save(); };
        }
        setupLabel (*this, rulesHelp, "To the grid: moves to the grid you quantize to. To its own value: a triplet stays a triplet, a 1/32 stays a 1/32, just tidied. "
                                      "Leave alone: never moved. The % is how far it moves (100 % all the way, 40 % less than half, more human).", 10.5f, th::faint);
        rulesHelp.setJustificationType (Justification::topLeft);

        for (auto* b : { &keepSwing, &useRules, &selOnly }) b->onClick = [this] { updateHelp(); save(); };
        swingFrom.onChange = ownSwing.onValueChange = [this] { updateHelp(); save(); };
        swingOn.onChange = grid.onChange = mode.onChange = [this] { updateHelp(); save(); };
        catchR.onValueChange = strength.onValueChange = [this] { save(); };

        setupLabel (*this, lRec, "While recording");
        StringArray rq { "Off (as played)" }; for (auto& v : noteValues) rq.add (v.name);
        setupCombo (*this, recQ, rq);
        recQ.onChange = [this] { st.song().settings.recQuantize = recQ.getSelectedItemIndex(); st.publish(); };
        setupButton (*this, apply, "Quantize now  (Q)");
        apply.onClick = [this] { run(); };
        setupLabel (*this, help, "Catch: with 1/16 and 25 %, triplets and faster notes are left alone. Dual: 16ths snap to 16ths, triplets to triplets.", 10.5f, th::faint);
        help.setJustificationType (Justification::topLeft);
        load(); updateHelp();
    }
    QuantOptions options() const
    {
        QuantOptions o;
        o.valueIndex = grid.getSelectedItemIndex(); o.mode = (QuantMode) mode.getSelectedItemIndex();
        o.catchPct = catchR.getValue(); o.strength = strength.getValue(); o.keepSwing = keepSwing.getToggleState();
        o.selectedOnly = selOnly.getToggleState();
        o.swingPct = swingFrom.getSelectedItemIndex() == 1 ? ownSwing.getValue() : (double) st.proc.apvts.getRawParameterValue (ids::swing)->load();
        o.swingValue = swingValueIndex();
        o.useRules = useRules.getToggleState();
        for (int c = 0; c < numQClasses; ++c) { o.rules[(size_t) c].action = rules[c]->action.getSelectedItemIndex(); o.rules[(size_t) c].strength = rules[c]->amount.getValue(); }
        return o;
    }
    void run() { auto o = options(); st.apply ("Quantize", [o] (Pattern& p) { quantize (p, o); }); }
    void refresh() override { recQ.setSelectedItemIndex (st.song().settings.recQuantize, dontSendNotification); updateHelp(); }
    void resized() override
    {
        auto a = content();
        grid.setBounds (row (a, lGrid).removeFromLeft (90));
        mode.setBounds (row (a, lMode));
        catchR.setBounds (row (a, lCatch));
        strength.setBounds (row (a, lStr));
        {   auto r = a.removeFromTop (26); a.removeFromTop (4); selOnly.setBounds (r.removeFromLeft (selOnly.getWidth()).reduced (0, 1)); }
        // keep the swing
        {   auto r = a.removeFromTop (26); a.removeFromTop (3);
            swingHead.setBounds (r.removeFromLeft (swingHead.getWidth()).reduced (0, 1)); r.removeFromLeft (8);
            keepSwing.setBounds (r.removeFromLeft (keepSwing.getWidth()).reduced (0, 1)); }
        for (Component* c : { (Component*) &lSwAmt, (Component*) &swingFrom, (Component*) &ownSwing, (Component*) &lSwOn, (Component*) &swingOn, (Component*) &swingHelp }) c->setVisible (swingOpen);
        if (swingOpen) {
            auto r = row (a, lSwAmt); swingFrom.setBounds (r.removeFromLeft (210)); r.removeFromLeft (6); ownSwing.setBounds (r);
            ownSwing.setEnabled (swingFrom.getSelectedItemIndex() == 1);
            swingOn.setBounds (row (a, lSwOn).removeFromLeft (160));
            swingHelp.setBounds (a.removeFromTop (46)); a.removeFromTop (4);
        }
        // rules
        {   auto r = a.removeFromTop (26); a.removeFromTop (3);
            rulesHead.setBounds (r.removeFromLeft (rulesHead.getWidth()).reduced (0, 1)); r.removeFromLeft (8);
            useRules.setBounds (r.removeFromLeft (useRules.getWidth()).reduced (0, 1)); r.removeFromLeft (8);
            presetsBtn.setBounds (r.removeFromLeft (presetsBtn.getWidth()).reduced (0, 1)); }
        for (auto* r : rules) for (Component* c : { (Component*) &r->name, (Component*) &r->action, (Component*) &r->amount }) c->setVisible (rulesOpen);
        rulesHelp.setVisible (rulesOpen); presetsBtn.setVisible (rulesOpen);
        if (rulesOpen) {
            for (auto* r : rules) {
                auto rr = a.removeFromTop (24); a.removeFromTop (2);
                r->name.setBounds (rr.removeFromLeft (110)); r->action.setBounds (rr.removeFromLeft (140).reduced (0, 1)); rr.removeFromLeft (6); r->amount.setBounds (rr);
            }
            rulesHelp.setBounds (a.removeFromTop (44)); a.removeFromTop (4);
        }
        apply.setBounds (a.removeFromTop (28).removeFromLeft (apply.getWidth()).reduced (0, 3)); a.removeFromTop (6);
        recQ.setBounds (row (a, lRec).removeFromLeft (140));
        help.setBounds (a.removeFromTop (40));
    }
    ComboBox grid;
private:
    EditState& st;
    Label lGrid, lMode, lCatch, lStr, lRec, help, lSwAmt, lSwOn, swingHelp, rulesHelp;
    ComboBox mode, recQ, swingFrom, swingOn;
    FineSlider catchR, strength, ownSwing;
    TextButton selOnly, apply, swingHead, keepSwing, rulesHead, useRules, presetsBtn;
    bool swingOpen = false, rulesOpen = false;
    struct RuleRow { Label name; ComboBox action; FineSlider amount; };
    OwnedArray<RuleRow> rules;

    int swingValueIndex() const
    {
        const int i = swingOn.getSelectedItemIndex(); if (i <= 0) return -1;
        int n = 0; for (int v = 0; v < numNoteValues; ++v) if (! noteValues[v].triplet && ++n == i) return v;
        return -1;
    }
    // how keep-the-swing works, with today's numbers
    void updateHelp()
    {
        const double sw = swingFrom.getSelectedItemIndex() == 1 ? ownSwing.getValue() : (double) st.proc.apvts.getRawParameterValue (ids::swing)->load();
        const int vi = swingValueIndex();
        const auto gName = String (noteValues[jlimit (0, numNoteValues - 1, vi >= 0 ? vi : grid.getSelectedItemIndex())].name);
        const int stepTicks = noteValues[jlimit (0, numNoteValues - 1, vi >= 0 ? vi : grid.getSelectedItemIndex())].ticks;
        const int late = (int) std::round ((sw - 50.0) / 50.0 * stepTicks);
        String t;
        t << "How it works: the grid is cut into pairs of " << gName << " steps. The first of each pair stays on the beat, the second is moved late by the swing ("
               << String (sw, 1) << " % = " << late << " ticks of 960 per beat). With it on, the notes snap to that swung grid, so a swung groove stays swung.";
        if (sw <= 50.01) t << " The swing is 50 % now (straight), so nothing is moved late: raise the Swing, or use My own amount.";
        swingHelp.setText (t, dontSendNotification);
        ownSwing.setEnabled (swingFrom.getSelectedItemIndex() == 1);
        for (auto* r : rules) r->amount.setEnabled (r->action.getSelectedItemIndex() != 2);
    }
    void presetMenu()
    {
        PopupMenu m;
        m.addItem (1, "Everything to the grid, 100 %");
        m.addItem (2, "Leave the triplets alone");
        m.addItem (3, "Leave notes faster than 1/16 alone");
        m.addItem (4, "Triplets stay triplets, the rest to the grid");
        m.addItem (5, "Human: 1/4 and 1/8 firm, 1/16 70 %, fast notes 40 %");
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&presetsBtn), [this] (int r) {
            if (r <= 0) return;
            for (int c = 0; c < numQClasses; ++c) {
                const bool trip = c == 2 || c == 4 || c == 6, fast = c >= 5;
                int a = 0; double amt = 100;
                if (r == 2 && trip) a = 2;
                if (r == 3 && (fast || c == 4 || c == 6)) a = 2;
                if (r == 4 && trip) a = 1;
                if (r == 5) { amt = c <= 2 ? 100 : c <= 4 ? 70 : 40; if (trip) a = 1; }
                rules[c]->action.setSelectedItemIndex (a, dontSendNotification); rules[c]->amount.setValue (amt, dontSendNotification);
            }
            useRules.setToggleState (true, dontSendNotification);
            updateHelp(); save();
        });
    }
    static File prefsFile() { return dataFolder().getChildFile ("Quantize.xml"); }
    bool loading = false;
    void save()
    {
        if (loading) return;
        XmlElement x ("DrumPlayerQuantize");
        x.setAttribute ("grid", grid.getSelectedItemIndex()); x.setAttribute ("mode", mode.getSelectedItemIndex());
        x.setAttribute ("catch", catchR.getValue()); x.setAttribute ("strength", strength.getValue()); x.setAttribute ("sel", selOnly.getToggleState());
        x.setAttribute ("keepSwing", keepSwing.getToggleState()); x.setAttribute ("swingFrom", swingFrom.getSelectedItemIndex());
        x.setAttribute ("ownSwing", ownSwing.getValue()); x.setAttribute ("swingOn", swingOn.getSelectedItemIndex());
        x.setAttribute ("useRules", useRules.getToggleState()); x.setAttribute ("swingOpen", swingOpen); x.setAttribute ("rulesOpen", rulesOpen);
        for (int c = 0; c < numQClasses; ++c) { auto* r = x.createNewChildElement ("Rule"); r->setAttribute ("a", rules[c]->action.getSelectedItemIndex()); r->setAttribute ("s", rules[c]->amount.getValue()); }
        x.writeTo (prefsFile());
    }
    void load()
    {
        auto x = XmlDocument::parse (prefsFile()); if (x == nullptr) return;
        const ScopedValueSetter<bool> sv (loading, true);
        grid.setSelectedItemIndex (x->getIntAttribute ("grid", 3), dontSendNotification); mode.setSelectedItemIndex (x->getIntAttribute ("mode", 0), dontSendNotification);
        catchR.setValue (x->getDoubleAttribute ("catch", 25), dontSendNotification); strength.setValue (x->getDoubleAttribute ("strength", 100), dontSendNotification);
        selOnly.setToggleState (x->getBoolAttribute ("sel", true), dontSendNotification);
        keepSwing.setToggleState (x->getBoolAttribute ("keepSwing"), dontSendNotification); swingFrom.setSelectedItemIndex (x->getIntAttribute ("swingFrom", 0), dontSendNotification);
        ownSwing.setValue (x->getDoubleAttribute ("ownSwing", 58), dontSendNotification); swingOn.setSelectedItemIndex (x->getIntAttribute ("swingOn", 0), dontSendNotification);
        useRules.setToggleState (x->getBoolAttribute ("useRules"), dontSendNotification);
        swingOpen = x->getBoolAttribute ("swingOpen"); rulesOpen = x->getBoolAttribute ("rulesOpen");
        swingHead.setToggleState (swingOpen, dontSendNotification); rulesHead.setToggleState (rulesOpen, dontSendNotification);
        int c = 0; for (auto* r : x->getChildWithTagNameIterator ("Rule")) { if (c >= numQClasses) break;
            rules[c]->action.setSelectedItemIndex (r->getIntAttribute ("a", 0), dontSendNotification); rules[c]->amount.setValue (r->getDoubleAttribute ("s", 100), dontSendNotification); ++c; }
    }
};


// ---------------------------------------------------------------- ghost notes: which drums, how many, how soft
class GhostPanel : public Component
{
public:
    explicit GhostPanel (EditState& e) : st (e)
    {
        setupLabel (*this, title, "Add ghost notes", 12.5f, th::amber);
        setupLabel (*this, lDrums, "On these drums");
        auto& s = st.song(); auto& p = st.pat();
        std::set<int> used; for (auto& n : p.notes) used.insert (n.note);
        for (auto& l : s.lanes) {
            if (! used.count (l.note) && ! isSnare (l)) continue;           // the drums this pattern plays, plus the snares
            auto* b = drums.add (new TextButton());
            setupButton (*this, *b, l.name, true);
            b->getProperties().set ("note", l.note);
            b->setToggleState (isSnare (l) && l.note == firstSnare(), dontSendNotification);
        }
        setupLabel (*this, lAmt, "How many"); setupSlider (*this, amount, th::amber); amount.setRange (5, 100, 1); amount.setValue (30, dontSendNotification); amount.setTextValueSuffix (" %");
        setupLabel (*this, lLo, "Softest"); setupSlider (*this, lo); lo.setRange (1, 90, 1); lo.setValue (15, dontSendNotification);
        setupLabel (*this, lHi, "Loudest"); setupSlider (*this, hi); hi.setRange (1, 110, 1); hi.setValue (42, dontSendNotification);
        setupLabel (*this, lGrid, "On the grid of");
        StringArray vals; for (auto& v : noteValues) vals.add (v.name);
        setupCombo (*this, grid, vals); grid.setSelectedItemIndex (st.grid >= 0 && st.grid > 1 ? st.grid : 3, dontSendNotification);
        setupButton (*this, between, "Only between the 8th notes", true); between.setToggleState (true, dontSendNotification);
        setupButton (*this, apply, "Add them"); apply.onClick = [this] { run(); };
        setupButton (*this, again, "Undo and try again"); again.onClick = [this] { if (added) { st.proc.undo.undo(); if (st.changed) st.changed(); } run(); };
        setupLabel (*this, help, "Ghost notes are quiet in-between hits, usually on the snare. Each try is different; Ctrl+Z takes them out.", 10.5f, th::faint);
        help.setJustificationType (Justification::topLeft);
        setSize (420, 230 + ((drums.size() + 2) / 3) * 28);
    }
    void paint (Graphics& g) override { g.fillAll (th::panel); }
    void resized() override
    {
        auto a = getLocalBounds().reduced (12, 8);
        title.setBounds (a.removeFromTop (22)); a.removeFromTop (4);
        lDrums.setBounds (a.removeFromTop (20));
        const int w = (a.getWidth() - 8) / 3;
        for (int i = 0; i < drums.size(); i += 3) {
            auto r = a.removeFromTop (26); a.removeFromTop (2);
            for (int k = i; k < jmin (i + 3, drums.size()); ++k) { drums[k]->setBounds (r.removeFromLeft (w).reduced (0, 2)); r.removeFromLeft (4); }
        }
        a.removeFromTop (6);
        auto rowOf = [&a] (Label& l, Component& c) { auto r = a.removeFromTop (26); a.removeFromTop (3); l.setBounds (r.removeFromLeft (100)); c.setBounds (r); };
        rowOf (lAmt, amount); rowOf (lLo, lo); rowOf (lHi, hi);
        { auto r = a.removeFromTop (26); a.removeFromTop (3); lGrid.setBounds (r.removeFromLeft (100)); grid.setBounds (r.removeFromLeft (80)); r.removeFromLeft (8); between.setBounds (r.removeFromLeft (between.getWidth()).reduced (0, 2)); }
        { auto r = a.removeFromTop (28); apply.setBounds (r.removeFromLeft (apply.getWidth()).reduced (0, 2)); r.removeFromLeft (6); again.setBounds (r.removeFromLeft (again.getWidth()).reduced (0, 2)); }
        help.setBounds (a.removeFromTop (34));
    }
private:
    EditState& st;
    Label title, lDrums, lAmt, lLo, lHi, lGrid, help;
    OwnedArray<TextButton> drums;
    FineSlider amount, lo, hi; ComboBox grid; TextButton between, apply, again;
    bool added = false;
    static bool isSnare (const Lane& l) { return l.name.containsIgnoreCase ("snare") || l.name.containsIgnoreCase ("side stick") || l.note == 38 || l.note == 40 || l.note == 37; }
    int firstSnare() const { for (auto& l : st.song().lanes) if (l.name.containsIgnoreCase ("snare") || l.note == 38) return l.note; return 38; }
    void run()
    {
        std::set<int> lanes;
        for (auto* b : drums) if (b->getToggleState()) lanes.insert ((int) b->getProperties()["note"]);
        if (lanes.empty()) { if (st.toast) st.toast ("Choose at least one drum"); return; }
        const int gi = grid.getSelectedItemIndex(), pct = (int) amount.getValue(), vl = (int) lo.getValue(), vh = (int) hi.getValue();
        const bool btw = between.getToggleState();
        const uint32 seed = Time::getMillisecondCounter();
        st.apply ("Add ghost notes", [=] (Pattern& p) { std::mt19937 r (seed); for (auto& n : p.notes) n.selected = false; addGhosts (p, gi, pct, r, lanes, vl, vh, btw); });
        added = true;
    }
};

// ---------------------------------------------------------------- humanize page
class HumanizePage : public Page
{
public:
    // the ghost-note options open over this page, inside the plugin window
    void openGhosts (Component& near)
    {
        auto* top = near.getTopLevelComponent();
        CallOutBox::launchAsynchronously (std::make_unique<GhostPanel> (st), top->getLocalArea (&near, near.getLocalBounds()), top);
    }
    HumanizePage (EditState& e) : Page ("HUMANIZE & FEEL"), st (e)
    {
        auto& ap = st.proc.apvts;
        auto make = [&] (Label& l, FineSlider& s, const char* id, const String& text, const String& suffix) {
            setupLabel (*this, l, text); setupSlider (*this, s); s.setTextValueSuffix (suffix);
            att.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (ap, id, s));
        };
        make (lT, hT, ids::humTime, "Timing (live)", " ms");
        make (lV, hV, ids::humVel, "Velocity (live)", "");
        make (lP, push, ids::push, "Push / lay back", " ms");
        make (lI, inten, ids::intensity, "Intensity", " %");
        make (lD, dyn, ids::dynamics, "Dynamics", " %");
        setupLabel (*this, lVint, "Vintage clock");
        setupCombo (*this, vint, { "Off", "96 per beat (MPC era)", "48 per beat (LM-1 era)", "24 per beat (early sync)" });
        vintAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (ap, ids::vintRes, vint);
        make (lW, wobble, ids::vintJitter, "Clock wobble", " ms");
        setupButton (*this, stagger, "MIDI cable stagger", true, "Notes that land together come out about 1 ms apart, like over a 5-pin MIDI cable");
        stagAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (ap, ids::stagger, stagger);
        setupLabel (*this, help, "Live settings change every pass, like a real drummer, without touching the notes. Shift-drag for fine steps.", 10.5f, th::faint);
        help.setJustificationType (Justification::topLeft);
        setupLabel (*this, lBake, "Into the notes");
        setupButton (*this, bake, "Humanize selection");
        bake.onClick = [this] {
            const double tpm = st.proc.lastBpm.load() / 60.0 * PPQ / 1000.0;
            const int tt = jmax (1, (int) std::round (hT.getValue() > 0 ? hT.getValue() * tpm : 8));
            const int vv = hV.getValue() > 0 ? (int) hV.getValue() : 12;
            st.apply ("Humanize", [tt, vv] (Pattern& p) { std::mt19937 rng ((uint32) Time::getMillisecondCounter()); humanizeBake (p, tt, vv, rng); });
        };
        setupLabel (*this, lRand, "Randomize");
        for (auto* b : { &rVel, &rTime, &rProb, &rThin, &rGhost }) addAndMakeVisible (b);
        setupButton (*this, rVel, "Velocities"); setupButton (*this, rTime, "Timing"); setupButton (*this, rProb, "Chance");
        setupButton (*this, rThin, "Thin out"); setupButton (*this, rGhost, "Add ghosts");
        rVel.onClick = [this] { st.apply ("Randomize velocity", [] (Pattern& p) { std::mt19937 r ((uint32) Time::getMillisecondCounter()); randomizeVelocity (p, 64, 120, r); }); };
        rTime.onClick = [this] { st.apply ("Randomize timing", [] (Pattern& p) { std::mt19937 r ((uint32) Time::getMillisecondCounter()); randomizeTiming (p, 12, r); }); };
        rProb.onClick = [this] { st.apply ("Randomize chance", [] (Pattern& p) { std::mt19937 r ((uint32) Time::getMillisecondCounter()); randomizeProbability (p, 40, 100, r); }); };
        rThin.onClick = [this] { st.apply ("Thin out", [] (Pattern& p) { std::mt19937 r ((uint32) Time::getMillisecondCounter()); thinOut (p, 25, r); }); };
        setupButton (*this, rGhost, "Add ghosts...", false, "Choose the drums (usually the snare), how many and how soft");
        rGhost.onClick = [this] { openGhosts (rGhost); };
    }
    void resized() override
    {
        auto a = content();
        hT.setBounds (row (a, lT)); hV.setBounds (row (a, lV)); push.setBounds (row (a, lP));
        inten.setBounds (row (a, lI)); dyn.setBounds (row (a, lD));
        auto vr = row (a, lVint); vint.setBounds (vr.removeFromLeft (200)); vr.removeFromLeft (8); stagger.setBounds (vr.removeFromLeft (stagger.getWidth()).reduced (0, 3));
        wobble.setBounds (row (a, lW));
        help.setBounds (a.removeFromTop (34)); a.removeFromTop (4);
        bake.setBounds (row (a, lBake).removeFromLeft (bake.getWidth()).reduced (0, 3));
        auto r = row (a, lRand);
        auto r2 = a.removeFromTop (28);
        for (auto* b : { &rVel, &rTime, &rProb }) { b->setBounds (r.removeFromLeft (b->getWidth()).reduced (0, 3)); r.removeFromLeft (5); }
        r2.removeFromLeft (130);
        for (auto* b : { &rThin, &rGhost }) { b->setBounds (r2.removeFromLeft (b->getWidth()).reduced (0, 3)); r2.removeFromLeft (5); }
    }
private:
    EditState& st;
    Label lT, lV, lP, lI, lD, help, lBake, lRand, lVint, lW;
    FineSlider hT, hV, push, inten, dyn, wobble;
    ComboBox vint; TextButton stagger;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> vintAtt;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> stagAtt;
    TextButton bake, rVel, rTime, rProb, rThin, rGhost;
    std::vector<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>> att;
};

// ---------------------------------------------------------------- chain page
class ChainPage : public Page
{
public:
    ChainPage (EditState& e) : Page ("CHAINS"), st (e)
    {
        setupCombo (*this, chainSel, {}, 1, "Which chain");
        chainSel.onChange = [this] { const int i = chainSel.getSelectedItemIndex(); if (i >= 0) { st.currentChain = i; refresh(); if (st.changed) st.changed(); } };
        setupButton (*this, newChain, "+ New chain");
        newChain.onClick = [this] { st.applySong ("New chain", [] (Song& s) { s.chains.push_back ({ "Chain " + String ((int) s.chains.size() + 1), {}, -1, true }); }); st.currentChain = (int) st.song().chains.size() - 1; refresh(); };
        setupButton (*this, renameChain, "Rename");
        renameChain.onClick = [this] {
            auto* aw = new AlertWindow ("Rename chain", "", MessageBoxIconType::NoIcon);
            aw->addTextEditor ("n", st.song().chains[(size_t) current].name);
            aw->addButton ("OK", 1, KeyPress (KeyPress::returnKey)); aw->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
            aw->enterModalState (true, ModalCallbackFunction::create ([this, aw] (int r) { if (r == 1) { const auto t = aw->getTextEditorContents ("n"); edit ([t] (Chain& c) { c.name = t; }); } }), true);
        };
        setupButton (*this, delChain, "Delete chain");
        delChain.onClick = [this] {
            const int c = current;
            st.applySong ("Delete chain", [c] (Song& s) { if (c < (int) s.chains.size()) s.chains.erase (s.chains.begin() + c); if (s.chains.empty()) s.chains.push_back ({ "Chain 1", {}, -1, true }); });
            st.currentChain = jmax (0, c - 1); refresh();
        };
        setupButton (*this, add, "+ Add the edited key"); add.onClick = [this] {
            const int k = st.song().editKey;
            edit ([k] (Chain& c) { c.steps.push_back ({ k, 1 }); });
        };
        setupButton (*this, clear, "Clear"); clear.onClick = [this] { edit ([] (Chain& c) { c.steps.clear(); }); };
        setupButton (*this, loop, "Loop", true); loop.onClick = [this] { const bool on = loop.getToggleState(); edit ([on] (Chain& c) { c.loop = on; }); };
        setupButton (*this, play, "Play chain"); play.onClick = [this] { st.proc.uiPlayChain (current); };
        setupButton (*this, stop, "Stop"); stop.onClick = [this] { st.proc.uiStop(); };
        setupLabel (*this, lTrig, "Start from key");
        StringArray keys { "None (play button only)" }; for (int n = 0; n < 128; ++n) keys.add (keyName (n) + "  (" + String (n) + ")");
        setupCombo (*this, trig, keys);
        trig.onChange = [this] { const int n = trig.getSelectedItemIndex() - 1; edit ([n] (Chain& c) { c.triggerNote = n; }); };
        addAndMakeVisible (view); view.setViewedComponent (&list, false); view.setScrollBarsShown (true, false);
        setupLabel (*this, help, "Each step plays a key's pattern the set number of times, then moves on. A key used as a start key plays the chain instead of its pattern.", 10.5f, th::faint);
        help.setJustificationType (Justification::topLeft);
        dragOut.setButtonText ("Drag chain MIDI"); dragOut.setSize (fitW ("Drag chain MIDI"), btnH); addAndMakeVisible (dragOut);
        dragOut.getParts = [this] {
            std::vector<const Pattern*> v;
            for (auto& s : st.song().chains[(size_t) current].steps)
                if (s.key >= 0) for (int r = 0; r < s.repeats; ++r) v.push_back (&st.song().keys[(size_t) s.key]);
            return v;
        };
        dragOut.getChannel = [this] { return st.song().settings.outChannel; };
    }
    void refresh() override
    {
        auto& chains = st.song().chains;
        current = jlimit (0, (int) chains.size() - 1, st.currentChain);
        chainSel.clear (dontSendNotification);
        for (int i = 0; i < (int) chains.size(); ++i) chainSel.addItem (chains[(size_t) i].name.isNotEmpty() ? chains[(size_t) i].name : "Chain " + String (i + 1), i + 1);
        chainSel.setSelectedItemIndex (current, dontSendNotification);
        auto& c = chains[(size_t) current];
        loop.setToggleState (c.loop, dontSendNotification);
        trig.setSelectedItemIndex (c.triggerNote + 1, dontSendNotification);
        rows.clear();
        const bool playingThis = st.proc.chainPlaying.load() == current;
        for (int i = 0; i < (int) c.steps.size(); ++i) {
            auto* r = rows.add (new StepRow (*this, i, playingThis && st.proc.chainStep.load() == i));
            list.addAndMakeVisible (r);
        }
        resized();
    }
    void resized() override
    {
        auto a = content();
        auto r = a.removeFromTop (28);
        chainSel.setBounds (r.removeFromLeft (150).reduced (0, 3)); r.removeFromLeft (6);
        for (auto* b : { &newChain, &renameChain, &delChain }) { b->setBounds (r.removeFromLeft (b->getWidth()).reduced (0, 3)); r.removeFromLeft (5); }
        a.removeFromTop (4);
        r = a.removeFromTop (28);
        for (Component* b : { (Component*) &add, (Component*) &clear, (Component*) &loop, (Component*) &play, (Component*) &stop, (Component*) &dragOut }) { b->setBounds (r.removeFromLeft (b->getWidth()).reduced (0, 3)); r.removeFromLeft (5); }
        a.removeFromTop (4);
        trig.setBounds (row (a, lTrig).removeFromLeft (180));
        help.setBounds (a.removeFromBottom (34));
        view.setBounds (a);
        list.setSize (a.getWidth() - 10, jmax (a.getHeight(), (int) rows.size() * 30));
        for (int i = 0; i < rows.size(); ++i) rows[i]->setBounds (0, i * 30, list.getWidth(), 28);
    }
    int current = 0;
private:
    struct StepRow : public Component
    {
        StepRow (ChainPage& o, int i, bool active) : owner (o), idx (i), on (active)
        {
            auto& s = owner.st.song();
            for (int k = 0; k < 128; ++k) if (! s.keys[(size_t) k].isEmpty())
                key.addItem (keyName (k) + "  " + s.keys[(size_t) k].name, k + 1);
            const auto& step = s.chains[(size_t) owner.current].steps[(size_t) i];
            key.setSelectedId (step.key + 1, dontSendNotification);
            key.onChange = [this] { const int k = key.getSelectedId() - 1; const int i2 = idx; owner.edit ([k, i2] (Chain& c) { c.steps[(size_t) i2].key = k; }); };
            addAndMakeVisible (key);
            for (auto* b : { &minus, &plus, &up, &del }) { b->setWantsKeyboardFocus (false); addAndMakeVisible (b); }
            minus.setButtonText ("-"); plus.setButtonText ("+"); up.setButtonText ("Up"); del.setButtonText ("Remove");
            minus.onClick = [this] { const int i2 = idx; owner.edit ([i2] (Chain& c) { auto& r = c.steps[(size_t) i2].repeats; r = jmax (1, r - 1); }); };
            plus.onClick  = [this] { const int i2 = idx; owner.edit ([i2] (Chain& c) { auto& r = c.steps[(size_t) i2].repeats; r = jmin (99, r + 1); }); };
            up.onClick    = [this] { const int i2 = idx; if (i2 > 0) owner.edit ([i2] (Chain& c) { std::swap (c.steps[(size_t) i2], c.steps[(size_t) i2 - 1]); }); };
            del.onClick   = [this] { const int i2 = idx; owner.edit ([i2] (Chain& c) { c.steps.erase (c.steps.begin() + i2); }); };
            reps = step.repeats;
        }
        void paint (Graphics& g) override
        {
            g.setColour (on ? th::amber.withAlpha (0.18f) : th::surface.withAlpha (0.5f)); g.fillRoundedRectangle (getLocalBounds().toFloat(), 5);
            g.setFont (font (12.0f, 3)); g.setColour (on ? th::amber : th::dim);
            g.drawText (String (idx + 1), 6, 0, 22, getHeight(), Justification::centredLeft);
            g.setColour (th::text); g.setFont (font (12.0f, 2));
            g.drawText ("x " + String (reps), repsArea, Justification::centred);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (4, 3);
            r.removeFromLeft (24);
            del.setBounds (r.removeFromRight (64)); r.removeFromRight (4);
            up.setBounds (r.removeFromRight (34)); r.removeFromRight (8);
            plus.setBounds (r.removeFromRight (24));
            repsArea = r.removeFromRight (40);
            minus.setBounds (r.removeFromRight (24)); r.removeFromRight (8);
            key.setBounds (r);
        }
        ChainPage& owner; int idx; bool on; int reps = 1; Rectangle<int> repsArea;
        ComboBox key; TextButton minus, plus, up, del;
    };
    void edit (std::function<void (Chain&)> f)
    {
        const int c = current;
        st.applySong ("Edit chain", [c, f] (Song& s) { f (s.chains[(size_t) c]); });
        refresh();
    }
    EditState& st;
    ComboBox chainSel; TextButton newChain, renameChain, delChain;
    TextButton add, clear, loop, play, stop;
    DragMidiButton dragOut;
    Label lTrig, help; ComboBox trig;
    Viewport view; Component list; OwnedArray<StepRow> rows;
};


// ---------------------------------------------------------------- Look editor (a tab of Settings)
class LookEditor : public Component, private ListBoxModel
{
public:
    std::function<StringArray()> collectIds;      // buttons and texts found on screen
    std::function<void()> onChanged;

    LookEditor()
    {
        addAndMakeVisible (list); list.setModel (this); list.setRowHeight (22);
        list.setColour (ListBox::backgroundColourId, Colours::black.withAlpha (0.2f));
        setupLabel (*this, name, "", 13.0f, th::amber);
        setupLabel (*this, lAll, "All text", 11.5f, th::amber);
        setupSlider (*this, allText, th::amber); allText.setRange (70, 160, 1); allText.setTextValueSuffix (" %");
        allText.setValue (Theme::get().textScale * 100.0, dontSendNotification);
        allText.setTooltip ("Makes every text in Drum Player bigger or smaller at once; each part's own size still applies on top");
        allText.onValueChange = [this] { if (loading) return; Theme::get().textScale = (float) allText.getValue() / 100.0f; Theme::get().changed(); if (onChanged) onChanged(); };
        setupLabel (*this, lAllB, "All buttons", 11.5f, th::amber);
        setupSlider (*this, allButtons, th::amber); allButtons.setRange (80, 160, 1); allButtons.setTextValueSuffix (" %");
        allButtons.setValue (Theme::get().buttonScale * 100.0, dontSendNotification);
        allButtons.setTooltip ("Makes every button bigger or smaller at once");
        allButtons.onValueChange = [this] { if (loading) return; Theme::get().buttonScale = (float) allButtons.getValue() / 100.0f; Theme::get().changed(); if (onChanged) onChanged(); };
        setupLabel (*this, lKeys, "Keys", 11.5f, th::amber);
        setupSlider (*this, keySize, th::amber); keySize.setRange (70, 250, 1); keySize.setTextValueSuffix (" %");
        keySize.setTooltip ("Bigger keys on the keyboard strip (fewer shown at a time; scroll with the arrows)");
        keySize.onValueChange = [this] { if (loading) return; Theme::get().keyScale = (float) keySize.getValue() / 100.0f; Theme::get().changed(); if (onChanged) onChanged(); };
        setupLabel (*this, lKeyText, "Key text", 11.5f, th::amber);
        setupSlider (*this, keyText, th::amber); keyText.setRange (70, 250, 1); keyText.setTextValueSuffix (" %");
        keyText.setTooltip ("Text on the keys, on its own");
        keyText.onValueChange = [this] { if (loading) return; Theme::get().keyTextScale = (float) keyText.getValue() / 100.0f; Theme::get().changed(); if (onChanged) onChanged(); };
        setupButton (*this, useMine, "Use my colours", true);
        useMine.onClick = [this] { auto& e = el(); if (useMine.getToggleState() && ! e.custom) seed (e); e.custom = useMine.getToggleState(); changed(); };
        for (int i = 0; i < 3; ++i) {
            auto* sw = swatches.add (new Swatch (i == 0 ? "Top" : i == 1 ? "Middle" : "Bottom"));
            addAndMakeVisible (sw);
            sw->onClick = [this, i, sw] { pick (i, sw); };
            sw->onDrop = [this, i] (Colour c) { setColour (i, c); };
            sw->onMenu = [this, i, sw] {
                PopupMenu m; m.addItem (1, "Copy this colour"); if (hasCopied) m.addItem (2, "Paste colour");
                m.addItem (3, "Copy to all three boxes");
                m.showMenuAsync (PopupMenu::Options().withTargetComponent (sw), [this, i, sw] (int r) {
                    if (r == 1) { copied = sw->colour; hasCopied = true; }
                    else if (r == 2) setColour (i, copied);
                    else if (r == 3) for (int k = 0; k < 3; ++k) setColour (k, sw->colour);
                });
            };
        }
        {   auto* sw = swatches.add (new Swatch ("Text"));
            addAndMakeVisible (sw);
            sw->onClick = [this, sw] {
                auto& e = el(); if (! e.ownText) { e.ownText = true; changed(); }
                auto p = std::make_unique<Picker>(); p->setCurrentColour (e.text, dontSendNotification);
                p->onColour = [this] (Colour c) { auto& e2 = el(); e2.text = c; e2.ownText = true; changed(); };
                auto* top = sw->getTopLevelComponent();
                CallOutBox::launchAsynchronously (std::move (p), top->getLocalArea (sw, sw->getLocalBounds()), top);
            };
            sw->onDrop = [this] (Colour c) { auto& e = el(); e.text = c; e.ownText = true; changed(); };
            sw->onMenu = [this, sw] {
                PopupMenu m; m.addItem (1, "Copy this colour"); if (hasCopied) m.addItem (2, "Paste colour"); m.addItem (3, "Text colour as designed");
                m.showMenuAsync (PopupMenu::Options().withTargetComponent (sw), [this, sw] (int r) {
                    auto& e = el();
                    if (r == 1) { copied = sw->colour; hasCopied = true; }
                    else if (r == 2) { e.text = copied; e.ownText = true; changed(); }
                    else if (r == 3) { e.ownText = false; changed(); }
                });
            };
        }
        setupButton (*this, boldBtn, "Bold: as designed", false, "Click: as designed, bold, plain");
        boldBtn.onClick = [this] { auto& e = el(); e.bold = e.bold == -1 ? 1 : e.bold == 1 ? 0 : -1; changed(); };
        setupButton (*this, d3, "3D", true);
        d3.onClick = [this] { el().threeD = d3.getToggleState(); changed(); };
        setupLabel (*this, lDepth, "Depth"); setupSlider (*this, depth, th::amber); depth.setRange (0.0, 2.0, 0.05);
        depth.onValueChange = [this] { if (! loading) { el().depth = (float) depth.getValue(); changed(); } };
        setupLabel (*this, lFont, "Text size"); setupSlider (*this, fontSz, th::blue); fontSz.setRange (0.0, 28.0, 0.5);
        fontSz.setTextValueSuffix (" pt"); fontSz.setTooltip ("0 = as designed");
        fontSz.onValueChange = [this] { if (! loading) { el().fontSize = (float) fontSz.getValue(); changed(); } };
        setupButton (*this, resetOne, "Reset this"); resetOne.onClick = [this] { Theme::get().reset (currentId()); show(); if (onChanged) onChanged(); };
        setupButton (*this, resetAll, "Reset every look"); resetAll.onClick = [this] {
            AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "Reset every look", "Go back to the designed look everywhere?", "Reset", "Cancel", this,
                ModalCallbackFunction::create ([this] (int r) { if (r == 1) { Theme::get().resetAll(); show(); if (onChanged) onChanged(); } }));
        };
        setupButton (*this, saveLook, "Save look as..."); saveLook.onClick = [this] {
            auto* aw = new AlertWindow ("Save look", "A name for this look:", MessageBoxIconType::NoIcon);
            aw->addTextEditor ("n", "My look");
            aw->addButton ("Save", 1, KeyPress (KeyPress::returnKey)); aw->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
            aw->enterModalState (true, ModalCallbackFunction::create ([aw] (int r) { if (r == 1) Theme::get().saveAs (aw->getTextEditorContents ("n").trim()); }), true);
        };
        setupButton (*this, loadLook, "Looks..."); loadLook.onClick = [this] {
            PopupMenu m;
            m.addItem (1, "Original look (as designed)");
            auto files = Theme::get().looksFolder().findChildFiles (File::findFiles, false, "*.xml"); files.sort();
            if (! files.isEmpty()) m.addSeparator();
            for (int i = 0; i < files.size(); ++i) m.addItem (100 + i, files[i].getFileNameWithoutExtension());
            if (! files.isEmpty()) { PopupMenu del; for (int i = 0; i < files.size(); ++i) del.addItem (1000 + i, files[i].getFileNameWithoutExtension()); m.addSubMenu ("Delete a saved look", del); }
            m.showMenuAsync (PopupMenu::Options().withTargetComponent (&loadLook), [this, files] (int r) {
                if (r == 1) Theme::get().resetAll();
                else if (r >= 100 && r < 1000) Theme::get().loadFrom (files[r - 100]);
                else if (r >= 1000) files[r - 1000].deleteFile();
                else return;
                show(); list.repaint(); if (onChanged) onChanged();
            });
        };
        setupLabel (*this, help, "Pick a part on the left. Its own settings win over \"Buttons\" and \"Labels\", which style all of them at once. Your changes are kept automatically; save a look to come back to it.", 10.5f, th::faint);
        help.setJustificationType (Justification::topLeft);
    }
    void refreshList()
    {
        items.clear(); ids.clear();
        for (auto& p : themePanels()) { items.add (p); ids.add (p); }
        items.add ("Text: Title (DRUM PLAYER)"); ids.add ("txt:Title");
        if (collectIds)
            for (auto& id : collectIds()) {
                if (id.startsWith ("btn:")) { items.add ("Button: " + id.substring (4)); ids.add (id); }
                else if (id.startsWith ("txt:")) { items.add ("Text: " + id.substring (4)); ids.add (id); }
            }
        list.updateContent();
        if (list.getSelectedRow() < 0) list.selectRow (0);
        show();
    }
    void resized() override
    {
        auto a = getLocalBounds();
        { auto r = a.removeFromTop (26); lAll.setBounds (r.removeFromLeft (80)); allText.setBounds (r.removeFromLeft (240)); a.removeFromTop (4); }
        { auto r = a.removeFromTop (26); lAllB.setBounds (r.removeFromLeft (80)); allButtons.setBounds (r.removeFromLeft (240)); a.removeFromTop (4); }
        { auto r = a.removeFromTop (26); lKeys.setBounds (r.removeFromLeft (80)); keySize.setBounds (r.removeFromLeft (240)); a.removeFromTop (4); }
        { auto r = a.removeFromTop (26); lKeyText.setBounds (r.removeFromLeft (80)); keyText.setBounds (r.removeFromLeft (240)); a.removeFromTop (6); }
        list.setBounds (a.removeFromLeft (200)); a.removeFromLeft (12);
        name.setBounds (a.removeFromTop (24)); a.removeFromTop (2);
        useMine.setBounds (a.removeFromTop (26).removeFromLeft (useMine.getWidth()).reduced (0, 2)); a.removeFromTop (6);
        auto sw = a.removeFromTop (54);
        for (auto* s2 : swatches) { s2->setBounds (sw.removeFromLeft (62)); sw.removeFromLeft (8); }
        a.removeFromTop (8);
        auto r = a.removeFromTop (26); d3.setBounds (r.removeFromLeft (d3.getWidth()).reduced (0, 2)); r.removeFromLeft (10);
        lDepth.setBounds (r.removeFromLeft (44)); depth.setBounds (r);
        a.removeFromTop (6);
        r = a.removeFromTop (26); lFont.setBounds (r.removeFromLeft (62)); boldBtn.setBounds (r.removeFromRight (130).reduced (0, 2)); r.removeFromRight (8); fontSz.setBounds (r);
        a.removeFromTop (10);
        r = a.removeFromTop (26); resetOne.setBounds (r.removeFromLeft (resetOne.getWidth()).reduced (0, 2)); r.removeFromLeft (8); resetAll.setBounds (r.removeFromLeft (resetAll.getWidth()).reduced (0, 2));
        a.removeFromTop (6);
        r = a.removeFromTop (26); saveLook.setBounds (r.removeFromLeft (saveLook.getWidth()).reduced (0, 2)); r.removeFromLeft (8); loadLook.setBounds (r.removeFromLeft (loadLook.getWidth()).reduced (0, 2));
        a.removeFromTop (8); help.setBounds (a.removeFromTop (58));
    }
private:
    // a colour box: click to pick, drag it onto another box to copy the colour, right-click to copy and paste
    struct Swatch : public Component, public DragAndDropTarget, public SettableTooltipClient
    {
        explicit Swatch (String t) : title (std::move (t)) { setTooltip ("Click: pick a colour. Drag onto another box: copy this colour there. Right-click: copy / paste"); }
        std::function<void()> onClick; std::function<void (Colour)> onDrop; std::function<void()> onMenu;
        Colour colour; bool enabled = true; String title; bool hover = false, dragged = false;
        void mouseDown (const MouseEvent& e) override { dragged = false; if (e.mods.isPopupMenu() && onMenu) onMenu(); }
        void mouseDrag (const MouseEvent& e) override
        {
            if (dragged || e.mods.isPopupMenu() || e.getDistanceFromDragStart() < 5) return;
            if (auto* c = DragAndDropContainer::findParentDragContainerFor (this)) {
                dragged = true;
                Image img (Image::ARGB, 28, 28, true); { Graphics g (img); g.setColour (colour); g.fillRoundedRectangle (1, 1, 26, 26, 5); g.setColour (Colours::white); g.drawRoundedRectangle (1, 1, 26, 26, 5, 1.5f); }
                c->startDragging ("colour:" + colour.toString(), this, ScaledImage (img));
            }
        }
        bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("colour:") && d.sourceComponent.get() != this; }
        void itemDragEnter (const SourceDetails&) override { hover = true; repaint(); }
        void itemDragExit (const SourceDetails&) override { hover = false; repaint(); }
        void itemDropped (const SourceDetails& d) override
        {
            hover = false;
            if (onDrop) onDrop (Colour::fromString (d.description.toString().fromFirstOccurrenceOf ("colour:", false, false)));
        }
        void paint (Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (1).withTrimmedBottom (16);
            g.setColour (colour.withMultipliedAlpha (enabled ? 1.0f : 0.3f)); g.fillRoundedRectangle (r, 5);
            g.setColour (hover ? th::green : th::line); g.drawRoundedRectangle (r, 5, hover ? 2.5f : 1.0f);
            g.setFont (font (10.5f, 2)); g.setColour (th::dim);
            g.drawText (title, getLocalBounds().removeFromBottom (14), Justification::centred);
        }
        void mouseUp (const MouseEvent& e) override { if (! dragged && ! e.mods.isPopupMenu() && onClick) onClick(); dragged = false; }
    };
    struct Picker : public ColourSelector, private ChangeListener
    {
        std::function<void (Colour)> onColour;
        Picker() : ColourSelector (ColourSelector::showColourAtTop | ColourSelector::showSliders | ColourSelector::showColourspace | ColourSelector::showAlphaChannel)
        { setSize (300, 320); addChangeListener (this); }
        void changeListenerCallback (ChangeBroadcaster*) override { if (onColour) onColour (getCurrentColour()); }
    };

    ListBox list; Label name, lDepth, lFont, help;
    TextButton useMine, d3, resetOne, resetAll, saveLook, loadLook, boldBtn;
    Label lAll, lAllB, lKeys, lKeyText; FineSlider allText, allButtons, keySize, keyText;
    OwnedArray<Swatch> swatches;
    FineSlider depth, fontSz;
    StringArray items, ids;
    bool loading = false;

    String currentId() const { const int r = list.getSelectedRow(); return (r >= 0 && r < ids.size()) ? ids[r] : String ("Buttons"); }
    ThemeEl& el() { return Theme::get().edit (currentId()); }
    void changed() { Theme::get().changed(); show(); if (onChanged) onChanged(); }

    // first time a part gets its own colours, start from what it looks like now
    void seed (ThemeEl& e)
    {
        const auto id = currentId();
        Colour base = th::panel;
        if (id == "Window background" || id == "Note grid" || id == "Keyboard strip") base = th::bg;
        if (id == "Buttons" || id.startsWith ("btn:") || id == "Boxes and menus") base = th::surface;
        if (id == "Messages") base = th::raised;
        if (id == "Lit buttons") { e.c0 = th::amber.brighter (0.15f); e.c1 = th::amber; e.c2 = th::amber.darker (0.25f); return; }
        e.c0 = base.brighter (0.25f); e.c1 = base; e.c2 = base.darker (0.2f);
    }
    void show()
    {
        loading = true;
        const auto* e = Theme::get().find (currentId());
        ThemeEl d; if (e == nullptr) e = &d;
        const int r = list.getSelectedRow();
        name.setText (r >= 0 && r < items.size() ? items[r] : String(), dontSendNotification);
        useMine.setToggleState (e->custom, dontSendNotification);
        const Colour c[3] = { e->c0, e->c1, e->c2 };
        for (int i = 0; i < 3; ++i) { swatches[i]->colour = c[i]; swatches[i]->enabled = e->custom; swatches[i]->repaint(); }
        swatches[3]->colour = e->ownText ? e->text : th::text; swatches[3]->enabled = e->ownText; swatches[3]->repaint();
        boldBtn.setButtonText (e->bold == 1 ? "Bold: on" : e->bold == 0 ? "Bold: off" : "Bold: as designed");
        d3.setToggleState (e->threeD, dontSendNotification);
        keySize.setValue (Theme::get().keyScale * 100.0, dontSendNotification); keyText.setValue (Theme::get().keyTextScale * 100.0, dontSendNotification);
        allText.setValue (Theme::get().textScale * 100.0, dontSendNotification);
        depth.setValue (e->depth, dontSendNotification); fontSz.setValue (e->fontSize, dontSendNotification);
        loading = false;
    }
    Colour copied; bool hasCopied = false;
    void setColour (int i, Colour c)
    {
        auto& e = el();
        if (! e.custom) { seed (e); e.custom = true; }
        (i == 0 ? e.c0 : i == 1 ? e.c1 : e.c2) = c;
        changed();
    }
    void pick (int i, Swatch* sw)
    {
        // clicking a colour switches this part to your own colours
        auto& e = el();
        if (! e.custom) { seed (e); e.custom = true; changed(); }
        sw->colour = i == 0 ? e.c0 : i == 1 ? e.c1 : e.c2;
        auto p = std::make_unique<Picker>();
        p->setCurrentColour (sw->colour, dontSendNotification);
        p->onColour = [this, i] (Colour c) { auto& e = el(); (i == 0 ? e.c0 : i == 1 ? e.c1 : e.c2) = c; changed(); };
        // open inside the plugin window: a free-floating picker can end up hidden behind Studio One
        auto* top = sw->getTopLevelComponent();
        CallOutBox::launchAsynchronously (std::move (p), top->getLocalArea (sw, sw->getLocalBounds()), top);
    }
    int getNumRows() override { return items.size(); }
    void paintListBoxItem (int row, Graphics& g, int w, int h, bool sel) override
    {
        if (row >= items.size()) return;
        if (sel) { g.setColour (th::blue.withAlpha (0.22f)); g.fillRoundedRectangle (2, 1, w - 4.0f, h - 2.0f, 4); }
        const auto* fe = Theme::get().find (ids[row]);
        const bool custom = Theme::get().findCustom (ids[row]) != nullptr || (fe != nullptr && (fe->fontSize > 0 || fe->bold >= 0 || fe->ownText));
        g.setFont (font (11.5f, custom ? 3 : 1)); g.setColour (custom ? th::amber : th::text);
        g.drawText (items[row], 8, 0, w - 12, h, Justification::centredLeft, true);
    }
    void selectedRowsChanged (int) override { show(); }
};


// ---------------------------------------------------------------- Settings > Inner workings: how the drumming is shaped
class InnerPage : public Page
{
public:
    InnerPage (EditState& e) : Page ("INNER WORKINGS"), st (e)
    {
        embedded = true;
        auto& ap = st.proc.apvts;
        auto make = [&] (Label& l, FineSlider& s2, const char* id, const String& text, const String& suffix, const String& tip) {
            setupLabel (*this, l, text); setupSlider (*this, s2); s2.setTextValueSuffix (suffix); s2.setTooltip (tip);
            att.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (ap, id, s2));
        };
        setupLabel (*this, hFeel, "FEEL (like a drummer)", 11.0f, th::amber);
        make (lDT, driftT, ids::driftTime, "Drift timing", " ms", "Timing leans ahead and lays back slowly over a few bars, instead of jumping on every note");
        make (lDV, driftV, ids::driftVel, "Drift velocity", "", "Playing swells and settles slowly");
        make (lDS, driftS, ids::driftBars, "Drift speed", " bars", "How many bars one lean takes");
        make (lPh, phrase, ids::phrase, "Phrase dynamics", " %", "Builds into the one, lifts fills, eases the middle of the pattern");
        make (lHT, humT, ids::humTime, "Random timing", " ms", "Small random timing on every note");
        make (lHV, humV, ids::humVel, "Random velocity", "", "Small random velocity on every note");
        setupLabel (*this, hVint, "VINTAGE MACHINES (Akai MPC, Linn era)", 11.0f, th::amber);
        setupLabel (*this, lVc, "Clock");
        setupCombo (*this, vint, { "Off", "96 per beat (MPC era)", "48 per beat (LM-1 era)", "24 per beat (early sync)" });
        vintAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (ap, ids::vintRes, vint);
        make (lW, wob, ids::vintJitter, "Clock wobble", " ms", "How much the old clock wanders");
        setupButton (*this, stag, "MIDI cable stagger", true, "Notes that land together come out about 1 ms apart, like a 5-pin MIDI cable");
        stagAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (ap, ids::stagger, stag);
        setupLabel (*this, hHH, "HI-HAT", 11.0f, th::amber);
        setupLabel (*this, lHH, "Openness by controller");
        setupCombo (*this, hh, { "Off", "CC1 mod wheel (Groove Agent)", "CC4 foot controller", "CC11 expression", "CC2 breath" }, 1,
                    "Before each hi-hat note, Drum Player sends how open it is, so plugins that follow a controller open the hat smoothly");
        hh.onChange = [this] { static const int cc[] = { 0, 1, 4, 11, 2 }; const int v = cc[jlimit (0, 4, hh.getSelectedItemIndex())]; st.song().settings.hhCC = v; st.proc.publish(); };
        setupLabel (*this, help, "Everything here is live: the notes in your patterns don't change. Shift-drag sliders for fine steps.", 10.5f, th::faint);
    }
    void refresh() override
    {
        const int v = st.song().settings.hhCC;
        hh.setSelectedItemIndex (v == 1 ? 1 : v == 4 ? 2 : v == 11 ? 3 : v == 2 ? 4 : 0, dontSendNotification);
    }
    void resized() override
    {
        auto a = content(); const int lw = 150;
        hFeel.setBounds (a.removeFromTop (20));
        driftT.setBounds (row (a, lDT, lw)); driftV.setBounds (row (a, lDV, lw)); driftS.setBounds (row (a, lDS, lw));
        phrase.setBounds (row (a, lPh, lw)); humT.setBounds (row (a, lHT, lw)); humV.setBounds (row (a, lHV, lw));
        a.removeFromTop (4); hVint.setBounds (a.removeFromTop (20));
        auto r = row (a, lVc, lw); vint.setBounds (r.removeFromLeft (190)); r.removeFromLeft (8); stag.setBounds (r.removeFromLeft (stag.getWidth()).reduced (0, 3));
        wob.setBounds (row (a, lW, lw));
        a.removeFromTop (4); hHH.setBounds (a.removeFromTop (20));
        hh.setBounds (row (a, lHH, lw).removeFromLeft (230));
        help.setBounds (a.removeFromTop (34));
    }
private:
    EditState& st;
    Label hFeel, lDT, lDV, lDS, lPh, lHT, lHV, hVint, lVc, lW, hHH, lHH, help;
    FineSlider driftT, driftV, driftS, phrase, humT, humV, wob;
    ComboBox vint, hh; TextButton stag;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> vintAtt;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> stagAtt;
    std::vector<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>> att;
};

// ---------------------------------------------------------------- drum map page
class MapPage : public Page
{
public:
    MapPage (EditState& e) : Page ("DRUM MAP"), st (e)
    {
        StringArray names; for (auto& m : builtInMaps()) names.add (m.name);
        setupLabel (*this, lPlay, "Playing on");
        setupCombo (*this, play, names, 1, "The drum plugin Drum Player feeds. Changing it converts every pattern (and changing back restores them)");
        play.onChange = [this] {
            const int to = play.getSelectedItemIndex();
            if (loading || to < 0 || to == st.song().playMap) return;
            st.applySong ("Drum map", [to] (Song& s) { convertSong (s, s.playMap, to); });
            if (st.toast) st.toast ("All patterns now play on " + builtInMaps()[(size_t) to].name + ", MIDI channel " + String (st.song().settings.outChannel));
            refresh();
        };
        StringArray from { "Same as above" }; from.addArray (names);
        setupLabel (*this, lFrom, "Dropped MIDI is from");
        setupCombo (*this, importFrom, from, 1, "Which plugin made the MIDI you drop in. It is converted on the way in");
        importFrom.onChange = [this] { if (loading) return; const int v = importFrom.getSelectedItemIndex() - 1; st.applySong ("Import map", [v] (Song& s) { s.importMap = v; }); refresh(); };
        StringArray chs; for (int c = 1; c <= 16; ++c) chs.add ("Channel " + String (c));
        setupLabel (*this, lOut, "MIDI out"); setupCombo (*this, outCh, chs, 1, "The channel Drum Player sends on. It must match what the drum plugin's track lets through");
        outCh.onChange = [this] { if (loading) return; const int c = outCh.getSelectedItemIndex() + 1; st.applySong ("MIDI out channel", [c] (Song& s) { s.settings.outChannel = c; }); refresh(); };
        setupButton (*this, convertBtn, "Convert the edited pattern from...", false, "For a pattern that came from another plugin and sounds wrong");
        convertBtn.onClick = [this] {
            PopupMenu m;
            for (int i = 0; i < (int) numMaps; ++i) if (i != st.song().playMap) m.addItem (i + 1, builtInMaps()[(size_t) i].name);
            m.showMenuAsync (PopupMenu::Options().withTargetComponent (&convertBtn), [this] (int r) {
                if (r <= 0) return;
                const int from = r - 1;
                st.applySong ("Convert pattern", [from] (Song& s) { convertPattern (s.keys[(size_t) s.editKey], s.map (from), s.map (s.playMap)); s.ensureLanesForPattern (s.keys[(size_t) s.editKey]); });
            });
        };
        setupLabel (*this, info, "", 11.0f, th::dim); info.setJustificationType (Justification::topLeft);
        setupLabel (*this, lCustom, "Custom map: the note for each sound", 11.5f, th::amber);
        addAndMakeVisible (view); view.setViewedComponent (&list, false); view.setScrollBarsShown (true, false);
        StringArray notes { "None" }; for (int n = 0; n < 128; ++n) notes.add (String (n) + "  " + keyName (n));
        for (int a = 0; a < aCount; ++a) {
            auto* l = labels.add (new Label()); setupLabel (list, *l, artName (a), 11.5f, th::text);
            auto* c = combos.add (new ComboBox()); setupCombo (list, *c, notes);
            c->onChange = [this, a, c] {
                if (loading) return;
                const int n = c->getSelectedItemIndex() - 1;
                st.applySong ("Custom map", [a, n] (Song& s) {
                    const int old = s.customNotes[(size_t) a];
                    s.customNotes[(size_t) a] = n;
                    if (s.playMap == mapCustom && old >= 0 && n >= 0)        // move existing notes of that sound along
                        for (auto& p : s.keys) for (auto& x : p.notes) if (x.note == old) x.note = n;
                    if (s.playMap == mapCustom && n >= 0) { s.ensureLane (n); if (auto* l2 = s.lane (n)) l2->name = artName (a); }
                });
            };
        }
    }
    void refresh() override
    {
        loading = true;
        auto& s = st.song();
        play.setSelectedItemIndex (s.playMap, dontSendNotification);
        importFrom.setSelectedItemIndex (s.importMap + 1, dontSendNotification);
        outCh.setSelectedItemIndex (s.settings.outChannel - 1, dontSendNotification);
        const auto& m = builtInMaps()[(size_t) s.playMap];
        info.setText (m.note + "\nOutput: MIDI channel " + String (s.settings.outChannel) + ".  Notes the map doesn't know pass through unchanged.", dontSendNotification);
        for (int a = 0; a < aCount; ++a) combos[a]->setSelectedItemIndex (s.customNotes[(size_t) a] + 1, dontSendNotification);
        const bool custom = s.playMap == mapCustom || s.importMap == mapCustom;
        view.setVisible (custom); lCustom.setVisible (custom);
        loading = false;
        resized();
    }
    void resized() override
    {
        auto a = content();
        play.setBounds (row (a, lPlay, 150).removeFromLeft (300));
        importFrom.setBounds (row (a, lFrom, 150).removeFromLeft (300));
        outCh.setBounds (row (a, lOut, 150).removeFromLeft (130));
        convertBtn.setBounds (a.removeFromTop (28).removeFromLeft (convertBtn.getWidth()).reduced (0, 3)); a.removeFromTop (4);
        info.setBounds (a.removeFromTop (46)); a.removeFromTop (4);
        lCustom.setBounds (a.removeFromTop (22));
        view.setBounds (a);
        list.setSize (jmax (100, a.getWidth() - 12), aCount * 26);
        for (int i = 0; i < aCount; ++i) { labels[i]->setBounds (0, i * 26, 170, 24); combos[i]->setBounds (176, i * 26 + 1, 150, 22); }
    }
private:
    EditState& st; bool loading = false;
    Label lPlay, lFrom, info, lCustom, lOut;
    ComboBox play, importFrom, outCh;
    TextButton convertBtn;
    Viewport view; Component list;
    OwnedArray<Label> labels; OwnedArray<ComboBox> combos;
};


// ---------------------------------------------------------------- settings page
class SettingsPage : public Page, private Timer
{
public:
    SettingsPage (EditState& e) : Page ("SETTINGS"), st (e), map (e), inner (e)
    {
        map.embedded = true; addChildComponent (map); addChildComponent (inner);
        StringArray ch; for (int c = 1; c <= 16; ++c) ch.add ("Channel " + String (c) + (c == 10 ? "  (GM drums)" : ""));
        setupLabel (*this, lOut, "MIDI out"); setupCombo (*this, out, ch);
        StringArray keys; for (int n = 0; n < 128; ++n) keys.add (keyName (n) + "  (" + String (n) + ")");
        setupLabel (*this, lZone, "Pattern keys"); setupCombo (*this, zLo, keys); setupCombo (*this, zHi, keys);
        setupLabel (*this, lStart, "Start / switch on"); setupCombo (*this, start, { "Immediately", "Next beat", "Next bar", "End of pattern", "At once, in time (keeps the bar)" });
        setupLabel (*this, lMode, "Keys"); setupCombo (*this, mode, { "Latch (keeps playing until STOP)", "Hold (release to stop)" });
        StringArray stopKeys { "None" }; for (int n = 0; n < 128; ++n) stopKeys.add (keyName (n));
        StringArray chans { "Any channel" }; for (int c = 1; c <= 16; ++c) chans.add ("Channel " + String (c));
        setupLabel (*this, lTrig, "Pattern keys listen on"); setupCombo (*this, trigCh, chans, 1, "Give a second controller its own channel to use it only for patterns");
        setupLabel (*this, lPlayCh, "Drum pads / live notes on"); setupCombo (*this, playCh, chans, 1, "Notes on this channel are always played and recorded, never pattern keys");
        setupButton (*this, mirror, "Pattern keys in every octave", true, "The pattern keys repeat up the whole keyboard, so any octave works. Use separate channels for live drums then");
        setupLabel (*this, lStop, "STOP keys"); setupCombo (*this, stopKey, stopKeys, 1, "First STOP key on your keyboard");
        setupCombo (*this, stopKey2, stopKeys, 1, "Second STOP key, for when you can't reach the first");
        setupButton (*this, learn1, "Learn", true, "Press a key on your MIDI keyboard to make it this STOP key");
        setupButton (*this, learn2, "Learn", true, "Press a key on your MIDI keyboard to make it this STOP key");
        learn1.onClick = [this] { learnSlot = learn1.getToggleState() ? 1 : 0; learn2.setToggleState (false, dontSendNotification); learnSerial = st.proc.lastInSerial.load(); };
        learn2.onClick = [this] { learnSlot = learn2.getToggleState() ? 2 : 0; learn1.setToggleState (false, dontSendNotification); learnSerial = st.proc.lastInSerial.load(); };
        setupLabel (*this, lPcKey, "Stop on the computer");
        setupLabel (*this, pcKey, "", 12.0f, th::text);
        setupButton (*this, learnPc, "Learn", true, "Press a key on the computer keyboard (while Drum Player has focus) to stop with it");
        learnPc.onClick = [this] { st.proc.uiLearnStopKey = learnPc.getToggleState(); };
        setupButton (*this, noPc, "None", false, "No computer key stops");
        noPc.onClick = [this] { st.proc.uiStopKey = 0; st.proc.uiLearnStopKey = false; learnPc.setToggleState (false, dontSendNotification); refresh(); };
        setupLabel (*this, lSplit, "Cutting a groove into keys");
        setupCombo (*this, split, { "Keep whole", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars" }, 1, "Used when you hold Shift while dropping, or choose 'cut into keys' when pasting. A normal drop keeps the groove whole");
        setupButton (*this, hostStop, "Stop when Studio One stops", true);
        setupButton (*this, thru, "Pass other notes through", true, "Notes outside the pattern keys go straight to the drum plugin, so you can play along");
        setupButton (*this, fills, "Dropped fills to black keys", true, "When several files are dropped, files with 'fill' in the name go to black keys");
        setupButton (*this, remember, "Open the way I left it", true, "On: new Drum Player instances start with your latest patterns, even ones you didn't save. Off (best): only what's saved in the song");
        setupButton (*this, songWithHost, "Song lane plays with Studio One", true, "On: when Studio One plays, the song lane plays (a key you press takes over). Off: only your keys play");
        setupButton (*this, loopSel, "Clicking a chunk loops it", true, "Click a chunk in the song lane and the loop goes around it, so Play repeats it");
        setupLabel (*this, help, "In Studio One: put Drum Player on an instrument track, then on the drum plugin's track choose Drum Player as the input.", 10.5f, th::faint);
        help.setJustificationType (Justification::topLeft);
        setupButton (*this, tabGeneral, "General", true); setupButton (*this, tabMap, "Drum map", true);
        setupButton (*this, tabInner, "Inner workings", true); setupButton (*this, tabLook, "Look", true);
        for (auto* b : { &tabGeneral, &tabMap, &tabInner, &tabLook }) b->setRadioGroupId (91);
        tabGeneral.setToggleState (true, dontSendNotification);
        tabGeneral.onClick = [this] { showTab (0); };
        tabMap.onClick = [this] { showTab (1); };
        tabInner.onClick = [this] { showTab (2); };
        tabLook.onClick = [this] { showTab (3); };
        addChildComponent (look);
        auto commit = [this] {
            if (loading) return;
            st.applySong ("Settings", [this] (Song& s) {
                auto& o = s.settings;
                o.outChannel = out.getSelectedItemIndex() + 1;
                o.zoneLow = zLo.getSelectedItemIndex(); o.zoneHigh = jmax (o.zoneLow, zHi.getSelectedItemIndex());
                o.startQuant = (StartQuant) start.getSelectedItemIndex(); o.latch = mode.getSelectedItemIndex() == 0;
                { const int si = stopKey.getSelectedItemIndex(); o.stopNote = si <= 0 ? -1 : si - 1; }
                { const int si = stopKey2.getSelectedItemIndex(); o.stopNote2 = si <= 0 ? -1 : si - 1; }
                o.triggerChannel = trigCh.getSelectedItemIndex(); o.playChannel = playCh.getSelectedItemIndex();
                o.mirrorOctaves = mirror.getToggleState();
                static const int sb[] = { 0, 1, 2, 4, 8, 16 }; o.splitBars = sb[jlimit (0, 5, split.getSelectedItemIndex())];
                o.stopWithHost = hostStop.getToggleState(); o.passThrough = thru.getToggleState();
                o.fillsToBlack = fills.getToggleState(); o.rememberLast = remember.getToggleState(); o.loopSelected = loopSel.getToggleState();
                st.song().songMode = songWithHost.getToggleState();
            });
        };
        for (auto* c : { &out, &zLo, &zHi, &start, &mode, &stopKey, &stopKey2, &split, &trigCh, &playCh }) c->onChange = commit;
        startTimerHz (10);
        for (auto* b : { &hostStop, &thru, &fills, &remember, &mirror, &songWithHost, &loopSel }) b->onClick = commit;
    }
    LookEditor look;
    MapPage map; InnerPage inner;
    void showTab (bool lookTab) { showTab (lookTab ? 3 : 0); }
    void showTab (int t)
    {
        tab = t;
        for (int i = 0; i < getNumChildComponents(); ++i) {
            auto* c = getChildComponent (i);
            if (c == &tabGeneral || c == &tabLook || c == &tabMap || c == &tabInner) continue;
            const bool special = c == &look || c == &map || c == &inner;
            c->setVisible (special ? (c == &look ? t == 3 : c == &map ? t == 1 : t == 2) : t == 0);
        }
        tabGeneral.setToggleState (t == 0, dontSendNotification); tabMap.setToggleState (t == 1, dontSendNotification);
        tabInner.setToggleState (t == 2, dontSendNotification); tabLook.setToggleState (t == 3, dontSendNotification);
        if (t == 3) look.refreshList();
        if (t == 1) map.refresh();
        if (t == 2) inner.refresh();
        static const char* names[] = { "SETTINGS", "SETTINGS  ·  DRUM MAP", "SETTINGS  ·  INNER WORKINGS", "SETTINGS  ·  LOOK" };
        title = String::fromUTF8 (names[jlimit (0, 3, t)]);
        repaint();
    }
    int tab = 0;
    void refresh() override
    {
        if (tab == 1) map.refresh();
        if (tab == 2) inner.refresh();
        loading = true;
        const auto& o = st.song().settings;
        out.setSelectedItemIndex (o.outChannel - 1, dontSendNotification);
        zLo.setSelectedItemIndex (o.zoneLow, dontSendNotification); zHi.setSelectedItemIndex (o.zoneHigh, dontSendNotification);
        start.setSelectedItemIndex ((int) o.startQuant, dontSendNotification); mode.setSelectedItemIndex (o.latch ? 0 : 1, dontSendNotification);
        stopKey.setSelectedItemIndex (o.resolveStop() >= 0 ? o.resolveStop() + 1 : 0, dontSendNotification);
        stopKey2.setSelectedItemIndex (o.stopNote2 >= 0 ? o.stopNote2 + 1 : 0, dontSendNotification);
        pcKey.setText (st.proc.uiStopKey == 0 ? String ("none") : KeyPress (st.proc.uiStopKey).getTextDescription(), dontSendNotification);
        learnPc.setToggleState (st.proc.uiLearnStopKey, dontSendNotification);
        trigCh.setSelectedItemIndex (o.triggerChannel, dontSendNotification); playCh.setSelectedItemIndex (o.playChannel, dontSendNotification);
        mirror.setToggleState (o.mirrorOctaves, dontSendNotification);
        const int sb = o.splitBars; split.setSelectedItemIndex (sb <= 0 ? 0 : sb == 1 ? 1 : sb == 2 ? 2 : sb <= 4 ? 3 : sb <= 8 ? 4 : 5, dontSendNotification);
        hostStop.setToggleState (o.stopWithHost, dontSendNotification); thru.setToggleState (o.passThrough, dontSendNotification);
        fills.setToggleState (o.fillsToBlack, dontSendNotification); remember.setToggleState (o.rememberLast, dontSendNotification);
        loopSel.setToggleState (o.loopSelected, dontSendNotification); songWithHost.setToggleState (st.song().songMode, dontSendNotification);
        loading = false;
    }
    void resized() override
    {
        auto a = content();
        auto tabs = a.removeFromTop (26);
        for (auto* b : { &tabGeneral, &tabMap, &tabInner, &tabLook }) { b->setBounds (tabs.removeFromLeft (b->getWidth()).reduced (0, 2)); tabs.removeFromLeft (6); }
        a.removeFromTop (8);
        look.setBounds (a); map.setBounds (a); inner.setBounds (a);
        const int lw = 170;
        out.setBounds (row (a, lOut, lw).removeFromLeft (170));
        auto z = row (a, lZone, lw); zLo.setBounds (z.removeFromLeft (110)); z.removeFromLeft (8); zHi.setBounds (z.removeFromLeft (110));
        start.setBounds (row (a, lStart, lw).removeFromLeft (170));
        mode.setBounds (row (a, lMode, lw).removeFromLeft (210));
        {   auto r = row (a, lStop, lw);
            stopKey.setBounds (r.removeFromLeft (90)); r.removeFromLeft (4); learn1.setBounds (r.removeFromLeft (learn1.getWidth()).withSizeKeepingCentre (learn1.getWidth(), btnH)); r.removeFromLeft (12);
            stopKey2.setBounds (r.removeFromLeft (90)); r.removeFromLeft (4); learn2.setBounds (r.removeFromLeft (learn2.getWidth()).withSizeKeepingCentre (learn2.getWidth(), btnH)); }
        {   auto r = row (a, lPcKey, lw);
            pcKey.setBounds (r.removeFromLeft (90)); r.removeFromLeft (4); learnPc.setBounds (r.removeFromLeft (learnPc.getWidth()).withSizeKeepingCentre (learnPc.getWidth(), btnH)); r.removeFromLeft (4);
            noPc.setBounds (r.removeFromLeft (noPc.getWidth()).withSizeKeepingCentre (noPc.getWidth(), btnH)); }
        trigCh.setBounds (row (a, lTrig, lw).removeFromLeft (130));
        playCh.setBounds (row (a, lPlayCh, lw).removeFromLeft (130));
        split.setBounds (row (a, lSplit, lw).removeFromLeft (120));
        a.removeFromTop (4);
        mirror.setVisible (false);
        for (auto* b : { &songWithHost, &hostStop, &loopSel, &thru, &fills, &remember }) { b->setBounds (a.removeFromTop (26).removeFromLeft (b->getWidth()).reduced (0, 2)); a.removeFromTop (4); }
        help.setBounds (a.removeFromTop (40));
    }
private:
    EditState& st; bool loading = false;
    Label lOut, lZone, lStart, lMode, lStop, lSplit, help, lTrig, lPlayCh, lPcKey, pcKey;
    ComboBox out, zLo, zHi, start, mode, stopKey, stopKey2, split, trigCh, playCh;
    TextButton learn1, learn2, learnPc, noPc;
    int learnSlot = 0, learnSerial = 0;
    void timerCallback() override
    {
        // MIDI learn for the STOP keys: the next key you play
        if (learnSlot != 0 && st.proc.lastInSerial.load() != learnSerial) {
            const int n = st.proc.lastInNote.load();
            const int slot = learnSlot; learnSlot = 0;
            learn1.setToggleState (false, dontSendNotification); learn2.setToggleState (false, dontSendNotification);
            if (n >= 0) st.applySong ("STOP key", [n, slot] (Song& s) { if (slot == 1) s.settings.stopNote = n; else s.settings.stopNote2 = n; movePatternOffKey (s, n); });
            refresh();
        }
        if (! st.proc.uiLearnStopKey && learnPc.getToggleState()) refresh();     // learnt on the computer keyboard
    }
    TextButton hostStop, thru, fills, remember, mirror, songWithHost, loopSel, tabGeneral, tabLook, tabMap, tabInner;
};

// ---------------------------------------------------------------- preset browser
class PresetPage : public Page, private ListBoxModel
{
public:
    PresetPage (EditState& e) : Page ("PRESETS"), st (e)
    {
        setupLabel (*this, lFolder, "Folder", 11.5f);
        setupCombo (*this, folder, {});
        folder.onChange = [this] { scan(); };
        addAndMakeVisible (search); search.setTextToShowWhenEmpty ("Search", th::faint); search.setFont (font (12.0f, 1));
        search.onTextChange = [this] { scan(); };
        setupButton (*this, favOnly, "Favourites", true); favOnly.onClick = [this] { scan(); };
        addAndMakeVisible (list); list.setModel (this); list.setRowHeight (24);
        list.setColour (ListBox::backgroundColourId, Colours::black.withAlpha (0.2f));
        addAndMakeVisible (name); name.setFont (font (12.0f, 1)); name.setTextToShowWhenEmpty ("Preset name", th::faint);
        setupButton (*this, load, "Load"); load.onClick = [this] { loadSelected(); };
        setupButton (*this, save, "Save"); save.onClick = [this] { saveAs (name.getText().trim()); };
        setupButton (*this, fav, "Star"); fav.onClick = [this] { toggleFav(); };
        setupButton (*this, del, "Delete"); del.onClick = [this] { deleteSelected(); };
        setupButton (*this, newFolder, "New folder"); newFolder.onClick = [this] { makeFolder(); };
        setupButton (*this, reveal, "Show files"); reveal.onClick = [] { presetFolder().revealToUser(); };
        setupButton (*this, importMidi, "Import MIDI files..."); importMidi.onClick = [this] { if (onImport) onImport(); };
        loadFavs();
    }
    std::function<void()> onImport, onLoaded;
    void refresh() override { scanFolders(); }
    void step (int dir)                        // previous / next preset in the current list
    {
        if (files.isEmpty()) scanFolders();
        if (files.isEmpty()) return;
        int i = -1;
        for (int k = 0; k < files.size(); ++k) if (files[k].getFileNameWithoutExtension() == st.proc.presetName) i = k;
        i = (i + dir + files.size()) % files.size();
        if (st.proc.loadPreset (files[i])) { list.selectRow (i); if (onLoaded) onLoaded(); }
    }
    void resized() override
    {
        auto a = content();
        auto r = a.removeFromTop (28);
        lFolder.setBounds (r.removeFromLeft (48)); folder.setBounds (r.removeFromLeft (150).reduced (0, 3)); r.removeFromLeft (8);
        favOnly.setBounds (r.removeFromRight (favOnly.getWidth()).reduced (0, 3)); r.removeFromRight (8);
        search.setBounds (r.reduced (0, 3));
        a.removeFromTop (4);
        auto bottom = a.removeFromBottom (64);
        list.setBounds (a);
        auto b1 = bottom.removeFromTop (30).withTrimmedTop (6);
        name.setBounds (b1.removeFromLeft (180)); b1.removeFromLeft (6);
        for (auto* b : { &save, &load, &fav, &del }) { b->setBounds (b1.removeFromLeft (b->getWidth())); b1.removeFromLeft (5); }
        auto b2 = bottom.withTrimmedTop (6).removeFromTop (24);
        for (auto* b : { &newFolder, &reveal, &importMidi }) { b->setBounds (b2.removeFromLeft (b->getWidth())); b2.removeFromLeft (5); }
    }
private:
    EditState& st;
    Label lFolder; ComboBox folder; TextEditor search, name;
    TextButton favOnly, load, save, fav, del, newFolder, reveal, importMidi;
    ListBox list;
    Array<File> files, folders;
    StringArray favs;

    File currentFolder() const { const int i = folder.getSelectedItemIndex(); return (i > 0 && i - 1 < folders.size()) ? folders[i - 1] : presetFolder(); }
    void scanFolders()
    {
        const auto keep = folder.getText();
        folders = presetFolder().findChildFiles (File::findDirectories, false);
        folders.sort();
        folder.clear (dontSendNotification);
        folder.addItem ("All presets", 1);
        for (int i = 0; i < folders.size(); ++i) folder.addItem (folders[i].getFileName(), i + 2);
        int sel = 0; for (int i = 0; i < folder.getNumItems(); ++i) if (folder.getItemText (i) == keep) sel = i;
        folder.setSelectedItemIndex (sel, dontSendNotification);
        scan();
    }
    void scan()
    {
        const bool all = folder.getSelectedItemIndex() <= 0;
        files = (all ? presetFolder() : currentFolder()).findChildFiles (File::findFiles, all, String ("*") + presetExt);
        const auto q = search.getText().trim();
        for (int i = files.size(); --i >= 0;) {
            const auto n = files[i].getFileNameWithoutExtension();
            if ((q.isNotEmpty() && ! n.containsIgnoreCase (q)) || (favOnly.getToggleState() && ! favs.contains (files[i].getFullPathName()))) files.remove (i);
        }
        files.sort();
        list.updateContent(); list.repaint();
    }
    int getNumRows() override { return files.size(); }
    void paintListBoxItem (int row, Graphics& g, int w, int h, bool sel) override
    {
        if (row >= files.size()) return;
        const auto& f = files[row];
        const bool current = f.getFileNameWithoutExtension() == st.proc.presetName;
        if (sel) { g.setColour (th::blue.withAlpha (0.22f)); g.fillRoundedRectangle (2, 1, w - 4.0f, h - 2.0f, 4); }
        g.setFont (font (12.5f, current ? 3 : 1)); g.setColour (current ? th::amber : th::text);
        g.drawText (f.getFileNameWithoutExtension(), 26, 0, w - 140, h, Justification::centredLeft, true);
        g.setFont (font (10.5f, 1)); g.setColour (th::faint);
        g.drawText (f.getParentDirectory() == presetFolder() ? String() : f.getParentDirectory().getFileName(), w - 120, 0, 112, h, Justification::centredRight, true);
        if (favs.contains (f.getFullPathName())) { g.setColour (th::amber); g.setFont (font (13.0f, 3)); g.drawText ("*", 8, 0, 14, h, Justification::centred); }
    }
    void listBoxItemDoubleClicked (int, const MouseEvent&) override { loadSelected(); }
    void selectedRowsChanged (int row) override { if (row >= 0 && row < files.size()) name.setText (files[row].getFileNameWithoutExtension(), false); }
    void loadSelected()
    {
        const int r = list.getSelectedRow();
        if (r >= 0 && r < files.size() && st.proc.loadPreset (files[r])) { list.repaint(); if (onLoaded) onLoaded(); if (st.toast) st.toast ("Loaded " + files[r].getFileNameWithoutExtension()); }
    }
    void saveAs (const String& n)
    {
        if (n.isEmpty()) { if (st.toast) st.toast ("Type a name first"); return; }
        auto f = currentFolder().getChildFile (File::createLegalFileName (n) + presetExt);
        if (st.proc.savePreset (f)) { scan(); if (st.toast) st.toast ("Saved " + n); if (onLoaded) onLoaded(); }
    }
    void deleteSelected()
    {
        const int r = list.getSelectedRow();
        if (r < 0 || r >= files.size()) return;
        const auto f = files[r];
        AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "Delete preset", "Delete \"" + f.getFileNameWithoutExtension() + "\"?", "Delete", "Cancel", this,
            ModalCallbackFunction::create ([this, f] (int res) { if (res == 1) { f.deleteFile(); scan(); } }));
    }
    void makeFolder()
    {
        auto* aw = new AlertWindow ("New folder", "", MessageBoxIconType::NoIcon);
        aw->addTextEditor ("n", "");
        aw->addButton ("Create", 1, KeyPress (KeyPress::returnKey)); aw->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        aw->enterModalState (true, ModalCallbackFunction::create ([this, aw] (int res) {
            if (res == 1) { const auto n = File::createLegalFileName (aw->getTextEditorContents ("n").trim()); if (n.isNotEmpty()) presetFolder().getChildFile (n).createDirectory(); scanFolders(); }
        }), true);
    }
    void loadFavs() { favs = StringArray::fromLines (favouritesFile().loadFileAsString()); favs.removeEmptyStrings(); }
    void toggleFav()
    {
        const int r = list.getSelectedRow();
        if (r < 0 || r >= files.size()) return;
        const auto p = files[r].getFullPathName();
        if (favs.contains (p)) favs.removeString (p); else favs.add (p);
        favouritesFile().replaceWithText (favs.joinIntoString ("\n"));
        list.repaint();
    }
};


// ---------------------------------------------------------------- recording page
class RecordPage : public Page
{
public:
    RecordPage (EditState& e) : Page ("RECORDING"), st (e)
    {
        setupLabel (*this, lCount, "Count-in"); setupCombo (*this, count, { "Off", "1 bar", "2 bars" });
        setupLabel (*this, lMetro, "Click"); setupCombo (*this, metro, { "Off", "Count-in only", "While recording", "Whenever a pattern plays" });
        setupLabel (*this, lLevel, "Click volume"); setupSlider (*this, level); level.setRange (0, 100, 1); level.setTextValueSuffix (" %");
        setupLabel (*this, lMode, "Mode"); setupCombo (*this, mode, { "Record mix: add to the pattern", "Record replace: a drum you play loses its old notes" });
        setupLabel (*this, lStart, "Start"); setupCombo (*this, startBox, { "With Rec (after the count-in)", "When I play the first note" });
        setupLabel (*this, lLayers, "Layers"); setupCombo (*this, layersBox, { "Off: one pattern", "Every pass is a layer (comp them like Studio One)" });
        setupLabel (*this, lTarget, "Record into"); setupCombo (*this, targetBox, { "Drum Player and the DAW", "Drum Player only", "The DAW only" });
        setupLabel (*this, lStop, "Stop recording after"); setupCombo (*this, stopAfter, { "I press Rec again", "1 pass", "2 passes", "4 passes", "8 passes" });
        setupLabel (*this, lQ, "Quantize while recording");
        StringArray rq { "Off (as played)" }; for (auto& v : noteValues) rq.add (v.name);
        setupCombo (*this, recQ, rq);
        setupLabel (*this, lVel, "Velocity"); setupCombo (*this, vel, { "As played", "127", "110", "100", "80", "64" });
        setupLabel (*this, help, "Press Rec: the key you're editing starts (after the count-in if nothing is playing) and every drum note you play goes into it, round and round. "
                                 "It works on an empty key too. Rec again stops recording; the pattern keeps playing. The click comes out of Drum Player's own track.", 10.5f, th::faint);
        help.setJustificationType (Justification::topLeft);
        auto commit = [this] {
            if (loading) return;
            static const int stops[] = { 0, 1, 2, 4, 8 };
            static const int vels[] = { 0, 127, 110, 100, 80, 64 };
            auto& o = st.song().settings;                       // small settings: no undo step needed
            o.countInBars = count.getSelectedItemIndex(); o.metroMode = metro.getSelectedItemIndex();
            o.metroLevel = (float) level.getValue() / 100.0f; o.recMode = mode.getSelectedItemIndex();
            o.recStopAfter = stops[jlimit (0, 4, stopAfter.getSelectedItemIndex())];
            o.recQuantize = recQ.getSelectedItemIndex(); o.recFixedVel = vels[jlimit (0, 5, vel.getSelectedItemIndex())];
            o.recWaitForKey = startBox.getSelectedItemIndex() == 1; o.recLayers = layersBox.getSelectedItemIndex() == 1;
            o.recTarget = jlimit (0, 2, targetBox.getSelectedItemIndex());
            st.proc.publish();
        };
        for (auto* c : { &count, &metro, &mode, &stopAfter, &recQ, &vel, &startBox, &layersBox, &targetBox }) c->onChange = commit;
        level.onValueChange = commit;
    }
    void refresh() override
    {
        loading = true;
        const auto& o = st.song().settings;
        count.setSelectedItemIndex (o.countInBars, dontSendNotification); metro.setSelectedItemIndex (o.metroMode, dontSendNotification);
        level.setValue (o.metroLevel * 100.0, dontSendNotification); mode.setSelectedItemIndex (o.recMode, dontSendNotification);
        const int sa = o.recStopAfter; stopAfter.setSelectedItemIndex (sa <= 0 ? 0 : sa == 1 ? 1 : sa == 2 ? 2 : sa <= 4 ? 3 : 4, dontSendNotification);
        recQ.setSelectedItemIndex (o.recQuantize, dontSendNotification);
        const int v = o.recFixedVel; vel.setSelectedItemIndex (v == 0 ? 0 : v >= 127 ? 1 : v >= 110 ? 2 : v >= 100 ? 3 : v >= 80 ? 4 : 5, dontSendNotification);
        startBox.setSelectedItemIndex (o.recWaitForKey ? 1 : 0, dontSendNotification); layersBox.setSelectedItemIndex (o.recLayers ? 1 : 0, dontSendNotification);
        targetBox.setSelectedItemIndex (o.recTarget, dontSendNotification);
        loading = false;
    }
    // every recording option as a menu (right-click Rec)
    void showMenu (Component* target)
    {
        refresh();
        PopupMenu m; m.addSectionHeader ("RECORDING");
        struct Item { const char* title; ComboBox* box; };
        const Item items[] = { { "Count-in", &count }, { "Click", &metro }, { "Mode", &mode }, { "Start", &startBox }, { "Record into", &targetBox },
                               { "Layers", &layersBox }, { "Stop recording after", &stopAfter }, { "Quantize while recording", &recQ }, { "Velocity", &vel } };
        int base = 1000;
        for (auto& it : items) {
            PopupMenu sub;
            for (int i = 0; i < it.box->getNumItems(); ++i) sub.addItem (base + i, it.box->getItemText (i), true, it.box->getSelectedItemIndex() == i);
            m.addSubMenu (String (it.title) + ":  " + it.box->getText(), sub);
            base += 100;
        }
        PopupMenu lv; for (int v : { 25, 50, 75, 100 }) lv.addItem (5000 + v, String (v) + " %", true, roundToInt (level.getValue()) == v);
        m.addSubMenu ("Click volume:  " + String (roundToInt (level.getValue())) + " %", lv);
        std::vector<ComboBox*> boxes; for (auto& it : items) boxes.push_back (it.box);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (target), [this, boxes] (int r) {
            if (r >= 5000) { level.setValue (r - 5000, sendNotificationSync); return; }
            if (r < 1000) return;
            const int which = (r - 1000) / 100, idx = (r - 1000) % 100;
            if (which < (int) boxes.size()) boxes[(size_t) which]->setSelectedItemIndex (idx, sendNotificationSync);
        });
    }
    void resized() override
    {
        auto a = content(); const int lw = 160;
        startBox.setBounds (row (a, lStart, lw).removeFromLeft (260));
        targetBox.setBounds (row (a, lTarget, lw).removeFromLeft (220));
        layersBox.setBounds (row (a, lLayers, lw));
        count.setBounds (row (a, lCount, lw).removeFromLeft (110));
        metro.setBounds (row (a, lMetro, lw).removeFromLeft (200));
        level.setBounds (row (a, lLevel, lw));
        mode.setBounds (row (a, lMode, lw));
        stopAfter.setBounds (row (a, lStop, lw).removeFromLeft (170));
        recQ.setBounds (row (a, lQ, lw).removeFromLeft (130));
        vel.setBounds (row (a, lVel, lw).removeFromLeft (110));
        help.setBounds (a.removeFromTop (70));
    }
private:
    EditState& st; bool loading = false;
    Label lCount, lMetro, lLevel, lMode, lStop, lQ, lVel, help, lStart, lLayers, lTarget;
    ComboBox count, metro, mode, stopAfter, recQ, vel, startBox, layersBox, targetBox;
    FineSlider level;
};

// ---------------------------------------------------------------- mixer: a strip per drum with its pan and a meter (MIDI only, no EQ)
class MixerPage : public Page, private Timer
{
public:
    explicit MixerPage (EditState& e) : Page ("MIXER"), st (e)
    {
        addAndMakeVisible (view); view.setViewedComponent (&strips, false); view.setScrollBarsShown (false, true);
        strips.owner = this;
        setupLabel (*this, help, "Pan per drum (sent as CC10 on the drum's own MIDI channel; give a drum its own channel in its lane's right-click menu). Meters show what Drum Player sends.", 10.5f, th::faint);
        help.setJustificationType (Justification::topLeft);
    }
    void refresh() override { rebuild(); }
    void visibilityChanged() override { if (isVisible()) { rebuild(); startTimerHz (30); } else stopTimer(); }
    void resized() override
    {
        auto a = content();
        help.setBounds (a.removeFromBottom (30));
        view.setBounds (a);
        const int need = (int) strips.items.size() * stripW;
        strips.setSize (jmax (a.getWidth(), need), jmax (120, a.getHeight() - (need > a.getWidth() ? 10 : 0)));
        strips.layout();
    }
private:
    static constexpr int stripW = 66;
    struct Strips : public Component
    {
        MixerPage* owner = nullptr;
        struct Item { int note; std::unique_ptr<Slider> pan; float level = 0; uint32 seen = 0; };
        std::vector<Item> items;
        void layout()
        {
            for (int i = 0; i < (int) items.size(); ++i)
                items[(size_t) i].pan->setBounds (i * stripW + 8, 26, stripW - 16, stripW - 16);
        }
        void paint (Graphics& g) override
        {
            auto& st = owner->st;
            for (int i = 0; i < (int) items.size(); ++i) {
                const auto& it = items[(size_t) i];
                auto r = Rectangle<float> ((float) i * stripW + 2, 0, stripW - 4.0f, (float) getHeight());
                g.setColour (Colours::black.withAlpha (i % 2 ? 0.18f : 0.28f)); g.fillRoundedRectangle (r, 5);
                const auto* l = st.song().lane (it.note);
                g.setColour (st.laneColour (it.note)); g.fillRoundedRectangle (r.withHeight (4).reduced (6, 0), 2);
                g.setFont (font (10.5f, 2)); g.setColour (th::text);
                g.drawFittedText (l != nullptr ? l->name : gmDrumName (it.note), r.withY (6).withHeight (18).toNearestInt().reduced (3, 0), Justification::centred, 1, 0.7f);
                // pan readout
                const int pv = l != nullptr ? l->pan : 64;
                g.setFont (font (9.5f, 1)); g.setColour (l != nullptr && l->channel > 0 ? th::dim : th::faint);
                const String pt = pv == 64 ? String ("C") : (pv < 64 ? "L" + String ((64 - pv) * 100 / 64) : "R" + String ((pv - 64) * 100 / 63));
                g.drawText (pt + (l != nullptr && l->channel > 0 ? "  ch " + String (l->channel) : "  main ch"), r.withY (26.0f + stripW - 16).withHeight (14).toNearestInt(), Justification::centred);
                // meter
                auto m = r.withTrimmedTop (26.0f + stripW).withTrimmedBottom (24).withSizeKeepingCentre (12.0f, r.getHeight() - 26.0f - stripW - 24.0f);
                g.setColour (Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (m, 2);
                const float v = it.level;
                auto fillR = m.reduced (2).withTrimmedTop ((m.getHeight() - 4) * (1.0f - v));
                g.setGradientFill (ColourGradient (th::red, m.getX(), m.getY(), th::green, m.getX(), m.getBottom(), false));
                g.fillRoundedRectangle (fillR, 1.5f);
                g.setFont (font (9.0f, 1)); g.setColour (th::faint);
                g.drawText (keyName (it.note), r.withTrimmedTop (r.getHeight() - 20).toNearestInt(), Justification::centred);
            }
        }
    } strips;
    EditState& st;
    Viewport view; Label help;

    void rebuild()
    {
        for (auto& it : strips.items) strips.removeChildComponent (it.pan.get());
        strips.items.clear();
        for (auto& l : st.song().lanes) {
            Strips::Item it; it.note = l.note;
            it.pan = std::make_unique<Slider> (Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox);
            it.pan->setRange (0, 127, 1); it.pan->setValue (l.pan, dontSendNotification); it.pan->setDoubleClickReturnValue (true, 64);
            it.pan->setColour (Slider::rotarySliderFillColourId, th::amber);
            const int note = l.note;
            it.pan->onDragStart = [this] { st.proc.beginEdit(); };
            it.pan->onValueChange = [this, note, sl = it.pan.get()] {
                if (auto* ln = st.song().lane (note)) { ln->pan = (int) sl->getValue(); st.proc.publish(); strips.repaint(); }
            };
            it.pan->onDragEnd = [this, note] {
                st.proc.endEdit ("Pan");
                if (auto* ln = st.song().lane (note); ln != nullptr && ln->channel == 0 && st.toast) st.toast ("This drum is on the main channel: its pan reaches the plugin once it has its own MIDI channel (lane menu)");
            };
            strips.addAndMakeVisible (*it.pan);
            strips.items.push_back (std::move (it));
        }
        resized();
    }
    void timerCallback() override
    {
        bool any = false;
        const auto now = Time::getMillisecondCounter(); ignoreUnused (now);
        for (auto& it : strips.items) {
            const uint32 h = st.proc.outHits[(size_t) it.note].load();
            if (h != it.seen) { it.seen = h; it.level = jmax (it.level, st.proc.outVel[(size_t) it.note].load() / 127.0f); any = true; }
            else if (it.level > 0.0f) { it.level = jmax (0.0f, it.level - 0.045f); any = true; }
        }
        if (any) strips.repaint();
    }
};

} // namespace dp
