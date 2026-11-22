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
    void paint (Graphics& g) override { fillArea (g, getLocalBounds(), "Window background", th::bg); if (onPaint) onPaint (g); }
};

// a bar you drag up or down to share the space between two areas
class Divider : public Component, public SettableTooltipClient
{
public:
    std::function<int()> getValue; std::function<void (int)> setValue; bool invert = false;
    Divider() { setMouseCursor (MouseCursor::UpDownResizeCursor); setTooltip ("Drag up or down"); }
    void paint (Graphics& g) override
    {
        g.setColour (Colours::white.withAlpha (isMouseOver() ? 0.22f : 0.08f));
        g.fillRoundedRectangle (getLocalBounds().toFloat().withSizeKeepingCentre (60.0f, 3.0f), 1.5f);
    }
    void mouseEnter (const MouseEvent&) override { repaint(); }
    void mouseExit (const MouseEvent&) override { repaint(); }
    void mouseDown (const MouseEvent&) override { start = getValue ? getValue() : 0; }
    void mouseDrag (const MouseEvent& e) override { if (setValue) setValue (start + (invert ? -1 : 1) * e.getDistanceFromDragStartY()); }
private:
    int start = 0;
};

class DrumPlayerEditor : public AudioProcessorEditor, public DragAndDropContainer, public FileDragAndDropTarget, private Timer, public MenuBarModel
{
public:
    explicit DrumPlayerEditor (DrumPlayerProcessor& p)
        : AudioProcessorEditor (p), proc (p), st (p),
          strip (st, false), bigStrip (st, true), tray (st), bigTray (st), chainStrip (st), ruler (st), headers (st), grid (st), vel (st), songLane (st), songHeader (st, songLane),
          quantPage (st), humPage (st), chainPage (st), setPage (st), presetPage (st), mapPage (st), recPage (st)
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
        st.chooseTool = [this] (Tool t) { setTool (t); };
        st.openCurve = [this] { openCurve(); };

        // ---------------- header
        setupButton (body, prevPreset, "<", false, "Previous preset");
        setupButton (body, nextPreset, ">", false, "Next preset");
        setupButton (body, presetName, "Presets", true, "Preset browser");
        setupButton (body, saveBtn, "Save", false, "Save the current preset (or name a new one in the browser)");
        setupButton (body, exchangeBtn, "Exchange", true, "Big keyboard for moving patterns between keys");
        setupButton (body, keysBtn, "Pattern mode", true, "Your keyboard plays patterns: one per key from C-1 up. The stop key sits below C-1");
        setupButton (body, playModeBtn, "Play mode", true, "Your keyboard plays the drums, for playing live and recording");
        keysBtn.setRadioGroupId (61); playModeBtn.setRadioGroupId (61);
        keysBtn.onClick = [this] { setKeyMode (0); };
        playModeBtn.onClick = [this] { setKeyMode (1); };
        body.addAndMakeVisible (keyInfo); keyInfo.setFont (font (11.5f, 2)); keyInfo.setColour (Label::textColourId, th::dim);
        keyInfo.setComponentID ("Pressed key"); keyInfo.setText ("Press a key...", dontSendNotification); keyInfo.setJustificationType (Justification::centredLeft);
        setupButton (body, chainsBtn, "Chains", true);
        setupButton (body, mapBtn, "Drum Map", true, "Which drum plugin you're playing, and where dropped MIDI comes from");
        mapBtn.onClick = [this] { togglePage (mapPage, mapBtn); };
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
        body.addAndMakeVisible (strip); body.addAndMakeVisible (tray); body.addAndMakeVisible (chainStrip);
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
            static const char* tips[] = { "Select and move (1, as in Studio One). Ctrl-drag copies, drag a note's right edge for length, drag the amber bar at a lane's end for polyrhythm",
                "Draw (5, Studio One's paint tool): click to add or remove, drag to add along the lane. Alt = erase", "Erase (4)", "Scissors (3): split the pattern here. Alt+X splits at the cursor",
                "Paint brush (7): drag to paint repeated notes with accents", "Mute (6): click notes to mute or unmute. M mutes the selection", "Line (2): draw velocity ramps in the velocity lane" };
            setupButton (body, *b, toolName ((Tool) i), true, tips[i]);
            b->setRadioGroupId (11);
            b->onClick = [this, i] { st.tool = (Tool) i; st.stamping = false; grid.repaint(); };
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
        setupButton (body, recBtn, "Rec", false, "Record into the key you're editing (it starts playing, after a count-in). Press again to stop recording");
        recBtn.onClick = [this] {
            const bool start = proc.recStateUi.load() == 0;
            const auto& o = st.song().settings;
            // recording needs keys that play drums: switch to Play mode unless a pad has its own channel
            if (start && o.keyMode == 0 && ! (o.playChannel != 0 && o.playChannel != o.triggerChannel)) { setKeyMode (1); showToast ("Play mode on: your keys now play drums for recording"); }
            proc.uiRecord (start, st.song().editKey);
        };
        setupButton (body, recOptBtn, "Record...", true, "Count-in, click, overdub or replace, quantize and more");
        recOptBtn.onClick = [this] { togglePage (recPage, recOptBtn); };
        StringArray guides { "Lines: off", "Lines every 8", "Lines every 16", "Lines every 20", "Lines every 32" };
        setupCombo (body, guideBox, guides, 1, "Guide lines in the velocity lane. Bars click onto them; hold Alt to move freely");
        guideBox.setSelectedItemIndex (2, dontSendNotification);
        guideBox.onChange = [this] { static const int g[] = { 0, 8, 16, 20, 32 }; st.velGuide = g[jlimit (0, 4, guideBox.getSelectedItemIndex())]; vel.repaint(); };
        setupButton (body, playBtn, "Play", false, "Play the pattern being edited");
        playBtn.onClick = [this] { proc.uiTrigger (st.song().editKey); };
        setupButton (body, stopBtn, "Stop"); stopBtn.onClick = [this] { proc.uiStop(); };
        dragBtn.setButtonText ("Drag MIDI"); dragBtn.setSize (fitW ("Drag MIDI"), btnH); body.addAndMakeVisible (dragBtn);
        dragBtn.getParts = [this] { return std::vector<const Pattern*> { &st.pat() }; };
        dragBtn.getChannel = [this] { return st.song().settings.outChannel; };

