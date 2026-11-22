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

    static File indexFile() { return dataFolder().getChildFile ("GrooveInfo.xml"); }
    static File defaultFolder() { auto d = dataFolder().getChildFile ("Grooves"); d.createDirectory(); return d; }

    void load()
    {
        folders.clear(); stored.clear();
        if (auto x = XmlDocument::parse (indexFile())) {
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

class GroovePage : public Page, private TableListBoxModel
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
        h.addColumn ("Name", 1, 190); h.addColumn ("Type", 2, 64); h.addColumn ("Style", 3, 110); h.addColumn ("Bars", 4, 44);
        h.addColumn ("BPM", 5, 52); h.addColumn ("Inspired by", 6, 120); h.addColumn ("Programmed by", 7, 120);
        h.setSortColumnId (1, true);
        h.setColour (TableHeaderComponent::backgroundColourId, th::surface); h.setColour (TableHeaderComponent::textColourId, th::text);
        h.setColour (TableHeaderComponent::outlineColourId, th::line);
        setupButton (*this, play, "Play"); play.getProperties().set ("lit", (int) 0xff6fe08f); play.onClick = [this] { preview(); };
        setupButton (*this, stop, "Stop"); stop.onClick = [this] { st.proc.uiStop(); };
        setupButton (*this, toKey, "Put on the edited key"); toKey.onClick = [this] { if (auto* g = selected()) if (onLoad) onLoad (g->file, st.song().editKey); };
        setupButton (*this, info, "Edit info..."); info.onClick = [this] { editInfo(); };
        setupButton (*this, addFolder, "Add folder..."); addFolder.onClick = [this] { chooseFolder(); };
        setupButton (*this, rescan, "Rescan"); rescan.onClick = [this] { lib.scan(); lib.save(); rebuild(); };
        setupButton (*this, openFolder, "Groove folder"); openFolder.onClick = [] { GrooveLibrary::defaultFolder().revealToUser(); };
        setupLabel (*this, help, "Double-click to listen, drag a groove onto a key. Put your MIDI grooves in the Groove folder (subfolders become styles), or add EZdrummer's or any other folder.", 10.5f, th::faint);
        help.setJustificationType (Justification::topLeft);
        lib.load();
    }
    std::function<void (const File&, int key)> onLoad;
    void refresh() override { rebuild(); }
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
        for (Component* c : { (Component*) &play, (Component*) &stop, (Component*) &toKey, (Component*) &info, (Component*) &addFolder, (Component*) &rescan, (Component*) &openFolder }) {
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
    TextButton play, stop, toKey, info, addFolder, rescan, openFolder;
    Label help;
    std::unique_ptr<FileChooser> chooser;

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

    void rebuild()
    {
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
    void cellClicked (int row, int, const MouseEvent& e) override { if (e.mods.isPopupMenu()) { table.selectRow (row); rowMenu(); } }
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

} // namespace dp
