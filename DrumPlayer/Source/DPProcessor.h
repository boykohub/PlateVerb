// Drum Player: the plugin processor and the playback engine.
// It is an instrument with MIDI out (Studio One: pick "Drum Player" as the input of the drum plugin's track).
#pragma once
#include "DPModel.h"

namespace dp {

namespace ids {
    static const char* swing = "swing";       // 50 … 75 %
    static const char* swingGrid = "swingGrid";
    static const char* humTime = "humTime";   // ms
    static const char* humVel = "humVel";
    static const char* push = "push";         // ms, + = late
    static const char* intensity = "intensity";
    static const char* dynamics = "dynamics";
    static const char* kitVol = "kitVol";
    static const char* kitPan = "kitPan";
    static const char* vintRes = "vintRes";       // vintage clock: 0 off, 96 / 48 / 24 steps per beat
    static const char* vintJitter = "vintJitter"; // ms of clock wobble
    static const char* stagger = "stagger";       // MIDI-cable stagger for notes that land together
    static const char* driftTime = "driftTime";   // ms: timing that leans ahead and back slowly, like a drummer
    static const char* driftVel = "driftVel";     // velocity that swells and settles slowly
    static const char* driftBars = "driftBars";   // how slowly the drift moves (bars per swing)
    static const char* phrase = "phrase";         // phrase dynamics: build into the one, lift fills, ease the middles
}

// deterministic noise in −1 … 1, so a pass sounds the same no matter how the host splits blocks
inline float hashNoise (uint32 a, uint32 b, uint32 c, uint32 salt)
{
    uint64 x = ((uint64) a * 0x9E3779B97F4A7C15ull) ^ ((uint64) b * 0xC2B2AE3D27D4EB4Full) ^ ((uint64) c * 0x165667B19E3779F9ull) ^ ((uint64) salt << 17);
    x ^= x >> 30; x *= 0xBF58476D1CE4E5B9ull; x ^= x >> 27; x *= 0x94D049BB133111EBull; x ^= x >> 31;
    return (float) ((double) (x >> 11) / (double) (1ull << 53)) * 2.0f - 1.0f;
}

class DrumPlayerProcessor;

class SongAction : public UndoableAction
{
public:
    SongAction (DrumPlayerProcessor& p, Song b, Song a) : proc (p), before (std::move (b)), after (std::move (a)) {}
    bool perform() override;
    bool undo() override;
    int getSizeInUnits() override { return 1; }
private:
    DrumPlayerProcessor& proc; Song before, after;
};

class DrumPlayerProcessor : public AudioProcessor, private Timer
{
public:
    DrumPlayerProcessor()
        : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "PARAMS", createLayout())
    {
        edit = Song::makeDefault();
        if (lastSessionFile().existsAsFile())
            if (auto x = XmlDocument::parse (lastSessionFile()))
                if (x->hasTagName ("DrumPlayerState")) {
                    if (auto* sx = x->getChildByName ("DrumPlayerSong")) {
                        auto s = songFromXml (*sx);
                        if (s.settings.rememberLast) { edit = s; if (auto* px = x->getChildByName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*px)); restoreUi (*x); }
                    }
                }
        publish();
        startTimerHz (20);
    }
    ~DrumPlayerProcessor() override { *aliveFlag = false; stopTimer(); saveLastSession(); }

    // ---------------------------------------------------------------- AudioProcessor
    const String getName() const override { return "Drum Player"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const String getProgramName (int) override { return {}; }
    void changeProgramName (int, const String&) override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        return l.getMainOutputChannelSet() == AudioChannelSet::stereo() || l.getMainOutputChannelSet() == AudioChannelSet::mono();
    }
    void prepareToPlay (double sr, int) override { sampleRate = sr; resetPlayback(); }
    void releaseResources() override {}
    bool hasEditor() const override { return true; }
    AudioProcessorEditor* createEditor() override;

    // Studio One may ask for the state on any thread, while the editor is changing patterns:
    // build it from the published copy, which never changes once made.
    void getStateInformation (MemoryBlock& dest) override
    {
        std::shared_ptr<const Song> snap;
        { SpinLock::ScopedLockType l (liveLock); snap = live; }
        auto x = stateXmlFor (snap != nullptr ? *snap : Song::makeDefault());
        copyXmlToBinary (*x, dest);
    }
    void setStateInformation (const void* data, int size) override
    {
        auto x = getXmlFromBinary (data, size);
        if (x == nullptr || ! x->hasTagName ("DrumPlayerState")) return;
        if (! MessageManager::existsAndIsCurrentThread()) {       // apply it on the editing thread
            std::weak_ptr<bool> alive = aliveFlag;
            std::shared_ptr<XmlElement> xs (x.release());
            MessageManager::callAsync ([this, alive, xs] { if (alive.lock() != nullptr) applyState (*xs); });
            return;
        }
        applyState (*x);
    }
    std::shared_ptr<bool> aliveFlag = std::make_shared<bool> (true);
    void applyState (const XmlElement& xr)
    {
        const XmlElement* x = &xr;
        {
            if (auto* sx = x->getChildByName ("DrumPlayerSong")) edit = songFromXml (*sx);
            if (auto* px = x->getChildByName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*px));
            restoreUi (*x);
            undo.clearUndoHistory();
            publish();
            stateVersion++;
        }
    }

    std::unique_ptr<XmlElement> stateXml() const { return stateXmlFor (edit); }
    std::unique_ptr<XmlElement> stateXmlFor (const Song& song) const
    {
        auto x = std::make_unique<XmlElement> ("DrumPlayerState");
        x->addChildElement (songToXml (song).release());
        if (auto px = const_cast<AudioProcessorValueTreeState&> (apvts).copyState().createXml()) x->addChildElement (px.release());
        auto* ui = x->createNewChildElement ("UI");
        String pn; { const ScopedLock l (uiLock); pn = presetName; }
        ui->setAttribute ("scale", uiScale); ui->setAttribute ("preset", pn); ui->setAttribute ("grid", uiGrid);
        ui->setAttribute ("zoomX", uiZoomX); ui->setAttribute ("strip", uiStripTools); ui->setAttribute ("shelf", uiShelfTools); ui->setAttribute ("qscope", uiQuantScope); ui->setAttribute ("headW", uiHeadW); ui->setAttribute ("editOpen", uiEditOpen);
        ui->setAttribute ("stopkey", uiStopKey); ui->setAttribute ("project", uiProjectPath);
        return x;
    }
    void restoreUi (const XmlElement& x)
    {
        if (auto* ui = x.getChildByName ("UI")) {
            uiScale = (float) jlimit (0.6, 2.0, ui->getDoubleAttribute ("scale", 1.0));
            { const ScopedLock l (uiLock); presetName = ui->getStringAttribute ("preset"); }
            uiGrid = jlimit (0, numNoteValues - 1, ui->getIntAttribute ("grid", 3));
            uiZoomX = (float) jlimit (0.25, 8.0, ui->getDoubleAttribute ("zoomX", 1.0));
            uiStripTools = ui->getStringAttribute ("strip", uiStripTools); uiShelfTools = ui->getStringAttribute ("shelf", uiShelfTools); uiQuantScope = jlimit (0, 2, ui->getIntAttribute ("qscope", 0)); uiHeadW = jlimit (70, 360, ui->getIntAttribute ("headW", 200)); uiEditOpen = ui->getBoolAttribute ("editOpen", true);
            uiStopKey = ui->getIntAttribute ("stopkey", KeyPress::spaceKey);
            uiProjectPath = ui->getStringAttribute ("project");
        }
    }
    void saveLastSession() const
    {
        if (! edit.settings.rememberLast) { lastSessionFile().deleteFile(); return; }
        if (auto x = stateXml()) x->writeTo (lastSessionFile());
    }

    // ---------------------------------------------------------------- presets (whole song + sound settings)
    bool savePreset (const File& f)
    {
        auto x = stateXml();
        if (! x->writeTo (f)) return false;
        { const ScopedLock l (uiLock); presetName = f.getFileNameWithoutExtension(); }
        return true;
    }
    bool loadPreset (const File& f)
    {
        auto x = XmlDocument::parse (f);
        if (x == nullptr || ! x->hasTagName ("DrumPlayerState")) return false;
        auto* sx = x->getChildByName ("DrumPlayerSong");
        if (sx == nullptr) return false;
        auto s = songFromXml (*sx);
        s.settings = edit.settings;                  // routing and keyboard setup stay as they are
        commit (std::move (s), "Load preset");
        if (auto* px = x->getChildByName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*px));
        { const ScopedLock l (uiLock); presetName = f.getFileNameWithoutExtension(); }
        stateVersion++;
        return true;
    }

    // ---------------------------------------------------------------- editing (message thread)
    Song edit;                         // the editor's working copy
    UndoManager undo { 1000, 100 };
    String presetName; CriticalSection uiLock;
    float uiScale = 1.0f, uiZoomX = 1.0f; int uiGrid = 3, uiHeadW = 200; bool uiEditOpen = true;
    std::atomic<int> loopReqKey { -1 }, loopReqFrom { 0 }, loopReqTo { 0 };   // editor loops (set by the editor)
    String uiProjectPath;                        // the project file Save writes to (Save As picks a new one)
    int uiStopKey = KeyPress::spaceKey;          // the computer key that stops (0 = none)
    bool uiLearnStopKey = false;                 // the next key you press becomes the stop key
    String uiStripTools = "fit,grid,tools,play,stop,rec,quantize";
    int uiQuantScope = 0;                                // the Quantize button: 0 this pattern, 1 the selected keys, 2 all
    String uiShelfTools = "humanize,functions";          // the buttons you put next to the pattern's length   // the Option-key strip: what you put on it
    std::atomic<int> stateVersion { 0 };

    void publish()
    {
        auto s = std::make_shared<Song> (edit);
        const auto dm = s->map (s->playMap);
        for (auto& p : s->keys) if (! p.notes.empty()) {
            computeAutoSwing (p);
            for (auto& n : p.notes) {
                n.silent = n.art >= 0 && dm.resolve (n.art) < 0 && dm.artFor (n.note) != n.art;
                const int a = n.art >= 0 && dm.resolve (n.art) == n.note ? n.art : dm.artFor (n.note);
                n.hhOpen = a == aHHClosed || a == aHHClosedEdge || a == aHHBell ? 0 : a == aHHTight ? 20 : a == aHHPedal ? 0
                         : a == aHHLoose ? 64 : a == aHHOpen ? 100 : a == aHHOpenWide ? 127 : a == aHHSplash ? 90 : -1;
            }
        }
        std::shared_ptr<const Song> old;
        { SpinLock::ScopedLockType l (liveLock); old = live; live = s; }
        if (old != nullptr) graveyard.push_back (std::move (old));
        sendLanePans = true;
        dirty = true;
    }
    // replace the whole song as one undo step
    void commit (Song s, const String& name)
    {
        keepOriginals (edit, s); uniqueClipIds (s); followLoop (s);
        undo.beginNewTransaction (name);
        undo.perform (new SongAction (*this, edit, std::move (s)));
        ++editSerial;
    }
    std::atomic<int> editSerial { 0 };          // goes up with every change you make (for "save changes?")
    // live edits (dragging): remember the start, edit `edit` freely with publish(), then endEdit()
    void beginEdit() { editStart = std::make_unique<Song> (edit); }
    void endEdit (const String& name)
    {
        if (editStart == nullptr) return;
        auto before = std::move (*editStart); editStart.reset();
        keepOriginals (before, edit); uniqueClipIds (edit); followLoop (edit);
        undo.beginNewTransaction (name);
        undo.perform (new SongAction (*this, std::move (before), edit));
        ++editSerial;
    }
    void setEditFromUndo (const Song& s) { edit = s; publish(); stateVersion++; }

    // ---------------------------------------------------------------- UI → engine commands
    void uiTrigger (int key) { pushCmd (CmdNoteOn, key); }
    void uiRelease (int key) { pushCmd (CmdNoteOff, key); }
    void uiStop() { pushCmd (CmdStop, 0); }
    void uiPlayChain (int c) { pushCmd (CmdChain, c); }
    void uiPreview (int note) { pushCmd (CmdPreview, note); }
    void uiPlayFrom (int key, int tick) { pushCmd (CmdPlayFrom, (key << 20) | jlimit (0, (1 << 20) - 1, tick)); }
    void uiRecord (bool start, int key) { pushCmd (start ? CmdRecStart : CmdRecStop, key); }
    void uiSongPlay (bool start, double fromBeat) { pushCmd (start ? CmdSongPlay : CmdSongStop, (int) std::round (fromBeat * 100)); }
    std::atomic<double> songBeatUi { -1.0 };          // song lane playhead (beats), -1 = not playing
    // recording what you play live (pattern keys) onto the song lane
    std::atomic<bool> songRecArmed { false };
    std::atomic<double> songRecCursor { 0.0 };          // where on the lane a recording starts when Studio One isn't playing
    std::atomic<double> nowPpqUi { 0.0 };
    void setSongRecord (bool on)
    {
        if (on) { perfOpen = -1; perfFirst = -1; perfSession++; songRecArmed = true; }
        else { songRecArmed = false; }
    }
    std::atomic<bool> recArmed { false };            // true while counting in or recording
    std::atomic<int> recStateUi { 0 };               // 0 off, 1 counting in, 2 recording, 3 waiting for the first note
    std::atomic<int> recSession { 0 };
    std::array<std::atomic<uint32>, 128> outHits {}, inHits {};   // per note, for the lane lights
    std::array<std::atomic<int>, 128> outVel {};                  // per note: velocity of the last note sent (the mixer's meters)
    std::atomic<int> lastInNote { -1 }, lastInKey { -1 }, lastInSerial { 0 };   // the last key pressed and the pattern key it hit
    std::atomic<int> lastTrigger { 0 };              // what that press did: 1 starts, 2 empty key, 3 stops it, 4 stop key, 5 chain
    std::atomic<uint32> notesSent { 0 };             // every note Drum Player has sent out

    // ---------------------------------------------------------------- display (read by the editor)
    std::atomic<int> playingKey { -1 }, queuedKey { -2 }, chainPlaying { -1 }, chainStep { -1 };
    std::atomic<double> playRelTicks { 0.0 };
    std::atomic<double> lastBpm { 120.0 }, hostBpm { 120.0 };
    std::atomic<bool> hostLooping { false }; std::atomic<double> hostLoopA { 0.0 }, hostLoopB { 0.0 };   // Studio One's loop
    std::atomic<bool> hostPlaying { false };
    std::atomic<uint32> activity { 0 };          // bit per lane note hit recently, for the lane lights (low 0..63 notes folded)
    std::atomic<int> lastHitNote { -1 }, lastHitSerial { 0 };

    AudioProcessorValueTreeState apvts;

