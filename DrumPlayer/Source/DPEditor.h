// Drum Player: the main window.
#pragma once
#include "DPPanels.h"

namespace dp {

// everything lives in Body; the editor scales Body for the zoom setting
class Body : public Component
{
public:
    std::function<void()> onResized;
    std::function<void (Graphics&)> onPaint;
    void resized() override { if (onResized) onResized(); }
    void paint (Graphics& g) override { g.fillAll (th::bg); if (onPaint) onPaint (g); }
};

class DrumPlayerEditor : public AudioProcessorEditor, public DragAndDropContainer, public FileDragAndDropTarget, private Timer
{
public:
    explicit DrumPlayerEditor (DrumPlayerProcessor& p)
        : AudioProcessorEditor (p), proc (p), st (p),
          strip (st, false), bigStrip (st, true), tray (st), bigTray (st), ruler (st), headers (st), grid (st), vel (st),
          quantPage (st), humPage (st), chainPage (st), setPage (st), presetPage (st)
    {
        setLookAndFeel (&lnf);
        LookAndFeel::setDefaultLookAndFeel (&lnf);
        addAndMakeVisible (body);
        body.onResized = [this] { layout(); };
        body.onPaint = [this] (Graphics& g) { paintBody (g); };
        st.grid = proc.uiGrid;
        st.pxPerBeat = 80.0f * proc.uiZoomX;
        st.changed = [this] { refreshAll(); };
        st.toast = [this] (const String& t) { showToast (t); };
        st.splitAt = [this] (int t) { splitMenu (t); };

        // ---------------- header
        setupButton (body, prevPreset, "<", false, "Previous preset");
        setupButton (body, nextPreset, ">", false, "Next preset");
        setupButton (body, presetName, "Presets", true, "Preset browser");
        setupButton (body, saveBtn, "Save", false, "Save the current preset (or name a new one in the browser)");
        setupButton (body, exchangeBtn, "Exchange", true, "Big keyboard for moving patterns between keys");
        setupButton (body, chainsBtn, "Chains", true);
        setupButton (body, settingsBtn, "Settings", true);
        setupButton (body, zoomOut, "-", false, "Smaller"); setupButton (body, zoomPct, "100%", false, "Window size"); setupButton (body, zoomIn, "+", false, "Bigger");
        prevPreset.onClick = [this] { presetPage.step (-1); refreshAll(); };
        nextPreset.onClick = [this] { presetPage.step (1); refreshAll(); };
        saveBtn.onClick = [this] { quickSave(); };
        presetName.onClick = [this] { togglePage (presetPage, presetName); };
        exchangeBtn.onClick = [this] { toggleExchange(); };
        chainsBtn.onClick = [this] { togglePage (chainPage, chainsBtn); };
        settingsBtn.onClick = [this] { togglePage (setPage, settingsBtn); };
        zoomOut.onClick = [this] { setScale (scale - 0.1f); };
        zoomIn.onClick = [this] { setScale (scale + 0.1f); };
        zoomPct.onClick = [this] { zoomMenu(); };

        // ---------------- keyboard strip
        body.addAndMakeVisible (strip); body.addAndMakeVisible (tray);
        strip.onFiles = [this] (const StringArray& f, int k) { importFiles (f, k); };
        strip.onKeyMenu = [this] (int k) { keyMenu (k); };
        bigStrip.onFiles = strip.onFiles; bigStrip.onKeyMenu = strip.onKeyMenu;

        // ---------------- pattern toolbar
        body.addAndMakeVisible (nameEd); nameEd.setFont (font (12.5f, 2)); nameEd.setTextToShowWhenEmpty ("Pattern name", th::faint);
        nameEd.onReturnKey = nameEd.onFocusLost = [this] { if (nameEd.getText() != st.pat().name) { const auto t = nameEd.getText(); st.apply ("Rename", [t] (Pattern& p) { p.name = t; }); } };
        setupCombo (body, typeBox, { "Groove", "Fill", "Intro", "Ending" }, 1, "Fill and Intro play once and go back to the groove; Ending plays once and stops");
        typeBox.onChange = [this] { const int t = typeBox.getSelectedItemIndex(); if (t >= 0 && t != (int) st.pat().type) st.apply ("Pattern type", [t] (Pattern& p) { p.type = (PatternType) t; }); };
        setupLabel (body, lLen, "Length");
        body.addAndMakeVisible (barsEd); barsEd.setFont (font (12.5f, 2)); barsEd.setInputRestrictions (4, "0123456789"); barsEd.setJustification (Justification::centred);
        barsEd.onReturnKey = barsEd.onFocusLost = [this] { const int b = jlimit (1, maxBars, barsEd.getText().getIntValue()); if (b != st.pat().bars) st.apply ("Pattern length", [b] (Pattern& p) { p.bars = b; }); else refreshControls(); };
        setupButton (body, lenMenuBtn, "bars", false, "Quick lengths and length tools");
        lenMenuBtn.onClick = [this] { lengthMenu(); };
        StringArray bpb; for (int i = 1; i <= 16; ++i) bpb.add (String (i) + "/4");
        setupCombo (body, bpbBox, bpb, 1, "Beats per bar");
        bpbBox.onChange = [this] { const int b = bpbBox.getSelectedItemIndex() + 1; if (b != st.pat().beatsPerBar) st.apply ("Beats per bar", [b] (Pattern& p) { p.beatsPerBar = b; }); };
        setupLabel (body, lGrid, "Grid");
        StringArray grids; for (auto& v : noteValues) grids.add (v.name); grids.add ("Off");
        setupCombo (body, gridBox, grids, 1, "Snap grid for drawing and moving (Alt while dragging = no snap)");
        gridBox.onChange = [this] { const int i = gridBox.getSelectedItemIndex(); st.grid = i >= numNoteValues ? -1 : i; proc.uiGrid = jmax (0, st.grid); quantPage.grid.setSelectedItemIndex (jmax (0, st.grid), dontSendNotification); refreshAll(); };
        for (int i = 0; i < 7; ++i) {
            auto* b = toolBtns.add (new TextButton());
            static const char* tips[] = { "Select and move (1). Ctrl-drag copies, drag a note's right edge for length, drag the amber bar at a lane's end for polyrhythm",
                "Draw (2): click to add or remove, drag to add along the lane. Alt = erase", "Erase (3)", "Scissors (4): split the pattern here",
                "Paint brush (5): drag to paint repeated notes with accents", "Mute (6): click notes to mute or unmute", "Line (7): draw velocity ramps in the velocity lane" };
            setupButton (body, *b, toolName ((Tool) i), true, tips[i]);
            b->setRadioGroupId (11);
            b->onClick = [this, i] { st.tool = (Tool) i; grid.repaint(); };
        }
        toolBtns[0]->setToggleState (true, dontSendNotification);
        setupButton (body, undoBtn, "Undo", false, "Ctrl+Z"); setupButton (body, redoBtn, "Redo", false, "Ctrl+Shift+Z");
        undoBtn.onClick = [this] { proc.undo.undo(); refreshAll(); };
        redoBtn.onClick = [this] { proc.undo.redo(); refreshAll(); };

        // ---------------- feel toolbar
        setupLabel (body, lSwing, "Swing");
        setupCombo (body, swingPreset, { "Straight 50%", "54%", "58%", "60%", "62%", "66%", "70%", "75%", "Custom" }, 1,
                    "Classic drum machine swing steps (the 48-per-beat set); use the slider for anything in between");
        swingPreset.onChange = [this] {
            static const float v[] = { 50, 54, 58, 60, 62, 66, 70, 75 };
            const int i = swingPreset.getSelectedItemIndex();
            if (i >= 0 && i < 8) if (auto* prm = proc.apvts.getParameter (ids::swing)) prm->setValueNotifyingHost (prm->convertTo0to1 (v[i]));
        };
        setupSlider (body, swingSlider, th::amber);
        swingSlider.setTextBoxStyle (Slider::TextBoxRight, false, 58, btnH);
        swingSlider.setTooltip ("Fine swing, 0.01 % steps. Shift-drag for very fine moves, or type a value");
        swingAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids::swing, swingSlider);
        swingSlider.setNumDecimalPlacesToDisplay (2);
        swingSlider.onValueChange = [this] { syncSwingPreset(); };
        setupCombo (body, swingValue, StringArray::fromTokens (swingChoiceNames(), "|", ""), 1, "Which notes swing. Auto finds the value you played in each beat");
        swingValAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ids::swingGrid, swingValue);
        setupButton (body, quantBtn, "Quantize", true); quantBtn.onClick = [this] { togglePage (quantPage, quantBtn); };
        setupButton (body, humBtn, "Humanize & Feel", true); humBtn.onClick = [this] { togglePage (humPage, humBtn); };
        setupButton (body, funcBtn, "Functions", false, "Every editing function"); funcBtn.onClick = [this] { functionsMenu(); };
        setupLabel (body, lVel, "Vel");
        StringArray vels; for (int v : { 127, 120, 110, 100, 90, 80, 70, 60, 50, 40, 30, 20 }) vels.add (String (v));
        setupCombo (body, velBox, vels, 1, "Velocity for new notes");
        velBox.setSelectedItemIndex (3, dontSendNotification);
        velBox.onChange = [this] { st.defaultVel = velBox.getText().getIntValue(); };
        setupButton (body, recBtn, "Rec", true, "Record notes you play (outside the pattern keys) into the playing pattern, looping");
        recBtn.onClick = [this] { proc.recArmed = recBtn.getToggleState(); };
        setupButton (body, playBtn, "Play", false, "Play the pattern being edited");
        playBtn.onClick = [this] { proc.uiTrigger (st.song().editKey); };
        setupButton (body, stopBtn, "Stop"); stopBtn.onClick = [this] { proc.uiStop(); };
        dragBtn.setButtonText ("Drag MIDI"); dragBtn.setSize (fitW ("Drag MIDI"), btnH); body.addAndMakeVisible (dragBtn);
        dragBtn.getParts = [this] { return std::vector<const Pattern*> { &st.pat() }; };
        dragBtn.getChannel = [this] { return st.song().settings.outChannel; };

