// Drum Player: groove browser. A library of MIDI grooves you can sort by style and type (groove, fill, intro,
// ending), with name, who it's inspired by and who programmed it; preview, and drag onto keys.
#pragma once
#include "DPPanels.h"

namespace dp {

struct GrooveInfo
{
    File file;
    String name, style, type, inspired, programmer;
    int bars = 0; double bpm = 0; int beatsPerBar = 4; bool scanned = false;
};

class GrooveLibrary
{
public:
    std::vector<GrooveInfo> items;
    StringArray folders;
    bool autoPlay = false;                  // a groove plays as soon as you click it

    static File indexFile() { return dataFolder().getChildFile ("GrooveInfo.xml"); }
    static File defaultFolder() { auto d = dataFolder().getChildFile ("Grooves"); d.createDirectory(); return d; }

    void load()
    {
        folders.clear(); stored.clear();
        if (auto x = XmlDocument::parse (indexFile())) {
            autoPlay = x->getBoolAttribute ("autoplay", false);
            for (auto* f : x->getChildWithTagNameIterator ("Folder")) folders.addIfNotAlreadyThere (f->getStringAttribute ("path"));
            for (auto* e : x->getChildWithTagNameIterator ("Groove")) {
                GrooveInfo g; g.file = File (e->getStringAttribute ("path"));
                g.name = e->getStringAttribute ("name"); g.style = e->getStringAttribute ("style"); g.type = e->getStringAttribute ("type");
                g.inspired = e->getStringAttribute ("inspired"); g.programmer = e->getStringAttribute ("programmer");
                g.bpm = e->getDoubleAttribute ("bpm"); g.bars = e->getIntAttribute ("bars"); g.beatsPerBar = e->getIntAttribute ("bpb", 4);
                g.scanned = g.bars > 0;
                stored[g.file.getFullPathName()] = g;
            }
        }
        folders.addIfNotAlreadyThere (defaultFolder().getFullPathName());
        scan();
    }
    void save() const
    {
        XmlElement x ("DrumPlayerGrooves");
        x.setAttribute ("autoplay", autoPlay);
        for (auto& f : folders) x.createNewChildElement ("Folder")->setAttribute ("path", f);
        for (auto& g : items) {
            auto* e = x.createNewChildElement ("Groove");
            e->setAttribute ("path", g.file.getFullPathName()); e->setAttribute ("name", g.name); e->setAttribute ("style", g.style);
            e->setAttribute ("type", g.type); e->setAttribute ("inspired", g.inspired); e->setAttribute ("programmer", g.programmer);
            e->setAttribute ("bpm", g.bpm); e->setAttribute ("bars", g.bars); e->setAttribute ("bpb", g.beatsPerBar);
        }
        x.writeTo (indexFile());
    }
    void scan()
    {
        items.clear();
        for (auto& path : folders) {
            File dir (path);
            if (! dir.isDirectory()) continue;
            for (const auto& entry : RangedDirectoryIterator (dir, true, "*.mid;*.midi", File::findFiles)) {
                const auto f = entry.getFile();
                auto it = stored.find (f.getFullPathName());
                if (it != stored.end()) { items.push_back (it->second); continue; }
                GrooveInfo g; g.file = f; g.name = f.getFileNameWithoutExtension();
                // first guesses from the file and folder names; you can change every field
                const auto low = (f.getFileName() + " " + f.getParentDirectory().getFileName()).toLowerCase();
                g.type = low.contains ("fill") ? "Fill" : low.contains ("intro") ? "Intro" : (low.contains ("ending") || low.contains ("outro") || low.contains ("end ")) ? "Ending" : "Groove";
                g.style = f.getParentDirectory() == dir ? String ("Unsorted") : f.getParentDirectory().getFileName();
                items.push_back (g);
                if (items.size() > 20000) break;
            }
        }
    }
    // bars and tempo are read from the file the first time a groove is shown
    void ensureScanned (GrooveInfo& g)
    {
        if (g.scanned) return;
        g.scanned = true;
        auto part = readMidiFile (g.file);
        g.beatsPerBar = part.beatsPerBar;
        g.bars = part.lengthTicks / jmax (1, part.beatsPerBar * PPQ);
        FileInputStream in (g.file); MidiFile mf;
        if (in.openedOk() && mf.readFrom (in)) {
            MidiMessageSequence tempos; mf.findAllTempoEvents (tempos);
            if (tempos.getNumEvents() > 0) {
                const double spq = tempos.getEventPointer (0)->message.getTempoSecondsPerQuarterNote();
                if (spq > 0) g.bpm = std::round (600.0 / spq) / 10.0;
            }
        }
    }
private:
    std::map<String, GrooveInfo> stored;
};

class GroovePage : public Page, private TableListBoxModel, public DragAndDropTarget
{
public:
    GroovePage (EditState& e) : Page ("GROOVES"), st (e)
    {
        styleModel.owner = this;
        addAndMakeVisible (styles); styles.setModel (&styleModel); styles.setRowHeight (22);
        styles.setColour (ListBox::backgroundColourId, Colours::black.withAlpha (0.2f));
        addAndMakeVisible (search); search.setTextToShowWhenEmpty ("Search names, styles, people", th::faint); search.setFont (font (12.0f, 1));
        search.onTextChange = [this] { filter(); };
        for (auto* t : { "All", "Groove", "Fill", "Intro", "Ending" }) {
            auto* b = typeBtns.add (new TextButton());
            setupButton (*this, *b, t, true); b->setRadioGroupId (71);
            b->onClick = [this] { filter(); };
        }
        typeBtns[0]->setToggleState (true, dontSendNotification);
        addAndMakeVisible (table); table.setModel (this); table.setRowHeight (22); table.setMultipleSelectionEnabled (false);
        table.setColour (ListBox::backgroundColourId, Colours::black.withAlpha (0.2f));
        auto& h = table.getHeader();
        h.addColumn (String::fromUTF8 ("\xe2\x98\x85"), 8, 26, 22, 30, TableHeaderComponent::defaultFlags, 0);   // your stars
        h.addColumn ("Name", 1, 190); h.addColumn ("Type", 2, 64); h.addColumn ("Style", 3, 110); h.addColumn ("Bars", 4, 44);
        h.addColumn ("BPM", 5, 52); h.addColumn ("Inspired by", 6, 120); h.addColumn ("Programmed by", 7, 120);
        h.setSortColumnId (1, true);
        h.setColour (TableHeaderComponent::backgroundColourId, th::surface); h.setColour (TableHeaderComponent::textColourId, th::text);
        h.setColour (TableHeaderComponent::outlineColourId, th::line);
        setupButton (*this, play, "Play"); play.getProperties().set ("lit", (int) 0xff6fe08f); play.onClick = [this] { preview(); };
        setupButton (*this, stop, "Stop"); stop.onClick = [this] { st.proc.uiStop(); };
        setupButton (*this, autoBtn, "Auto-play", true, "A groove plays as soon as you click it");
        autoBtn.onClick = [this] { lib.autoPlay = autoBtn.getToggleState(); lib.save(); };
        setupButton (*this, saveKey, "Save key as groove...", false, "Save the key you're editing into your groove library (a MIDI file in Documents, kept when Drum Player is removed). You can also drag a key here");
        saveKey.onClick = [this] { saveKeyAsGroove (st.song().editKey); };
        setupButton (*this, toKey, "Put on the edited key"); toKey.onClick = [this] { if (auto* g = selected()) if (onLoad) onLoad (g->file, st.song().editKey); };
        setupButton (*this, info, "Edit info..."); info.onClick = [this] { editInfo(); };
        setupButton (*this, addFolder, "Add folder..."); addFolder.onClick = [this] { chooseFolder(); };
        setupButton (*this, rescan, "Rescan"); rescan.onClick = [this] { lib.scan(); lib.save(); rebuild(); };
        setupButton (*this, openFolder, "Groove folder"); openFolder.onClick = [] { GrooveLibrary::defaultFolder().revealToUser(); };
        setupLabel (*this, help, "Double-click to listen, drag a groove onto a key. Drag a key here (or Save key as groove) to keep it in your library. Every groove is a MIDI file in Documents/BD Plugins/Drum Player/Grooves, so it stays when Drum Player is removed.", 10.5f, th::faint);
        help.setJustificationType (Justification::topLeft);
        lib.load();
        autoBtn.setToggleState (lib.autoPlay, dontSendNotification);
    }
    std::function<void (const File&, int key)> onLoad;
    void refresh() override { rebuild(); }

