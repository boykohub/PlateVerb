// Drum Player: drum maps of the common drum plugins.
// Notes are translated through articulations (kick, closed hi-hat tip, ride bell...), so MIDI made for one
// plugin plays the same drums on another. Sources for the tables:
//   General MIDI: MIDI 1.0 GM percussion map (notes 35-81).
//   AIR Strike 2: user guide - channel 2 plays instruments by General MIDI, channel 1 triggers its patterns.
//   Addictive Drums 2: XLN Audio "Addictive Drums 2 Keymap" (support.xlnaudio.com).
//   Toontrack EZdrummer / Superior Drummer: drumverter.com note map for EZdrummer 2 (GM-compatible core).
//   Steinberg Groove Agent (Acoustic Agent): steinberg.help "The Kit" - GM core, hi-hat shank on 30/34.
//   BFD3 and GetGood Drums Invasion: drumverter.com note maps.
#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <array>

namespace dp {

enum Art : int {
    aNone = -1,
    aKick = 0, aSnare, aRimshot, aSideStick, aSnareFlam, aRimClick,
    aHHClosed, aHHClosedEdge, aHHTight, aHHLoose, aHHOpen, aHHOpenWide, aHHPedal, aHHSplash, aHHBell,
    aTom1, aTom2, aTom3, aTom4, aTom5, aTom6,
    aCrash1, aCrash1Choke, aCrash2, aCrash2Choke, aCrash3, aCrash3Choke,
    aRide, aRideBell, aRideEdge, aRideChoke, aChina, aSplash,
    aCowbell, aTambourine, aClap, aShaker,
    aCount
};

inline const char* artName (int a)
{
    static const char* n[] = {
        "Kick", "Snare", "Snare Rimshot", "Side Stick", "Snare Flam", "Snare Rim Click",
        "Hi-Hat Closed", "Hi-Hat Closed Edge", "Hi-Hat Tight", "Hi-Hat Half Open", "Hi-Hat Open", "Hi-Hat Wide Open", "Hi-Hat Pedal", "Hi-Hat Foot Splash", "Hi-Hat Bell",
        "Tom 1 (high)", "Tom 2", "Tom 3", "Tom 4", "Floor Tom 1", "Floor Tom 2",
        "Crash 1", "Crash 1 Choke", "Crash 2", "Crash 2 Choke", "Crash 3", "Crash 3 Choke",
        "Ride", "Ride Bell", "Ride Edge", "Ride Choke", "China", "Splash",
        "Cowbell", "Tambourine", "Clap", "Shaker" };
    return (a >= 0 && a < aCount) ? n[a] : "";
}

// nearest sounds to fall back on when a plugin has no exact match
inline const std::vector<int>& artFallback (int a)
{
    static const std::vector<std::vector<int>> f = [] {
        std::vector<std::vector<int>> v ((size_t) aCount);
        auto set = [&v] (int a, std::vector<int> list) { v[(size_t) a] = std::move (list); };
        set (aSnare, { aRimshot });                 set (aRimshot, { aSnare });
        set (aSideStick, { aRimClick, aSnare });    set (aSnareFlam, { aSnare });  set (aRimClick, { aSideStick, aSnare });
        set (aHHClosed, { aHHTight, aHHClosedEdge }); set (aHHClosedEdge, { aHHClosed, aHHTight });
        set (aHHTight, { aHHClosed, aHHClosedEdge }); set (aHHLoose, { aHHOpen, aHHClosed });
        set (aHHOpen, { aHHLoose, aHHOpenWide });     set (aHHOpenWide, { aHHOpen, aHHLoose });
        set (aHHPedal, { aHHClosed });                set (aHHSplash, { aHHOpen, aHHPedal });   set (aHHBell, { aHHClosed });
        set (aTom1, { aTom2, aTom3 }); set (aTom2, { aTom1, aTom3 }); set (aTom3, { aTom4, aTom2 });
        set (aTom4, { aTom3, aTom5 }); set (aTom5, { aTom6, aTom4 }); set (aTom6, { aTom5, aTom4 });
        set (aCrash1, { aCrash2, aCrash3 });          set (aCrash2, { aCrash1, aCrash3 });      set (aCrash3, { aCrash2, aCrash1 });
        set (aCrash1Choke, { aCrash1 }); set (aCrash2Choke, { aCrash2, aCrash1 }); set (aCrash3Choke, { aCrash3, aCrash2 });
        set (aRide, { aRideEdge });   set (aRideBell, { aRide });   set (aRideEdge, { aRide, aCrash2 });   set (aRideChoke, { aRide });
        set (aChina, { aCrash3, aCrash2 });  set (aSplash, { aCrash1, aCrash2 });
        set (aCowbell, { aRideBell });       set (aTambourine, { aShaker });  set (aShaker, { aTambourine });  set (aClap, { aSnare });
        return v;
    }();
    static const std::vector<int> none;
    return (a >= 0 && a < aCount) ? f[(size_t) a] : none;
}

struct DrumMap
{
    juce::String name;
    int channel = 10;                                   // the channel this plugin listens on for single hits
    std::vector<std::pair<int, int>> notes;             // note → articulation; the first note listed for an articulation is the one written
    juce::String note;                                  // shown in the map page

