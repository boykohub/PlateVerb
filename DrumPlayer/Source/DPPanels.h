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
    void paint (Graphics& g) override
    {
        drawCard (g, getLocalBounds().toFloat().reduced (2), th::panel, 12, "Pages");
        g.setFont (font (13.0f, 3)); g.setColour (th::amber);
        g.drawText (title, 16, 8, getWidth() - 32, 22, Justification::centredLeft);
        g.setFont (font (10.5f, 1)); g.setColour (th::faint);
        g.drawText ("press its button again to close", 16, 8, getWidth() - 32, 22, Justification::centredRight);
    }
    virtual void refresh() {}
    String title;
protected:
    Rectangle<int> content() const { return getLocalBounds().reduced (16, 0).withTrimmedTop (38).withTrimmedBottom (12); }
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
        setTooltip (big ? "Drag a key onto another key to swap. Drag to the tray to park. Click to choose which key you edit."
                        : "Click: play (and edit).  Ctrl+click: edit without playing.  Red key: stop.  Drag onto another key: swap.  Drag out of the window: MIDI file.  Drop MIDI files here.");
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
            const bool usable = isPatternKey (k) || k == s.settings.resolveStop();
            g.setGradientFill (ColourGradient (Colour (0xffe9eaee).withMultipliedBrightness (usable ? 1.0f : 0.55f), r.getTopLeft(),
                                               Colour (0xffb9bcc4).withMultipliedBrightness (usable ? 1.0f : 0.55f), r.getBottomLeft(), false));
            g.fillRoundedRectangle (r.reduced (0.5f, 0), 3);
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
            drawTile (g, r.withTrimmedTop (r.getHeight() * 0.3f).reduced (2, 3), k, p, true);
            decorate (g, r, k, play, queued, blink);
        }
        // octave labels
        g.setFont (font (9.0f, 2)); g.setColour (Colours::black.withAlpha (0.55f));
        for (int k = lo; k <= hi; ++k) if (k % 12 == 0) { auto r = keyRect (k); g.drawText (keyName (k), r.withHeight (14).translated (0, (int) (r.getHeight() * 0.42f)), Justification::centred); }
        // scroll arrows: the whole keyboard is there, an octave per click
        drawArrow (g, leftArrow(), true, viewLow > firstKey());
        drawArrow (g, rightArrow(), false, viewEnd < s.settings.zoneHigh);
    }

    // ------------------------------------------------------------ geometry (a scrolling window onto the whole keyboard)
    int firstKey() const
    {
        const auto& o = st.song().settings;
        const int stop = o.resolveStop();
        int f = (stop >= 0 && stop < o.zoneLow) ? stop : o.zoneLow;
        while (f > 0 && isBlackKey (f)) --f;
        return f;
    }
    bool isPatternKey (int k) const { const auto& o = st.song().settings; return k >= o.zoneLow && k <= o.zoneHigh && k != o.resolveStop(); }
    int visibleWhites() const { return jmax (7, (getWidth() - 2 * arrowW) / (large ? 30 : 24)); }
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
        if (! isPatternKey (k) && k != st.song().settings.resolveStop()) return;
        if (e.mods.isPopupMenu()) { if (onKeyMenu) onKeyMenu (k); return; }
        if (! large && k == st.song().settings.resolveStop()) { st.proc.uiStop(); pressedStop = true; repaint(); return; }
        st.song().editKey = k;
        st.focusLane = -1;
        if (! large && ! e.mods.isCommandDown()) { st.proc.uiTrigger (k); playedKey = k; }
        if (st.changed) st.changed();
        dragKey = k;
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (dragKey < 0 || e.getDistanceFromDragStart() < 6) return;
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
        if (! st.song().settings.latch && playedKey >= 0) st.proc.uiRelease (playedKey);
        playedKey = -1; dragKey = -1;
        if (pressedStop) { pressedStop = false; repaint(); }
    }

    // ------------------------------------------------------------ internal drag (swap / unpark)
    bool isInterestedInDragSource (const SourceDetails& d) override { const auto s = d.description.toString(); return s.startsWith ("key:") || s.startsWith ("park:"); }
    void itemDragMove (const SourceDetails& d) override { dropHover = keyAt (d.localPosition.toFloat()); repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHover = -1; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        const int to = keyAt (d.localPosition.toFloat());
        dropHover = -1;
        if (to < 0 || ! isPatternKey (to)) { repaint(); return; }
        const auto s = d.description.toString();
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
    int dragKey = -1, playedKey = -1;
    int viewLow = -1, viewEnd = 0; float wheelAcc = 0;
    static constexpr int arrowW = 18;
    bool pressedStop = false;

    void drawTile (Graphics& g, Rectangle<float> r, int k, const Pattern& p, bool black)
    {
        if (k == st.song().settings.resolveStop()) {             // the stop key
            g.setColour (th::red.withAlpha (pressedStop ? 1.0f : 0.85f)); g.fillRoundedRectangle (r, 3);
            g.setColour (th::ink); g.setFont (font (large ? 11.0f : 9.5f, 3));
            g.drawFittedText ("STOP", r.reduced (1).toNearestInt(), Justification::centred, 1, 0.7f);
            return;
        }
        if (p.notes.empty() && p.name.isEmpty()) return;
        const auto c = th::typeColour (p.type);
        g.setColour (c.withAlpha (black ? 0.85f : 0.9f));
        g.fillRoundedRectangle (r, 3);
        g.setColour (th::ink);
        const float fs = large ? 11.0f : 9.5f;
        g.setFont (font (fs, 2));
        String label = p.name.isNotEmpty() ? p.name : String (typeName (p.type));
        if (black && ! large) label = label.substring (0, 3);
        g.drawFittedText (label, r.reduced (2, 1).toNearestInt(), Justification::centred, large ? 3 : 2, 0.8f);
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
        if (k == dropHover) { g.setColour (th::green.withAlpha (0.45f)); g.fillRoundedRectangle (r.reduced (1.0f), 3); }
        if (k == flashNote && Time::getMillisecondCounter() < flashUntil) { g.setColour (th::text.withAlpha (0.45f)); g.fillRoundedRectangle (r.reduced (2.0f), 3); }
    }
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
                g.setColour (th::typeColour (p.type)); g.fillRoundedRectangle (r.reduced (1.5f), 4);
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
        setTooltip ("Drag keys here to chain them. Drag a tile to reorder, right-click it for repeats. Click a tile to edit its pattern");
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
            g.setColour (th::typeColour (p.type).withAlpha (p.notes.empty() ? 0.35f : 0.9f)); g.fillRoundedRectangle (r, 5);
            if (on) { g.setColour (th::amber); g.drawRoundedRectangle (r.expanded (1), 5, 2.2f); }
            if (i == dragStep) { g.setColour (Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (r, 5); }
            g.setColour (th::ink); g.setFont (font (10.5f, 2));
            g.drawFittedText ((p.name.isNotEmpty() ? p.name : keyName (step.key)), r.reduced (6, 1).withTrimmedRight (22).toNearestInt(), Justification::centredLeft, 1, 0.8f);
            g.setFont (font (10.5f, 3));
            g.drawText ("x" + String (step.repeats), r.reduced (5, 0).toNearestInt(), Justification::centredRight);
            if (dropAt == i) { g.setColour (th::green); g.fillRect (r.getX() - 3.0f, r.getY(), 2.0f, r.getHeight()); }
        }
        g.restoreState();
    }
    void resized() override
    {
        RowPlacer rp (getLocalBounds().withTrimmedLeft (52), 5);
        rp.place (sel, 120); rp.place (play); rp.place (loop); rp.place (keyBtn, 86); rp.place (dragOut);
        tilesX = rp.x + 6;
    }
    void mouseDown (const MouseEvent& e) override
    {
        const int i = stepAt (e.position);
        downStep = i;
        if (i < 0) return;
        if (e.mods.isPopupMenu()) { stepMenu (i); return; }
        st.song().editKey = chain().steps[(size_t) i].key;
        if (st.changed) st.changed();
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (downStep < 0 || e.getDistanceFromDragStart() < 6) return;
        if (auto* c = DragAndDropContainer::findParentDragContainerFor (this))
            if (! c->isDragAndDropActive()) {
                const auto r = tileRect (downStep).toNearestInt();
                const auto img = createComponentSnapshot (r, true);
                const Point<int> off = r.getPosition() - e.getMouseDownPosition();
                dragStep = downStep;
                c->startDragging ("chainstep:" + String (downStep), this, ScaledImage (img), false, &off);
            }
    }
    void mouseUp (const MouseEvent&) override { downStep = -1; dragStep = -1; repaint(); }
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
                    "their left edge to trim. Ctrl-drag copies. D duplicates, Alt+X splits at the cursor, M mutes, Del deletes. Double-click: edit the pattern");
    }
    std::function<void()> onFocus;
    float pxPerBeat = 12.0f; int scrollX = 0; double cursorBeat = 0; int snap = 0;   // snap: 0 bar, 1 beat, 2 1/8, 3 1/16, 4 off
    static constexpr int rulerH = 16;

    double beatToX (double b) const { return b * pxPerBeat - scrollX; }
    double xToBeat (double x) const { return (x + scrollX) / pxPerBeat; }
    double snapBeat (double b, bool alt) const
    {
        if (alt || snap >= 4) return jmax (0.0, std::round (b * 16) / 16);   // free (1/64 steps)
        static const double g[] = { 4.0, 1.0, 0.5, 0.25 };
        return jmax (0.0, std::round (b / g[snap]) * g[snap]);
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
            const auto col = th::typeColour (p.type).withMultipliedAlpha (c.muted ? 0.3f : 1.0f);
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
                g.setColour (th::ink.withAlpha (0.45f));
                for (auto& n : p.notes) {
                    if (n.tick >= p.lengthTicks()) continue;
                    const double nt = n.tick / (double) PPQ;
                    const float y = r.getY() + 14 + (r.getHeight() - 17) * (1.0f - (float) jlimit (0, 127, n.note - 24) / 72.0f);
                    for (double base = std::floor (c.offset / L) * L; base < c.offset + c.length; base += L) {
                        const double t = base + nt;
                        if (t < c.offset || t >= c.offset + c.length) continue;
                        g.fillRect ((float) beatToX (c.start + t - c.offset), y, 2.0f, 2.0f);
                    }
                }
            }
            g.setColour (th::ink); g.setFont (font (10.5f, 3));
            g.drawText ((p.name.isNotEmpty() ? p.name : keyName (c.key)) + (c.muted ? "  (muted)" : ""), r.reduced (5, 1).withHeight (13).toNearestInt(), Justification::centredLeft, true);
            if (c.selected) { g.setColour (th::amber); g.drawRoundedRectangle (r.expanded (1), 4, 2.0f); }
        }
        if (dropBeat >= 0) { g.setColour (th::green); g.fillRect ((float) beatToX (dropBeat) - 1, (float) rulerH, 2.0f, (float) getHeight()); }
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
        if (e.y < rulerH || i < 0) cursorBeat = snapBeat (xToBeat (e.x), e.mods.isAltDown());
        if (e.mods.isPopupMenu()) { if (i >= 0 && ! clips[(size_t) i].selected) { for (auto& c : clips) c.selected = false; clips[(size_t) i].selected = true; } menu (i); repaint(); return; }
        if (i < 0) {
            if (! e.mods.isShiftDown()) for (auto& c : clips) c.selected = false;
            mode = Band; downPos = e.position; repaint(); return;
        }
        auto& c = clips[(size_t) i];
        if (e.getNumberOfClicks() > 1) { st.song().editKey = c.key; if (st.changed) st.changed(); return; }
        if (e.mods.isShiftDown()) c.selected = ! c.selected;
        else if (! c.selected) { for (auto& x : clips) x.selected = false; c.selected = true; }
        const auto r = clipRect (c);
        mode = e.position.x > r.getRight() - 6 ? ResizeR : (e.position.x < r.getX() + 6 ? ResizeL : Move);
        st.proc.beginEdit();
        orig = clips; anchor = i; downPos = e.position;
        copying = e.mods.isCommandDown() && mode == Move;
        repaint();
    }
    void mouseDrag (const MouseEvent& e) override
    {
        auto& clips = st.song().clips;
        if (mode == Band) {
            band = Rectangle<float> (downPos, e.position);
            for (auto& c : clips) c.selected = clipRect (c).intersects (band) || (e.mods.isShiftDown() && c.selected);
            repaint(); return;
        }
        if (mode == None || anchor < 0) return;
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
        st.proc.publish(); repaint();
    }
    void mouseUp (const MouseEvent&) override
    {
        if (mode == Move || mode == ResizeL || mode == ResizeR) st.proc.endEdit (copying ? "Copy chunks" : mode == Move ? "Move chunks" : "Chunk length");
        mode = None; band = {}; anchor = -1; repaint();
    }
    void mouseMove (const MouseEvent& e) override
    {
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
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("key:"); }
    void itemDragMove (const SourceDetails& d) override { dropBeat = snapBeat (xToBeat (d.localPosition.x), false); repaint(); }
    void itemDragExit (const SourceDetails&) override { dropBeat = -1; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        const int k = d.description.toString().fromFirstOccurrenceOf ("key:", false, false).getIntValue();
        const double at = snapBeat (xToBeat (d.localPosition.x), false);
        dropBeat = -1;
        const double len = jmax (1.0, st.song().keys[(size_t) jlimit (0, 127, k)].lengthTicks() / (double) PPQ);
        st.applySong ("Add chunk", [k, at, len] (Song& s) { for (auto& c : s.clips) c.selected = false; Clip c; c.start = at; c.length = len; c.key = k; c.selected = true; s.clips.push_back (c); });
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
    void deleteSelected() { st.applySong ("Delete chunks", [] (Song& s) { s.clips.erase (std::remove_if (s.clips.begin(), s.clips.end(), [] (const Clip& c) { return c.selected; }), s.clips.end()); }); }
    void muteSelected() { st.applySong ("Mute chunks", [] (Song& s) { for (auto& c : s.clips) if (c.selected) c.muted = ! c.muted; }); }
    void selectAll() { for (auto& c : st.song().clips) c.selected = true; repaint(); }
    void copy() { clipboard.clear(); double lo = 1e9; for (auto& c : st.song().clips) if (c.selected) { clipboard.push_back (c); lo = jmin (lo, c.start); } for (auto& c : clipboard) c.start -= lo; }
    void paste() { auto clip = clipboard; const double at = cursorBeat; st.applySong ("Paste chunks", [clip, at] (Song& s) { for (auto& c : s.clips) c.selected = false; for (auto c : clip) { c.start += at; c.selected = true; s.clips.push_back (c); } }); }
    void makeUnique()
    {
        st.applySong ("Make unique", [] (Song& s) {
            for (auto& c : s.clips) if (c.selected) {
                int free = -1; for (int k = s.settings.zoneLow; k <= s.settings.zoneHigh; ++k) if (s.keys[(size_t) k].isEmpty() && k != s.settings.resolveStop()) { free = k; break; }
                if (free < 0) return;
                s.keys[(size_t) free] = s.keys[(size_t) c.key];
                s.keys[(size_t) free].name = s.keys[(size_t) c.key].name + " (unique)";
                c.key = free;
            }
        });
    }
    bool anySelected() const { for (auto& c : st.song().clips) if (c.selected) return true; return false; }

private:
    enum Mode { None, Move, ResizeL, ResizeR, Band };
    EditState& st;
    Mode mode = None; Point<float> downPos; std::vector<Clip> orig, clipboard; int anchor = -1; bool copying = false;
    Rectangle<float> band; double dropBeat = -1;

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
            if (cursorBeat > c.start + 1e-6 && cursorBeat < c.start + c.length - 1e-6) m.addItem (3, "Split at the cursor  (Alt+X)");
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
        setupButton (*this, onBtn, "Song plays", true, "On: the song lane plays, with Studio One's transport or the Play button here. Off: your keys play patterns as usual");
        onBtn.onClick = [this] { const bool on = onBtn.getToggleState(); st.applySong ("Song mode", [on] (Song& s) { s.songMode = on; }); };
        setupButton (*this, play, "Play"); play.onClick = [this] { if (! st.song().songMode) st.applySong ("Song mode", [] (Song& s) { s.songMode = true; }); st.proc.uiSongPlay (true, lane.cursorBeat); };
        setupButton (*this, stop, "Stop"); stop.onClick = [this] { st.proc.uiSongPlay (false, 0); };
        setupCombo (*this, snap, { "Snap: bar", "Snap: beat", "Snap: 1/8", "Snap: 1/16", "Snap: off" }, 1, "Chunks also click onto each other's edges. Alt while dragging = free");
        snap.setSelectedItemIndex (0, dontSendNotification);
        snap.onChange = [this] { lane.snap = snap.getSelectedItemIndex(); };
        dragOut.setButtonText ("Song MIDI"); dragOut.setSize (fitW ("Song MIDI"), btnH); dragOut.setComponentID ("Song MIDI"); addAndMakeVisible (dragOut);
        dragOut.setTooltip ("Drag the whole song lane out as one MIDI file");
    }
    void refresh() { onBtn.setToggleState (st.song().songMode, dontSendNotification); }
    void paint (Graphics& g) override
    {
        fillArea (g, getLocalBounds(), "Lane names", th::panel);
        g.setFont (font (10.0f, 3)); g.setColour (th::faint); g.drawText ("SONG", 8, 2, 50, 14, Justification::centredLeft);
        g.setColour (th::line); g.drawVerticalLine (getWidth() - 1, 0, (float) getHeight());
    }
    void resized() override
    {
        auto a = getLocalBounds().reduced (6, 3);
        auto r1 = a.removeFromTop (btnH); r1.removeFromLeft (38);
        onBtn.setBounds (r1.removeFromLeft (onBtn.getWidth())); r1.removeFromLeft (4); dragOut.setBounds (r1.removeFromLeft (jmin (r1.getWidth(), dragOut.getWidth())));
        a.removeFromTop (4);
        auto r2 = a.removeFromTop (btnH);
        play.setBounds (r2.removeFromLeft (play.getWidth())); r2.removeFromLeft (4); stop.setBounds (r2.removeFromLeft (stop.getWidth())); r2.removeFromLeft (4);
        snap.setBounds (r2);
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
            auto f = dir.getChildFile ("Drum Player Song.mid");
            writeSongMidi (st->song(), f, st->song().settings.outChannel);
            DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [this] { dragging = false; });
        }
    };
    EditState& st; SongLane& lane;
    TextButton onBtn, play, stop; ComboBox snap;
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
    QuantizePage (EditState& e) : Page ("QUANTIZE"), st (e)
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
        setupButton (*this, keepSwing, "Keep the swing", true, "Snap to the swung grid (uses the current swing amount) so the groove keeps its swing");
        setupButton (*this, selOnly, "Only selected notes", true); selOnly.setToggleState (true, dontSendNotification);
        setupLabel (*this, lRec, "While recording");
        StringArray rq { "Off (as played)" }; for (auto& v : noteValues) rq.add (v.name);
        setupCombo (*this, recQ, rq);
        recQ.onChange = [this] { st.song().settings.recQuantize = recQ.getSelectedItemIndex(); st.publish(); };
        setupButton (*this, apply, "Quantize now  (Q)");
        apply.onClick = [this] { run(); };
        setupLabel (*this, help, "Catch: with 1/16 and 25 %, triplets and faster notes are left alone. Dual: 16ths snap to 16ths, triplets to triplets.", 10.5f, th::faint);
        help.setJustificationType (Justification::topLeft);
    }
    QuantOptions options() const
    {
        QuantOptions o;
        o.valueIndex = grid.getSelectedItemIndex(); o.mode = (QuantMode) mode.getSelectedItemIndex();
        o.catchPct = catchR.getValue(); o.strength = strength.getValue(); o.keepSwing = keepSwing.getToggleState();
        o.selectedOnly = selOnly.getToggleState();
        o.swingPct = st.proc.apvts.getRawParameterValue (ids::swing)->load();
        return o;
    }
    void run() { auto o = options(); st.apply ("Quantize", [o] (Pattern& p) { quantize (p, o); }); }
    void refresh() override { recQ.setSelectedItemIndex (st.song().settings.recQuantize, dontSendNotification); }
    void resized() override
    {
        auto a = content();
        grid.setBounds (row (a, lGrid).removeFromLeft (90));
        mode.setBounds (row (a, lMode));
        catchR.setBounds (row (a, lCatch));
        strength.setBounds (row (a, lStr));
        auto r = a.removeFromTop (28); a.removeFromTop (4);
        keepSwing.setBounds (r.removeFromLeft (keepSwing.getWidth())); r.removeFromLeft (8); selOnly.setBounds (r.removeFromLeft (selOnly.getWidth()));
        apply.setBounds (a.removeFromTop (28).removeFromLeft (apply.getWidth()).reduced (0, 3)); a.removeFromTop (6);
        recQ.setBounds (row (a, lRec).removeFromLeft (140));
        help.setBounds (a.removeFromTop (40));
    }
    ComboBox grid;