    // drag a key onto the library to save it
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("key:"); }
    void itemDragEnter (const SourceDetails&) override { dropGlow = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dropGlow = false; repaint(); }
    void itemDropped (const SourceDetails& d) override { dropGlow = false; repaint(); saveKeyAsGroove (d.description.toString().fromFirstOccurrenceOf ("key:", false, false).getIntValue()); }
    void paintOverChildren (Graphics& g) override
    {
        if (! dropGlow) return;
        g.setColour (th::amber.withAlpha (0.8f)); g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (3), 10, 2.0f);
        g.setFont (font (13.0f, 2)); g.drawText ("Drop to save this key in your grooves", getLocalBounds().removeFromBottom (36), Justification::centred);
    }

    // a key's pattern becomes a MIDI file in the groove folder (style = subfolder), with its info in the library
    void saveKeyAsGroove (int key)
    {
        if (key < 0 || key >= 128 || st.song().keys[(size_t) key].notes.empty()) { if (st.toast) st.toast ("That key is empty"); return; }
        const auto& p = st.song().keys[(size_t) key];
        static const char* types[] = { "Groove", "Fill", "Intro", "Ending" };
        auto* aw = new AlertWindow ("Save as groove", "Key " + keyName (key) + " goes into your groove library", MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", p.name.isNotEmpty() ? p.name : "Groove " + keyName (key), "Name");
        StringArray styleList; for (int i = 1; i < styleNames.size(); ++i) if (styleNames[i] != "Unsorted") styleList.add (styleNames[i]);
        aw->addTextEditor ("style", styleSel > 0 ? styleNames[styleSel] : String ("My grooves"), "Style (folder)");
        aw->addComboBox ("type", { "Groove", "Fill", "Intro", "Ending" }, "Type");
        if (auto* cb = aw->getComboBoxComponent ("type")) cb->setSelectedItemIndex (jlimit (0, 3, (int) p.type), dontSendNotification);
        aw->addTextEditor ("inspired", "", "Sounds like / inspired by");
        aw->addTextEditor ("programmer", "", "Programmed by");
        aw->addButton ("Save", 1, KeyPress (KeyPress::returnKey)); aw->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        aw->enterModalState (true, ModalCallbackFunction::create ([this, aw, key] (int r) {
            if (r != 1) return;
            const auto& pat = st.song().keys[(size_t) key];
            auto name = aw->getTextEditorContents ("name").trim(); if (name.isEmpty()) name = "Groove";
            auto style = aw->getTextEditorContents ("style").trim(); if (style.isEmpty()) style = "My grooves";
            auto dir = GrooveLibrary::defaultFolder().getChildFile (File::createLegalFileName (style)); dir.createDirectory();
            auto f = dir.getChildFile (File::createLegalFileName (name) + ".mid");
            for (int i = 2; f.existsAsFile() && i < 999; ++i) f = dir.getChildFile (File::createLegalFileName (name) + " " + String (i) + ".mid");
            const double bpm = st.proc.lastBpm.load();
            writeMidiFile ({ &pat }, f, st.song().settings.outChannel, bpm);
            GrooveInfo gi; gi.file = f; gi.name = name; gi.style = style;
            if (auto* cb = aw->getComboBoxComponent ("type")) gi.type = cb->getText(); else gi.type = types[0];
            gi.inspired = aw->getTextEditorContents ("inspired"); gi.programmer = aw->getTextEditorContents ("programmer");
            gi.bars = pat.bars; gi.beatsPerBar = pat.beatsPerBar; gi.bpm = bpm > 0 ? std::round (bpm * 10.0) / 10.0 : 0; gi.scanned = true;
            lib.items.push_back (gi); lib.save(); lib.load(); rebuild();
            if (st.toast) st.toast ("Saved " + name + " in " + style);
        }), true);
    }
    void resized() override
    {
        auto a = content();
        auto top = a.removeFromTop (28);
        search.setBounds (top.removeFromLeft (220).reduced (0, 3)); top.removeFromLeft (10);
        for (auto* b : typeBtns) { b->setBounds (top.removeFromLeft (b->getWidth()).reduced (0, 3)); top.removeFromLeft (4); }
        a.removeFromTop (6);
        auto bottom = a.removeFromBottom (64);
        styles.setBounds (a.removeFromLeft (150)); a.removeFromLeft (8);
        table.setBounds (a);
        auto b1 = bottom.withTrimmedTop (6).removeFromTop (26);
        for (Component* c : { (Component*) &play, (Component*) &stop, (Component*) &autoBtn, (Component*) &saveKey, (Component*) &toKey, (Component*) &info, (Component*) &addFolder, (Component*) &rescan, (Component*) &openFolder }) {
            c->setBounds (b1.removeFromLeft (c->getWidth()).reduced (0, 2)); b1.removeFromLeft (5);
        }
        help.setBounds (bottom.withTrimmedTop (34));
    }

private:
    EditState& st;
    GrooveLibrary lib;
    std::vector<int> rows;                 // indices into lib.items, filtered and sorted
    StringArray styleNames; int styleSel = 0;
    TextEditor search; OwnedArray<TextButton> typeBtns;
    TableListBox table; ListBox styles;
    TextButton play, stop, autoBtn, saveKey, toKey, info, addFolder, rescan, openFolder;
    Label help;
    std::unique_ptr<FileChooser> chooser;
    bool dropGlow = false;

    struct StyleModel : public ListBoxModel
    {
        GroovePage* owner = nullptr;
        int getNumRows() override { return owner != nullptr ? owner->styleNames.size() : 0; }
        void paintListBoxItem (int r, Graphics& g, int w, int h, bool sel) override
        {
            if (sel) { g.setColour (th::blue.withAlpha (0.22f)); g.fillRoundedRectangle (2, 1, w - 4.0f, h - 2.0f, 4); }
            g.setFont (font (12.0f, r == 0 ? 3 : 1)); g.setColour (th::text);
            g.drawText (owner->styleNames[r], 8, 0, w - 10, h, Justification::centredLeft, true);
        }
        void selectedRowsChanged (int r) override { owner->styleSel = jmax (0, r); owner->filter(); }
    } styleModel;

    StringArray stars;
    bool starred (const GrooveInfo& g) const { return stars.contains (g.file.getFullPathName()) || stars.contains (g.name); }
    void rebuild()
    {
        stars = grooveStars();
        StringArray s2; for (auto& g : lib.items) s2.addIfNotAlreadyThere (g.style.isNotEmpty() ? g.style : "Unsorted");
        s2.sortNatural(); s2.insert (0, "All styles");
        styleNames = s2; styles.updateContent();
        if (styles.getSelectedRow() < 0) styles.selectRow (0);
        filter();
    }
    void filter()
    {
        rows.clear();
        String type; for (auto* b : typeBtns) if (b->getToggleState()) type = b->getButtonText();
        const auto style = styleSel > 0 ? styleNames[styleSel] : String();
        const auto q = search.getText().trim();
        for (int i = 0; i < (int) lib.items.size(); ++i) {
            const auto& g = lib.items[(size_t) i];
            if (type.isNotEmpty() && type != "All" && g.type != type) continue;
            if (style.isNotEmpty() && (g.style.isNotEmpty() ? g.style : "Unsorted") != style) continue;
            if (q.isNotEmpty() && ! (g.name.containsIgnoreCase (q) || g.style.containsIgnoreCase (q) || g.inspired.containsIgnoreCase (q) || g.programmer.containsIgnoreCase (q))) continue;
            rows.push_back (i);
        }
        sortOrderChanged (table.getHeader().getSortColumnId(), table.getHeader().isSortedForwards());
    }
    GrooveInfo* selected() { const int r = table.getSelectedRow(); return (r >= 0 && r < (int) rows.size()) ? &lib.items[(size_t) rows[(size_t) r]] : nullptr; }
    String field (const GrooveInfo& g, int col) const
    {
        switch (col) {
            case 8: return starred (g) ? "1" : "";
            case 1: return g.name; case 2: return g.type; case 3: return g.style;
            case 4: return g.bars > 0 ? String (g.bars) : String();
            case 5: return g.bpm > 0 ? String (g.bpm, 1) : String();
            case 6: return g.inspired; case 7: return g.programmer;
        }
        return {};
    }

    // ------------------------------------------------------------ TableListBoxModel
    int getNumRows() override { return (int) rows.size(); }
    void paintRowBackground (Graphics& g, int row, int w, int h, bool sel) override
    {
        if (sel) { g.setColour (th::blue.withAlpha (0.22f)); g.fillRect (0, 0, w, h); }
        else if (row % 2) { g.setColour (Colours::white.withAlpha (0.03f)); g.fillRect (0, 0, w, h); }
    }
    void paintCell (Graphics& g, int row, int col, int w, int h, bool) override
    {
        if (row >= (int) rows.size()) return;
        auto& gi = lib.items[(size_t) rows[(size_t) row]];
        if (col == 8) { if (starred (gi)) KeyStrip::drawStar (g, { w * 0.5f, h * 0.5f }, (float) h * 0.75f); return; }
        if (col == 4 || col == 5) lib.ensureScanned (gi);
        g.setFont (font (12.0f, col == 1 ? 2 : 1));
        g.setColour (col == 2 ? (gi.type == "Fill" ? th::amber : gi.type == "Intro" ? th::green : gi.type == "Ending" ? th::red : th::blue) : th::text);
        g.drawText (field (gi, col), 6, 0, w - 8, h, Justification::centredLeft, true);
    }
    void sortOrderChanged (int col, bool forwards) override
    {
        std::stable_sort (rows.begin(), rows.end(), [this, col, forwards] (int a, int b) {
            auto& ga = lib.items[(size_t) a]; auto& gb = lib.items[(size_t) b];
            if (col == 4 || col == 5) { lib.ensureScanned (ga); lib.ensureScanned (gb); const double va = col == 4 ? ga.bars : ga.bpm, vb = col == 4 ? gb.bars : gb.bpm; return forwards ? va < vb : va > vb; }
            const int c = field (ga, col).compareNatural (field (gb, col));
            return forwards ? c < 0 : c > 0;
        });
        table.updateContent(); table.repaint();
    }
    void cellDoubleClicked (int, int, const MouseEvent&) override { preview(); }
    void selectedRowsChanged (int) override { if (lib.autoPlay) preview(); }
    void cellClicked (int row, int col, const MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { table.selectRow (row); rowMenu(); return; }
        if (col == 8 && row < (int) rows.size()) {                        // click the star column: star or unstar
            auto& gi = lib.items[(size_t) rows[(size_t) row]];
            const bool on = ! starred (gi);
            setGrooveStar (gi.file.getFullPathName(), on); if (! on) setGrooveStar (gi.name, false);
            stars = grooveStars();
            // keys holding this groove follow
            for (auto& k : st.song().keys) if (k.source == gi.file.getFullPathName()) k.fav = on;
            st.proc.publish(); if (st.changed) st.changed();
            table.repaintRow (row);
        }
    }
    var getDragSourceDescription (const SparseSet<int>& sel) override
    {
        if (sel.isEmpty() || sel[0] >= (int) rows.size()) return {};
        return "groove:" + lib.items[(size_t) rows[(size_t) sel[0]]].file.getFullPathName();
    }

    void preview()
    {
        auto* g = selected(); if (g == nullptr) return;
        auto part = readMidiFile (g->file);
        auto pieces = splitToPatterns (part, 0);
        if (pieces.empty()) return;
        auto p = pieces.front();
        const int from = st.song().importMap;
        if (from >= 0 && from != st.song().playMap) convertPattern (p, st.song().map (from), st.song().map (st.song().playMap));
        st.song().keys[previewKey] = p;
        st.song().ensureLanesForPattern (p);
        st.proc.publish();
        st.proc.uiStop();
        st.proc.uiTrigger (previewKey);
    }
    void rowMenu()
    {
        auto* g = selected(); if (g == nullptr) return;
        PopupMenu m;
        m.addItem (1, "Listen"); m.addItem (2, "Put on the edited key (" + keyName (st.song().editKey) + ")"); m.addItem (3, "Edit info...");
        PopupMenu t; for (auto* n : { "Groove", "Fill", "Intro", "Ending" }) t.addItem (10 + t.getNumItems(), n, true, g->type == n);
        m.addSubMenu ("Type", t);
        m.addItem (4, "Show the file");
        m.showMenuAsync (PopupMenu::Options().withMousePosition(), [this, g] (int r) {
            static const char* types[] = { "Groove", "Fill", "Intro", "Ending" };
            if (r == 1) preview();
            else if (r == 2) { if (onLoad) onLoad (g->file, st.song().editKey); }
            else if (r == 3) editInfo();
            else if (r == 4) g->file.revealToUser();
            else if (r >= 10 && r < 14) { g->type = types[r - 10]; lib.save(); filter(); }
        });
    }
    void editInfo()
    {
        auto* g = selected(); if (g == nullptr) return;
        const String path = g->file.getFullPathName();
        auto* aw = new AlertWindow ("Groove info", g->file.getFileName(), MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", g->name, "Name");
        aw->addTextEditor ("style", g->style, "Style");
        aw->addComboBox ("type", { "Groove", "Fill", "Intro", "Ending" }, "Type");
        if (auto* cb = aw->getComboBoxComponent ("type")) cb->setText (g->type, dontSendNotification);
        aw->addTextEditor ("inspired", g->inspired, "Sounds like / inspired by");
        aw->addTextEditor ("programmer", g->programmer, "Programmed by");
        aw->addTextEditor ("bpm", g->bpm > 0 ? String (g->bpm) : String(), "Tempo (BPM)");
        aw->addButton ("Save", 1, KeyPress (KeyPress::returnKey)); aw->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
        aw->enterModalState (true, ModalCallbackFunction::create ([this, aw, path] (int r) {
            if (r != 1) return;
            for (auto& gi : lib.items) if (gi.file.getFullPathName() == path) {
                gi.name = aw->getTextEditorContents ("name"); gi.style = aw->getTextEditorContents ("style");
                if (auto* cb = aw->getComboBoxComponent ("type")) gi.type = cb->getText();
                gi.inspired = aw->getTextEditorContents ("inspired"); gi.programmer = aw->getTextEditorContents ("programmer");
                gi.bpm = aw->getTextEditorContents ("bpm").getDoubleValue();
            }
            lib.save(); rebuild();
        }), true);
    }
    void chooseFolder()
    {
        chooser = std::make_unique<FileChooser> ("Add a folder of MIDI grooves", File::getSpecialLocation (File::userDocumentsDirectory));
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [this] (const FileChooser& fc) {
            auto d = fc.getResult();
            if (! d.isDirectory()) return;
            lib.folders.addIfNotAlreadyThere (d.getFullPathName());
            lib.scan(); lib.save(); rebuild();
            if (st.toast) st.toast ("Added " + d.getFileName() + ": " + String ((int) lib.items.size()) + " grooves in the library");
        });
    }
};