        // ---------------- roll
        body.addAndMakeVisible (ruler); body.addAndMakeVisible (headers); body.addAndMakeVisible (grid); body.addAndMakeVisible (vel);
        body.addAndMakeVisible (hScroll); body.addAndMakeVisible (vScroll);
        hScroll.setAutoHide (false); vScroll.setAutoHide (false);
        hScroll.addListener (&scrollRelay); vScroll.addListener (&scrollRelay);
        scrollRelay.owner = this;
        headers.preview = grid.preview = [this] (int n) { proc.uiPreview (n); };
        auto wheel = [this] (const MouseWheelDetails& w, bool shift, bool cmd, float mouseX) {
            if (cmd) {                                                    // zoom around the mouse
                const double t = st.xToTick (mouseX);
                st.pxPerBeat = jlimit (12.0f, 900.0f, st.pxPerBeat * (w.deltaY > 0 ? 1.15f : 1.0f / 1.15f));
                proc.uiZoomX = st.pxPerBeat / 80.0f;
                st.scrollX = jmax (0, (int) (t / PPQ * st.pxPerBeat - mouseX));
            } else if (shift || std::abs (w.deltaX) > std::abs (w.deltaY)) {
                st.scrollX = jmax (0, st.scrollX - (int) ((shift ? w.deltaY : w.deltaX) * 400));
            } else st.scrollY = jmax (0, st.scrollY - (int) (w.deltaY * 300));
            clampScroll(); repaintRoll();
        };
        grid.onWheel = [wheel] (const MouseWheelDetails& w, const MouseEvent& e) { wheel (w, e.mods.isShiftDown(), e.mods.isCommandDown(), e.position.x); };
        headers.onWheel = [wheel] (const MouseWheelDetails& w) { wheel (w, false, false, 0); };

        body.addAndMakeVisible (velMode); setupButton (body, velMode, "Velocity", false, "Switch the lower lane between velocity and chance to play");
        velMode.onClick = [this] { st.probMode = ! st.probMode; velMode.setButtonText (st.probMode ? "Chance" : "Velocity"); vel.repaint(); };

        // ---------------- pages
        for (Page* pg : { (Page*) &quantPage, (Page*) &humPage, (Page*) &chainPage, (Page*) &setPage, (Page*) &presetPage }) body.addChildComponent (pg);
        presetPage.onLoaded = [this] { refreshAll(); };
        presetPage.onImport = [this] { importDialog(); };
        body.addChildComponent (exchange);
        exchange.addAndMakeVisible (bigStrip); exchange.addAndMakeVisible (bigTray);
        exchange.addAndMakeVisible (exchangeHelp);
        setupLabel (exchange, exchangeHelp, "Drag a key onto another key to swap them. Drag to the parked tray to set one aside, then drag it back to any key. Right-click a key for more.", 12.0f, th::dim);

