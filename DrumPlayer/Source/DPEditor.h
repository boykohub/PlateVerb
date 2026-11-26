// Drum Player: the main window.
#pragma once
#include "DPGrooves.h"

std::vector<std::wstring> dpClipboardFiles();      // DPClipboard.cpp

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
        // the whole line is the handle: drag it anywhere along its length
        g.setColour (Colours::white.withAlpha (isMouseOver() || isMouseButtonDown() ? 0.30f : 0.07f));
        g.fillRect (getLocalBounds().toFloat().withSizeKeepingCentre ((float) getWidth(), isMouseOver() ? 3.0f : 1.0f));
    }
    void mouseEnter (const MouseEvent&) override { repaint(); }
    void mouseExit (const MouseEvent&) override { repaint(); }
    void mouseDown (const MouseEvent&) override { start = getValue ? getValue() : 0; }
    void mouseDrag (const MouseEvent& e) override { if (setValue) setValue (start + (invert ? -1 : 1) * e.getDistanceFromDragStartY()); }
private:
    int start = 0;
};

// a bar you drag left or right (the width of the drum list)
class SideDivider : public Component
{
public:
    std::function<int()> getValue; std::function<void (int)> setValue;
    SideDivider() { setMouseCursor (MouseCursor::LeftRightResizeCursor); }
    void paint (Graphics& g) override
    {
        g.setColour (Colours::white.withAlpha (isMouseOver() || isMouseButtonDown() ? 0.30f : 0.07f));
        g.fillRect (getLocalBounds().toFloat().withSizeKeepingCentre (isMouseOver() ? 3.0f : 1.0f, (float) getHeight()));
    }
    void mouseEnter (const MouseEvent&) override { repaint(); }
    void mouseExit (const MouseEvent&) override { repaint(); }
    void mouseDown (const MouseEvent&) override { start = getValue ? getValue() : 0; }
    void mouseDrag (const MouseEvent& e) override { if (setValue) setValue (start + e.getDistanceFromDragStartX()); }
private:
    int start = 0;
};

// a button that also answers a right-click (Rec: right-click for the recording options)
struct RightClickButton : public TextButton
{
    std::function<void()> onRight;
    void mouseDown (const MouseEvent& e) override { if (e.mods.isPopupMenu()) { if (onRight) onRight(); return; } TextButton::mouseDown (e); }
    void mouseUp (const MouseEvent& e) override { if (e.mods.isPopupMenu()) return; TextButton::mouseUp (e); }
};

// the settings button: a cog wheel
struct GearButton : public TextButton
{
    void paintButton (Graphics& g, bool over, bool down) override
    {
        getLookAndFeel().drawButtonBackground (g, *this, {}, over, down);
        const auto c = getLocalBounds().toFloat().getCentre();
        const float R = jmin (getWidth(), getHeight()) * 0.36f, r = R * 0.72f;
        Path p; const int teeth = 8;
        for (int i = 0; i < teeth * 2; ++i) {
            const float a0 = MathConstants<float>::twoPi * (float) i / (teeth * 2) - 0.2f, a1 = a0 + MathConstants<float>::twoPi / (teeth * 2);
            const float rad = (i % 2 == 0) ? R : r;
            const float x0 = c.x + rad * std::sin (a0), y0 = c.y - rad * std::cos (a0), x1 = c.x + rad * std::sin (a1), y1 = c.y - rad * std::cos (a1);
            if (i == 0) p.startNewSubPath (x0, y0); else p.lineTo (x0, y0);
            p.lineTo (x1, y1);
        }
        p.closeSubPath();
        p.addEllipse (c.x - R * 0.34f, c.y - R * 0.34f, R * 0.68f, R * 0.68f); p.setUsingNonZeroWinding (false);
        g.setColour (getToggleState() ? th::ink : th::text); g.fillPath (p);
    }
};

// the window size: type a number, or right-click for quick sizes
struct ZoomBox : public Label
{
    std::function<void()> onRight;
    ZoomBox() { setEditable (true, true, false); setJustificationType (Justification::centred); }
    void mouseDown (const MouseEvent& e) override { if (e.mods.isPopupMenu()) { if (onRight) onRight(); return; } Label::mouseDown (e); }
    void mouseUp (const MouseEvent& e) override { if (e.mods.isPopupMenu()) return; Label::mouseUp (e); }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (Colours::black.withAlpha (0.28f)); g.fillRoundedRectangle (r, 5);
        g.setColour (Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (r, 5, 0.8f);
        if (! isBeingEdited()) { g.setFont (dp::font (12.0f, 2)); g.setColour (th::text); g.drawText (getText(), getLocalBounds(), Justification::centred); }
    }
};

// the tempo: drag up or down to change it, double-click to type, right-click for Studio One's tempo
struct TempoLabel : public Label
{
    std::function<double()> get; std::function<void (double)> set; std::function<void()> onRight;
    TempoLabel() { setMouseCursor (MouseCursor::UpDownResizeCursor); }
    void mouseDown (const MouseEvent& e) override { if (e.mods.isPopupMenu()) { if (onRight) onRight(); return; } start = get ? get() : 120.0; dragged = false; Label::mouseDown (e); }
    void mouseDrag (const MouseEvent& e) override
    {
        const int dy = -e.getDistanceFromDragStartY();
        if (std::abs (dy) < 2 && ! dragged) return;
        dragged = true;
        const double step = e.mods.isShiftDown() ? 0.1 : 0.5;                 // half a BPM per pixel, Shift for fine
        if (set) set (std::round ((start + dy * step) * 10.0) / 10.0);
    }
    void mouseUp (const MouseEvent& e) override { if (e.mods.isPopupMenu()) return; if (! dragged) Label::mouseUp (e); }
    double start = 120.0; bool dragged = false;
};

// the Option-key strip: a long bar of the tools you pick (right-click it to choose)
class ToolStrip : public Component
{
public:
    struct Feature { String id, label, tip; std::function<void (Component*)> run; };
    std::vector<Feature> all; StringArray on;
    std::function<void()> onChanged;
    String hint = "Right-click here to choose the tools for this strip (Option / Alt shows and hides it)";
    bool compact = false;                         // no hint text (the tool shelf in the toolbar)
    void rebuild()
    {
        btns.clear();
        for (auto& id : on)
            for (auto& f : all)
                if (f.id == id) {
                    auto* b = btns.add (new StripButton());
                    setupButton (*this, *b, f.label, false, f.tip + " (right-click: choose the tools on this strip)");
                    auto run = f.run; auto* bp = b;
                    b->onClick = [run, bp] { if (run) run (bp); };
                    b->onRight = [this] { chooseMenu(); };
                    b->onMoved = [this, id = f.id] (int x) { moveTool (id, x); };
                }
        resized(); repaint();
    }
    void resized() override
    {
        int x = 8;
        for (auto* b : btns) { b->setTopLeftPosition (x, (getHeight() - b->getHeight()) / 2); x += b->getWidth() + 5; }
    }
    void paint (Graphics& g) override
    {
        if (btns.isEmpty() || compact) {
            g.setFont (font (11.0f, 1)); g.setColour (th::faint.withAlpha (btns.isEmpty() ? 1.0f : 0.6f));
            auto r = getLocalBounds().reduced (10, 0);
            if (! btns.isEmpty()) r.setLeft (btns.getLast()->getRight() + 12);
            g.drawText (hint, r, Justification::centredLeft, true);
        }
    }
    void mouseDown (const MouseEvent& e) override { if (e.mods.isPopupMenu()) chooseMenu(); }
    void mouseDoubleClick (const MouseEvent&) override { chooseMenu(); }
    // Alt+Shift drag: put a tool somewhere else on the strip
    void moveTool (const String& id, int x)
    {
        int idx = 0; for (auto* b : btns) if (b->getBounds().getCentreX() < x) ++idx;
        StringArray next; for (auto& t : on) if (t != id) next.add (t);
        int pos = 0, seen = 0; for (; pos < next.size(); ++pos) { if (seen >= idx) break; ++seen; }
        next.insert (jmin (pos, next.size()), id);
        on = next; rebuild(); if (onChanged) onChanged();
    }
    void chooseMenu()
    {
        PopupMenu m; m.addSectionHeader ("Tools on the strip");
        for (int i = 0; i < (int) all.size(); ++i) m.addItem (i + 1, all[(size_t) i].label, true, on.contains (all[(size_t) i].id));
        m.addSeparator(); m.addItem (1000, "All"); m.addItem (1001, "None");
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMousePosition(), [sp = Component::SafePointer<ToolStrip> (this)] (int r) {
            if (sp == nullptr || r <= 0) return;
            if (r == 1000) { sp->on.clear(); for (auto& f : sp->all) sp->on.add (f.id); }
            else if (r == 1001) sp->on.clear();
            else {
                StringArray next;                                  // keep the order of the list
                const auto& id = sp->all[(size_t) r - 1].id;
                const bool add = ! sp->on.contains (id);
                for (auto& f : sp->all) if ((f.id == id && add) || (f.id != id && sp->on.contains (f.id))) next.add (f.id);
                sp->on = next;
            }
            sp->rebuild(); if (sp->onChanged) sp->onChanged();
            sp->chooseMenu();                                     // stay open for the next pick
        });
    }
private:
    struct StripButton : public TextButton
    {
        std::function<void()> onRight; std::function<void (int)> onMoved; bool moving = false;
        void mouseDown (const MouseEvent& e) override
        {
            if (e.mods.isPopupMenu()) { if (onRight) onRight(); return; }
            if (e.mods.isAltDown() && e.mods.isShiftDown()) { moving = true; setMouseCursor (MouseCursor::DraggingHandCursor); toFront (false); return; }
            TextButton::mouseDown (e);
        }
        void mouseDrag (const MouseEvent& e) override
        {
            if (moving) { setTopLeftPosition (getX() + e.getDistanceFromDragStartX() - (getX() - startX), getY()); return; }
            TextButton::mouseDrag (e);
        }
        void mouseUp (const MouseEvent& e) override
        {
            if (moving) { moving = false; setMouseCursor (MouseCursor::NormalCursor); if (onMoved) onMoved (getBounds().getCentreX()); return; }
            if (e.mods.isPopupMenu()) return; TextButton::mouseUp (e);
        }
        void moved() override { if (! moving) startX = getX(); }
        int startX = 0;
    };
    OwnedArray<StripButton> btns;
};

// the pattern player, note editor and velocities in their own window
class DetachWindow : public DocumentWindow
{
public:
    struct Content : public Component
    {
        Body inner; float scale = 1.0f;
        std::function<bool (const KeyPress&)> onKey; std::function<void (const ModifierKeys&)> onMods;
        Content() { addAndMakeVisible (inner); setWantsKeyboardFocus (true); }
        void resized() override
        {
            inner.setTransform (AffineTransform::scale (scale));
            inner.setBounds (0, 0, (int) std::round (getWidth() / scale), (int) std::round (getHeight() / scale));
        }
        bool keyPressed (const KeyPress& k) override { return onKey && onKey (k); }
        void modifierKeysChanged (const ModifierKeys& m) override { if (onMods) onMods (m); }
    } content;
    std::function<void()> onClose;
    DetachWindow() : DocumentWindow ("Drum Player - editor", th::bg, DocumentWindow::closeButton | DocumentWindow::maximiseButton | DocumentWindow::minimiseButton)
    {
        setUsingNativeTitleBar (true);
        setContentNonOwned (&content, false);
        setResizable (true, false);
        setResizeLimits (600, 360, 10000, 10000);
    }
    void closeButtonPressed() override { if (onClose) onClose(); }
};