        // ---------------- roll
        body.addAndMakeVisible (ruler); body.addAndMakeVisible (headers); body.addAndMakeVisible (grid); body.addAndMakeVisible (vel);
        body.addAndMakeVisible (songLane); body.addAndMakeVisible (songHeader); songHeader.attach();
        body.addAndMakeVisible (songDiv); body.addAndMakeVisible (velDiv);
        songDiv.getValue = [this] { return songH; }; songDiv.setValue = [this] (int v) { songH = jlimit (40, 260, v); layout(); };
        velDiv.getValue = [this] { return velH; }; velDiv.setValue = [this] (int v) { velH = jlimit (50, 420, v); layout(); }; velDiv.invert = true;
        menuBar.setModel (this); body.addAndMakeVisible (menuBar);
        setupButton (body, zH1, "W-", false, "Narrower (Ctrl+wheel)"); setupButton (body, zH2, "W+", false, "Wider (Ctrl+wheel)");
        setupButton (body, zV1, "H-", false, "Lower lanes (Alt+wheel)"); setupButton (body, zV2, "H+", false, "Taller lanes (Alt+wheel)");
        setupButton (body, zFit, "Fit", false, "Fit the pattern to the window (Ctrl+Shift+F)");
        zH1.onClick = [this] { zoomX (1.0f / 1.25f); }; zH2.onClick = [this] { zoomX (1.25f); };
        zV1.onClick = [this] { zoomY (-3); }; zV2.onClick = [this] { zoomY (3); };
        zFit.onClick = [this] { zoomToFit(); };
        body.addAndMakeVisible (hScroll); body.addAndMakeVisible (vScroll);
        hScroll.setAutoHide (false); vScroll.setAutoHide (false);
        hScroll.addListener (&scrollRelay); vScroll.addListener (&scrollRelay);
        scrollRelay.owner = this;
        headers.preview = grid.preview = [this] (int n) { proc.uiPreview (n); };
        auto wheel = [this] (const MouseWheelDetails& w, bool shift, bool cmd, float mouseX) {
            if (ModifierKeys::currentModifiers.isAltDown()) { zoomY (w.deltaY > 0 ? 2 : -2); return; }
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

        body.addAndMakeVisible (velMode); setupButton (body, velMode, "Velocity", false, "What the lower lane shows, and velocity actions");
        velMode.onClick = [this] {
            PopupMenu m;
            m.addItem (1, "Show velocity", true, ! st.probMode); m.addItem (2, "Show chance to play", true, st.probMode);
            m.addSeparator();
            m.addItem (3, "Curve (sine, triangle, ramps)...");
            m.addItem (10020, "Ramp up"); m.addItem (10021, "Ramp down"); m.addItem (10022, "Accent the beats");
            m.addItem (10023, "Louder +10"); m.addItem (10024, "Softer -10"); m.addItem (10025, "Compress 50%"); m.addItem (10026, "Expand 150%");
            m.addItem (10030, "Randomize"); m.addItem (10011, "Humanize");
            PopupMenu fx; for (int x : { 127, 110, 100, 90, 80, 64, 48, 32 }) fx.addItem (10200 + x, String (x));
            m.addSubMenu ("All to", fx);
            m.showMenuAsync (PopupMenu::Options().withTargetComponent (&velMode), [this] (int r) {
                if (r == 1 || r == 2) { st.probMode = r == 2; velMode.setButtonText (st.probMode ? "Chance" : "Velocity"); vel.repaint(); }
                else if (r == 3) openCurve();
                else if (r >= 10000) runFunction (r - 10000);
            });
        };

        // ---------------- pages
        for (Page* pg : { (Page*) &quantPage, (Page*) &humPage, (Page*) &chainPage, (Page*) &setPage, (Page*) &presetPage, (Page*) &mapPage, (Page*) &recPage }) body.addChildComponent (pg);
        presetPage.onLoaded = [this] { refreshAll(); };
        setPage.look.collectIds = [this] {
            StringArray out;
            std::function<void (Component&)> walk = [&] (Component& c) {
                for (auto* ch : c.getChildren()) {
                    const auto id = ch->getComponentID();
                    if (id.isNotEmpty()) {
                        if (dynamic_cast<TextButton*> (ch) != nullptr) out.addIfNotAlreadyThere ("btn:" + id);
                        else if (dynamic_cast<Label*> (ch) != nullptr) out.addIfNotAlreadyThere ("txt:" + id);
                    }
                    walk (*ch);
                }
            };
            walk (body);
            return out;
        };
        setPage.look.onChanged = [this] { repaintEverything (body); };
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

    // Studio One key commands where they exist: 1 arrow, 3 split, 4 erase, 5 paint (pencil), 6 mute, D duplicate,
    // Alt+X split at cursor, Q quantize, Ctrl+Y / Ctrl+Z, Alt+Left/Right nudge, Shift+Up/Down move to the next drum
    bool keyPressed (const KeyPress& k) override
    {
        const auto m = k.getModifiers();
        const int code = k.getKeyCode();
        const juce_wchar ch = CharacterFunctions::toUpperCase (k.getTextCharacter());
        auto& p = st.pat();
        const bool plain = ! m.isCommandDown() && ! m.isAltDown() && ! m.isCtrlDown();
        if (m.isCommandDown() && code == 'Z') { if (m.isShiftDown()) proc.undo.redo(); else proc.undo.undo(); refreshAll(); return true; }
        if (m.isCommandDown() && code == 'Y') { proc.undo.redo(); refreshAll(); return true; }
        if (m.isCommandDown() && m.isShiftDown() && code == 'F') { zoomToFit(); return true; }

        if (songLane.hasKeyboardFocus (false)) {                    // the song lane has the keys
            if (m.isCommandDown() && code == 'A') { songLane.selectAll(); return true; }
            if (m.isCommandDown() && code == 'C') { songLane.copy(); return true; }
            if (m.isCommandDown() && code == 'X') { songLane.copy(); songLane.deleteSelected(); return true; }
            if (m.isCommandDown() && code == 'V') { songLane.paste(); return true; }
            if ((plain && ch == 'D') || (m.isCommandDown() && code == 'D')) { songLane.duplicate(); return true; }
            if (m.isAltDown() && code == 'X') { songLane.splitAtCursor(); return true; }
            if (plain && ch == 'M') { songLane.muteSelected(); return true; }
            if (code == KeyPress::deleteKey || code == KeyPress::backspaceKey) { songLane.deleteSelected(); return true; }
            if (code == KeyPress::spaceKey) { if (proc.songBeatUi.load() >= 0) proc.uiSongPlay (false, 0); else songHeader.refresh(), proc.uiSongPlay (true, songLane.cursorBeat); return true; }
            if (code == KeyPress::escapeKey) { for (auto& c : st.song().clips) c.selected = false; songLane.repaint(); return true; }
            return false;
        }

        if (m.isCommandDown() && code == 'R') { st.repeatLast(); return true; }
        if (m.isCommandDown() && code == 'A') { selectAll (p, true); st.publish(); return true; }
        if ((plain && ch == 'D') || (m.isCommandDown() && code == 'D')) { st.apply ("Duplicate", [] (Pattern& q) { duplicateSelection (q); }); return true; }
        if (m.isCommandDown() && code == 'C') { copySel(); return true; }
        if (m.isCommandDown() && code == 'X') { copySel(); st.apply ("Cut", [] (Pattern& q) { deleteSelected (q); }); return true; }
        if (m.isCommandDown() && code == 'V') { paste(); return true; }
        if (m.isAltDown() && code == 'X') { splitMenu (st.cursorTick); return true; }
        if (plain && ch == 'M') { st.apply ("Mute / unmute", [] (Pattern& q) { for (auto& n : q.notes) if (n.selected) n.probability = n.probability == 0 ? 100 : 0; }); return true; }
        if (code == KeyPress::deleteKey || code == KeyPress::backspaceKey) { st.apply ("Delete", [] (Pattern& q) { deleteSelected (q); }); return true; }
        if (code == KeyPress::escapeKey && st.stamping) { st.stamping = false; showToast ("Stopped"); return true; }
        if (code == KeyPress::escapeKey) { if (openPage != nullptr || exchange.isVisible()) closePages(); else { selectAll (p, false); st.publish(); } return true; }
        if (plain && ch >= '1' && ch <= '7') {
            static const Tool map[] = { Tool::Select, Tool::Line, Tool::Split, Tool::Erase, Tool::Draw, Tool::Mute, Tool::Paint };
            setTool (map[ch - '1']); return true;
        }
        if (plain && ch == 'Q') { quantPage.run(); return true; }
        if (code == KeyPress::spaceKey) { if (proc.playingKey.load() >= 0) proc.uiStop(); else proc.uiTrigger (st.song().editKey); return true; }
        if (code == KeyPress::leftKey || code == KeyPress::rightKey) {
            const int g = st.grid < 0 ? 10 : st.gridTicks();
            const int d = code == KeyPress::leftKey ? -g : g;
            st.apply ("Nudge", [d] (Pattern& q) { nudge (q, d); }); return true;
        }
        if (code == KeyPress::upKey || code == KeyPress::downKey) {
            if (m.isCommandDown()) { const int d = code == KeyPress::upKey ? 10 : -10; st.apply ("Velocity", [d] (Pattern& q) { velocityAdd (q, d); }); return true; }
            auto lanes = st.lanes(); const int dir = code == KeyPress::upKey ? -1 : 1;
            st.apply ("Move to the next drum", [lanes, dir] (Pattern& q) {
                for (auto& n : q.notes) if (n.selected) { int i = 0; for (; i < (int) lanes.size(); ++i) if (lanes[(size_t) i] == n.note) break; i += dir; if (i >= 0 && i < (int) lanes.size()) n.note = lanes[(size_t) i]; }
                q.sort(); });
            return true;
        }
        return false;
    }
    void zoomX (float f)
    {
        const double centreTick = st.xToTick (grid.getWidth() * 0.5f);
        st.pxPerBeat = jlimit (12.0f, 900.0f, st.pxPerBeat * f); proc.uiZoomX = st.pxPerBeat / 80.0f;
        st.scrollX = jmax (0, (int) (centreTick / PPQ * st.pxPerBeat - grid.getWidth() * 0.5f));
        clampScroll(); repaintRoll();
    }
    void zoomY (int d) { st.laneH = jlimit (14, 60, st.laneH + d); clampScroll(); repaintRoll(); }
    void openCurve()
    {
        auto c = std::make_unique<CurvePanel> (st);
        CallOutBox::launchAsynchronously (std::move (c), vel.getScreenBounds().withSizeKeepingCentre (40, 40).translated (0, -vel.getHeight() / 2), nullptr);
    }

    // ------------------------------------------------------------ the menu bar (File, Edit, Notes, ...)
    StringArray getMenuBarNames() override { return { "File", "Edit", "Notes", "Pattern", "Song", "View", "Tools" }; }
    PopupMenu getMenuForIndex (int index, const String&) override
    {
        PopupMenu m;
        auto& p = st.pat();
        const bool sel = anySelected (p);
        switch (index) {
            case 0:
                m.addItem (1, "New (clear everything)"); m.addItem (2, "Presets..."); m.addItem (3, "Save preset");
                m.addSeparator(); m.addItem (4, "Import MIDI files..."); m.addItem (5, "Export this pattern as MIDI..."); m.addItem (6, "Export the song lane as MIDI...");
                m.addSeparator(); m.addItem (7, "Drum map..."); m.addItem (8, "Settings..."); m.addItem (9, "Look...");
                break;
            case 1:
                m.addItem (101, "Undo  (Ctrl+Z)", proc.undo.canUndo()); m.addItem (102, "Redo  (Ctrl+Y)", proc.undo.canRedo());
                m.addSeparator(); m.addItem (103, "Cut  (Ctrl+X)", sel); m.addItem (104, "Copy  (Ctrl+C)", sel); m.addItem (105, "Paste  (Ctrl+V)", ! st.clipboard.empty());
                m.addItem (106, "Duplicate  (D)", sel); m.addItem (107, "Delete  (Del)", sel);
                m.addSeparator(); m.addItem (108, "Select all  (Ctrl+A)"); m.addItem (109, "Select none  (Esc)", sel); m.addItem (110, "Invert selection");
                m.addItem (111, st.lastAction ? "Repeat \"" + st.lastName + "\"  (Ctrl+R)" : String ("Repeat last action  (Ctrl+R)"), (bool) st.lastAction);
                break;
            case 2: {
                m.addItem (10010, "Quantize  (Q)"); m.addItem (201, "Quantize settings..."); m.addItem (202, "Humanize & feel..."); m.addItem (10011, "Humanize the notes");
                m.addSeparator();
                PopupMenu v; v.addItem (203, "Curve (sine, triangle, ramps)..."); v.addItem (10020, "Ramp up"); v.addItem (10021, "Ramp down"); v.addItem (10022, "Accent the beats");
                v.addItem (10023, "Louder +10"); v.addItem (10024, "Softer -10"); v.addItem (10025, "Compress 50%"); v.addItem (10026, "Expand 150%");
                for (int x : { 127, 110, 100, 90, 80, 64, 48, 32 }) v.addItem (10200 + x, "All to " + String (x));
                m.addSubMenu ("Velocity", v);
                PopupMenu t; t.addItem (10012, "Reverse"); t.addItem (10013, "Double speed"); t.addItem (10014, "Half speed"); t.addItem (10015, "Nudge left");
                t.addItem (10016, "Nudge right"); t.addItem (10017, "Lengths = grid"); t.addItem (10018, "Remove doubled notes");
                m.addSubMenu ("Timing", t);
                PopupMenu r; r.addItem (10030, "Velocities"); r.addItem (10031, "Timing"); r.addItem (10032, "Chance to play"); r.addItem (10033, "Thin out 25%"); r.addItem (10034, "Add ghost notes");
                m.addSubMenu ("Randomize", r);
                PopupMenu d; for (auto& l : st.song().lanes) d.addItem (11000 + l.note, l.name + "  (" + String (l.note) + ")");
                m.addSubMenu ("Move to drum", d, sel);
                m.addItem (204, "Mute / unmute  (M)", sel);
                break;
            }
            case 3: {
                m.addItem (301, "Play  (Space)"); m.addItem (302, "Stop"); m.addItem (303, "Record into this key");
                m.addSeparator();
                PopupMenu len; for (int b : { 1, 2, 3, 4, 6, 8, 12, 16, 32 }) len.addItem (400 + b, String (b) + (b == 1 ? " bar" : " bars"), true, p.bars == b);
                m.addSubMenu ("Length", len);
                PopupMenu ty; for (int t2 = 0; t2 < 4; ++t2) ty.addItem (420 + t2, typeName ((PatternType) t2), true, (int) p.type == t2);
                m.addSubMenu ("Type", ty);
                m.addItem (10043, "Split at the cursor  (Alt+X)"); m.addItem (10040, "Copy pattern"); m.addItem (10041, "Paste pattern", hasPatternClip); m.addItem (10042, "Clear pattern");
                m.addItem (304, "Convert from another drum plugin...");
                break;
            }
            case 4:
                m.addItem (501, "Song plays", true, st.song().songMode); m.addItem (502, "Play the song from the cursor"); m.addItem (503, "Stop the song");
                m.addSeparator(); m.addItem (504, "Duplicate chunks  (D)"); m.addItem (505, "Split chunks at the cursor  (Alt+X)"); m.addItem (506, "Mute chunks  (M)");
                m.addItem (507, "Delete chunks"); m.addItem (508, "Make unique"); m.addItem (509, "Select all chunks");
                break;
            case 5:
                m.addItem (601, "Wider  (Ctrl+wheel)"); m.addItem (602, "Narrower"); m.addItem (603, "Taller lanes  (Alt+wheel)"); m.addItem (604, "Lower lanes");
                m.addItem (605, "Fit the pattern  (Ctrl+Shift+F)");
                m.addItem (606, "Bigger velocity area"); m.addItem (607, "Smaller velocity area"); m.addItem (608, "Bigger song lane"); m.addItem (609, "Smaller song lane");
                m.addItem (610, "Show only used lanes", true, st.usedOnly);
                m.addSeparator(); m.addItem (611, "Exchange keys"); m.addItem (612, "Chains page"); m.addItem (613, "Recording options");
                break;
            case 6: {
                static const char* tn[] = { "Select (arrow)  1", "Draw (pencil)  5", "Erase  4", "Split (scissors)  3", "Paint brush  7", "Mute  6", "Line (velocity ramps)  2" };
                for (int t2 = 0; t2 < 7; ++t2) m.addItem (700 + t2, tn[t2], true, (int) st.tool == t2);
                break;
            }
            default: break;
        }
        return m;
    }
    void menuItemSelected (int id, int) override
    {
        auto& p = st.pat();
        if (id >= 10000) { runFunction (id - 10000); return; }
        switch (id) {
            case 1: AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "New", "Clear every pattern, chain and the song lane?", "Clear", "Cancel", this,
                        ModalCallbackFunction::create ([this] (int r) { if (r == 1) { auto s2 = Song::makeDefault(); s2.settings = st.song().settings; s2.playMap = st.song().playMap; proc.commit (s2, "New"); refreshAll(); } })); break;
            case 2: togglePage (presetPage, presetName); break;
            case 3: quickSave(); break;
            case 4: importDialog(); break;
            case 5: exportPattern (st.song().editKey); break;
            case 6: {
                chooser = std::make_unique<FileChooser> ("Export song", File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Drum Player Song.mid"), "*.mid");
                chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                    [this] (const FileChooser& fc) { auto f = fc.getResult(); if (f != File()) { writeSongMidi (st.song(), f.withFileExtension ("mid"), st.song().settings.outChannel); showToast ("Exported " + f.getFileName()); } });
                break;
            }
            case 7: togglePage (mapPage, mapBtn); break;
            case 8: togglePage (setPage, settingsBtn); break;
            case 9: togglePage (setPage, settingsBtn); setPage.showTab (true); break;
            case 101: proc.undo.undo(); refreshAll(); break;
            case 102: proc.undo.redo(); refreshAll(); break;
            case 103: copySel(); st.apply ("Cut", [] (Pattern& q) { deleteSelected (q); }); break;
            case 104: copySel(); break;
            case 105: paste(); break;
            case 106: st.apply ("Duplicate", [] (Pattern& q) { duplicateSelection (q); }); break;
            case 107: st.apply ("Delete", [] (Pattern& q) { deleteSelected (q); }); break;
            case 108: selectAll (p, true); st.publish(); break;
            case 109: selectAll (p, false); st.publish(); break;
            case 110: for (auto& n : p.notes) n.selected = ! n.selected; st.publish(); break;
            case 111: st.repeatLast(); break;
            case 201: togglePage (quantPage, quantBtn); break;
            case 202: togglePage (humPage, humBtn); break;
            case 203: openCurve(); break;
            case 204: st.apply ("Mute / unmute", [] (Pattern& q) { for (auto& n : q.notes) if (n.selected) n.probability = n.probability == 0 ? 100 : 0; }); break;
            case 301: proc.uiTrigger (st.song().editKey); break;
            case 302: proc.uiStop(); break;
            case 303: recBtn.triggerClick(); break;
            case 304: togglePage (mapPage, mapBtn); break;
            case 501: { const bool on = ! st.song().songMode; st.applySong ("Song mode", [on] (Song& s2) { s2.songMode = on; }); break; }
            case 502: if (! st.song().songMode) st.applySong ("Song mode", [] (Song& s2) { s2.songMode = true; }); proc.uiSongPlay (true, songLane.cursorBeat); break;
            case 503: proc.uiSongPlay (false, 0); break;
            case 504: songLane.duplicate(); break;
            case 505: songLane.splitAtCursor(); break;
            case 506: songLane.muteSelected(); break;
            case 507: songLane.deleteSelected(); break;
            case 508: songLane.makeUnique(); break;
            case 509: songLane.selectAll(); break;
            case 601: zoomX (1.25f); break;
            case 602: zoomX (0.8f); break;
            case 603: zoomY (3); break;
            case 604: zoomY (-3); break;
            case 605: zoomToFit(); break;
            case 606: velH += 40; layout(); break;
            case 607: velH -= 40; layout(); break;
            case 608: songH = jlimit (40, 260, songH + 30); layout(); break;
            case 609: songH = jlimit (40, 260, songH - 30); layout(); break;
            case 610: st.usedOnly = ! st.usedOnly; refreshAll(); break;
            case 611: toggleExchange(); break;
            case 612: togglePage (chainPage, chainsBtn); break;
            case 613: togglePage (recPage, recOptBtn); break;
            default:
                if (id >= 400 && id < 420) { const int b = id - 400; st.apply ("Pattern length", [b] (Pattern& q) { q.bars = b; }); }
                else if (id >= 420 && id < 424) { const int t2 = id - 420; st.apply ("Pattern type", [t2] (Pattern& q) { q.type = (PatternType) t2; }); }
                else if (id >= 700 && id < 707) setTool ((Tool) (id - 700));
                break;
        }
        refreshControls();
    }