private:
    EditState& st;
    Label lGrid, lMode, lCatch, lStr, lRec, help;
    ComboBox mode, recQ;
    FineSlider catchR, strength;
    TextButton keepSwing, selOnly, apply;
};

// ---------------------------------------------------------------- humanize page
class HumanizePage : public Page
{
public:
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
        rGhost.onClick = [this] { const int g = st.grid >= 0 ? st.grid : 3; st.apply ("Add ghosts", [g] (Pattern& p) { std::mt19937 r ((uint32) Time::getMillisecondCounter()); addGhosts (p, g, 30, r); }); };
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
        setupButton (*this, useMine, "Use my colours", true);
        useMine.onClick = [this] { auto& e = el(); if (useMine.getToggleState() && ! e.custom) seed (e); e.custom = useMine.getToggleState(); changed(); };
        for (int i = 0; i < 3; ++i) {
            auto* sw = swatches.add (new Swatch (i == 0 ? "Top" : i == 1 ? "Middle" : "Bottom"));
            addAndMakeVisible (sw);
            sw->onClick = [this, i, sw] { pick (i, sw); };
        }
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
        list.setBounds (a.removeFromLeft (200)); a.removeFromLeft (12);
        name.setBounds (a.removeFromTop (24)); a.removeFromTop (2);
        useMine.setBounds (a.removeFromTop (26).removeFromLeft (useMine.getWidth()).reduced (0, 2)); a.removeFromTop (6);
        auto sw = a.removeFromTop (54);
        for (auto* s2 : swatches) { s2->setBounds (sw.removeFromLeft (62)); sw.removeFromLeft (8); }
        a.removeFromTop (8);
        auto r = a.removeFromTop (26); d3.setBounds (r.removeFromLeft (d3.getWidth()).reduced (0, 2)); r.removeFromLeft (10);
        lDepth.setBounds (r.removeFromLeft (44)); depth.setBounds (r);
        a.removeFromTop (6);
        r = a.removeFromTop (26); lFont.setBounds (r.removeFromLeft (62)); fontSz.setBounds (r);
        a.removeFromTop (10);
        r = a.removeFromTop (26); resetOne.setBounds (r.removeFromLeft (resetOne.getWidth()).reduced (0, 2)); r.removeFromLeft (8); resetAll.setBounds (r.removeFromLeft (resetAll.getWidth()).reduced (0, 2));
        a.removeFromTop (6);
        r = a.removeFromTop (26); saveLook.setBounds (r.removeFromLeft (saveLook.getWidth()).reduced (0, 2)); r.removeFromLeft (8); loadLook.setBounds (r.removeFromLeft (loadLook.getWidth()).reduced (0, 2));
        a.removeFromTop (8); help.setBounds (a.removeFromTop (58));
    }
