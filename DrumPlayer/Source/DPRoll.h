// Drum Player: the piano-roll drum editor (lane headers, ruler, note grid with adaptive coloured grids,
// polyrhythm lane lengths, velocity / probability lane) and the editing tools.
#pragma once
#include "DPLook.h"

namespace dp {

enum class Tool { Select = 0, Draw, Erase, Split, Paint, Mute, Line };
inline const char* toolName (Tool t) { static const char* n[] = { "Select", "Draw", "Erase", "Split", "Paint", "Mute", "Line" }; return n[(int) t]; }

// shared editor state (one per editor window)
struct EditState
{
    explicit EditState (DrumPlayerProcessor& p) : proc (p) {}
    DrumPlayerProcessor& proc;
    Tool tool = Tool::Select;
    int grid = 3;                    // note value index, -1 = no snap
    int focusLane = -1;              // note number shown in the velocity lane (-1 = all)
    bool usedOnly = false;
    bool probMode = false;           // velocity lane shows probability
    float pxPerBeat = 80.0f;
    int scrollX = 0, scrollY = 0;
    int cursorTick = 0;              // where Paste goes
    int defaultVel = 100;
    std::vector<Note> clipboard;
    static constexpr int laneH = 22;

    std::function<void()> changed;                    // repaint + refresh controls
    std::function<void (const String&)> toast;
    std::function<void (int)> splitAt;

    Song& song() { return proc.edit; }
    Pattern& pat() { return proc.edit.keys[(size_t) jlimit (0, 127, proc.edit.editKey)]; }
    int gridTicks() const { return grid < 0 ? 1 : noteValues[grid].ticks; }
    int snapRound (double t) const { const int g = gridTicks(); return jmax (0, (int) std::round (t / g) * g); }
    int snapFloor (double t) const { const int g = gridTicks(); return jmax (0, (int) std::floor (t / g) * g); }
    float tickToX (double t) const { return (float) (t / PPQ * pxPerBeat) - (float) scrollX; }
    double xToTick (float x) const { return (x + scrollX) / pxPerBeat * PPQ; }

    std::vector<int> lanes()
    {
        std::vector<int> v;
        auto& p = pat();
        std::set<int> used; for (auto& n : p.notes) used.insert (n.note);
        for (auto& l : song().lanes) if (! usedOnly || used.count (l.note) || l.note == focusLane) v.push_back (l.note);
        return v;
    }
    int laneIndex (int note) { auto v = lanes(); for (int i = 0; i < (int) v.size(); ++i) if (v[(size_t) i] == note) return i; return -1; }
    Colour laneColour (int note)
    {
        int i = 0; for (auto& l : song().lanes) { if (l.note == note) break; ++i; }
        return th::pal (i);
    }
    int contentTicks()
    {
        auto& p = pat(); int m = p.lengthTicks();
        for (auto& [n, l] : p.laneLength) m = jmax (m, l);
        return m + p.beatsPerBar * PPQ;
    }
    void publish() { proc.publish(); if (changed) changed(); }
    // single-shot edit: one undo step
    void apply (const String& name, const std::function<void (Pattern&)>& f)
    {
        Song s = proc.edit;
        f (s.keys[(size_t) s.editKey]);
        s.ensureLanesForPattern (s.keys[(size_t) s.editKey]);
        proc.commit (std::move (s), name);
        if (changed) changed();
    }
    void applySong (const String& name, const std::function<void (Song&)>& f)
    {
        Song s = proc.edit; f (s); proc.commit (std::move (s), name);
        if (changed) changed();
    }
};

// ---------------------------------------------------------------- ruler (bar numbers, pattern end marker)
class Ruler : public Component, public SettableTooltipClient
{
public:
    explicit Ruler (EditState& e) : st (e) { setTooltip ("Drag the amber end marker to set the pattern length in bars"); }
    void paint (Graphics& g) override
    {
        g.fillAll (th::panel);
        auto& p = st.pat();
        const int bpb = p.beatsPerBar;
        const int total = st.contentTicks() / PPQ + 1;
        g.setFont (font (11.0f, 2));
        for (int b = 0; b <= total; ++b) {
            const float x = st.tickToX (b * PPQ);
            if (x < -40 || x > getWidth() + 10) continue;
            const bool bar = b % bpb == 0;
            g.setColour (bar ? th::dim : th::faint);
            g.drawVerticalLine ((int) x, bar ? 6.0f : 14.0f, (float) getHeight());
            if (bar) { g.setColour (th::text); g.drawText (String (b / bpb + 1), (int) x + 4, 1, 40, 14, Justification::centredLeft); }
            else if (st.pxPerBeat > 40) { g.setColour (th::faint); g.drawText (String (b / bpb + 1) + "." + String (b % bpb + 1), (int) x + 3, 8, 40, 14, Justification::centredLeft); }
        }
        // end marker
        const float ex = st.tickToX (p.lengthTicks());
        Path tri; tri.addTriangle (ex - 6, 2, ex + 6, 2, ex, 12);
        g.setColour (th::amber); g.fillPath (tri); g.drawVerticalLine ((int) ex, 2.0f, (float) getHeight());
        g.setColour (th::line); g.drawHorizontalLine (getHeight() - 1, 0, (float) getWidth());
    }
    void mouseDown (const MouseEvent& e) override
    {
        dragging = std::abs (e.x - st.tickToX (st.pat().lengthTicks())) < 10;
        if (dragging) st.proc.beginEdit();
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (! dragging) return;
        auto& p = st.pat();
        const int bar = p.beatsPerBar * PPQ;
        p.bars = jlimit (1, maxBars, (int) std::round (st.xToTick ((float) e.x) / bar));
        st.publish();
    }
    void mouseUp (const MouseEvent&) override { if (dragging) st.proc.endEdit ("Pattern length"); dragging = false; }
    void mouseMove (const MouseEvent& e) override { setMouseCursor (std::abs (e.x - st.tickToX (st.pat().lengthTicks())) < 10 ? MouseCursor::LeftRightResizeCursor : MouseCursor::NormalCursor); }
private:
    EditState& st; bool dragging = false;
};

// ---------------------------------------------------------------- lane headers
class LaneHeaders : public Component
{
public:
    explicit LaneHeaders (EditState& e) : st (e) {}
    std::function<void (int note)> preview;

