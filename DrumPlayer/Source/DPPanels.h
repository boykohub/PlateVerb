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
        drawCard (g, getLocalBounds().toFloat().reduced (2), th::panel, 12);
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
        setTooltip ("Click: edit.  Double-click: play.  Drag onto another key: swap.  Drag to the tray: park.  Drag out of the window: MIDI file.  Drop MIDI files here.");
    }
    std::function<void (const StringArray&, int key)> onFiles;
    std::function<void (int key)> onKeyMenu;
    int dropHover = -1;

    void paint (Graphics& g) override
    {
        auto& s = st.song();
        const int lo = s.settings.zoneLow, hi = s.settings.zoneHigh;
        const int play = st.proc.playingKey.load(), queued = st.proc.queuedKey.load();
        const bool blink = (Time::getMillisecondCounter() / 250) % 2 == 0;
        g.fillAll (th::bg);
        // white keys
        for (int k = lo; k <= hi; ++k) {
            if (isBlackKey (k)) continue;
            auto r = keyRect (k);
            const auto& p = s.keys[(size_t) k];
            g.setGradientFill (ColourGradient (Colour (0xffe9eaee), r.getTopLeft(), Colour (0xffb9bcc4), r.getBottomLeft(), false));
            g.fillRoundedRectangle (r.reduced (0.5f, 0), 3);
            g.setColour (Colours::black.withAlpha (0.55f)); g.drawRoundedRectangle (r.reduced (0.5f, 0), 3, 1.0f);
            drawTile (g, r.withTrimmedTop (r.getHeight() * 0.56f).reduced (2.5f, 3), k, p, false);
            decorate (g, r, k, play, queued, blink);
        }
        // black keys on top
        for (int k = lo; k <= hi; ++k) {
            if (! isBlackKey (k)) continue;
            auto r = keyRect (k);
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
    }

    // ------------------------------------------------------------ geometry
    int numWhite() const { int n = 0; for (int k = st.song().settings.zoneLow; k <= st.song().settings.zoneHigh; ++k) if (! isBlackKey (k)) ++n; return jmax (1, n); }
    Rectangle<float> keyRect (int k) const
    {
        const int lo = st.song().settings.zoneLow;
        const float ww = (float) getWidth() / numWhite();
        int wi = 0; for (int i = lo; i < k; ++i) if (! isBlackKey (i)) ++wi;
        if (! isBlackKey (k)) return { wi * ww, 0.0f, ww, (float) getHeight() };
        return { wi * ww - ww * 0.31f, 0.0f, ww * 0.62f, getHeight() * 0.58f };
    }
    int keyAt (Point<float> pt) const
    {
        const auto& s = st.song().settings;
        for (int k = s.zoneLow; k <= s.zoneHigh; ++k) if (isBlackKey (k) && keyRect (k).contains (pt)) return k;
        for (int k = s.zoneLow; k <= s.zoneHigh; ++k) if (! isBlackKey (k) && keyRect (k).contains (pt)) return k;
        return -1;
    }

    // ------------------------------------------------------------ mouse
    void mouseDown (const MouseEvent& e) override
    {
        const int k = keyAt (e.position);
        if (k < 0) return;
        if (e.mods.isPopupMenu()) { if (onKeyMenu) onKeyMenu (k); return; }
        st.song().editKey = k;
        st.focusLane = -1;
        if (e.getNumberOfClicks() > 1 || e.mods.isAltDown()) st.proc.uiTrigger (k);
        if (st.changed) st.changed();
        dragKey = k;
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (dragKey < 0 || e.getDistanceFromDragStart() < 6) return;
        if (st.song().keys[(size_t) dragKey].isEmpty()) return;
        if (auto* c = DragAndDropContainer::findParentDragContainerFor (this))
            if (! c->isDragAndDropActive()) c->startDragging ("key:" + String (dragKey), this);
    }
    void mouseUp (const MouseEvent& e) override
    {
        if (! st.song().settings.latch && dragKey >= 0 && (e.getNumberOfClicks() > 1 || e.mods.isAltDown())) st.proc.uiRelease (dragKey);
        dragKey = -1;
    }

    // ------------------------------------------------------------ internal drag (swap / unpark)
    bool isInterestedInDragSource (const SourceDetails& d) override { const auto s = d.description.toString(); return s.startsWith ("key:") || s.startsWith ("park:"); }
    void itemDragMove (const SourceDetails& d) override { dropHover = keyAt (d.localPosition.toFloat()); repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHover = -1; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        const int to = keyAt (d.localPosition.toFloat());
        dropHover = -1;
        if (to < 0) { repaint(); return; }
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
    int dragKey = -1;

    void drawTile (Graphics& g, Rectangle<float> r, int k, const Pattern& p, bool black)
    {
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
    }
};

// ---------------------------------------------------------------- parking tray: hold patterns aside while rearranging
class ParkTray : public Component, public DragAndDropTarget, public SettableTooltipClient
{
public:
    explicit ParkTray (EditState& e) : st (e) { setTooltip ("Parking tray: drag a key here to set its pattern aside, drag it back onto any key"); }
    void paint (Graphics& g) override
    {
        drawCard (g, getLocalBounds().toFloat().reduced (1), th::panel, 8);
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
        if (auto* c = DragAndDropContainer::findParentDragContainerFor (this)) if (! c->isDragAndDropActive()) c->startDragging ("park:" + String (dragSlot), this);
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
        for (int i = 0; i < 4; ++i) {
            auto* b = chainBtns.add (new TextButton());
            setupButton (*this, *b, "Chain " + String::charToString ((juce_wchar) ('A' + i)), false);
            b->setRadioGroupId (77); b->setClickingTogglesState (true);
            b->onClick = [this, i] { current = i; refresh(); };
        }
        chainBtns[0]->setToggleState (true, dontSendNotification);
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
        auto& c = st.song().chains[(size_t) current];
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
        for (auto* b : chainBtns) { b->setBounds (r.removeFromLeft (b->getWidth()).reduced (0, 3)); r.removeFromLeft (5); }
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
    OwnedArray<TextButton> chainBtns;
    TextButton add, clear, loop, play, stop;
    DragMidiButton dragOut;
    Label lTrig, help; ComboBox trig;
    Viewport view; Component list; OwnedArray<StepRow> rows;
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
        setupLabel (*this, lStart, "Start / switch on"); setupCombo (*this, start, { "Immediately", "Next beat", "Next bar", "End of pattern" });
        setupLabel (*this, lMode, "Keys"); setupCombo (*this, mode, { "Latch (press again to stop)", "Hold (release to stop)" });
        StringArray stopKeys { "None" }; stopKeys.addArray (keys);
        setupLabel (*this, lStop, "Stop key"); setupCombo (*this, stopKey, stopKeys);
        setupLabel (*this, lSplit, "Cut dropped grooves every");
        setupCombo (*this, split, { "Keep whole", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars" });
        setupButton (*this, hostStop, "Stop when Studio One stops", true);
        setupButton (*this, thru, "Pass other notes through", true, "Notes outside the pattern keys go straight to the drum plugin, so you can play along");
        setupButton (*this, fills, "Dropped fills to black keys", true, "When several files are dropped, files with 'fill' in the name go to black keys");
        setupButton (*this, remember, "Open the way I left it", true, "New Drum Player instances start with your last patterns and settings");
        setupLabel (*this, help, "In Studio One: put Drum Player on an instrument track, then on the drum plugin's track choose Drum Player as the input.", 10.5f, th::faint);
        help.setJustificationType (Justification::topLeft);
        auto commit = [this] {
            if (loading) return;
            st.applySong ("Settings", [this] (Song& s) {
                auto& o = s.settings;
                o.outChannel = out.getSelectedItemIndex() + 1;
                o.zoneLow = zLo.getSelectedItemIndex(); o.zoneHigh = jmax (o.zoneLow, zHi.getSelectedItemIndex());
                o.startQuant = (StartQuant) start.getSelectedItemIndex(); o.latch = mode.getSelectedItemIndex() == 0;
                o.stopNote = stopKey.getSelectedItemIndex() - 1;
                static const int sb[] = { 0, 1, 2, 4, 8, 16 }; o.splitBars = sb[jlimit (0, 5, split.getSelectedItemIndex())];
                o.stopWithHost = hostStop.getToggleState(); o.passThrough = thru.getToggleState();
                o.fillsToBlack = fills.getToggleState(); o.rememberLast = remember.getToggleState();
            });
        };
        for (auto* c : { &out, &zLo, &zHi, &start, &mode, &stopKey, &split }) c->onChange = commit;
        for (auto* b : { &hostStop, &thru, &fills, &remember }) b->onClick = commit;
    }
    void refresh() override
    {
        loading = true;
        const auto& o = st.song().settings;
        out.setSelectedItemIndex (o.outChannel - 1, dontSendNotification);
        zLo.setSelectedItemIndex (o.zoneLow, dontSendNotification); zHi.setSelectedItemIndex (o.zoneHigh, dontSendNotification);
        start.setSelectedItemIndex ((int) o.startQuant, dontSendNotification); mode.setSelectedItemIndex (o.latch ? 0 : 1, dontSendNotification);
        stopKey.setSelectedItemIndex (o.stopNote + 1, dontSendNotification);
        const int sb = o.splitBars; split.setSelectedItemIndex (sb <= 0 ? 0 : sb == 1 ? 1 : sb == 2 ? 2 : sb <= 4 ? 3 : sb <= 8 ? 4 : 5, dontSendNotification);
        hostStop.setToggleState (o.stopWithHost, dontSendNotification); thru.setToggleState (o.passThrough, dontSendNotification);
        fills.setToggleState (o.fillsToBlack, dontSendNotification); remember.setToggleState (o.rememberLast, dontSendNotification);
        loading = false;
    }
    void resized() override
    {
        auto a = content();
        const int lw = 170;
        out.setBounds (row (a, lOut, lw).removeFromLeft (170));
        auto z = row (a, lZone, lw); zLo.setBounds (z.removeFromLeft (110)); z.removeFromLeft (8); zHi.setBounds (z.removeFromLeft (110));
        start.setBounds (row (a, lStart, lw).removeFromLeft (170));
        mode.setBounds (row (a, lMode, lw).removeFromLeft (210));
        stopKey.setBounds (row (a, lStop, lw).removeFromLeft (120));
        split.setBounds (row (a, lSplit, lw).removeFromLeft (120));
        a.removeFromTop (4);
        for (auto* b : { &hostStop, &thru, &fills, &remember }) { b->setBounds (a.removeFromTop (26).removeFromLeft (b->getWidth()).reduced (0, 2)); a.removeFromTop (4); }
        help.setBounds (a.removeFromTop (40));
    }
private:
    EditState& st; bool loading = false;
    Label lOut, lZone, lStart, lMode, lStop, lSplit, help;
    ComboBox out, zLo, zHi, start, mode, stopKey, split;
    TextButton hostStop, thru, fills, remember;
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

} // namespace dp