private:
    struct Swatch : public Component
    {
        explicit Swatch (String t) : title (std::move (t)) {}
        std::function<void()> onClick; Colour colour; bool enabled = true; String title;
        void paint (Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (1).withTrimmedBottom (16);
            g.setColour (colour.withMultipliedAlpha (enabled ? 1.0f : 0.3f)); g.fillRoundedRectangle (r, 5);
            g.setColour (th::line); g.drawRoundedRectangle (r, 5, 1.0f);
            g.setFont (font (10.5f, 2)); g.setColour (th::dim);
            g.drawText (title, getLocalBounds().removeFromBottom (14), Justification::centred);
        }
        void mouseUp (const MouseEvent&) override { if (onClick) onClick(); }
    };
    struct Picker : public ColourSelector, private ChangeListener
    {
        std::function<void (Colour)> onColour;
        Picker() : ColourSelector (ColourSelector::showColourAtTop | ColourSelector::showSliders | ColourSelector::showColourspace | ColourSelector::showAlphaChannel)
        { setSize (300, 320); addChangeListener (this); }
        void changeListenerCallback (ChangeBroadcaster*) override { if (onColour) onColour (getCurrentColour()); }
    };

    ListBox list; Label name, lDepth, lFont, help;
    TextButton useMine, d3, resetOne, resetAll, saveLook, loadLook;
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
        d3.setToggleState (e->threeD, dontSendNotification);
        depth.setValue (e->depth, dontSendNotification); fontSz.setValue (e->fontSize, dontSendNotification);
        loading = false;
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
        const bool custom = Theme::get().findCustom (ids[row]) != nullptr || (Theme::get().find (ids[row]) && Theme::get().find (ids[row])->fontSize > 0);
        g.setFont (font (11.5f, custom ? 3 : 1)); g.setColour (custom ? th::amber : th::text);
        g.drawText (items[row], 8, 0, w - 12, h, Justification::centredLeft, true);
    }
    void selectedRowsChanged (int) override { show(); }
};