    void paint (Graphics& g) override
    {
        g.fillAll (th::panel);
        auto lanes = st.lanes();
        auto& p = st.pat();
        bool anySolo = false; for (auto& l : st.song().lanes) anySolo |= l.solo;
        for (int i = 0; i < (int) lanes.size(); ++i) {
            const int y = i * EditState::laneH - st.scrollY;
            if (y < -EditState::laneH || y > getHeight()) continue;
            const int note = lanes[(size_t) i];
            const auto* lane = st.song().lane (note);
            auto r = Rectangle<int> (0, y, getWidth(), EditState::laneH);
            const bool focus = note == st.focusLane;
            const bool muted = p.laneMute.count (note) > 0 || (anySolo && (lane == nullptr || ! lane->solo));
            g.setColour (focus ? th::blue.withAlpha (0.16f) : (i % 2 ? th::panel : th::surface.withAlpha (0.55f)));
            g.fillRect (r);
            g.setColour (st.laneColour (note).withAlpha (muted ? 0.3f : 1.0f));
            g.fillRoundedRectangle (Rectangle<float> (4, (float) y + 4, 5, EditState::laneH - 8.0f), 2);
            // name + note number
            g.setFont (font (12.0f, focus ? 2 : 1));
            g.setColour (muted ? th::faint : th::text);
            g.drawText (lane != nullptr ? lane->name : gmDrumName (note), 14, y, getWidth() - 120, EditState::laneH, Justification::centredLeft, true);
            g.setFont (font (10.0f, 1)); g.setColour (th::faint);
            g.drawText (keyName (note), getWidth() - 116, y, 30, EditState::laneH, Justification::centredRight);
            // level bar
            auto lv = levelRect (y);
            g.setColour (Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (lv.toFloat(), 2);
            g.setColour (th::blue.withAlpha (0.75f)); g.fillRoundedRectangle (lv.toFloat().withWidth (lv.getWidth() * (lane != nullptr ? lane->level / 1.5f : 0.67f)), 2);
            // M S lamps
            drawLamp (g, mRect (y), "M", p.laneMute.count (note) > 0, th::red);
            drawLamp (g, sRect (y), "S", lane != nullptr && lane->solo, th::amber);
            // polyrhythm length badge
            if (p.laneLength.count (note)) {
                g.setFont (font (9.5f, 2)); g.setColour (th::amber);
                g.drawText (lengthText (p.laneLen (note)), getWidth() - 86, y, 28, EditState::laneH, Justification::centred);
            }
            g.setColour (th::line); g.drawHorizontalLine (y + EditState::laneH - 1, 0, (float) getWidth());
        }
        g.setColour (th::line); g.drawVerticalLine (getWidth() - 1, 0, (float) getHeight());
    }

    static String lengthText (int ticks)
    {
        if (ticks % PPQ == 0) return String (ticks / PPQ) + "b";
        if (ticks % 240 == 0) return String (ticks / 240) + "/16";
        if (ticks % 160 == 0) return String (ticks / 160) + "T";
        return String (ticks) + "t";
    }

    void mouseDown (const MouseEvent& e) override
    {
        const int idx = (e.y + st.scrollY) / EditState::laneH;
        auto lanes = st.lanes();
        if (idx < 0 || idx >= (int) lanes.size()) return;
        const int note = lanes[(size_t) idx];
        const int y = idx * EditState::laneH - st.scrollY;
        if (e.mods.isPopupMenu()) { laneMenu (note); return; }
        if (mRect (y).contains (e.getPosition())) {
            st.apply ("Lane mute", [note] (Pattern& p) { if (p.laneMute.count (note)) p.laneMute.erase (note); else p.laneMute.insert (note); });
            return;
        }
        if (sRect (y).contains (e.getPosition())) {
            st.applySong ("Lane solo", [note] (Song& s) { if (auto* l = s.lane (note)) l->solo = ! l->solo; });
            return;
        }
        if (levelRect (y).expanded (2, 4).contains (e.getPosition())) { levelNote = note; levelX0 = levelRect (y).getX(); st.proc.beginEdit(); setLevel (e.x); return; }
        if (e.x < 12) { if (preview) preview (note); return; }
        st.focusLane = st.focusLane == note ? -1 : note;
        if (preview && e.getNumberOfClicks() > 1) preview (note);
        if (st.changed) st.changed();
    }
    void mouseDrag (const MouseEvent& e) override { if (levelNote >= 0) setLevel (e.x); }
    void mouseUp (const MouseEvent&) override { if (levelNote >= 0) st.proc.endEdit ("Lane level"); levelNote = -1; }
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w) override { if (onWheel) onWheel (w); }
    std::function<void (const MouseWheelDetails&)> onWheel;

private:
    EditState& st;
    int levelNote = -1, levelX0 = 0;
    Rectangle<int> levelRect (int y) const { return { getWidth() - 56, y + 8, 22, 6 }; }
    Rectangle<int> mRect (int y) const { return { getWidth() - 31, y + 3, 14, 16 }; }
    Rectangle<int> sRect (int y) const { return { getWidth() - 16, y + 3, 14, 16 }; }
    static void drawLamp (Graphics& g, Rectangle<int> r, const char* t, bool on, Colour c)
    {
        g.setColour (on ? c : Colours::black.withAlpha (0.3f)); g.fillRoundedRectangle (r.toFloat(), 3);
        g.setFont (font (9.5f, 3)); g.setColour (on ? th::ink : th::faint);
        g.drawText (t, r, Justification::centred);
    }
    void setLevel (int x)
    {
        if (auto* l = st.song().lane (levelNote)) { l->level = jlimit (0.0f, 1.5f, (float) (x - levelX0) / 22.0f * 1.5f); st.publish(); }
    }
    void laneMenu (int note)
    {
        auto& p = st.pat();
        auto* lane = st.song().lane (note);
        PopupMenu m;
        m.addSectionHeader ((lane ? lane->name : gmDrumName (note)) + "  (" + keyName (note) + ", note " + String (note) + ")");
        m.addItem (1, "Rename lane...");
        m.addItem (2, "Preview sound");
        PopupMenu len;
        len.addItem (100, "Same as pattern", true, ! p.laneLength.count (note));
        const int g = st.gridTicks() > 1 ? st.gridTicks() : 240;
        for (int steps : { 3, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15, 17, 18, 20, 24, 32 })
            len.addItem (100 + steps, String (steps) + " x " + (st.grid >= 0 ? noteValues[st.grid].name : "1/16"), true, p.laneLength.count (note) && p.laneLen (note) == steps * g);
        for (int beats : { 3, 5, 6, 7, 9 }) len.addItem (200 + beats, String (beats) + " beats", true, p.laneLength.count (note) && p.laneLen (note) == beats * PPQ);
        m.addSubMenu ("Lane length (polyrhythm)", len);
        PopupMenu sw;
        const int cur = lane ? lane->swingMode : 0;
        sw.addItem (300, "Follow main swing", true, cur == 0);
        sw.addItem (301, "Straight (no swing)", true, cur == 1);
        auto names = StringArray::fromTokens (swingChoiceNames(), "|", "");
        for (int i = 0; i < names.size(); ++i) sw.addItem (302 + i, "Own value: " + names[i], true, cur == 2 + i);
        m.addSubMenu ("Swing for this lane", sw);
        PopupMenu ch;
        ch.addItem (400, "Main output channel", true, lane == nullptr || lane->channel == 0);
        for (int c = 1; c <= 16; ++c) ch.addItem (400 + c, "Channel " + String (c), true, lane != nullptr && lane->channel == c);
        m.addSubMenu ("MIDI channel", ch);
        PopupMenu pan;
        for (int v : { 0, 16, 32, 48, 64, 80, 96, 112, 127 })
            pan.addItem (500 + v, v == 64 ? "Centre" : (v < 64 ? "Left " : "Right ") + String (std::abs (v - 64) * 100 / 64) + "%", lane != nullptr && lane->channel > 0, lane != nullptr && lane->pan == v);
        m.addSubMenu ("Pan (CC10, needs own channel)", pan);
        PopupMenu remap;
        for (int n = 35; n <= 81; ++n) remap.addItem (1000 + n, String (n) + "  " + gmDrumName (n), n != note);
        m.addSubMenu ("Move notes to another drum", remap);
        m.addSeparator();
        PopupMenu add;
        for (int n = 35; n <= 81; ++n) add.addItem (2000 + n, String (n) + "  " + gmDrumName (n), st.song().lane (n) == nullptr);
        PopupMenu add2;
        for (int n = 0; n < 128; ++n) if (n < 35 || n > 81) add2.addItem (2000 + n, String (n) + "  " + keyName (n), st.song().lane (n) == nullptr);
        add.addSubMenu ("Other notes", add2);
        m.addSubMenu ("Add lane", add);
        m.addItem (3, "Remove this lane (if empty everywhere)");
        m.addItem (4, "Show only lanes used in this pattern", true, st.usedOnly);
        m.addItem (5, "Select all notes in this lane");
        m.showMenuAsync (PopupMenu::Options().withMousePosition(), [this, note] (int r) { menuResult (note, r); });
    }
    void menuResult (int note, int r)
    {
        if (r <= 0) return;
        if (r == 1) {
            auto* aw = new AlertWindow ("Rename lane", "", MessageBoxIconType::NoIcon);
            aw->addTextEditor ("n", st.song().lane (note) ? st.song().lane (note)->name : gmDrumName (note));
            aw->addButton ("OK", 1, KeyPress (KeyPress::returnKey)); aw->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
            aw->enterModalState (true, ModalCallbackFunction::create ([this, aw, note] (int res) {
                if (res == 1) { const auto t = aw->getTextEditorContents ("n"); st.applySong ("Rename lane", [note, t] (Song& s) { if (auto* l = s.lane (note)) l->name = t; }); }
            }), true);
        }
        else if (r == 2) { if (preview) preview (note); }
        else if (r == 3) {
            bool used = false; for (auto& p : st.song().keys) for (auto& n : p.notes) if (n.note == note) used = true;
            if (used) { if (st.toast) st.toast ("That lane still has notes in some patterns"); return; }
            st.applySong ("Remove lane", [note] (Song& s) { s.lanes.erase (std::remove_if (s.lanes.begin(), s.lanes.end(), [note] (const Lane& l) { return l.note == note; }), s.lanes.end()); });
        }
        else if (r == 4) { st.usedOnly = ! st.usedOnly; if (st.changed) st.changed(); }
        else if (r == 5) { auto& p = st.pat(); for (auto& n : p.notes) n.selected = n.note == note; st.publish(); }
        else if (r == 100) st.apply ("Lane length", [note] (Pattern& p) { p.laneLength.erase (note); });
        else if (r > 100 && r < 200) { const int g = st.gridTicks() > 1 ? st.gridTicks() : 240; const int steps = r - 100; st.apply ("Lane length", [note, g, steps] (Pattern& p) { p.laneLength[note] = steps * g; }); }
        else if (r > 200 && r < 300) { const int beats = r - 200; st.apply ("Lane length", [note, beats] (Pattern& p) { p.laneLength[note] = beats * PPQ; }); }
        else if (r >= 300 && r < 400) { const int mode = r - 300; st.applySong ("Lane swing", [note, mode] (Song& s) { if (auto* l = s.lane (note)) l->swingMode = mode; }); }
        else if (r >= 400 && r < 500) { const int c = r - 400; st.applySong ("Lane channel", [note, c] (Song& s) { if (auto* l = s.lane (note)) l->channel = c; }); }
        else if (r >= 500 && r < 700) { const int v = r - 500; st.applySong ("Lane pan", [note, v] (Song& s) { if (auto* l = s.lane (note)) l->pan = v; }); }
        else if (r >= 1000 && r < 1200) {
            const int to = r - 1000;
            st.apply ("Move to drum", [note, to] (Pattern& p) { for (auto& n : p.notes) if (n.note == note) n.note = to; if (p.laneMute.count (note)) { p.laneMute.erase (note); p.laneMute.insert (to); } p.sort(); });
        }
        else if (r >= 2000 && r < 2200) { const int n = r - 2000; st.applySong ("Add lane", [n] (Song& s) { s.ensureLane (n); }); }
    }
};

// ---------------------------------------------------------------- the note grid
class NoteGrid : public Component
{
public:
    explicit NoteGrid (EditState& e) : st (e) { setWantsKeyboardFocus (true); }
    std::function<void (const MouseWheelDetails&, const MouseEvent&)> onWheel;
    std::function<void (int note)> preview;
    double playTicks = -1.0;      // continuous ticks since the pattern started (for polyrhythm playheads), -1 = not playing this one