// ---------------------------------------------------------------- groove sets: folders of grooves (Style / Tempo / Feel / Tune / Part / Variation),
// shown as columns you click through. A folder with MIDI files (or part folders with MIDI files) is a set you can load onto the keys.
class GrooveSetsPage : public Page
{
public:
    explicit GrooveSetsPage (EditState& e) : Page ("GROOVE SETS"), st (e)
    {
        setupLabel (*this, lRoot, "Library");
        setupCombo (*this, root, {}, 1, "The top folder of your sets. Add more with Add library...");
        root.onChange = [this] { path.clear(); if (File::isAbsolutePath (root.getText())) path.add (File (root.getText())); rebuildColumns(); };
        for (int i = 0; i < maxCols; ++i) {
            auto* c = cols.add (new Column (*this, i));
            addAndMakeVisible (c->list);
        }
        setupButton (*this, play, "Play"); play.getProperties().set ("lit", (int) 0xff6fe08f); play.onClick = [this] { preview(); };
        setupButton (*this, stop, "Stop"); stop.onClick = [this] { st.proc.uiStop(); };
        setupButton (*this, autoBtn, "Auto-play", true, "A groove plays as soon as you click it");
        autoBtn.onClick = [this] { savePrefs(); };
        setupButton (*this, loadSet, "Load this set onto the keys", false, "Every groove in the selected folder goes onto the keys from C-1 up: grooves on white keys, fills on black keys, each part folder as a key colour group. Ctrl+Z undoes it");
        loadSet.onClick = [this] { if (onLoadSet) onLoadSet (selectedFolder()); };
        setupButton (*this, toKey, "Put on the edited key"); toKey.onClick = [this] { if (selFile.existsAsFile() && onLoadFile) onLoadFile (selFile, st.song().editKey); };
        setupButton (*this, saveSet, "Save keys as a set...", false, "Write every key into folders like these (Style / Tempo / Feel / Tune / Part)");
        saveSet.onClick = [this] { if (onSaveSet) onSaveSet(); };
        setupButton (*this, addRoot, "Add library..."); addRoot.onClick = [this] { chooseRoot(); };
        setupButton (*this, showBtn, "Show folder"); showBtn.onClick = [this] { (selFile.existsAsFile() ? selFile : selectedFolder()).revealToUser(); };
        setupLabel (*this, help, "A set is a folder: Style / Tempo / Feel / Tune / Part / Variation, the way Save keys as a set writes it, and the way other drum programs keep their MIDI. "
                                 "Any folder of MIDI files works. Click through the columns, drag a groove onto a key, or load a whole set.", 10.5f, th::faint);
        help.setJustificationType (Justification::topLeft);
    }
    std::function<void (const File&)> onLoadSet;
    std::function<void (const File&, int)> onLoadFile;
    std::function<void()> onSaveSet;
    std::function<StringArray()> knownRoots;           // folders you exported sets to
    std::function<void (const File&)> onPreview;