    int artFor (int noteNumber) const { for (auto& [n, a] : notes) if (n == noteNumber) return a; return aNone; }
    int mainNote (int art) const { for (auto& [n, a] : notes) if (a == art) return n; return -1; }
    // the note to write for an articulation, using the nearest sound if this plugin lacks it
    int resolve (int art) const
    {
        if (art < 0) return -1;
        if (int n = mainNote (art); n >= 0) return n;
        for (int f : artFallback (art)) { if (int n = mainNote (f); n >= 0) return n; }
        for (int f : artFallback (art)) for (int g : artFallback (f)) { if (int n = mainNote (g); n >= 0) return n; }
        return -1;
    }
};

enum MapId { mapGM = 0, mapToontrack, mapStrike, mapAD2, mapGrooveAgent, mapBFD3, mapGGD, mapCustom, numMaps };

inline std::vector<std::pair<int, int>> gmNotes()
{
    return { { 36, aKick }, { 35, aKick }, { 38, aSnare }, { 40, aRimshot }, { 37, aSideStick }, { 39, aClap },
             { 42, aHHClosed }, { 44, aHHPedal }, { 46, aHHOpen },
             { 50, aTom1 }, { 48, aTom2 }, { 47, aTom3 }, { 45, aTom4 }, { 43, aTom5 }, { 41, aTom6 },
             { 49, aCrash1 }, { 57, aCrash2 }, { 51, aRide }, { 53, aRideBell }, { 59, aRideEdge },
             { 52, aChina }, { 55, aSplash }, { 56, aCowbell }, { 54, aTambourine }, { 70, aShaker }, { 82, aShaker } };
}

inline const std::vector<DrumMap>& builtInMaps()
{
    static const std::vector<DrumMap> maps = [] {
        std::vector<DrumMap> m ((size_t) numMaps);

        m[mapGM] = { "General MIDI (Impact XT, most plugins)", 10, gmNotes(),
                     "The standard layout. Studio One's Impact XT follows the samples you load, so GM is the starting point there." };

        m[mapToontrack] = { "Toontrack EZdrummer / Superior Drummer", 10, {
            { 36, aKick }, { 35, aKick }, { 34, aKick },
            { 38, aSnare }, { 33, aSnare }, { 39, aSnare }, { 6, aSnare }, { 40, aRimshot }, { 37, aSideStick }, { 4, aSideStick }, { 5, aSideStick },
            { 42, aHHClosed }, { 11, aHHClosed }, { 61, aHHClosed }, { 22, aHHClosedEdge }, { 64, aHHClosedEdge },
            { 63, aHHTight }, { 62, aHHTight }, { 24, aHHLoose }, { 12, aHHLoose }, { 13, aHHLoose },
            { 46, aHHOpen }, { 25, aHHOpen }, { 14, aHHOpen }, { 26, aHHOpenWide }, { 15, aHHOpenWide }, { 60, aHHOpenWide }, { 16, aHHOpenWide }, { 17, aHHOpenWide },
            { 44, aHHPedal }, { 21, aHHPedal }, { 10, aHHPedal }, { 23, aHHSplash },
            { 48, aTom2 }, { 47, aTom3 }, { 45, aTom4 }, { 43, aTom5 }, { 41, aTom6 },
            { 55, aCrash1 }, { 56, aCrash1Choke }, { 49, aCrash2 }, { 50, aCrash2Choke }, { 57, aCrash3 }, { 58, aCrash3Choke },
            { 51, aRide }, { 53, aRideBell }, { 59, aRideEdge }, { 83, aRideChoke }, { 52, aChina } },
            "GM-compatible core. Note 50 is a crash choke here, not a tom." };

        auto strike = gmNotes();
        m[mapStrike] = { "AIR Strike", 2, strike,
                         "Strike plays single drum hits from General MIDI notes on MIDI channel 2 (channel 1 starts its own patterns)." };

        m[mapAD2] = { "XLN Addictive Drums 2", 10, {
            { 36, aKick },
            { 38, aSnare }, { 40, aSnare }, { 43, aSnare }, { 39, aRimshot }, { 37, aRimshot }, { 41, aRimshot }, { 42, aSideStick }, { 44, aRimClick },
            { 49, aHHClosed }, { 51, aHHClosed }, { 50, aHHClosedEdge }, { 52, aHHClosedEdge }, { 53, aHHBell },
            { 54, aHHLoose }, { 55, aHHLoose }, { 56, aHHOpen }, { 57, aHHOpenWide }, { 58, aHHBell }, { 48, aHHPedal }, { 59, aHHSplash },
            { 71, aTom2 }, { 72, aTom2 }, { 69, aTom3 }, { 70, aTom3 }, { 67, aTom5 }, { 68, aTom5 }, { 65, aTom6 }, { 66, aTom6 },
            { 77, aCrash1 }, { 46, aCrash1 }, { 78, aCrash1Choke }, { 79, aCrash2 }, { 80, aCrash2Choke }, { 81, aCrash3 }, { 82, aCrash3Choke },
            { 60, aRide }, { 45, aRide }, { 61, aRideBell }, { 62, aRideEdge }, { 63, aRideChoke } },
            "Hi-hats sit on 48-59, toms on 65-72 and cymbals from 77 up, so GM files need converting." };

        auto ga = gmNotes();
        ga.push_back ({ 30, aHHClosedEdge }); ga.push_back ({ 34, aHHLoose });
        m[mapGrooveAgent] = { "Steinberg Groove Agent / Groove Agent SE", 10, ga,
                              "Acoustic Agent kits: General MIDI core, hi-hat shank on 30 and 34. Hi-hat openness can also follow the mod wheel." };

        m[mapBFD3] = { "FXpansion / inMusic BFD3", 10, {
            { 24, aKick }, { 84, aKick },
            { 26, aSnare }, { 28, aSnare }, { 29, aRimshot }, { 88, aRimClick }, { 25, aSideStick }, { 86, aSnareFlam },
            { 30, aHHClosed }, { 37, aHHClosedEdge }, { 42, aHHLoose }, { 44, aHHLoose }, { 49, aHHOpen }, { 46, aHHOpen }, { 51, aHHOpen }, { 54, aHHOpen },
            { 34, aHHOpenWide }, { 39, aHHOpenWide }, { 32, aHHPedal }, { 56, aHHSplash }, { 58, aHHBell },
            { 38, aTom2 }, { 40, aTom2 }, { 35, aTom3 }, { 36, aTom3 }, { 31, aTom5 }, { 33, aTom5 },
            { 72, aCrash1 }, { 41, aCrash1 }, { 75, aCrash1Choke }, { 76, aCrash2 }, { 45, aCrash2 }, { 80, aCrash2Choke }, { 74, aCrash3 }, { 43, aCrash3 }, { 78, aCrash3Choke },
            { 60, aRide }, { 71, aRideBell }, { 50, aRideEdge }, { 81, aRideEdge }, { 87, aRideChoke }, { 83, aClap } },
            "BFD's own layout starts the kick at 24." };

        m[mapGGD] = { "GetGood Drums (Invasion layout)", 10, {
            { 24, aKick }, { 22, aKick }, { 23, aKick },
            { 26, aSnare }, { 29, aSnare }, { 27, aSnareFlam }, { 30, aSideStick },
            { 43, aHHClosed }, { 44, aHHClosedEdge }, { 41, aHHTight }, { 42, aHHTight }, { 45, aHHLoose }, { 46, aHHOpen }, { 47, aHHOpenWide }, { 48, aHHPedal },
            { 33, aTom2 }, { 34, aTom3 }, { 35, aTom4 }, { 36, aTom5 }, { 37, aTom6 }, { 38, aTom6 },
            { 52, aCrash1 }, { 53, aCrash1Choke }, { 54, aCrash2 }, { 55, aCrash2Choke }, { 56, aCrash3 }, { 57, aCrash3Choke },
            { 62, aRide }, { 61, aRideBell }, { 63, aRideEdge }, { 65, aChina }, { 67, aChina }, { 73, aSplash }, { 75, aSplash }, { 79, aCowbell } },
            "GGD packs share a similar layout; this is the Invasion map." };

        m[mapCustom] = { "Custom (your own map)", 10, gmNotes(), "Set each sound's note yourself in the list below." };
        return m;
    }();
    return maps;
}

// converts note numbers of one plugin's map into another's; keeps the articulation so switching back is exact
struct MapConverter
{
    const DrumMap& from;
    const DrumMap& to;
    // returns the new note (unchanged if the source note is unknown); art is in/out
    int convert (int note, int& art) const
    {
        int a = art;
        if (a < 0 || from.resolve (a) != note) a = from.artFor (note);   // stored articulation no longer matches: re-read it
        if (a < 0) return note;
        art = a;
        const int n = to.resolve (a);
        return n >= 0 ? n : note;          // no such sound on that plugin: the note stays, and plays silent (see publish)
    }
};

} // namespace dp