// ---------------------------------------------------------------- settings page
class SettingsPage : public Page
{
public:
    SettingsPage (EditState& e) : Page ("SETTINGS"), st (e)
    {
        StringArray ch; for (int c = 1; c <= 16; ++c) ch.add ("Channel " + String (c) + (c == 10 ? "  (GM drums)" : ""));
        setupLabel (*this, lOut, "MIDI out"); setupCombo (*this, out, ch);
        StringArray keys; for (int n = 0; n < 128; ++n) keys.add (keyName (n) + "  (" + String (n) + ")");
        setupLabel (*this, lZone, "Pattern keys"); setupCombo (*this, zLo, keys); setupCombo (*this, zHi, keys);
        setupLabel (*this, lStart, "Start / switch on"); setupCombo (*this, start, { "Immediately", "Next beat", "Next bar", "End of pattern", "At once, in time (keeps the bar)" });
        setupLabel (*this, lMode, "Keys"); setupCombo (*this, mode, { "Latch (press again to stop)", "Hold (release to stop)" });
        StringArray stopKeys { "None" }; for (int n = 0; n < 12; ++n) stopKeys.add (keyName (n) + "  (" + String (n) + ")");
        StringArray chans { "Any channel" }; for (int c = 1; c <= 16; ++c) chans.add ("Channel " + String (c));
        setupLabel (*this, lTrig, "Pattern keys listen on"); setupCombo (*this, trigCh, chans, 1, "Give a second controller its own channel to use it only for patterns");
        setupLabel (*this, lPlayCh, "Drum pads / live notes on"); setupCombo (*this, playCh, chans, 1, "Notes on this channel are always played and recorded, never pattern keys");
        setupButton (*this, mirror, "Pattern keys in every octave", true, "The pattern keys repeat up the whole keyboard, so any octave works. Use separate channels for live drums then");
        setupLabel (*this, lStop, "Stop key"); setupCombo (*this, stopKey, stopKeys);
        setupLabel (*this, lSplit, "Cut dropped grooves every");
        setupCombo (*this, split, { "Keep whole", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars" });
        setupButton (*this, hostStop, "Stop when Studio One stops", true);
        setupButton (*this, thru, "Pass other notes through", true, "Notes outside the pattern keys go straight to the drum plugin, so you can play along");
        setupButton (*this, fills, "Dropped fills to black keys", true, "When several files are dropped, files with 'fill' in the name go to black keys");
        setupButton (*this, remember, "Open the way I left it", true, "New Drum Player instances start with your last patterns and settings");
        setupLabel (*this, help, "In Studio One: put Drum Player on an instrument track, then on the drum plugin's track choose Drum Player as the input.", 10.5f, th::faint);
        help.setJustificationType (Justification::topLeft);
        setupButton (*this, tabGeneral, "General", true); setupButton (*this, tabLook, "Look", true);
        tabGeneral.setRadioGroupId (91); tabLook.setRadioGroupId (91); tabGeneral.setToggleState (true, dontSendNotification);
        tabGeneral.onClick = [this] { showTab (false); };
        tabLook.onClick = [this] { showTab (true); };
        addChildComponent (look);
        auto commit = [this] {
            if (loading) return;
            st.applySong ("Settings", [this] (Song& s) {
                auto& o = s.settings;
                o.outChannel = out.getSelectedItemIndex() + 1;
                o.zoneLow = zLo.getSelectedItemIndex(); o.zoneHigh = jmax (o.zoneLow, zHi.getSelectedItemIndex());
                o.startQuant = (StartQuant) start.getSelectedItemIndex(); o.latch = mode.getSelectedItemIndex() == 0;
                { const int si = stopKey.getSelectedItemIndex(); o.stopNote = si <= 0 ? -1 : si - 1; }
                o.triggerChannel = trigCh.getSelectedItemIndex(); o.playChannel = playCh.getSelectedItemIndex();
                o.mirrorOctaves = mirror.getToggleState();
                static const int sb[] = { 0, 1, 2, 4, 8, 16 }; o.splitBars = sb[jlimit (0, 5, split.getSelectedItemIndex())];
                o.stopWithHost = hostStop.getToggleState(); o.passThrough = thru.getToggleState();
                o.fillsToBlack = fills.getToggleState(); o.rememberLast = remember.getToggleState();
            });
        };
        for (auto* c : { &out, &zLo, &zHi, &start, &mode, &stopKey, &split, &trigCh, &playCh }) c->onChange = commit;
        for (auto* b : { &hostStop, &thru, &fills, &remember, &mirror }) b->onClick = commit;
    }
    LookEditor look;
    void showTab (bool lookTab)
    {
        for (int i = 0; i < getNumChildComponents(); ++i) {
            auto* c = getChildComponent (i);
            if (c == &tabGeneral || c == &tabLook) continue;
            c->setVisible (c == &look ? lookTab : ! lookTab);
        }
        if (lookTab) look.refreshList();
        title = lookTab ? "SETTINGS  ·  LOOK" : "SETTINGS";
        repaint();
    }
    void refresh() override
    {
        loading = true;
        const auto& o = st.song().settings;
        out.setSelectedItemIndex (o.outChannel - 1, dontSendNotification);
        zLo.setSelectedItemIndex (o.zoneLow, dontSendNotification); zHi.setSelectedItemIndex (o.zoneHigh, dontSendNotification);
        start.setSelectedItemIndex ((int) o.startQuant, dontSendNotification); mode.setSelectedItemIndex (o.latch ? 0 : 1, dontSendNotification);
        stopKey.setSelectedItemIndex (o.stopNote >= 0 && o.stopNote < 12 ? o.stopNote + 1 : 0, dontSendNotification);
        trigCh.setSelectedItemIndex (o.triggerChannel, dontSendNotification); playCh.setSelectedItemIndex (o.playChannel, dontSendNotification);
        mirror.setToggleState (o.mirrorOctaves, dontSendNotification);
        const int sb = o.splitBars; split.setSelectedItemIndex (sb <= 0 ? 0 : sb == 1 ? 1 : sb == 2 ? 2 : sb <= 4 ? 3 : sb <= 8 ? 4 : 5, dontSendNotification);
        hostStop.setToggleState (o.stopWithHost, dontSendNotification); thru.setToggleState (o.passThrough, dontSendNotification);
        fills.setToggleState (o.fillsToBlack, dontSendNotification); remember.setToggleState (o.rememberLast, dontSendNotification);
        loading = false;
    }
    void resized() override
    {
        auto a = content();
        auto tabs = a.removeFromTop (26);
        tabGeneral.setBounds (tabs.removeFromLeft (tabGeneral.getWidth()).reduced (0, 2)); tabs.removeFromLeft (6);
        tabLook.setBounds (tabs.removeFromLeft (tabLook.getWidth()).reduced (0, 2));
        a.removeFromTop (8);
        look.setBounds (a);
        const int lw = 170;
        out.setBounds (row (a, lOut, lw).removeFromLeft (170));
        auto z = row (a, lZone, lw); zLo.setBounds (z.removeFromLeft (110)); z.removeFromLeft (8); zHi.setBounds (z.removeFromLeft (110));
        start.setBounds (row (a, lStart, lw).removeFromLeft (170));
        mode.setBounds (row (a, lMode, lw).removeFromLeft (210));
        stopKey.setBounds (row (a, lStop, lw).removeFromLeft (170));
        trigCh.setBounds (row (a, lTrig, lw).removeFromLeft (130));
        playCh.setBounds (row (a, lPlayCh, lw).removeFromLeft (130));
        split.setBounds (row (a, lSplit, lw).removeFromLeft (120));
        a.removeFromTop (4);
        mirror.setVisible (false);
        for (auto* b : { &hostStop, &thru, &fills, &remember }) { b->setBounds (a.removeFromTop (26).removeFromLeft (b->getWidth()).reduced (0, 2)); a.removeFromTop (4); }
        help.setBounds (a.removeFromTop (40));
    }
private:
    EditState& st; bool loading = false;
    Label lOut, lZone, lStart, lMode, lStop, lSplit, help, lTrig, lPlayCh;
    ComboBox out, zLo, zHi, start, mode, stopKey, split, trigCh, playCh;
    TextButton hostStop, thru, fills, remember, mirror, tabGeneral, tabLook;
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


// ---------------------------------------------------------------- recording page
class RecordPage : public Page
{
public:
    RecordPage (EditState& e) : Page ("RECORDING"), st (e)
    {
        setupLabel (*this, lCount, "Count-in"); setupCombo (*this, count, { "Off", "1 bar", "2 bars" });
        setupLabel (*this, lMetro, "Click"); setupCombo (*this, metro, { "Off", "Count-in only", "While recording", "Whenever a pattern plays" });
        setupLabel (*this, lLevel, "Click volume"); setupSlider (*this, level); level.setRange (0, 100, 1); level.setTextValueSuffix (" %");
        setupLabel (*this, lMode, "Mode"); setupCombo (*this, mode, { "Overdub: add to the pattern", "Replace: a drum you play loses its old notes" });
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
            st.proc.publish();
        };
        for (auto* c : { &count, &metro, &mode, &stopAfter, &recQ, &vel }) c->onChange = commit;
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
        loading = false;
    }
    void resized() override
    {
        auto a = content(); const int lw = 160;
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
    Label lCount, lMetro, lLevel, lMode, lStop, lQ, lVel, help;
    ComboBox count, metro, mode, stopAfter, recQ, vel;
    FineSlider level;
};

} // namespace dp