    void zoomToFit()
    {
        st.pxPerBeat = jlimit (12.0f, 900.0f, (float) grid.getWidth() / jmax (1.0f, (float) st.pat().lengthTicks() / PPQ));
        proc.uiZoomX = st.pxPerBeat / 80.0f; st.scrollX = 0; repaintRoll();
    }

private:
    DrumPlayerProcessor& proc;
    DPLnF lnf;
    EditState st;
    Body body;
    float scale = 1.0f;

    TextButton prevPreset, nextPreset, presetName, saveBtn, exchangeBtn, chainsBtn, mapBtn, settingsBtn, zoomOut, zoomPct, zoomIn, keysBtn, playModeBtn;
    Label keyInfo; int lastInSerialSeen = -1;
    KeyStrip strip, bigStrip;
    ParkTray tray, bigTray;
    ChainStrip chainStrip;
    TextEditor nameEd, barsEd;
    ComboBox typeBox, bpbBox, gridBox, swingPreset, swingValue, velBox;
    Label lLen, lGrid, lSwing, lVel, exchangeHelp, status;
    TextButton lenMenuBtn, undoBtn, redoBtn, quantBtn, humBtn, funcBtn, recBtn, recOptBtn, playBtn, stopBtn, velMode;
    ComboBox guideBox;
    OwnedArray<TextButton> toolBtns;
    FineSlider swingSlider;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> swingAtt;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> swingValAtt;
    DragMidiButton dragBtn;
    Ruler ruler; LaneHeaders headers; NoteGrid grid; VelocityLane vel;
    SongLane songLane; SongHeader songHeader;
    Divider songDiv, velDiv;
    MenuBarComponent menuBar;
    TextButton zH1, zH2, zV1, zV2, zFit;
    int songH = 64, velH = 104;
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
    QuantizePage quantPage; HumanizePage humPage; ChainPage chainPage; SettingsPage setPage; PresetPage presetPage; MapPage mapPage; RecordPage recPage;
    Component exchange;
    Page* openPage = nullptr; TextButton* openPageBtn = nullptr;
    struct Toast : public Component
    {
        String text; uint32 until = 0;
        void paint (Graphics& g) override
        {
            if (Time::getMillisecondCounter() > until || text.isEmpty()) return;
            drawCard (g, getLocalBounds().toFloat().reduced (1), th::raised, 8, "Messages");
            g.setFont (font (12.5f * getHeight() / 34.0f, 2)); g.setColour (th::text);
            g.drawFittedText (text, getLocalBounds().reduced (10, 2), Justification::centred, 2);
        }
    } toast;
    int lastStateVersion = -1, lastPlayKey = -2, lastHitSerial = 0;
    double lastPlayTicks = -1;

