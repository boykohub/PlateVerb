// Drum Player: data model, MIDI file tools and the editing algorithms (quantize, swing, humanize, randomize).
// Everything here runs on the message thread except the read-only helpers the engine uses on a published copy.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <map>
#include <set>
#include <random>
#include <climits>
#include "DPDrumMaps.h"

namespace dp {
using namespace juce;

constexpr int PPQ = 960;                 // internal ticks per quarter note
constexpr int maxBars = 256;           // any length the user sets (the old 4-bar limit is gone)
constexpr int numKeys = 128;

// ------------------------------------------------------------------ note values
struct NoteValue { const char* name; int ticks; bool triplet; };
// ordered coarse → fine; the order is used to find the coarsest value a note sits on
static const NoteValue noteValues[] = {
    { "1/4", 960, false }, { "1/8", 480, false }, { "1/8T", 320, false }, { "1/16", 240, false },
    { "1/16T", 160, false }, { "1/32", 120, false }, { "1/32T", 80, false }, { "1/64", 60, false },
    { "1/64T", 40, false }, { "1/128", 30, false }, { "1/128T", 20, false }
};
constexpr int numNoteValues = (int) (sizeof (noteValues) / sizeof (noteValues[0]));

inline int valueIndexForTicks (int ticks)
{
    for (int i = 0; i < numNoteValues; ++i) if (noteValues[i].ticks == ticks) return i;
    return 3;
}

// coarsest note value a tick position lies on (within tol ticks); -1 = off every grid (free timing)
inline int detectValue (int tick, int tol = 4)
{
    for (int i = 0; i < numNoteValues; ++i) {
        const int d = noteValues[i].ticks;
        int m = tick % d; if (m < 0) m += d;
        if (m <= tol || d - m <= tol) return i;
    }
    return -1;
}

// ------------------------------------------------------------------ General MIDI drum names
inline String gmDrumName (int n)
{
    static const char* names[] = {
        "Acoustic Bass Drum", "Kick", "Side Stick", "Snare", "Hand Clap", "Electric Snare", "Low Floor Tom",
        "Closed Hi-Hat", "High Floor Tom", "Pedal Hi-Hat", "Low Tom", "Open Hi-Hat", "Low-Mid Tom", "Hi-Mid Tom",
        "Crash 1", "High Tom", "Ride 1", "China", "Ride Bell", "Tambourine", "Splash", "Cowbell", "Crash 2",
        "Vibraslap", "Ride 2", "Hi Bongo", "Low Bongo", "Mute Hi Conga", "Open Hi Conga", "Low Conga",
        "High Timbale", "Low Timbale", "High Agogo", "Low Agogo", "Cabasa", "Maracas", "Short Whistle",
        "Long Whistle", "Short Guiro", "Long Guiro", "Claves", "Hi Wood Block", "Low Wood Block",
        "Mute Cuica", "Open Cuica", "Mute Triangle", "Open Triangle" };
    if (n >= 35 && n <= 81) return names[n - 35];
    return "Note " + String (n);
}

// Studio One names middle C (60) "C3"
inline String keyName (int n)
{
    static const char* nm[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return String (nm[((n % 12) + 12) % 12]) + String (n / 12 - 2);
}
inline bool isBlackKey (int n) { const int k = ((n % 12) + 12) % 12; return k == 1 || k == 3 || k == 6 || k == 8 || k == 10; }

// ------------------------------------------------------------------ model
struct Note
{
    int tick = 0, length = 60, note = 36, velocity = 100, probability = 100;
    bool selected = false;                              // editor only, never saved
    int autoDiv = 0;                                    // filled on publish: swing value for "Auto"
    int art = -1;                                       // drum articulation, kept so map changes can be undone exactly
    bool silent = false;                                // filled on publish: this plugin has no such sound
    bool operator< (const Note& o) const { return tick != o.tick ? tick < o.tick : note < o.note; }
};

enum class PatternType { Groove = 0, Fill, Intro, Ending };
inline const char* typeName (PatternType t) { static const char* n[] = { "Groove", "Fill", "Intro", "Ending" }; return n[(int) t]; }

struct Pattern
{
    String name;
    PatternType type = PatternType::Groove;
    int group = -1;                      // your own group (Intro, Verse...), shown as the key's colour
    int bars = 1, beatsPerBar = 4;
    std::vector<Note> notes;
    std::map<int, int> laneLength;       // note → loop length in ticks (polyrhythm); absent = pattern length
    std::set<int> laneMute;

    bool isEmpty() const { return notes.empty() && name.isEmpty(); }
    int lengthTicks() const { return bars * beatsPerBar * PPQ; }
    int laneLen (int note) const
    {
        auto it = laneLength.find (note);
        return (it == laneLength.end() || it->second <= 0) ? lengthTicks() : jmin (it->second, lengthTicks() * 4);
    }
    void sort() { std::stable_sort (notes.begin(), notes.end()); }
};

struct Lane
{
    int note = 36;
    String name;
    float level = 1.0f;          // velocity scale 0..1.5
    int pan = 64;                // CC10 on the lane's channel (only used when the lane has its own channel)
    int channel = 0;             // 0 = main output channel
    int swingMode = 0;           // 0 follow, 1 straight, 2+ = fixed value index+2 (swingChoices)
    bool solo = false;
};

struct ChainStep { int key = -1, repeats = 1; };
struct Group { String name; uint32 colour = 0xff7fd1ff; };          // your own designations, each with a colour
inline std::vector<Group> defaultGroups()
{
    return { { "Intro", 0xff5aa9ff }, { "Verse", 0xffb8f28b }, { "Pre-chorus", 0xff6ee7d8 }, { "Chorus", 0xffffc15e },
             { "Bridge", 0xffc9a7ff }, { "Fill", 0xffffa36e }, { "Breakdown", 0xffe3e37a }, { "Outro", 0xffff8fa3 } };
}
// a chunk on the song lane: plays a key's pattern from `start` for `length` beats, `offset` beats into it (it repeats if longer)
struct Clip { double start = 0, length = 4, offset = 0; int key = 12; bool muted = false, selected = false; };
struct Chain { String name; std::vector<ChainStep> steps; int triggerNote = -1; bool loop = true; };

enum class StartQuant { Immediate = 0, Beat, Bar, PatternEnd, InTime };   // InTime: switch at once, keeping the bar position

struct Settings
{
    int outChannel = 10;
    int zoneLow = 12, zoneHigh = 127;              // pattern keys: C-1 (12) up the whole keyboard (Studio One names)
    int keyMode = 0;
    int channelBeforeStrike = 10;                               // 0 = Pattern mode (keys play patterns), 1 = Play mode (keys play drums)
    StartQuant startQuant = StartQuant::InTime;
    bool latch = true;                             // false = hold (release stops)
    bool stopWithHost = true;
    bool passThrough = true;
    bool fillsToBlack = false;                     // multi-file import: names containing "fill" go to black keys
    bool rememberLast = true;
    int stopNote = 11;                             // B-2, just below the patterns; any key under C-1 (-1 none, -2 the last pattern key)
    int triggerChannel = 0;                        // pattern keys listen on this channel (0 = any)
    int playChannel = 0;                           // drum pads / live notes come on this channel (0 = any)
    bool mirrorOctaves = false;                    // pattern keys repeat in every octave
    // recording
    int countInBars = 1;                           // 0, 1, 2
    int metroMode = 2;                             // 0 off, 1 count-in only, 2 while recording, 3 whenever playing
    float metroLevel = 0.5f;
    int recMode = 0;                               // 0 overdub, 1 replace (a lane's old notes go when you play it)
    int recStopAfter = 0;                          // passes, 0 = until stopped
    int recFixedVel = 0;                           // 0 = as played

    int resolveStop() const { return stopNote == -2 ? zoneHigh : stopNote; }
    // the pattern key an incoming note triggers, or -1 (stop key included)
    int keyForNote (int n, int ch) const
    {
        if (keyMode == 1) return -1;                   // Play mode: every key plays drums
        if (triggerChannel != 0 && ch != triggerChannel) return -1;
        if (playChannel != 0 && playChannel != triggerChannel && ch == playChannel) return -1;
        if (n >= zoneLow && n <= zoneHigh) return n;
        if (! mirrorOctaves) return -1;
        const int width = zoneHigh - zoneLow + 1;
        if (width % 12 == 0) {                         // whole octaves: the zone repeats up the keyboard, C plays the C pattern
            int k = zoneLow + ((n - zoneLow) % width + width) % width;
            return k;
        }
        int k = n;
        while (k > zoneHigh) k -= 12;
        while (k < zoneLow) k += 12;
        return (k >= zoneLow && k <= zoneHigh) ? k : -1;
    }
    int recQuantize = 0;                           // 0 off, else note value index + 1
    int splitBars = 4;                             // dropped grooves are cut every N bars (0 = keep whole)
};

struct Song
{
    int playMap = mapGM;                           // the drum plugin Drum Player feeds
    int importMap = -1;                            // where dropped MIDI comes from (-1 = same as playMap)
    std::array<int, aCount> customNotes {};        // Custom map: note per articulation (-1 = none)
    std::array<Pattern, numKeys> keys;
    std::vector<Clip> clips;                       // the song lane
    std::vector<Group> groups = defaultGroups();
    bool songMode = false;                         // the song lane plays (with Studio One's transport, or its own Play)
    std::vector<Lane> lanes;
    std::vector<Chain> chains { Chain { "Chain 1", {}, -1, true } };      // as many as you like
    std::array<Pattern, 8> park;
    Settings settings;
    int editKey = 12;

    const Lane* lane (int note) const { for (auto& l : lanes) if (l.note == note) return &l; return nullptr; }
    Lane* lane (int note) { for (auto& l : lanes) if (l.note == note) return &l; return nullptr; }

    DrumMap map (int id) const
    {
        if (id < 0) id = playMap;
        id = jlimit (0, (int) numMaps - 1, id);
        if (id != mapCustom) return builtInMaps()[(size_t) id];
        DrumMap m = builtInMaps()[mapCustom];
        m.notes.clear();
        for (int a = 0; a < aCount; ++a) if (customNotes[(size_t) a] >= 0) m.notes.push_back ({ customNotes[(size_t) a], a });
        return m;
    }
    String laneName (int note) const
    {
        const auto m = map (playMap);
        const int a = m.artFor (note);
        return a >= 0 ? String (artName (a)) : gmDrumName (note);
    }
    static std::array<int, aCount> defaultCustom()
    {
        std::array<int, aCount> c; c.fill (-1);
        for (auto& [n, a] : gmNotes()) if (c[(size_t) a] < 0) c[(size_t) a] = n;
        return c;
    }
    void ensureLane (int note)
    {
        if (lane (note) != nullptr) return;
        Lane l; l.note = note; l.name = laneName (note);
        lanes.push_back (l);
        sortLanes();
    }
    void sortLanes() { std::sort (lanes.begin(), lanes.end(), [] (const Lane& a, const Lane& b) { return a.note > b.note; }); }
    void ensureLanesForPattern (const Pattern& p) { for (auto& n : p.notes) ensureLane (n.note); }

    static Song makeDefault()
    {
        Song s;
        s.customNotes = defaultCustom();
        for (int n : { 36, 37, 38, 39, 40, 42, 44, 46, 41, 43, 45, 47, 48, 50, 49, 57, 51, 53, 55, 52, 54, 56 }) s.ensureLane (n);
        // a starter groove and fill so the plugin makes a sound straight away
        auto& g = s.keys[12]; g.name = "Basic Rock"; g.bars = 1;
        for (int i = 0; i < 8; ++i) g.notes.push_back ({ i * 480, 60, 42, i % 2 == 0 ? 100 : 72 });
        g.notes.push_back ({ 0, 60, 36, 112 }); g.notes.push_back ({ 1440, 60, 36, 104 }); g.notes.push_back ({ 1920, 60, 36, 96 });
        g.notes.push_back ({ 960, 60, 38, 110 }); g.notes.push_back ({ 2880, 60, 38, 112 });
        g.sort();
        auto& f = s.keys[13]; f.name = "Basic Fill"; f.type = PatternType::Fill;
        for (int i = 0; i < 8; ++i) f.notes.push_back ({ 1920 + i * 240, 60, i < 4 ? 38 : 45, 80 + i * 5 });
        for (int i = 0; i < 4; ++i) f.notes.push_back ({ i * 480, 60, 42, 96 });
        f.notes.push_back ({ 0, 60, 36, 110 }); f.notes.push_back ({ 960, 60, 38, 108 });
        f.sort();
        return s;
    }
};

// ------------------------------------------------------------------ swing
// swing amount 50 … 75 (%): position of the off-beat note inside a pair of values
inline const char* swingChoiceNames() { return "Auto|1/4|1/8|1/16|1/32|1/64|1/128"; }
inline int swingChoiceTicks (int choice) { static const int t[] = { 0, 960, 480, 240, 120, 60, 30 }; return t[jlimit (0, 6, choice)]; }

// offset in ticks for a note at tick, swinging pairs of value d
inline double swingOffset (int tick, int d, double swingPct)
{
    if (d <= 0 || swingPct <= 50.0001) return 0.0;
    int m = tick % (2 * d); if (m < 0) m += 2 * d;
    const int tol = jmax (2, d / 12);
    if (std::abs (m - d) <= tol) return (swingPct - 50.0) / 50.0 * d;
    return 0.0;
}

// For "Auto": per lane and per beat, the finest straight value used there (so a 16th groove swings 16ths
// even on its 8th-note positions). Written into Note::autoDiv.
inline void computeAutoSwing (Pattern& p)
{
    std::map<std::pair<int, int>, int> finest;      // (note, beat) → ticks
    for (auto& n : p.notes) {
        const int v = detectValue (n.tick);
        int d = 480;
        if (v >= 0 && ! noteValues[v].triplet) d = jmin (480, noteValues[v].ticks);
        if (v >= 0 && noteValues[v].triplet) d = 0;
        auto key = std::make_pair (n.note, n.tick / PPQ);
        auto it = finest.find (key);
        if (it == finest.end()) finest[key] = d; else if (d > 0 && (it->second == 0 || d < it->second)) it->second = d;
    }
    for (auto& n : p.notes) {
        int d = finest[std::make_pair (n.note, n.tick / PPQ)];
        // a note on the off-beat of a value coarser than d is still swung with d only if it sits on d's off-beat
        n.autoDiv = d;
    }
}

// ------------------------------------------------------------------ quantize
enum class QuantMode { Normal = 0, Catch, Dual };
struct QuantOptions
{
    int valueIndex = 3;            // grid
    QuantMode mode = QuantMode::Normal;
    double catchPct = 25.0;        // Catch: only notes within this % of a step from the grid move
    double strength = 100.0;
    bool keepSwing = false;
    double swingPct = 50.0;
    bool selectedOnly = true;
};

inline void quantize (Pattern& p, const QuantOptions& o)
{
    const NoteValue nv = noteValues[jlimit (0, numNoteValues - 1, o.valueIndex)];
    const int g = nv.ticks;
    const int tg = nv.triplet ? g * 3 / 2 : g * 2 / 3;        // the partner grid for Dual
    bool anySel = false; for (auto& n : p.notes) anySel |= n.selected;
    auto target = [&] (int t, int grid, bool swingIt) {
        const double k = std::round ((double) t / grid);
        double pos = k * grid;
        if (swingIt && o.keepSwing) pos += swingOffset ((int) pos, grid, o.swingPct);
        return pos;
    };
    for (auto& n : p.notes) {
        if (o.selectedOnly && anySel && ! n.selected) continue;
        double best = target (n.tick, g, ! nv.triplet);
        if (o.mode == QuantMode::Dual) {
            const double alt = target (n.tick, tg, nv.triplet);
            if (std::abs (alt - n.tick) < std::abs (best - n.tick)) best = alt;
        }
        if (o.mode == QuantMode::Catch && std::abs (best - n.tick) > o.catchPct / 100.0 * g) continue;
        n.tick = (int) std::round (n.tick + (best - n.tick) * o.strength / 100.0);
        n.tick = jlimit (0, p.lengthTicks() * 4 - 1, n.tick);
    }
    p.sort();
}

// ------------------------------------------------------------------ humanize / randomize (destructive, on selection or all)
inline std::vector<Note*> targetNotes (Pattern& p)
{
    std::vector<Note*> v; bool anySel = false;
    for (auto& n : p.notes) anySel |= n.selected;
    for (auto& n : p.notes) if (! anySel || n.selected) v.push_back (&n);
    return v;
}

inline void humanizeBake (Pattern& p, int timingTicks, int velRange, std::mt19937& rng)
{
    std::normal_distribution<double> nd (0.0, 0.45);
    for (auto* n : targetNotes (p)) {
        n->tick = jmax (0, n->tick + (int) std::round (jlimit (-1.0, 1.0, nd (rng)) * timingTicks));
        n->velocity = jlimit (1, 127, n->velocity + (int) std::round (jlimit (-1.0, 1.0, nd (rng)) * velRange));
    }
    p.sort();
}

inline void randomizeVelocity (Pattern& p, int lo, int hi, std::mt19937& rng)
{
    std::uniform_int_distribution<int> d (jmin (lo, hi), jmax (lo, hi));
    for (auto* n : targetNotes (p)) n->velocity = jlimit (1, 127, d (rng));
}
inline void randomizeTiming (Pattern& p, int ticks, std::mt19937& rng)
{
    std::uniform_int_distribution<int> d (-ticks, ticks);
    for (auto* n : targetNotes (p)) n->tick = jmax (0, n->tick + d (rng));
    p.sort();
}
inline void randomizeProbability (Pattern& p, int lo, int hi, std::mt19937& rng)
{
    std::uniform_int_distribution<int> d (jmin (lo, hi), jmax (lo, hi));
    for (auto* n : targetNotes (p)) n->probability = jlimit (0, 100, d (rng));
}
inline void thinOut (Pattern& p, int pct, std::mt19937& rng)
{
    std::uniform_int_distribution<int> d (0, 99);
    bool anySel = false; for (auto& n : p.notes) anySel |= n.selected;
    p.notes.erase (std::remove_if (p.notes.begin(), p.notes.end(), [&] (const Note& n) {
        return (! anySel || n.selected) && d (rng) < pct; }), p.notes.end());
}
// add quiet ghost notes on free grid steps of the lanes that already play
inline void addGhosts (Pattern& p, int valueIndex, int pct, std::mt19937& rng)
{
    const int g = noteValues[jlimit (0, numNoteValues - 1, valueIndex)].ticks;
    std::set<int> lanes; bool anySel = false;
    for (auto& n : p.notes) anySel |= n.selected;
    for (auto& n : p.notes) if (! anySel || n.selected) lanes.insert (n.note);
    std::uniform_int_distribution<int> d (0, 99), v (18, 42);
    std::vector<Note> add;
    for (int ln : lanes) {
        const int len = p.laneLen (ln);
        for (int t = 0; t < len; t += g) {
            bool used = false;
            for (auto& n : p.notes) if (n.note == ln && std::abs (n.tick - t) < g / 2) { used = true; break; }
            if (! used && d (rng) < pct) add.push_back ({ t, jmin (60, g), ln, v (rng) });
        }
    }
    for (auto& n : add) { n.selected = true; p.notes.push_back (n); }
    p.sort();
}


// ------------------------------------------------------------------ DAW-style edit functions
// split a pattern at tick (snapped by the caller): left keeps [0,tick), right gets [tick,len) moved to 0
inline std::pair<Pattern, Pattern> splitPattern (const Pattern& p, int tick)
{
    Pattern a = p, b = p;
    a.notes.clear(); b.notes.clear(); a.laneLength.clear(); b.laneLength.clear();
    const int bar = p.beatsPerBar * PPQ;
    const int len = p.lengthTicks();
    tick = jlimit (1, len - 1, tick);
    for (auto n : p.notes) { if (n.tick < tick) a.notes.push_back (n); else if (n.tick < len) { n.tick -= tick; b.notes.push_back (n); } }
    // lengths: whole beats, at least one beat (a pattern is made of beats)
    auto toBeats = [] (int t) { return jmax (1, (t + PPQ / 2) / PPQ); };
    const int beatsA = toBeats (tick), beatsB = toBeats (len - tick);
    auto fit = [bar] (Pattern& q, int beats) {
        if (beats % q.beatsPerBar == 0) q.bars = jlimit (1, maxBars, beats / q.beatsPerBar);
        else { q.bars = 1; q.beatsPerBar = jlimit (1, 16, beats); }
        ignoreUnused (bar);
    };
    fit (a, beatsA); fit (b, beatsB);
    b.name = p.name + " (split)";
    return { a, b };
}

inline void selectAll (Pattern& p, bool on) { for (auto& n : p.notes) n.selected = on; }
inline bool anySelected (const Pattern& p) { for (auto& n : p.notes) if (n.selected) return true; return false; }
inline void deleteSelected (Pattern& p) { p.notes.erase (std::remove_if (p.notes.begin(), p.notes.end(), [] (const Note& n) { return n.selected; }), p.notes.end()); }

// duplicate the selection right after itself (whole beats)
inline void duplicateSelection (Pattern& p)
{
    int lo = INT_MAX, hi = 0;
    for (auto& n : p.notes) if (n.selected) { lo = jmin (lo, n.tick); hi = jmax (hi, n.tick + 1); }
    if (lo == INT_MAX) return;
    const int span = jmax (PPQ / 4, ((hi - lo) + PPQ / 4 - 1) / (PPQ / 4) * (PPQ / 4));
    std::vector<Note> add;
    for (auto& n : p.notes) if (n.selected) { auto c = n; c.tick += span; if (c.tick < p.lengthTicks()) add.push_back (c); }
    for (auto& n : p.notes) n.selected = false;
    for (auto& n : add) p.notes.push_back (n);
    p.sort();
}

inline void reverseNotes (Pattern& p)
{
    auto tn = targetNotes (p);
    if (tn.empty()) return;
    int lo = INT_MAX, hi = 0;
    for (auto* n : tn) { lo = jmin (lo, n->tick); hi = jmax (hi, n->tick); }
    for (auto* n : tn) n->tick = lo + hi - n->tick;
    p.sort();
}

// speed: 2 = double speed (half length), 0.5 = half speed
inline void scaleTime (Pattern& p, double f)
{
    auto tn = targetNotes (p);
    int lo = INT_MAX; for (auto* n : tn) lo = jmin (lo, n->tick);
    if (lo == INT_MAX) return;
    for (auto* n : tn) { n->tick = lo + (int) std::round ((n->tick - lo) / f); n->length = jmax (10, (int) std::round (n->length / f)); }
    p.notes.erase (std::remove_if (p.notes.begin(), p.notes.end(), [&] (const Note& n) { return n.tick >= p.lengthTicks() * 4; }), p.notes.end());
    p.sort();
}

inline void velocityFixed (Pattern& p, int v) { for (auto* n : targetNotes (p)) n->velocity = jlimit (1, 127, v); }
inline void velocityAdd (Pattern& p, int d) { for (auto* n : targetNotes (p)) n->velocity = jlimit (1, 127, n->velocity + d); }
// compress (<100) or expand (>100) around the average
inline void velocityScale (Pattern& p, int pct)
{
    auto tn = targetNotes (p); if (tn.empty()) return;
    double avg = 0; for (auto* n : tn) avg += n->velocity; avg /= (double) tn.size();
    for (auto* n : tn) n->velocity = jlimit (1, 127, (int) std::round (avg + (n->velocity - avg) * pct / 100.0));
}
// ramp across the selected notes in time order: from v1 to v2, curve −1 (fast start) … 0 (line) … +1 (slow start)
inline void velocityRamp (Pattern& p, int v1, int v2, double curve)
{
    auto tn = targetNotes (p); if (tn.size() < 2) return;
    int lo = INT_MAX, hi = 0; for (auto* n : tn) { lo = jmin (lo, n->tick); hi = jmax (hi, n->tick); }
    for (auto* n : tn) {
        double x = hi > lo ? (double) (n->tick - lo) / (hi - lo) : 0.0;
        x = curve >= 0 ? std::pow (x, 1.0 + curve * 2.0) : 1.0 - std::pow (1.0 - x, 1.0 - curve * 2.0);
        n->velocity = jlimit (1, 127, (int) std::round (v1 + (v2 - v1) * x));
    }
}
inline void moveToLane (Pattern& p, int note) { for (auto* n : targetNotes (p)) n->note = jlimit (0, 127, note); p.sort(); }
inline void nudge (Pattern& p, int ticks) { for (auto* n : targetNotes (p)) n->tick = jmax (0, n->tick + ticks); p.sort(); }
inline void setLengths (Pattern& p, int ticks) { for (auto* n : targetNotes (p)) n->length = jmax (10, ticks); }
// remove notes that sit on top of each other in the same lane
inline void removeDoubles (Pattern& p)
{
    p.sort();
    std::vector<Note> keep;
    for (auto& n : p.notes) {
        bool dup = false;
        for (auto& k : keep) if (k.note == n.note && std::abs (k.tick - n.tick) < 10) { k.velocity = jmax (k.velocity, n.velocity); dup = true; break; }
        if (! dup) keep.push_back (n);
    }
    p.notes = keep;
}


// ------------------------------------------------------------------ drum map conversion
inline void convertPattern (Pattern& p, const DrumMap& from, const DrumMap& to)
{
    MapConverter c { from, to };
    std::map<int, int> laneMap;                       // old lane note → new note (for lane lengths and mutes)
    for (auto& n : p.notes) { const int old = n.note; n.note = c.convert (n.note, n.art); laneMap[old] = n.note; }
    // two sounds of the old plugin can land on the same note: keep one hit, the louder, never a doubled one
    p.sort();
    std::vector<Note> kept;
    for (auto& n : p.notes) {
        if (! kept.empty() && kept.back().tick == n.tick && kept.back().note == n.note) { kept.back().velocity = jmax (kept.back().velocity, n.velocity); continue; }
        kept.push_back (n);
    }
    p.notes = kept;
    std::map<int, int> ll; for (auto& [k, v] : p.laneLength) { auto it = laneMap.find (k); ll[it != laneMap.end() ? it->second : k] = v; }
    p.laneLength = ll;
    std::set<int> lm; for (int k : p.laneMute) { auto it = laneMap.find (k); lm.insert (it != laneMap.end() ? it->second : k); }
    p.laneMute = lm;
    p.sort();
}

// switch the whole song to another drum plugin's map; lanes follow their sounds
inline void convertSong (Song& s, int fromId, int toId)
{
    const auto from = s.map (fromId), to = s.map (toId);
    for (auto& p : s.keys) convertPattern (p, from, to);
    for (auto& p : s.park) convertPattern (p, from, to);
    std::vector<Lane> old = s.lanes;
    s.playMap = toId;
    s.lanes.clear();
    for (auto& l : old) {
        int art = from.artFor (l.note);
        int n = art >= 0 ? to.resolve (art) : l.note;
        if (n < 0) n = l.note;
        if (s.lane (n) != nullptr) continue;
        Lane nl = l; nl.note = n; nl.name = s.laneName (n);
        s.lanes.push_back (nl);
    }
    for (auto& p : s.keys) s.ensureLanesForPattern (p);
    s.sortLanes();
    // only AIR Strike needs its own channel (2); every other plugin keeps the channel you chose
    if (toId == mapStrike && fromId != mapStrike) { s.settings.channelBeforeStrike = s.settings.outChannel; s.settings.outChannel = 2; }
    else if (fromId == mapStrike && toId != mapStrike) s.settings.outChannel = s.settings.channelBeforeStrike;
}

// ------------------------------------------------------------------ MIDI files
struct ImportedPart { String name; std::vector<Note> notes; int lengthTicks = 0; int beatsPerBar = 4; };

inline ImportedPart readMidiFile (const File& f)
{
    ImportedPart r; r.name = f.getFileNameWithoutExtension();
    FileInputStream in (f);
    if (! in.openedOk()) return r;
    MidiFile mf;
    if (! mf.readFrom (in)) return r;
    int tpq = mf.getTimeFormat();
    double secScale = 0.0;
    if (tpq <= 0) { mf.convertTimestampTicksToSeconds(); secScale = 2.0 * PPQ; tpq = PPQ; }   // SMPTE: assume 120 bpm
    int lastEnd = 0;
    for (int t = 0; t < mf.getNumTracks(); ++t) {
        MidiMessageSequence seq (*mf.getTrack (t));
        seq.updateMatchedPairs();
        for (int i = 0; i < seq.getNumEvents(); ++i) {
            auto* e = seq.getEventPointer (i);
            const auto& m = e->message;
            auto toTicks = [&] (double ts) { return (int) std::round (secScale > 0 ? ts * secScale : ts * PPQ / tpq); };
            if (m.isTimeSignatureMetaEvent()) { int num = 4, den = 4; m.getTimeSignatureInfo (num, den); r.beatsPerBar = jlimit (1, 16, (int) std::round (num * 4.0 / den)); }
            if (! m.isNoteOn()) continue;
            Note n; n.tick = toTicks (m.getTimeStamp()); n.note = m.getNoteNumber(); n.velocity = jlimit (1, 127, (int) m.getVelocity());
            n.length = e->noteOffObject != nullptr ? jmax (10, toTicks (e->noteOffObject->message.getTimeStamp()) - n.tick) : 60;
            r.notes.push_back (n);
            lastEnd = jmax (lastEnd, n.tick + 1);
        }
    }
    std::stable_sort (r.notes.begin(), r.notes.end());
    // length: whole bars covering the last note
    const int bar = r.beatsPerBar * PPQ;
    r.lengthTicks = jmax (bar, (lastEnd + bar - 1) / bar * bar);
    // a clip that starts a little late (some plugins write an offset) is shifted back to the first bar line
    if (! r.notes.empty()) {
        const int first = r.notes.front().tick;
        const int shift = first / bar * bar;
        if (shift > 0) { for (auto& n : r.notes) n.tick -= shift; r.lengthTicks -= shift; }
    }
    return r;
}

// cut an imported part into patterns of barsPerPattern bars (0 = one pattern)
inline std::vector<Pattern> splitToPatterns (const ImportedPart& part, int barsPerPattern)
{
    std::vector<Pattern> out;
    const int bar = part.beatsPerBar * PPQ;
    const int chunk = barsPerPattern <= 0 ? jmax (bar, part.lengthTicks) : jlimit (1, maxBars, barsPerPattern) * bar;
    const int total = jmax (bar, part.lengthTicks);
    const int count = (total + chunk - 1) / chunk;
    for (int c = 0; c < count; ++c) {
        Pattern p;
        p.beatsPerBar = part.beatsPerBar;
        const int start = c * chunk, end = jmin (total, start + chunk);
        p.bars = jlimit (1, maxBars, (end - start + bar - 1) / bar);
        p.name = count > 1 ? part.name + " " + String (c + 1) : part.name;
        for (auto n : part.notes) if (n.tick >= start && n.tick < end) { n.tick -= start; p.notes.push_back (n); }
        if (part.name.containsIgnoreCase ("fill")) p.type = PatternType::Fill;
        else if (part.name.containsIgnoreCase ("intro")) p.type = PatternType::Intro;
        else if (part.name.containsIgnoreCase ("end") || part.name.containsIgnoreCase ("outro")) p.type = PatternType::Ending;
        out.push_back (std::move (p));
    }
    return out;
}

inline File writeMidiFile (const std::vector<const Pattern*>& parts, const File& f, int channel)
{
    MidiMessageSequence seq;
    int offset = 0;
    for (auto* p : parts) {
        for (auto& n : p->notes) {
            if (n.tick >= p->lengthTicks()) continue;
            seq.addEvent (MidiMessage::noteOn (channel, n.note, (uint8) jlimit (1, 127, n.velocity)), (double) (offset + n.tick));
            seq.addEvent (MidiMessage::noteOff (channel, n.note), (double) (offset + n.tick + jmax (1, n.length)));
        }
        offset += p->lengthTicks();
    }
    seq.updateMatchedPairs();
    MidiMessageSequence meta;
    if (! parts.empty()) meta.addEvent (MidiMessage::timeSignatureMetaEvent (parts.front()->beatsPerBar, 4), 0);
    meta.addEvent (MidiMessage::endOfTrack(), (double) offset);
    MidiFile mf; mf.setTicksPerQuarterNote (PPQ);
    mf.addTrack (meta); mf.addTrack (seq);
    f.deleteFile();
    FileOutputStream out (f);
    if (out.openedOk()) mf.writeTo (out, 1);
    return f;
}


// the song lane written out as one MIDI file: every chunk's notes in place
inline void writeSongMidi (const Song& s, const File& f, int channel)
{
    MidiMessageSequence seq;
    double end = 0;
    for (auto& c : s.clips) {
        if (c.muted) continue;
        const auto& p = s.keys[(size_t) jlimit (0, 127, c.key)];
        const double L = p.lengthTicks() / (double) PPQ;
        if (L <= 0) continue;
        end = jmax (end, c.start + c.length);
        for (auto& n : p.notes) {
            const double LL = p.laneLen (n.note) / (double) PPQ;
            if (n.tick >= p.laneLen (n.note)) continue;
            for (double base = std::floor (c.offset / LL) * LL; base < c.offset + c.length; base += LL) {
                const double t = base + n.tick / (double) PPQ;
                if (t < c.offset - 1e-9 || t >= c.offset + c.length - 1e-9) continue;
                const double at = (c.start + t - c.offset) * PPQ;
                seq.addEvent (MidiMessage::noteOn (channel, n.note, (uint8) jlimit (1, 127, n.velocity)), at);
                seq.addEvent (MidiMessage::noteOff (channel, n.note), at + jmax (1, n.length));
            }
        }
    }
    seq.updateMatchedPairs();
    MidiMessageSequence meta; meta.addEvent (MidiMessage::endOfTrack(), end * PPQ);
    MidiFile mf; mf.setTicksPerQuarterNote (PPQ); mf.addTrack (meta); mf.addTrack (seq);
    f.deleteFile(); FileOutputStream out (f); if (out.openedOk()) mf.writeTo (out, 1);
}

// ------------------------------------------------------------------ XML state
inline std::unique_ptr<XmlElement> patternToXml (const Pattern& p, const String& tag = "P")
{
    auto x = std::make_unique<XmlElement> (tag);
    x->setAttribute ("name", p.name); x->setAttribute ("type", (int) p.type); x->setAttribute ("group", p.group);
    x->setAttribute ("bars", p.bars); x->setAttribute ("bpb", p.beatsPerBar);
    String s; s.preallocateBytes (p.notes.size() * 24);
    for (auto& n : p.notes) s << n.tick << ',' << n.length << ',' << n.note << ',' << n.velocity << ',' << n.probability << ',' << n.art << ';';
    x->setAttribute ("notes", s);
    String ll; for (auto& [k, v] : p.laneLength) ll << k << ':' << v << ';';
    x->setAttribute ("lanelen", ll);
    String lm; for (int k : p.laneMute) lm << k << ';';
    x->setAttribute ("lanemute", lm);
    return x;
}
inline Pattern patternFromXml (const XmlElement& x)
{
    Pattern p;
    p.name = x.getStringAttribute ("name"); p.type = (PatternType) jlimit (0, 3, x.getIntAttribute ("type"));
    p.group = jlimit (-1, 63, x.getIntAttribute ("group", -1));
    p.bars = jlimit (1, maxBars, x.getIntAttribute ("bars", 1)); p.beatsPerBar = jlimit (1, 16, x.getIntAttribute ("bpb", 4));
    for (auto& tok : StringArray::fromTokens (x.getStringAttribute ("notes"), ";", "")) {
        auto f = StringArray::fromTokens (tok, ",", "");
        if (f.size() < 4) continue;
        Note n; n.tick = f[0].getIntValue(); n.length = f[1].getIntValue(); n.note = jlimit (0, 127, f[2].getIntValue());
        n.velocity = jlimit (1, 127, f[3].getIntValue()); n.probability = f.size() > 4 ? jlimit (0, 100, f[4].getIntValue()) : 100;
        n.art = f.size() > 5 ? jlimit ((int) aNone, (int) aCount - 1, f[5].getIntValue()) : -1;
        p.notes.push_back (n);
    }
    for (auto& tok : StringArray::fromTokens (x.getStringAttribute ("lanelen"), ";", "")) {
        auto f = StringArray::fromTokens (tok, ":", ""); if (f.size() == 2) p.laneLength[f[0].getIntValue()] = f[1].getIntValue();
    }
    for (auto& tok : StringArray::fromTokens (x.getStringAttribute ("lanemute"), ";", "")) if (tok.isNotEmpty()) p.laneMute.insert (tok.getIntValue());
    p.sort();
    return p;
}

inline std::unique_ptr<XmlElement> songToXml (const Song& s)
{
    auto x = std::make_unique<XmlElement> ("DrumPlayerSong");
    x->setAttribute ("version", 3); x->setAttribute ("editKey", s.editKey);
    x->setAttribute ("playMap", s.playMap); x->setAttribute ("importMap", s.importMap);
    String cn; for (int v : s.customNotes) cn << v << ','; x->setAttribute ("customMap", cn);
    auto* ks = x->createNewChildElement ("Keys");
    for (int k = 0; k < numKeys; ++k) if (! s.keys[(size_t) k].isEmpty()) { auto px = patternToXml (s.keys[(size_t) k]); px->setAttribute ("key", k); ks->addChildElement (px.release()); }
    auto* pk = x->createNewChildElement ("Park");
    for (int i = 0; i < (int) s.park.size(); ++i) if (! s.park[(size_t) i].isEmpty()) { auto px = patternToXml (s.park[(size_t) i]); px->setAttribute ("slot", i); pk->addChildElement (px.release()); }
    auto* ls = x->createNewChildElement ("Lanes");
    for (auto& l : s.lanes) {
        auto* e = ls->createNewChildElement ("L");
        e->setAttribute ("note", l.note); e->setAttribute ("name", l.name); e->setAttribute ("level", l.level);
        e->setAttribute ("pan", l.pan); e->setAttribute ("ch", l.channel); e->setAttribute ("swing", l.swingMode);
    }
    auto* cs = x->createNewChildElement ("Chains");
    for (auto& c : s.chains) {
        auto* e = cs->createNewChildElement ("C");
        e->setAttribute ("name", c.name); e->setAttribute ("trigger", c.triggerNote); e->setAttribute ("loop", c.loop);
        String st; for (auto& step : c.steps) st << step.key << ':' << step.repeats << ';';
        e->setAttribute ("steps", st);
    }
    auto* gs = x->createNewChildElement ("Groups");
    for (auto& g : s.groups) { auto* e = gs->createNewChildElement ("G"); e->setAttribute ("name", g.name); e->setAttribute ("colour", String::toHexString ((int) g.colour)); }
    auto* sg = x->createNewChildElement ("SongLane");
    sg->setAttribute ("on", s.songMode);
    for (auto& c : s.clips) {
        auto* e = sg->createNewChildElement ("Clip");
        e->setAttribute ("start", c.start); e->setAttribute ("len", c.length); e->setAttribute ("off", c.offset);
        e->setAttribute ("key", c.key); e->setAttribute ("muted", c.muted);
    }
    auto* st = x->createNewChildElement ("Settings");
    const auto& o = s.settings;
    st->setAttribute ("out", o.outChannel); st->setAttribute ("zlo", o.zoneLow); st->setAttribute ("zhi", o.zoneHigh);
    st->setAttribute ("sq", (int) o.startQuant); st->setAttribute ("latch", o.latch); st->setAttribute ("hoststop", o.stopWithHost);
    st->setAttribute ("thru", o.passThrough); st->setAttribute ("fillsblack", o.fillsToBlack); st->setAttribute ("remember", o.rememberLast);
    st->setAttribute ("stop", o.stopNote); st->setAttribute ("recq", o.recQuantize); st->setAttribute ("split", o.splitBars);
    st->setAttribute ("trigch", o.triggerChannel); st->setAttribute ("playch", o.playChannel); st->setAttribute ("mirror", o.mirrorOctaves);
    st->setAttribute ("countin", o.countInBars); st->setAttribute ("metro", o.metroMode); st->setAttribute ("metrolvl", o.metroLevel);
    st->setAttribute ("keymode", o.keyMode);
    st->setAttribute ("recmode", o.recMode); st->setAttribute ("recstop", o.recStopAfter); st->setAttribute ("recvel", o.recFixedVel);
    return x;
}

inline Song songFromXml (const XmlElement& x)
{
    Song s;
    s.editKey = jlimit (0, 127, x.getIntAttribute ("editKey", 0));
    s.playMap = jlimit (0, (int) numMaps - 1, x.getIntAttribute ("playMap", mapGM));
    s.importMap = jlimit (-1, (int) numMaps - 1, x.getIntAttribute ("importMap", -1));
    s.customNotes = Song::defaultCustom();
    { auto f = StringArray::fromTokens (x.getStringAttribute ("customMap"), ",", ""); f.removeEmptyStrings();
      if (f.size() == aCount) for (int a = 0; a < aCount; ++a) s.customNotes[(size_t) a] = jlimit (-1, 127, f[a].getIntValue()); }
    if (auto* ks = x.getChildByName ("Keys"))
        for (auto* e : ks->getChildIterator()) { const int k = e->getIntAttribute ("key", -1); if (k >= 0 && k < numKeys) s.keys[(size_t) k] = patternFromXml (*e); }
    if (auto* pk = x.getChildByName ("Park"))
        for (auto* e : pk->getChildIterator()) { const int i = e->getIntAttribute ("slot", -1); if (i >= 0 && i < (int) s.park.size()) s.park[(size_t) i] = patternFromXml (*e); }
    if (auto* ls = x.getChildByName ("Lanes"))
        for (auto* e : ls->getChildIterator()) {
            Lane l; l.note = jlimit (0, 127, e->getIntAttribute ("note")); l.name = e->getStringAttribute ("name", gmDrumName (l.note));
            l.level = (float) e->getDoubleAttribute ("level", 1.0); l.pan = e->getIntAttribute ("pan", 64);
            l.channel = e->getIntAttribute ("ch", 0); l.swingMode = e->getIntAttribute ("swing", 0);
            s.lanes.push_back (l);
        }
    if (auto* cs = x.getChildByName ("Chains")) {
        s.chains.clear();
        for (auto* e : cs->getChildIterator()) {
            s.chains.push_back ({});
            auto& c = s.chains.back();
            c.name = e->getStringAttribute ("name", "Chain " + String ((int) s.chains.size())); c.triggerNote = e->getIntAttribute ("trigger", -1); c.loop = e->getBoolAttribute ("loop", true);
            for (auto& tok : StringArray::fromTokens (e->getStringAttribute ("steps"), ";", "")) {
                auto f = StringArray::fromTokens (tok, ":", ""); if (f.size() == 2) c.steps.push_back ({ f[0].getIntValue(), jmax (1, f[1].getIntValue()) });
            }
        }
    }
    if (auto* gs = x.getChildByName ("Groups")) {
        s.groups.clear();
        for (auto* e : gs->getChildIterator()) s.groups.push_back ({ e->getStringAttribute ("name"), (uint32) e->getStringAttribute ("colour", "ff7fd1ff").getHexValue32() });
    }
    if (auto* sg = x.getChildByName ("SongLane")) {
        s.songMode = sg->getBoolAttribute ("on", false);
        for (auto* e : sg->getChildIterator()) {
            Clip c; c.start = jmax (0.0, e->getDoubleAttribute ("start")); c.length = jmax (0.25, e->getDoubleAttribute ("len", 4));
            c.offset = jmax (0.0, e->getDoubleAttribute ("off")); c.key = jlimit (0, 127, e->getIntAttribute ("key", 12)); c.muted = e->getBoolAttribute ("muted");
            s.clips.push_back (c);
        }
    }
    // older saves had four fixed chains: keep only the ones in use
    s.chains.erase (std::remove_if (s.chains.begin(), s.chains.end(), [] (const Chain& c) { return c.steps.empty() && c.triggerNote < 0; }), s.chains.end());
    if (s.chains.empty()) s.chains.push_back ({ "Chain 1", {}, -1, true });
    if (auto* st = x.getChildByName ("Settings")) {
        auto& o = s.settings;
        o.outChannel = jlimit (1, 16, st->getIntAttribute ("out", 10));
        o.zoneLow = jlimit (0, 127, st->getIntAttribute ("zlo", 0)); o.zoneHigh = jlimit (o.zoneLow, 127, st->getIntAttribute ("zhi", 35));
        o.startQuant = (StartQuant) jlimit (0, 4, st->getIntAttribute ("sq", 4));
        o.latch = st->getBoolAttribute ("latch", true); o.stopWithHost = st->getBoolAttribute ("hoststop", true);
        o.passThrough = st->getBoolAttribute ("thru", true); o.fillsToBlack = st->getBoolAttribute ("fillsblack", false);
        o.rememberLast = st->getBoolAttribute ("remember", true); o.stopNote = jlimit (-2, 127, st->getIntAttribute ("stop", -2));
        o.triggerChannel = jlimit (0, 16, st->getIntAttribute ("trigch", 0)); o.playChannel = jlimit (0, 16, st->getIntAttribute ("playch", 0));
        o.mirrorOctaves = st->getBoolAttribute ("mirror", false);
        o.countInBars = jlimit (0, 2, st->getIntAttribute ("countin", 1)); o.metroMode = jlimit (0, 3, st->getIntAttribute ("metro", 2));
        o.metroLevel = (float) jlimit (0.0, 1.0, st->getDoubleAttribute ("metrolvl", 0.5));
        o.recMode = jlimit (0, 1, st->getIntAttribute ("recmode", 0)); o.recStopAfter = jlimit (0, 64, st->getIntAttribute ("recstop", 0));
        o.recFixedVel = jlimit (0, 127, st->getIntAttribute ("recvel", 0));
        o.keyMode = jlimit (0, 1, st->getIntAttribute ("keymode", 0));
        if (x.getIntAttribute ("version", 1) < 3) {     // older saves: patterns move up an octave to start at C-1, stop on B-2
            o.zoneLow = 12; o.zoneHigh = 127; o.stopNote = 11; o.mirrorOctaves = false; o.startQuant = StartQuant::InTime;
            for (int k = 127; k >= 12; --k) if (s.keys[(size_t) (k - 12)].isEmpty() == false && s.keys[(size_t) k].isEmpty()) std::swap (s.keys[(size_t) k], s.keys[(size_t) (k - 12)]);
            for (auto& c : s.chains) for (auto& step : c.steps) if (step.key >= 0 && step.key < 116) step.key += 12;
            for (auto& c : s.clips) if (c.key < 116) c.key += 12;
            s.editKey = jlimit (12, 127, s.editKey + 12);
        }
        o.recQuantize = st->getIntAttribute ("recq", 0); o.splitBars = jlimit (0, maxBars, st->getIntAttribute ("split", 4));
    }
    if (s.lanes.empty()) s.lanes = Song::makeDefault().lanes;
    for (auto& p : s.keys) s.ensureLanesForPattern (p);
    s.sortLanes();
    return s;
}

// ------------------------------------------------------------------ files on disk
inline File dataFolder()
{
    auto d = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("BD Plugins").getChildFile ("Drum Player");
    d.createDirectory();
    return d;
}
inline File presetFolder() { auto d = dataFolder().getChildFile ("Presets"); d.createDirectory(); return d; }
inline File lastSessionFile() { return dataFolder().getChildFile ("LastSession.xml"); }
inline File favouritesFile() { return dataFolder().getChildFile ("Favourites.txt"); }
constexpr const char* presetExt = ".drumplayer";

} // namespace dp
