// Flexiverb 480: the 480L's programs and parameter pages (no JUCE dependency).
// Banks 1-17: factory programs with the values printed in the Lexicon 480L owner's manual (software v4.0).
// Banks 18-21 (Classic Cart): the manuals have no tables for these, so their settings are Flexiverb's own.
#pragma once
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

namespace l480 {

constexpr int kSliders = 6, kMaxPages = 5, kMaxParams = kSliders * kMaxPages;

// ---------------------------------------------------------------- parameter units and display
enum Unit { uNone, uNum, uSec, uMs, uUsMs, uHz, uDb, uPct, uMult, uMeter, uRatio, uPitch, uCents, uSmp, uDeg, uPan, uBal, uChoice, uKey, uNote };
enum Curve { cLin = 0, cLog, cSkew };

struct PSpec {
    const char* lab = nullptr;      // LARC slider label (3-4 characters); nullptr = empty slot
    const char* name = "";          // full name for the main display
    Unit unit = uNone; float lo = 0, hi = 1; int dec = 0; Curve curve = cLin;
    const char* loText = nullptr;   // shown at the bottom of the range (e.g. "Off")
    const char* hiText = nullptr;   // shown at the top (e.g. "Full", "All Fx")
    const char* zeroText = nullptr; // shown at 0 (e.g. "Flat", "Center")
    const char* choices = nullptr;  // "a|b|c" for uChoice
    int alias = -1;                 // this slot shows another slot's value (LARC repeats MIX on two pages)
};

inline int numChoices (const PSpec& s) { if (! s.choices) return 1; int n = 1; for (const char* c = s.choices; *c; ++c) n += *c == '|'; return n; }
inline std::string choiceText (const PSpec& s, int i) {
    const char* c = s.choices; if (! c) return "";
    for (int k = 0; k < i; ++k) { c = std::strchr (c, '|'); if (! c) return ""; ++c; }
    const char* e = std::strchr (c, '|'); return e ? std::string (c, (size_t) (e - c)) : std::string (c);
}

// normalised slider position (0..1) <-> value in the parameter's units
inline float toValue (const PSpec& s, float t) {
    t = std::min (1.0f, std::max (0.0f, t));
    if (s.unit == uChoice) { const int n = numChoices (s); return (float) std::min (n - 1, (int) std::floor (t * n)); }
    if (s.unit == uKey || ! s.lab) return 0.0f;
    float v;
    if (s.curve == cLog && s.lo > 0) v = s.lo * std::pow (s.hi / s.lo, t);
    else if (s.curve == cSkew) v = s.lo + (s.hi - s.lo) * t * t;
    else v = s.lo + (s.hi - s.lo) * t;
    if (s.dec == 0 && s.unit != uPitch) v = std::round (v);
    return v;
}
inline float toNorm (const PSpec& s, float v) {
    if (s.unit == uChoice) { const int n = numChoices (s); return ((float) std::max (0, std::min (n - 1, (int) std::lround (v))) + 0.5f) / (float) n; }
    if (s.unit == uKey || ! s.lab) return 0.0f;
    v = std::min (s.hi, std::max (s.lo, v));
    if (s.curve == cLog && s.lo > 0) return std::log (v / s.lo) / std::log (s.hi / s.lo);
    if (s.curve == cSkew) return std::sqrt ((v - s.lo) / (s.hi - s.lo));
    return (v - s.lo) / (s.hi - s.lo);
}

inline std::string fmtNum (float v, int dec) { char b[48]; std::snprintf (b, sizeof b, "%.*f", dec, (double) v); return b; }
inline std::string pitchText (float semis) {
    static const char* iv[] = { "null", "m2nd", "M2nd", "m3rd", "M3rd", "4th", "tri", "5th", "m6th", "M6th", "m7th", "M7th", "oct" };
    int c = (int) std::lround (semis * 100.0f);
    const char sign = c < 0 ? '-' : '+'; c = std::abs (c);
    int st = c / 100, ct = c % 100;
    if (ct > 50) { ++st; ct -= 100; }
    std::string s (1, sign);
    if (st == 0 && ct != 0) { s += std::to_string (ct) + "c"; return s; }
    if (st <= 12) s += iv[st]; else s += std::to_string (st) + "st";
    if (ct != 0) s += (ct > 0 ? "+" : "") + std::to_string (ct) + "c";
    else if (st != 0) s += "0c";
    return s;
}
inline std::string noteText (int n) { static const char* nm[] = { "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b" }; return std::to_string (n) + nm[((n % 12) + 12) % 12] + std::to_string (n / 12 - 2); }

inline std::string formatValue (const PSpec& s, float v) {
    if (! s.lab) return "";
    if (s.unit == uKey) return "";
    if (s.unit == uChoice) return choiceText (s, (int) std::lround (v));
    const float eps = (s.hi - s.lo) * 1.0e-4f + 1.0e-6f;
    if (s.loText && v <= s.lo + eps) return s.loText;
    if (s.hiText && v >= s.hi - eps) return s.hiText;
    if (s.zeroText && std::fabs (v) < eps) return s.zeroText;
    switch (s.unit) {
        case uSec:   return fmtNum (v, v < 10 ? 2 : 1) + " s";
        case uMs:    return fmtNum (v, v < 10 ? 2 : (v < 100 ? 1 : 0)) + " ms";
        case uUsMs:  return v < 1.0f ? std::to_string ((int) std::lround (v * 1000.0f)) + " us" : fmtNum (v, v < 10 ? 2 : 1) + " ms";
        case uHz:    return v >= 1000.0f ? fmtNum (v / 1000.0f, v < 10000 ? 3 : 2) + " kHz" : std::to_string ((int) std::lround (v)) + " Hz";
        case uDb:    return fmtNum (v, s.dec) + " dB";
        case uPct:   return std::to_string ((int) std::lround (v)) + "%";
        case uMult:  return fmtNum (v, s.dec) + " x";
        case uMeter: return fmtNum (v, v < 10 && s.dec > 0 ? 1 : 0) + " m";
        case uRatio: return fmtNum (v, 2) + ":1";
        case uPitch: return pitchText (v);
        case uCents: return (v >= 0 ? "+" : "") + std::to_string ((int) std::lround (v)) + "c";
        case uSmp:   return std::to_string ((int) std::lround (v)) + " smp";
        case uDeg:   return fmtNum (v, s.dec) + " deg";
        case uPan: { const int p = (int) std::lround (v); return p == 50 ? std::string ("Center") : std::to_string (p); }
        case uBal: { const int p = (int) std::lround (v * 100); return p == 0 ? std::string ("Center") : (p < 0 ? "L" + std::to_string (-p) : "R" + std::to_string (p)); }
        case uNote:  return noteText ((int) std::lround (v));
        default:     return fmtNum (v, s.dec);
    }
}

// ---------------------------------------------------------------- algorithms
enum Algo { aRev6 = 0, aRev2, aRHall, aAmb, aFx, aTwin, aSamp, aPitch, aDopp, aInOut, aSAdj, aPons, aSPeq, aMPeq, aPano, aComp,
            aPrime, aFDyn, aDist, aOsc, aPink, aBin, aCHall, aRPlate, aChorus, aMBand, aHD, aLegacy, kNumAlgos };

struct AlgoDef { const char* name; int pages; PSpec p[kMaxParams]; };

// building blocks
#define K(l, n)                     PSpec { l, n, uKey }
#define B                            PSpec {}
#define N(l, n, lo, hi)              PSpec { l, n, uNum, lo, hi, 0 }
#define C(l, n, ch)                  PSpec { l, n, uChoice, 0, 1, 0, cLin, nullptr, nullptr, nullptr, ch }
#define LV(l, n)                     PSpec { l, n, uDb, -48, 0, 0, cLin, "Off", "Full" }
#define LV0(l, n)                    PSpec { l, n, uDb, -48, 0, 0, cLin, "Off" }
#define MS(l, n, hi)                 PSpec { l, n, uMs, 0, hi, 2, cSkew }
#define HZF(l, n, lo)                PSpec { l, n, uHz, lo, 21181, 0, cLog, nullptr, "Full" }
#define HZ(l, n, lo, hi)             PSpec { l, n, uHz, lo, hi, 0, cLog }
#define MIXP                         PSpec { "MIX", "Wet/Dry Mix", uPct, 0, 100, 0, cLin, "Dry", "All Fx" }
#define PCT(l, n)                    PSpec { l, n, uPct, -99, 99, 0 }
#define PAN(l, n)                    PSpec { l, n, uPan, 0, 100, 0, cLin, "Left", "Right" }
#define DBR(l, n, lo, hi, d)         PSpec { l, n, uDb, lo, hi, d, cLin, nullptr, nullptr, "0 dB" }
#define SHELF(l, n)                  PSpec { l, n, uDb, -18, 6, 1, cLin, "Full Cut", nullptr, "Flat" }
#define LVL72                        PSpec { "LVL", "Level", uDb, -72, 12, 2, cLin, "Off", nullptr, "0" }
#define FINL                         PSpec { "FIN", "Fine Level", uDb, -3.5f, 3.5f, 2, cLin, nullptr, nullptr, "0" }
#define BALP                         PSpec { "BAL", "Balance", uBal, -1, 1, 2 }
#define ROTP                         PSpec { "ROT", "Rotate", uDeg, -90, 90, 1, cLin, nullptr, nullptr, "Center" }
#define DCO_CH "R0|R1|R2|R3|R4|R5|R6|R7|R8|R9|E0|E1|E2|E3|E4|E5|E6|E7|E8|E9"
#define INV_CH "+L/+R|+L/-R|+R/+L|+R/-L"
#define Q2_CH  "SL|0.5|0.7|1.0|1.5|2.0|3.0|5.0|7.0|16|32"
#define Q3_CH  "0.5|0.7|1.0|1.5|2.0|3.0|5.0|7.0|SH"
#define ATC_CH "7ms|15ms|30ms|60ms|120ms|240ms|480ms|960ms"

#define REV_P1(rtHi) PSpec { "RTM", "Mid Reverb Time", uSec, 0.01f, 70, 2, cLog, nullptr, rtHi }, N ("SHP", "Shape", 0, 255), N ("SPR", "Spread", 0, 255), \
                     PSpec { "SIZ", "Size", uMeter, 1, 40, 0 }, HZF ("HFC", "High Freq Cutoff", 200), MS ("PDL", "Predelay", 500)
#define REV_BXR      PSpec { "BAS", "Bass Multiply", uMult, 0.2f, 4, 1, cLog }, HZ ("XOV", "Crossover", 100, 5000), HZF ("RTC", "RT High Freq Cutoff", 500), N ("DIF", "Diffusion", 0, 99)

inline const AlgoDef& algoDef (int a) {
    static const AlgoDef defs[kNumAlgos] = {
        // aRev6: Halls, Wild Spaces and the less dense Rooms - six preecho voices
        { "Reverb", 4, { REV_P1 (nullptr), REV_BXR, C ("DCO", "Decay Optimization", DCO_CH), MIXP,
                         LV ("PE1", "Echo Level 1 L>L"), LV ("PE2", "Echo Level 2 R>R"), LV ("PE3", "Echo Level 3 R>L"), LV ("PE4", "Echo Level 4 L>R"), LV ("PE5", "Echo Level 5 L>L"), LV ("PE6", "Echo Level 6 R>R"),
                         MS ("PD1", "Echo Delay 1", 800), MS ("PD2", "Echo Delay 2", 800), MS ("PD3", "Echo Delay 3", 800), MS ("PD4", "Echo Delay 4", 800), MS ("PD5", "Echo Delay 5", 800), MS ("PD6", "Echo Delay 6", 800) } },
        // aRev2: the dense variant (Rooms, Plates) - two preecho voices
        { "Reverb", 4, { REV_P1 (nullptr), REV_BXR, C ("DCO", "Decay Optimization", DCO_CH), MIXP,
                         LV ("PDE", "Echo Level 1 L>L"), LV ("PDE", "Echo Level 2 R>R"), B, B, B, B,
                         MS ("PDL", "Echo Delay 1", 800), MS ("PDL", "Echo Delay 2", 800), B, B, B, B } },
        // aRHall: Random Hall / Random Spaces
        { "Random Hall", 4, { REV_P1 ("Infinite"), REV_BXR, C ("MOD", "Mode", "Reverb|Effects"), MIXP,
                              LV ("LVL1", "Preecho Level 1 L>L"), LV ("LVL2", "Preecho Level 2 R>R"), LV ("LVL3", "Preecho Level 3 R>L"), LV ("LVL4", "Preecho Level 4 L>R"),
                              N ("SPN", "Spin", 0, 99), PSpec { "WAN", "Wander", uUsMs, 0, 40, 3, cSkew },
                              MS ("DLY1", "Preecho Delay 1", 800), MS ("DLY2", "Preecho Delay 2", 800), MS ("DLY3", "Preecho Delay 3", 800), MS ("DLY4", "Preecho Delay 4", 800),
                              N ("SHL", "Shelf", 0, 255), N ("RLV", "Reverb Level", 0, 255) } },
        // aAmb: Ambience
        { "Ambience", 2, { PSpec { "RT", "Reverb Time", uSec, 0.05f, 10, 2, cLog }, N ("RTL", "Reverb Level", 0, 127), PSpec { "SIZ", "Size", uMeter, 1, 40, 1 },
                           HZF ("ROL", "Rolloff", 200), N ("DIF", "Diffusion", 0, 99), MIXP,
                           N ("SPN", "Spin", 0, 99), PSpec { "WAN", "Wander", uUsMs, 0, 40, 3, cSkew }, MS ("PDL", "Predelay", 300), MS ("IND", "Dry Delay", 300), B,
                           PSpec { "MIX", "Wet/Dry Mix", uPct, 0, 100, 0, cLin, "Dry", "All Fx", nullptr, nullptr, 5 } } },
        // aFx: the 40-voice Effects algorithm (bank 5)
        { "Effects", 3, { N ("SPN", "Spin", 0, 99), N ("SLP", "Slope", 0, 255), MS ("LNG", "Length", 1000), PSpec { "WAN", "Wander", uUsMs, 0, 40, 3, cSkew },
                          N ("NUM", "Number", 1, 40), MS ("PDL", "Predelay", 500),
                          C ("MON", "Input Blend", "Stereo|Mono A|Mono B|Reverse"), LV0 ("FBL", "Feedback Level"), MS ("FBD", "Feedback Delay", 500), N ("DIF", "Diffusion", 0, 99),
                          MS ("IND", "Input Delay", 600), MIXP,
                          PSpec { "HPL", "High Pass L", uHz, 0, 2000, 0, cSkew }, PSpec { "HPR", "High Pass R", uHz, 0, 2000, 0, cSkew }, C ("SGN", "Signs", "0|1"), B, B, B } },
        // aTwin: Twin Delays (bank 6)
        { "Twin Delays", 4, { LV0 ("DRY", "L Ch Dry Level"), LV0 ("DRY", "R Ch Dry Level"), HZF ("ROL", "L DLY1 Rolloff", 120), HZF ("ROL", "R DLY2 Rolloff", 120),
                              PAN ("PAN", "L Channel Pan"), PAN ("PAN", "R Channel Pan"),
                              MS ("DL1", "L DLY1 Value", 1500), LV0 ("LV1", "L DLY1 Level"), PCT ("FB1", "L DLY1 Feedback"),
                              MS ("DL2", "R DLY2 Value", 1500), LV0 ("LV2", "R DLY2 Level"), PCT ("FB2", "R DLY2 Feedback"),
                              MS ("DL3", "L DLY3 Value", 1500), LV0 ("LV3", "L DLY3 Level"), PCT ("FB3", "L-R Cross Feedback"),
                              MS ("DL4", "R DLY4 Value", 1500), LV0 ("LV4", "R DLY4 Level"), PCT ("FB4", "R-L Cross Feedback"),
                              PSpec { "FIN", "L Fine Delay", uSmp, 0, 99, 0 }, PSpec { "FIN", "R Fine Delay", uSmp, 0, 99, 0 }, PSpec { "MST", "Delay Multiplier", uMult, 0, 2, 2 }, B, B, B } },
        // aSamp: Samplers (bank 7)
        { "Sampler", 4, { K ("DUB", "Overdub"), K ("REC", "Record"), PSpec { "MARK", "Mark (pre-trigger)", uMs, 0, 1000, 0, cSkew, "Head", "Tail" },
                          PSpec { "MODE", "Capture Mode", uDb, -61, 1, 0, cLin, "Immed", "Cap Key" }, K ("CAP", "Capture"), K ("CHK", "Check"),
                          PSpec { "HED", "Head Trim", uMs, 0, 20000, 0, cSkew }, PSpec { "TAL", "Tail Trim", uMs, 0, 20000, 0, cSkew }, C ("FAD", "Fade Type", "Fast|Perc"),
                          PSpec { "RAT", "Rate", uPct, -200, 199, 0 }, PSpec { "RP", "Repeat", uNum, 1, 100, 0, cLin, nullptr, "Loop" }, K ("CUE", "Cue"),
                          K ("PLAY", "Play"), PSpec { "TLV", "Play Trigger", uDb, -61, 1, 0, cLin, "Cont", "Manual" },
                          PSpec { "HOLD", "Retrigger Hold", uMs, 0, 2000, 0, cSkew, "No Retrig" }, LV0 ("LVF", "Level Forward"), PAN ("PNF", "Pan Forward"), N ("ORD", "Play Order", 0, 100),
                          LV0 ("LVR", "Level Reverse"), PAN ("PNR", "Pan Reverse"), PSpec { "LEN", "Sample Memory", uSec, 0.5f, 21.8f, 1 }, C ("CHN", "Channels", "Stereo|Mono"),
                          C ("FWR", "Play Type", "Forward|Fwd & Rev"), B } },
        // aPitch: Pitch Change (bank 8)
        { "Pitch Change", 4, { C ("MOD", "Stereo/Mono", "Stereo|Mono"), K ("SNC", "Play Sync"), PSpec { "PCH", "Pitch L", uPitch, -12, 12, 0 }, PSpec { "FIN", "Fine Pitch L", uCents, -50, 50, 0 },
                               PSpec { "PCH", "Pitch R", uPitch, -12, 12, 0 }, PSpec { "FIN", "Fine Pitch R", uCents, -50, 50, 0 },
                               MS ("PDL", "Predelay L", 800), MS ("PDR", "Predelay R", 800), PCT ("FBL", "Feedback L"), PCT ("FBR", "Feedback R"), B, MIXP,
                               PSpec { "MIDI", "MIDI Pitch L", uPitch, -24, 24, 0 }, B, PSpec { "REF", "MIDI Ref Note L", uNote, 0, 127, 0 }, C ("MIR", "Pitch Mirror L", "Off|On"),
                               PSpec { "GLL", "Glide L", uPitch, -12, 12, 2 }, N ("SPL", "Splice Time", 1, 64),
                               PSpec { "MIDI", "MIDI Pitch R", uPitch, -24, 24, 0 }, B, PSpec { "REF", "MIDI Ref Note R", uNote, 0, 127, 0 }, C ("MIR", "Pitch Mirror R", "Off|On"),
                               PSpec { "GLR", "Glide R", uPitch, -12, 12, 2 }, B } },
        // aDopp: Doppler (bank 8)
        { "Doppler", 2, { K ("PLAY", "Play Doppler"), C ("TYP", "Play Type", "Norm|Zoom"), N ("FG", "Amp Q/Pch Q", 0, 255), N ("SPD", "Speed", 0, 400),
                          PSpec { "TIM", "Time", uSec, 0.1f, 20, 1, cLog }, PSpec { "DST", "Distance Q", uMeter, 0.1f, 300, 1, cLog },
                          K ("PLAY", "Play Doppler"), B, B, B, B, PSpec { "TRIG", "Trigger Doppler", uDb, -61, 1, 0, cLin, nullptr, "Manual" } } },
        // aInOut: The In/Out - no adjustable parameters
        { "In/Out", 0, {} },
        // aSAdj: Stereo Adjust
        { "Stereo Adjust", 4, { LVL72, FINL, BALP, ROTP, SHELF ("BAS", "Bass EQ"), SHELF ("TRB", "Treble EQ"),
                                HZ ("XOV", "Bass Crossover", 50, 2000), HZ ("HFC", "Stereo Treble X-over", 1000, 20000), HZ ("HFC", "Ind Treble X-over", 1000, 20000),
                                PSpec { "SPC", "Spatial EQ", uNum, -99, 99, 0, cLin, nullptr, nullptr, "Flat" }, SHELF ("TBL", "Treble L"), SHELF ("TBR", "Treble R"),
                                C ("DLY", "11 usec Correction", "0|1"), C ("EMP", "De-emphasis", "0|1"), C ("INV", "Flip & Swap", INV_CH), B, N ("SHL", "Shelf", 0, 255), N ("SHUF", "Shuffler", 0, 99),
                                C ("AUTO", "Auto DC Cut", "-|Reset|Hold|Auto Null"), PSpec { "DCL", "DC L Offset", uPct, -100, 100, 0 }, PSpec { "DCR", "DC R Offset", uPct, -100, 100, 0 }, B, B, B } },
        // aPons: PONS Adjust
        { "PONS Adjust", 3, { LVL72, FINL, BALP, ROTP, SHELF ("TBL", "Treble Left"), SHELF ("TBR", "Treble Right"),
                              HZ ("XOV", "Bass Crossover", 50, 2000), HZ ("HFC", "Treble Crossover", 1000, 20000), PSpec { "SPC", "Spatial EQ", uNum, -99, 99, 0, cLin, nullptr, nullptr, "Flat" },
                              SHELF ("BAS", "Bass Left"), SHELF ("BAS", "Bass Right"), C ("FLP", "Flip & Swap", INV_CH),
                              N ("DIT", "Dither", 0, 99), C ("PONS", "Noise Shaping", "-"), N ("SHL", "Shelf", 0, 255), N ("SHUF", "Shuffler", 0, 99), B, B } },
        // aSPeq: Stereo Parametric EQ
        { "Stereo Param EQ", 3, { LVL72, FINL, BALP, C ("LNK", "Stereo Link", "Off|On"), PSpec { "FIN", "Fine Frequency L", uCents, -100, 100, 0 }, PSpec { "FIN", "Fine Frequency R", uCents, -100, 100, 0 },
                                  HZ ("FR1L", "Filter 1 Left", 30, 17000), C ("Q", "Filter 1 Left Q", Q2_CH), DBR ("LEV", "Filter 1 Left Level", -36, 36, 1),
                                  HZ ("FR1R", "Filter 1 Right", 30, 17000), C ("Q", "Filter 1 Right Q", Q2_CH), DBR ("LEV", "Filter 1 Right Level", -36, 36, 1),
                                  HZ ("FR2L", "Filter 2 Left", 30, 17000), C ("Q", "Filter 2 Left Q", Q3_CH), DBR ("LEV", "Filter 2 Left Level", -12, 12, 1),
                                  HZ ("FR2R", "Filter 2 Right", 30, 17000), C ("Q", "Filter 2 Right Q", Q3_CH), DBR ("LEV", "Filter 2 Right Level", -12, 12, 1) } },
        // aMPeq: Mono Parametric EQ (4 bands)
        { "Mono Param EQ", 3, { LVL72, FINL, B, B, PSpec { "FIN", "Fine Freq 1", uCents, -100, 100, 0 }, PSpec { "FIN", "Fine Freq 2", uCents, -100, 100, 0 },
                                HZ ("FRLF", "Low Frequency", 30, 17000), C ("Q", "Low Q", Q2_CH), DBR ("LEV", "Low Level", -36, 36, 1),
                                HZ ("FRLM", "Low-Mid Freq", 30, 17000), C ("Q", "Low-Mid Q", Q2_CH), DBR ("LEV", "Low-Mid Level", -36, 36, 1),
                                HZ ("FRHM", "High-Mid Freq", 30, 17000), C ("Q", "High-Mid Q", Q3_CH), DBR ("LEV", "High-Mid Level", -12, 12, 1),
                                HZ ("FRHF", "High Freq", 30, 17000), C ("Q", "High Q", Q3_CH), DBR ("LEV", "High Level", -12, 12, 1) } },
        // aPano: Panorama
        { "Panorama", 1, { N ("CH1", "Left In", 0, 254), N ("CH2", "Right In", 0, 254), N ("EFF", "Effect", 0, 254), PSpec { "ANG", "Speaker Angle", uDeg, 10, 90, 0 },
                           N ("LFB", "LF L-R Boost", 0, 254), PSpec { "MIX", "Dry Input Mix", uPct, 0, 100, 0, cLin, "Dry", "All Fx" } } },
        // aComp: Compressor/Expander (bank 10)
        { "Compressor", 2, { MS ("DLY", "Front Delay", 200), PSpec { "SLP", "Slope", uRatio, 1, 99.99f, 2, cLog }, PSpec { "GAIN", "Maximum Gain", uDb, 0, 24, 0 },
                             PSpec { "THR", "Threshold Down", uDb, -48, 48, 0 }, C ("ATC", "Attack Time Const", ATC_CH), PSpec { "RTC", "Release Time Const", uMs, 20, 2000, 0, cLog },
                             PSpec { "EXS", "Expander Slope", uRatio, 1, 10, 2, cLog }, PSpec { "EXG", "Expander Gain", uDb, 0, 48, 0 }, PSpec { "EXT", "Expander Thresh", uDb, -90, 60, 0 }, B, B, B } },
        // aPrime: Prime Time III (bank 15)
        { "Prime Time III", 3, { N ("SPD", "Speed L", 0, 300), N ("SPD", "Speed R", 0, 300), N ("MAX", "Max Pitch Shift L", 0, 30), N ("MAX", "Max Pitch Shift R", 0, 30),
                                 PSpec { "WIN", "Window L", uSmp, 1, 2000, 0, cLog }, PSpec { "WIN", "Window R", uSmp, 1, 2000, 0, cLog },
                                 MS ("PDL", "Predelay L", 700), MS ("PDL", "Predelay R", 700), MS ("IND", "Dry Delay L", 100), MS ("IND", "Dry Delay R", 100),
                                 HZ ("TRB", "Treble Cut", 1000, 21000), MIXP,
                                 PCT ("FBK", "Feedback Level L"), PCT ("FBK", "Feedback Level R"), PCT ("CFB", "Cross Feedback L"), PCT ("CFB", "Cross Feedback R"),
                                 PAN ("PAN", "Pan L"), PAN ("PAN", "Pan R") } },
        // aFDyn: Frequency Dynamics (bank 16)
        { "Freq Dynamics", 3, { N ("TSL", "Treble Slope", 0, 255), PSpec { "TBR", "Treble Rotate", uDb, -48, 48, 0 }, PSpec { "TBM", "Treble Max Gain", uDb, 0, 30, 0 },
                                HZ ("XOV", "Xover Frequency", 30, 16000), HZF ("HFC", "High Freq Cut", 1000), PSpec { "XOM", "Xover Mix", uPct, 0, 100, 0, cLin, "All Low", "All Fx" },
                                N ("LSL", "Lo Freq Comp Ratio", 0, 255), PSpec { "LFR", "Lo Freq Rotation Pt", uDb, -48, 48, 0 }, PSpec { "LMG", "Lo Freq Max Gain", uDb, 0, 30, 0 },
                                N ("ATC", "Attack Time Const", 1, 8), N ("RTC", "Release Time Const", 1, 8), MS ("DLY", "Front Delay", 30),
                                N ("GATE", "Noise Gate", 0, 99), B, B, B, B, B } },
        // aDist: Distression (bank 16)
        { "Distression", 4, { N ("DST", "Distortion Mix", 0, 255), N ("EXM", "Expansion Mix", 0, 63), HZ ("XOV", "Xover Frequency", 30, 16000), HZF ("HFC", "High Freq Cut", 1000),
                              PSpec { "XOM", "Xover Mix", uPct, 0, 100, 0, cLin, "All Low", "All Fx" }, N ("DGN", "Distortion Gain", 0, 255),
                              N ("TSL", "Treble Slope", 0, 255), PSpec { "TBR", "Treble Rotate", uDb, -48, 48, 0 }, PSpec { "TBM", "Treble Max Gain", uDb, 0, 30, 0 },
                              N ("EXP", "Expansion Slope", 0, 127), N ("SUS", "Sustain", 0, 99), HZ ("DHP", "Distortion Hi Pass", 20, 8000),
                              MS ("DLY", "Front Delay", 30), N ("LSL", "Lo Freq Comp Ratio", 0, 255), PSpec { "LFR", "Lo Freq Rotation Pt", uDb, -48, 48, 0 },
                              PSpec { "LMG", "Lo Freq Max Gain", uDb, 0, 30, 0 }, N ("ATC", "Attack Time Const", 1, 8), N ("RTC", "Release Time Const", 1, 8),
                              N ("GATE", "Noise Gate", 0, 99), B, B, B, B, B } },
        // aOsc: Oscillator (bank 17)
        { "Oscillator", 1, { PSpec { "LVL", "Level", uDb, -60, 0, 0 }, C ("DEC", "Decade", "0|1|2|3|4|5"), PSpec { "FRQ", "Frequency", uHz, 1, 24000, 2, cLog },
                             C ("WAV", "Wave Form", "Sine|Triangle|Square"), C ("ACC", "Accuracy", "0.5%|0.01%"), PSpec { "ILEV", "Input Level", uNone, 0, 1, 0 } } },
        // aPink: Pink Noise
        { "Pink Noise", 1, { PSpec { "LVL", "Level", uDb, -60, 0, 0 }, B, B, B, B, B } },
        // aBin: Binaural Simulator
        { "Binaural Simulator", 5, { LV0 ("DRY", "Dry Level"), LV0 ("WET", "Reverb Level"), LV0 ("ERL", "Early Reflections Level"),
                                     N ("DWT", "Dry Width", 0, 99), N ("WDT", "Reverb Width", 0, 99), N ("EWT", "Early Reflection Width", 0, 99),
                                     LV0 ("PE1", "Pre-Echo Level 1"), LV0 ("PE2", "Pre-Echo Level 2"), LV0 ("PE3", "Pre-Echo Level 3"), LV0 ("PE4", "Pre-Echo Level 4"), LV0 ("PE5", "Pre-Echo Level 5"), LV0 ("PE6", "Pre-Echo Level 6"),
                                     MS ("PD1", "Pre-Echo Delay 1", 300), MS ("PD2", "Pre-Echo Delay 2", 300), MS ("PD3", "Pre-Echo Delay 3", 300), MS ("PD4", "Pre-Echo Delay 4", 300), MS ("PD5", "Pre-Echo Delay 5", 300), MS ("PD6", "Pre-Echo Delay 6", 300),
                                     N ("DIF", "Diffusion", 0, 99), C ("DOF", "Diffusion Off", "0|1"), PSpec { "DWC", "Dry Treble Width", uHz, 100, 20000, 0, cLog, "Off" },
                                     PSpec { "WWC", "Reverb Treble Width", uHz, 100, 20000, 0, cLog, "Off" }, PSpec { "EWC", "Early Treble Width", uHz, 100, 20000, 0, cLog, "Off" },
                                     DBR ("CAL", "Reverb Level Calibration", -12, 12, 2),
                                     HZF ("ROL", "Early Reflection Rolloff", 500), B, B, B, B, B } },
        // aCHall: Classic Cart Concert Hall (224XL style)
        { "Concert Hall", 2, { PSpec { "BAS", "Bass Decay", uSec, 0.1f, 20, 2, cLog }, PSpec { "MID", "Mid Decay", uSec, 0.1f, 20, 2, cLog }, HZ ("XOV", "Crossover", 100, 5000),
                               HZF ("TDC", "Treble Decay", 500), N ("DEP", "Depth", 0, 99), MS ("PDL", "Predelay", 500),
                               N ("DIF", "Diffusion", 0, 99), N ("CHO", "Chorus", 0, 99), PSpec { "SIZ", "Size", uMeter, 8, 40, 0 }, HZF ("HFC", "High Freq Cutoff", 1000), B, MIXP } },
        // aRPlate: Classic Cart Rich Plate
        { "Rich Plate", 2, { PSpec { "BAS", "Bass Decay", uSec, 0.1f, 20, 2, cLog }, PSpec { "MID", "Mid Decay", uSec, 0.1f, 20, 2, cLog }, HZ ("XOV", "Crossover", 100, 5000),
                             HZF ("TDC", "Treble Decay", 500), N ("DEP", "Depth", 0, 99), MS ("PDL", "Predelay", 500),
                             N ("DIF", "Diffusion", 0, 99), N ("CHO", "Chorus", 0, 99), PSpec { "SIZ", "Size", uMeter, 8, 40, 0 }, HZF ("HFC", "High Freq Cutoff", 1000), B, MIXP } },
        // aChorus: Classic Cart 6-Voice Chorus
        { "6-Voice Chorus", 3, { PSpec { "DEP", "Chorus Depth", uMs, 0, 20, 1 }, PSpec { "RAT", "Chorus Rate", uHz, 0.05f, 10, 2, cLog }, PCT ("FBK", "Feedback"),
                                 N ("DIF", "Diffusion", 0, 99), HZF ("HFC", "High Freq Cutoff", 1000), MIXP,
                                 MS ("DL1", "Voice 1 Delay", 1500), MS ("DL2", "Voice 2 Delay", 1500), MS ("DL3", "Voice 3 Delay", 1500), MS ("DL4", "Voice 4 Delay", 1500), MS ("DL5", "Voice 5 Delay", 1500), MS ("DL6", "Voice 6 Delay", 1500),
                                 LV0 ("LV1", "Voice 1 Level"), LV0 ("LV2", "Voice 2 Level"), LV0 ("LV3", "Voice 3 Level"), LV0 ("LV4", "Voice 4 Level"), LV0 ("LV5", "Voice 5 Level"), LV0 ("LV6", "Voice 6 Level") } },
        // aMBand: Classic Cart Multiband Delays
        { "Multiband Delays", 4, { PCT ("FBK", "Feedback"), N ("RES", "Resonance", 0, 99), PSpec { "SWP", "Sweep Rate", uHz, 0, 5, 2, cSkew, "Off" },
                                   PSpec { "SWD", "Sweep Depth", uNum, 0, 3, 1 }, HZF ("HFC", "High Freq Cutoff", 1000), MIXP,
                                   MS ("DL1", "Band 1 Delay", 1500), MS ("DL2", "Band 2 Delay", 1500), MS ("DL3", "Band 3 Delay", 1500), MS ("DL4", "Band 4 Delay", 1500), MS ("DL5", "Band 5 Delay", 1500), MS ("DL6", "Band 6 Delay", 1500),
                                   LV0 ("LV1", "Band 1 Level"), LV0 ("LV2", "Band 2 Level"), LV0 ("LV3", "Band 3 Level"), LV0 ("LV4", "Band 4 Level"), LV0 ("LV5", "Band 5 Level"), LV0 ("LV6", "Band 6 Level"),
                                   HZ ("FR1", "Band 1 Freq", 60, 12000), HZ ("FR2", "Band 2 Freq", 60, 12000), HZ ("FR3", "Band 3 Freq", 60, 12000), HZ ("FR4", "Band 4 Freq", 60, 12000), HZ ("FR5", "Band 5 Freq", 60, 12000), HZ ("FR6", "Band 6 Freq", 60, 12000) } },
        // aHD: Surround/HD Reverb Cartridge (banks 22-26). Pages as in the cart's manual; no MIX (the cart is meant for a send).
        // OUT: Surround = the four-output algorithm folded to stereo, HD = the two-channel high-density algorithm.
        { "Surround/HD", 4, { LV0 ("ELV", "Early Level"), LV0 ("RLV", "Reverb Level"), PSpec { "RTM", "Mid Reverb Time", uSec, 0.1f, 20, 2, cLog },
                              N ("SPR", "Spread", 0, 255), HZF ("HFC", "High Freq Cutoff", 200), N ("LFB", "Low Freq Boost", 0, 512),
                              PSpec { "BAS", "Bass Multiply", uMult, 0.2f, 4, 1, cLog }, HZ ("XOV", "Crossover", 50, 2000), HZF ("RTC", "RT High Cut", 500),
                              N ("SPN", "Spin", 0, 99), N ("SIZ", "Size", 1, 63), MS ("RPD", "Predelay", 200),
                              MS ("LBD", "LF Boost Delay", 200), HZ ("LBF", "LF Boost Freq", 30, 500), N ("DIFF", "Diffusion", 0, 99), B, B, B,
                              MS ("ELD", "Early Delay", 100), N ("ELS", "Early Spin", 0, 99), HZF ("ELR", "Rear Early Rolloff", 500),
                              PSpec { "ELRA", "Rear Early Atten", uDb, -24, 0, 0 }, MS ("RDL", "Rear Delay Offset", 100), C ("OUT", "Output", "Surround|HD") } },
        // aLegacy: the earlier Flexiverb's reverb engine ("Classic"): its plate, or its 480-style hall tank, with its input
        // tone, converter, high cut and 4-band EQ (low shelf 250 Hz, 400 Hz, 2.5 kHz, high shelf 8 kHz)
        { "Classic Engine", 4, { PSpec { "DEC", "Decay Time", uSec, 0.05f, 20, 2, cLog }, PSpec { "LOW", "Bass Decay", uMult, 0.3f, 3, 2, cLog },
                                 PSpec { "HIGH", "Treble Decay", uMult, 0.15f, 1.5f, 2, cLog }, PSpec { "SIZE", "Size", uMult, 0.5f, 1.5f, 2 },
                                 PSpec { "PDL", "Predelay", uMs, 0, 250, 1, cSkew }, PSpec { "BLD", "Build-Up", uMs, 3, 230, 1, cLog },
                                 PSpec { "ER", "Early Reflections", uNum, 0, 1.5f, 2 }, PSpec { "DIF", "Diffusion", uNum, 0, 1, 2 },
                                 PSpec { "DMP", "Damping", uNum, 0, 1, 2 }, PSpec { "TONE", "Input Tone", uNum, 0, 1, 2 },
                                 PSpec { "MOD", "Modulation", uNum, 0, 1, 2 }, PSpec { "WID", "Input Width", uNum, 0, 1, 2 },
                                 PSpec { "SPIN", "Spin", uHz, 0.05f, 4, 2, cLog }, HZ ("XOV", "Bass Crossover", 100, 2000),
                                 PSpec { "HCUT", "High Cut", uHz, 2000, 20000, 0, cLog, nullptr, "Off" }, C ("CONV", "Converter", "24-bit|12-bit|15-bit|16-bit"), B, B,
                                 DBR ("EQ1", "EQ 250 Hz", -15, 15, 1), DBR ("EQ2", "EQ 400 Hz", -15, 15, 1), DBR ("EQ3", "EQ 2.5 kHz", -15, 15, 1), DBR ("EQ4", "EQ 8 kHz", -15, 15, 1),
                                 C ("TYPE", "Reverb Type", "Plate|Hall"), B } },
    };
    return defs[std::max (0, std::min (kNumAlgos - 1, a))];
}
#undef K
#undef B
#undef N
#undef C
#undef LV
#undef LV0
#undef MS
#undef HZF
#undef HZ
#undef MIXP
#undef PCT
#undef PAN
#undef DBR
#undef SHELF
#undef LVL72
#undef FINL
#undef BALP
#undef ROTP
#undef REV_P1
#undef REV_BXR

inline int realIndex (int algo, int i) { const auto& s = algoDef (algo).p[i]; return s.alias >= 0 ? s.alias : i; }

// ---------------------------------------------------------------- banks and programs
inline const char* bankName (int bank) {
    static const char* n[] = { "", "480 Halls", "480 Rooms", "Wild Spaces", "Plates", "Effects", "Twin Delays", "Samplers", "Pitch/Doppler", "Mastering", "Compressor",
                               "Random Hall", "Random Spaces", "Ambience", "Post Ambience", "Prime Time III", "Freq. Stuff", "Test & Reference",
                               "Concert Hall", "Rich Plate", "6-Voice Chorus", "Multiband Delays",
                               "HD Combo", "Surround", "Classic Surround", "HD Reverb", "HD Classic",
                               "Halls", "Rooms", "Spaces", "Plates", "Plates 2", "",
                               "Earlier Fit Halls", "Earlier Fit Rooms", "Earlier Fit Spaces", "Earlier Fit Plates", "Earlier Fit Plates 2" };
    return (bank >= 1 && bank <= 37) ? n[bank] : "";
}
constexpr int kNumBanks = 37;

constexpr float OFF = -1.0e9f, FULL = 1.0e9f;   // bottom / top of a parameter's range
constexpr float RV = 0, EF = 1;                 // Random Hall MOD
constexpr float R0 = 0, R6 = 6, R7 = 7, E4 = 14, E7 = 17, E9 = 19;   // decay optimisation
constexpr float ST = 0, MO = 1;                 // stereo / mono
constexpr float L_ = 0, R_ = 100;               // pan Left / Right

// trimDb: output level of the program. shape: the program's own tone shaping (dB) - low shelf 100 Hz, 400 Hz,
// 2.5 kHz, high shelf 8 kHz - applied inside its machine (used by the IR-matched presets; zero for the others)
struct Program { int bank, num; const char* name; int algo; float v[kMaxParams]; float trimDb = 0.0f; float shape[4] = { 0, 0, 0, 0 }; };

#define OFF6 OFF, OFF, OFF, OFF, OFF, OFF
#define Z6 0, 0, 0, 0, 0, 0
#define OFF4 OFF, OFF, OFF, OFF

inline void addSurroundCart (std::vector<Program>& v);

inline const std::vector<Program>& programs() {
    static const std::vector<Program> p = [] { std::vector<Program> t = {
        // ---------------- Bank 1: 480 Halls
        { 1, 1, "Large Hall",    aRev6, { 2.19f, 126, 179, 37, 2862, 24,  1.2f, 752, 4186, 99, R7, 100,  OFF6,  Z6 } },
        { 1, 2, "Large + Stage", aRev6, { 2.19f, 126, 179, 37, 2862, 24,  1.2f, 752, 4186, 99, R7, 100,  -12, -10, -8, -8, -9, -9,  16, 22, 64, 56, 112, 102 } },
        { 1, 3, "Medium Hall",   aRev6, { 1.74f, 126, 128, 25, 4395, 24,  1.2f, 752, 3982, 99, R7, 100,  OFF6,  Z6 } },
        { 1, 4, "Medium + Stage",aRev6, { 1.74f, 126, 128, 25, 4395, 24,  1.2f, 752, 3982, 99, R7, 100,  -14, -12, -10, -10, -12, -12,  16, 22, 44, 38, 80, 76 } },
        { 1, 5, "Small Hall",    aRev6, { 1.13f, 96, 50, 25, 4395, 24,  1.0f, 752, 3784, 99, R7, 100,  OFF6,  Z6 } },
        { 1, 6, "Small + Stage", aRev6, { 1.13f, 96, 50, 25, 4395, 24,  1.0f, 752, 3784, 99, R7, 100,  -12, -12, -10, -10, -14, -14,  12, 18, 44, 36, 72, 52 } },
        { 1, 7, "Large Church",  aRev6, { 4.04f, 85, 247, 38, 2523, 30,  1.5f, 1020, 2691, 80, R7, 100,  OFF6,  Z6 } },
        { 1, 8, "Small Church",  aRev6, { 2.42f, 65, 106, 31, 3402, 0,  1.0f, 752, 3591, 70, R7, 100,  OFF6,  Z6 } },
        { 1, 9, "Jazz Hall",     aRev6, { 1.26f, 34, 98, 23, 12177, 0,  1.2f, 752, 5538, 80, R7, 100,  OFF6,  Z6 } },
        { 1, 0, "Auto Park",     aRev6, { 5.29f, 149, 247, 38, 7818, 24,  1.0f, 752, 5538, 99, R0, 100,  -6, -6, -9, -9, -12, -12,  22, 12, 44, 66, 164, 136 } },
        // ---------------- Bank 2: 480 Rooms
        { 2, 1, "Music Club",    aRev2, { 1.03f, 40, 55, 25, 7181, 0,  1.0f, 752, 3784, 78, R7, 100,  OFF, OFF, 0, 0, 0, 0,  0, 0 } },
        { 2, 2, "Large Room",    aRev2, { 0.70f, 52, 82, 19, 6593, 0,  1.0f, 752, 3784, 65, R7, 100,  OFF, OFF, 0, 0, 0, 0,  0, 0 } },
        { 2, 3, "Medium Room",   aRev2, { 0.50f, 22, 10, 19, 7181, 0,  1.0f, 752, 3784, 65, R7, 100,  OFF, OFF, 0, 0, 0, 0,  0, 0 } },
        { 2, 4, "Small Room",    aRev6, { 0.31f, 16, 0, 10, 7181, 0,  1.0f, 752, 3784, 60, R6, 100,  OFF6,  Z6 } },
        { 2, 5, "Very Small Room", aRev6, { 0.13f, 8, 0, 4, 7181, 0,  1.0f, 752, 3784, 55, R0, 100,  OFF6,  Z6 } },
        { 2, 6, "Large Wood Room", aRev2, { 1.33f, 73, 34, 23, 8513, 0,  0.8f, 1158, 5538, 82, R7, 100,  OFF, OFF, 0, 0, 0, 0,  0, 0 } },
        { 2, 7, "Small Wood Room", aRev2, { 0.71f, 45, 19, 13, 8513, 0,  0.8f, 1158, 5538, 82, R7, 100,  OFF, OFF, 0, 0, 0, 0,  0, 0 } },
        { 2, 8, "Large Chamber", aRev2, { 0.88f, 3, 0, 20, 7181, 10,  1.0f, 752, 6047, 99, R6, 100,  OFF, OFF, 0, 0, 0, 0,  0, 0 } },
        { 2, 9, "Small Chamber", aRev6, { 0.36f, 16, 0, 10, 7181, 0,  1.0f, 752, 3784, 70, R6, 100,  OFF6,  Z6 } },
        { 2, 0, "Small & Bright",aRev6, { 0.65f, 40, 39, 9, 10591, 6,  0.8f, 621, 7493, 81, R7, 100,  -14, -14, -14, -14, OFF, OFF,  14, 10, 28, 44, 0, 0 } },
        // ---------------- Bank 3: Wild Spaces
        { 3, 1, "Brick Wall",    aRev6, { 0.24f, 0, 254, 26, 10591, 0,  1.5f, 1886, FULL, 88, E7, 100,  OFF6,  Z6 } },
        { 3, 2, "Buckram",       aRev2, { 0.24f, 94, 61, 24, 6882, 0,  1.5f, 1886, FULL, 98, E7, 100,  -6, -6, 0, 0, 0, 0,  64, 40 } },
        { 3, 3, "Big Bottom",    aRev6, { 0.89f, 66, 210, 31, 11084, 0,  4.0f, 243, FULL, 88, E7, 100,  OFF6,  Z6 } },
        { 3, 4, "10W-40",        aRev6, { 0.78f, 10, 88, 19, FULL, 4,  4.0f, 885, 1886, 99, E7, 100,  0, 0, -5, -3, OFF, OFF,  0, 0, 26, 46, 0, 0 } },
        { 3, 5, "20W-50",        aRev6, { 1.01f, 152, 94, 23, 11084, 4,  4.0f, 621, 621, 99, E7, 100,  0, 0, -5, -3, OFF, OFF,  0, 0, 50, 64, 0, 0 } },
        { 3, 6, "Metallica",     aRev6, { 0.97f, 57, 187, 28, 14986, 14,  1.5f, 1020, 7493, 90, E7, 100,  -7, -7, -18, -12, -18, -20,  70, 88, 136, 156, 284, 276 } },
        { 3, 7, "Silica Beads",  aRev2, { 5.46f, 126, 252, 37, 9278, 24,  0.2f, 4395, FULL, 80, E7, 100,  OFF, OFF, 0, 0, 0, 0,  64, 40 } },
        { 3, 8, "Inside Out",    aRev2, { 1.36f, 243, 112, 20, 10591, 22,  1.2f, 752, 4611, 99, E7, 100,  -14, -14, 0, 0, 0, 0,  20, 22 } },
        { 3, 9, "Ricochet",      aRev2, { 1.56f, 0, 0, 34, 14986, 18,  0.6f, 1735, 10127, 90, E7, 100,  -12, -10, 0, 0, 0, 0,  378, 322 } },
        { 3, 0, "Varoom",        aRev2, { 0.78f, 255, 216, 28, 12177, 0,  2.0f, 621, 12177, 98, E7, 100,  OFF, OFF, 0, 0, 0, 0,  14, 18 } },
        // ---------------- Bank 4: Plates
        { 4, 1, "A Plate",       aRev2, { 2.00f, 0, 0, 20, 8513, 0,  0.6f, 752, FULL, 97, R0, 100,  -8, -9, 0, 0, 0, 0,  14, 18 } },
        { 4, 2, "Snare Plate",   aRev2, { 1.84f, 1, 0, 16, FULL, 60,  0.6f, 120, FULL, 95, R0, 100,  -9, -12, 0, 0, 0, 0,  110, 152 } },
        { 4, 3, "Small Plate",   aRev2, { 1.65f, 0, 6, 18, 15886, 2,  1.0f, 885, 10127, 99, R0, 100,  -2, -4, 0, 0, 0, 0,  10, 6 } },
        { 4, 4, "Thin Plate",    aRev2, { 1.59f, 0, 0, 15, FULL, 0,  0.6f, 752, 15886, 85, R0, 100,  -6, -6, 0, 0, 0, 0,  14, 18 } },
        { 4, 5, "Fat Plate",     aRev2, { 1.98f, 97, 130, 34, 9278, 2,  1.0f, 1586, FULL, 75, R0, 100,  -6, -9, 0, 0, 0, 0,  30, 30 } },
        // ---------------- Bank 5: Effects
        { 5, 1, "Illusion",      aFx, { 30, 154, 0, 0.042f, 40, 20,  ST, OFF, 0, 64, 52, 100,  0, 0, 0 } },
        { 5, 2, "Surfin'",       aFx, { 44, 247, 0, 0.882f, 40, 20,  ST, OFF, 0, 64, 0, 100,  120, 120, 0 } },
        { 5, 3, "Vocal Whispers",aFx, { 48, 106, 38, 23, 28, 8,  ST, -16, 412, 44, 244, 76,  0, 0, 0 } },
        { 5, 4, "Doubler",       aFx, { 44, 170, 16, 26, 16, 48,  ST, -18, 10, 82, 0, 100,  0, 0, 0 } },
        { 5, 5, "Back Slap",     aFx, { 43, 219, 86, 10, 40, 38,  ST, OFF, 0, 52, 0, 100,  0, 0, 0 } },
        { 5, 6, "Rebound",       aFx, { 48, 254, 444, 35, 40, 254,  ST, -9, 0, 0, 122, 64,  0, 0, 0 } },
        { 5, 7, "Git It Wet",    aFx, { 17, 187, 80, 35, 8, 80,  ST, -12, 44, 59, 0, 100,  0, 0, 1 } },
        { 5, 8, "Sudden Stop",   aFx, { 48, 130, 216, 37, 40, 52,  ST, -18, 48, 99, 0, 100,  0, 0, 0 } },
        { 5, 9, "In the Past",   aFx, { 45, 247, 500, 13, 40, 0,  ST, OFF, 0, 87, 504, 52,  0, 0, 0 } },
        { 5, 0, "Tremolo L&R",   aFx, { 30, 135, 0, 0, 4, 0,  2, OFF, 0, 0, 0, 100,  0, 0, 0 } },
        // ---------------- Bank 6: Twin Delays
        { 6, 1, "4-Voice Double",aTwin, { OFF, OFF, FULL, FULL, L_, R_,  32.03f, 0, 0, 21.04f, 0, 0,  60.01f, -6, 0, 42.09f, 0, 0,  0, 0, 1.0f } },
        { 6, 2, "Double Delay",  aTwin, { OFF, OFF, FULL, FULL, L_, R_,  26.04f, 0, 25, 32.03f, 0, 25,  300, -4, 30, 600, -4, 30,  0, 0, 1.0f } },
        { 6, 3, "4-Bounce Delay",aTwin, { OFF, OFF, FULL, FULL, L_, R_,  100, 0, 0, 200, 0, 0,  300, -2, -30, 400, -2, 30,  0, 0, 1.0f } },
        { 6, 4, "Pitter Patter", aTwin, { OFF, OFF, 1158, 7181, L_, R_,  112.07f, -6, 0, 198.04f, -6, 0,  300, -15, 73, 954, -15, -75,  0, 0, 1.0f } },
        { 6, 5, "X-Pan Double",  aTwin, { OFF, OFF, FULL, FULL, R_, L_,  30.03f, 0, 0, 60.01f, 0, 0,  0, OFF, 0, 0, OFF, 0,  0, 0, 1.0f } },
        { 6, 6, "Delay Cave",    aTwin, { OFF, OFF, 8158, 8158, 42, 58,  21.04f, 0, 0, 42.84f, 0, 0,  100, 0, 37, 80.03f, 0, 37,  0, 0, 1.0f } },
        { 6, 7, "Circles",       aTwin, { OFF, OFF, FULL, FULL, L_, R_,  0, OFF, 0, 0, OFF, 0,  300.1f, 0, 40, 150, 0, 40,  0, 0, 0.5f } },
        { 6, 8, "There & Back",  aTwin, { OFF, OFF, 9691, 9691, L_, R_,  102, -6, 0, 198, -6, 0,  300.1f, -15, 73, 1000, -15, -75,  0, 0, 1.0f } },
        { 6, 9, "Soft Roller",   aTwin, { OFF, OFF, 6882, 6882, L_, R_,  300.1f, 0, 26, 300.1f, 0, 26,  310.8f, -4, -16, 310.8f, -4, -16,  0, 0, 1.0f } },
        { 6, 0, "On and On",     aTwin, { OFF, OFF, 5788, 5788, L_, R_,  600.8f, 0, 24, 600.8f, 0, 24,  1200, -6, 33, 1200, -6, -33,  0, 0, 1.0f } },
        // ---------------- Bank 7: Samplers (DUB REC MARK MODE CAP CHK | HED TAL FAD RAT RP CUE | PLAY TLV HOLD LVF PNF ORD | LVR PNR LEN CHN FWR)
        { 7, 1, "Stereo 3 Sec.",      aSamp, { 0, 0, 5, -24, 0, 0,  0, 0, 0, 100, 1, 0,  0, FULL, 0, 0, 50, 100,  0, 50, 2.7f, ST, 0 } },
        { 7, 2, "Mono 6 Sec.",        aSamp, { 0, 0, 5, -24, 0, 0,  0, 0, 0, 100, 1, 0,  0, FULL, 0, 0, 50, 100,  0, 50, 5.4f, MO, 0 } },
        { 7, 3, "Mono 3 Sec.",        aSamp, { 0, 0, 5, -24, 0, 0,  0, 0, 0, 100, 1, 0,  0, FULL, 0, 0, 50, 100,  0, 50, 2.7f, MO, 0 } },
        { 7, 4, "Dual Rate Change",   aSamp, { 0, 0, 5, -24, 0, 0,  0, 0, 0, 100, 1, 0,  0, FULL, 0, 0, 50, 100,  0, 50, 1.4f, MO, 0 } },
        { 7, 5, "Mono Fwd & Rev",     aSamp, { 0, 0, 5, -24, 0, 0,  0, 0, 0, 100, 1, 0,  0, FULL, 0, 0, 0, 100,  0, 100, 1.4f, MO, 1 } },
        { 7, 6, "Stereo 3S Drum",     aSamp, { 0, 0, 0, -24, 0, 0,  0, 0, 1, 100, 1, 0,  0, FULL, 0, 0, 50, 100,  0, 50, 2.7f, ST, 0 } },
        { 7, 7, "Dual Rate Chg Drum", aSamp, { 0, 0, 0, -24, 0, 0,  0, 0, 1, 100, 1, 0,  0, FULL, 0, 0, 50, 100,  0, 50, 1.4f, MO, 0 } },
        { 7, 8, "Mono Fwd Rev Drum",  aSamp, { 0, 0, 0, -24, 0, 0,  0, 0, 1, 100, 1, 0,  0, FULL, 0, 0, 0, 100,  0, 100, 1.4f, MO, 1 } },
        { 7, 9, "Stereo 10S Rate Chg",aSamp, { 0, 0, 2, -24, 0, 0,  0, 0, 0, 100, 1, 0,  0, FULL, 0, 0, 50, 100,  0, 50, 10.9f, ST, 0 } },
        { 7, 0, "Mono 20S Rate Chg",  aSamp, { 0, 0, 2, -24, 0, 0,  0, 0, 0, 100, 1, 0,  0, FULL, 0, 0, 50, 100,  0, 50, 21.8f, MO, 0 } },
        // ---------------- Bank 8: Pitch / Doppler (MOD SNC PCH FIN PCH FIN | PDL PDR FBL FBR - MIX | MIDI - REF MIR GLL SPL | MIDI - REF MIR GLR)
        { 8, 1, "Pitch Change",  aPitch, { ST, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 100,  0, 0, 48, 0, 0, 16,  0, 0, 72, 0, 0 } },
        { 8, 2, "Pitch Chorus",  aPitch, { MO, 0, 0, -3, 0, 6,  6.48f, 9.81f, 23, 13, 0, 100,  -0.03f, 0, 48, 0, -0.03f, 16,  0.06f, 0, 72, 0, 0.06f } },
        { 8, 3, "1% Up 1% Down", aPitch, { MO, 0, 0, 10, 0, -10,  39.27f, 32.69f, 10, 10, 0, 100,  0.10f, 0, 48, 0, 0.10f, 16,  -0.10f, 0, 72, 0, -0.10f } },
        { 8, 4, "Barber Pole",   aPitch, { ST, 0, 0, -3, 0, -3,  0, 0, -41, 41, 0, 100,  -0.03f, 0, 48, 0, -0.03f, 16,  -0.03f, 0, 72, 0, -0.03f } },
        { 8, 5, "Half Steps",    aPitch, { ST, 0, -1, 0, -1, 0,  600.03f, 600.03f, 44, 44, 0, 100,  -1, 0, 48, 0, -1, 16,  -1, 0, 72, 0, -1 } },
        { 8, 6, "Stair Case",    aPitch, { MO, 0, 3, 47, 4, 2,  29.03f, 30.95f, 55, -60, 0, 20,  3, 0, 48, 0, 3.47f, 16,  4, 0, 72, 0, 4.02f } },
        { 8, 7, "XPres Subway",  aDopp, { 0, 1, 89, 122, 6.2f, 35.9f,  0, 0, 0, 0, 0, FULL } },
        { 8, 8, "Indy Corner",   aDopp, { 0, 1, 226, 339, 1.2f, 5.3f,  0, 0, 0, 0, 0, FULL } },
        { 8, 9, "Airport Land",  aDopp, { 0, 0, 153, 70, 12.0f, 119.2f,  0, 0, 0, 0, 0, FULL } },
        { 8, 0, "Airport Takeoff", aDopp, { 0, 0, 213, 184, 6.8f, 268.1f,  0, 0, 0, 0, 0, FULL } },
        // ---------------- Bank 9: Mastering
        { 9, 1, "The In-Out",    aInOut, {} },
        { 9, 2, "Stereo Adjust", aSAdj, { 0, 0, 0, 0, 0, 0,  493, 5538, 5538, 0, 0, 0,  0, 0, 0, 0, 0, 0,  0, 0, 0 } },
        { 9, 3, "PONS Adjust",   aPons, { 0, 0, 0, 0, 0, 0,  367, 2040, 0, 0, 0, 0,  30, 0, 0, 0 } },
        { 9, 4, "M/S Decode",    aSAdj, { 0, 0, 0, 45, 0, 0,  493, 5538, 5538, 0, 0, 0,  0, 0, 0, 0, 0, 0,  0, 0, 0 } },
        { 9, 5, "Invert L-R",    aSAdj, { 0, 0, 0, 0, 0, 0,  493, 5538, 5538, 0, 0, 0,  0, 0, 1, 0, 0, 0,  0, 0, 0 } },
        { 9, 6, "Channel Swap",  aSAdj, { 0, 0, 0, 0, 0, 0,  493, 5538, 5538, 0, 0, 0,  0, 0, 2, 0, 0, 0,  0, 0, 0 } },
        { 9, 7, "Stereo Param EQ", aSPeq, { 0, 0, 0, 1, 0, 0,  1000, 4, 0, 1000, 4, 0,  1000, 3, 0, 1000, 3, 0 } },
        { 9, 8, "Stereo 60Hz Notch", aSPeq, { 0, 0, 0, 1, 0, 0,  60, 10, -36, 60, 10, -36,  1000, 3, 0, 1000, 3, 0 } },
        { 9, 9, "Mono Param EQ", aMPeq, { 0, 0, 0, 0, 0, 0,  1000, 4, 0, 1000, 4, 0,  1000, 3, 0, 1000, 3, 0 } },
        { 9, 0, "Panorama",      aPano, { 254, 254, 254, 56, 0, 100 } },
        // ---------------- Bank 10: Compressor (DLY SLP GAIN THR ATC RTC | EXS EXG EXT)
        { 10, 1, "2:1 Comp",     aComp, { 27.45f, 2.00f, 9, 21, 1, 91,  2.00f, 10, -48 } },
        { 10, 2, "6.24:1 Comp",  aComp, { 12.56f, 6.24f, 12, 16, 0, 91,  1.88f, 7, -40 } },
        { 10, 3, "18.28:1 Comp", aComp, { 20.38f, 18.28f, 10, 20, 1, 91,  2.00f, 10, -48 } },
        { 10, 4, "Transfer",     aComp, { 40.26f, 1.33f, 10, 10, 2, 73,  1.00f, 0, 0 } },
        { 10, 5, "Vox Easy",     aComp, { 12.56f, 5.00f, 8, 15, 0, 73,  1.88f, 7, -40 } },
        { 10, 6, "FM",           aComp, { 15.64f, 14.22f, 7, 15, 1, 58,  1.88f, 7, -40 } },
        { 10, 7, "Nailed",       aComp, { 39.27f, 21.33f, 6, 0, 0, 286,  2.00f, 10, -40 } },
        { 10, 8, "Noise Gate",   aComp, { 20.38f, 1.00f, 0, 0, 0, 37,  2.98f, 24, -30 } },
        { 10, 9, "2-Slp-Mach A", aComp, { 9.40f, 1.77f, 10, -18, 0, 181,  1.50f, 10, 50 } },
        { 10, 0, "2-Slp-Mach B", aComp, { 0, 2.41f, 4, -12, 0, 91,  1.00f, 10, 46 } },
        // ---------------- Bank 11: Random Hall (RTM SHP SPR SIZ HFC PDL | BAS XOV RTC DIF MOD MIX | L1-4 SPN WAN | D1-4 SHL RLV)
        { 11, 1, "Large R Hall",    aRHall, { 2.09f, 120, 158, 38, 3402, 0,  1.2f, 493, 3591, 65, RV, 100,  OFF4, 43, 10,  0, 0, 0, 0, 0, 160 } },
        { 11, 2, "Large R + Stage", aRHall, { 2.61f, 120, 158, 38, 3402, 0,  1.2f, 493, 3591, 65, RV, 100,  -8, -8, -7, -7, 43, 10,  14, 20, 40, 48, 0, 160 } },
        { 11, 3, "Medium R Hall",   aRHall, { 2.06f, 120, 127, 30, 3402, 0,  1.2f, 493, 3591, 68, RV, 100,  OFF4, 43, 10,  0, 0, 0, 0, 0, 160 } },
        { 11, 4, "Medium R + Stage",aRHall, { 2.06f, 120, 127, 30, 3402, 0,  1.2f, 493, 3591, 65, RV, 100,  -8, -8, -7, -7, 43, 10,  14, 20, 40, 48, 0, 160 } },
        { 11, 5, "Small R Hall",    aRHall, { 1.12f, 92, 51, 25, 4611, 0,  1.0f, 493, 3591, 65, RV, 100,  OFF4, 43, 10,  0, 0, 0, 0, 0, 160 } },
        { 11, 6, "Small R + Stage", aRHall, { 1.12f, 92, 51, 25, 4611, 0,  1.0f, 493, 3591, 65, RV, 100,  -9, -9, -8, -8, 43, 6.762f,  8, 12, 44, 36, 0, 160 } },
        { 11, 7, "Large R Church",  aRHall, { 4.07f, 82, 228, 39, 2523, 36,  1.5f, 885, 2862, 70, RV, 100,  OFF4, 43, 10,  0, 0, 0, 0, 0, 160 } },
        { 11, 8, "Small R Church",  aRHall, { 2.42f, 72, 107, 31, 3784, 16,  1.0f, 493, 3591, 65, RV, 100,  OFF4, 43, 6.762f,  0, 0, 0, 0, 0, 160 } },
        { 11, 9, "Jazz R Hall",     aRHall, { 1.32f, 34, 103, 24, 12788, 0,  1.2f, 752, 5538, 98, RV, 100,  OFF4, 38, 6.762f,  0, 0, 0, 0, 0, 160 } },
        { 11, 0, "Auto Park R",     aRHall, { 5.33f, 149, 248, 39, 7181, 24,  1.0f, 752, 5538, 98, RV, 100,  -8, -8, -12, -12, 34, 9.114f,  22, 16, 40, 48, 0, 160 } },
        // ---------------- Bank 12: Random Spaces
        { 12, 1, "Music Club R",    aRHall, { 1.06f, 40, 57, 26, 7181, 0,  1.0f, 752, 3591, 78, RV, 100,  OFF4, 43, 6.762f,  0, 0, 0, 0, 0, 160 } },
        { 12, 2, "Large Room R",    aRHall, { 0.71f, 52, 83, 20, 6593, 0,  1.2f, 493, 3591, 65, RV, 100,  OFF4, 43, 10,  0, 0, 0, 0, 0, 160 } },
        { 12, 3, "Medium Room R",   aRHall, { 0.50f, 22, 10, 20, 7493, 0,  1.2f, 493, 3591, 65, RV, 100,  OFF4, 43, 5.586f,  0, 0, 0, 0, 0, 160 } },
        { 12, 4, "Small Room R",    aRHall, { 0.32f, 16, 0, 11, 7493, 0,  1.0f, 493, 3591, 65, RV, 100,  OFF4, 43, 3.402f,  0, 0, 0, 0, 0, 160 } },
        { 12, 5, "Very Small R",    aRHall, { 0.15f, 8, 0, 5, 7493, 0,  1.0f, 493, 3591, 65, RV, 100,  OFF4, 43, 3.066f,  0, 0, 0, 0, 0, 160 } },
        { 12, 6, "Large Chamber R", aRHall, { 0.90f, 3, 0, 21, 7181, 0,  1.0f, 1158, 5538, 99, RV, 100,  OFF4, 43, 3.066f,  0, 0, 0, 0, 0, 160 } },
        { 12, 7, "Small Chamber R", aRHall, { 0.37f, 16, 0, 11, 7181, 0,  1.0f, 1158, 3591, 70, RV, 100,  OFF4, 43, 3.066f,  0, 0, 0, 0, 0, 160 } },
        { 12, 8, "Small & Bright R",aRHall, { 0.68f, 40, 41, 10, 10127, 0,  0.8f, 1158, 7493, 70, RV, 100,  OFF4, 43, 3.066f,  0, 0, 0, 0, 0, 160 } },
        { 12, 9, "Chorus Room",     aRHall, { 0.12f, 255, 12, 2, FULL, 16,  1.5f, 2198, FULL, 5, RV, 100,  -4, -4, OFF, OFF, 48, 1.302f,  22, 12, 0, 0, 0, 160 } },
        { 12, 0, "Wet & Tacky",     aRHall, { 1.39f, 23, 36, 24, 7493, 0,  0.8f, 1158, 4395, 70, RV, 100,  -14, -14, -14, -14, 48, 9.618f,  14, 18, 28, 34, 0, 160 } },
        // ---------------- Bank 13: Ambience (RT RTL SIZ ROL DIF MIX | SPN WAN PDL IND - MIX)
        { 13, 1, "Very Large Ambience", aAmb, { 3.34f, 71, 40, 2862, 75, 100,  34, 7.098f, 0, 0, 0, 100 } },
        { 13, 2, "Large Ambience",  aAmb, { 2.75f, 71, 33, 2862, 75, 100,  34, 7.098f, 0, 0, 0, 100 } },
        { 13, 3, "Medium Ambience", aAmb, { 1.77f, 71, 20, 2862, 75, 100,  34, 7.098f, 0, 0, 0, 100 } },
        { 13, 4, "Small Ambience",  aAmb, { 1.10f, 71, 13, 2862, 75, 100,  34, 7.098f, 0, 0, 0, 100 } },
        { 13, 5, "Strong Ambience", aAmb, { 0.59f, 34, 27, 3591, 56, 100,  38, 12, 10.15f, 0, 0, 100 } },
        { 13, 6, "Heavy Ambience",  aAmb, { 1.68f, 100, 23, 3591, 99, 100,  34, 7.098f, 19.96f, 0, 0, 100 } },
        { 13, 7, "Ambient Hall",    aAmb, { 3.0f, 106, 36, 2198, 94, 100,  29, 25, 27.04f, 0, 0, 100 } },
        { 13, 8, "Announcer",       aAmb, { 0.69f, 48, 10, 2862, 88, 100,  16, 1.218f, 0, 0, 0, 100 } },
        { 13, 9, "Closet",          aAmb, { 0.51f, 106, 7, 1586, 99, 100,  0, 0, 0, 0, 0, 100 } },
        { 13, 0, "Gated Ambience",  aAmb, { 0.29f, 29, 20, 6315, 51, 100,  33, 7.098f, 0, 0, 0, 100 } },
        // ---------------- Bank 14: Post Ambience
        { 14, 1, "Car Interior",    aRHall, { 0.01f, 26, 0, 1, 12177, 0,  0.6f, 4611, 8886, 0, RV, 100,  OFF4, 20, 0.651f,  2, 4, 4, 6, 0, 160 } },
        { 14, 2, "Living Room",     aRev6, { 0.24f, 24, 245, 10, 4832, 6,  1.0f, 752, 5060, 38, E4, 100,  -4, -4, -4, -4, -4, -4,  8, 12, 10, 14, 12, 8 } },
        { 14, 3, "Bathroom",        aRev6, { 0.32f, 18, 231, 7, FULL, 0,  0.4f, 120, 14986, 36, E9, 100,  0, 0, -7, -7, -8, -8,  8, 4, 14, 12, 6, 6 } },
        { 14, 4, "Kitchen Ambience",aAmb, { 0.47f, 78, 3.5f, 6315, 36, 100,  18, 0.042f, 4.65f, 29.45f, 0, 100 } },
        { 14, 5, "Kellars Cell",    aRHall, { 0.20f, 37, 12, 2, 11084, 14,  1.5f, 367, 8886, 0, RV, 100,  -3, -3, OFF, OFF, 10, 0.126f,  16, 8, 0, 0, 70, 160 } },
        { 14, 6, "Small Foley",     aAmb, { 0.18f, 87, 1.5f, 6315, 17, 100,  10, 0.672f, 4.65f, 19.65f, 0, 100 } },
        { 14, 7, "Warehouse",       aAmb, { 3.12f, 44, 40, 3591, 40, 100,  48, 3.738f, 41.26f, 0, 0, 100 } },
        { 14, 8, "Airhead",         aRHall, { 0.01f, 0, 0, 2, 9278, 0,  1.5f, 493, 6315, 29, RV, 100,  OFF4, 37, 0,  0, 0, 0, 0, 0, 160 } },
        { 14, 9, "Dial It Up",      aDist, { 255, 63, 654, 4395, 100, 224,  219, 20, 22, 14, 14, 461,  12.31f, 130, -8, 10, 1, 8,  99 } },
        { 14, 0, "Reverb Tail",     aRHall, { 32.24f, 101, 246, 39, 4611, 0,  1.2f, 243, 6315, 51, EF, 100,  OFF4, 44, 19,  0, 0, 0, 0, 128, 160 } },
        // ---------------- Bank 15: Prime Time III (SPD SPD MAX MAX WIN WIN | PDL PDL IND IND TRB MIX | FBK FBK CFB CFB PAN PAN)
        { 15, 1, "Prime Chorus",   aPrime, { 2, 19, 3, 3, 59, 31,  19.30f, 18.55f, 22.79f, 22.79f, 13400, 100,  2, -2, -11, -10, L_, R_ } },
        { 15, 2, "Slap Chorus",    aPrime, { 121, 158, 2, 3, 31, 125,  93.79f, 80.35f, 22.79f, 22.79f, 6500, 100,  24, 24, -27, -27, L_, R_ } },
        { 15, 3, "Bounce Glide",   aPrime, { 67, 28, 2, 2, 59, 62,  325.0f, 661.29f, 22.79f, 22.79f, 6000, 100,  6, 6, 70, 35, R_, L_ } },
        { 15, 4, "Swirls",         aPrime, { 3, 5, 2, 3, 51, 43,  0, 0, 8.30f, 5.62f, 5620, 100,  -39, -39, 24, -24, L_, R_ } },
        { 15, 5, "Heavy Detune",   aPrime, { 110, 85, 4, 5, 78, 74,  13.34f, 8.89f, 5.62f, 5.62f, 6500, 100,  -38, 15, -15, 38, L_, R_ } },
        { 15, 6, "Shake It Up",    aPrime, { 204, 192, 5, 7, 23, 31,  98.24f, 107.2f, 5.62f, 5.62f, 5530, 100,  0, 0, 20, 20, R_, L_ } },
        { 15, 7, "Wowza",          aPrime, { 182, 260, 4, 5, 78, 39,  0, 0, 18.30f, 5.62f, 5500, 100,  -36, 40, 40, -29, L_, R_ } },
        { 15, 8, "Wowza 2",        aPrime, { 115, 115, 4, 5, 59, 122,  10.6f, 9.44f, 5.62f, 5.62f, 7800, 100,  -38, 21, 38, -21, 70, 30 } },
        { 15, 9, "Vocalz",         aPrime, { 39, 49, 3, 4, 771, 850,  4.44f, 4.44f, 48.57f, 48.57f, 4180, 80,  -21, 21, 25, -25, L_, R_ } },
        { 15, 0, "X Flange",       aPrime, { 50, 25, 1, 1, 200, 500,  0, 0, 24.21f, 24.21f, 19400, 100,  -30, -30, -45, -45, R_, L_ } },
        // ---------------- Bank 16: Freq. Stuff (Frequency Dynamics 1-5, Distression 6-0)
        { 16, 1, "Mix Finish",     aFDyn, { 144, -20, 27, 7400, FULL, 60,  140, -21, 6, 1, 5, 8.23f,  94 } },
        { 16, 2, "BG Vocals",      aFDyn, { 141, -20, 12, 654, FULL, 54,  180, -26, 5, 1, 5, 5.15f,  99 } },
        { 16, 3, "Drums",          aFDyn, { 255, 20, 17, 5500, 12700, 43,  123, 30, 8, 1, 5, 6.07f,  87 } },
        { 16, 4, "AC Guitars",     aFDyn, { 96, 20, 27, 3000, FULL, 60,  128, -21, 6, 2, 5, 8.23f,  94 } },
        { 16, 5, "Bass Thump",     aFDyn, { 166, -15, 5, 90, 6040, 30,  248, -25, 14, 3, 7, 23.2f,  90 } },
        { 16, 6, "Carbon Mic",     aDist, { 208, 63, 181, 5500, 100, 106,  84, -17, 17, 3, 94, 304,  5.82f, 122, -19, 9, 2, 4,  99 } },
        { 16, 7, "Saxophone",      aDist, { 225, 45, 430, 5500, 52, 255,  255, 14, 20, 7, 68, 4600,  7.32f, 196, -19, 10, 3, 7,  99 } },
        { 16, 8, "Horn Blast",     aDist, { 255, 63, 60, 6300, 50, 57,  226, 15, 9, 94, 21, 90,  10.3f, 206, -23, 9, 2, 4,  99 } },
        { 16, 9, "Softener",       aDist, { 65, 63, 151, 5500, 64, 0,  190, 21, 22, 115, 0, 212,  14.9f, 82, -13, 0, 2, 8,  77 } },
        { 16, 0, "Some Fuzz",      aDist, { 255, 63, 752, 6300, 64, 66,  226, 15, 21, 0, 0, 30,  5.82f, 206, -23, 9, 2, 4,  99 } },
        // ---------------- Bank 17: Test & Reference
        { 17, 1, "100 Hz -12dB",   aOsc, { -12, 2, 100, 0, 1, 0 } },
        { 17, 2, "500 Hz -17dB",   aOsc, { -17, 2, 500, 0, 1, 0 } },
        { 17, 3, "500 Hz -12dB",   aOsc, { -12, 2, 500, 0, 1, 0 } },
        { 17, 4, "1 kHz -17dB",    aOsc, { -17, 3, 1000, 0, 1, 0 } },
        { 17, 5, "1 kHz -12dB",    aOsc, { -12, 3, 1000, 0, 1, 0 } },
        { 17, 6, "10 kHz -12dB",   aOsc, { -12, 4, 10000, 0, 1, 0 } },
        { 17, 7, "30 Hz Slate",    aOsc, { -12, 2, 30, 0, 1, 0 } },
        { 17, 8, "A-440 Tuner",    aOsc, { -12, 2, 440, 0, 1, 0 } },
        { 17, 9, "Pink Noise",     aPink, { -12 } },
        { 17, 0, "Binaural Simulator", aBin, { 0, OFF, OFF, 30, 30, 30,  -5, -5, -5, -5, -5, -5,  11.39f, 22.38f, 22.79f, 36.19f, 42.43f, 54.99f,  80, 0, 1020, 1020, 1020, -4.5f,  FULL } },
        // ---------------- Bank 18: Concert Hall (Classic Cart; Flexiverb's own settings) BAS MID XOV TDC DEP PDL | DIF CHO SIZ HFC - MIX
        { 18, 1, "Concert Hall",   aCHall, { 2.4f, 2.0f, 600, 6000, 40, 20,  70, 40, 32, 8000, 0, 100 } },
        { 18, 2, "Medium Hall",    aCHall, { 1.9f, 1.6f, 600, 6500, 40, 16,  70, 40, 26, 8000, 0, 100 } },
        { 18, 3, "Small Hall",     aCHall, { 1.4f, 1.2f, 600, 7000, 45, 10,  70, 35, 20, 8000, 0, 100 } },
        { 18, 4, "Gold Hall",      aCHall, { 2.8f, 2.2f, 500, 5000, 30, 24,  75, 45, 36, 7000, 0, 100 } },
        { 18, 5, "Guitar Hall",    aCHall, { 1.6f, 1.4f, 700, 6000, 50, 30,  60, 50, 22, 7000, 0, 100 } },
        { 18, 6, "Reggae Hall",    aCHall, { 3.2f, 2.6f, 400, 4000, 20, 60,  80, 30, 38, 6000, 0, 100 } },
        { 18, 7, "Reflex Hall",    aCHall, { 1.2f, 1.0f, 800, 8000, 70, 8,  50, 30, 18, 9000, 0, 100 } },
        { 18, 8, "Bright Hall",    aCHall, { 2.0f, 1.8f, 800, 12000, 40, 18,  70, 40, 30, 12000, 0, 100 } },
        { 18, 9, "Dark Hall",      aCHall, { 2.6f, 2.0f, 400, 3500, 40, 22,  75, 40, 32, 5000, 0, 100 } },
        { 18, 0, "Oliver's Hall",  aCHall, { 3.6f, 3.0f, 500, 6000, 35, 28,  85, 55, 40, 7500, 0, 100 } },
        // ---------------- Bank 19: Rich Plate (Classic Cart; Flexiverb's own settings)
        { 19, 1, "Large Plate",    aRPlate, { 2.6f, 2.2f, 700, 9000, 20, 10,  85, 30, 30, 10000, 0, 100 } },
        { 19, 2, "Drum Plate",     aRPlate, { 1.4f, 1.2f, 700, 10000, 10, 0,  80, 20, 20, 12000, 0, 100 } },
        { 19, 3, "Vocal Plate",    aRPlate, { 2.0f, 1.8f, 600, 8000, 15, 25,  85, 35, 26, 9000, 0, 100 } },
        { 19, 4, "Vox Plate #2",   aRPlate, { 1.7f, 1.6f, 600, 9000, 15, 40,  85, 30, 24, 10000, 0, 100 } },
        { 19, 5, "Slap Plate",     aRPlate, { 1.5f, 1.3f, 700, 9000, 10, 90,  80, 25, 22, 10000, 0, 100 } },
        { 19, 6, "Guitar Plate",   aRPlate, { 2.2f, 1.9f, 800, 7500, 15, 30,  85, 40, 28, 8000, 0, 100 } },
        { 19, 7, "Short Plate",    aRPlate, { 0.8f, 0.7f, 700, 10000, 10, 0,  75, 20, 16, 12000, 0, 100 } },
        { 19, 8, "Horn Plate",     aRPlate, { 1.9f, 1.7f, 600, 7000, 20, 20,  85, 30, 26, 8000, 0, 100 } },
        { 19, 9, "Echo Plate",     aRPlate, { 2.4f, 2.0f, 700, 8000, 15, 120,  85, 30, 28, 9000, 0, 100 } },
        { 19, 0, "Silver Plate",   aRPlate, { 2.2f, 2.0f, 800, 14000, 10, 6,  90, 30, 26, 16000, 0, 100 } },
        // ---------------- Bank 20: 6-Voice Chorus (Classic Cart; Flexiverb's own settings) DEP RAT FBK DIF HFC MIX | DL1-6 | LV1-6
        { 20, 1, "Woo-Woo",        aChorus, { 6, 0.6f, 20, 0, 10000, 100,  12, 18, 25, 31, 37, 44,  0, 0, -2, -2, -4, -4 } },
        { 20, 2, "6 Vc Korus",     aChorus, { 4, 0.8f, 0, 20, 12000, 100,  9, 13, 17, 21, 26, 31,  0, 0, 0, 0, 0, 0 } },
        { 20, 3, "3-Voice Chorus", aChorus, { 4, 0.7f, 0, 10, 12000, 100,  11, 0, 19, 0, 27, 0,  0, OFF, 0, OFF, 0, OFF } },
        { 20, 4, "4-Voice Chorus", aChorus, { 4, 0.7f, 0, 10, 12000, 100,  10, 15, 22, 27, 0, 0,  0, 0, 0, 0, OFF, OFF } },
        { 20, 5, "Ekoz & Korus",   aChorus, { 3, 0.7f, 30, 20, 8000, 100,  12, 17, 250, 375, 0, 0,  0, 0, -6, -6, OFF, OFF } },
        { 20, 6, "Vocal Chorus",   aChorus, { 3, 0.5f, 0, 30, 10000, 100,  14, 18, 23, 29, 0, 0,  0, 0, -3, -3, OFF, OFF } },
        { 20, 7, "Soft Echoes",    aChorus, { 1, 0.4f, 35, 60, 5000, 100,  180, 270, 360, 450, 0, 0,  -3, -3, -6, -6, OFF, OFF } },
        { 20, 8, "Hard Echorus",   aChorus, { 3, 0.9f, 45, 0, 12000, 100,  15, 21, 300, 450, 0, 0,  0, 0, -3, -3, OFF, OFF } },
        { 20, 9, "Canyon Korus",   aChorus, { 4, 0.3f, 55, 40, 6000, 100,  20, 28, 600, 850, 1100, 1300,  0, 0, -4, -4, -8, -8 } },
        { 20, 0, "Tuff Stuff",     aChorus, { 8, 1.5f, 40, 10, 9000, 100,  8, 12, 16, 20, 24, 28,  0, 0, 0, 0, 0, 0 } },
        // ---------------- Bank 21: Multiband Delays (Classic Cart; Flexiverb's own settings) FBK RES SWP SWD HFC MIX | DL | LV | FR
        { 21, 1, "Closet",         aMBand, { 20, 30, 0, 0, 8000, 100,  4, 7, 9, 12, 15, 18,  0, -2, -3, -4, -5, -6,  400, 800, 1500, 2500, 4000, 6000 } },
        { 21, 2, "Telephone",      aMBand, { 10, 70, 0, 0, 4000, 100,  1, 10, 20, 30, 40, 50,  0, -6, -8, -10, -12, -14,  1000, 1200, 1500, 1800, 2200, 2600 } },
        { 21, 3, "Phartage",       aMBand, { 30, 50, 0.3f, 1.0f, 6000, 100,  80, 120, 160, 200, 240, 280,  0, -2, -4, -6, -8, -10,  200, 400, 800, 1600, 3200, 6400 } },
        { 21, 4, "Stadium",        aMBand, { 25, 20, 0, 0, 6000, 100,  120, 260, 410, 590, 780, 990,  -2, -4, -6, -8, -10, -12,  300, 600, 1200, 2400, 4000, 6000 } },
        { 21, 5, "Downstairs",     aMBand, { 35, 40, 0, 0, 3000, 100,  60, 95, 140, 190, 250, 320,  0, -3, -6, -9, -12, -15,  150, 250, 400, 600, 900, 1300 } },
        { 21, 6, "Bandsweep",      aMBand, { 40, 80, 0.25f, 2.0f, 8000, 100,  100, 200, 300, 400, 500, 600,  0, 0, 0, 0, 0, 0,  500, 800, 1200, 1800, 2600, 3800 } },
        { 21, 7, "BassEchoes",     aMBand, { 45, 50, 0, 0, 2000, 100,  150, 300, 450, 600, 750, 900,  0, -2, -4, -6, -8, -10,  80, 100, 125, 160, 200, 250 } },
        { 21, 8, "BandBounce",     aMBand, { 50, 60, 0, 0, 8000, 100,  125, 250, 375, 500, 625, 750,  0, 0, -2, -2, -4, -4,  3000, 300, 2000, 500, 1200, 800 } },
        { 21, 9, "Whispers",       aMBand, { 20, 40, 0.15f, 1.0f, 10000, 100,  30, 55, 85, 120, 160, 210,  -3, -3, -6, -6, -9, -9,  4000, 5000, 6000, 7000, 8000, 9000 } },
        { 21, 0, "On Stage",       aMBand, { 15, 10, 0, 0, 9000, 100,  18, 32, 51, 77, 105, 140,  0, -3, -5, -7, -9, -11,  500, 1000, 2000, 3000, 4000, 5000 } },
        // ---------------- Banks 27-31: the Flexiverb presets, one per impulse response of a real 480L, named after the IRs.
        // Each is the better of two fits (a 480 program, or the Classic engine) to its IR: octave-band level and decay in seven
        // time windows plus echo density; then the level per band in the first 0.4 s set with the 4-band shape EQ (the last
        // four numbers), so a louder bass is a louder bass and not a longer one. The IRs themselves are not part of Flexiverb.
        // ---------------- Bank 27: Halls
        { 27, 1, "Large Hall", aRev6, { 2.19001, 184, 120, 40, 1668, 0.3645, 1.161, 1431, 9865, 5, 7, 100, -48, -48, -48, -48, -48, -48, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 2.36f, { 1.1f, 0.1f, 1.0f, -1.1f } },
        { 27, 2, "Large+Stage", aLegacy, { 2.29199, 1.40425, 0.783934, 1.13, 9.5, 15, 0.535001, 0.22, 0.08, 0.8325, 0.5, 1, 0.8, 759, 12913, 0, 0, 0, -0.585, 0.360001, 0.599999, -15, 1, 0, 0, 0, 0, 0, 0, 0 }, 7.06f, { 0.4f, -0.8f, -1.8f, -0.2f } },
        { 27, 3, "Medium Hall", aRev6, { 1.74, 186, 130, 34, 3768, 10.368, 1.18751, 691, 6718, 66, 7, 100, -48, -48, -48, -48, -48, -48, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 1.62f, { -2.8f, 1.2f, 0.1f, -3.3f } },
        { 27, 4, "Medium Hall+Stage", aLegacy, { 1.78, 1.26768, 0.803112, 1.04, 10.2957, 7.1731, 0.451, 0.37, 0.1, 0.97, 0.5, 1, 1.3535, 1186, 14827, 0, 0, 0, 3.08001, 2.03001, 0.72999, -12.19, 1, 0, 0, 0, 0, 0, 0, 0 }, 6.67f, { -3.0f, 1.0f, 0.1f, 0.7f } },
        { 27, 5, "Small Hall", aRev6, { 1.0527, 195, 78, 18, 3307, 1.568, 1.1223, 133, 12905, 39, 7, 100, -48, -48, -48, -48, -48, -48, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, -0.88f, { 5.5f, 1.5f, 2.6f, 0.2f } },
        { 27, 6, "Small+Stage", aRev6, { 1.13, 0, 0, 33, 4355, 3.9685, 1.0713, 490, 13423, 93, 7, 100, -12, -12, -10, -10, -14, -14, 12, 18, 44, 36, 72, 52, 0, 0, 0, 0, 0, 0 }, 0.79f, { -2.8f, 1.8f, -0.9f, -2.5f } },
        { 27, 7, "Large Church", aRev6, { 4.03999, 248, 255, 30, 917, 7.8125, 1.38343, 918, 7201, 37, 7, 100, -48, -48, -48, -48, -48, -48, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 2.03f, { -3.8f, -1.6f, 2.4f, 1.0f } },
        { 27, 8, "Small Church", aLegacy, { 2.47601, 1.24454, 0.673628, 1.2, 0, 11.5615, 0.142, 0.4, 0.0225, 0.652, 0.8, 1, 1.2291, 413, 15171, 0, 0, 0, -3.48999, 0.95001, 0.48, -15, 1, 0, 0, 0, 0, 0, 0, 0 }, 6.98f, { 1.3f, 0.2f, -1.0f, -2.3f } },
        { 27, 9, "Jazz Hall", aRev6, { 1.26, 0, 174, 20, 21181, 0, 1.2, 926, 8055, 32, 7, 100, -48, -48, -48, -48, -48, -48, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, -1.48f, { 4.1f, 3.1f, 2.7f, -1.9f } },
        { 27, 0, "Auto Park", aLegacy, { 6.88625, 0.838535, 0.542883, 1.015, 5.51, 21.2256, 1.155, 0.2, 0.1025, 0.84, 0.62, 1, 1.0406, 746, 12619, 0, 0, 0, 2.61, 2.97, 1.34499, -0.02001, 1, 0, 0, 0, 0, 0, 0, 0 }, 0.59f, { 0.8f, -0.1f, 1.0f, 0.6f } },
        // ---------------- Bank 28: Rooms
        { 28, 1, "Music Club", aLegacy, { 0.958003, 1.31768, 1.04128, 1.23, 0.225, 11.5615, 0.2275, 0.56, 0, 0.97, 0.5, 1, 2.29, 2000, 20000, 0, 0, 0, 0.71001, 7.5, 6.21, -5.27001, 1, 0, 0, 0, 0, 0, 0, 0 }, 4.12f, { -0.6f, -1.9f, -1.6f, -0.6f } },
        { 28, 2, "Large Room", aLegacy, { 0.718, 1.35029, 1.12249, 1.39, 0, 11.5615, 0.194501, 0.58, 0.14, 0.792, 0.5, 1, 1.0406, 211, 20000, 0, 0, 0, -3.53001, 3.45999, 1.92, -9.6, 1, 0, 0, 0, 0, 0, 0, 0 }, 9.41f, { 0.5f, -0.6f, -0.8f, -0.8f } },
        { 28, 3, "Small & Bright", aLegacy, { 0.638036, 0.990666, 1.0963, 1, 1.13, 20.3242, 0.662501, 0.67, 0.215, 0.87, 0.5, 1, 0.8, 300, 20000, 0, 0, 0, -2.73999, 0.470011, 1.22001, -1.61001, 1, 0, 0, 0, 0, 0, 0, 0 }, 9.86f, { 0.3f, 0.2f, 0.2f, 1.1f } },
        { 28, 4, "Brick Wall", aLegacy, { 0.858517, 0.639117, 1.39988, 1.092, 0, 11.3134, 0.505, 0.15, 0.025, 0.895, 0.515, 1, 0.7297, 1256, 14159, 0, 0, 0, -4.94001, -3.42999, -0.140011, -5.97999, 1, 0, 0, 0, 0, 0, 0, 0 }, 11.49f, { 1.0f, 1.7f, -2.4f, -0.9f } },
        { 28, 5, "Buck Ram", aRev2, { 0.399303, 244, 2, 36, 6302, 3.2, 1.36087, 1672, 20594, 83, 17, 100, -6, -6, 0, 0, 0, 0, 64, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, -0.54f, { -0.6f, 2.3f, 1.2f, 2.8f } },
        { 28, 6, "Big Bottom", aRev6, { 1.0255, 46, 205, 23, 20453, 0, 1.81923, 358, 17728, 42, 17, 100, -48, -48, -48, -48, -48, -48, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0.92f, { 3.5f, 3.3f, 2.5f, 2.8f } },
        { 28, 7, "10W-40", aLegacy, { 0.999, 1.49317, 0.186678, 0.94, 0, 7.49126, 1.3575, 0.4, 0.435, 0.91, 0.5, 1, 0.8, 663, 16635, 0, 0, 0, 0.210001, 0.549991, -5.78001, -1.80999, 1, 0, 0, 0, 0, 0, 0, 0 }, 13.28f, { -0.5f, 0.3f, 1.5f, 1.0f } },
        { 28, 8, "20W-50", aLegacy, { 1.035, 1.8382, 0.15707, 0.71, 0, 7.82349, 1.5, 0.22, 0.41, 0.831, 0.38, 1, 1.0047, 379, 14224, 0, 0, 0, -1.71, 2.78001, -1.41999, -1.08999, 1, 0, 0, 0, 0, 0, 0, 0 }, 11.74f, { -0.1f, -0.9f, -0.2f, 0.8f } },
        { 28, 9, "Metallica", aRev6, { 0.969996, 95, 237, 21, 20216, 0, 1.56898, 1468, 18929, 46, 17, 100, -7, -7, -18, -12, -18, -20, 70, 88, 136, 156, 284, 276, 0, 0, 0, 0, 0, 0 }, -0.47f, { 2.9f, 2.0f, 1.3f, 0.7f } },
        { 28, 0, "Sillica Beads", aRev2, { 5.45998, 28, 246, 31, 10773, 0.4805, 0.33791, 2425, 13311, 32, 17, 100, -48, -48, 0, 0, 0, 0, 64, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 2.64f, { -1.0f, -6.3f, -5.1f, -2.9f } },
        // ---------------- Bank 29: Spaces
        { 29, 1, "Inside Out", aRev2, { 1.35999, 167, 24, 22, 14408, 0.4805, 1.36088, 2208, 10832, 70, 17, 100, -14, -14, 0, 0, 0, 0, 20, 22, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 1.27f, { 6.7f, -4.6f, -4.4f, -1.5f } },
        { 29, 2, "Ricochet", aRev2, { 1.7044, 0, 78, 32, 11689, 0.3125, 0.687603, 4032, 21181, 90, 17, 100, -12, -10, 0, 0, 0, 0, 378, 322, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 2.31f, { -0.3f, 0.7f, 2.5f, 4.0f } },
        { 29, 3, "Varoom", aLegacy, { 1.0806, 1.64818, 0.25354, 0.88, 0, 6.57683, 1.455, 0.38, 0.338, 0.872, 0.38, 1, 0.9124, 440, 20000, 0, 0, 0, 0.320011, 0.519989, -4.85001, 0.179999, 1, 0, 0, 0, 0, 0, 0, 0 }, 9.63f, { -1.3f, 0.4f, 1.7f, 0.1f } },
        { 29, 4, "Very Large Ambience", aLegacy, { 2.096, 0.966312, 0.570848, 0.92, 45.5922, 21.2256, 0.5065, 0.7, 0.08, 1, 0.5, 1, 0.8, 1463, 17825, 0, 0, 0, -0.77001, 0.24999, -1.08, -8.64, 1, 0, 0, 0, 0, 0, 0, 0 }, 0.72f, { -0.2f, 0.7f, 0.1f, -0.7f } },
        { 29, 5, "Large Ambience", aLegacy, { 1.757, 0.955459, 0.552035, 0.983, 30.91, 18.6347, 0.463001, 0.67, 0.06, 0.92, 0.65, 1, 0.9449, 516, 20000, 0, 0, 0, 2.94999, 3.18999, 2.45001, -5.43, 1, 0, 0, 0, 0, 0, 0, 0 }, -2.28f, { -4.7f, 0.6f, 0.4f, 0.2f } },
        { 29, 6, "Medium Ambience", aLegacy, { 1.142, 1.0861, 0.603447, 0.96, 9.251, 35.7284, 0.673, 0.74, 0.277, 0.9, 0.612, 1, 0.9787, 2000, 20000, 0, 0, 0, 4.77999, 8.34999, 5.63001, -2.43, 1, 0, 0, 0, 0, 0, 0, 0 }, -3.83f, { -3.4f, -0.3f, -0.4f, -0.3f } },
        { 29, 7, "Small Ambience", aLegacy, { 0.689001, 1.10796, 0.724799, 0.94, 2.025, 8.17043, 0.99, 0.7025, 0.1875, 1, 0.44, 1, 1.0824, 192, 17825, 0, 0, 0, 1.71, 5.19999, 3.72999, -8.82, 1, 0, 0, 0, 0, 0, 0, 0 }, -1.76f, { -0.2f, -0.5f, -0.6f, -0.6f } },
        { 29, 8, "Strong Ambience", aLegacy, { 1.032, 0.904224, 0.700453, 1.16, 28.46, 27.5383, 0.317501, 0.62, 0.15, 0.95, 0.38, 1, 1.0406, 359, 18665, 0, 0, 0, 1.86999, 5.84001, 2.72001, -3.96999, 1, 0, 0, 0, 0, 0, 0, 0 }, -3.80f, { 0.6f, -0.5f, -0.3f, -0.8f } },
        { 29, 9, "Gated Ambience", aAmb, { 0.66985, 16, 12.98, 12335, 66, 100, 0, 0, 17.28, 20.28, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0.48f, { -2.7f, -0.4f, 0.1f, 3.5f } },
        // ---------------- Bank 30: Plates
        { 30, 1, "A Plate", aLegacy, { 1.61491, 0.676466, 1.5, 1, 0.2, 25.249, 0.7075, 0.605, 0.12, 1, 0.5, 1, 0.8, 287, 20000, 0, 0, 0, -1.94001, -2.10999, 1.08, 0.669991, 0, 0, 0, 0, 0, 0, 0, 0 }, 3.21f, { -0.6f, 0.4f, -0.7f, -0.4f } },
        { 30, 2, "Snare Plate", aLegacy, { 1.826, 0.987507, 1.39988, 1.42, 0, 9.9324, 0.0475001, 0.535, 0.08, 1, 0.328, 1, 0.615, 952, 16257, 0, 0, 0, -3.42, 1.16001, -3.72, -2.13999, 1, 0, 0, 0, 0, 0, 0, 0 }, 7.44f, { -2.6f, 2.4f, 4.5f, 3.6f } },
        { 30, 3, "Small Plate", aLegacy, { 1.609, 1.1258, 1.10152, 0.9, 0, 18.2347, 1.17, 0.73, 0.16, 1, 0.56, 1, 0.6177, 246, 16635, 0, 0, 0, -0.620009, -0.44001, -0.36, -0.24, 1, 0, 0, 0, 0, 0, 0, 0 }, 6.08f, { -2.1f, 0.9f, 0.1f, 0.9f } },
        { 30, 4, "Thin Plate", aLegacy, { 1.25499, 0.819957, 1.5, 0.91, 0, 23.1501, 0.82, 0.65, 0.09, 0.72, 0.538, 1, 0.8, 300, 20000, 0, 0, 0, -4.34001, -2.31, 1.73001, 5.33001, 0, 0, 0, 0, 0, 0, 0, 0 }, 3.41f, { 0.4f, 0.5f, -2.3f, 1.6f } },
        { 30, 5, "Fat Plate", aLegacy, { 1.942, 1.12917, 1.39988, 1.185, 0, 10.6004, 0.0775009, 0.515, 0.015, 0.9, 0.5, 1, 0.8, 979, 20000, 0, 0, 0, -0.48999, 1.70001, 0.53001, 1.22001, 1, 0, 0, 0, 0, 0, 0, 0 }, 2.70f, { 1.6f, -0.1f, -0.3f, -0.2f } },
        { 30, 6, "Large Plate", aLegacy, { 2.01243, 0.760673, 0.77111, 0.94, 7.21, 24.707, 0.587501, 0.5, 0.07, 0.9375, 0.26, 1, 0.8, 430, 20000, 0, 0, 0, -1.92, 1.455, -0.0600004, -2.46999, 0, 0, 0, 0, 0, 0, 0, 0 }, 3.65f, { 0.5f, -0.5f, -0.6f, 2.0f } },
        { 30, 7, "RP Large Plate", aLegacy, { 1.92, 0.912408, 0.874473, 1.12, 0.52, 35.7284, 0.703, 0.52, 0.4, 0.97, 0.695, 1, 0.8, 300, 20000, 0, 0, 0, -1.94001, -0.23001, -2.28999, 1.8, 0, 0, 0, 0, 0, 0, 0, 0 }, 2.13f, { -0.0f, 0.6f, 0.3f, 0.5f } },
        { 30, 8, "Drum Plate", aLegacy, { 1.3939, 0.847888, 0.5307, 0.5, 0.5063, 11.0705, 0.7375, 0.2925, 0.066, 0.82, 0.245, 1, 0.8, 212, 20000, 0, 0, 0, -7.32999, -7.70001, -10.63, -1.11999, 0, 0, 0, 0, 0, 0, 0, 0 }, 8.88f, { 1.1f, 0.4f, 1.6f, 0.1f } },
        { 30, 9, "Vocal Plate", aLegacy, { 1.499, 1.4615, 0.9875, 1.02, 16.51, 8.91125, 0.583, 0.46, 0.12, 0.96, 0.5, 1, 0.7491, 670, 20000, 0, 0, 0, -0.920011, -0.9, -1.79001, 2.88, 1, 0, 0, 0, 0, 0, 0, 0 }, 3.28f, { 0.3f, 0.3f, 0.8f, -0.5f } },
        { 30, 0, "Vox Plate No2", aLegacy, { 1.306, 1.51321, 0.922515, 0.92, 17.5333, 19.4611, 0.691001, 0.53, 0.04, 0.85, 0.38, 1, 0.5393, 660, 20000, 0, 0, 0, -4.89999, -1.53501, -2.18001, 4.10001, 1, 0, 0, 0, 0, 0, 0, 0 }, 5.08f, { 1.9f, 0.4f, -0.1f, -0.4f } },
        // ---------------- Bank 31: Plates 2
        { 31, 1, "Slap Plate", aLegacy, { 2.124, 0.482, 0.908907, 1.01, 0.0609, 10.6004, 0.601001, 0.58, 0.373, 0.846, 0.44, 1, 0.8285, 418, 15795, 0, 0, 0, 5.52999, 2.13999, 4.35, 10.08, 1, 0, 0, 0, 0, 0, 0, 0 }, -0.23f, { 0.2f, 1.0f, 0.1f, 0.8f } },
        { 31, 2, "Guitar Plate", aLegacy, { 1.641, 1.3809, 0.726966, 1.12, 26.49, 17.0856, 1.38, 0.55, 0, 0.845, 0.41, 1, 0.7758, 455, 15813, 0, 0, 0, -1.73499, 0.20001, -0.75, 5.10501, 1, 0, 0, 0, 0, 0, 0, 0 }, 2.42f, { 1.3f, 0.5f, 0.4f, -0.0f } },
        { 31, 3, "Short Plate", aLegacy, { 1.114, 1.17417, 0.926865, 1.15, 0, 10.1502, 0.6925, 0.7, 0.13, 0.93, 0.5, 1, 0.7491, 120, 20000, 0, 0, 0, 0.690001, -0.93, -1.01001, 0.600001, 1, 0, 0, 0, 0, 0, 0, 0 }, 4.99f, { 1.3f, -0.2f, -0.0f, -0.2f } },
        { 31, 4, "Horn Plate", aLegacy, { 1.18, 1.18, 0.95, 1.2, 7.0245, 21.2256, 0.385, 0.66, 0.148, 0.9825, 0.125, 1, 2.29, 258, 18345, 0, 0, 0, -0.5, -1.44999, -1.5, -1.0, 1, 0, 0, 0, 0, 0, 0, 0 }, 5.61f, { 1.0f, 0.1f, 0.4f, 1.1f } },
        { 31, 5, "Echo Plate", aRPlate, { 2.42556, 2.15397, 957, 5647, 7, 12.7837, 26, 4, 9, 15372, 0, 100, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 0.07f, { -2.5f, -1.1f, -0.8f, 4.4f } },
        { 31, 6, "Silver Plate", aLegacy, { 2.61701, 1.13964, 0.869832, 1.09, 9.89, 8.3967, 0.0355, 0.4, 0.04, 0.837, 0.5, 1, 0.7233, 331, 16635, 0, 0, 0, -4.02999, 0.179999, -0.86001, 10.97, 1, 0, 0, 0, 0, 0, 0, 0 }, 2.29f, { 1.4f, -0.7f, -0.1f, -0.8f } },
        // ---------------- Bank 32: Matched Effects
        // ---------------- Banks 33-37: 480L II - the IR-matched presets of the earlier Flexiverb (its 'Classic' reverb engine,
        // fitted to the same 480L impulse responses), unchanged, for comparison with banks 27-31 (same order: 33.1 = 27.1 ...).
        { 33, 1, "Large Hall", aLegacy, { 2.334, 1.129, 0.726, 1, 22.47, 15, 0.7, 0.2, 0, 0.63, 0.5, 1, 0.8, 300, 20000, 0, 0, 0, 1.47, 0.3, -1.98, -15, 1, 0, 0, 0, 0, 0, 0, 0 }, 7.6f },
        { 33, 2, "Large+Stage", aLegacy, { 2.292, 1.168, 0.84, 1, 9.5, 15, 0.7, 0.2, 0, 0.745, 0.5, 1, 0.8, 300, 20000, 0, 0, 0, 1.59, -0.84, -2.7, -15, 1, 0, 0, 0, 0, 0, 0, 0 }, 7.6f },
        { 33, 3, "Medium Hall", aLegacy, { 1.807, 1.113, 0.602, 1.233, 20.84, 58.9143, 0.0355, 0.4, 0, 0.804, 0.53, 1, 1.4518, 1034, 17702, 0, 0, 0, -4.04, -2.1, -4.47, -15, 1, 0, 0, 0, 0, 0, 0, 0 }, 12.1f },
        { 33, 4, "Medium Hall+Stage", aLegacy, { 1.78, 1.1041, 0.668, 1.12, 10.2957, 8.9112, 0.376, 0.3, 0, 0.888, 0.5, 1, 1.3535, 757, 20000, 0, 0, 0, 2.18, 1.58, 0.73, -12.19, 1, 0, 0, 0, 0, 0, 0, 0 }, 7.3f },
        { 33, 5, "Small Hall", aLegacy, { 1.027, 1.1339, 0.904, 1.09, 16.7342, 28.7596, 0.193, 0.3, 0, 0.784, 0.5, 1, 0.6568, 722, 18324, 0, 0, 0, -1.98, -1.49, -0.71, -15, 1, 0, 0, 0, 0, 0, 0, 0 }, 10.5f },
        { 33, 6, "Small+Stage", aLegacy, { 1.024, 1.186, 1.0607, 1.135, 5.49, 8.9112, 0.4075, 0.4, 0.099, 0.902, 0.5, 1, 1.3595, 528, 20000, 0, 0, 0, -3.96, -0.44, -1.58, -15, 1, 0, 0, 0, 0, 0, 0, 0 }, 9.6f },
        { 33, 7, "Large Church", aLegacy, { 4.181, 1.2004, 0.4104, 1.15, 39.6116, 16.0089, 0.1375, 0.3, 0, 0.784, 0.83, 1, 2.0079, 757, 20000, 0, 0, 0, 0.61, 3.09, -0.52, -12.48, 1, 0, 0, 0, 0, 0, 0, 0 }, 2.9f },
        { 33, 8, "Small Church", aLegacy, { 2.476, 1.1092, 0.5603, 1.12, 0, 11.5615, 0.172, 0.4, 0, 0.747, 0.8, 1, 1.2291, 442, 20000, 0, 0, 0, -1.09, 0.95, 1.08, -15, 1, 0, 0, 0, 0, 0, 0, 0 }, 5.3f },
        { 33, 9, "Jazz Hall", aLegacy, { 1.382, 1.123, 0.733, 1.06, 0, 11.5615, 0.2845, 0.4, 0.09, 0.97, 0.62, 1, 0.7015, 737, 20000, 0, 0, 0, -0.71, 1.12, -0.58, -4.4, 1, 0, 0, 0, 0, 0, 0, 0 }, 8.9f },
        { 33, 0, "Auto Park", aLegacy, { 4.264, 1.1575, 0.836, 0.895, 5.51, 15, 1.5, 0.2, 0, 0.963, 0.62, 1, 1.0406, 1420, 20000, 0, 0, 0, 3.21, 3.27, 6.97, -1.22, 1, 0, 0, 0, 0, 0, 0, 0 }, -2.7f },
        { 34, 1, "Music Club", aLegacy, { 0.958, 1.096, 0.8661, 1.12, 0.225, 8.9112, 0.2275, 0.5, 0, 0.97, 0.5, 1, 2.29, 1582, 20000, 0, 0, 0, 3.41, 6.9, 6.21, -4.82, 1, 0, 0, 0, 0, 0, 0, 0 }, 3.7f },
        { 34, 2, "Large Room", aLegacy, { 0.718, 0.967, 0.8515, 1.12, 0, 15, 0.1945, 0.5, 0, 0.807, 0.5, 1, 1.0406, 100, 20000, 0, 0, 0, -0.53, 3.91, -0.48, -9.6, 1, 0, 0, 0, 0, 0, 0, 0 }, 8.7f },
        { 34, 3, "Small & Bright", aLegacy, { 0.61, 0.824, 0.851, 1, 1.13, 15, 0.7, 0.7, 0, 1, 0.5, 1, 0.8, 300, 20000, 0, 0, 0, -5.59, 1.37, 1.37, -1.16, 1, 0, 0, 0, 0, 0, 0, 0 }, 8.2f },
        { 34, 4, "Brick Wall", aLegacy, { 0.5316, 0.8046, 1.1171, 0.992, 0, 16.0089, 1.105, 0.3, 0.105, 0.963, 0.515, 1, 0.7297, 903, 20000, 0, 0, 0, 5.26, 4.07, -1.04, -4.78, 1, 0, 0, 0, 0, 0, 0, 0 }, 7.5f },
        { 34, 5, "Buck Ram", aLegacy, { 0.4301, 1.5506, 1.5, 1, 0, 15, 0.025, 0.4, 0, 1, 0.26, 1, 2.6117, 219, 20000, 0, 0, 0, -2.46, 5.82, 2.95, -13.83, 1, 0, 0, 0, 0, 0, 0, 0 }, 6.6f },
        { 34, 6, "Big Bottom", aLegacy, { 0.982, 2.2972, 1.2571, 1.225, 0, 15, 0.1705, 0.4, 0, 1, 0.5, 1, 0.8249, 307, 20000, 0, 0, 0, -4.74, 1.47, 0.89, -3.16, 1, 0, 0, 0, 0, 0, 0, 0 }, 11.0f },
        { 34, 7, "10W-40", aLegacy, { 0.999, 1.1998, 0.15, 0.88, 0, 15, 1.0825, 0.4, 0.255, 0.85, 0.5, 1, 0.8, 965, 20000, 0, 0, 0, 1.71, 3.85, -8.18, 2.99, 1, 0, 0, 0, 0, 0, 0, 0 }, 10.6f },
        { 34, 8, "20W-50", aLegacy, { 1.035, 1.4269, 0.15, 0.91, 0, 13.169, 1.5, 0.3, 0.6, 0.671, 0.38, 1, 1.0047, 527, 17101, 0, 0, 0, -4.41, 0.98, -8.62, -3.49, 1, 0, 0, 0, 0, 0, 0, 0 }, 17.1f },
        { 34, 9, "Metallica", aLegacy, { 1.216, 1.404, 0.7886, 1.3, 0, 11.5615, 0.037, 0.4, 0, 0.978, 0.68, 1, 2.29, 674, 20000, 0, 0, 0, -3.49, -0.11, -1.18, -2.67, 1, 0, 0, 0, 0, 0, 0, 0 }, 8.5f },
        { 34, 0, "Sillica Beads", aLegacy, { 1.9737, 0.6763, 1.5, 1.405, 0.5063, 7.3305, 0.013, 0.3, 0, 0.82, 0.575, 1, 1.3535, 492, 19321, 0, 0, 0, -1.41, -15, -3.49, -0.75, 1, 0, 0, 0, 0, 0, 0, 0 }, 9.8f },
        { 35, 1, "Inside Out", aLegacy, { 1.329, 1.2645, 0.7353, 1.015, 0, 6.2161, 0.013, 0.5, 0, 0.971, 0.83, 1, 0.7491, 790, 18708, 0, 0, 0, -8.07, -3.91, -5.53, -11.24, 1, 0, 0, 0, 0, 0, 0, 0 }, 13.5f },
        { 35, 2, "Ricochet", aLegacy, { 0.967, 1.1352, 1.3999, 1.42, 0, 4.6478, 0.025, 0.7, 0, 0.993, 0.56, 1, 0.3882, 163, 20000, 0, 0, 0, -4.01, 0.98, 3.59, -1.62, 1, 0, 0, 0, 0, 0, 0, 0 }, 9.9f },
        { 35, 3, "Varoom", aLegacy, { 1.0806, 1.3709, 0.1637, 0.88, 0, 15, 1.5, 0.3, 0.608, 0.602, 0.38, 1, 0.9124, 826, 17419, 0, 0, 0, -2.08, 0.52, -4.85, -9.42, 1, 0, 0, 0, 0, 0, 0, 0 }, 15.7f },
        { 35, 4, "Very Large Ambience", aLegacy, { 2.096, 1.182, 1.0093, 1, 45.5922, 15, 0.9265, 0.7, 0, 0.98, 0.5, 1, 0.8, 906, 20000, 0, 0, 0, 1.63, 5.95, 3.72, -13.44, 1, 0, 0, 0, 0, 0, 0, 0 }, -5.1f },
        { 35, 5, "Large Ambience", aLegacy, { 1.757, 1.3189, 0.855, 1.083, 30.91, 15, 1.093, 0.7, 0, 1, 0.65, 1, 0.9449, 1059, 19680, 0, 0, 0, 1.15, 2.59, 7.25, -8.43, 1, 0, 0, 0, 0, 0, 0, 0 }, -4.8f },
        { 35, 6, "Medium Ambience", aLegacy, { 1.142, 1.1114, 0.9564, 0.88, 9.251, 25.249, 0.913, 0.9, 0.187, 1, 0.612, 1, 0.9787, 1481, 20000, 0, 0, 0, 4.78, 8.35, 5.63, -8.43, 1, 0, 0, 0, 0, 0, 0, 0 }, -3.5f },
        { 35, 7, "Small Ambience", aLegacy, { 0.689, 1.1872, 0.8714, 0.94, 2.025, 8.9112, 1.5, 0.6, 0, 1, 0.44, 1, 1.0824, 244, 20000, 0, 0, 0, 1.11, 7.75, 5.23, -13.62, 1, 0, 0, 0, 0, 0, 0, 0 }, -2.5f },
        { 35, 8, "Strong Ambience", aLegacy, { 1.032, 0.936, 0.7377, 1.24, 28.46, 19.4611, 0.7, 0.7, 0, 1, 0.38, 1, 1.0406, 300, 20000, 0, 0, 0, 3.07, 9.74, 7.52, -8.77, 1, 0, 0, 0, 0, 0, 0, 0 }, -7.0f },
        { 35, 9, "Gated Ambience", aAmb, { 0.6025, 12, 16.88, 3845, 99, 100, 0, 0, 17.28, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, 3.2f, { -1.1f, -1.2f, -0.3f, 4.1f } },
        { 36, 1, "A Plate", aLegacy, { 1.521, 0.596, 1.5, 1, 0.2, 15, 0.7, 0.7, 0, 1, 0.5, 1, 0.8, 300, 20000, 0, 0, 0, -2.69, -0.46, 0.18, 1.57, 0, 0, 0, 0, 0, 0, 0, 0 }, 6.1f },
        { 36, 2, "Snare Plate", aLegacy, { 1.826, 1.108, 1.4491, 1.18, 0, 12.3391, 0.0475, 0.6, 0, 0.873, 0.328, 1, 0.615, 1953, 16257, 0, 0, 0, -6.72, 2.66, -3.72, -0.64, 1, 0, 0, 0, 0, 0, 0, 0 }, 10.5f },
        { 36, 3, "Small Plate", aLegacy, { 1.609, 1.1258, 1.028, 0.82, 0, 20.77, 1.5, 0.7, 0, 0.873, 0.56, 1, 0.6177, 505, 20000, 0, 0, 0, -4.52, 0.46, 0.54, -2.64, 1, 0, 0, 0, 0, 0, 0, 0 }, 6.8f },
        { 36, 4, "Thin Plate", aLegacy, { 1.182, 0.8786, 1.4337, 0.91, 0, 25.249, 0.745, 0.7, 0.09, 0.79, 0.538, 1, 0.8, 300, 20000, 0, 0, 0, -5.54, -2.31, 1.43, 2.93, 0, 0, 0, 0, 0, 0, 0, 0 }, 7.5f },
        { 36, 5, "Fat Plate", aLegacy, { 1.942, 1.066, 1.5, 1, 0, 15, 0.7, 0.5, 0, 1, 0.5, 1, 0.8, 300, 20000, 0, 0, 0, -3.64, 1.1, 0.08, -0.28, 1, 0, 0, 0, 0, 0, 0, 0 }, 1.7f },
        { 36, 6, "Large Plate", aLegacy, { 1.924, 0.6327, 0.6304, 1.06, 7.21, 30.6938, 0.4975, 0.5, 0.038, 0.865, 0.26, 1, 0.8, 300, 20000, 0, 0, 0, -1.92, -1.17, -1.26, -2.02, 0, 0, 0, 0, 0, 0, 0, 0 }, 5.0f },
        { 36, 7, "RP Large Plate", aLegacy, { 1.92, 0.5891, 0.6335, 1.12, 0.52, 71.5401, 0.463, 0.5, 0.24, 0.97, 0.695, 1, 0.8, 300, 20000, 0, 0, 0, -1.94, -5.03, -2.89, -3, 0, 0, 0, 0, 0, 0, 0, 0 }, 5.4f },
        { 36, 8, "Drum Plate", aLegacy, { 1.3939, 0.6813, 0.5307, 0.73, 0.5063, 22.1669, 0.52, 0.3, 0.066, 0.82, 0.245, 1, 0.8, 300, 20000, 0, 0, 0, -5.38, -12.5, -13.03, -3.52, 0, 0, 0, 0, 0, 0, 0, 0 }, 12.1f },
        { 36, 9, "Vocal Plate", aLegacy, { 1.499, 1.4615, 0.8405, 0.94, 16.51, 15, 0.418, 0.5, 0, 1, 0.5, 1, 0.7491, 368, 20000, 0, 0, 0, -0.92, 1.5, 1.81, 0.48, 1, 0, 0, 0, 0, 0, 0, 0 }, 3.3f },
        { 36, 0, "Vox Plate No2", aLegacy, { 1.306, 1.4451, 0.7161, 1, 17.5333, 19.4611, 0.691, 0.5, 0, 0.88, 0.38, 1, 0.5393, 660, 20000, 0, 0, 0, -6.4, -3.41, -2.78, 2.6, 1, 0, 0, 0, 0, 0, 0, 0 }, 5.8f },
        { 37, 1, "Slap Plate", aLegacy, { 2.124, 0.3432, 1.1182, 1.12, 0.0609, 15, 0.631, 0.5, 0.293, 0.766, 0.44, 1, 0.8285, 329, 20000, 0, 0, 0, 12.13, -2.66, 6.75, 5.28, 1, 0, 0, 0, 0, 0, 0, 0 }, 1.4f },
        { 37, 2, "Guitar Plate", aLegacy, { 1.641, 1.3809, 0.7439, 1.06, 26.49, 17.0856, 1.5, 0.6, 0, 0.865, 0.41, 1, 0.7758, 578, 19012, 0, 0, 0, -3.91, 2.6, 0.45, 5.48, 1, 0, 0, 0, 0, 0, 0, 0 }, 2.8f },
        { 37, 3, "Short Plate", aLegacy, { 1.114, 0.8907, 0.865, 1.15, 0, 8.9112, 0.8125, 0.7, 0.21, 1, 0.5, 1, 0.7491, 120, 20000, 0, 0, 0, -0.51, -0.93, -1.01, 0.6, 1, 0, 0, 0, 0, 0, 0, 0 }, 6.9f },
        { 37, 4, "Horn Plate", aLegacy, { 1.19, 0.9968, 0.7955, 1.24, 7.0245, 15, 0.385, 0.7, 0.113, 0.97, 0.125, 1, 2.29, 191, 18665, 0, 0, 0, -3.41, -1.45, -2.31, -1.87, 1, 0, 0, 0, 0, 0, 0, 0 }, 6.5f },
        { 37, 5, "Echo Plate", aLegacy, { 1.683, 1.4373, 0.5345, 1.135, 15.46, 15.4154, 0.148, 0.5, 0, 0.767, 0.44, 1, 0.8, 618, 20000, 0, 0, 0, -5.36, -2.67, -2.36, 8.66, 1, 0, 0, 0, 0, 0, 0, 0 }, 4.0f },
        { 37, 6, "Silver Plate", aLegacy, { 2.617, 1.1137, 0.9004, 0.97, 9.89, 20, 0.058, 0.6, 0, 0.797, 0.5, 1, 0.7233, 245, 20000, 0, 0, 0, -2.38, 1.38, 2.74, 8.57, 1, 0, 0, 0, 0, 0, 0, 0 }, 2.8f },
    }; addSurroundCart (t); return t; }();
    return p;
}

// ---------------- Banks 22-26: the Surround/HD Reverb Cartridge. The program names and bank layout are the cart's
// (its manual; its bank 6, a copy of bank 1, is left out). The manual prints no parameter values, so these settings
// are Flexiverb's own, following its guidance: SPN about 46, SIZ about 31, RPD about 10 ms, LBD about 80 ms,
// LBF 180 Hz or lower, BAS 1.5x or less, XOV below 180 Hz, RTC about 4 kHz, ELV 3 dB lower on the HD versions.
//  ELV RLV RTM SPR HFC LFB | BAS XOV RTC SPN SIZ RPD | LBD LBF DIFF | ELD ELS ELR ELRA RDL (OUT added per bank)
inline void addSurroundCart (std::vector<Program>& v) {
    struct Voice { const char* name; float p[23]; };
    static const Voice V[] = {
        /* 0 */ { "Large SR",      { 0, -4, 2.40f, 140, 2600, 128,  1.3f, 160, 4200, 46, 34, 10,  80, 180, 60,  2, 30, 4000, -4, 12 } },
        /* 1 */ { "Small SR",      { 0, -6, 1.20f,  90, 3200,  96,  1.2f, 170, 4800, 46, 24,  8,  70, 180, 55,  0, 30, 5500, -2,  6 } },
        /* 2 */ { "Opera SR",      { 0, -5, 1.90f, 120, 2800,  96,  1.2f, 160, 4600, 46, 30, 12,  80, 170, 45,  4, 28, 4500, -3, 10 } },
        /* 3 */ { "Acoustisolo",   { 0, -8, 1.50f, 100, 3800,  64,  1.1f, 150, 5500, 44, 27,  6,  80, 160, 40,  0, 32, 6000, -2,  4 } },
        /* 4 */ { "Lil Bigger",    { 0, OFF, 0.80f, 60, 5000,   0,  1.0f, 150, 6000, 40, 22,  0,  80, 180, 50,  0, 36, 7000, -1,  2 } },
        /* 5 */ { "Bigfoot",       { -6, -4, 0.90f, 80, 1800, 512,  1.8f, 140, 3000, 46, 28,  6,  60, 140, 70,  0, 26, 3000, -4,  4 } },
        /* 6 */ { "Voc Enhance",   { 0, -6, 2.20f, 130, 4500,  64,  1.2f, 170, 5000, 46, 33, 14,  80, 180, 40,  2, 30, 5000, -3,  8 } },
        /* 7 */ { "Slap Hall",     { 0, -5, 1.70f, 120, 3000,  96,  1.2f, 160, 4200, 46, 30, 12,  80, 180, 50, 35, 30, 4500, -3, 18 } },
        /* 8 */ { "Slap Room",     { 0, -7, 0.90f,  70, 3500,  64,  1.1f, 170, 5000, 46, 20,  8,  70, 180, 50, 55, 30, 5000, -2, 20 } },
        /* 9 */ { "Chamber",       { 0, -5, 1.30f, 100, 4000,  96,  1.1f, 160, 4600, 46, 26, 10,  80, 170, 75,  2, 28, 5000, -3,  8 } },
        /*10 */ { "Church",        { 0, -3, 3.20f, 190, 2200, 160,  1.4f, 150, 3600, 46, 42, 14,  90, 160, 65,  6, 26, 3500, -5, 16 } },
        /*11 */ { "Cathedral",     { 0, -2, 5.20f, 230, 1900, 192,  1.5f, 140, 3200, 46, 52, 18, 100, 150, 70,  8, 24, 3000, -6, 20 } },
        /*12 */ { "Nice Place",    { 0, -6, 1.40f, 100, 4200,  96,  1.2f, 170, 5200, 47, 28,  8,  80, 180, 60,  0, 32, 5500, -2,  6 } },
        /*13 */ { "Percusspace",   { 0, -6, 1.60f, 110, 1600,  64,  1.0f, 160, 2400, 46, 30, 10,  80, 180, 80,  0, 30, 4000, -3,  8 } },
        /*14 */ { "Large Amb",     { 0, -8, 0.70f,  60, 4500,  64,  1.1f, 170, 6000, 46, 34,  4,  70, 180, 85,  0, 34, 6000, -2,  6 } },
        /*15 */ { "Snare Amb",     { 0, -6, 0.80f,  70, 6500,  32,  1.0f, 170, 7000, 46, 24,  4,  60, 180, 90,  0, 34, 7000, -2,  4 } },
        /*16 */ { "Gately",        { 0,  0, 0.35f, 255, 5000,  64,  1.0f, 170, 6000, 46, 30,  0,  60, 180, 99,  0, 30, 6000, -2,  6 } },
        /*17 */ { "Lead Solo",     { 0, -8, 1.10f,  90, 4500,  64,  1.1f, 170, 5000, 46, 26, 20,  80, 180, 40, 80, 35, 5000, -1, 40 } },
        /*18 */ { "Large Hall",    { 0, -3, 2.60f, 160, 2500, 128,  1.3f, 160, 3900, 46, 36, 12,  80, 170, 65,  4, 28, 4000, -4, 12 } },
        /*19 */ { "Medium Hall",   { 0, -4, 1.90f, 130, 3000, 112,  1.2f, 160, 4300, 46, 31, 10,  80, 170, 65,  2, 28, 4500, -3, 10 } },
        /*20 */ { "Small Hall",    { 0, -5, 1.30f, 100, 3400,  96,  1.2f, 170, 4700, 46, 26,  8,  70, 180, 60,  0, 30, 5000, -3,  8 } },
        /*21 */ { "Studio A",      { 0, -6, 1.00f,  80, 4800,  64,  1.1f, 170, 6000, 46, 24,  4,  70, 180, 70,  0, 32, 6000, -2,  6 } },
        /*22 */ { "Contemplate",   { 0, -4, 1.80f, 140, 7500,  64,  1.1f, 170, 8000, 47, 30, 10,  80, 180, 90,  0, 34, 8000, -2,  8 } },
        /*23 */ { "Sm Woodroom",   { 0, -7, 0.80f,  70, 3800,  64,  0.8f, 180, 5000, 46, 20,  4,  70, 180, 70,  0, 30, 5000, -2,  4 } },
        /*24 */ { "Lg Woodroom",   { 0, -6, 1.20f,  90, 3600,  80,  0.8f, 180, 4800, 46, 27,  6,  80, 180, 70,  0, 30, 4800, -3,  6 } },
        /*25 */ { "Large Chamber", { 0, -5, 1.50f, 110, 4000,  96,  1.1f, 160, 4800, 46, 28,  8,  80, 170, 75,  2, 30, 5000, -3,  8 } },
        /*26 */ { "Snare Plate",   { 0, -3, 1.40f, 120, FULL,  32,  1.0f, 170, FULL, 46, 22,  0,  60, 180, 99,  0, 34, 8000, -1,  2 } },
        /*27 */ { "Fat Plate",     { 0, -3, 2.20f, 180, 3500, 256,  1.4f, 150, 4200, 46, 33,  4,  80, 160, 99,  0, 30, 4500, -2,  4 } },
    };
    auto add = [&v] (int bank, int num, const char* name, int voice, bool hd) {
        Program p { bank, num, name, aHD, {} };
        const auto& s = V[voice].p;
        for (int i = 0; i < 15; ++i) p.v[i] = s[i];            // pages 1-3
        for (int i = 0; i < 5; ++i) p.v[18 + i] = s[15 + i];   // page 4
        p.v[23] = hd ? 1.0f : 0.0f;
        if (hd && p.v[0] > -45.0f) p.v[0] -= 3.0f;              // the HD algorithm's early level runs 3 dB hotter
        v.push_back (p);
    };
    // bank 22: Combo (surround and HD)
    add (22, 1, "Large SR", 0, false);    add (22, 2, "Small SR", 1, false);   add (22, 3, "Opera SR", 2, false);
    add (22, 4, "Acoustisolo SR", 3, false); add (22, 5, "Lil Bigger", 4, false); add (22, 6, "Bigfoot", 5, false);
    add (22, 7, "Voc Enhance", 6, false); add (22, 8, "Large HD", 0, true);    add (22, 9, "Small HD", 1, true);
    add (22, 0, "Acoustisolo HD", 3, true);
    // bank 23: Surround
    add (23, 1, "Slap Hall SR", 7, false); add (23, 2, "Slap Room SR", 8, false); add (23, 3, "Chamber SR", 9, false);
    add (23, 4, "Church SR", 10, false);  add (23, 5, "Cathedral SR", 11, false); add (23, 6, "Nice Place SR", 12, false);
    add (23, 7, "Percusspace SR", 13, false); add (23, 8, "Large Amb SR", 14, false); add (23, 9, "Gately SR", 16, false);
    add (23, 0, "Lead Solo SR", 17, false);
    // bank 24: "Classic" Surround (the first five from the 300/300L, the second five from the 480L)
    const char* cl[] = { "Large Hall SR", "Medium Hall SR", "Small Hall SR", "Studio A SR", "Contemplate SR",
                         "Sm Woodroom SR", "Lg Woodroom SR", "Large Chamber SR", "Snare Plate SR", "Fat Plate SR" };
    for (int i = 0; i < 10; ++i) add (24, (i + 1) % 10, cl[i], 18 + i, false);
    // bank 25: HD Reverb
    add (25, 1, "Slap Hall HD", 7, true);  add (25, 2, "Slap Room HD", 8, true);  add (25, 3, "Chamber HD", 9, true);
    add (25, 4, "Church HD", 10, true);   add (25, 5, "Cathedral HD", 11, true); add (25, 6, "Nice Place HD", 12, true);
    add (25, 7, "Percusspace HD", 13, true); add (25, 8, "Snare Amb HD", 15, true); add (25, 9, "Gately HD", 16, true);
    add (25, 0, "Lead Solo HD", 17, true);
    // bank 26: HD Reverb (classic)
    const char* hc[] = { "Large Hall HD", "Medium Hall HD", "Small Hall HD", "Studio A HD", "Contemplate HD",
                         "Sm Woodroom HD", "Lg Woodroom HD", "LG Chamber HD", "Snare Plate HD", "Fat Plate HD" };
    for (int i = 0; i < 10; ++i) add (26, (i + 1) % 10, hc[i], 18 + i, true);
}
#undef OFF6
#undef Z6
#undef OFF4

inline int numPrograms() { return (int) programs().size(); }
inline std::string programLabel (int i) {
    const auto& p = programs()[(size_t) std::max (0, std::min (numPrograms() - 1, i))];
    return std::to_string (p.bank) + "." + std::to_string (p.num) + " " + p.name;
}
// a program's value for slot i (factory tables give OFF/FULL for the ends of a range)
inline float programValue (const Program& p, int i) {
    const auto& s = algoDef (p.algo).p[i];
    if (! s.lab || s.unit == uKey) return 0.0f;
    const int ri = s.alias >= 0 ? s.alias : i;
    float v = p.v[ri];
    if (s.unit == uChoice) return (float) std::max (0, std::min (numChoices (s) - 1, (int) std::lround (v)));
    if (v <= OFF * 0.5f) return s.lo;
    if (v >= FULL * 0.5f) return s.hi;
    return std::min (s.hi, std::max (s.lo, v));
}
// program index of bank.num (num 0 = the tenth), or -1
inline int findProgram (int bank, int num) {
    for (int i = 0; i < numPrograms(); ++i) if (programs()[(size_t) i].bank == bank && programs()[(size_t) i].num == num) return i;
    return -1;
}

} // namespace l480