private:
    std::unique_ptr<Song> editStart;

    static AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        std::vector<std::unique_ptr<RangedAudioParameter>> p;
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::swing, 1 }, "Swing",
            NormalisableRange<float> (50.0f, 75.0f, 0.01f), 50.0f, AudioParameterFloatAttributes().withLabel ("%")));
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::swingGrid, 1 }, "Swing Value",
            StringArray::fromTokens (swingChoiceNames(), "|", ""), 3));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::humTime, 1 }, "Humanize Timing",
            NormalisableRange<float> (0.0f, 40.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("ms")));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::humVel, 1 }, "Humanize Velocity",
            NormalisableRange<float> (0.0f, 40.0f, 1.0f), 0.0f));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::push, 1 }, "Feel (push/lay back)",
            NormalisableRange<float> (-30.0f, 30.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("ms")));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::intensity, 1 }, "Intensity",
            NormalisableRange<float> (25.0f, 150.0f, 1.0f), 100.0f, AudioParameterFloatAttributes().withLabel ("%")));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::dynamics, 1 }, "Dynamics",
            NormalisableRange<float> (0.0f, 200.0f, 1.0f), 100.0f, AudioParameterFloatAttributes().withLabel ("%")));
        p.push_back (std::make_unique<AudioParameterInt> (ParameterID { ids::kitVol, 1 }, "Kit Volume (CC7)", 0, 127, 100));
        p.push_back (std::make_unique<AudioParameterInt> (ParameterID { ids::kitPan, 1 }, "Kit Pan (CC10)", 0, 127, 64));
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ids::vintRes, 1 }, "Vintage Clock",
            StringArray { "Off", "96 per beat (MPC era)", "48 per beat (LM-1 era)", "24 per beat (early sync)" }, 0));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::vintJitter, 1 }, "Vintage Wobble",
            NormalisableRange<float> (0.0f, 4.0f, 0.01f), 0.0f, AudioParameterFloatAttributes().withLabel ("ms")));
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ids::stagger, 1 }, "MIDI Cable Stagger", false));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::driftTime, 1 }, "Drift Timing",
            NormalisableRange<float> (0.0f, 30.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("ms")));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::driftVel, 1 }, "Drift Velocity",
            NormalisableRange<float> (0.0f, 30.0f, 0.5f), 0.0f));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::driftBars, 1 }, "Drift Speed",
            NormalisableRange<float> (1.0f, 16.0f, 0.5f), 4.0f, AudioParameterFloatAttributes().withLabel ("bars")));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::phrase, 1 }, "Phrase Dynamics",
            NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f, AudioParameterFloatAttributes().withLabel ("%")));
        return { p.begin(), p.end() };
    }

    // ---------------------------------------------------------------- timer: recording merge, cleanup, autosave
    void timerCallback() override
    {
        // drop old song copies the audio thread no longer holds
        graveyard.erase (std::remove_if (graveyard.begin(), graveyard.end(), [] (const std::shared_ptr<const Song>& s) { return s.use_count() <= 1; }), graveyard.end());
        // recorded notes
        int s1, n1, s2, n2;
        const int ready = recFifo.getNumReady();
        if (ready > 0) {
            recFifo.prepareToRead (ready, s1, n1, s2, n2);
            if (editStart == nullptr) beginEdit();
            if (recSession.load() != lastRecSession) { lastRecSession = recSession.load(); replacedLanes.clear(); }
            auto add = [this] (const RecNote& r) {
                auto& p = edit.keys[(size_t) jlimit (0, 127, r.key)];
                if (edit.settings.recTarget == 2) return;                 // the DAW records: nothing is kept here
                if (edit.settings.recLayers) {                            // every pass is its own layer, the newest plays
                    if (recSession.load() != layerSession || r.pass != layerPass) {
                        layerSession = recSession.load(); layerPass = r.pass;
                        if (p.layers.empty() && ! p.notes.empty()) p.layers.push_back (p.notes);     // what was there becomes layer 1
                        p.layers.push_back ({});
                        p.comp = { { 0, p.lengthTicks() * 4, (int) p.layers.size() - 1 } };
                    }
                    Note n; n.tick = r.tick; n.note = r.note; n.velocity = r.vel; n.length = 60;
                    if (edit.settings.recQuantize > 0) {
                        const int g = noteValues[jlimit (0, numNoteValues - 1, edit.settings.recQuantize - 1)].ticks;
                        n.tick = (int) std::round ((double) n.tick / g) * g; if (n.tick >= p.lengthTicks()) n.tick -= p.lengthTicks();
                    }
                    auto& L = p.layers.back();
                    for (auto& o : L) if (o.note == n.note && std::abs (o.tick - n.tick) < 12) { o.velocity = n.velocity; p.applyComp(); return; }
                    L.push_back (n); std::sort (L.begin(), L.end());
                    p.applyComp(); edit.ensureLane (n.note);
                    return;
                }
                if (edit.settings.recMode == 1 && replacedLanes.insert ((r.key << 8) | r.note).second)       // replace: this lane's old notes go
                    p.notes.erase (std::remove_if (p.notes.begin(), p.notes.end(), [&r] (const Note& x) { return x.note == r.note; }), p.notes.end());
                Note n; n.tick = r.tick; n.note = r.note; n.velocity = r.vel; n.length = 60;
                if (edit.settings.recQuantize > 0) {
                    const int g = noteValues[jlimit (0, numNoteValues - 1, edit.settings.recQuantize - 1)].ticks;
                    n.tick = (int) std::round ((double) n.tick / g) * g;
                    if (n.tick >= p.lengthTicks()) n.tick -= p.lengthTicks();
                }
                for (auto& o : p.notes) if (o.note == n.note && std::abs (o.tick - n.tick) < 12) { o.velocity = n.velocity; return; }
                p.notes.push_back (n); p.sort();
                edit.ensureLane (n.note);
            };
            for (int i = 0; i < n1; ++i) add (recBuf[(size_t) (s1 + i)]);
            for (int i = 0; i < n2; ++i) add (recBuf[(size_t) (s2 + i)]);
            recFifo.finishedRead (n1 + n2);
            publish(); stateVersion++;
            recIdle = 0;
        } else if (editStart != nullptr && ! recArmed && ++recIdle > 10) {
            endEdit ("Record");
        }
        mergePerformance();
        if (dirty && ++saveCounter > 60) { saveCounter = 0; dirty = false; saveLastSession(); }
    }
    int lastRecSession = -1, layerSession = -1, layerPass = -1;
    // ---------------------------------------------------------------- performance → song lane
    struct PerfEvent { int key; double ppq, offset; bool host; };
    AbstractFifo perfFifo { 256 }; std::array<PerfEvent, 256> perfBuf;
    int perfOpen = -1, perfSession = 0, perfEditSession = -1; double perfFirst = -1;
    double perfBeat (const PerfEvent& e)
    {
        if (e.host) return jmax (0.0, e.ppq);                               // Studio One playing: its own song position
        if (perfFirst < 0) perfFirst = e.ppq;                               // otherwise from the lane cursor
        return songRecCursor.load() + (e.ppq - perfFirst);
    }
    void mergePerformance()
    {
        const int ready = perfFifo.getNumReady();
        if (ready > 0) {
            int s1, n1, s2, n2;
            perfFifo.prepareToRead (ready, s1, n1, s2, n2);
            if (perfEditSession != perfSession) { if (editStart == nullptr) beginEdit(); perfEditSession = perfSession; }
            for (int i = 0; i < n1 + n2; ++i) {
                const auto e = perfBuf[(size_t) (i < n1 ? s1 + i : s2 + i - n1)];
                const double b = perfBeat (e);
                if (perfOpen >= 0 && perfOpen < (int) edit.clips.size()) { auto& c = edit.clips[(size_t) perfOpen]; c.length = jmax (0.25, b - c.start); }
                perfOpen = -1;
                if (e.key >= 0) {
                    Clip c; c.start = b; c.key = e.key; c.offset = jmax (0.0, e.offset); c.length = 0.25;
                    // whatever was already on the lane there gets replaced
                    edit.clips.erase (std::remove_if (edit.clips.begin(), edit.clips.end(), [b] (const Clip& x) { return x.start >= b - 1e-6 && x.start < b + 0.01; }), edit.clips.end());
                    edit.clips.push_back (c);
                    perfOpen = (int) edit.clips.size() - 1;
                }
            }
            perfFifo.finishedRead (n1 + n2);
            publish(); stateVersion++;
        }
        // the open chunk grows while its pattern plays
        if (perfOpen >= 0 && perfOpen < (int) edit.clips.size()) {
            auto& c = edit.clips[(size_t) perfOpen];
            const double b = perfBeat ({ 0, nowPpqUi.load(), 0, hostPlaying.load() });
            if (b > c.start + c.length + 0.05) { c.length = b - c.start; publish(); stateVersion++; }
        }
        if (! songRecArmed && perfEditSession == perfSession) {             // recording ended: one undo step
            perfOpen = -1; perfEditSession = -1;
            if (editStart != nullptr && ! recArmed) endEdit ("Record song");
        }
    }
    void pushPerf (int key, double ppq, double offset)
    {
        if (! songRecArmed || songOn()) return;
        int s1, n1, s2, n2;
        perfFifo.prepareToWrite (1, s1, n1, s2, n2);
        const PerfEvent e { key, ppq, offset, wasPlaying };
        if (n1 > 0) perfBuf[(size_t) s1] = e; else if (n2 > 0) perfBuf[(size_t) s2] = e;
        perfFifo.finishedWrite (n1 + n2);
    }
    double nowPpq = 0;
    std::set<int> replacedLanes;

    // ---------------------------------------------------------------- audio
    void processBlock (AudioBuffer<float>& audio, MidiBuffer& midi) override
    {
        audio.clear();
        const int n = audio.getNumSamples();
        blockSize = n;
        out.clear();
        std::shared_ptr<const Song> song;
        { SpinLock::ScopedLockType l (liveLock); song = live; }
        S = song.get();

        // host clock
        double bpm = 120.0, ppq = internalPpq; bool playing = false;
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition()) {
                if (auto b = pos->getBpm()) bpm = jlimit (20.0, 400.0, *b);
                playing = pos->getIsPlaying();
                hostLooping = pos->getIsLooping();
                if (auto lp = pos->getLoopPoints()) { hostLoopA = lp->ppqStart; hostLoopB = lp->ppqEnd; }
                if (playing) if (auto pp = pos->getPpqPosition()) ppq = *pp;
            }
        hostBpm = bpm;
        if (! playing && ! S->settings.followHost) bpm = jlimit (20.0, 400.0, S->settings.ownBpm);   // your own tempo while Studio One is stopped
        const double beatsPerSample = bpm / 60.0 / sampleRate;
        samplesPerBeat = 1.0 / beatsPerSample;
        curBpm = bpm;
        lastBpm = bpm;

        if (playing && ! wasPlaying) {               // transport started: lock running pattern to the bar grid
            if (cur.key >= 0) { const int bpb = S->keys[(size_t) cur.key].beatsPerBar; cur.start = cur.anchor = std::floor (ppq / bpb + 1e-6) * bpb; queued.active = false; }
            flushOffs (out, 0, true);
        } else if (! playing && wasPlaying) {
            if (S->settings.stopWithHost) { stopNow (out, 0); chain = -1; }
            internalPpq = ppq = lastPpqEnd;
        } else if (playing && std::abs (ppq - lastPpqEnd) > 0.02) {   // locate / loop jump
            flushOffs (out, 0, true);
            if (cur.key >= 0 && ppq < cur.start) { const int bpb = S->keys[(size_t) cur.key].beatsPerBar; cur.start = cur.anchor = std::floor (ppq / bpb + 1e-6) * bpb; }
            if (queued.active && queued.ppq < ppq) queued.ppq = ppq;
        }
        wasPlaying = playing; hostPlaying = playing;
        blockPpq = ppq;
        const double ppqEnd = ppq + n * beatsPerSample;

        sendControllers (out);
        lastStagSample = -1000000;

        if (playing && ! wasPlayingSong && S->songMode) { songPlaying = false; stopNow (out, 0); }   // the song follows Studio One
        wasPlayingSong = playing;
        // UI commands land at the start of the block
        int cs1, cn1, cs2, cn2;
        cmdFifo.prepareToRead (cmdFifo.getNumReady(), cs1, cn1, cs2, cn2);
        for (int i = 0; i < cn1 + cn2; ++i) {
            const auto c = cmdBuf[(size_t) (i < cn1 ? cs1 + i : cs2 + i - cn1)];
            handleCommand (c, ppq);
        }
        cmdFifo.finishedRead (cn1 + cn2);

        // MIDI input, in time order, splitting the render at each event
        double pos = ppq;
        for (const auto meta : midi) {
            const auto m = meta.getMessage();
            const double evPpq = ppq + meta.samplePosition * beatsPerSample;
            if (evPpq > pos) { renderTo (pos, evPpq); pos = evPpq; }
            handleInput (m, meta.samplePosition, evPpq);
        }
        if (ppqEnd > pos) renderTo (pos, ppqEnd);
        flushOffs (out, ppqEnd, false);

        // recording that stops by itself after N passes
        if (recState == 2 && cur.key >= 0 && S->settings.recStopAfter > 0) {
            const double len = S->keys[(size_t) cur.key].lengthTicks() / (double) PPQ;
            if (ppqEnd - recStartPpq >= S->settings.recStopAfter * len) recState = 0;
        }
        if (cur.key < 0 && ! (queued.active && recState == 1) && recState != 3) recState = 0;    // 3: waiting for your first note
        metronome (audio, ppq, ppqEnd);
        recArmed = recState != 0; recStateUi = recState;

        midi.swapWith (out);
        lastPpqEnd = ppqEnd;
        nowPpqUi = ppqEnd; nowPpq = ppqEnd;
        if (! playing) internalPpq = ppqEnd;
        const bool songNow = songOn() && cur.key < 0 && ! queued.active;
        playingKey = songNow ? songKeyNow : cur.key;
        if (! songNow) songBeatUi = -1.0;
        queuedKey = queued.active ? queued.key : -2;
        chainPlaying = chain; chainStep = chainIdx;
        if (cur.key >= 0) playRelTicks = (ppqEnd - cur.start) * PPQ;
        S = nullptr;
    }

    // ---------------------------------------------------------------- engine
    struct Play { int key = -1; double start = 0; int returnKey = -1; double anchor = 0; };   // anchor: bar 1 of the running groove's grid
    struct Queue { bool active = false; double ppq = 0; int key = -1; int returnKey = -1; bool fromChain = false; bool force = false; double virtStart = -1; double anchor = -1; };

    // "in time": the new pattern joins at the spot it would be at had it been playing from the grid's bar 1
    double alignedStart (int key, double t, double anchor) const
    {
        const double len = S->keys[(size_t) key].lengthTicks() / (double) PPQ;
        if (len <= 0) return t;
        return anchor + std::floor ((t - anchor) / len + 1e-9) * len;
    }
    bool inTime() const { return S->settings.startQuant == StartQuant::InTime; }
    struct Off { double ppq; int note, ch; };
    enum CmdType { CmdNoteOn = 1, CmdNoteOff, CmdStop, CmdChain, CmdPreview, CmdRecStart, CmdRecStop, CmdSongPlay, CmdSongStop, CmdPlayFrom };
    struct Cmd { int type = 0, value = 0; };
    struct RecNote { int key, tick, note, vel, pass = 0; };

    const Song* S = nullptr;
    Play cur; Queue queued;
    int chain = -1, chainIdx = -1;
    double sampleRate = 44100.0, samplesPerBeat = 22050.0, curBpm = 120.0, blockPpq = 0.0, internalPpq = 0.0, lastPpqEnd = 0.0;
    bool wasPlaying = false;
    MidiBuffer out;
    std::vector<Off> offs;
    int lastVol = -1, lastPan = -1;
    std::atomic<bool> sendLanePans { true };
    bool dirty = false; int saveCounter = 0, recIdle = 0;

    SpinLock liveLock;
    std::shared_ptr<const Song> live;
    std::vector<std::shared_ptr<const Song>> graveyard;

    AbstractFifo cmdFifo { 256 }; std::array<Cmd, 256> cmdBuf;
    AbstractFifo recFifo { 1024 }; std::array<RecNote, 1024> recBuf;

    void pushCmd (int type, int value)
    {
        int s1, n1, s2, n2;
        cmdFifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 > 0) cmdBuf[(size_t) s1] = { type, value }; else if (n2 > 0) cmdBuf[(size_t) s2] = { type, value };
        cmdFifo.finishedWrite (n1 + n2);
    }

    int sampleAt (double p) const { return jlimit (0, jmax (0, blockSamples() - 1), (int) std::floor ((p - blockPpq) * samplesPerBeat)); }
    int blockSamples() const { return blockSize; }
    int blockSize = 512;

    void resetPlayback() { cur = {}; queued = {}; chain = -1; chainIdx = -1; offs.clear(); offs.reserve (512); lastVol = lastPan = -1; sendLanePans = true; }

    void sendControllers (MidiBuffer& o)
    {
        const int ch = S->settings.outChannel;
        const int v = (int) apvts.getRawParameterValue (ids::kitVol)->load();
        const int p = (int) apvts.getRawParameterValue (ids::kitPan)->load();
        if (v != lastVol) { o.addEvent (MidiMessage::controllerEvent (ch, 7, v), 0); lastVol = v; }
        if (p != lastPan) { o.addEvent (MidiMessage::controllerEvent (ch, 10, p), 0); lastPan = p; }
        if (sendLanePans.exchange (false))
            for (auto& l : S->lanes) if (l.channel > 0) o.addEvent (MidiMessage::controllerEvent (l.channel, 10, l.pan), 0);
    }

    void flushOffs (MidiBuffer& o, double upTo, bool all)
    {
        for (size_t i = 0; i < offs.size();) {
            if (all || offs[i].ppq < upTo) {
                o.addEvent (MidiMessage::noteOff (offs[i].ch, offs[i].note), all ? 0 : sampleAt (offs[i].ppq));
                offs[i] = offs.back(); offs.pop_back();
            } else ++i;
        }
    }

    void stopNow (MidiBuffer& o, int)
    {
        if (cur.key >= 0) pushPerf (-1, nowPpq, 0);
        flushOffs (o, 0, true);
        cur = {}; queued = {};
        recState = 0;
    }

    // ---------------------------------------------------------------- recording state and metronome
    int recState = 0, recKey = -1;
    double recStartPpq = 0, countStart = 0, countEnd = 0;
    int clickPos = -1; float clickFreq = 1000.0f, clickAmp = 0.0f;

    void startRecording (int k, double now)
    {
        if (k < 0 || k >= numKeys) return;
        chain = -1;
        recSession++;
        if (cur.key == k) { recState = 2; recKey = k; recStartPpq = now; return; }
        if (S->settings.recWaitForKey) { recState = 3; recKey = k; return; }    // starts with the first note you play
        const auto& p = S->keys[(size_t) k];
        double at = cur.key >= 0 ? boundary (now) : (wasPlaying ? std::ceil (now - 0.02) : now);
        const double count = cur.key >= 0 ? 0.0 : (double) S->settings.countInBars * p.beatsPerBar;
        countStart = at; countEnd = at + count;
        queued = { true, countEnd, k, -1, false, true };
        recState = 1; recKey = k;
    }

    void metronome (AudioBuffer<float>& audio, double p0, double p1)
    {
        const auto& st = S->settings;
        if (st.metroMode == 0) { clickPos = -1; return; }
        double ref = 0; int bpb = 4; bool on = false;
        const bool counting = recState == 1 && queued.active;
        if (counting) { ref = countStart; bpb = S->keys[(size_t) jlimit (0, 127, recKey)].beatsPerBar; on = true; }
        else if (cur.key >= 0) {
            ref = cur.start; bpb = S->keys[(size_t) cur.key].beatsPerBar;
            on = st.metroMode == 3 || (st.metroMode == 2 && recState == 2);
        }
        const int n = audio.getNumSamples();
        int starts[16]; float freqs[16]; int ns = 0;
        if (on)
            for (double b = std::ceil (p0 - 1e-9); b < p1 && ns < 16; b += 1.0) {
                if (counting && (b < countStart - 1e-6 || b >= countEnd - 1e-6)) continue;
                if (! counting && b < ref - 1e-6) continue;
                double m = std::fmod (b - ref, (double) bpb); if (m < 0) m += bpb;
                starts[ns] = jlimit (0, n - 1, (int) std::round ((b - p0) * samplesPerBeat));
                freqs[ns] = (m < 1e-3 || bpb - m < 1e-3) ? 1600.0f : 1000.0f;
                ++ns;
            }
        if (clickPos < 0 && ns == 0) return;
        const int len = (int) (0.035 * sampleRate);
        const float level = st.metroLevel * 0.6f;
        int si = 0;
        for (int i = 0; i < n; ++i) {
            if (si < ns && starts[si] == i) { clickPos = 0; clickFreq = freqs[si]; ++si; }
            if (clickPos < 0) continue;
            const float t = (float) clickPos / (float) sampleRate;
            const float v = std::sin (MathConstants<float>::twoPi * clickFreq * t) * std::exp (-t / 0.007f) * level;
            for (int c = 0; c < audio.getNumChannels(); ++c) audio.addSample (c, i, v);
            if (++clickPos >= len) clickPos = -1;
        }
    }

    double boundary (double now) const
    {
        if (cur.key < 0) return now;
        const auto& p = S->keys[(size_t) cur.key];
        const double bar = p.beatsPerBar, len = p.lengthTicks() / (double) PPQ;
        switch (S->settings.startQuant) {
            case StartQuant::Immediate: return now;
            case StartQuant::Beat: return std::ceil (now - 1e-6);
            case StartQuant::Bar: return cur.start + std::ceil ((now - cur.start) / bar - 1e-6) * bar;
            case StartQuant::PatternEnd: return cur.start + std::ceil ((now - cur.start) / len - 1e-6) * len;
            case StartQuant::InTime: return now;
        }
        return now;
    }

    bool keyHasPattern (int k) const { return k >= 0 && k < numKeys && ! S->keys[(size_t) k].notes.empty(); }

    void triggerKey (int k, double now, bool fromChain = false)
    {
        if (! keyHasPattern (k)) return;
        if (! fromChain && recState == 1) recState = 0;          // a key press cancels a waiting count-in
        if (! fromChain) chain = -1;
        const auto type = S->keys[(size_t) k].type;
        if (! fromChain && k == cur.key && ! queued.active && type == PatternType::Groove) return;   // same key again: keeps playing (only STOP stops)
        int ret = -1;
        if (type == PatternType::Fill || type == PatternType::Intro) {
            ret = cur.key >= 0 ? (S->keys[(size_t) cur.key].type == PatternType::Groove ? cur.key : cur.returnKey) : -1;
            if (queued.active && queued.key >= 0 && S->keys[(size_t) queued.key].type == PatternType::Groove) ret = queued.key;
        }
        double at = now;
        if (cur.key >= 0) at = boundary (now);
        else if (wasPlaying && S->settings.startQuant != StartQuant::Immediate && S->settings.startQuant != StartQuant::InTime) at = std::ceil (now - 0.02);   // host running: next beat
        queued = { true, at, k, ret, fromChain };
        if (inTime() && cur.key >= 0 && ! fromChain) { queued.anchor = cur.anchor; queued.virtStart = alignedStart (k, at, cur.anchor); }
    }

    void startChain (int c, double now)
    {
        if (c < 0 || c >= (int) S->chains.size()) return;
        const auto& ch = S->chains[(size_t) c];
        if (ch.steps.empty()) return;
        chain = c; chainIdx = 0; pendingChainIdx = 0;
        triggerKey (ch.steps[0].key, now, true);
    }

    void handleCommand (const Cmd& c, double now)
    {
        nowPpq = now;
        if (c.type == CmdNoteOn) triggerKey (c.value, now);
        else if (c.type == CmdNoteOff) { if (! S->settings.latch && (c.value == cur.key || (queued.active && c.value == queued.key))) { stopNow (out, 0); chain = -1; } }
        else if (c.type == CmdStop) { stopNow (out, 0); chain = -1; songPlaying = false; }
        else if (c.type == CmdSongPlay) { stopNow (out, 0); chain = -1; songPlaying = true; songOrigin = now - c.value / 100.0; }
        else if (c.type == CmdSongStop) { songPlaying = false; flushOffs (out, 0, true); }
        else if (c.type == CmdPlayFrom) {                       // this key, from this spot in it, straight away
            const int k = c.value >> 20, t = c.value & ((1 << 20) - 1);
            if (k >= 0 && k < numKeys && keyHasPattern (k)) { flushOffs (out, 0, true); chain = -1; queued = {}; cur = { k, now - t / (double) PPQ, -1, now - t / (double) PPQ }; pushPerf (k, now, t / (double) PPQ); }
        }
        else if (c.type == CmdRecStart) startRecording (c.value, now);
        else if (c.type == CmdRecStop) { if (recState == 1) { queued = {}; } recState = 0; }
        else if (c.type == CmdChain) { if (c.value >= 0 && c.value < (int) S->chains.size()) startChain (c.value, now); }
        else if (c.type == CmdPreview) {
            const auto* l = S->lane (c.value);
            const int ch = (l != nullptr && l->channel > 0) ? l->channel : S->settings.outChannel;
            out.addEvent (MidiMessage::noteOn (ch, jlimit (0, 127, c.value), (uint8) 100), 0);
            outHits[(size_t) jlimit (0, 127, c.value)]++;
            offs.push_back ({ now + 0.25, c.value, ch });
        }
    }

    void handleInput (const MidiMessage& m, int sample, double ppq)
    {
        nowPpq = ppq;
        const auto& st = S->settings;
        if (m.isNoteOnOrOff()) {
            const int nn = m.getNoteNumber();
            int key = st.keyForNote (nn, m.getChannel());
            // Pattern mode: an empty key plays its drum, so you can play hits along with the grooves
            if (key >= 0 && ! keyHasPattern (key) && key != cur.key && ! (queued.active && key == queued.key)) key = -1;
            if (m.isNoteOn()) { lastInNote = nn; lastInKey = key; lastInSerial++; }
            if (st.isStop (nn) && st.keyMode != 1 && (st.triggerChannel == 0 || m.getChannel() == st.triggerChannel)) {
                if (m.isNoteOn()) { stopNow (out, sample); chain = -1; lastTrigger = 4; }
                return;
            }
            if (key >= 0) {
                if (m.isNoteOn()) {
                    lastTrigger = ! keyHasPattern (key) ? 2 : 1;
                    triggerKey (key, ppq);
                }
                else if (! st.latch && (key == cur.key || (queued.active && key == queued.key))) { stopNow (out, sample); chain = -1; }
                return;
            }
            if (m.isNoteOn()) inHits[(size_t) nn]++;
            if (m.isNoteOn() && recState == 3 && recKey >= 0) {              // waiting for a key: this note starts the pattern and the recording
                flushOffs (out, sample, true); chain = -1; queued = {};
                cur = { recKey, ppq, -1, ppq }; pushPerf (recKey, ppq, 0.0);
                recState = 2; recStartPpq = ppq; recSession++;
            }
            if (m.isNoteOn() && recState == 2 && cur.key >= 0) {
                const auto& p = S->keys[(size_t) cur.key];
                double rel = std::fmod ((ppq - cur.start) * PPQ, (double) p.lengthTicks()); if (rel < 0) rel += p.lengthTicks();
                int s1, n1, s2, n2;
                recFifo.prepareToWrite (1, s1, n1, s2, n2);
                const double plen = p.lengthTicks() / (double) PPQ;
                const int pass = plen > 0 ? (int) std::floor ((ppq - recStartPpq) / plen + 1e-9) : 0;
                const RecNote r { cur.key, (int) rel, nn, st.recFixedVel > 0 ? st.recFixedVel : (int) m.getVelocity(), jmax (0, pass) };
                if (n1 > 0) recBuf[(size_t) s1] = r; else if (n2 > 0) recBuf[(size_t) s2] = r;
                recFifo.finishedWrite (n1 + n2);
            }
        }
        if (st.passThrough && ! (recState == 2 && st.recTarget == 1 && m.isNoteOnOrOff() && st.keyForNote (m.getNoteNumber(), m.getChannel()) < 0)) out.addEvent (m, sample);
    }

    void applyQueue()
    {
        const auto q = queued; queued = {};
        nowPpq = q.ppq;
        if (q.key < 0) { stopNow (out, sampleAt (q.ppq)); chain = -1; return; }
        cur = { q.key, q.virtStart >= 0 ? q.virtStart : q.ppq, q.returnKey, q.anchor >= 0 ? q.anchor : q.ppq };
        pushPerf (cur.key, q.ppq, q.ppq - cur.start);
        if (q.force && recState == 1 && q.key == recKey) { recState = 2; recStartPpq = q.ppq; }
        const auto& p = S->keys[(size_t) cur.key];
        const double len = p.lengthTicks() / (double) PPQ;
        if (chain >= (int) S->chains.size()) chain = -1;
        if (chain >= 0) {
            const auto& ch = S->chains[(size_t) chain];
            const bool holdHere = chainIdx >= 0 && chainIdx < (int) ch.steps.size() && ch.steps[(size_t) chainIdx].looping;
            const int reps = holdHere ? 1 : (chainIdx >= 0 && chainIdx < (int) ch.steps.size()) ? jmax (1, ch.steps[(size_t) chainIdx].repeats) : 1;
            int next = holdHere ? chainIdx : chainIdx + 1;
            if (next >= (int) ch.steps.size()) next = ch.loop ? 0 : -1;
            if (next >= 0 && keyHasPattern (ch.steps[(size_t) next].key)) { queued = { true, cur.start + reps * len, ch.steps[(size_t) next].key, -1, true }; pendingChainIdx = next; }
            else queued = { true, cur.start + reps * len, -1, -1, true };
            return;
        }
        if (p.type == PatternType::Fill || p.type == PatternType::Intro) {
            const double end = cur.start + len;               // in time, this is the end of the fill's own bar(s), never later
            const int back = keyHasPattern (cur.returnKey) ? cur.returnKey : -1;
            queued = { true, end, back, -1, false };
            if (inTime() && back >= 0) { queued.anchor = cur.anchor; queued.virtStart = alignedStart (back, end, cur.anchor); }   // the groove carries on where it would be
        }
        else if (p.type == PatternType::Ending)
            queued = { true, cur.start + len, -1, -1, false };
    }
    int pendingChainIdx = 0;
    int lastHH = -1;
    // smooth, slow wandering in -1 … 1 (three sine waves at unrelated speeds), the same every time for a song position
    static float slowDrift (double beats, double cycleBeats, int which)
    {
        const double t = beats / jmax (1.0, cycleBeats) * MathConstants<double>::twoPi;
        const double ph = which * 1.7;
        return (float) ((std::sin (t + ph) * 0.55 + std::sin (t * 1.618 + ph * 2.3) * 0.3 + std::sin (t * 0.382 + ph * 0.7) * 0.15));
    }
    int lastStagSample = -1000000;

    // ---------------------------------------------------------------- song lane playback
    bool songPlaying = false, wasPlayingSong = false; double songOrigin = 0;
    bool songOn() const { return S->songMode && (wasPlaying || songPlaying); }
    double songShift() const { return wasPlaying ? 0.0 : songOrigin; }      // ppq → song beats
    void renderSong (double a, double b)
    {
        const double sh = songShift(), sa = a - sh, sb = b - sh;
        int keyNow = -1;
        const int nc = (int) S->clips.size();
        for (int i = 0; i < nc; ++i) {
            const auto& c = S->clips[(size_t) i];
            if (c.muted) continue;
            const double cs = c.start, ce = c.start + c.length;
            if (sb > cs && sa < ce) {
                // the stretch this chunk plays, minus the parts covered by a chunk on top of it
                std::vector<std::pair<double, double>> parts { { jmax (sa, cs), jmin (sb, ce) } };
                for (int j = i + 1; j < nc; ++j) {
                    const auto& o = S->clips[(size_t) j];
                    if (o.muted) continue;
                    std::vector<std::pair<double, double>> next;
                    for (auto [a0, b0] : parts) {
                        if (o.start >= b0 || o.start + o.length <= a0) { next.push_back ({ a0, b0 }); continue; }
                        if (o.start > a0) next.push_back ({ a0, o.start });
                        if (o.start + o.length < b0) next.push_back ({ o.start + o.length, b0 });
                    }
                    parts.swap (next);
                }
                for (auto [ia, ib] : parts) if (ib > ia) renderKey (c.key, sh + cs - c.offset, ia + sh, ib + sh);
            }
            if (sb > cs && sb <= ce) keyNow = c.key;                 // the top one wins here too
        }
        songKeyNow = keyNow;
        songBeatUi = sb;
    }
    int songKeyNow = -1;

    void renderTo (double a, double b)
    {
        if (songOn() && cur.key < 0 && ! queued.active) {              // a pressed key takes over from the song
            flushOffs (out, a, false);
            // the lane's own loop, when it plays by itself (with Studio One playing, Studio One's loop rules)
            if (! wasPlaying && S->loopOn && S->loopEnd > S->loopStart + 0.01) {
                const double le = songOrigin + S->loopEnd;
                if (a < le && b > le) { renderSong (a, le); flushOffs (out, le, true); songOrigin += S->loopEnd - S->loopStart; renderSong (le, b); return; }
            }
            renderSong (a, b); return;
        }
        while (true) {
            flushOffs (out, a, false);
            if (queued.active && queued.ppq < b) {
                const double t = jmax (a, queued.ppq);
                renderPattern (a, t);
                if (chain >= 0 && queued.fromChain && queued.key >= 0 && cur.key >= 0) chainIdx = pendingChainIdx;
                applyQueue();
                a = t;
                if (a >= b) break;
                continue;
            }
            renderPattern (a, b);
            break;
        }
    }

    // editor loops: the key being edited repeats a stretch of itself (loopReq*: set by the editor; -1 = whole pattern)
    int loopKey = -1, loopFrom = 0, loopTo = 0;
    void renderPattern (double a, double b)
    {
        if (cur.key < 0) return;
        const double L = S->keys[(size_t) cur.key].lengthTicks();
        if (L <= 0) { renderKey (cur.key, cur.start, a, b); return; }
        auto active = [&] { return loopKey == cur.key && loopTo > loopFrom && loopTo <= L; };
        for (int guard = 0; guard < 64 && b - a > 1e-9; ++guard) {
            const int rk = loopReqKey.load(), rf = loopReqFrom.load(), rt = loopReqTo.load();
            const bool pending = rk != loopKey || rf != loopFrom || rt != loopTo;
            const double rel = (a - cur.start) * PPQ, cyc = std::floor (rel / L + 1e-9) * L, pos = rel - cyc;
            if (pending && S->settings.loopJumpAtOnce) {                // jump at once
                loopKey = rk; loopFrom = rf; loopTo = rt;
                if (active() && (pos < loopFrom - 1e-6 || pos >= loopTo - 1e-6)) cur.start = a - (cyc + loopFrom) / PPQ;
                continue;
            }
            const bool on = active();
            if (on && (pos < loopFrom - 1e-6 || pos >= loopTo - 1e-6)) { cur.start = a - (cyc + loopFrom) / PPQ; continue; }
            const double endPpq = cur.start + (on ? cyc + loopTo : cyc + L) / PPQ;
            const double e = jmin (b, endPpq);
            renderKey (cur.key, cur.start, a, e);
            a = e;
            if (e >= endPpq - 1e-9) {                                    // this loop (or the pattern) is over
                if (pending) { loopKey = rk; loopFrom = rf; loopTo = rt; }
                if (active()) cur.start = e - ((on ? cyc : cyc + L) + loopFrom) / PPQ;
            }
        }
    }

    // plays one key's pattern as if it started at `start` (ppq), for the stretch a … b
    void renderKey (int key, double start, double a, double b)
    {
        if (key < 0 || b <= a) return;
        const auto& p = S->keys[(size_t) key];
        if (p.notes.empty()) return;
        const double relA = (a - start) * PPQ, relB = (b - start) * PPQ;
        const double ticksPerMs = curBpm / 60.0 * PPQ / 1000.0;
        const double humT = apvts.getRawParameterValue (ids::humTime)->load() * ticksPerMs;
        const double pushT = apvts.getRawParameterValue (ids::push)->load() * ticksPerMs;
        const int humV = (int) apvts.getRawParameterValue (ids::humVel)->load();
        const double swing = apvts.getRawParameterValue (ids::swing)->load();
        const int swingChoice = (int) apvts.getRawParameterValue (ids::swingGrid)->load();
        const float inten = apvts.getRawParameterValue (ids::intensity)->load() / 100.0f;
        const float dyn = apvts.getRawParameterValue (ids::dynamics)->load() / 100.0f;
        const int vres = (int) apvts.getRawParameterValue (ids::vintRes)->load();
        const int vstep = vres == 1 ? PPQ / 96 : vres == 2 ? PPQ / 48 : vres == 3 ? PPQ / 24 : 0;
        const double wob = apvts.getRawParameterValue (ids::vintJitter)->load() * ticksPerMs;
        const bool stag = apvts.getRawParameterValue (ids::stagger)->load() > 0.5f;
        const double driftT = apvts.getRawParameterValue (ids::driftTime)->load() * ticksPerMs;
        const float driftV = apvts.getRawParameterValue (ids::driftVel)->load();
        const double driftLen = jmax (1.0f, apvts.getRawParameterValue (ids::driftBars)->load()) * p.beatsPerBar;
        const float phraseAmt = apvts.getRawParameterValue (ids::phrase)->load() / 100.0f;
        const int hhCC = S->settings.hhCC;
        const double L = p.lengthTicks();
        const double margin = 260.0 + humT + std::abs (pushT) + wob + 40 + apvts.getRawParameterValue (ids::driftTime)->load() * ticksPerMs;
        bool anySolo = false; for (auto& l : S->lanes) anySolo |= l.solo;

        for (size_t i = 0; i < p.notes.size(); ++i) {
            const auto& nt = p.notes[i];
            const int LL = p.laneLen (nt.note);
            if (LL <= 0 || nt.tick >= LL || nt.silent) continue;
            if (p.laneMute.count (nt.note)) continue;
            const Lane* lane = S->lane (nt.note);
            if (anySolo && (lane == nullptr || ! lane->solo)) continue;
            int sw = 0;
            const int laneSwing = lane != nullptr ? lane->swingMode : 0;
            if (laneSwing == 0) sw = swingChoice == 0 ? nt.autoDiv : swingChoiceTicks (swingChoice);
            else if (laneSwing >= 2) sw = laneSwing - 2 == 0 ? nt.autoDiv : swingChoiceTicks (laneSwing - 2);
            const double swOff = laneSwing == 1 ? 0.0 : swingOffset (nt.tick, sw, swing);

            const int k0 = jmax (0, (int) std::floor ((relA - nt.tick - margin) / LL));
            const int k1 = (int) std::floor ((relB - nt.tick + margin) / LL);
            for (int k = k0; k <= k1; ++k) {
                const uint32 salt = (uint32) key * 131u + (uint32) i;
                double T = (double) k * LL + nt.tick + swOff + pushT + hashNoise ((uint32) k, (uint32) i, (uint32) key, 7) * humT * 0.6;
                const double here = start + T / PPQ;                                      // where this hit falls in the song
                if (driftT > 0) T += slowDrift (here, driftLen, 1) * driftT;
                if (vstep > 0) T = std::round (T / vstep) * vstep;                       // coarse vintage clock
                if (wob > 0) T += hashNoise ((uint32) k, (uint32) i, (uint32) key, 19) * wob;   // clock wobble
                if (k == 0 && nt.tick == 0) T = jmax (T, 0.0);
                if (T < relA || T >= relB) continue;
                if (nt.probability < 100 && (hashNoise ((uint32) k, (uint32) i, salt, 3) * 0.5f + 0.5f) * 100.0f >= nt.probability) continue;
                float v = (float) nt.velocity * (lane != nullptr ? lane->level : 1.0f);
                v = 64.0f + (v - 64.0f) * dyn;
                v *= inten;
                if (driftV > 0) v += slowDrift (here, driftLen, 2) * driftV;
                if (phraseAmt > 0) {                                                       // phrase dynamics
                    double pos = std::fmod (T, L) / L; if (pos < 0) pos += 1.0;
                    float g = -0.07f * (float) std::sin (pos * MathConstants<double>::pi) + 0.12f * (float) std::pow (pos, 4.0);
                    if (std::fmod (T, L) < 12.0) g += 0.10f;                                // the one
                    if (p.type == PatternType::Fill) g += 0.22f * (float) pos - 0.08f;       // fills build up
                    v *= 1.0f + phraseAmt * g;
                }
                v += hashNoise ((uint32) k, (uint32) i, salt, 11) * humV;
                const int vel = jlimit (1, 127, (int) std::round (v));
                const int ch = (lane != nullptr && lane->channel > 0) ? lane->channel : S->settings.outChannel;
                const double evPpq = start + T / PPQ;
                int smp = sampleAt (evPpq);
                if (stag) {                     // 5-pin MIDI sends about one note per millisecond
                    if (smp <= lastStagSample) smp = jmin (blockSamples() - 1, lastStagSample + jmax (1, (int) (sampleRate / 1000.0)));
                    lastStagSample = smp;
                }
                for (size_t o = 0; o < offs.size(); ++o)
                    if (offs[o].note == nt.note && offs[o].ch == ch) { out.addEvent (MidiMessage::noteOff (ch, nt.note), smp); offs[o] = offs.back(); offs.pop_back(); break; }
                if (hhCC > 0 && nt.hhOpen >= 0 && nt.hhOpen != lastHH) { out.addEvent (MidiMessage::controllerEvent (ch, hhCC, nt.hhOpen), smp); lastHH = nt.hhOpen; }
                out.addEvent (MidiMessage::noteOn (ch, nt.note, (uint8) vel), smp);
                notesSent++;
                offs.push_back ({ evPpq + jmax (10, nt.length) / (double) PPQ, nt.note, ch });
                lastHitNote = nt.note; lastHitSerial++;
                outVel[(size_t) nt.note] = vel; outHits[(size_t) nt.note]++;
            }
        }
    }

public:
    friend class SongAction;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumPlayerProcessor)
};

inline bool SongAction::perform() { proc.setEditFromUndo (after); return true; }
inline bool SongAction::undo() { proc.setEditFromUndo (before); return true; }

} // namespace dp