        addAndMakeVisible (toast); toast.setInterceptsMouseClicks (false, false);
        body.addAndMakeVisible (status); status.setFont (font (11.0f, 1)); status.setColour (Label::textColourId, th::faint);

        scale = proc.uiScale;
        setResizable (true, true);
        setResizeLimits (900, 560, 3200, 2000);
        setSize ((int) (1240 * scale), (int) (800 * scale));
        refreshAll();
        startTimerHz (30);
    }
    ~DrumPlayerEditor() override
    {
        stopTimer();
        hScroll.removeListener (&scrollRelay); vScroll.removeListener (&scrollRelay);
        LookAndFeel::setDefaultLookAndFeel (nullptr);
        setLookAndFeel (nullptr);
    }

    void resized() override
    {
        body.setTransform (AffineTransform::scale (scale));
        body.setBounds (0, 0, (int) std::round (getWidth() / scale), (int) std::round (getHeight() / scale));
        toast.setBounds (getLocalBounds().removeFromBottom ((int) (60 * scale)).withSizeKeepingCentre ((int) (520 * scale), (int) (34 * scale)));
    }

    // ------------------------------------------------------------ file drops anywhere else: into the edited key
    bool isInterestedInFileDrag (const StringArray& f) override { return strip.isInterestedInFileDrag (f); }
    void filesDropped (const StringArray& f, int, int) override { importFiles (f, st.song().editKey); }

    // dragging a key out of the window turns into a MIDI file drag
    bool shouldDropFilesWhenDraggedExternally (const DragAndDropTarget::SourceDetails& d, StringArray& files, bool& canMove) override
    {
        const auto s = d.description.toString();
        const Pattern* p = nullptr;
        if (s.startsWith ("key:")) p = &st.song().keys[(size_t) jlimit (0, 127, s.fromFirstOccurrenceOf ("key:", false, false).getIntValue())];
        else if (s.startsWith ("park:")) p = &st.song().park[(size_t) jlimit (0, 7, s.fromFirstOccurrenceOf ("park:", false, false).getIntValue())];
        if (p == nullptr || p->notes.empty()) return false;
        auto dir = dataFolder().getChildFile ("Temp"); dir.createDirectory();
        auto f = dir.getChildFile (File::createLegalFileName (p->name.isNotEmpty() ? p->name : String ("Pattern")) + ".mid");
        writeMidiFile ({ p }, f, st.song().settings.outChannel);
        files.add (f.getFullPathName()); canMove = false;
        return true;
    }

    bool keyPressed (const KeyPress& k) override
    {
        const auto m = k.getModifiers();
        const int code = k.getKeyCode();
        auto& p = st.pat();
        if (m.isCommandDown() && code == 'Z') { if (m.isShiftDown()) proc.undo.redo(); else proc.undo.undo(); refreshAll(); return true; }
        if (m.isCommandDown() && code == 'Y') { proc.undo.redo(); refreshAll(); return true; }
        if (m.isCommandDown() && code == 'A') { selectAll (p, true); st.publish(); return true; }
        if (m.isCommandDown() && code == 'D') { st.apply ("Duplicate", [] (Pattern& q) { duplicateSelection (q); }); return true; }
        if (m.isCommandDown() && code == 'C') { copySel(); return true; }
        if (m.isCommandDown() && code == 'X') { copySel(); st.apply ("Cut", [] (Pattern& q) { deleteSelected (q); }); return true; }
        if (m.isCommandDown() && code == 'V') { paste(); return true; }
        if (code == KeyPress::deleteKey || code == KeyPress::backspaceKey) { st.apply ("Delete", [] (Pattern& q) { deleteSelected (q); }); return true; }
        if (code == KeyPress::escapeKey) { if (openPage != nullptr || exchange.isVisible()) closePages(); else { selectAll (p, false); st.publish(); } return true; }
        if (! m.isAnyModifierKeyDown() && code >= '1' && code <= '7') { setTool ((Tool) (code - '1')); return true; }
        if (! m.isAnyModifierKeyDown() && (code == 'Q' || code == 'q')) { quantPage.run(); return true; }
        if (code == KeyPress::spaceKey) { if (proc.playingKey.load() >= 0) proc.uiStop(); else proc.uiTrigger (st.song().editKey); return true; }
        if (code == KeyPress::leftKey || code == KeyPress::rightKey) {
            const int g = (m.isAltDown() || st.grid < 0) ? 10 : st.gridTicks();
            const int d = code == KeyPress::leftKey ? -g : g;
            st.apply ("Nudge", [d] (Pattern& q) { nudge (q, d); }); return true;
        }
        if (code == KeyPress::upKey || code == KeyPress::downKey) {
            if (m.isShiftDown()) { const int d = code == KeyPress::upKey ? 10 : -10; st.apply ("Velocity", [d] (Pattern& q) { velocityAdd (q, d); }); return true; }
            auto lanes = st.lanes(); const int dir = code == KeyPress::upKey ? -1 : 1;
            st.apply ("Move lane", [lanes, dir] (Pattern& q) {
                for (auto& n : q.notes) if (n.selected) { int i = 0; for (; i < (int) lanes.size(); ++i) if (lanes[(size_t) i] == n.note) break; i += dir; if (i >= 0 && i < (int) lanes.size()) n.note = lanes[(size_t) i]; }
                q.sort(); });
            return true;
        }
        return false;
    }