    void paint (Graphics& g) override
    {
        g.fillAll (th::bg);
        auto& p = st.pat();
        auto lanes = st.lanes();
        const int L = p.lengthTicks();
        const int bpb = p.beatsPerBar;
        const int total = st.contentTicks();
        const int rows = (int) lanes.size();
        const int gT = st.gridTicks() > 1 ? st.gridTicks() : 240;
        const float w = (float) getWidth();

        // rows
        for (int i = 0; i < rows; ++i) {
            const int y = i * EditState::laneH - st.scrollY;
            if (y < -EditState::laneH || y > getHeight()) continue;
            g.setColour (lanes[(size_t) i] == st.focusLane ? th::blue.withAlpha (0.07f) : (i % 2 ? th::bg : th::panel.withAlpha (0.7f)));
            g.fillRect (0, y, getWidth(), EditState::laneH);
        }
        // base grid
        const int t0 = jmax (0, (int) st.xToTick (0)), t1 = (int) st.xToTick (w) + 1;
        if (st.grid >= 0 && noteValues[st.grid].ticks * st.pxPerBeat / PPQ >= 4.0f) {
            g.setColour (Colours::white.withAlpha (0.045f));
            for (int t = t0 / gT * gT; t <= t1; t += gT) g.drawVerticalLine ((int) st.tickToX (t), 0, (float) getHeight());
        }
        for (int b = t0 / PPQ; b * PPQ <= t1; ++b) {
            g.setColour (b % bpb == 0 ? Colours::white.withAlpha (0.22f) : Colours::white.withAlpha (0.10f));
            g.drawVerticalLine ((int) st.tickToX (b * PPQ), 0, (float) getHeight());
        }
        // adaptive grid: under notes that sit on a finer (or triplet) value, that value's lines in its own colour
        std::map<std::pair<int, int>, int> cells;   // (row, beat) → value index
        const int baseTicks = st.grid >= 0 ? noteValues[st.grid].ticks : 240;
        for (auto& n : p.notes) {
            const int vi = detectValue (n.tick);
            if (vi < 0) continue;
            const auto& nv = noteValues[vi];
            const bool onBase = (n.tick % baseTicks) == 0;
            if (onBase && ! nv.triplet) continue;
            const int row = indexOf (lanes, n.note);
            if (row < 0) continue;
            auto key = std::make_pair (row, n.tick / PPQ);
            auto it = cells.find (key);
            if (it == cells.end() || noteValues[vi].ticks < noteValues[it->second].ticks) cells[key] = vi;
        }
        for (auto& [key, vi] : cells) {
            const int y = key.first * EditState::laneH - st.scrollY;
            const int beat = key.second;
            const float x0 = st.tickToX (beat * PPQ), x1 = st.tickToX ((beat + 1) * PPQ);
            if (x1 < 0 || x0 > w) continue;
            const auto c = th::valueColour (vi);
            g.setColour (c.withAlpha (0.07f)); g.fillRect (x0, (float) y, x1 - x0, (float) EditState::laneH);
            g.setColour (c.withAlpha (0.38f));
            for (int t = 0; t < PPQ; t += noteValues[vi].ticks) g.drawVerticalLine ((int) st.tickToX (beat * PPQ + t), (float) y + 1, (float) (y + EditState::laneH - 1));
        }
        // beyond the pattern end
        const float ex = st.tickToX (L);
        g.setColour (Colours::black.withAlpha (0.45f)); g.fillRect (ex, 0.0f, w - ex, (float) getHeight());
        g.setColour (th::amber.withAlpha (0.7f)); g.drawVerticalLine ((int) ex, 0, (float) getHeight());
        // polyrhythm lanes: shade past the lane length, show the repeats as ghosts, draw the handle
        for (int i = 0; i < rows; ++i) {
            const int note = lanes[(size_t) i];
            if (! p.laneLength.count (note)) continue;
            const int LL = p.laneLen (note);
            const int y = i * EditState::laneH - st.scrollY;
            const float lx = st.tickToX (LL);
            g.setColour (Colours::black.withAlpha (0.3f)); g.fillRect (lx, (float) y, w - lx, (float) EditState::laneH);
            for (auto& n : p.notes) if (n.note == note && n.tick < LL)
                for (int k = 1; (long) k * LL + n.tick < (long) total; ++k) {
                    const float x = st.tickToX ((double) k * LL + n.tick);
                    if (x > w) break;
                    g.setColour (st.laneColour (note).withAlpha (0.22f));
                    g.fillRoundedRectangle (x, (float) y + 5, jmax (5.0f, n.length * st.pxPerBeat / PPQ), EditState::laneH - 10.0f, 2);
                }
            g.setColour (th::amber); g.fillRoundedRectangle (lx - 2, (float) y + 2, 4, EditState::laneH - 4.0f, 2);
        }
        // notes
        g.setFont (font (9.0f, 2));
        for (auto& n : p.notes) {
            const int row = indexOf (lanes, n.note);
            if (row < 0) continue;
            auto r = noteRect (n, row);
            if (r.getRight() < 0 || r.getX() > w || r.getBottom() < 0 || r.getY() > getHeight()) continue;
            const auto c = st.laneColour (n.note);
            const float vel = n.velocity / 127.0f;
            const bool muted = n.probability == 0;
            auto body = muted ? th::faint : c.withMultipliedBrightness (0.55f + 0.45f * vel);
            g.setColour (Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (r.translated (0, 1), 3);
            g.setGradientFill (ColourGradient (body.brighter (0.25f), r.getTopLeft(), body.darker (0.2f), r.getBottomLeft(), false));
            g.fillRoundedRectangle (r, 3);
            // velocity strip
            g.setColour (Colours::black.withAlpha (0.35f)); g.fillRect (r.getX() + 2, r.getBottom() - 3, (r.getWidth() - 4), 2.0f);
            g.setColour (Colours::white.withAlpha (0.8f)); g.fillRect (r.getX() + 2, r.getBottom() - 3, (r.getWidth() - 4) * vel, 2.0f);
            if (n.probability > 0 && n.probability < 100 && r.getWidth() > 18) { g.setColour (th::ink); g.drawText (String (n.probability) + "%", r.toNearestInt(), Justification::centred); }
            if (n.selected) { g.setColour (th::amber); g.drawRoundedRectangle (r.expanded (1), 3, 1.6f); }
        }
        // playheads (one per polyrhythm lane, plus the main one)
        if (playTicks >= 0) {
            const double rel = std::fmod (playTicks, (double) L);
            g.setColour (th::text.withAlpha (0.85f)); g.drawVerticalLine ((int) st.tickToX (rel), 0, (float) getHeight());
            for (int i = 0; i < rows; ++i) {
                const int note = lanes[(size_t) i];
                if (! p.laneLength.count (note)) continue;
                const double lr = std::fmod (playTicks, (double) p.laneLen (note));
                const int y = i * EditState::laneH - st.scrollY;
                g.setColour (th::amber); g.fillRect (st.tickToX (lr) - 1, (float) y, 2.0f, (float) EditState::laneH);
            }
        }
        // rubber band, split line, paint preview
        if (! band.isEmpty()) { g.setColour (th::blue.withAlpha (0.12f)); g.fillRect (band); g.setColour (th::blue.withAlpha (0.7f)); g.drawRect (band, 1); }
        if (st.tool == Tool::Split && hoverX >= 0) {
            const float x = st.tickToX (st.snapRound (st.xToTick ((float) hoverX)));
            float dash[] = { 4, 3 }; g.setColour (th::red);
            g.drawDashedLine ({ x, 0, x, (float) getHeight() }, dash, 2, 1.5f);
        }
    }

    // ------------------------------------------------------------ mouse
    void mouseMove (const MouseEvent& e) override
    {
        hoverX = e.x;
        auto cursor = MouseCursor::NormalCursor;
        if (st.tool == Tool::Draw || st.tool == Tool::Paint) cursor = MouseCursor::CrosshairCursor;
        else if (st.tool == Tool::Erase || st.tool == Tool::Mute) cursor = MouseCursor::PointingHandCursor;
        else if (st.tool == Tool::Split) cursor = MouseCursor::IBeamCursor;
        else if (laneHandleAt (e.position) >= 0) cursor = MouseCursor::LeftRightResizeCursor;
        else if (auto* n = hit (e.position)) { if (nearRightEdge (*n, e.position)) cursor = MouseCursor::LeftRightResizeCursor; }
        setMouseCursor (cursor);
        if (st.tool == Tool::Split) repaint();
    }
    void mouseExit (const MouseEvent&) override { hoverX = -1; repaint(); }

    void mouseDown (const MouseEvent& e) override
    {
        grabKeyboardFocus();
        mode = Mode::None;
        auto& p = st.pat();
        const auto lanes = st.lanes();
        const int row = (e.y + st.scrollY) / EditState::laneH;
        const int note = (row >= 0 && row < (int) lanes.size()) ? lanes[(size_t) row] : -1;
        const double tick = st.xToTick ((float) e.x);
        st.cursorTick = st.snapFloor (tick);
        downPos = e.position;

        if (e.mods.isPopupMenu()) { contextMenu (e); return; }

        const Tool tool = (e.mods.isAltDown() && st.tool == Tool::Draw) ? Tool::Erase : st.tool;
        switch (tool) {
            case Tool::Split:
                if (st.splitAt) st.splitAt (st.snapRound (tick));
                return;
            case Tool::Erase:
                st.proc.beginEdit(); mode = Mode::Erase; eraseAt (e.position); return;
            case Tool::Mute:
                if (auto* n = hit (e.position)) { const int i = indexOfNote (*n); st.apply ("Mute note", [i] (Pattern& q) { auto& x = q.notes[(size_t) i]; x.probability = x.probability == 0 ? 100 : 0; }); }
                return;
            case Tool::Draw:
                if (note < 0) return;
                if (auto* n = hit (e.position)) { const int i = indexOfNote (*n); st.apply ("Delete note", [i] (Pattern& q) { q.notes.erase (q.notes.begin() + i); }); return; }
                st.proc.beginEdit(); mode = Mode::Draw; drawLane = note;
                addNote (note, st.snapFloor (tick));
                if (preview) preview (note);
                return;
            case Tool::Paint:
                if (note < 0) return;
                st.proc.beginEdit(); mode = Mode::Paint; drawLane = note; paintStart = st.snapFloor (tick); orig = p.notes;
                paintTo (tick);
                return;
            case Tool::Select:
            case Tool::Line:
            default: break;
        }

        // select tool
        const int lh = laneHandleAt (e.position);
        if (lh >= 0) { st.proc.beginEdit(); mode = Mode::LaneLen; drawLane = lh; return; }
        if (auto* n = hit (e.position)) {
            if (e.mods.isShiftDown()) n->selected = ! n->selected;
            else if (! n->selected) { selectAll (p, false); n->selected = true; }
            if (e.getNumberOfClicks() > 1) { const int i = indexOfNote (*n); st.apply ("Delete note", [i] (Pattern& q) { q.notes.erase (q.notes.begin() + i); }); return; }
            st.proc.beginEdit();
            orig = p.notes;
            anchorTick = n->tick; anchorEnd = n->tick + n->length; anchorRow = row;
            mode = nearRightEdge (*n, e.position) ? Mode::Resize : Mode::Move;
            copyDrag = e.mods.isCommandDown();
            if (preview && mode == Mode::Move) preview (n->note);
            st.publish();
            return;
        }
        if (e.getNumberOfClicks() > 1 && note >= 0) {
            st.proc.beginEdit(); addNote (note, st.snapFloor (tick)); st.proc.endEdit ("Add note");
            if (preview) preview (note);
            return;
        }
        if (! e.mods.isShiftDown()) selectAll (p, false);
        mode = Mode::Band; bandBase = p.notes;
        st.publish();
    }

    void mouseDrag (const MouseEvent& e) override
    {
        auto& p = st.pat();
        hoverX = e.x;
        switch (mode) {
            case Mode::Erase: eraseAt (e.position); break;
            case Mode::Draw: {
                const int t = st.snapFloor (st.xToTick ((float) e.x));
                bool exists = false; for (auto& n : p.notes) if (n.note == drawLane && std::abs (n.tick - t) < 2) exists = true;
                if (! exists && t < p.lengthTicks() * 4) addNote (drawLane, t);
                break;
            }
            case Mode::Paint: paintTo (st.xToTick ((float) e.x)); break;
            case Mode::LaneLen: {
                int t = st.snapRound (st.xToTick ((float) e.x));
                t = jlimit (st.gridTicks() > 1 ? st.gridTicks() : 30, p.lengthTicks() * 4, t);
                if (t == p.lengthTicks()) p.laneLength.erase (drawLane); else p.laneLength[drawLane] = t;
                st.publish();
                break;
            }
            case Mode::Move: case Mode::Resize: {
                const double dt = (e.position.x - downPos.x) / st.pxPerBeat * PPQ;
                const bool free = e.mods.isAltDown() || st.grid < 0;
                p.notes = orig;
                if (copyDrag && mode == Mode::Move) for (auto& n : orig) if (n.selected) { auto c = n; c.selected = false; p.notes.push_back (c); }
                if (mode == Mode::Move) {
                    int newAnchor = free ? (int) std::round (anchorTick + dt) : st.snapRound (anchorTick + dt);
                    const int d = newAnchor - anchorTick;
                    const auto lanes = st.lanes();
                    const int dRow = (int) std::round ((e.position.y - downPos.y) / EditState::laneH);
                    for (auto& n : p.notes) if (n.selected) {
                        n.tick = jmax (0, n.tick + d);
                        if (dRow != 0) {
                            const int r = indexOf (lanes, n.note) + dRow;
                            if (r >= 0 && r < (int) lanes.size()) n.note = lanes[(size_t) r];
                        }
                    }
                } else {
                    const int newEnd = free ? (int) std::round (anchorEnd + dt) : st.snapRound (anchorEnd + dt);
                    const int d = newEnd - anchorEnd;
                    for (auto& n : p.notes) if (n.selected) n.length = jmax (10, n.length + d);
                }
                st.publish();
                break;
            }
            case Mode::Band: {
                band = Rectangle<float> (downPos, e.position).toNearestInt().toFloat();
                const auto lanes = st.lanes();
                p.notes = bandBase;
                for (auto& n : p.notes) {
                    const int row = indexOf (lanes, n.note);
                    if (row >= 0 && noteRect (n, row).intersects (band)) n.selected = true;
                }
                st.publish();
                break;
            }
            default: break;
        }
        if (mode != Mode::None) autoscroll (e);
    }

    void mouseUp (const MouseEvent&) override
    {
        auto& p = st.pat();
        switch (mode) {
            case Mode::Erase: st.proc.endEdit ("Erase"); break;
            case Mode::Draw: st.proc.endEdit ("Draw notes"); break;
            case Mode::Paint: st.proc.endEdit ("Paint notes"); break;
            case Mode::LaneLen: st.proc.endEdit ("Lane length"); break;
            case Mode::Move: case Mode::Resize: p.sort(); removeDoubles (p); st.publish(); st.proc.endEdit (mode == Mode::Move ? (copyDrag ? "Copy notes" : "Move notes") : "Note length"); break;
            case Mode::Band: band = {}; repaint(); break;
            default: break;
        }
        mode = Mode::None;
    }

    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override { if (onWheel) onWheel (w, e); }

private:
    enum class Mode { None, Move, Resize, Band, Draw, Paint, Erase, LaneLen };
    EditState& st;
    Mode mode = Mode::None;
    Point<float> downPos;
    std::vector<Note> orig, bandBase;
    int anchorTick = 0, anchorEnd = 0, anchorRow = 0, drawLane = -1, paintStart = 0, hoverX = -1;
    bool copyDrag = false;
    Rectangle<float> band;

    static int indexOf (const std::vector<int>& v, int x) { for (int i = 0; i < (int) v.size(); ++i) if (v[(size_t) i] == x) return i; return -1; }
    int indexOfNote (const Note& n) { auto& p = st.pat(); return (int) (&n - p.notes.data()); }

    Rectangle<float> noteRect (const Note& n, int row) const
    {
        const float x = st.tickToX (n.tick);
        const float wpx = jmax (7.0f, n.length * st.pxPerBeat / PPQ - 1.0f);
        return { x, (float) (row * EditState::laneH - st.scrollY) + 3.0f, wpx, EditState::laneH - 6.0f };
    }
    Note* hit (Point<float> pt)
    {
        auto& p = st.pat(); const auto lanes = st.lanes();
        for (int i = (int) p.notes.size() - 1; i >= 0; --i) {
            auto& n = p.notes[(size_t) i];
            const int row = indexOf (lanes, n.note);
            if (row >= 0 && noteRect (n, row).expanded (1, 0).contains (pt)) return &n;
        }
        return nullptr;
    }
    bool nearRightEdge (const Note& n, Point<float> pt)
    {
        const int row = st.laneIndex (n.note);
        auto r = noteRect (n, row);
        return r.getWidth() > 12 && pt.x > r.getRight() - 5;
    }
    int laneHandleAt (Point<float> pt)
    {
        auto& p = st.pat(); const auto lanes = st.lanes();
        const int row = (int) (pt.y + st.scrollY) / EditState::laneH;
        if (row < 0 || row >= (int) lanes.size()) return -1;
        const int note = lanes[(size_t) row];
        const float lx = st.tickToX (p.laneLen (note));
        if (std::abs (pt.x - lx) < 5) return note;
        return -1;
    }
    void addNote (int note, int tick)
    {
        auto& p = st.pat();
        Note n; n.tick = tick; n.note = note; n.velocity = st.defaultVel;
        n.length = jmax (30, jmin (st.gridTicks() > 1 ? st.gridTicks() : 120, 240));
        p.notes.push_back (n); p.sort();
        st.publish();
    }
    void eraseAt (Point<float> pt)
    {
        auto& p = st.pat(); const auto lanes = st.lanes();
        const size_t before = p.notes.size();
        p.notes.erase (std::remove_if (p.notes.begin(), p.notes.end(), [&] (const Note& n) {
            const int row = indexOf (lanes, n.note); return row >= 0 && noteRect (n, row).expanded (2, 0).contains (pt); }), p.notes.end());
        if (p.notes.size() != before) st.publish();
    }
    // paint brush: repeated notes on the grid from the start point to the mouse, accents on the beat
    void paintTo (double tick)
    {
        auto& p = st.pat();
        p.notes = orig;
        const int g = st.gridTicks() > 1 ? st.gridTicks() : 240;
        const int end = st.snapFloor (tick);
        const int a = jmin (paintStart, end), b = jmax (paintStart, end);
        p.notes.erase (std::remove_if (p.notes.begin(), p.notes.end(), [&] (const Note& n) { return n.note == drawLane && n.tick >= a && n.tick <= b; }), p.notes.end());
        for (int t = a; t <= b; t += g) {
            Note n; n.tick = t; n.note = drawLane; n.length = jmin (g, 240);
            n.velocity = t % PPQ == 0 ? st.defaultVel : jlimit (1, 127, (int) (st.defaultVel * (t % (PPQ / 2) == 0 ? 0.82f : 0.66f)));
            p.notes.push_back (n);
        }
        p.sort(); st.publish();
    }
    void autoscroll (const MouseEvent& e)
    {
        if (e.x > getWidth() - 10) { st.scrollX += 12; st.publish(); }
        else if (e.x < 10 && st.scrollX > 0) { st.scrollX = jmax (0, st.scrollX - 12); st.publish(); }
    }
    void contextMenu (const MouseEvent& e)
    {
        auto& p = st.pat();
        if (auto* n = hit (e.position)) { if (! n->selected) { selectAll (p, false); n->selected = true; st.publish(); } }
        PopupMenu m;
        m.addItem (1, "Delete", anySelected (p));
        m.addItem (2, "Duplicate", anySelected (p));
        m.addItem (3, "Copy", anySelected (p));
        m.addItem (4, "Paste here", ! st.clipboard.empty());
        m.addSeparator();
        PopupMenu prob;
        for (int v : { 100, 90, 75, 50, 33, 25, 10, 0 }) prob.addItem (100 + v, String (v) + "%" + (v == 0 ? "  (muted)" : ""));
        m.addSubMenu ("Chance to play", prob, anySelected (p));
        PopupMenu vel;
        for (int v : { 127, 112, 100, 90, 80, 64, 48, 32 }) vel.addItem (300 + v, String (v));
        m.addSubMenu ("Velocity", vel, anySelected (p));
        m.addItem (5, "Split pattern here");
        st.cursorTick = st.snapFloor (st.xToTick ((float) e.x));
        const int splitTick = st.snapRound (st.xToTick ((float) e.x));
        m.showMenuAsync (PopupMenu::Options().withMousePosition(), [this, splitTick] (int r) {
            if (r == 1) st.apply ("Delete", [] (Pattern& q) { deleteSelected (q); });
            else if (r == 2) st.apply ("Duplicate", [] (Pattern& q) { duplicateSelection (q); });
            else if (r == 3) { st.clipboard.clear(); int lo = INT_MAX; for (auto& n : st.pat().notes) if (n.selected) { st.clipboard.push_back (n); lo = jmin (lo, n.tick); } for (auto& n : st.clipboard) n.tick -= lo; }
            else if (r == 4) { const int at = st.cursorTick; auto clip = st.clipboard; st.apply ("Paste", [at, clip] (Pattern& q) { selectAll (q, false); for (auto n : clip) { n.tick += at; n.selected = true; q.notes.push_back (n); } q.sort(); }); }
            else if (r == 5) { if (st.splitAt) st.splitAt (splitTick); }
            else if (r >= 100 && r <= 200) { const int v = r - 100; st.apply ("Chance", [v] (Pattern& q) { for (auto& n : q.notes) if (n.selected) n.probability = v; }); }
            else if (r >= 300) { const int v = r - 300; st.apply ("Velocity", [v] (Pattern& q) { for (auto& n : q.notes) if (n.selected) n.velocity = v; }); }
        });
    }
};

// ---------------------------------------------------------------- velocity / probability lane
class VelocityLane : public Component, public SettableTooltipClient
{
public:
    explicit VelocityLane (EditState& e) : st (e)
    {
        setTooltip ("Drag to draw values. Line tool (or Shift+drag): draw a straight line or ramp. Right-click: ramps, curves, accents");
    }
    void paint (Graphics& g) override
    {
        g.fillAll (th::panel);
        const float h = (float) getHeight() - 8.0f;
        for (int q = 1; q < 4; ++q) { g.setColour (th::line); g.drawHorizontalLine ((int) (4 + h * q / 4), 0, (float) getWidth()); }
        auto& p = st.pat();
        const float ex = st.tickToX (p.lengthTicks());
        g.setColour (Colours::black.withAlpha (0.4f)); g.fillRect (ex, 0.0f, (float) getWidth() - ex, (float) getHeight());
        const bool anySel = anySelected (p);
        for (auto& n : p.notes) {
            if (! shown (n)) continue;
            const float x = st.tickToX (n.tick);
            if (x < -6 || x > getWidth()) continue;
            const float v = value (n) / (st.probMode ? 100.0f : 127.0f);
            const auto c = st.probMode ? th::amber : st.laneColour (n.note);
            const bool dimmed = anySel && ! n.selected;
            g.setColour (c.withAlpha (dimmed ? 0.25f : 0.9f));
            g.fillRect (x, 4 + h * (1.0f - v), 4.0f, h * v);
            g.fillEllipse (x - 1.5f, 4 + h * (1.0f - v) - 3.5f, 7, 7);
        }
        if (lineMode && dragging) { g.setColour (th::text); g.drawLine (Line<float> (lineA, lineB), 1.5f); }
        g.setColour (th::line); g.drawHorizontalLine (0, 0, (float) getWidth());
    }
    void mouseDown (const MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { menu(); return; }
        lineMode = st.tool == Tool::Line || e.mods.isShiftDown();
        dragging = true;
        st.proc.beginEdit();
        lineA = lineB = e.position; last = e.position;
        if (! lineMode) setRange (e.position, e.position);
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (lineMode) { lineB = e.position; repaint(); return; }
        setRange (last, e.position); last = e.position;
    }
    void mouseUp (const MouseEvent&) override
    {
        if (lineMode) setRange (lineA, lineB);
        dragging = false;
        st.proc.endEdit (st.probMode ? "Draw chance" : "Draw velocity");
        repaint();
    }
private:
    EditState& st;
    bool dragging = false, lineMode = false;
    Point<float> lineA, lineB, last;