// ---------------------------------------------------------------- export every key into folders, like EZdrummer's groove browser:
// main folder / style / tempo type / feel / tune (BPM) / part (the key's colour group) / Variation 01.mid
class ExportPage : public Page
{
public:
    explicit ExportPage (EditState& e) : Page ("EXPORT ALL KEYS TO FOLDERS"), st (e)
    {
        setupLabel (*this, lFolder, "Main folder"); setupCombo (*this, folder, {}, 1, "The top folder. In EZdrummer 2: Settings > MIDI Libraries > Add Folder to User Libraries, once per main folder");
        setupButton (*this, newFolder, "New folder...", false, "Make a new main folder next to the current one");
        setupButton (*this, chooseFolder, "Choose...", false, "Pick any folder as the main folder");
        setupLabel (*this, lStyle, "Style"); setupCombo (*this, style, {}, 1, "Fusion, Rock, Latin... (type a new one)"); style.setEditableText (true);
        setupLabel (*this, lTempo, "Tempo type"); setupCombo (*this, tempoType, {}, 1, "Halftime, Midtempo, Uptempo... (type a new one)"); tempoType.setEditableText (true);
        setupLabel (*this, lFeel, "Feel"); setupCombo (*this, feel, {}, 1, "Straight 4/4, Swing 4/4... (type a new one)"); feel.setEditableText (true);
        setupLabel (*this, lTune, "Tune");
        addAndMakeVisible (tune); tune.setFont (font (12.5f, 2)); tune.setTextToShowWhenEmpty ("Tune name", th::faint);
        setupButton (*this, addBpm, "Add the tempo", true, "Add the tempo to the tune folder, like 'Straight 44 140 BPM'");
        setupLabel (*this, lNames, "Files named"); setupCombo (*this, naming, { "Variation 01, 02...", "Pattern names" });
        setupLabel (*this, path, "", 11.5f, th::text); path.setJustificationType (Justification::topLeft);
        setupButton (*this, exportBtn, "Export"); exportBtn.onClick = [this] { run(); };
        setupButton (*this, showBtn, "Show folder"); showBtn.onClick = [this] { auto f = tuneDir(); (f.isDirectory() ? f : File (folder.getText())).revealToUser(); };
        newFolder.onClick = [this] { makeFolder(); };
        chooseFolder.onClick = [this] {
            chooser = std::make_unique<FileChooser> ("Main folder", File (folder.getText()).isDirectory() ? File (folder.getText()) : File::getSpecialLocation (File::userDocumentsDirectory));
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [this] (const FileChooser& fc) {
                auto f = fc.getResult(); if (f.isDirectory()) { remember ("Folders", f.getFullPathName()); fillLists(); folder.setText (f.getFullPathName(), dontSendNotification); update(); } });
        };
        for (auto* c : { &folder, &style, &tempoType, &feel, &naming }) c->onChange = [this] { update(); };
        tune.onTextChange = [this] { update(); };
        addBpm.onClick = [this] { update(); };
    }
    std::function<String()> defaultTune;            // the project or preset name
    std::function<double()> currentBpm;
    std::function<void (const String&)> toast;

    void refresh() override
    {
        fillLists();
        const auto& e = st.song().exportInfo;
        auto lists = loadLists();
        folder.setText (e.folder.isNotEmpty() ? e.folder : lists[0].isEmpty() ? File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Drum Player Grooves").getFullPathName() : lists[0][0], dontSendNotification);
        style.setText (e.style, dontSendNotification); tempoType.setText (e.tempoType, dontSendNotification);
        feel.setText (e.feel.isNotEmpty() ? e.feel : suggestFeel(), dontSendNotification);
        tune.setText (e.tune.isNotEmpty() ? e.tune : (defaultTune ? defaultTune() : String()), false);
        addBpm.setToggleState (e.addBpm, dontSendNotification);
        naming.setSelectedItemIndex (e.naming, dontSendNotification);
        update();
    }
    void resized() override
    {
        auto a = content();
        const int lw = 110;
        {   auto r = row (a, lFolder, lw);
            showBtn.setBounds (r.removeFromRight (showBtn.getWidth()).withSizeKeepingCentre (showBtn.getWidth(), btnH)); r.removeFromRight (4);
            chooseFolder.setBounds (r.removeFromRight (chooseFolder.getWidth()).withSizeKeepingCentre (chooseFolder.getWidth(), btnH)); r.removeFromRight (4);
            newFolder.setBounds (r.removeFromRight (newFolder.getWidth()).withSizeKeepingCentre (newFolder.getWidth(), btnH)); r.removeFromRight (6);
            folder.setBounds (r); }
        style.setBounds (row (a, lStyle, lw).removeFromLeft (220));
        tempoType.setBounds (row (a, lTempo, lw).removeFromLeft (220));
        feel.setBounds (row (a, lFeel, lw).removeFromLeft (220));
        {   auto r = row (a, lTune, lw); tune.setBounds (r.removeFromLeft (260)); r.removeFromLeft (8);
            addBpm.setBounds (r.removeFromLeft (addBpm.getWidth()).withSizeKeepingCentre (addBpm.getWidth(), btnH)); }
        naming.setBounds (row (a, lNames, lw).removeFromLeft (220));
        a.removeFromTop (6);
        auto b = a.removeFromBottom (btnH + 6);
        exportBtn.setBounds (b.removeFromLeft (exportBtn.getWidth()).withSizeKeepingCentre (exportBtn.getWidth(), btnH));
        path.setBounds (a);
    }
private:
    EditState& st;
    Label lFolder, lStyle, lTempo, lFeel, lTune, lNames, path;
    ComboBox folder, style, tempoType, feel, naming;
    TextEditor tune;
    TextButton newFolder, chooseFolder, addBpm, exportBtn, showBtn;
    std::unique_ptr<FileChooser> chooser;

    // folder names: a "/" (as in 4/4) becomes a look-alike slash, since Windows doesn't allow "/" in names
    static String folderName (const String& t) { return File::createLegalFileName (t.replace ("/", String::charToString ((juce_wchar) 0x2215))); }
    String suggestFeel()
    {
        const auto& p = st.pat();
        return String (st.proc.apvts.getRawParameterValue ("swing") != nullptr && st.proc.apvts.getRawParameterValue ("swing")->load() > 52.0f ? "Swing " : "Straight ")
               + String (p.beatsPerBar) + "/4";
    }
    String tuneFolderName()
    {
        auto t = tune.getText().trim();
        if (t.isEmpty()) t = "My tune";
        if (addBpm.getToggleState() && currentBpm) t << " " << String (roundToInt (currentBpm())) << " BPM";
        return folderName (t);
    }
    File tuneDir()
    {
        File d (folder.getText().trim());
        if (! File::isAbsolutePath (folder.getText().trim())) return {};
        for (auto* c : { &style, &tempoType, &feel }) { const auto t = c->getText().trim(); if (t.isNotEmpty()) d = d.getChildFile (folderName (t)); }
        return d.getChildFile (tuneFolderName());
    }
    String partName (const Pattern& p)
    {
        const auto& g = st.song().groups;
        if (p.group >= 0 && p.group < (int) g.size() && g[(size_t) p.group].name.trim().isNotEmpty()) return folderName (g[(size_t) p.group].name.trim());
        return "Other";
    }
    // the parts and their keys, in key order
    std::map<String, std::vector<int>> parts()
    {
        std::map<String, std::vector<int>> m;
        const auto& s = st.song();
        for (int k = 0; k < 128; ++k) if (! s.keys[(size_t) k].notes.empty() && ! s.settings.isStop (k)) m[partName (s.keys[(size_t) k])].push_back (k);
        return m;
    }
    void update()
    {
        const auto d = tuneDir();
        const auto ps = parts();
        int n = 0; String names; for (auto& [name, keys] : ps) { n += (int) keys.size(); names << (names.isEmpty() ? "" : ", ") << name << " (" << (int) keys.size() << ")"; }
        String t;
        if (d == File()) t << "Choose a main folder first.";
        else {
            t << d.getFullPathName() << File::getSeparatorString() << "...\n\n";
            t << n << " grooves into " << (int) ps.size() << " part folders: " << (names.isEmpty() ? String ("none - put grooves on keys first") : names);
            t << "\n\nEach part is a key colour (Intro, Verse, Pre Chorus, Chorus, Bridge, Fills...). Keys with no colour go to 'Other'.";
            t << "\nIn EZdrummer 2, add the main folder once: Settings > MIDI Libraries > Add Folder to User Libraries.";
        }
        path.setText (t, dontSendNotification);
    }
    void run()
    {
        const auto d = tuneDir();
        if (d == File()) { if (toast) toast ("Choose a main folder first"); return; }
        const auto ps = parts();
        if (ps.empty()) { if (toast) toast ("No grooves on the keys to export"); return; }
        const auto& s = st.song();
        const double bpm = currentBpm ? currentBpm() : 0.0;
        const bool byName = naming.getSelectedItemIndex() == 1;
        int written = 0;
        for (auto& [name, keys] : ps) {
            auto dir = d.getChildFile (name);
            if (! dir.createDirectory()) continue;
            int num = 1;
            for (int k : keys) {
                const auto& p = s.keys[(size_t) k];
                File f;
                if (byName) {
                    auto base = File::createLegalFileName (p.name.trim().isNotEmpty() ? p.name.trim() : "Groove " + keyName (k));
                    f = dir.getChildFile (base + ".mid");
                    for (int i = 2; f.existsAsFile() && written > 0 && i < 999; ++i) {        // two patterns with one name in this export
                        bool ours = false; for (auto& w : writtenNow) if (w == f) ours = true;
                        if (! ours) break;
                        f = dir.getChildFile (base + " (" + String (i) + ").mid");
                    }
                } else f = dir.getChildFile ("Variation " + String (num++).paddedLeft ('0', 2) + ".mid");
                writeMidiFile ({ &p }, f, s.settings.outChannel, bpm);
                writtenNow.push_back (f); ++written;
                // the other version goes next to it, so the original is never lost
                const Pattern* other = p.showingOriginal ? p.edited.get() : p.original.get();
                if (other != nullptr) {
                    auto f2 = f.getSiblingFile (f.getFileNameWithoutExtension() + (p.showingOriginal ? " (edited)" : " (original)") + ".mid");
                    writeMidiFile ({ other }, f2, s.settings.outChannel, bpm); writtenNow.push_back (f2);
                }
            }
        }
        writtenNow.clear();
        // remember this tune's choices and the lists
        ExportInfo e { folder.getText().trim(), style.getText().trim(), tempoType.getText().trim(), feel.getText().trim(), tune.getText().trim(), addBpm.getToggleState(), naming.getSelectedItemIndex() };
        st.applySong ("Export settings", [e] (Song& so) { so.exportInfo = e; });
        remember ("Folders", e.folder); remember ("Styles", e.style); remember ("TempoTypes", e.tempoType); remember ("Feels", e.feel);
        fillLists(); update();
        if (toast) toast ("Exported " + String (written) + " grooves to " + d.getFileName());
    }
    std::vector<File> writtenNow;

    // ---- the lists you've used before (shared by all tunes)
    static std::array<StringArray, 4> loadLists()
    {
        std::array<StringArray, 4> l;
        static const char* names[] = { "Folders", "Styles", "TempoTypes", "Feels" };
        if (auto x = XmlDocument::parse (exportListsFile()))
            for (int i = 0; i < 4; ++i) if (auto* e = x->getChildByName (names[i])) for (auto* it : e->getChildIterator()) l[(size_t) i].add (it->getStringAttribute ("v"));
        return l;
    }
    static void remember (const String& list, const String& v)
    {
        if (v.isEmpty()) return;
        auto x = XmlDocument::parse (exportListsFile());
        if (x == nullptr) x = std::make_unique<XmlElement> ("ExportLists");
        auto* e = x->getChildByName (list); if (e == nullptr) e = x->createNewChildElement (list);
        for (auto* it : e->getChildIterator()) if (it->getStringAttribute ("v") == v) { e->removeChildElement (it, true); break; }
        auto* n = new XmlElement ("I"); n->setAttribute ("v", v); e->insertChildElement (n, 0);     // newest first
        x->writeTo (exportListsFile());
    }
    void fillLists()
    {
        auto l = loadLists();
        StringArray styles { "Fusion", "Rock", "Pop", "Funk", "Jazz", "Latin", "Blues", "Metal", "Country" };
        StringArray tempos { "Halftime", "Slow", "Midtempo", "Uptempo", "Fast" };
        StringArray feels { "Straight 4/4", "Swing 4/4", "Shuffle 4/4", "Straight 3/4", "Swing 3/4", "6/8", "12/8" };
        auto fill = [] (ComboBox& c, const StringArray& used, const StringArray& defaults) {
            const auto keep = c.getText(); c.clear (dontSendNotification);
            StringArray all (used); for (auto& d : defaults) if (! all.contains (d)) all.add (d);
            for (int i = 0; i < all.size(); ++i) c.addItem (all[i], i + 1);
            c.setText (keep, dontSendNotification);
        };
        auto folders = l[0]; folders.removeEmptyStrings();
        fill (folder, folders, {}); fill (style, l[1], styles); fill (tempoType, l[2], tempos); fill (feel, l[3], feels);
    }
    void makeFolder()
    {
        auto* w = new AlertWindow ("New main folder", "Name of the new folder (made next to the current main folder):", MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("name", "", "Folder name");
        w->addButton ("Make", 1, KeyPress (KeyPress::returnKey)); w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        w->enterModalState (true, ModalCallbackFunction::create ([this, w] (int r) {
            const auto name = File::createLegalFileName (w->getTextEditorContents ("name").trim());
            if (r == 1 && name.isNotEmpty()) {
                File cur (folder.getText().trim());
                File parent = File::isAbsolutePath (cur.getFullPathName()) && cur.getFullPathName().isNotEmpty() ? cur.getParentDirectory() : File::getSpecialLocation (File::userDocumentsDirectory);
                auto d = parent.getChildFile (name);
                if (d.createDirectory()) { remember ("Folders", d.getFullPathName()); fillLists(); folder.setText (d.getFullPathName(), dontSendNotification); update(); if (toast) toast ("Made " + d.getFullPathName()); }
                else if (toast) toast ("Couldn't make " + d.getFullPathName());
            }
        }), true);
    }
};

class DrumPlayerEditor : public AudioProcessorEditor, public DragAndDropContainer, public FileDragAndDropTarget, private Timer, public MenuBarModel
{
public:
    explicit DrumPlayerEditor (DrumPlayerProcessor& p)
        : AudioProcessorEditor (p), proc (p), st (p),
          strip (st, false), bigStrip (st, true), tray (st), bigTray (st), chainStrip (st), ruler (st), headers (st), grid (st), vel (st), songLane (st), songHeader (st, songLane),
          quantPage (st), humPage (st), chainPage (st), setPage (st), presetPage (st), mapPage (st), recPage (st), groovePage (st), mixerPage (st), exportPage (st), setsPage (st)
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
        st.zoomed = [this] { proc.uiZoomX = st.pxPerBeat / 80.0f; clampScroll(); repaintRoll(); };
        st.playFrom = [this] (int t) { proc.uiPlayFrom (st.song().editKey, t); };
        grid.onDropPattern = [this] (const String& d) { dropOnEditor (d); };
        songLane.onDropPattern = [this] (const String& d, double at) { dropParkOnSong (d, at); };

        // ---------------- header
        setupButton (body, prevPreset, "<", false, "Previous preset");
        setupButton (body, nextPreset, ">", false, "Next preset");
        setupButton (body, presetName, "Presets", true, "Preset browser");
        setupButton (body, exchangeBtn, "Exchange", true, "Big keyboard for moving patterns between keys");
        setupButton (body, keysBtn, "Pattern mode", true, "Keys play and arrange patterns: one per key from C-1 up (the stop key sits below C-1). Empty keys play their drum, so you can add hits live");
        setupButton (body, playModeBtn, "Drum kit mode", true, "Every key plays one drum: for finger drumming, playing live and recording");
        keysBtn.setRadioGroupId (61); playModeBtn.setRadioGroupId (61);
        keysBtn.onClick = [this] { setKeyMode (0); };
        playModeBtn.onClick = [this] { setKeyMode (1); };
        body.addAndMakeVisible (keyInfo); keyInfo.setFont (font (11.5f, 2)); keyInfo.setColour (Label::textColourId, th::dim);
        keyInfo.setComponentID ("Pressed key"); keyInfo.setText ("Press a key...", dontSendNotification); keyInfo.setJustificationType (Justification::centredLeft);
        setupButton (body, chainsBtn, "Chains", true);
        setupButton (body, groovesBtn, "Grooves", true, "Browse your MIDI grooves by style and type, listen, and drag them onto keys");
        setupButton (body, mixerBtn, "Mixer", true, "Pan and meters per drum (View > Mixer)");
        mixerBtn.onClick = [this] { togglePage (mixerPage, mixerBtn); };
        groovesBtn.onClick = [this] { togglePage (groovePage, groovesBtn); };
        setupButton (body, setsBtn, "Groove sets", true, "Your groove sets: folders of grooves (Style / Tempo / Feel / Tune / Part), to load onto the keys or save your keys as");
        setsBtn.onClick = [this] { togglePage (setsPage, setsBtn); };
        setupButton (body, settingsBtn, "Settings", true, "Settings: general, drum map, inner workings and look");
        settingsBtn.setButtonText ({}); settingsBtn.setSize (btnH + 8, btnH);
        body.addAndMakeVisible (zoomPct); zoomPct.setComponentID ("Window size"); zoomPct.setFont (font (12.0f, 2));
        zoomPct.setTooltip ("Window size: click and type a number (like 120), or right-click for quick sizes. You can also drag the window's corner");
        zoomPct.onTextChange = [this] { const int v = zoomPct.getText().retainCharacters ("0123456789").getIntValue(); if (v >= 50) setScale (v / 100.0f); else refreshControls(); };
        prevPreset.onClick = [this] { presetPage.step (-1); refreshAll(); };
        nextPreset.onClick = [this] { presetPage.step (1); refreshAll(); };
        presetName.onClick = [this] { togglePage (presetPage, presetName); };
        exchangeBtn.onClick = [this] { toggleExchange(); };
        chainsBtn.onClick = [this] { togglePage (chainPage, chainsBtn); };
        settingsBtn.onClick = [this] { togglePage (setPage, settingsBtn); };
        zoomOut.onClick = [this] { setScale (scale - 0.1f); };
        zoomIn.onClick = [this] { setScale (scale + 0.1f); };
        zoomPct.onRight = [this] { zoomMenu(); };

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
        StringArray vels; for (int v : { 127, 120, 110, 100, 90, 80, 70, 60, 50, 40, 30, 20 }) vels.add ("New notes: " + String (v));
        setupCombo (body, velBox, vels, 1, "Velocity for the notes you draw");
        velBox.setSelectedItemIndex (3, dontSendNotification);
        velBox.onChange = [this] { st.defaultVel = velBox.getText().retainCharacters ("0123456789").getIntValue(); };
        setupButton (body, recBtn, "Rec", false, "Drum kit mode: record your drumming into the pattern (added to its notes). Pattern mode: record the patterns you play onto the song lane. Right-click: recording options");
        recBtn.onClick = [this] {
            if (st.song().settings.keyMode == 0) {                  // Pattern mode: your key presses go onto the song lane
                const bool on = ! proc.songRecArmed.load();
                if (on) { proc.songRecCursor = songLane.cursorBeat; proc.publish(); }
                proc.setSongRecord (on);
                showToast (on ? "Recording: play your pattern keys, they go onto the song lane" : "Recording finished");
                return;
            }
            const bool start = proc.recStateUi.load() == 0;         // Drum kit mode: your hits go into the pattern
            proc.uiRecord (start, st.song().editKey);
        };
        recBtn.onRight = [this] { recPage.showMenu (&recBtn); };
        setupButton (body, editBtn, "Edit pattern", true, "Open the pattern editor under the song lane (notes, grid, length, velocities)");
        editBtn.onClick = [this] { proc.uiEditOpen = editBtn.getToggleState(); layout(); refreshAll(); };
        StringArray guides { "Guides: off", "Guides: 8", "Guides: 16", "Guides: 20", "Guides: 32" };
        setupCombo (body, guideBox, guides, 1, "Guide lines in the velocity lane. Bars click onto them; hold Alt to move freely");
        guideBox.setSelectedItemIndex (2, dontSendNotification);
        guideBox.onChange = [this] { static const int g[] = { 0, 8, 16, 20, 32 }; st.velGuide = g[jlimit (0, 4, guideBox.getSelectedItemIndex())]; vel.repaint(); };
        setupButton (body, playBtn, "Play", false, "Plays what you clicked last: the song lane, a pattern (a key or the editor) or the chain strip");
        playBtn.onClick = [this] { transportPlay(); };
        body.addAndMakeVisible (playsWhat); playsWhat.setFont (font (11.0f, 2)); playsWhat.setColour (Label::textColourId, th::dim);
        playsWhat.setComponentID ("Plays");
        // what Play plays follows where you click
        targetWatch.f = [this] (Component* c) {
            int t = playTarget;
            if (c == &chainStrip || chainStrip.isParentOf (c)) t = 2;
            else if (c == &songLane) t = 0;
            else if (c == &strip || c == &grid || c == &headers || c == &vel || c == &ruler) t = 1;
            if (t != playTarget) { playTarget = t; showPlays(); }
            if (c == &strip) { DrumPlayerEditor* self = this; Component::SafePointer<DrumPlayerEditor> sp (self); MessageManager::callAsync ([sp] { if (sp != nullptr) sp->setEditorOpen (true); }); }
        };
        for (Component* c : { (Component*) &chainStrip, (Component*) &songLane, (Component*) &strip, (Component*) &grid, (Component*) &headers, (Component*) &vel, (Component*) &ruler })
            c->addMouseListener (&targetWatch, true);
        setupButton (body, stopBtn, "Stop"); stopBtn.onClick = [this] { stopAll(); };
        // transport lamps: Rec and Play are green, Rec turns red (redder while recording), Play lights up bright green, Stop is blue
        playBtn.getProperties().set ("idle", (int) 0xff2f7a4c); playBtn.getProperties().set ("lit", (int) 0xff5cff8f);
        recBtn.getProperties().set ("idle", (int) 0xff2f7a4c);  recBtn.getProperties().set ("lit", (int) 0xffc8463f);
        stopBtn.getProperties().set ("idle", (int) 0xff2b5f7e); stopBtn.getProperties().set ("lit", (int) 0xff7fd1ff);
        for (TextButton* b : { (TextButton*) &recBtn, &playBtn, &stopBtn }) b->setSize (jmax (58, b->getWidth() + 14), btnH);
        dragBtn.setButtonText ("Drag MIDI"); dragBtn.setSize (fitW ("Drag MIDI"), btnH); body.addAndMakeVisible (dragBtn);
        dragBtn.getParts = [this] { return std::vector<const Pattern*> { &st.pat() }; };
        dragBtn.getChannel = [this] { return st.song().settings.outChannel; };

        // ---------------- roll
        body.addAndMakeVisible (ruler); body.addAndMakeVisible (headers); body.addAndMakeVisible (grid); body.addAndMakeVisible (vel);
        body.addAndMakeVisible (songLane); body.addAndMakeVisible (songHeader); songHeader.attach();
        body.addAndMakeVisible (songDiv); body.addAndMakeVisible (velDiv);
        songDiv.getValue = [this] { return songH; }; songDiv.setValue = [this] (int v) { songH = jlimit (40, 400, v); layout(); };
        velDiv.getValue = [this] { return velH; }; velDiv.setValue = [this] (int v) { velH = jlimit (50, 420, v); layout(); }; velDiv.invert = true;
        menuBar.setModel (this); body.addAndMakeVisible (menuBar);
        setupButton (body, pageClose, String::charToString ((juce_wchar) 0x00D7), false, "Close");
        pageClose.setComponentID ("Close X"); pageClose.setVisible (false);
        pageClose.onClick = [this] { closePages(); };
        setupButton (body, zH1, "W-", false, "Narrower (Ctrl+wheel)"); setupButton (body, zH2, "W+", false, "Wider (Ctrl+wheel)");
        setupButton (body, zV1, "H-", false, "Lower lanes (Alt+wheel)"); setupButton (body, zV2, "H+", false, "Taller lanes (Alt+wheel)");
        setupButton (body, zFit, "Fit", false, "Fit the pattern to the window (Ctrl+Shift+F)");
        zH1.onClick = [this] { zoomX (1.0f / 1.25f); }; zH2.onClick = [this] { zoomX (1.25f); };
        zV1.onClick = [this] { zoomY (-3); }; zV2.onClick = [this] { zoomY (3); };
        zFit.onClick = [this] { zoomToFit(); };
        {   // fit a number of bars on the screen
            StringArray fb { "Fit bars" }; for (int b = 1; b <= 16; ++b) fb.add (String (b) + (b == 1 ? " bar" : " bars"));
            setupCombo (body, fitBars, fb, 1, "Show this many bars across the editor");
            fitBars.setSelectedItemIndex (0, dontSendNotification);
            fitBars.onChange = [this] { const int b = fitBars.getSelectedItemIndex(); if (b > 0) fitToBars (b); };
        }
        body.addAndMakeVisible (headDiv);
        headW = jlimit (70, 360, proc.uiHeadW);
        headDiv.getValue = [this] { return headW; }; headDiv.setValue = [this] (int v) { headW = proc.uiHeadW = jlimit (70, 360, v); layout(); };
        body.addChildComponent (toolStrip);
        setupButton (body, backBtn, "Bring the editor back", false, "Close the editor window and put it back here");
        backBtn.onClick = [this] { reattach(); }; backBtn.setVisible (false);
        body.addChildComponent (detachedNote); detachedNote.setFont (font (13.0f, 2)); detachedNote.setJustificationType (Justification::centred);
        detachedNote.setColour (Label::textColourId, th::dim);
        detachedNote.setText ("The pattern player, note editor and velocities are in their own window", dontSendNotification);
        body.addAndMakeVisible (hScroll); body.addAndMakeVisible (vScroll);
        hScroll.setAutoHide (false); vScroll.setAutoHide (false);
        hScroll.addListener (&scrollRelay); vScroll.addListener (&scrollRelay);
        ruler.loopsChanged = [this] { grid.repaint(); };
        body.addAndMakeVisible (songScroll); songScroll.setAutoHide (false); songScroll.addListener (&scrollRelay);
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

        // what the lower lane shows: velocity or the chance that a note plays (random occurrence)
        setupButton (body, velShow, "Velocity", true, "The lane shows velocities"); setupButton (body, chanceShow, "Chance", true, "The lane shows each note's chance to play (100% always, 50% every other time at random)");
        velShow.setRadioGroupId (81); chanceShow.setRadioGroupId (81); velShow.setToggleState (true, dontSendNotification);
        velShow.onClick = [this] { st.probMode = false; vel.repaint(); refreshControls(); showToast ("The lane shows velocities: drag the bars to change them"); };
        chanceShow.onClick = [this] { st.probMode = true; vel.repaint(); refreshControls(); showToast ("The lane shows each note's chance to play (100% = always): drag the bars"); };
        setupButton (body, randBtn, "Randomize velocity", false, "Randomize the velocities (or the chances, when the lane shows Chance) of the selected notes, or of every note shown");
        randBtn.onClick = [this] {
            PopupMenu m;
            m.addSectionHeader (st.probMode ? "CHANCE TO PLAY" : "VELOCITY");
            if (! st.probMode) {
                for (int d : { 5, 10, 20, 30 }) m.addItem (d, "Vary by +/- " + String (d));
                m.addItem (100, "Random between 60 and 120"); m.addItem (101, "Random between 30 and 127");
            } else {
                m.addItem (200, "Random between 30% and 100%"); m.addItem (201, "Random between 60% and 100%"); m.addItem (202, "Some notes at 50%");
            }
            m.showMenuAsync (PopupMenu::Options().withTargetComponent (&randBtn), [this] (int r) {
                if (r <= 0) return;
                const int focus = st.focusLane;
                st.apply ("Randomize", [r, focus] (Pattern& p) {
                    std::mt19937 rng ((uint32) Time::getMillisecondCounter());
                    const bool anySel = anySelected (p);
                    if (! anySel) for (auto& n : p.notes) n.selected = focus < 0 || n.note == focus;
                    if (r < 100) { std::uniform_int_distribution<int> d (-r, r); for (auto& n : p.notes) if (n.selected) n.velocity = jlimit (1, 127, n.velocity + d (rng)); }
                    else if (r == 100) randomizeVelocity (p, 60, 120, rng);
                    else if (r == 101) randomizeVelocity (p, 30, 127, rng);
                    else if (r == 200) randomizeProbability (p, 30, 100, rng);
                    else if (r == 201) randomizeProbability (p, 60, 100, rng);
                    else if (r == 202) { std::uniform_int_distribution<int> d (0, 2); for (auto& n : p.notes) if (n.selected && d (rng) == 0) n.probability = 50; }
                    if (! anySel) selectAll (p, false);
                });
                int n = 0; for (auto& nn : st.pat().notes) n += (focus < 0 || nn.note == focus) ? 1 : 0;
                showToast (String (st.probMode ? "Randomized the chance of " : "Randomized the velocity of ") + (anySelected (st.pat()) ? String ("the selected notes") : String (n) + " notes"));
            });
        };
        body.addAndMakeVisible (velMode); setupButton (body, velMode, "Velocity shapes", false, "Ramps, accents, curves, louder, softer: on the selected notes, or every note shown");
        velMode.onClick = [this] {
            PopupMenu m;
            m.addItem (3, "Curve (sine, triangle, ramps)...");
            m.addItem (10020, "Ramp up"); m.addItem (10021, "Ramp down"); m.addItem (10022, "Accent the beats");
            m.addItem (10023, "Louder +10"); m.addItem (10024, "Softer -10"); m.addItem (10025, "Compress 50%"); m.addItem (10026, "Expand 150%");
            m.addItem (10030, "Randomize"); m.addItem (10011, "Humanize");
            PopupMenu fx; for (int x : { 127, 110, 100, 90, 80, 64, 48, 32 }) fx.addItem (10200 + x, String (x));
            m.addSubMenu ("All to", fx);
            m.showMenuAsync (PopupMenu::Options().withTargetComponent (&velMode), [this] (int r) {
                if (r == 3) openCurve();
                else if (r >= 10000) { runFunction (r - 10000); if (proc.undo.canUndo()) showToast (proc.undo.getUndoDescription() + ": done  (Ctrl+Z undoes it)"); }
            });
        };

        // ---------------- pages
        for (Page* pg : { (Page*) &quantPage, (Page*) &humPage, (Page*) &chainPage, (Page*) &setPage, (Page*) &presetPage, (Page*) &mapPage, (Page*) &recPage, (Page*) &groovePage, (Page*) &mixerPage, (Page*) &exportPage, (Page*) &setsPage }) body.addChildComponent (pg);
        setsPage.onLoadSet = [this] (const File& d) { loadGrooveSet (d); };
        setsPage.onLoadFile = [this] (const File& f, int k) { importFiles ({ f.getFullPathName() }, k); };
        setsPage.onSaveSet = [this] { togglePage (exportPage, exportDummy); };
        setsPage.onPreview = [this] (const File& f) { previewFile (f); };
        setsPage.knownRoots = [] {
            StringArray r;
            if (auto x = XmlDocument::parse (exportListsFile())) if (auto* e = x->getChildByName ("Folders")) for (auto* it : e->getChildIterator()) r.add (it->getStringAttribute ("v"));
            return r;
        };
        exportPage.defaultTune = [this] { return File (proc.uiProjectPath).existsAsFile() ? File (proc.uiProjectPath).getFileNameWithoutExtension() : presetName.getButtonText() != "Presets" ? presetName.getButtonText() : String(); };
        exportPage.currentBpm = [this] { return effectiveTempo(); };
        exportPage.toast = [this] (const String& t) { showToast (t); };
        {   // tempo
            setupLabel (body, lTempo, "TEMPO"); lTempo.setJustificationType (Justification::centredRight);
            setupButton (body, tempoDown, "-", false, "Slower (Shift: 0.1 BPM)"); setupButton (body, tempoUp, "+", false, "Faster (Shift: 0.1 BPM)");
            tempoDown.onClick = [this] { setOwnTempo (std::round (effectiveTempo()) - (ModifierKeys::currentModifiers.isShiftDown() ? 0.1 : 1.0)); };
            tempoUp.onClick = [this] { setOwnTempo (std::round (effectiveTempo()) + (ModifierKeys::currentModifiers.isShiftDown() ? 0.1 : 1.0)); };
            body.addAndMakeVisible (tempoLbl); tempoLbl.setEditable (false, true, false); tempoLbl.setJustificationType (Justification::centred);
            tempoLbl.setFont (font (20.0f, 3)); tempoLbl.setTooltip ("Drag up or down to change the tempo (Shift: fine), double-click to type. Locked: Studio One's tempo");
            tempoLbl.get = [this] { return effectiveTempo(); }; tempoLbl.set = [this] (double b) { setOwnTempo (b); };
            tempoLbl.onRight = [this] {
                PopupMenu m; const double hb = proc.hostBpm.load();
                m.addItem (1, "Take Studio One's tempo now (" + String (hb, std::abs (hb - std::round (hb)) < 0.05 ? 0 : 1) + " BPM)");
                m.addItem (2, "Lock to Studio One", true, st.song().settings.followHost);
                m.addSeparator(); for (int b : { 80, 90, 100, 110, 120, 130, 140 }) m.addItem (100 + b, String (b) + " BPM");
                m.showMenuAsync (PopupMenu::Options().withTargetComponent (&tempoLbl), [this, hb] (int r) {
                    if (r == 1) { setOwnTempo (hb); showToast ("Tempo from Studio One: " + String (hb, 1) + " BPM"); }
                    else if (r == 2) { const bool on = ! st.song().settings.followHost; st.applySong ("Follow tempo", [on] (Song& s2) { s2.settings.followHost = on; }); showTempo(); }
                    else if (r > 100) setOwnTempo (r - 100);
                });
            };
            tempoLbl.setColour (Label::backgroundColourId, Colours::black.withAlpha (0.25f));
            tempoLbl.onTextChange = [this] { const double b = tempoLbl.getText().getDoubleValue(); if (b >= 20.0) setOwnTempo (b); else showTempo(); };
            setupButton (body, tapBtn, "Tap", false, "Tap the tempo (4 taps or more)");
            tapBtn.onClick = [this] {
                const double now = Time::getMillisecondCounterHiRes();
                if (! taps.empty() && now - taps.back() > 2000.0) taps.clear();
                taps.push_back (now); if (taps.size() > 8) taps.erase (taps.begin());
                if (taps.size() >= 2) setOwnTempo (60000.0 / ((taps.back() - taps.front()) / (double) (taps.size() - 1)));
            };
            setupButton (body, followBtn, "Lock to Studio One", true, "On: always Studio One's tempo. Off: your tempo here while Studio One is stopped (while it plays, its tempo rules)");
            followBtn.onClick = [this] { const bool on = followBtn.getToggleState(); st.applySong ("Follow tempo", [on] (Song& s) { s.settings.followHost = on; }); showTempo(); };
        }
        groovePage.onLoad = [this] (const File& f, int k) { importFiles ({ f.getFullPathName() }, k); };
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
        setPage.look.onChanged = [this] { refitButtons (body); layout(); relayoutAll (body); repaintEverything (body); };
        presetPage.onImport = [this] { importDialog(); };
        body.addChildComponent (exchange);
        exchange.addAndMakeVisible (bigStrip); exchange.addAndMakeVisible (bigTray);
        exchange.addAndMakeVisible (exchangeHelp);
        setupLabel (exchange, exchangeHelp, "Drag a key onto another key to swap them. Drag to the parked tray to set one aside, then drag it back to any key. Right-click a key for more.", 12.0f, th::dim);

        addAndMakeVisible (toast); toast.setInterceptsMouseClicks (false, false);
        body.addAndMakeVisible (status); status.setFont (font (11.0f, 1)); status.setColour (Label::textColourId, th::faint);

        scale = proc.uiScale; DPLnF::menuScale = scale;
        setResizable (true, true);
        setResizeLimits (900, 560, 3200, 2000);
        setSize ((int) (1240 * scale), (int) (800 * scale));
        setupToolStrip();
        showPlays();
        savedSerial = proc.editSerial.load();
        refreshAll();
        startTimerHz (30);
    }
    ~DrumPlayerEditor() override
    {
        stopTimer();
        detached.reset();
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
        altClean = false;
        if (proc.uiLearnStopKey) {                                   // Settings > Learn: this key becomes the stop key
            proc.uiLearnStopKey = false; proc.uiStopKey = k.getKeyCode();
            showToast ("Stop key: " + KeyPress (k.getKeyCode()).getTextDescription());
            if (openPage != nullptr) openPage->refresh();
            return true;
        }
        if (! k.getModifiers().isCommandDown() && ! k.getModifiers().isAltDown()) {
            // the stop key stops at once; Space starts when nothing plays
            if (proc.uiStopKey != 0 && k.getKeyCode() == proc.uiStopKey) {
                if (anythingPlaying()) stopAll();
                else if (k.getKeyCode() == KeyPress::spaceKey) { if (songLane.hasKeyboardFocus (false)) { songHeader.refresh(); proc.uiSongPlay (true, songLane.cursorBeat); } else proc.uiTrigger (st.song().editKey); }
                return true;
            }
            if (k.getKeyCode() == KeyPress::spaceKey) {
                if (anythingPlaying()) stopAll();
                else if (songLane.hasKeyboardFocus (false)) { songHeader.refresh(); proc.uiSongPlay (true, songLane.cursorBeat); }
                else proc.uiTrigger (st.song().editKey);
                return true;
            }
        }
        const auto m = k.getModifiers();
        if (m.isCommandDown() && (k.getKeyCode() == 'S' || k.getKeyCode() == 's')) {          // Ctrl+S save, Ctrl+Shift+S save as
            if (! m.isShiftDown() && File (proc.uiProjectPath).existsAsFile()) saveProject (File (proc.uiProjectPath)); else saveProjectAs();
            return true;
        }
        const int code = k.getKeyCode();
        const juce_wchar ch = CharacterFunctions::toUpperCase (k.getTextCharacter());
        auto& p = st.pat();
        const bool plain = ! m.isCommandDown() && ! m.isAltDown() && ! m.isCtrlDown();
        if (m.isCommandDown() && code == 'Z') { if (m.isShiftDown()) proc.undo.redo(); else proc.undo.undo(); refreshAll(); return true; }
        if (m.isCommandDown() && code == 'Y') { proc.undo.redo(); refreshAll(); return true; }
        if (m.isCommandDown() && m.isShiftDown() && code == 'F') { zoomToFit(); return true; }
        if (m.isCommandDown() && ! m.isShiftDown() && (code == 'E' || code == 'e')) { setEditorOpen (! proc.uiEditOpen); return true; }

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
        if (code == KeyPress::escapeKey) {
            if (openPage != nullptr || exchange.isVisible()) closePages();
            else if (anySelected (p)) { selectAll (p, false); st.publish(); }
            else setEditorOpen (false);
            return true; }
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
        CallOutBox::launchAsynchronously (std::move (c), getLocalArea (&vel, vel.getLocalBounds()).withSizeKeepingCentre (40, 40).translated (0, -vel.getHeight() / 2), this);
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
                m.addItem (1, "New (clear everything)");
                m.addItem (12, "Open project..."); m.addItem (13, "Save project" + (File (proc.uiProjectPath).existsAsFile() ? "  (" + File (proc.uiProjectPath).getFileNameWithoutExtension() + ")" : String())); m.addItem (14, "Save project as...");
                m.addSeparator();
                m.addItem (2, "Presets..."); m.addItem (3, "Save preset");
                m.addSeparator(); m.addItem (4, "Import MIDI files..."); m.addItem (5, "Export this pattern as MIDI..."); m.addItem (6, "Export the song lane as MIDI...");
                m.addItem (15, "Export all keys to folders (EZdrummer style)...");
                m.addSeparator(); m.addItem (7, "Drum map..."); m.addItem (8, "Settings..."); m.addItem (9, "Look...");
                break;
            case 1:
                m.addItem (101, proc.undo.canUndo() ? "Undo " + proc.undo.getUndoDescription() + "  (Ctrl+Z)" : String ("Undo  (Ctrl+Z)"), proc.undo.canUndo());
                m.addItem (102, proc.undo.canRedo() ? "Redo " + proc.undo.getRedoDescription() + "  (Ctrl+Y)" : String ("Redo  (Ctrl+Y)"), proc.undo.canRedo());
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
                m.addItem (620, "Pattern editor  (Ctrl+E)", true, proc.uiEditOpen); m.addSeparator();
                m.addItem (601, "Wider  (Ctrl+wheel)"); m.addItem (602, "Narrower"); m.addItem (603, "Taller lanes  (Alt+wheel)"); m.addItem (604, "Lower lanes");
                m.addItem (605, "Fit the pattern  (Ctrl+Shift+F)");
                m.addItem (606, "Bigger velocity area"); m.addItem (607, "Smaller velocity area"); m.addItem (608, "Bigger song lane"); m.addItem (609, "Smaller song lane");
                m.addItem (610, "Show only used lanes", true, st.usedOnly);
                m.addSeparator(); m.addItem (614, "Mixer", true, openPage == &mixerPage);
                m.addItem (615, "Tool strip (Option / Alt)", true, stripOn); m.addItem (616, detached != nullptr ? "Bring the editor back" : "Editor in its own window (full screen)"); m.addItem (611, "Exchange keys"); m.addItem (612, "Chains page"); m.addItem (613, "Recording options");
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
            case 1: askSaveThen ([this] { auto s2 = Song::makeDefault(); s2.settings = st.song().settings; s2.playMap = st.song().playMap; proc.commit (s2, "New");
                                          proc.uiProjectPath = {}; savedSerial = proc.editSerial.load(); refreshAll(); showToast ("New: everything cleared (Ctrl+Z brings it back)"); }); break;
            case 2: togglePage (presetPage, presetName); break;
            case 12: openProject(); break;
            case 13: if (File (proc.uiProjectPath).existsAsFile()) saveProject (File (proc.uiProjectPath)); else saveProjectAs(); break;
            case 14: saveProjectAs(); break;
            case 3: quickSave(); break;
            case 4: importDialog(); break;
            case 5: exportPattern (st.song().editKey); break;
            case 6: {
                chooser = std::make_unique<FileChooser> ("Export song", File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Drum Player Song.mid"), "*.mid");
                chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                    [this] (const FileChooser& fc) { auto f = fc.getResult(); if (f != File()) { writeSongMidi (st.song(), f.withFileExtension ("mid"), st.song().settings.outChannel); showToast ("Exported " + f.getFileName()); } });
                break;
            }
            case 7: openSettings (1); break;
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
            case 304: openSettings (1); break;
            case 501: { const bool on = ! st.song().songMode; st.applySong ("Song mode", [on] (Song& s2) { s2.songMode = on; }); break; }
            case 502: if (! st.song().songMode) st.applySong ("Song mode", [] (Song& s2) { s2.songMode = true; }); proc.uiSongPlay (true, songLane.cursorBeat); break;
            case 503: proc.uiSongPlay (false, 0); break;
            case 504: songLane.duplicate(); break;
            case 505: songLane.splitAtCursor(); break;
            case 506: songLane.muteSelected(); break;
            case 507: songLane.deleteSelected(); break;
            case 508: songLane.makeUnique(); break;
            case 509: songLane.selectAll(); break;
            case 620: setEditorOpen (! proc.uiEditOpen); break;
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
            case 613: recPage.showMenu (&recBtn); break;
            case 614: togglePage (mixerPage, mixerBtn); break;
            case 15: togglePage (exportPage, exportDummy); break;
            case 615: toggleStrip(); break;
            case 616: MessageManager::callAsync ([sp = Component::SafePointer<DrumPlayerEditor> (this)] { if (sp) sp->toggleDetach(); }); break;
            default:
                if (id >= 400 && id < 420) { const int b = id - 400; st.apply ("Pattern length", [b] (Pattern& q) { q.bars = b; }); }
                else if (id >= 420 && id < 424) { const int t2 = id - 420; st.apply ("Pattern type", [t2] (Pattern& q) { q.type = (PatternType) t2; }); }
                else if (id >= 700 && id < 707) setTool ((Tool) (id - 700));
                break;
        }
        refreshControls();
    }

    // ------------------------------------------------------------ projects: everything (patterns, song lane, chains, settings, sound) in one file
    File projectsFolder() const { auto d = dataFolder().getChildFile ("Projects"); d.createDirectory(); return d; }
    void saveProject (const File& f)
    {
        if (auto x = proc.stateXml(); x != nullptr && x->writeTo (f)) { proc.uiProjectPath = f.getFullPathName(); savedSerial = proc.editSerial.load(); showToast ("Saved project " + f.getFileNameWithoutExtension()); }
        else showToast ("Couldn't save there");
    }
    void saveProjectAs()
    {
        chooser = std::make_unique<FileChooser> ("Save project", File (proc.uiProjectPath).existsAsFile() ? File (proc.uiProjectPath) : projectsFolder().getChildFile ("My song.dpproject"), "*.dpproject");
        chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
            [this] (const FileChooser& fc) { auto f = fc.getResult(); if (f != File()) saveProject (f.withFileExtension ("dpproject")); });
    }
    void openProject() { askSaveThen ([this] { openProjectNow(); }); }
    void openProjectNow()
    {
        chooser = std::make_unique<FileChooser> ("Open project", projectsFolder(), "*.dpproject");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc) {
            auto f = fc.getResult();
            if (f == File()) return;
            auto x = XmlDocument::parse (f);
            if (x == nullptr || ! x->hasTagName ("DrumPlayerState")) { showToast ("That isn't a Drum Player project"); return; }
            proc.applyState (*x);
            proc.uiProjectPath = f.getFullPathName(); savedSerial = proc.editSerial.load(); refreshAll(); showToast ("Opened " + f.getFileNameWithoutExtension());
        });
    }

    void fitToBars (int bars)
    {
        const auto& p = st.pat();
        st.pxPerBeat = jlimit (12.0f, 900.0f, (float) grid.getWidth() / jmax (1.0f, (float) (bars * p.beatsPerBar)));
        proc.uiZoomX = st.pxPerBeat / 80.0f; st.scrollX = 0; clampScroll(); repaintRoll();
    }
    // something dropped on the note editor: a key opens it, a parked pattern or a groove comes in
    void dropOnEditor (const String& d)
    {
        if (d.startsWith ("key:")) { st.song().editKey = jlimit (0, 127, d.fromFirstOccurrenceOf ("key:", false, false).getIntValue()); refreshAll(); return; }
        if (d.startsWith ("groove:")) { importFiles ({ d.fromFirstOccurrenceOf ("groove:", false, false) }, st.song().editKey); return; }
        if (d.startsWith ("park:")) {
            const int slot = jlimit (0, 7, d.fromFirstOccurrenceOf ("park:", false, false).getIntValue());
            int dest = st.song().editKey;
            if (! st.song().keys[(size_t) dest].isEmpty()) dest = nextFreeKey (dest, false, true);
            if (dest < 0) { showToast ("No free key left"); return; }
            st.applySong ("Unpark pattern", [slot, dest] (Song& x) { x.keys[(size_t) dest] = x.park[(size_t) slot]; x.ensureLanesForPattern (x.keys[(size_t) dest]); x.editKey = dest; });
            showToast ("Parked pattern on " + keyName (dest));
        }
    }
    // a parked pattern dropped on the song lane: it gets a free key and a chunk there
    void dropParkOnSong (const String& d, double at)
    {
        const int slot = jlimit (0, 7, d.fromFirstOccurrenceOf ("park:", false, false).getIntValue());
        const int dest = nextFreeKey (st.song().settings.zoneLow - 1, false, true);
        if (dest < 0) { showToast ("No free key left"); return; }
        st.applySong ("Parked pattern to the song", [slot, dest, at] (Song& x) {
            x.keys[(size_t) dest] = x.park[(size_t) slot]; x.ensureLanesForPattern (x.keys[(size_t) dest]);
            for (auto& c : x.clips) c.selected = false;
            Clip c; c.start = at; c.length = jmax (1.0, x.keys[(size_t) dest].lengthTicks() / (double) PPQ); c.key = dest; c.selected = true; x.clips.push_back (c);
        });
        showToast ("On " + keyName (dest) + " and in the song");
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

    TextButton groovesBtn;
    TextButton prevPreset, nextPreset, presetName, saveBtn, exchangeBtn, chainsBtn, mapBtn, zoomOut, zoomIn, keysBtn, playModeBtn, editBtn, setsBtn; GearButton settingsBtn; ZoomBox zoomPct;
    Label keyInfo; int lastInSerialSeen = -1;
    KeyStrip strip, bigStrip;
    ParkTray tray, bigTray;
    ChainStrip chainStrip;
    TextEditor nameEd, barsEd;
    ComboBox typeBox, bpbBox, gridBox, swingPreset, swingValue, velBox;
    Label lLen, lGrid, lSwing, lVel, exchangeHelp, status;
    TextButton lenMenuBtn, undoBtn, redoBtn, quantBtn, humBtn, funcBtn, recOptBtn, playBtn, stopBtn, velMode, velShow, chanceShow, randBtn; RightClickButton recBtn;
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
    int songH = 64, velH = 124, headW = 200;
    ComboBox fitBars; SideDivider headDiv;
    ToolStrip toolStrip; bool stripOn = false;
    std::unique_ptr<DetachWindow> detached;
    std::vector<int> partZ;
    TextButton backBtn; Label detachedNote;
    uint32 altDownAt = 0; bool altClean = false;
    void comboMenu (ComboBox& c, Component* t)
    {
        PopupMenu m;
        for (int i = 0; i < c.getNumItems(); ++i) m.addItem (c.getItemId (i), c.getItemText (i), true, c.getSelectedId() == c.getItemId (i));
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (t), [sp = Component::SafePointer<ComboBox> (&c)] (int r) {
            if (sp == nullptr || r <= 0) return;
            if (r == sp->getSelectedId()) { if (sp->onChange) sp->onChange(); } else sp->setSelectedId (r);
        });
    }
    void buttonsMenu (std::vector<Button*> bs, Component* t)
    {
        PopupMenu m;
        for (int i = 0; i < (int) bs.size(); ++i) m.addItem (i + 1, bs[(size_t) i]->getButtonText() + (bs[(size_t) i]->getTooltip().isNotEmpty() ? "  -  " + bs[(size_t) i]->getTooltip().upToFirstOccurrenceOf ("(", false, false).trim() : String()),
                                                          true, bs[(size_t) i]->getToggleState());
        std::vector<Component::SafePointer<Button>> sp; for (auto* b : bs) sp.push_back (b);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (t), [sp] (int r) { if (r > 0 && sp[(size_t) r - 1] != nullptr) sp[(size_t) r - 1]->triggerClick(); });
    }
    Component* menuAnchor = nullptr;         // the shelf button a menu opens under
    ToolStrip shelf;                         // the buttons next to the pattern's length: double-click to choose, Alt+Shift drag to arrange
    void setupShelf()
    {
        auto fn = [this] (int id) { return [this, id] (Component* t) { menuAnchor = t; runFunction (id); }; };
        shelf.all = {
            { "humanize", "Humanize & Feel", "Humanize, feel and vintage timing", [this] (Component*) { togglePage (humPage, humBtn); } },
            { "functions", "Functions", "Every editing function", [this] (Component* t) { menuAnchor = t; functionsMenu(); } },
            { "quantnow", "Quantize now", "Quantize with the settings on the Quantize page (Q)", fn (10) },
            { "ghosts", "Ghost notes", "Add ghost notes to the drums you pick", fn (34) },
            { "curve", "Velocity curve", "Sine, triangle and ramps over the velocities", [this] (Component*) { openCurve(); } },
            { "humnotes", "Humanize notes", "Humanize timing and velocity into the notes", fn (11) },
            { "randvel", "Randomize velocity", "Small random changes to the velocities", fn (30) },
            { "randtime", "Randomize timing", "Small random timing changes", fn (31) },
            { "thin", "Thin out", "Remove a quarter of the notes at random", fn (33) },
            { "reverse", "Reverse", "Play the notes backwards", fn (12) },
            { "double", "Double speed", "Twice as fast", fn (13) },
            { "half", "Half speed", "Half as fast", fn (14) },
            { "accent", "Accents", "Accent the beats", fn (22) },
            { "louder", "Louder", "Velocities +10", fn (23) },
            { "softer", "Softer", "Velocities -10", fn (24) },
            { "doubles", "Remove doubles", "Remove notes that sit on top of each other", fn (18) },
            { "split", "Split", "Split the pattern at the edit cursor", fn (43) },
            { "export", "Export MIDI", "Save this pattern as a MIDI file", fn (44) },
            { "import", "Import MIDI", "Bring MIDI files onto keys", fn (50) },
        };
        shelf.hint = String::fromUTF8 ("double-click: add buttons here  \xc2\xb7  Alt+Shift drag: arrange");
        shelf.compact = true;
        shelf.on = StringArray::fromTokens (proc.uiShelfTools, ",", ""); shelf.on.removeEmptyStrings();
        shelf.onChanged = [this] { proc.uiShelfTools = shelf.on.joinIntoString (","); };
        body.addAndMakeVisible (shelf);
        shelf.rebuild();
    }
    void setupToolStrip()
    {
        setupShelf();
        auto click = [] (Button& b) { auto* p = &b; return [p] (Component*) { p->triggerClick(); }; };
        toolStrip.all = {
            { "fit", "Bars", "Show this many bars across the editor", [this] (Component* t) { comboMenu (fitBars, t); } },
            { "grid", "Grid", "Snap grid", [this] (Component* t) { comboMenu (gridBox, t); } },
            { "tools", "Tools", "Arrow, draw, erase, scissors, paint, mute, line", [this] (Component* t) { std::vector<Button*> v; for (auto* b : toolBtns) v.push_back (b); buttonsMenu (v, t); } },
            { "play", "Play", "Play the pattern", click (playBtn) },
            { "stop", "Stop", "Stop", click (stopBtn) },
            { "rec", "Rec", "Record", click (recBtn) },
            { "quantize", "Quantize", "Quantize page", click (quantBtn) },
            { "humanize", "Humanize", "Humanize page", click (humBtn) },
            { "functions", "Functions", "Functions menu", click (funcBtn) },
            { "mixer", "Mixer", "Pan and meters", click (mixerBtn) },
            { "zoom", "Zoom", "Wider, narrower, taller, lower, fit", [this] (Component* t) { buttonsMenu ({ &zH2, &zH1, &zV2, &zV1, &zFit }, t); } },
            { "vel", "Velocity", "Velocity for new notes", [this] (Component* t) { comboMenu (velBox, t); } },
            { "swing", "Swing", "Swing preset", [this] (Component* t) { comboMenu (swingPreset, t); } },
            { "detach", "Window", "Detach the editor into its own window, or bring it back", [sp = Component::SafePointer<DrumPlayerEditor> (this)] (Component*) { MessageManager::callAsync ([sp] { if (sp) sp->toggleDetach(); }); } },
        };
        toolStrip.on = StringArray::fromTokens (proc.uiStripTools, ",", "");
        toolStrip.on.removeEmptyStrings();
        toolStrip.onChanged = [this] { proc.uiStripTools = toolStrip.on.joinIntoString (","); };
        toolStrip.rebuild();
    }
    void toggleStrip() { stripOn = ! stripOn; if (detached != nullptr) detached->content.inner.resized(); else layout(); }
    void toggleDetach() { if (detached != nullptr) reattach(); else detach(); }
    void detach()
    {
        if (detached != nullptr) return;
        closePages();
        detached = std::make_unique<DetachWindow>();
        auto& in = detached->content.inner;
        auto parts = rollParts();
        partZ.clear();
        for (auto* c : parts) partZ.push_back (body.getIndexOfChildComponent (c));
        for (auto* c : parts) in.addChildComponent (c);
        in.onPaint = [this] (Graphics& g) { paintRollCards (g); };
        in.onResized = [this] { if (detached != nullptr) layoutRoll (detached->content.inner.getLocalBounds().reduced (10, 6)); };
        detached->content.scale = scale;
        detached->content.onKey = [this] (const KeyPress& k) { return keyPressed (k); };
        detached->content.onMods = [this] (const ModifierKeys& m) { modifierKeysChanged (m); };
        detached->onClose = [sp = Component::SafePointer<DrumPlayerEditor> (this)] { MessageManager::callAsync ([sp] { if (sp) sp->reattach(); }); };
        const auto* d = Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds());
        detached->setBounds (d != nullptr ? d->userArea : Rectangle<int> (0, 0, 1400, 800));
        detached->setVisible (true);
        detached->toFront (true);
        layout();
        detached->content.inner.resized();
        detached->content.grabKeyboardFocus();
    }
    void reattach()
    {
        if (detached == nullptr) return;
        auto parts = rollParts();
        std::vector<std::pair<int, Component*>> order;
        for (size_t i = 0; i < parts.size(); ++i) order.push_back ({ i < partZ.size() ? partZ[i] : -1, parts[i] });
        std::sort (order.begin(), order.end(), [] (auto& a, auto& b) { return a.first < b.first; });
        for (auto& o : order) body.addChildComponent (o.second, o.first);
        detached.reset();
        layout();
    }
    void modifierKeysChanged (const ModifierKeys& m) override
    {
        // a lone Option / Alt press (no key, no click in between) shows or hides the tool strip
        const auto now = Time::getMillisecondCounter();
        if (m.isAltDown() && ! m.isCommandDown() && ! m.isCtrlDown() && ! m.isShiftDown()) {
            if (altDownAt == 0) { altDownAt = now; altClean = true; }
        } else if (altDownAt != 0) {
            const bool clicked = Desktop::getInstance().getMainMouseSource().getLastMouseDownTime().toMilliseconds() > Time::currentTimeMillis() - (int64) (now - altDownAt);
            if (altClean && ! clicked && ! m.isAnyModifierKeyDown() && now - altDownAt < 600) toggleStrip();
            altDownAt = 0; altClean = false;
        }
    }
    ScrollBar hScroll { false }, vScroll { true }, songScroll { false };
    void syncSongScroll()
    {
        double endB = 16; for (auto& c : st.song().clips) endB = jmax (endB, c.start + c.length);
        const double total = (endB + 32) * songLane.pxPerBeat, vis = jmax (1, songLane.getWidth());
        songScroll.setRangeLimits (0, jmax (total, vis + songLane.scrollX), dontSendNotification);
        songScroll.setCurrentRange (songLane.scrollX, vis, dontSendNotification);
    }
    struct ScrollRelay : public ScrollBar::Listener
    {
        DrumPlayerEditor* owner = nullptr;
        void scrollBarMoved (ScrollBar* sb, double v) override
        {
            if (owner == nullptr) return;
            if (sb == &owner->songScroll) { owner->songLane.scrollX = (int) v; owner->songLane.repaint(); return; }
            if (sb == &owner->hScroll) owner->st.scrollX = (int) v; else owner->st.scrollY = (int) v;
            owner->repaintRoll();
        }
    } scrollRelay;
    QuantizePage quantPage; HumanizePage humPage; ChainPage chainPage; SettingsPage setPage; PresetPage presetPage; MapPage mapPage; RecordPage recPage; GroovePage groovePage; MixerPage mixerPage; ExportPage exportPage; GrooveSetsPage setsPage;
    TextButton mixerBtn, exportDummy;
    // tempo: your own when Studio One is stopped, or follow Studio One
    TextButton tempoDown, tempoUp, tapBtn, followBtn; TempoLabel tempoLbl; Label lTempo;
    std::vector<double> taps;
    double ownTempo() { return st.song().settings.ownBpm; }
    void setOwnTempo (double b)
    {
        b = jlimit (20.0, 400.0, std::round (b * 10.0) / 10.0);
        st.applySong ("Tempo", [b] (Song& s) { s.settings.ownBpm = b; s.settings.followHost = false; });
        showTempo();
    }
    double effectiveTempo() { return st.song().settings.followHost || proc.hostPlaying.load() ? proc.hostBpm.load() : ownTempo(); }
    void showTempo()
    {
        if (tempoLbl.isBeingEdited()) return;
        const bool follow = st.song().settings.followHost;
        const double b = effectiveTempo();
        const auto t = String (b, std::abs (b - std::round (b)) < 0.05 ? 0 : 1);
        if (tempoLbl.getText() != t) tempoLbl.setText (t, dontSendNotification);
        tempoLbl.setColour (Label::textColourId, follow || proc.hostPlaying.load() ? th::dim : th::text);
        followBtn.setToggleState (follow, dontSendNotification);
    }
    bool anythingPlaying()
    {
        return proc.playingKey.load() >= 0 || proc.queuedKey.load() >= 0 || proc.chainPlaying.load() >= 0 || proc.songBeatUi.load() >= 0;
    }
    // ---- the transport: Play plays what you clicked last (0 song lane, 1 a pattern, 2 the chain strip)
    struct TargetWatch : public MouseListener { std::function<void (Component*)> f; void mouseDown (const MouseEvent& e) override { if (f) f (e.eventComponent); } } targetWatch;
    int playTarget = 1; Label playsWhat;
    void showPlays()
    {
        static const char* t[] = { "Play: song", "Play: pattern", "Play: chain" };
        playsWhat.setText (t[jlimit (0, 2, playTarget)], dontSendNotification);
        songLane.repaint(); chainStrip.repaint();
    }
    void transportPlay()
    {
        if (playTarget == 2) { proc.uiPlayChain (st.currentChain); return; }
        if (playTarget == 0) { const auto& sg = st.song(); proc.uiSongPlay (true, sg.loopOn && (songLane.cursorBeat < sg.loopStart || songLane.cursorBeat >= sg.loopEnd) ? sg.loopStart : songLane.cursorBeat); return; }
        proc.uiTrigger (st.song().editKey);
    }
    // ---- unsaved changes: ask before they'd be lost
    int savedSerial = 0;
    bool unsaved() const { return proc.editSerial.load() != savedSerial; }
    void askSaveThen (std::function<void()> next)
    {
        if (! unsaved()) { next(); return; }
        auto* aw = new AlertWindow ("Save changes?", "You have changes that aren't saved in a Drum Player project. Save them first?", MessageBoxIconType::QuestionIcon, this);
        aw->addButton ("Save", 1); aw->addButton ("Don't save", 2); aw->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        aw->enterModalState (true, ModalCallbackFunction::create ([this, next] (int r) {
            if (r == 0) return;
            if (r == 1) { if (File (proc.uiProjectPath).existsAsFile()) saveProject (File (proc.uiProjectPath)); else { saveProjectAs(); return; } }
            next();
        }), true);
    }
    void stopAll()
    {
        if (proc.songBeatUi.load() >= 0) proc.uiSongPlay (false, 0);
        proc.uiStop(); stopFlashUntil = Time::getMillisecondCounter() + 450;
    }
    Component exchange;
    Page* openPage = nullptr; TextButton* openPageBtn = nullptr;
    TextButton pageClose;                    // the X in the corner of an open page
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
        menuBar.setBounds (a.removeFromTop (jmax (22, (int) std::ceil (Theme::get().fontFor ("txt:Menu bar", "Menu bar", 14.0f) + 9)))); a.removeFromTop (4);
        // header
        auto h = a.removeFromTop (jmax (30, (int) btnH + 8));
        headerArea = h;
        headerTitle = h.removeFromLeft (170);
        RowPlacer hp (h, 5);
        hp.place (prevPreset, 24); hp.place (presetName, 200); hp.place (nextPreset, 24); hp.skip (16);
        hp.place (keysBtn); hp.place (playModeBtn); hp.skip (10);
        const int rightW = mixerBtn.getWidth() + groovesBtn.getWidth() + setsBtn.getWidth() + exchangeBtn.getWidth() + chainsBtn.getWidth() + settingsBtn.getWidth() + 52 + 8 * 6 + 20;
        hp.place (keyInfo, jmax (0, h.getRight() - rightW - hp.x - 10));
        RowPlacer hr (h.withTrimmedLeft (h.getWidth() - rightW), 5);
        hr.place (mixerBtn); hr.place (groovesBtn); hr.place (setsBtn); hr.place (exchangeBtn); hr.place (chainsBtn); hr.skip (8);
        hr.place (zoomPct, 52); hr.skip (6); hr.place (settingsBtn);
        zoomOut.setVisible (false); zoomIn.setVisible (false);
        for (auto* b : { &saveBtn, &mapBtn, &undoBtn, &redoBtn, &recOptBtn, (TextButton*) &dragBtn }) b->setVisible (false);
        lVel.setVisible (false);
        a.removeFromTop (6);
        // keyboard + tray
        auto ks = a.removeFromTop ((int) jlimit (56.0f, 170.0f, 72.0f * std::sqrt (Theme::get().keyScale)));
        tray.setBounds (ks.removeFromRight (250));
        ks.removeFromRight (8);
        strip.setBounds (ks);
        a.removeFromTop (5);
        belowKeys = a;
        // bottom: status line, then the transport and tempo bar (always there, under the velocities)
        status.setBounds (a.removeFromBottom (18));
        a.removeFromBottom (2);
        {   auto tb = a.removeFromBottom (jmax (36, (int) btnH + 14));
            transportArea = tb;
            tb.reduce (8, 0);
            RowPlacer tp (tb, 6);
            editBtn.setVisible (false);
            tp.place (recBtn); tp.place (playBtn); tp.place (stopBtn); tp.skip (6); tp.place (playsWhat, 110);
            // tempo, big, at the right end: label, - number + right next to each other, tap, lock
            const int tH = jmax ((int) btnH + 6, 30);
            auto tr = tb;
            followBtn.setBounds (tr.removeFromRight (followBtn.getWidth()).withSizeKeepingCentre (followBtn.getWidth(), btnH)); tr.removeFromRight (6);
            tapBtn.setBounds (tr.removeFromRight (tapBtn.getWidth()).withSizeKeepingCentre (tapBtn.getWidth(), btnH)); tr.removeFromRight (10);
            tempoUp.setBounds (tr.removeFromRight (24).withSizeKeepingCentre (24, tH)); tr.removeFromRight (1);
            tempoLbl.setBounds (tr.removeFromRight (76).withSizeKeepingCentre (76, tH)); tr.removeFromRight (1);
            tempoDown.setBounds (tr.removeFromRight (24).withSizeKeepingCentre (24, tH)); tr.removeFromRight (6);
            lTempo.setBounds (tr.removeFromRight (56).withSizeKeepingCentre (56, tH));
            a.removeFromBottom (6); }
        const bool editing = proc.uiEditOpen;
        // the pattern player (song lane) right under the keys, with its own scroll bar; it takes the room when the editor is closed
        {   const int chainH = jmax (32, (int) btnH + 10);
            songH = jlimit (40, jmax (40, a.getHeight() - 260), songH);
            const int h2 = editing ? songH : jmax (60, a.getHeight() - chainH - 12);
            auto songRow = a.removeFromTop (h2);
            songHeader.setBounds (songRow.removeFromLeft (200));
            songRow.removeFromRight (10);
            songScroll.setBounds (songRow.removeFromBottom (10));
            songLane.setBounds (songRow);
            songDiv.setVisible (editing);
            if (editing) songDiv.setBounds (a.removeFromTop (10)); else a.removeFromTop (8);
            chainStrip.setBounds (a.removeFromTop (chainH)); }
        a.removeFromTop (7);
        std::vector<Component*> editParts { &nameEd, &typeBox, &lLen, &barsEd, &lenMenuBtn, &bpbBox, &lGrid, &gridBox, &lSwing, &swingPreset, &swingSlider, &swingValue,
                                           &quantBtn, &velBox, &shelf };
        for (auto* c : rollParts()) editParts.push_back (c);
        {   const auto rp = rollParts();
            for (auto* c : editParts) {
                const bool isRoll = std::find (rp.begin(), rp.end(), c) != rp.end();
                if (isRoll && (detached != nullptr || c == &toolStrip)) continue;      // the detached window keeps its own
                c->setVisible (editing);
            } }
        toolStrip.setVisible (editing && stripOn);
        if (! editing) {
            toolArea = {}; rollArea = velLabelArea = {}; bodyRest = {};
            backBtn.setVisible (false); detachedNote.setVisible (false);
        } else {
            // toolbar 1: the pattern itself, and what you do to it
            auto t1 = a.removeFromTop (jmax (28, (int) btnH + 6));
            toolArea = t1;
            RowPlacer r1 (t1, 5);
            r1.place (nameEd, 100); r1.place (typeBox, 84); r1.skip (6);
            r1.place (lLen, 44); r1.place (barsEd, 40); r1.place (lenMenuBtn); r1.place (bpbBox, 58); r1.skip (10);
            shelf.setBounds (t1.withLeft (r1.x).withRight (t1.getRight()));          // your buttons: double-click to add, Alt+Shift drag to arrange
            humBtn.setVisible (false); funcBtn.setVisible (false);
            for (auto* b : toolBtns) b->setVisible (false);     // tools live in the right-click menu, the Tools menu and keys 1-7
            a.removeFromTop (4);
            // toolbar 2: timing - grid, quantize and swing together
            auto t2 = a.removeFromTop (jmax (28, (int) btnH + 6));
            toolArea = toolArea.getUnion (t2);
            RowPlacer r2 (t2, 5);
            r2.place (lGrid, 30); r2.place (gridBox, 70); r2.place (quantBtn); r2.skip (14);
            r2.place (lSwing, 40); r2.place (swingPreset, 104); r2.place (swingSlider, 190); r2.place (swingValue, 66);
            a.removeFromTop (8);
            bodyRest = a;
            if (detached == nullptr) { backBtn.setVisible (false); detachedNote.setVisible (false); layoutRoll (a); }
            else {
                rollArea = velLabelArea = {};
                detachedNote.setBounds (a.withSizeKeepingCentre (jmin (a.getWidth(), 520), 40).translated (0, -24)); detachedNote.setVisible (true);
                backBtn.setBounds (a.withSizeKeepingCentre (backBtn.getWidth(), btnH).translated (0, 16)); backBtn.setVisible (true);
                detached->content.inner.resized(); detached->content.inner.repaint();
            }
        }
        // pages float over the right side of everything under the keys
        const auto ra = belowKeys.withTrimmedBottom (20);
        auto pageArea = ra.withTrimmedLeft (jmax (0, ra.getWidth() - 520));
        for (Page* pg : { (Page*) &quantPage, (Page*) &humPage, (Page*) &chainPage, (Page*) &setPage, (Page*) &recPage, (Page*) &mapPage }) pg->setBounds (pageArea);
        presetPage.setBounds (ra.withTrimmedLeft (jmax (0, ra.getWidth() - 600)));
        groovePage.setBounds (ra.withHeight (jmin (ra.getHeight(), 380)));      // the grooves open right under the keys, the full width
        setsPage.setBounds (groovePage.getBounds());
        mixerPage.setBounds (ra);
        exportPage.setBounds (ra.withTrimmedLeft (jmax (0, ra.getWidth() - 760)));
        exchange.setBounds (ra);
        auto ex = exchange.getLocalBounds().reduced (12);
        exchangeHelp.setBounds (ex.removeFromTop (24));
        bigTray.setBounds (ex.removeFromBottom (120).removeFromLeft (420));
        ex.removeFromBottom (10);
        bigStrip.setBounds (ex);
        clampScroll();
        if (openPage != nullptr) placeClose (openPage->getBounds()); else if (exchange.isVisible()) placeClose (exchange.getBounds());
        body.repaint();
    }
    static int smallH() { return jmax (16, (int) btnH - 5); }
    Rectangle<int> headerTitle, rollArea, velLabelArea, headerArea, toolArea, bodyRest, belowKeys, transportArea;
    void layoutRoll (Rectangle<int> a)
    {
        // the Option-key strip, as tall as the song lane, above it
        toolStrip.setVisible (stripOn);
        if (stripOn) { toolStrip.setBounds (a.removeFromTop (jmax ((int) btnH + 12, songH))); a.removeFromTop (4); }
        // roll
        rollArea = a;
        const int rulerH = 22, sb = 10;
        velH = jlimit (4 * (smallH() + 3) + 14, jmax (4 * (smallH() + 3) + 14, a.getHeight() - 120), velH);
        auto velRow = a.removeFromBottom (velH);
        velDiv.setBounds (a.removeFromBottom (10));
        auto top = a.removeFromTop (rulerH);
        {   auto z = top.withWidth (jmax (headW, 200)).reduced (4, 0);
            for (auto* b : { &zH1, &zH2, &zV1, &zV2, &zFit }) { b->setVisible (headW >= 200); b->setBounds (z.removeFromLeft (b->getWidth()).withSizeKeepingCentre (b->getWidth(), 18)); z.removeFromLeft (3); } }
        // the fit-bars menu sits in the ruler's corner, always reachable
        fitBars.setBounds (top.getRight() - 96 - sb, top.getY() + 1, 92, rulerH - 2); fitBars.toFront (false);
        vScroll.setBounds (a.removeFromRight (sb)); top.removeFromRight (sb);
        hScroll.setBounds (a.removeFromBottom (sb).withTrimmedLeft (headW));
        headers.setBounds (a.removeFromLeft (headW)); top.removeFromLeft (headW);
        headDiv.setBounds (headers.getRight() - 3, headers.getY(), 6, headers.getHeight()); headDiv.toFront (false);
        grid.setBounds (a); ruler.setBounds (top);
        {   // the velocity area's buttons, stacked in the space under the drum list
            // small and plain: what the lane shows, then what you can do to it
            const int bx = velRow.getX() + 8, bw = jmax (40, headW - 16), gap = 3, h = smallH();
            int by = velRow.getY() + 6;
            const int half = (bw - gap) / 2;
            velShow.setBounds (bx, by, half, h); chanceShow.setBounds (bx + half + gap, by, half, h); by += h + gap;
            randBtn.setBounds (bx, by, bw, h); by += h + gap;
            velMode.setBounds (bx, by, bw, h); by += h + gap;
            guideBox.setBounds (bx, by, half, h); velBox.setBounds (bx + half + gap, by, half, h);
        }
        velLabelArea = velRow.removeFromLeft (headW);
        velRow.removeFromRight (sb);
        vel.setBounds (velRow);
        clampScroll();
    }
    // the parts that make up the editor area (they move to their own window when detached)
    std::vector<Component*> rollParts()
    {
        return { &toolStrip, &ruler, &headers, &headDiv, &grid, &vel, &velDiv, &hScroll, &vScroll,
                 &zH1, &zH2, &zV1, &zV2, &zFit, &fitBars, &velShow, &chanceShow, &randBtn, &velMode, &guideBox };
    }
    void paintRollCards (Graphics& g)
    {
        drawCard (g, rollArea.toFloat().expanded (2), th::panel, 10, "Roll card");
        drawCard (g, velLabelArea.toFloat(), th::panel, 6, "Roll card");
        if (stripOn) drawCard (g, toolStrip.getBounds().toFloat().expanded (2), th::panel, 8, "Toolbars");
        g.setFont (font (10.5f, 1)); g.setColour (th::faint);
        const auto focus = st.focusLane >= 0 && st.song().lane (st.focusLane) ? st.song().lane (st.focusLane)->name : String ("all lanes");
        g.drawFittedText ("Showing: " + focus, velLabelArea.reduced (10, 6).withTrimmedTop (4 * (smallH() + 3) + 4), Justification::topLeft, 2);
    }

    void paintBody (Graphics& g)
    {
        // title: two-tone wordmark like the practice app
        g.setFont (font (Theme::get().fontFor ("txt:Title", "", 17.0f), 3)); g.setColour (th::text);
        g.drawText ("DRUM", headerTitle.withWidth (60), Justification::centredLeft);
        g.setColour (th::amber);
        g.drawText ("PLAYER", headerTitle.withTrimmedLeft (58), Justification::centredLeft);
        // cards behind the roll and the keyboard
        if (themed ("Header")) drawCard (g, headerArea.toFloat().expanded (4, 3), th::panel, 8, "Header");
        if (themed ("Toolbars") && ! toolArea.isEmpty()) drawCard (g, toolArea.toFloat().expanded (4, 3), th::panel, 8, "Toolbars");
        drawCard (g, transportArea.toFloat(), th::panel, 8, "Toolbars");
        if (detached == nullptr && proc.uiEditOpen) paintRollCards (g);
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
        randBtn.setButtonText (st.probMode ? "Randomize chance" : "Randomize velocity");
        editBtn.setToggleState (proc.uiEditOpen, dontSendNotification);

        if (! zoomPct.isBeingEdited()) zoomPct.setText (String ((int) std::round (scale * 100)) + "%", dontSendNotification);
        keysBtn.setToggleState (st.song().settings.keyMode == 0, dontSendNotification);
        playModeBtn.setToggleState (st.song().settings.keyMode == 1, dontSendNotification);
        strip.ensureVisible (st.song().editKey); bigStrip.ensureVisible (st.song().editKey);
        syncSwingPreset();
        status.setText ("Tool: " + String (toolName (st.tool)) + "   |   " + keyName (st.song().editKey) + "   " + (p.name.isNotEmpty() ? p.name : String ("(empty key)")) + "   |   "
                        + String ((int) p.notes.size()) + " notes   |   " + String (p.bars) + " x " + String (p.beatsPerBar) + "/4"
                        + "   |   pattern keys " + keyName (st.song().settings.zoneLow) + " to " + keyName (st.song().settings.zoneHigh)
                        + "   |   playing on " + builtInMaps()[(size_t) st.song().playMap].name + ", ch " + String (st.song().settings.outChannel)
                        + "   |   notes sent: " + String ((int) proc.notesSent.load()), dontSendNotification);
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
        if (st.song().editKey != lastEditKeySeen) { const bool first = lastEditKeySeen < 0; lastEditKeySeen = st.song().editKey; if (! first) setEditorOpen (true); }
        const int pk = proc.playingKey.load();
        const double pt = pk == st.song().editKey ? proc.playRelTicks.load() : -1.0;
        if (pk != lastPlayKey) { lastPlayKey = pk; strip.repaint(); bigStrip.repaint(); }
        if (pt != lastPlayTicks) { lastPlayTicks = pt; grid.playTicks = pt; grid.repaint(); }
        if (proc.queuedKey.load() != -2) { strip.repaint(); bigStrip.repaint(); }
        showTempo();
        if (! songScroll.isMouseButtonDown()) syncSongScroll();
        if (st.activeLoop >= 0 && st.loopForKey != st.song().editKey) { st.setActiveLoop (-1, false); ruler.repaint(); grid.repaint(); }   // another key: loops start over
        {   // Rec lamp: steady while recording, blinking during the count-in
            const int rs = proc.recStateUi.load();
            const bool lit = rs != 0 || proc.songRecArmed.load();
            const int want = (rs == 2 || proc.songRecArmed.load()) ? (int) 0xffff2a2a : (int) 0xffc8463f;          // count-in red, recording redder
            if ((int) recBtn.getProperties()["lit"] != want) { recBtn.getProperties().set ("lit", want); recBtn.repaint(); }
            if (lit != recBtn.getToggleState()) recBtn.setToggleState (lit, dontSendNotification);
        }
        if (headers.pollHits()) headers.repaint();
        {   // transport lamps
            const bool playingNow = proc.playingKey.load() >= 0 || proc.songBeatUi.load() >= 0;
            if (playingNow != playBtn.getToggleState()) playBtn.setToggleState (playingNow, dontSendNotification);
            const bool stopLit = Time::getMillisecondCounter() < stopFlashUntil;
            if (stopLit != stopBtn.getToggleState()) stopBtn.setToggleState (stopLit, dontSendNotification);
            bool looping = false; for (auto& c : st.song().chains) for (auto& s2 : c.steps) looping |= s2.looping;
            if (looping) chainStrip.repaint();
        }
        if (const uint32 ns = proc.notesSent.load(); ns != lastNotesSent && (Time::getMillisecondCounter() - lastStatusMs) > 250) { lastNotesSent = ns; lastStatusMs = Time::getMillisecondCounter(); refreshControls(); }
        if (const double sb = proc.songBeatUi.load(); sb >= 0 || lastSongBeat >= 0) { lastSongBeat = sb; songLane.repaint(); }
        if (const int ser = proc.lastInSerial.load(); ser != lastInSerialSeen) {     // which key was pressed, and what it did
            lastInSerialSeen = ser;
            const int n = proc.lastInNote.load(), k = proc.lastInKey.load();
            if (n >= 0) { strip.flash (n); bigStrip.flash (n); }
            if (learnFrom >= 0 && ser != learnSerial && n >= 0) {          // "move to the next key I press"
                const int from = learnFrom, to = n; learnFrom = -1;
                if (to == from) showToast ("That's the same key");
                else if (to < st.song().settings.zoneLow || st.song().settings.isStop (to)) showToast (keyName (to) + " isn't a pattern key");
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
            const int how = proc.lastTrigger.load();
            String t = "Key " + keyName (n) + " (" + String (n) + ")";
            if (how == 4) t << "  ->  STOP";
            else if (how == 5) t << "  ->  starts a chain";
            else if (k >= 0) {
                t << "  ->  " + keyName (k) + (st.song().keys[(size_t) k].notes.empty() ? String (": empty key") : "  " + st.song().keys[(size_t) k].name);
                if (how == 1 && ! st.song().keys[(size_t) k].notes.empty()) t << "  (starts)";
                if (how == 3) t << "  (stops)";
            }
            else t << "  ->  plays drum: " << st.song().laneName (n);
            if (n >= 0) keyInfo.setText (t, dontSendNotification);
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
    uint32 stopFlashUntil = 0;
    uint32 lastNotesSent = 0, lastStatusMs = 0;
    int learnFrom = -1, learnSerial = 0;

    // after a text size change, buttons grow or shrink to fit their words
    static void refitButtons (Component& c)
    {
        for (auto* ch : c.getChildren()) {
            if (auto* b = dynamic_cast<TextButton*> (ch); b != nullptr && b->getComponentID().isNotEmpty())
                b->setSize (b->getWidth() > 30 ? fitW (b->getButtonText()) : b->getWidth(), btnH);
            refitButtons (*ch);
        }
    }
    static void relayoutAll (Component& c) { for (auto* ch : c.getChildren()) { ch->resized(); relayoutAll (*ch); } }
    static void repaintEverything (Component& c) { c.repaint(); for (auto* ch : c.getChildren()) repaintEverything (*ch); }
    void showToast (const String& t) { toast.text = t; toast.until = Time::getMillisecondCounter() + 2600; toast.toFront (false); toast.repaint(); }

    // ------------------------------------------------------------ pages
    void togglePage (Page& pg, TextButton& btn)
    {
        if (exchange.isVisible()) { exchange.setVisible (false); exchangeBtn.setToggleState (false, dontSendNotification); }
        const bool open = openPage != &pg;
        closePages();
        if (open) {
            openPage = &pg; openPageBtn = &btn; pg.refresh(); pg.setVisible (true); pg.toFront (false); btn.setToggleState (true, dontSendNotification);
            placeClose (pg.getBounds());
        }
    }
    void closePages()
    {
        if (openPage != nullptr) openPage->setVisible (false);
        if (openPageBtn != nullptr) openPageBtn->setToggleState (false, dontSendNotification);
        openPage = nullptr; openPageBtn = nullptr;
        exchange.setVisible (false); exchangeBtn.setToggleState (false, dontSendNotification);
        pageClose.setVisible (false);
    }
    void placeClose (Rectangle<int> r)
    {
        pageClose.setBounds (r.getRight() - 34, r.getY() + 8, 24, 22);
        pageClose.setVisible (true); pageClose.toFront (false);
    }
    void toggleExchange()
    {
        const bool show = ! exchange.isVisible();
        closePages();
        if (show) { exchange.setVisible (true); exchange.toFront (false); exchangeBtn.setToggleState (true, dontSendNotification); placeClose (exchange.getBounds()); }
    }

    void setKeyMode (int m)
    {
        st.applySong (m == 0 ? "Pattern mode" : "Drum kit mode", [m] (Song& s) { s.settings.keyMode = m; });
        showToast (m == 0 ? "Pattern mode: keys from C-1 up play patterns, empty keys play drums" : "Drum kit mode: every key plays a drum");
        refreshControls();
    }
    // the pattern editor opens by itself when you pick a pattern; Esc (with nothing selected) or Ctrl+E closes it for the big song view
    void setEditorOpen (bool on)
    {
        if (proc.uiEditOpen == on) return;
        proc.uiEditOpen = on; layout(); refreshAll();
    }
    int lastEditKeySeen = -1;
    // listen to a groove file (on the hidden preview key)
    void previewFile (const File& f)
    {
        auto pieces = splitToPatterns (readMidiFile (f), 0);
        if (pieces.empty()) return;
        auto p = pieces.front();
        const int from = st.song().importMap;
        if (from >= 0 && from != st.song().playMap) convertPattern (p, st.song().map (from), st.song().map (st.song().playMap));
        st.song().keys[previewKey] = p; st.song().ensureLanesForPattern (p);
        proc.publish(); proc.uiStop(); proc.uiTrigger (previewKey);
    }
    // a whole set onto the keys: grooves on white keys, fills on black keys, each part folder a colour group
    void loadGrooveSet (const File& dir)
    {
        if (! dir.isDirectory()) return;
        struct Part { String name; Array<File> files; };
        std::vector<Part> parts;
        auto byName = [] (const File& a, const File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; };
        Array<File> own = dir.findChildFiles (File::findFiles, false, "*.mid;*.midi"); std::sort (own.begin(), own.end(), byName);
        if (! own.isEmpty()) parts.push_back ({ {}, own });
        Array<File> subs = dir.findChildFiles (File::findDirectories, false); std::sort (subs.begin(), subs.end(), byName);
        for (auto& d : subs) {
            Array<File> fs = d.findChildFiles (File::findFiles, true, "*.mid;*.midi"); std::sort (fs.begin(), fs.end(), byName);
            if (! fs.isEmpty()) parts.push_back ({ d.getFileName(), fs });
        }
        if (parts.empty()) { showToast ("No MIDI grooves in " + dir.getFileName()); return; }
        const int from = st.song().importMap, to = st.song().playMap;
        int placed = 0;
        st.applySong ("Load groove set", [&] (Song& s) {
            for (int k = s.settings.zoneLow; k < 128; ++k) if (! s.settings.isStop (k)) s.keys[(size_t) k] = Pattern();
            int cursor = s.settings.zoneLow;
            for (auto& part : parts) {
                int group = -1;
                if (part.name.isNotEmpty()) {
                    for (int i = 0; i < (int) s.groups.size(); ++i) if (s.groups[(size_t) i].name.equalsIgnoreCase (part.name)) group = i;
                    if (group < 0) { static const uint32 cols[] = { 0xff5aa9ff, 0xffb8f28b, 0xff6ee7d8, 0xffffc15e, 0xffd3a6ff, 0xffff8a65, 0xfff06292 };
                                     s.groups.push_back ({ part.name, cols[s.groups.size() % 7] }); group = (int) s.groups.size() - 1; }
                }
                int white = cursor, black = cursor, last = cursor - 1;
                for (auto& f : part.files) {
                    const bool fill = f.getFileName().containsIgnoreCase ("fill");
                    int& k = fill ? black : white;
                    while (k < 128 && (MidiMessage::isMidiNoteBlack (k) != fill || s.settings.isStop (k))) ++k;
                    if (k >= 128) break;
                    auto pieces = splitToPatterns (readMidiFile (f), 0);
                    if (pieces.empty()) continue;
                    auto p = pieces.front();
                    if (from >= 0 && from != to) convertPattern (p, s.map (from), s.map (to));
                    p.name = f.getFileNameWithoutExtension(); p.source = f.getFullPathName(); p.group = group;
                    if (fill) p.type = PatternType::Fill;
                    s.keys[(size_t) k] = p; s.ensureLanesForPattern (p);
                    last = jmax (last, k); ++k; ++placed;
                }
                cursor = jmin (127, last + 1);
                while (cursor < 127 && MidiMessage::isMidiNoteBlack (cursor)) ++cursor;      // the next part starts on a white key
            }
            s.editKey = s.settings.zoneLow;
        });
        refreshAll();
        showToast ("Loaded the set " + dir.getFileName() + ": " + String (placed) + " grooves on the keys  (Ctrl+Z undoes it)");
    }
    void openSettings (int tab)
    {
        if (openPage != &setPage) togglePage (setPage, settingsBtn);
        setPage.showTab (tab);
    }
    void setTool (Tool t) { st.tool = t; st.stamping = false; refreshControls(); for (int i = 0; i < toolBtns.size(); ++i) toolBtns[i]->setToggleState (i == (int) t, dontSendNotification); grid.repaint(); }

    void setScale (float s)
    {
        const float old = scale;
        scale = jlimit (0.6f, 2.0f, std::round (s * 10.0f) / 10.0f);
        proc.uiScale = scale; DPLnF::menuScale = scale;
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
    // MIDI files on the clipboard (copied in Explorer or another program), or paths copied as text
    StringArray clipboardMidiFiles() const
    {
        StringArray out;
        for (auto& w : dpClipboardFiles()) { String p (w.c_str()); if (isMidiPath (p) && File (p).existsAsFile()) out.add (p); }
        if (out.isEmpty())
            for (auto& line : StringArray::fromLines (SystemClipboard::getTextFromClipboard())) {
                const auto p = line.trim().unquoted();
                if (isMidiPath (p) && File::isAbsolutePath (p) && File (p).existsAsFile()) out.add (p);
            }
        return out;
    }
    static bool isMidiPath (const String& p) { return p.endsWithIgnoreCase (".mid") || p.endsWithIgnoreCase (".midi") || p.endsWithIgnoreCase (".smf"); }
    void pasteMidiToKey (int key, bool cut)
    {
        auto files = clipboardMidiFiles();
        if (files.isEmpty()) { showToast ("No MIDI file on the clipboard. Copy a .mid file (for example in Explorer), or drag the groove onto the key"); return; }
        importFiles (files, key, cut);
    }

    void importFiles (StringArray files, int startKey, bool cut = false)
    {
        // one dropped groove becomes one pattern; it's cut into several keys only when you ask (Shift while dropping, or the menu)
        cut = cut || ModifierKeys::currentModifiers.isShiftDown();
        files.sortNatural();
        std::vector<Pattern> pieces;
        for (auto& path : files) {
            File f (path);
            if (! f.existsAsFile()) continue;
            auto part = readMidiFile (f);
            if (part.notes.empty()) continue;
            for (auto& p : splitToPatterns (part, cut ? st.song().settings.splitBars : 0)) {
                if (! cut) p.source = f.getFullPathName();
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
                while (k < 128 && s.settings.isStop (k)) ++k;           // never onto a STOP key
                if (k > 127) break;
                s.keys[(size_t) k] = p;
                s.keys[(size_t) k].type = isBlackKey (k) ? PatternType::Fill : PatternType::Groove;   // dropped on a black key: a fill, on a white key: a groove (change it by hand afterwards)
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
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (menuAnchor != nullptr ? menuAnchor : (Component*) &funcBtn), [this] (int r) { runFunction (r); });
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
            case 34: { Component* a = menuAnchor != nullptr ? menuAnchor : (Component*) &funcBtn; CallOutBox::launchAsynchronously (std::make_unique<GhostPanel> (st), getLocalArea (a, a->getLocalBounds()), this); }; break;
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

    // ------------------------------------------------------------ key groups (Intro, Verse...)
    void newGroup (int key, std::set<int> targets = {})
    {
        targets.insert (key);
        auto* aw = new AlertWindow ("New group", "A name, for example Intro, Verse, Chorus:", MessageBoxIconType::NoIcon);
        aw->addTextEditor ("n", "");
        aw->addButton ("Create", 1, KeyPress (KeyPress::returnKey)); aw->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        aw->enterModalState (true, ModalCallbackFunction::create ([this, aw, key, targets] (int r) {
            const auto n = aw->getTextEditorContents ("n").trim();
            if (r != 1 || n.isEmpty()) return;
            static const uint32 cols[] = { 0xff5aa9ff, 0xffb8f28b, 0xffffc15e, 0xffc9a7ff, 0xffff8fa3, 0xff6ee7d8, 0xffffa36e, 0xffe3e37a };
            st.applySong ("New group", [n, targets] (Song& s2) { s2.groups.push_back ({ n, cols[s2.groups.size() % 8] }); for (int t : targets) s2.keys[(size_t) t].group = (int) s2.groups.size() - 1; });
            st.keySel.clear();
            groupColour (st.song().keys[(size_t) key].group);
        }), true);
    }
    void renameGroup (int g)
    {
        if (g < 0 || g >= (int) st.song().groups.size()) return;
        auto* aw = new AlertWindow ("Rename group", "", MessageBoxIconType::NoIcon);
        aw->addTextEditor ("n", st.song().groups[(size_t) g].name);
        aw->addButton ("OK", 1, KeyPress (KeyPress::returnKey)); aw->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        aw->enterModalState (true, ModalCallbackFunction::create ([this, aw, g] (int r) {
            const auto n = aw->getTextEditorContents ("n").trim();
            if (r == 1 && n.isNotEmpty()) st.applySong ("Rename group", [g, n] (Song& s2) { if (g < (int) s2.groups.size()) s2.groups[(size_t) g].name = n; });
        }), true);
    }
    struct GroupPicker : public ColourSelector, private ChangeListener
    {
        std::function<void (Colour)> onColour;
        GroupPicker() : ColourSelector (ColourSelector::showColourAtTop | ColourSelector::showSliders | ColourSelector::showColourspace) { setSize (300, 300); addChangeListener (this); }
        void changeListenerCallback (ChangeBroadcaster*) override { if (onColour) onColour (getCurrentColour()); }
    };
    void groupColour (int g)
    {
        if (g < 0 || g >= (int) st.song().groups.size()) return;
        auto p = std::make_unique<GroupPicker>();
        p->setCurrentColour (Colour (st.song().groups[(size_t) g].colour), dontSendNotification);
        p->onColour = [this, g] (Colour c) { if (g < (int) proc.edit.groups.size()) { proc.edit.groups[(size_t) g].colour = c.getARGB(); proc.publish(); strip.repaint(); bigStrip.repaint(); tray.repaint(); chainStrip.repaint(); songLane.repaint(); } };
        CallOutBox::launchAsynchronously (std::move (p), getLocalArea (&strip, strip.getLocalBounds()).withSizeKeepingCentre (40, 20), this);
    }

    void keyMenu (int k)
    {
        auto& s = st.song();
        const auto& p = s.keys[(size_t) k];
        PopupMenu m;
        m.addSectionHeader (keyName (k) + (p.isEmpty() ? "  (empty)" : "  " + p.name));
        if (! p.isEmpty()) m.addSectionHeader (String (p.bars) + (p.bars == 1 ? " BAR" : " BARS") + "   " + String (typeName (p.type)).toUpperCase() + "   " + String (p.beatsPerBar) + "/4");
        std::set<int> targets = st.keySel; targets.insert (k);
        if (targets.size() > 1) m.addSectionHeader (String ((int) targets.size()) + " keys picked (Alt+click, Shift+click for a range): type, group and star go to all of them");
        m.addItem (1, "Edit");
        if (! p.isEmpty()) m.addItem (50, String (p.fav ? "Remove the star" : "Favourite (star)") + (targets.size() > 1 ? "  (all picked keys)" : ""), true, p.fav);
        if (! p.notes.empty()) m.addItem (2, "Play");
        if (p.hasOriginal()) {                                     // the version it started with is always kept
            m.addSeparator();
            m.addItem (60, "Play the original", true, p.showingOriginal);
            m.addItem (61, "Play my edited version", p.edited != nullptr || ! p.showingOriginal, ! p.showingOriginal);
            m.addSeparator();
        }
        if (! p.isEmpty()) {
            PopupMenu type; for (int t = 0; t < 4; ++t) type.addItem (10 + t, typeName ((PatternType) t), true, (int) p.type == t);
            m.addSubMenu (targets.size() > 1 ? "Type  (all picked keys)" : "Type", type);
            PopupMenu grp;
            grp.addItem (3000, "No group", true, p.group < 0);
            for (int i = 0; i < (int) s.groups.size() && i < 60; ++i) grp.addColouredItem (3001 + i, s.groups[(size_t) i].name, Colour (s.groups[(size_t) i].colour), true, p.group == i);
            grp.addSeparator();
            if (p.group >= 0) { for (int n : { 2, 4, 8 }) grp.addItem (2910 + n, "Give this group to the next " + String (n) + " keys too"); grp.addSeparator(); }
            grp.addItem (2900, "New group...");
            if (p.group >= 0 && p.group < (int) s.groups.size()) {
                const auto gn = s.groups[(size_t) p.group].name;
                grp.addItem (2901, "Rename \"" + gn + "\"..."); grp.addItem (2902, "Change \"" + gn + "\" colour..."); grp.addItem (2903, "Delete \"" + gn + "\"");
            }
            m.addSubMenu ("Group", grp);
            m.addItem (3, "Copy pattern");
        }
        if (hasPatternClip) m.addItem (4, "Paste pattern here");
        if (! clipboardMidiFiles().isEmpty()) {
            m.addItem (40, p.isEmpty() ? "Paste MIDI file here" : "Replace with the MIDI file on the clipboard");
            m.addItem (41, "Paste MIDI file, cut into keys every " + String (jmax (1, s.settings.splitBars)) + " bars");
        }
        m.addItem (42, p.isEmpty() ? "Import MIDI file here..." : "Replace with a MIDI file...");
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
        m.showMenuAsync (PopupMenu::Options().withMousePosition(), [this, k, targets] (int r) {
            switch (r) {
                case 60: st.applySong ("Play the original", [k] (Song& s2) { playOriginal (s2.keys[(size_t) k], true); }); break;
                case 61: st.applySong ("Play my edited version", [k] (Song& s2) { playOriginal (s2.keys[(size_t) k], false); }); break;
                case 50: {
                    const bool on = ! st.song().keys[(size_t) k].fav;
                    st.applySong (on ? "Star" : "Remove star", [targets, on] (Song& s2) { for (int t : targets) if (! s2.keys[(size_t) t].isEmpty()) s2.keys[(size_t) t].fav = on; });
                    for (int t : targets) { const auto& q = st.song().keys[(size_t) t]; setGrooveStar (q.source.isNotEmpty() ? q.source : q.name, on); }
                    break;
                }
                case 1: st.song().editKey = k; refreshAll(); break;
                case 2: proc.uiTrigger (k); break;
                case 3: patternClip = st.song().keys[(size_t) k]; hasPatternClip = true; break;
                case 4: { auto c = patternClip; st.applySong ("Paste pattern", [k, c] (Song& s2) { s2.keys[(size_t) k] = c; s2.ensureLanesForPattern (c); s2.editKey = k; }); break; }
                case 5: { const int d = nextFreeKey (k, false, true); if (d < 0) { showToast ("No free key left"); break; }
                          st.applySong ("Duplicate pattern", [k, d] (Song& s2) { s2.keys[(size_t) d] = s2.keys[(size_t) k]; s2.keys[(size_t) d].name += " copy"; s2.editKey = d; }); break; }
                case 6: exportPattern (k); break;
                case 7: st.applySong ("Clear key", [k] (Song& s2) { s2.keys[(size_t) k] = Pattern(); }); break;
                case 40: pasteMidiToKey (k, false); break;
                case 41: pasteMidiToKey (k, true); break;
                case 42: {
                    chooser = std::make_unique<FileChooser> ("Import MIDI", File::getSpecialLocation (File::userDocumentsDirectory), "*.mid;*.midi");
                    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::canSelectMultipleItems,
                        [this, k] (const FileChooser& fc) { StringArray f; for (auto& r2 : fc.getResults()) f.add (r2.getFullPathName()); if (! f.isEmpty()) importFiles (f, k); });
                    break;
                }
                case 8: learnFrom = k; learnSerial = proc.lastInSerial.load(); showToast ("Press the key on your keyboard where " + keyName (k) + "'s pattern should go"); break;
                case 2900: newGroup (k, targets); break;
                case 2901: renameGroup (st.song().keys[(size_t) k].group); break;
                case 2902: groupColour (st.song().keys[(size_t) k].group); break;
                case 2903: { const int g = st.song().keys[(size_t) k].group;
                             st.applySong ("Delete group", [g] (Song& s2) { if (g < 0 || g >= (int) s2.groups.size()) return; s2.groups.erase (s2.groups.begin() + g);
                                 for (auto& q : s2.keys) { if (q.group == g) q.group = -1; else if (q.group > g) q.group--; }
                                 for (auto& q : s2.park) { if (q.group == g) q.group = -1; else if (q.group > g) q.group--; } }); break; }
                default:
                    if (r >= 3000 && r < 3062) { const int g = r - 3001; st.applySong ("Key group", [targets, g] (Song& s2) { for (int t : targets) s2.keys[(size_t) t].group = g; }); st.keySel.clear(); strip.repaint(); }
                    else if (r >= 2912 && r <= 2918) {
                        const int n = r - 2910;
                        st.applySong ("Key group", [k, n] (Song& s2) { const int g = s2.keys[(size_t) k].group; for (int i = 1; i <= n && k + i < 128; ++i) s2.keys[(size_t) (k + i)].group = g; });
                    }
                    else if (r >= 10 && r < 14) { const int t = r - 10; st.applySong ("Pattern type", [targets, t] (Song& s2) { for (int q : targets) if (! s2.keys[(size_t) q].isEmpty()) s2.keys[(size_t) q].type = (PatternType) t; }); st.keySel.clear(); strip.repaint(); }
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