    void refresh() override
    {
        StringArray roots;
        auto prefs = prefsFile();
        if (auto x = XmlDocument::parse (prefs)) for (auto* r : x->getChildWithTagNameIterator ("Root")) roots.addIfNotAlreadyThere (r->getStringAttribute ("path"));
        if (knownRoots) for (auto& r : knownRoots()) roots.addIfNotAlreadyThere (r);
        roots.addIfNotAlreadyThere (GrooveLibrary::defaultFolder().getFullPathName());
        StringArray existing; for (auto& r : roots) if (File (r).isDirectory()) existing.add (r);
        const auto keep = root.getText();
        root.clear (dontSendNotification);
        for (int i = 0; i < existing.size(); ++i) root.addItem (existing[i], i + 1);
        root.setText (existing.contains (keep) ? keep : existing[0], dontSendNotification);
        if (! File::isAbsolutePath (root.getText())) { path.clear(); rebuildColumns(); return; }
        if (path.isEmpty() || path[0] != File (root.getText())) { path.clear(); path.add (File (root.getText())); }
        if (auto x = XmlDocument::parse (prefs)) {
            autoBtn.setToggleState (x->getBoolAttribute ("autoplay", false), dontSendNotification);
            if (path.size() <= 1) { const File last (x->getStringAttribute ("last")); if (last.isDirectory()) { openPath (last); return; } }
        }
        rebuildColumns();
    }
    // open the columns down to this folder (it must be inside the library shown)
    void openPath (const File& d)
    {
        const File r (File::isAbsolutePath (root.getText()) ? File (root.getText()) : File());
        const auto rp = r.getFullPathName(), dp = d.getFullPathName();
        if (rp.isEmpty() || ! (dp == rp || dp.startsWith (rp + File::getSeparatorString()))) { rebuildColumns(); return; }
        Array<File> chain; for (File f = d; f != r && f.exists() && chain.size() < 64; f = f.getParentDirectory()) chain.insert (0, f);
        path.clear(); path.add (r); path.addArray (chain); selFile = File();
        rebuildColumns();
    }
    void resized() override
    {
        auto a = content();
        auto top = a.removeFromTop (28);
        lRoot.setBounds (top.removeFromLeft (56)); root.setBounds (top.removeFromLeft (jmin (520, top.getWidth())).reduced (0, 3));
        a.removeFromTop (6);
        auto bottom = a.removeFromBottom (64);
        const int n = maxCols, w = (a.getWidth() - (n - 1) * 4) / n;
        for (int i = 0; i < n; ++i) { cols[i]->list.setBounds (a.removeFromLeft (w)); a.removeFromLeft (4); }
        auto b1 = bottom.withTrimmedTop (6).removeFromTop (26);
        for (Component* c : { (Component*) &play, (Component*) &stop, (Component*) &autoBtn, (Component*) &loadSet, (Component*) &toKey, (Component*) &saveSet, (Component*) &addRoot, (Component*) &showBtn }) {
            c->setBounds (b1.removeFromLeft (c->getWidth()).reduced (0, 2)); b1.removeFromLeft (5);
        }
        help.setBounds (bottom.withTrimmedTop (34));
    }
    File selectedFolder() const { return path.isEmpty() ? File() : path.getLast(); }

private:
    static constexpr int maxCols = 6;
    EditState& st;
    Label lRoot, help; ComboBox root;
    TextButton play, stop, autoBtn, loadSet, toKey, saveSet, addRoot, showBtn;
    Array<File> path;              // the folders clicked, from the library down
    File selFile;                  // the groove clicked
    std::unique_ptr<FileChooser> chooser;