    bool shown (const Note& n) const { return st.focusLane < 0 || n.note == st.focusLane; }
    int value (const Note& n) const { return st.probMode ? n.probability : n.velocity; }
    int fromY (float y) const
    {
        const float h = (float) getHeight() - 8.0f;
        const float v = jlimit (0.0f, 1.0f, 1.0f - (y - 4) / h);
        return st.probMode ? (int) std::round (v * 100) : jmax (1, (int) std::round (v * 127));
    }
    // every visible note between a and b gets the value on the straight line a→b
    void setRange (Point<float> a, Point<float> b)
    {
        auto& p = st.pat();
        const bool anySel = anySelected (p);
        if (a.x > b.x) std::swap (a, b);
        bool changed = false;
        for (auto& n : p.notes) {
            if (! shown (n) || (anySel && ! n.selected)) continue;
            const float x = st.tickToX (n.tick) + 2.0f;
            if (x < a.x - 3 || x > b.x + 3) continue;
            const float t = b.x - a.x > 1 ? jlimit (0.0f, 1.0f, (x - a.x) / (b.x - a.x)) : 0.0f;
            const int v = fromY (a.y + (b.y - a.y) * t);
            if (st.probMode) n.probability = v; else n.velocity = v;
            changed = true;
        }
        if (changed) st.publish();
    }
    void menu()
    {
        PopupMenu m;
        m.addSectionHeader (st.probMode ? "CHANCE (selected notes, or all shown)" : "VELOCITY (selected notes, or all shown)");
        m.addItem (1, "Ramp up (crescendo)");
        m.addItem (2, "Ramp down (decrescendo)");
        m.addItem (3, "Curve up: slow start");
        m.addItem (4, "Curve up: fast start");
        m.addItem (5, "Curve down: slow start");
        m.addItem (6, "Accent the beats");
        m.addItem (7, "Random");
        m.addItem (8, "Compress 50%");
        m.addItem (9, "Expand 150%");
        m.addItem (10, "All to 100");
        const bool prob = st.probMode;
        const int focus = st.focusLane;
        m.showMenuAsync (PopupMenu::Options().withMousePosition(), [this, prob, focus] (int r) {
            if (r <= 0) return;
            st.apply (prob ? "Chance shape" : "Velocity shape", [r, prob, focus] (Pattern& p) {
                // work on the selection, or on the notes the lane shows
                bool anySel = anySelected (p);
                if (! anySel) for (auto& n : p.notes) n.selected = focus < 0 || n.note == focus;
                if (prob) {
                    std::mt19937 rng ((uint32) Time::getMillisecondCounter());
                    if (r == 7) randomizeProbability (p, 30, 100, rng);
                    else if (r == 10) for (auto& n : p.notes) { if (n.selected) n.probability = 100; }
                    else {
                        auto tn = targetNotes (p); int lo = INT_MAX, hi = 0;
                        for (auto* n : tn) { lo = jmin (lo, n->tick); hi = jmax (hi, n->tick); }
                        for (auto* n : tn) { const double x = hi > lo ? (double) (n->tick - lo) / (hi - lo) : 0; n->probability = jlimit (0, 100, (int) std::round (r == 2 ? 100 - 70 * x : 30 + 70 * x)); }
                    }
                } else {
                    std::mt19937 rng ((uint32) Time::getMillisecondCounter());
                    switch (r) {
                        case 1: velocityRamp (p, 40, 120, 0.0); break;
                        case 2: velocityRamp (p, 120, 40, 0.0); break;
                        case 3: velocityRamp (p, 40, 124, 0.8); break;
                        case 4: velocityRamp (p, 40, 124, -0.8); break;
                        case 5: velocityRamp (p, 124, 40, 0.8); break;
                        case 6: for (auto* n : targetNotes (p)) n->velocity = n->tick % PPQ == 0 ? 116 : (n->tick % (PPQ / 2) == 0 ? 88 : 66); break;
                        case 7: randomizeVelocity (p, 60, 120, rng); break;
                        case 8: velocityScale (p, 50); break;
                        case 9: velocityScale (p, 150); break;
                        case 10: velocityFixed (p, 100); break;
                        default: break;
                    }
                }
                if (! anySel) selectAll (p, false);
            });
        });
    }
};

} // namespace dp