private:
    DrumPlayerProcessor& proc;
    DPLnF lnf;
    EditState st;
    Body body;
    float scale = 1.0f;

    TextButton prevPreset, nextPreset, presetName, saveBtn, exchangeBtn, chainsBtn, settingsBtn, zoomOut, zoomPct, zoomIn;
    KeyStrip strip, bigStrip;
    ParkTray tray, bigTray;
    TextEditor nameEd, barsEd;
    ComboBox typeBox, bpbBox, gridBox, swingPreset, swingValue, velBox;
    Label lLen, lGrid, lSwing, lVel, exchangeHelp, status;
    TextButton lenMenuBtn, undoBtn, redoBtn, quantBtn, humBtn, funcBtn, recBtn, playBtn, stopBtn, velMode;
    OwnedArray<TextButton> toolBtns;
    FineSlider swingSlider;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> swingAtt;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> swingValAtt;
    DragMidiButton dragBtn;
    Ruler ruler; LaneHeaders headers; NoteGrid grid; VelocityLane vel;
    ScrollBar hScroll { false }, vScroll { true };
    struct ScrollRelay : public ScrollBar::Listener
    {
        DrumPlayerEditor* owner = nullptr;
        void scrollBarMoved (ScrollBar* sb, double v) override
        {
            if (owner == nullptr) return;
            if (sb == &owner->hScroll) owner->st.scrollX = (int) v; else owner->st.scrollY = (int) v;
            owner->repaintRoll();
        }
    } scrollRelay;
    QuantizePage quantPage; HumanizePage humPage; ChainPage chainPage; SettingsPage setPage; PresetPage presetPage;
    Component exchange;
    Page* openPage = nullptr; TextButton* openPageBtn = nullptr;
    struct Toast : public Component
    {
        String text; uint32 until = 0;
        void paint (Graphics& g) override
        {
            if (Time::getMillisecondCounter() > until || text.isEmpty()) return;
            drawCard (g, getLocalBounds().toFloat().reduced (1), th::raised, 8);
            g.setFont (font (12.5f * getHeight() / 34.0f, 2)); g.setColour (th::text);
            g.drawFittedText (text, getLocalBounds().reduced (10, 2), Justification::centred, 2);
        }
    } toast;
    int lastStateVersion = -1, lastPlayKey = -2, lastHitSerial = 0;
    double lastPlayTicks = -1;

    // ------------------------------------------------------------ layout
    void layout()
    {
        auto a = body.getLocalBounds().reduced (10, 8);
        // header
        auto h = a.removeFromTop (30);
        headerTitle = h.removeFromLeft (170);
        RowPlacer hp (h, 5);
        hp.place (prevPreset, 24); hp.place (presetName, 230); hp.place (nextPreset, 24); hp.skip (4); hp.place (saveBtn);
        RowPlacer hr (h.withTrimmedLeft (h.getWidth() - 360), 5);
        hr.place (exchangeBtn); hr.place (chainsBtn); hr.place (settingsBtn); hr.skip (8);
        hr.place (zoomOut, 24); hr.place (zoomPct, 48); hr.place (zoomIn, 24);
        a.removeFromTop (6);
        // keyboard + tray
        auto ks = a.removeFromTop (78);
        tray.setBounds (ks.removeFromRight (250));
        ks.removeFromRight (8);
        strip.setBounds (ks);
        a.removeFromTop (8);
        // toolbar 1
        auto t1 = a.removeFromTop (28);
        RowPlacer r1 (t1, 5);
        r1.place (nameEd, 150); r1.place (typeBox, 84); r1.skip (6);
        r1.place (lLen, 44); r1.place (barsEd, 40); r1.place (lenMenuBtn); r1.place (bpbBox, 58); r1.skip (6);
        r1.place (lGrid, 30); r1.place (gridBox, 70); r1.skip (10);
        for (auto* b : toolBtns) r1.place (*b);
        r1.skip (10); r1.place (undoBtn); r1.place (redoBtn);
        a.removeFromTop (4);
        // toolbar 2
        auto t2 = a.removeFromTop (28);
        RowPlacer r2 (t2, 5);
        r2.place (lSwing, 40); r2.place (swingPreset, 104); r2.place (swingSlider, 190); r2.place (swingValue, 66); r2.skip (10);
        r2.place (quantBtn); r2.place (humBtn); r2.place (funcBtn); r2.skip (10);
        r2.place (lVel, 24); r2.place (velBox, 56); r2.skip (10);
        r2.place (recBtn); r2.place (playBtn); r2.place (stopBtn); r2.place (dragBtn);
        a.removeFromTop (8);
        // status line
        status.setBounds (a.removeFromBottom (18));
        a.removeFromBottom (2);
        // roll
        rollArea = a;
        const int headW = 200, rulerH = 22, velH = 104, sb = 10;
        auto velRow = a.removeFromBottom (velH);
        a.removeFromBottom (4);
        auto top = a.removeFromTop (rulerH);
        vScroll.setBounds (a.removeFromRight (sb)); top.removeFromRight (sb);
        hScroll.setBounds (a.removeFromBottom (sb).withTrimmedLeft (headW));
        headers.setBounds (a.removeFromLeft (headW)); top.removeFromLeft (headW);
        grid.setBounds (a); ruler.setBounds (top);
        velMode.setBounds (velRow.getX() + 8, velRow.getY() + 8, fitW ("Velocity"), btnH);
        velLabelArea = velRow.removeFromLeft (headW);
        velRow.removeFromRight (sb);
        vel.setBounds (velRow);
        // pages float over the roll's right side
        auto pageArea = rollArea.withTrimmedLeft (jmax (0, rollArea.getWidth() - 520)).withTrimmedBottom (velH / 2);
        for (Page* pg : { (Page*) &quantPage, (Page*) &humPage, (Page*) &chainPage, (Page*) &setPage }) pg->setBounds (pageArea);
        presetPage.setBounds (rollArea.withTrimmedLeft (jmax (0, rollArea.getWidth() - 600)));
        exchange.setBounds (rollArea.withTrimmedTop (-90));
        auto ex = exchange.getLocalBounds().reduced (12);
        exchangeHelp.setBounds (ex.removeFromTop (24));
        bigTray.setBounds (ex.removeFromBottom (120).removeFromLeft (420));
        ex.removeFromBottom (10);
        bigStrip.setBounds (ex);
        clampScroll();
        body.repaint();
    }
    Rectangle<int> headerTitle, rollArea, velLabelArea;

    void paintBody (Graphics& g)
    {
        // title: two-tone wordmark like the practice app
        g.setFont (font (17.0f, 3)); g.setColour (th::text);
        g.drawText ("DRUM", headerTitle.withWidth (60), Justification::centredLeft);
        g.setColour (th::amber);
        g.drawText ("PLAYER", headerTitle.withTrimmedLeft (58), Justification::centredLeft);
        // cards behind the roll and the keyboard
        drawCard (g, rollArea.toFloat().expanded (2), th::panel, 10);
        drawCard (g, velLabelArea.toFloat(), th::panel, 6);
        g.setFont (font (10.5f, 1)); g.setColour (th::faint);
        const auto focus = st.focusLane >= 0 && st.song().lane (st.focusLane) ? st.song().lane (st.focusLane)->name : String ("all lanes");
        g.drawFittedText ("Showing: " + focus + "\nClick a lane name to show only that drum", velLabelArea.reduced (10, 6).withTrimmedTop (30), Justification::topLeft, 3);
    }

    // ------------------------------------------------------------ refresh
    void refreshAll()
    {
        refreshControls();
        if (openPage != nullptr) openPage->refresh();
        clampScroll();
        strip.repaint(); bigStrip.repaint(); tray.repaint(); bigTray.repaint();
        repaintRoll();
        body.repaint (headerTitle);
    }
    void refreshControls()
    {
        auto& p = st.pat();
        if (! nameEd.hasKeyboardFocus (true)) nameEd.setText (p.name, false);
        typeBox.setSelectedItemIndex ((int) p.type, dontSendNotification);
        if (! barsEd.hasKeyboardFocus (true)) barsEd.setText (String (p.bars), false);
        bpbBox.setSelectedItemIndex (p.beatsPerBar - 1, dontSendNotification);
        gridBox.setSelectedItemIndex (st.grid < 0 ? numNoteValues : st.grid, dontSendNotification);
        presetName.setButtonText (proc.presetName.isNotEmpty() ? proc.presetName : String ("Presets"));
        undoBtn.setEnabled (proc.undo.canUndo()); redoBtn.setEnabled (proc.undo.canRedo());
        recBtn.setToggleState (proc.recArmed.load(), dontSendNotification);
        zoomPct.setButtonText (String ((int) std::round (scale * 100)) + "%");
        syncSwingPreset();
        status.setText (keyName (st.song().editKey) + "   " + (p.name.isNotEmpty() ? p.name : String ("(empty key)")) + "   |   "
                        + String ((int) p.notes.size()) + " notes   |   " + String (p.bars) + " x " + String (p.beatsPerBar) + "/4"
                        + "   |   pattern keys " + keyName (st.song().settings.zoneLow) + " to " + keyName (st.song().settings.zoneHigh)
                        + "   |   out ch " + String (st.song().settings.outChannel), dontSendNotification);
    }
    void syncSwingPreset()
    {
        static const float v[] = { 50, 54, 58, 60, 62, 66, 70, 75 };
        const double s = swingSlider.getValue();
        int idx = 8; for (int i = 0; i < 8; ++i) if (std::abs (s - v[i]) < 0.005) idx = i;
        swingPreset.setSelectedItemIndex (idx, dontSendNotification);
    }
    void repaintRoll()
    {
        hScroll.setRangeLimits (0, jmax ((double) grid.getWidth(), (double) st.contentTicks() / PPQ * st.pxPerBeat + 40), dontSendNotification);
        hScroll.setCurrentRange (st.scrollX, grid.getWidth(), dontSendNotification);
        vScroll.setRangeLimits (0, jmax ((double) grid.getHeight(), (double) st.lanes().size() * EditState::laneH), dontSendNotification);
        vScroll.setCurrentRange (st.scrollY, grid.getHeight(), dontSendNotification);
        ruler.repaint(); headers.repaint(); grid.repaint(); vel.repaint();
    }
    void clampScroll()
    {
        const int maxX = jmax (0, (int) (st.contentTicks() / (double) PPQ * st.pxPerBeat + 40) - grid.getWidth());
        const int maxY = jmax (0, (int) st.lanes().size() * EditState::laneH - grid.getHeight());
        st.scrollX = jlimit (0, maxX, st.scrollX); st.scrollY = jlimit (0, maxY, st.scrollY);
    }

    void timerCallback() override
    {
        const int sv = proc.stateVersion.load();
        if (sv != lastStateVersion) { lastStateVersion = sv; st.grid = jlimit (-1, numNoteValues - 1, st.grid); refreshAll(); }
        const int pk = proc.playingKey.load();
        const double pt = pk == st.song().editKey ? proc.playRelTicks.load() : -1.0;
        if (pk != lastPlayKey) { lastPlayKey = pk; strip.repaint(); bigStrip.repaint(); }
        if (pt != lastPlayTicks) { lastPlayTicks = pt; grid.playTicks = pt; grid.repaint(); }
        if (proc.queuedKey.load() != -2) { strip.repaint(); bigStrip.repaint(); }
        if (proc.recArmed.load() != recBtn.getToggleState()) recBtn.setToggleState (proc.recArmed.load(), dontSendNotification);
        if (Time::getMillisecondCounter() > toast.until && toast.text.isNotEmpty()) { toast.text = {}; toast.repaint(); }
        if (openPage == &chainPage && proc.chainPlaying.load() >= 0) {
            const int cs = proc.chainStep.load() * 10 + proc.chainPlaying.load();
            if (cs != lastChainState) { lastChainState = cs; chainPage.refresh(); }
        }
    }
    int lastChainState = -1;

    void showToast (const String& t) { toast.text = t; toast.until = Time::getMillisecondCounter() + 2600; toast.toFront (false); toast.repaint(); }

    // ------------------------------------------------------------ pages
    void togglePage (Page& pg, TextButton& btn)
    {
        if (exchange.isVisible()) { exchange.setVisible (false); exchangeBtn.setToggleState (false, dontSendNotification); }
        const bool open = openPage != &pg;
        closePages();
        if (open) { openPage = &pg; openPageBtn = &btn; pg.refresh(); pg.setVisible (true); pg.toFront (false); btn.setToggleState (true, dontSendNotification); }
    }
    void closePages()
    {
        if (openPage != nullptr) openPage->setVisible (false);
        if (openPageBtn != nullptr) openPageBtn->setToggleState (false, dontSendNotification);
        openPage = nullptr; openPageBtn = nullptr;
        exchange.setVisible (false); exchangeBtn.setToggleState (false, dontSendNotification);
    }
    void toggleExchange()
    {
        const bool show = ! exchange.isVisible();
        closePages();
        if (show) { exchange.setVisible (true); exchange.toFront (false); exchangeBtn.setToggleState (true, dontSendNotification); }
    }

    void setTool (Tool t) { st.tool = t; for (int i = 0; i < toolBtns.size(); ++i) toolBtns[i]->setToggleState (i == (int) t, dontSendNotification); grid.repaint(); }

    void setScale (float s)
    {
        const float old = scale;
        scale = jlimit (0.6f, 2.0f, std::round (s * 10.0f) / 10.0f);
        proc.uiScale = scale;
        setSize ((int) std::round (getWidth() / old * scale), (int) std::round (getHeight() / old * scale));
        resized(); refreshControls();
    }
    void zoomMenu()
    {
        PopupMenu m;
        for (int pc : { 70, 80, 90, 100, 110, 120, 130, 150, 175, 200 }) m.addItem (pc, String (pc) + "%", true, (int) std::round (scale * 100) == pc);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&zoomPct), [this] (int r) { if (r > 0) setScale (r / 100.0f); });
    }

    // ------------------------------------------------------------ presets
    void quickSave()
    {
        if (proc.presetName.isEmpty()) { if (openPage != &presetPage) togglePage (presetPage, presetName); showToast ("Type a name and press Save"); return; }
        auto existing = presetFolder().findChildFiles (File::findFiles, true, proc.presetName + presetExt);
        auto f = existing.isEmpty() ? presetFolder().getChildFile (proc.presetName + presetExt) : existing.getFirst();
        if (proc.savePreset (f)) showToast ("Saved " + proc.presetName);
    }

    // ------------------------------------------------------------ MIDI import
    std::unique_ptr<FileChooser> chooser;
    void importDialog()
    {
        chooser = std::make_unique<FileChooser> ("Import MIDI files", File::getSpecialLocation (File::userDocumentsDirectory), "*.mid;*.midi");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::canSelectMultipleItems,
            [this] (const FileChooser& fc) { StringArray f; for (auto& r : fc.getResults()) f.add (r.getFullPathName()); if (! f.isEmpty()) importFiles (f, st.song().editKey); });
    }
    void importFiles (StringArray files, int startKey)
    {
        files.sortNatural();
        std::vector<Pattern> pieces;
        for (auto& path : files) {
            File f (path);
            if (! f.existsAsFile()) continue;
            auto part = readMidiFile (f);
            if (part.notes.empty()) continue;
            for (auto& p : splitToPatterns (part, st.song().settings.splitBars)) pieces.push_back (std::move (p));
        }
        if (pieces.empty()) { showToast ("No MIDI notes found in that file"); return; }
        const bool toBlack = st.song().settings.fillsToBlack && files.size() > 1;
        int first = -1, last = -1;
        st.applySong ("Import MIDI", [&] (Song& s) {
            int k = startKey;
            for (auto& p : pieces) {
                if (toBlack) {
                    const bool wantBlack = p.type == PatternType::Fill;
                    while (k < 128 && isBlackKey (k) != wantBlack) ++k;
                }
                if (k > 127) break;
                s.keys[(size_t) k] = p;
                s.ensureLanesForPattern (p);
                if (first < 0) first = k;
                last = k;
                ++k;
            }
            if (first >= 0) s.editKey = first;
        });
        showToast ("Imported " + String ((int) pieces.size()) + (pieces.size() == 1 ? " pattern to " + keyName (first) : " patterns to " + keyName (first) + " to " + keyName (last)));
    }

    // ------------------------------------------------------------ scissors
    int nextFreeKey (int from, bool black, bool anyColour) const
    {
        auto& s = proc.edit;
        for (int pass = 0; pass < 2; ++pass) {
            const int lo = pass == 0 ? from + 1 : s.settings.zoneLow, hi = pass == 0 ? 127 : from;
            for (int k = lo; k <= hi; ++k)
                if (s.keys[(size_t) k].isEmpty() && (anyColour || isBlackKey (k) == black) && k >= s.settings.zoneLow) return k;
        }
        return -1;
    }
    void splitMenu (int tick)
    {
        auto& p = st.pat();
        if (tick <= 0 || tick >= p.lengthTicks()) { showToast ("Split inside the pattern"); return; }
        PopupMenu m;
        m.addSectionHeader ("SPLIT AT " + String (tick / (p.beatsPerBar * PPQ) + 1) + "." + String ((tick / PPQ) % p.beatsPerBar + 1));
        m.addItem (1, "Cut: the right part goes to the next free key");
        m.addItem (2, "Cut: the right part becomes a Fill on the next free black key");
        m.addItem (3, "Copy the right part as a Fill (keep this pattern whole)");
        m.addItem (4, "Copy the left part as a Fill (keep this pattern whole)");
        m.addItem (5, "Keep only the left part");
        m.addItem (6, "Keep only the right part");
        m.showMenuAsync (PopupMenu::Options().withMousePosition(), [this, tick] (int r) { if (r > 0) doSplit (tick, r); });
    }
    void doSplit (int tick, int how)
    {
        const int key = st.song().editKey;
        auto parts = splitPattern (st.pat(), tick);
        auto& left = parts.first; auto& right = parts.second;
        int dest = -1;
        if (how == 1) dest = nextFreeKey (key, false, true);
        if (how == 2) dest = nextFreeKey (key, true, false);
        if (how == 3 || how == 4) dest = nextFreeKey (key, true, false) >= 0 ? nextFreeKey (key, true, false) : nextFreeKey (key, false, true);
        if ((how >= 1 && how <= 4) && dest < 0) { showToast ("No free key left"); return; }
        st.applySong ("Split pattern", [&] (Song& s) {
            auto& cur = s.keys[(size_t) key];
            switch (how) {
                case 1: right.type = cur.type; s.keys[(size_t) dest] = right; cur = left; break;
                case 2: right.type = PatternType::Fill; s.keys[(size_t) dest] = right; cur = left; break;
                case 3: right.type = PatternType::Fill; right.name = cur.name + " fill"; s.keys[(size_t) dest] = right; break;
                case 4: left.type = PatternType::Fill; left.name = cur.name + " fill"; s.keys[(size_t) dest] = left; break;
                case 5: left.name = cur.name; cur = left; break;
                case 6: right.name = cur.name; cur = right; break;
                default: break;
            }
        });
        if (dest >= 0) showToast ("New pattern on " + keyName (dest));
    }

    // ------------------------------------------------------------ menus
    void copySel()
    {
        st.clipboard.clear(); int lo = INT_MAX;
        for (auto& n : st.pat().notes) if (n.selected) { st.clipboard.push_back (n); lo = jmin (lo, n.tick); }
        for (auto& n : st.clipboard) { n.tick -= lo; n.selected = false; }
    }
    void paste()
    {
        if (st.clipboard.empty()) return;
        const int at = st.cursorTick; auto clip = st.clipboard;
        st.apply ("Paste", [at, clip] (Pattern& q) { selectAll (q, false); for (auto n : clip) { n.tick += at; n.selected = true; q.notes.push_back (n); } q.sort(); });
    }
    Pattern patternClip; bool hasPatternClip = false;

    void lengthMenu()
    {
        PopupMenu m;
        auto& p = st.pat();
        for (int b : { 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 64 }) m.addItem (b, String (b) + (b == 1 ? " bar" : " bars"), true, p.bars == b);
        m.addSeparator();
        m.addItem (101, "Length from selection (whole bars)", anySelected (p));
        m.addItem (102, "Length to the last note");
        m.addItem (103, "Double length (copy the notes)");
        m.addItem (104, "Halve length");
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&lenMenuBtn), [this] (int r) {
            if (r <= 0) return;
            if (r < 100) { st.apply ("Pattern length", [r] (Pattern& p) { p.bars = r; }); return; }
            st.apply ("Pattern length", [r] (Pattern& p) {
                const int bar = p.beatsPerBar * PPQ;
                if (r == 101 || r == 102) {
                    int lo = INT_MAX, hi = 0;
                    for (auto& n : p.notes) if (r == 102 || n.selected) { lo = jmin (lo, n.tick); hi = jmax (hi, n.tick + 1); }
                    if (lo == INT_MAX) return;
                    if (r == 101) { const int start = lo / bar * bar; for (auto& n : p.notes) n.tick -= start; p.notes.erase (std::remove_if (p.notes.begin(), p.notes.end(), [] (const Note& n) { return n.tick < 0; }), p.notes.end()); hi -= start; }
                    p.bars = jlimit (1, maxBars, (hi + bar - 1) / bar);
                } else if (r == 103) {
                    const int L = p.lengthTicks(); std::vector<Note> add;
                    for (auto& n : p.notes) if (n.tick < L) { auto c = n; c.tick += L; c.selected = false; add.push_back (c); }
                    for (auto& n : add) p.notes.push_back (n);
                    p.bars = jmin (maxBars, p.bars * 2); p.sort();
                } else if (r == 104) { if (p.bars > 1) p.bars /= 2; else if (p.beatsPerBar > 1) p.beatsPerBar = jmax (1, p.beatsPerBar / 2); }
            });
        });
    }

    void functionsMenu()
    {
        auto& p = st.pat();
        const bool sel = anySelected (p);
        PopupMenu m;
        m.addSectionHeader (sel ? "ON THE SELECTED NOTES" : "ON THE WHOLE PATTERN (nothing selected)");
        m.addItem (1, "Select all  (Ctrl+A)"); m.addItem (2, "Select none  (Esc)", sel); m.addItem (3, "Invert selection");
        m.addItem (4, "Copy  (Ctrl+C)", sel); m.addItem (5, "Cut  (Ctrl+X)", sel); m.addItem (6, "Paste  (Ctrl+V)", ! st.clipboard.empty());
        m.addItem (7, "Duplicate  (Ctrl+D)", sel); m.addItem (8, "Delete  (Del)", sel);
        m.addSeparator();
        m.addItem (10, "Quantize  (Q)");
        m.addItem (11, "Humanize (into the notes)");
        PopupMenu timing;
        timing.addItem (12, "Reverse"); timing.addItem (13, "Double speed"); timing.addItem (14, "Half speed");
        timing.addItem (15, "Nudge left by grid  (Left)"); timing.addItem (16, "Nudge right by grid  (Right)");
        timing.addItem (17, "Note lengths = grid"); timing.addItem (18, "Remove doubled notes");
        m.addSubMenu ("Timing", timing);
        PopupMenu v;
        v.addItem (20, "Ramp up (crescendo)"); v.addItem (21, "Ramp down"); v.addItem (22, "Accent the beats");
        v.addItem (23, "Louder +10  (Shift+Up)"); v.addItem (24, "Softer -10  (Shift+Down)");
        v.addItem (25, "Compress 50%"); v.addItem (26, "Expand 150%");
        PopupMenu fixed; for (int x : { 127, 110, 100, 90, 80, 64, 48, 32 }) fixed.addItem (200 + x, String (x));
        v.addSubMenu ("Fixed value", fixed);
        m.addSubMenu ("Velocity", v);
        PopupMenu rnd;
        rnd.addItem (30, "Velocities"); rnd.addItem (31, "Timing (small)"); rnd.addItem (32, "Chance to play");
        rnd.addItem (33, "Thin out 25%"); rnd.addItem (34, "Add ghost notes");
        m.addSubMenu ("Randomize", rnd);
        PopupMenu lanes;
        for (auto& l : st.song().lanes) lanes.addItem (1000 + l.note, l.name + "  (" + String (l.note) + ")");
        m.addSubMenu ("Move notes to drum", lanes, sel);
        m.addSeparator();
        PopupMenu pat;
        pat.addItem (40, "Copy pattern"); pat.addItem (41, "Paste pattern here", hasPatternClip);
        pat.addItem (42, "Clear pattern"); pat.addItem (43, "Split at the edit cursor");
        pat.addItem (44, "Export as MIDI file...");
        m.addSubMenu ("Pattern", pat);
        m.addItem (50, "Import MIDI files...");
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&funcBtn), [this] (int r) { runFunction (r); });
    }
    void runFunction (int r)
    {
        std::mt19937 rng ((uint32) Time::getMillisecondCounter());
        auto& p = st.pat();
        switch (r) {
            case 1: selectAll (p, true); st.publish(); break;
            case 2: selectAll (p, false); st.publish(); break;
            case 3: for (auto& n : p.notes) n.selected = ! n.selected; st.publish(); break;
            case 4: copySel(); break;
            case 5: copySel(); st.apply ("Cut", [] (Pattern& q) { deleteSelected (q); }); break;
            case 6: paste(); break;
            case 7: st.apply ("Duplicate", [] (Pattern& q) { duplicateSelection (q); }); break;
            case 8: st.apply ("Delete", [] (Pattern& q) { deleteSelected (q); }); break;
            case 10: quantPage.run(); break;
            case 11: st.apply ("Humanize", [] (Pattern& q) { std::mt19937 g ((uint32) Time::getMillisecondCounter()); humanizeBake (q, 12, 12, g); }); break;
            case 12: st.apply ("Reverse", [] (Pattern& q) { reverseNotes (q); }); break;
            case 13: st.apply ("Double speed", [] (Pattern& q) { scaleTime (q, 2.0); }); break;
            case 14: st.apply ("Half speed", [] (Pattern& q) { scaleTime (q, 0.5); }); break;
            case 15: { const int g = st.gridTicks(); st.apply ("Nudge", [g] (Pattern& q) { nudge (q, -g); }); break; }
            case 16: { const int g = st.gridTicks(); st.apply ("Nudge", [g] (Pattern& q) { nudge (q, g); }); break; }
            case 17: { const int g = jmax (10, st.gridTicks()); st.apply ("Note lengths", [g] (Pattern& q) { setLengths (q, g); }); break; }
            case 18: st.apply ("Remove doubles", [] (Pattern& q) { removeDoubles (q); }); break;
            case 20: st.apply ("Ramp up", [] (Pattern& q) { velocityRamp (q, 40, 120, 0.0); }); break;
            case 21: st.apply ("Ramp down", [] (Pattern& q) { velocityRamp (q, 120, 40, 0.0); }); break;
            case 22: st.apply ("Accents", [] (Pattern& q) { for (auto* n : targetNotes (q)) n->velocity = n->tick % PPQ == 0 ? 116 : (n->tick % (PPQ / 2) == 0 ? 88 : 66); }); break;
            case 23: st.apply ("Louder", [] (Pattern& q) { velocityAdd (q, 10); }); break;
            case 24: st.apply ("Softer", [] (Pattern& q) { velocityAdd (q, -10); }); break;
            case 25: st.apply ("Compress", [] (Pattern& q) { velocityScale (q, 50); }); break;
            case 26: st.apply ("Expand", [] (Pattern& q) { velocityScale (q, 150); }); break;
            case 30: st.apply ("Randomize velocity", [] (Pattern& q) { std::mt19937 g ((uint32) Time::getMillisecondCounter()); randomizeVelocity (q, 64, 120, g); }); break;
            case 31: st.apply ("Randomize timing", [] (Pattern& q) { std::mt19937 g ((uint32) Time::getMillisecondCounter()); randomizeTiming (q, 12, g); }); break;
            case 32: st.apply ("Randomize chance", [] (Pattern& q) { std::mt19937 g ((uint32) Time::getMillisecondCounter()); randomizeProbability (q, 40, 100, g); }); break;
            case 33: st.apply ("Thin out", [] (Pattern& q) { std::mt19937 g ((uint32) Time::getMillisecondCounter()); thinOut (q, 25, g); }); break;
            case 34: { const int gi = st.grid >= 0 ? st.grid : 3; st.apply ("Add ghosts", [gi] (Pattern& q) { std::mt19937 g ((uint32) Time::getMillisecondCounter()); addGhosts (q, gi, 30, g); }); break; }
            case 40: patternClip = p; hasPatternClip = true; showToast ("Pattern copied"); break;
            case 41: { auto c = patternClip; st.apply ("Paste pattern", [c] (Pattern& q) { q = c; }); break; }
            case 42: st.apply ("Clear pattern", [] (Pattern& q) { q = Pattern(); }); break;
            case 43: splitMenu (st.cursorTick); break;
            case 44: exportPattern (st.song().editKey); break;
            case 50: importDialog(); break;
            default:
                if (r >= 200 && r < 400) { const int v = r - 200; st.apply ("Fixed velocity", [v] (Pattern& q) { velocityFixed (q, v); }); }
                else if (r >= 1000) { const int n = r - 1000; st.apply ("Move to drum", [n] (Pattern& q) { moveToLane (q, n); }); }
                break;
        }
        ignoreUnused (rng);
    }

    void exportPattern (int key)
    {
        const auto& p = st.song().keys[(size_t) key];
        if (p.notes.empty()) { showToast ("That key is empty"); return; }
        chooser = std::make_unique<FileChooser> ("Export MIDI", File::getSpecialLocation (File::userDocumentsDirectory).getChildFile (File::createLegalFileName (p.name.isNotEmpty() ? p.name : String ("Pattern")) + ".mid"), "*.mid");
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
            [this, key] (const FileChooser& fc) {
                auto f = fc.getResult(); if (f == File()) return;
                writeMidiFile ({ &st.song().keys[(size_t) key] }, f.withFileExtension ("mid"), st.song().settings.outChannel);
                showToast ("Exported " + f.getFileName());
            });
    }

    void keyMenu (int k)
    {
        auto& s = st.song();
        const auto& p = s.keys[(size_t) k];
        PopupMenu m;
        m.addSectionHeader (keyName (k) + (p.isEmpty() ? "  (empty)" : "  " + p.name));
        m.addItem (1, "Edit"); m.addItem (2, "Play", ! p.notes.empty());
        PopupMenu type; for (int t = 0; t < 4; ++t) type.addItem (10 + t, typeName ((PatternType) t), ! p.isEmpty(), (int) p.type == t);
        m.addSubMenu ("Type", type);
        m.addItem (3, "Copy pattern", ! p.isEmpty()); m.addItem (4, "Paste pattern", hasPatternClip);
        m.addItem (5, "Duplicate to the next free key", ! p.isEmpty());
        m.addItem (6, "Export as MIDI file...", ! p.notes.empty());
        m.addItem (7, "Clear", ! p.isEmpty());
        PopupMenu chain; for (int c = 0; c < 4; ++c) chain.addItem (20 + c, "Add to chain " + String::charToString ((juce_wchar) ('A' + c)), ! p.notes.empty());
        m.addSubMenu ("Chains", chain);
        m.showMenuAsync (PopupMenu::Options().withMousePosition(), [this, k] (int r) {
            switch (r) {
                case 1: st.song().editKey = k; refreshAll(); break;
                case 2: proc.uiTrigger (k); break;
                case 3: patternClip = st.song().keys[(size_t) k]; hasPatternClip = true; break;
                case 4: { auto c = patternClip; st.applySong ("Paste pattern", [k, c] (Song& s2) { s2.keys[(size_t) k] = c; s2.ensureLanesForPattern (c); s2.editKey = k; }); break; }
                case 5: { const int d = nextFreeKey (k, false, true); if (d < 0) { showToast ("No free key left"); break; }
                          st.applySong ("Duplicate pattern", [k, d] (Song& s2) { s2.keys[(size_t) d] = s2.keys[(size_t) k]; s2.keys[(size_t) d].name += " copy"; s2.editKey = d; }); break; }
                case 6: exportPattern (k); break;
                case 7: st.applySong ("Clear key", [k] (Song& s2) { s2.keys[(size_t) k] = Pattern(); }); break;
                default:
                    if (r >= 10 && r < 14) { const int t = r - 10; st.applySong ("Pattern type", [k, t] (Song& s2) { s2.keys[(size_t) k].type = (PatternType) t; }); }
                    else if (r >= 20 && r < 24) { const int c = r - 20; st.applySong ("Add to chain", [k, c] (Song& s2) { s2.chains[(size_t) c].steps.push_back ({ k, 1 }); }); showToast ("Added to chain " + String::charToString ((juce_wchar) ('A' + c))); }
                    break;
            }
        });
    }

public:
    void paint (Graphics& g) override { g.fillAll (th::bg); }
    void paintOverChildrenOfBody (Graphics&) {}
};

} // namespace dp