    static File prefsFile() { return dataFolder().getChildFile ("GrooveSets.xml"); }
    void savePrefs (const String& addRootPath = {})
    {
        auto x = XmlDocument::parse (prefsFile()); if (x == nullptr) x = std::make_unique<XmlElement> ("GrooveSets");
        if (addRootPath.isNotEmpty()) x->createNewChildElement ("Root")->setAttribute ("path", addRootPath);
        x->setAttribute ("autoplay", autoBtn.getToggleState());
        x->setAttribute ("last", path.isEmpty() ? String() : path.getLast().getFullPathName());
        x->writeTo (prefsFile());
    }

    // one column: the folders and grooves in one folder
    struct Column : public ListBoxModel
    {
        Column (GrooveSetsPage& o, int i) : owner (o), index (i)
        {
            list.setModel (this); list.setRowHeight (22);
            list.setColour (ListBox::backgroundColourId, Colours::black.withAlpha (0.2f));
            list.setColour (ListBox::outlineColourId, Colours::white.withAlpha (0.05f)); list.setOutlineThickness (1);
        }
        GrooveSetsPage& owner; int index; ListBox list; File dir; Array<File> items;
        void fill (const File& d)
        {
            dir = d; items.clear();
            if (d.isDirectory()) {
                Array<File> dirs = d.findChildFiles (File::findDirectories, false), files = d.findChildFiles (File::findFiles, false, "*.mid;*.midi");
                auto byName = [] (const File& a, const File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; };
                std::sort (dirs.begin(), dirs.end(), byName); std::sort (files.begin(), files.end(), byName);
                items.addArray (dirs); items.addArray (files);
            }
            list.updateContent(); list.deselectAllRows(); list.repaint();
        }
        int getNumRows() override { return items.size(); }
        void paintListBoxItem (int r, Graphics& g, int w, int h, bool sel) override
        {
            if (r >= items.size()) return;
            const auto& f = items[r];
            const bool onPath = owner.path.contains (f) || f == owner.selFile;
            if (sel || onPath) { g.setColour (th::blue.withAlpha (sel ? 0.28f : 0.16f)); g.fillRoundedRectangle (2, 1, w - 4.0f, h - 2.0f, 4); }
            const bool isDir = f.isDirectory();
            const auto low = f.getFileName().toLowerCase();
            g.setColour (isDir ? th::text : (low.contains ("fill") ? th::amber : th::blue));
            g.setFont (font (12.0f, isDir ? 2 : 1));
            g.drawText (isDir ? f.getFileName() : f.getFileNameWithoutExtension(), 8, 0, w - 22, h, Justification::centredLeft, true);
            if (isDir) { g.setColour (th::faint); g.drawText (">", w - 16, 0, 12, h, Justification::centred); }
        }
        void listBoxItemClicked (int r, const MouseEvent&) override { owner.clicked (index, r); }
        void listBoxItemDoubleClicked (int r, const MouseEvent&) override
        {
            if (r < items.size() && items[r].existsAsFile() && owner.onLoadFile) owner.onLoadFile (items[r], owner.st.song().editKey);
        }
        var getDragSourceDescription (const SparseSet<int>& s) override
        {
            if (s.isEmpty() || s[0] >= items.size() || ! items[s[0]].existsAsFile()) return {};
            return "groove:" + items[s[0]].getFullPathName();
        }
    };
    OwnedArray<Column> cols;

