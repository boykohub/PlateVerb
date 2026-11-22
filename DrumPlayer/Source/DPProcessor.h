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
        ui->setAttribute ("zoomX", uiZoomX);
        return x;
    }
    void restoreUi (const XmlElement& x)
    {
        if (auto* ui = x.getChildByName ("UI")) {
            uiScale = (float) jlimit (0.6, 2.0, ui->getDoubleAttribute ("scale", 1.0));
            { const ScopedLock l (uiLock); presetName = ui->getStringAttribute ("preset"); }
            uiGrid = jlimit (0, numNoteValues - 1, ui->getIntAttribute ("grid", 3));
            uiZoomX = (float) jlimit (0.25, 8.0, ui->getDoubleAttribute ("zoomX", 1.0));
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
    float uiScale = 1.0f, uiZoomX = 1.0f; int uiGrid = 3;
    std::atomic<int> stateVersion { 0 };

    void publish()
    {
        auto s = std::make_shared<Song> (edit);
        const auto dm = s->map (s->playMap);
        for (auto& p : s->keys) if (! p.notes.empty()) {
            computeAutoSwing (p);
            for (auto& n : p.notes) n.silent = n.art >= 0 && dm.resolve (n.art) < 0 && dm.artFor (n.note) != n.art;
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
        undo.beginNewTransaction (name);
        undo.perform (new SongAction (*this, edit, std::move (s)));
    }
    // live edits (dragging): remember the start, edit `edit` freely with publish(), then endEdit()
    void beginEdit() { editStart = std::make_unique<Song> (edit); }
    void endEdit (const String& name)
    {
        if (editStart == nullptr) return;
        auto before = std::move (*editStart); editStart.reset();
        undo.beginNewTransaction (name);
        undo.perform (new SongAction (*this, std::move (before), edit));
    }
    void setEditFromUndo (const Song& s) { edit = s; publish(); stateVersion++; }

    // ---------------------------------------------------------------- UI → engine commands
    void uiTrigger (int key) { pushCmd (CmdNoteOn, key); }
    void uiRelease (int key) { pushCmd (CmdNoteOff, key); }
    void uiStop() { pushCmd (CmdStop, 0); }
    void uiPlayChain (int c) { pushCmd (CmdChain, c); }
    void uiPreview (int note) { pushCmd (CmdPreview, note); }
    void uiRecord (bool start, int key) { pushCmd (start ? CmdRecStart : CmdRecStop, key); }
    void uiSongPlay (bool start, double fromBeat) { pushCmd (start ? CmdSongPlay : CmdSongStop, (int) std::round (fromBeat * 100)); }
    std::atomic<double> songBeatUi { -1.0 };          // song lane playhead (beats), -1 = not playing
    std::atomic<bool> recArmed { false };            // true while counting in or recording
    std::atomic<int> recStateUi { 0 };               // 0 off, 1 counting in, 2 recording
    std::atomic<int> recSession { 0 };
    std::array<std::atomic<uint32>, 128> outHits {}, inHits {};   // per note, for the lane lights
    std::atomic<int> lastInNote { -1 }, lastInKey { -1 }, lastInSerial { 0 };   // the last key pressed and the pattern key it hit

    // ---------------------------------------------------------------- display (read by the editor)
    std::atomic<int> playingKey { -1 }, queuedKey { -2 }, chainPlaying { -1 }, chainStep { -1 };
    std::atomic<double> playRelTicks { 0.0 };
    std::atomic<double> lastBpm { 120.0 };
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
        if (dirty && ++saveCounter > 60) { saveCounter = 0; dirty = false; saveLastSession(); }
    }
    int lastRecSession = -1;
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
                if (playing) if (auto pp = pos->getPpqPosition()) ppq = *pp;
            }
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
        if (cur.key < 0 && ! (queued.active && recState == 1)) recState = 0;
        metronome (audio, ppq, ppqEnd);
        recArmed = recState != 0; recStateUi = recState;

        midi.swapWith (out);
        lastPpqEnd = ppqEnd;
        if (! playing) internalPpq = ppqEnd;
        playingKey = songOn() ? songKeyNow : cur.key;
        if (! songOn()) songBeatUi = -1.0;
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
    enum CmdType { CmdNoteOn = 1, CmdNoteOff, CmdStop, CmdChain, CmdPreview, CmdRecStart, CmdRecStop, CmdSongPlay, CmdSongStop };
    struct Cmd { int type = 0, value = 0; };
    struct RecNote { int key, tick, note, vel; };

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
        if (S->settings.latch && ! fromChain && k == cur.key && ! queued.active && type == PatternType::Groove) {   // same key again: stop
            queued = { true, boundary (now), -1, -1, false };
            return;
        }
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
        if (c.type == CmdNoteOn) triggerKey (c.value, now);
        else if (c.type == CmdNoteOff) { if (! S->settings.latch && (c.value == cur.key || (queued.active && c.value == queued.key))) { stopNow (out, 0); chain = -1; } }
        else if (c.type == CmdStop) { stopNow (out, 0); chain = -1; songPlaying = false; }
        else if (c.type == CmdSongPlay) { stopNow (out, 0); chain = -1; songPlaying = true; songOrigin = now - c.value / 100.0; }
        else if (c.type == CmdSongStop) { songPlaying = false; flushOffs (out, 0, true); }
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
        const auto& st = S->settings;
        if (m.isNoteOnOrOff()) {
            const int nn = m.getNoteNumber();
            for (int c = 0; c < (int) S->chains.size(); ++c)
                if (S->chains[(size_t) c].triggerNote == nn) { if (m.isNoteOn()) startChain (c, ppq); return; }
            const int key = st.keyForNote (nn, m.getChannel());
            const int stop = st.resolveStop();
            if (m.isNoteOn()) { lastInNote = nn; lastInKey = key; lastInSerial++; }
            if (stop >= 0 && (key == stop || (key < 0 && nn == stop && st.keyForNote (nn, m.getChannel()) < 0 && stop > st.zoneHigh))) {
                if (m.isNoteOn()) { stopNow (out, sample); chain = -1; }
                return;
            }
            if (key >= 0) {
                if (m.isNoteOn()) triggerKey (key, ppq);
                else if (! st.latch && (key == cur.key || (queued.active && key == queued.key))) { stopNow (out, sample); chain = -1; }
                return;
            }
            if (m.isNoteOn()) inHits[(size_t) nn]++;
            if (m.isNoteOn() && recState == 2 && cur.key >= 0) {
                const auto& p = S->keys[(size_t) cur.key];
                double rel = std::fmod ((ppq - cur.start) * PPQ, (double) p.lengthTicks()); if (rel < 0) rel += p.lengthTicks();
                int s1, n1, s2, n2;
                recFifo.prepareToWrite (1, s1, n1, s2, n2);
                const RecNote r { cur.key, (int) rel, nn, st.recFixedVel > 0 ? st.recFixedVel : (int) m.getVelocity() };
                if (n1 > 0) recBuf[(size_t) s1] = r; else if (n2 > 0) recBuf[(size_t) s2] = r;
                recFifo.finishedWrite (n1 + n2);
            }
        }
        if (st.passThrough) out.addEvent (m, sample);
    }

    void applyQueue()
    {
        const auto q = queued; queued = {};
        if (q.key < 0) { stopNow (out, sampleAt (q.ppq)); chain = -1; return; }
        cur = { q.key, q.virtStart >= 0 ? q.virtStart : q.ppq, q.returnKey, q.anchor >= 0 ? q.anchor : q.ppq };
        if (q.force && recState == 1 && q.key == recKey) { recState = 2; recStartPpq = q.ppq; }
        const auto& p = S->keys[(size_t) cur.key];
        const double len = p.lengthTicks() / (double) PPQ;
        if (chain >= (int) S->chains.size()) chain = -1;
        if (chain >= 0) {
            const auto& ch = S->chains[(size_t) chain];
            const int reps = (chainIdx >= 0 && chainIdx < (int) ch.steps.size()) ? jmax (1, ch.steps[(size_t) chainIdx].repeats) : 1;
            int next = chainIdx + 1;
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
    int lastStagSample = -1000000;

    // ---------------------------------------------------------------- song lane playback
    bool songPlaying = false, wasPlayingSong = false; double songOrigin = 0;
    bool songOn() const { return S->songMode && (wasPlaying || songPlaying); }
    double songShift() const { return wasPlaying ? 0.0 : songOrigin; }      // ppq → song beats
    void renderSong (double a, double b)
    {
        const double sh = songShift(), sa = a - sh, sb = b - sh;
        int keyNow = -1;
        for (auto& c : S->clips) {
            if (c.muted) continue;
            const double cs = c.start, ce = c.start + c.length;
            if (sb > cs && sa < ce) {
                const double ia = jmax (sa, cs), ib = jmin (sb, ce);
                if (ib > ia) renderKey (c.key, sh + cs - c.offset, ia + sh, ib + sh);
            }
            if (sb > cs && sb <= ce) keyNow = c.key;
        }
        songKeyNow = keyNow;
        songBeatUi = sb;
    }
    int songKeyNow = -1;

    void renderTo (double a, double b)
    {
        if (songOn()) { flushOffs (out, a, false); renderSong (a, b); return; }
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

    void renderPattern (double a, double b) { if (cur.key >= 0) renderKey (cur.key, cur.start, a, b); }

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
        const double margin = 260.0 + humT + std::abs (pushT) + wob + 40;
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
                if (vstep > 0) T = std::round (T / vstep) * vstep;                       // coarse vintage clock
                if (wob > 0) T += hashNoise ((uint32) k, (uint32) i, (uint32) key, 19) * wob;   // clock wobble
                if (k == 0 && nt.tick == 0) T = jmax (T, 0.0);
                if (T < relA || T >= relB) continue;
                if (nt.probability < 100 && (hashNoise ((uint32) k, (uint32) i, salt, 3) * 0.5f + 0.5f) * 100.0f >= nt.probability) continue;
                float v = (float) nt.velocity * (lane != nullptr ? lane->level : 1.0f);
                v = 64.0f + (v - 64.0f) * dyn;
                v *= inten;
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
                out.addEvent (MidiMessage::noteOn (ch, nt.note, (uint8) vel), smp);
                offs.push_back ({ evPpq + jmax (10, nt.length) / (double) PPQ, nt.note, ch });
                lastHitNote = nt.note; lastHitSerial++;
                outHits[(size_t) nt.note]++;
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