    // ------------------------------------------------------------ layout
    void layout()
    {
        auto a = body.getLocalBounds().reduced (10, 6);
        menuBar.setBounds (a.removeFromTop (20)); a.removeFromTop (4);
        // header
        auto h = a.removeFromTop (30);
        headerArea = h;
        headerTitle = h.removeFromLeft (170);
        RowPlacer hp (h, 5);
        hp.place (prevPreset, 24); hp.place (presetName, 200); hp.place (nextPreset, 24); hp.skip (4); hp.place (saveBtn); hp.skip (12);
        hp.place (keysBtn); hp.place (playModeBtn); hp.skip (6); hp.place (keyInfo, jmax (60, h.getRight() - 455 - hp.x));
        RowPlacer hr (h.withTrimmedLeft (h.getWidth() - 450), 5);
        hr.place (exchangeBtn); hr.place (chainsBtn); hr.place (mapBtn); hr.place (settingsBtn); hr.skip (8);
        hr.place (zoomOut, 24); hr.place (zoomPct, 48); hr.place (zoomIn, 24);
        a.removeFromTop (6);
        // keyboard + tray
        auto ks = a.removeFromTop (72);
        tray.setBounds (ks.removeFromRight (250));
        ks.removeFromRight (8);
        strip.setBounds (ks);
        a.removeFromTop (5);
        chainStrip.setBounds (a.removeFromTop (32));
        a.removeFromTop (7);
        // toolbar 1
        auto t1 = a.removeFromTop (28);
        toolArea = t1;
        RowPlacer r1 (t1, 5);
        r1.place (nameEd, 150); r1.place (typeBox, 84); r1.skip (6);
        r1.place (lLen, 44); r1.place (barsEd, 40); r1.place (lenMenuBtn); r1.place (bpbBox, 58); r1.skip (6);
        r1.place (lGrid, 30); r1.place (gridBox, 70); r1.skip (10);
        for (auto* b : toolBtns) r1.place (*b);
        r1.skip (10); r1.place (undoBtn); r1.place (redoBtn);
        a.removeFromTop (4);
        // toolbar 2
        auto t2 = a.removeFromTop (28);
        toolArea = toolArea.getUnion (t2);
        RowPlacer r2 (t2, 5);
        r2.place (lSwing, 40); r2.place (swingPreset, 104); r2.place (swingSlider, 190); r2.place (swingValue, 66); r2.skip (10);
        r2.place (quantBtn); r2.place (humBtn); r2.place (funcBtn); r2.skip (10);
        r2.place (lVel, 24); r2.place (velBox, 56); r2.skip (10);
        r2.place (recBtn); r2.place (recOptBtn); r2.place (playBtn); r2.place (stopBtn); r2.place (dragBtn);
        a.removeFromTop (8);
        // song lane
        auto songRow = a.removeFromTop (songH);
        songHeader.setBounds (songRow.removeFromLeft (200));
        songRow.removeFromRight (10);
        songLane.setBounds (songRow);
        songDiv.setBounds (a.removeFromTop (8));
        // status line
        status.setBounds (a.removeFromBottom (18));
        a.removeFromBottom (2);
        // roll
        rollArea = a;
        const int headW = 200, rulerH = 22, sb = 10;
        velH = jlimit (50, jmax (50, a.getHeight() - 120), velH);
        auto velRow = a.removeFromBottom (velH);
        velDiv.setBounds (a.removeFromBottom (8));
        auto top = a.removeFromTop (rulerH);
        {   auto z = top.withWidth (headW).reduced (4, 0);
            for (auto* b : { &zH1, &zH2, &zV1, &zV2, &zFit }) { b->setBounds (z.removeFromLeft (b->getWidth()).withSizeKeepingCentre (b->getWidth(), 18)); z.removeFromLeft (3); } }
        vScroll.setBounds (a.removeFromRight (sb)); top.removeFromRight (sb);
        hScroll.setBounds (a.removeFromBottom (sb).withTrimmedLeft (headW));
        headers.setBounds (a.removeFromLeft (headW)); top.removeFromLeft (headW);
        grid.setBounds (a); ruler.setBounds (top);
        velMode.setBounds (velRow.getX() + 8, velRow.getY() + 8, fitW ("Velocity"), btnH);
        guideBox.setBounds (velRow.getX() + 8, velRow.getY() + 8 + btnH + 6, 130, btnH);
        velLabelArea = velRow.removeFromLeft (headW);
        velRow.removeFromRight (sb);
        vel.setBounds (velRow);
        // pages float over the roll's right side
        auto pageArea = rollArea.withTrimmedLeft (jmax (0, rollArea.getWidth() - 520)).withTrimmedBottom (velH / 2);
        for (Page* pg : { (Page*) &quantPage, (Page*) &humPage, (Page*) &chainPage, (Page*) &setPage, (Page*) &recPage }) pg->setBounds (pageArea);
        mapPage.setBounds (rollArea.withTrimmedLeft (jmax (0, rollArea.getWidth() - 520)));
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
    Rectangle<int> headerTitle, rollArea, velLabelArea, headerArea, toolArea;

    void paintBody (Graphics& g)
    {
        // title: two-tone wordmark like the practice app
        g.setFont (font (Theme::get().fontFor ("txt:Title", "", 17.0f), 3)); g.setColour (th::text);
        g.drawText ("DRUM", headerTitle.withWidth (60), Justification::centredLeft);
        g.setColour (th::amber);
        g.drawText ("PLAYER", headerTitle.withTrimmedLeft (58), Justification::centredLeft);
        // cards behind the roll and the keyboard
        if (themed ("Header")) drawCard (g, headerArea.toFloat().expanded (4, 3), th::panel, 8, "Header");
        if (themed ("Toolbars")) drawCard (g, toolArea.toFloat().expanded (4, 3), th::panel, 8, "Toolbars");
        drawCard (g, rollArea.toFloat().expanded (2), th::panel, 10, "Roll card");
        drawCard (g, velLabelArea.toFloat(), th::panel, 6, "Roll card");
        g.setFont (font (10.5f, 1)); g.setColour (th::faint);
        const auto focus = st.focusLane >= 0 && st.song().lane (st.focusLane) ? st.song().lane (st.focusLane)->name : String ("all lanes");
        g.drawFittedText ("Showing: " + focus, velLabelArea.reduced (10, 6).withTrimmedTop (60), Justification::topLeft, 2);
    }

    // ------------------------------------------------------------ refresh
    void refreshAll()
    {
        refreshControls();
        if (openPage != nullptr) openPage->refresh();
        chainStrip.refresh();
        songHeader.refresh(); songLane.repaint();
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
        zoomPct.setButtonText (String ((int) std::round (scale * 100)) + "%");
        keysBtn.setToggleState (st.song().settings.keyMode == 0, dontSendNotification);
        playModeBtn.setToggleState (st.song().settings.keyMode == 1, dontSendNotification);
        strip.ensureVisible (st.song().editKey); bigStrip.ensureVisible (st.song().editKey);
        syncSwingPreset();
        status.setText (keyName (st.song().editKey) + "   " + (p.name.isNotEmpty() ? p.name : String ("(empty key)")) + "   |   "
                        + String ((int) p.notes.size()) + " notes   |   " + String (p.bars) + " x " + String (p.beatsPerBar) + "/4"
                        + "   |   pattern keys " + keyName (st.song().settings.zoneLow) + " to " + keyName (st.song().settings.zoneHigh)
                        + "   |   playing on " + builtInMaps()[(size_t) st.song().playMap].name + ", ch " + String (st.song().settings.outChannel), dontSendNotification);
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
        vScroll.setRangeLimits (0, jmax ((double) grid.getHeight(), (double) st.lanes().size() * st.laneH), dontSendNotification);
        vScroll.setCurrentRange (st.scrollY, grid.getHeight(), dontSendNotification);
        ruler.repaint(); headers.repaint(); grid.repaint(); vel.repaint();
    }
    void clampScroll()
    {
        const int maxX = jmax (0, (int) (st.contentTicks() / (double) PPQ * st.pxPerBeat + 40) - grid.getWidth());
        const int maxY = jmax (0, (int) st.lanes().size() * st.laneH - grid.getHeight());
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
        {   // Rec lamp: steady while recording, blinking during the count-in
            const int rs = proc.recStateUi.load();
            const bool lit = rs == 2 || (rs == 1 && (Time::getMillisecondCounter() / 200) % 2 == 0);
            if (lit != recBtn.getToggleState()) recBtn.setToggleState (lit, dontSendNotification);
        }
        if (headers.pollHits()) headers.repaint();
        if (const double sb = proc.songBeatUi.load(); sb >= 0 || lastSongBeat >= 0) { lastSongBeat = sb; songLane.repaint(); }
        if (const int ser = proc.lastInSerial.load(); ser != lastInSerialSeen) {     // which key was pressed, and what it did
            lastInSerialSeen = ser;
            const int n = proc.lastInNote.load(), k = proc.lastInKey.load();
            if (n >= 0) { strip.flash (n); bigStrip.flash (n); }
            if (learnFrom >= 0 && ser != learnSerial && n >= 0) {          // "move to the next key I press"
                const int from = learnFrom, to = n; learnFrom = -1;
                if (to == from) showToast ("That's the same key");
                else if (to < st.song().settings.zoneLow || to == st.song().settings.resolveStop()) showToast (keyName (to) + " isn't a pattern key");
                else {
                    st.applySong ("Move pattern", [from, to] (Song& s2) {
                        std::swap (s2.keys[(size_t) from], s2.keys[(size_t) to]);
                        for (auto& c : s2.chains) for (auto& step : c.steps) { if (step.key == from) step.key = to; else if (step.key == to) step.key = from; }
                        for (auto& c : s2.clips) { if (c.key == from) c.key = to; else if (c.key == to) c.key = from; }
                        s2.editKey = to;
                    });
                    showToast ("Moved to " + keyName (to));
                }
            }
            String t = "Key " + keyName (n) + " (" + String (n) + ")";
            if (k >= 0 && k == st.song().settings.resolveStop()) t << "  ->  STOP";
            else if (k >= 0) t << "  ->  pattern " + keyName (k) + (st.song().keys[(size_t) k].notes.empty() ? String ("  (empty key)") : "  " + st.song().keys[(size_t) k].name);
            else t << "  ->  drum: " << st.song().laneName (n);
            keyInfo.setText (t, dontSendNotification);
        }
        {   // new drums arriving from the keyboard or pads get their own lane straight away
            bool added = false;
            for (int nn = 0; nn < 128; ++nn) {
                const uint32 h = proc.inHits[(size_t) nn].load();
                if (h != lastIn[(size_t) nn]) { lastIn[(size_t) nn] = h; if (proc.edit.lane (nn) == nullptr) { proc.edit.ensureLane (nn); added = true; } }
            }
            if (added) { proc.publish(); repaintRoll(); }
        }
        if (Time::getMillisecondCounter() > toast.until && toast.text.isNotEmpty()) { toast.text = {}; toast.repaint(); }
        {
            const int cs = proc.chainStep.load() * 1000 + proc.chainPlaying.load();
            if (cs != lastChainState) { lastChainState = cs; chainStrip.repaint(); if (openPage == &chainPage) chainPage.refresh(); }
        }
    }
    int lastChainState = -1;
    std::array<uint32, 128> lastIn {};
    double lastSongBeat = -1;
    int learnFrom = -1, learnSerial = 0;

    static void repaintEverything (Component& c) { c.repaint(); for (auto* ch : c.getChildren()) repaintEverything (*ch); }
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

    void setKeyMode (int m)
    {
        st.applySong (m == 0 ? "Pattern mode" : "Play mode", [m] (Song& s) { s.settings.keyMode = m; });
        if (m == 0) showToast ("Pattern mode: keys from C-1 up play patterns");
        refreshControls();
    }
    void setTool (Tool t) { st.tool = t; st.stamping = false; for (int i = 0; i < toolBtns.size(); ++i) toolBtns[i]->setToggleState (i == (int) t, dontSendNotification); grid.repaint(); }

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
            for (auto& p : splitToPatterns (part, st.song().settings.splitBars)) {
                const int from = st.song().importMap;
                if (from >= 0 && from != st.song().playMap) convertPattern (p, st.song().map (from), st.song().map (st.song().playMap));
                pieces.push_back (std::move (p));
            }
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
        int nNotes = 0, vLo = 127, vHi = 0;
        for (auto& p : pieces) for (auto& n : p.notes) { ++nNotes; vLo = jmin (vLo, n.velocity); vHi = jmax (vHi, n.velocity); }
        showToast ("Imported " + String ((int) pieces.size()) + (pieces.size() == 1 ? " pattern to " + keyName (first) : " patterns to " + keyName (first) + " to " + keyName (last))
                   + ": " + String (nNotes) + " notes, velocities " + String (vLo) + "-" + String (vHi) + " exactly as in the file");
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
        m.addItem (1, "Edit");
        if (! p.notes.empty()) m.addItem (2, "Play");
        if (! p.isEmpty()) {
            PopupMenu type; for (int t = 0; t < 4; ++t) type.addItem (10 + t, typeName ((PatternType) t), true, (int) p.type == t);
            m.addSubMenu ("Type", type);
            m.addItem (3, "Copy pattern");
        }
        if (hasPatternClip) m.addItem (4, "Paste pattern here");
        if (! p.isEmpty()) {
            m.addItem (5, "Duplicate to the next free key");
            m.addItem (8, "Move to the next key I press");
        }
        if (! p.notes.empty()) {
            m.addItem (6, "Export as MIDI file...");
            PopupMenu chain;
            for (int c = 0; c < (int) s.chains.size() && c < 200; ++c) chain.addItem (2000 + c, "Add to " + (s.chains[(size_t) c].name.isNotEmpty() ? s.chains[(size_t) c].name : "Chain " + String (c + 1)));
            m.addSubMenu ("Chains", chain);
        }
        if (! p.isEmpty()) m.addItem (7, "Clear");
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
                case 8: learnFrom = k; learnSerial = proc.lastInSerial.load(); showToast ("Press the key on your keyboard where " + keyName (k) + "'s pattern should go"); break;
                default:
                    if (r >= 10 && r < 14) { const int t = r - 10; st.applySong ("Pattern type", [k, t] (Song& s2) { s2.keys[(size_t) k].type = (PatternType) t; }); }
                    else if (r >= 2000 && r < 2200) { const int c = r - 2000; st.applySong ("Add to chain", [k, c] (Song& s2) { if (c < (int) s2.chains.size()) s2.chains[(size_t) c].steps.push_back ({ k, 1 }); }); showToast ("Added to the chain"); }
                    break;
            }
        });
    }

public:
    void paint (Graphics& g) override { g.fillAll (th::bg); }
    void paintOverChildrenOfBody (Graphics&) {}
};

} // namespace dp