    void clicked (int col, int row)
    {
        auto* c = cols[col];
        if (row >= c->items.size()) return;
        const auto f = c->items[row];
        // the path shown: the columns scroll when the folders go deeper than six
        const int depthOfCol = firstShown() + col;                       // index in path of the folder this column shows
        while (path.size() > depthOfCol + 1) path.removeLast();
        if (f.isDirectory()) { path.add (f); selFile = File(); savePrefs(); }
        else { selFile = f; if (autoBtn.getToggleState()) preview(); }
        rebuildColumns();
    }
    int firstShown() const { return jmax (0, path.size() - maxCols); }
    void rebuildColumns()
    {
        const int first = firstShown();
        for (int i = 0; i < maxCols; ++i) {
            const int pi = first + i;
            if (pi < path.size()) cols[i]->fill (path[pi]); else cols[i]->fill (File());
        }
        loadSet.setEnabled (path.size() > 1);
        repaint();
    }
    void preview() { if (selFile.existsAsFile() && onPreview) onPreview (selFile); }
    void chooseRoot()
    {
        chooser = std::make_unique<FileChooser> ("Add a library of groove sets", File::getSpecialLocation (File::userDocumentsDirectory));
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [this] (const FileChooser& fc) {
            auto d = fc.getResult(); if (! d.isDirectory()) return;
            savePrefs (d.getFullPathName()); refresh(); root.setText (d.getFullPathName(), sendNotification);
        });
    }
public:
    void autoChanged() { savePrefs(); }
};

} // namespace dp
