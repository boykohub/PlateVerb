// ChannelStrip DSP engine - no JUCE dependency.
// Modules: PRE, FILTER, GATE, COMP, EQ, LIM (any order) between a fixed input stage (input mode,
// gain, polarity, broadband phase rotation) and the output trim.
// The compressor is PlateVerb's compressor (pv::Compressor from ../../Source/Engine.h), unchanged.
// EQ/preamp models are approximations built from published hardware behaviour, voiced against
// measurements of the user's own reference plugins (Nov 2026, 96 kHz, see CSModels below).
#pragma once
#include "Engine.h"     // PlateVerb: pv::Biquad, pv::OnePole, pv::Compressor, pv::gainComputer, ...
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

namespace cs {

using pv::Biquad;
using pv::clampf;
using pv::dbToGain;
constexpr float kPi = 3.14159265358979f;

enum Module { ModPre = 0, ModFilter, ModGate, ModComp, ModEq, ModLim, kNumModules };
inline const char* moduleName (int m) { static const char* n[] = { "PRE", "FILTER", "GATE", "COMP", "EQ", "LIMIT" }; return n[m]; }

// permutation <-> index (Lehmer code), so the module order is one host-saved integer parameter
inline std::array<int, kNumModules> orderFromIndex (int idx) {
    std::array<int, kNumModules> out {}; std::vector<int> pool; for (int i = 0; i < kNumModules; ++i) pool.push_back (i);
    int f = 1; for (int i = 2; i < kNumModules; ++i) f *= i;      // (n-1)!
    idx = std::max (0, std::min (idx, f * kNumModules - 1));
    for (int i = 0; i < kNumModules; ++i) {
        const int k = idx / f; idx %= f; out[(size_t) i] = pool[(size_t) k]; pool.erase (pool.begin() + k);
        if (i < kNumModules - 1) f /= (kNumModules - 1 - i);
    }
    return out;
}
inline int indexFromOrder (const std::array<int, kNumModules>& o) {
    int idx = 0; std::vector<int> pool; for (int i = 0; i < kNumModules; ++i) pool.push_back (i);
    int f = 1; for (int i = 2; i < kNumModules; ++i) f *= i;
    for (int i = 0; i < kNumModules; ++i) {
        const int k = (int) (std::find (pool.begin(), pool.end(), o[(size_t) i]) - pool.begin());
        idx += k * f; pool.erase (pool.begin() + k);
        if (i < kNumModules - 1) f /= (kNumModules - 1 - i);
    }
    return idx;
}
inline int defaultOrderIndex() { return indexFromOrder ({ ModPre, ModFilter, ModGate, ModComp, ModEq, ModLim }); }

// ---------------------------------------------------------------- small helpers
struct FirstOrder {      // generic 1st-order section through the bilinear transform (prewarped)
    float b0 = 1, b1 = 0, a1 = 0, z = 0;
    void reset() { z = 0; }
    inline float process (float x) { float y = b0 * x + z; z = b1 * x - a1 * y; return y; }
    // analog H(s) = (B1 s + B0) / (s + A0), s normalised to rad/s
    void setAnalog (float fs, float B1, float B0, float A0) {
        const float K = 2.0f * fs;
        const float n = K + A0;
        b0 = (B1 * K + B0) / n; b1 = (B0 - B1 * K) / n; a1 = (A0 - K) / n;
    }
    static float warp (float fs, float f) { f = clampf (f, 1.0f, 0.49f * fs); return 2.0f * fs * std::tan (kPi * f / fs); }
    void lowpass (float fs, float f) { const float w = warp (fs, f); setAnalog (fs, 0, w, w); }
    void highpass (float fs, float f) { const float w = warp (fs, f); setAnalog (fs, 1, 0, w); }
    // 1st-order low shelf: gain g at DC, 1 at HF, geometric mid frequency fm
    void lowShelf (float fs, float fm, float dB) {
        const float g = dbToGain (dB), w = warp (fs, fm), wp = w / std::sqrt (g), wz = w * std::sqrt (g);
        setAnalog (fs, 1, wz, wp);
    }
    void highShelf (float fs, float fm, float dB) {       // gain g at HF, 1 at DC
        const float g = dbToGain (dB), w = warp (fs, fm), wp = w * std::sqrt (g), wz = w / std::sqrt (g);
        setAnalog (fs, g, g * wz, wp);
    }
};

// one EQ stage = up to 3 sections (biquads; 1st-order ones use b2 = a2 = 0)
struct Section {
    Biquad bq; bool active = false;
    void reset() { bq.reset(); }
    inline float process (float x) { return active ? bq.process (x) : x; }
    void off() { active = false; }
    void fromFirst (const FirstOrder& f) { bq.b0 = f.b0; bq.b1 = f.b1; bq.b2 = 0; bq.a1 = f.a1; bq.a2 = 0; active = true; }
};
inline double sectionMagDb (const Biquad& q, double f, double fs) {
    const double w = 2.0 * 3.141592653589793 * f / fs;
    const double cr = std::cos (w), ci = -std::sin (w), c2r = std::cos (2 * w), c2i = -std::sin (2 * w);
    const double nr = q.b0 + q.b1 * cr + q.b2 * c2r, ni = q.b1 * ci + q.b2 * c2i;
    const double dr = 1.0 + q.a1 * cr + q.a2 * c2r, di = q.a1 * ci + q.a2 * c2i;
    return 10.0 * std::log10 ((nr * nr + ni * ni) / std::max (1e-30, dr * dr + di * di));
}

// ---------------------------------------------------------------- EQ models
enum EqModel { EqClean = 0, EqNeveFC, EqNevePA, EqSslPA, EqSslUAD, EqPultec, EqApi, kNumEqModels };
inline const char* eqModelName (int m) {
    static const char* n[] = { "Clean Digital", "British 73 - FC", "British 73/84 - PA", "E-Series 4K - PA",
                               "E-Series 4K - UAD", "Passive Tube EQP/MEQ - UAD", "American 550 - UAD" };
    return n[m];
}
inline const char* eqModelShort (int m) { static const char* n[] = { "CLEAN", "73 FC", "73/84 PA", "4K PA", "4K UAD", "TUBE EQP", "550" }; return n[m]; }

// fixed frequency points of the original units, per band (0 = low, 1 = low-mid, 2 = high-mid, 3 = high)
inline const std::vector<float>& eqSteps (int model, int band) {
    static const std::vector<float> none;
    static const std::vector<float> fcL { 35, 60, 110, 220 }, fcLM { 360, 700, 1600 }, fcHM { 3200, 4800, 7200 }, fcH { 12000 };
    static const std::vector<float> paL { 35, 60, 110, 220 }, paM { 360, 700, 1600, 3200, 4800, 7200 }, paH { 10000, 12000, 16000 };
    static const std::vector<float> puL { 20, 30, 60, 100 }, puLM { 200, 300, 500, 700, 1000 }, puHM { 1500, 2000, 3000, 4000, 5000 },
                                    puH { 3000, 4000, 5000, 8000, 10000, 12000, 16000 };
    static const std::vector<float> apL { 30, 40, 50, 100, 200, 300, 400 }, apLM { 75, 150, 180, 240, 500, 700, 1000 },
                                    apHM { 800, 1500, 3000, 5000, 8000, 10000, 12500 }, apH { 2500, 5000, 7500, 10000, 12500, 15000, 20000 };
    switch (model) {
        case EqNeveFC: return band == 0 ? fcL : band == 1 ? fcLM : band == 2 ? fcHM : fcH;
        case EqNevePA: return band == 0 ? paL : band == 3 ? paH : paM;
        case EqPultec: return band == 0 ? puL : band == 1 ? puLM : band == 2 ? puHM : puH;
        case EqApi:    return band == 0 ? apL : band == 1 ? apLM : band == 2 ? apHM : apH;
        default: return none;
    }
}
inline bool eqHasSteps (int model) { return ! eqSteps (model, 0).empty(); }
inline float snapToSteps (const std::vector<float>& s, float f) {
    if (s.empty()) return f;
    float best = s[0];
    for (float v : s) if (std::fabs (std::log (v / f)) < std::fabs (std::log (best / f))) best = v;
    return best;
}
// gain range of each model's knob (dB) and whether gains are stepped
inline float eqMaxGain (int model, int band) {
    switch (model) {
        case EqNeveFC: return 16.0f;
        case EqNevePA: return (band == 1 || band == 2) ? 18.0f : 16.0f;
        case EqSslPA: case EqSslUAD: return 15.0f;
        case EqPultec: return band == 0 ? 16.3f : band == 3 ? 20.0f : 10.0f;   // boost range
        case EqApi: return 12.0f;
        default: return 18.0f;
    }
}
inline float apiGainStep (float g) {   // 550: 0, 2, 4, 6, 9, 12 dB either way
    static const float s[] = { 0, 2, 4, 6, 9, 12 };
    const float a = std::fabs (g); float best = 0;
    for (float v : s) if (std::fabs (v - a) < std::fabs (best - a)) best = v;
    return g < 0 ? -best : best;
}
inline bool eqHasAux (int model, int band) { return model == EqPultec && (band == 0 || band == 3); }

struct BandSettings {
    bool on = true; int model = EqNeveFC; bool shelf = false; float freq = 1000, gain = 0, qScale = 1; bool stepped = true;
    float aux = 0; int auxSel = 1;      // Pultec atten (dB, <= 0) and HF atten select (5k / 10k / 20k)
};

// "native" bell Q of each model (RBJ definition, fitted to the measured reference curves)
inline float nativeBellQ (int model, int band, float gainDb) {
    switch (model) {
        case EqNeveFC: return 0.54f;
        case EqNevePA: return 0.46f;
        case EqSslPA: case EqSslUAD: return 0.44f;
        case EqPultec: return band == 3 ? 0.93f : 0.7f;
        case EqApi: return 0.35f + 0.1f * std::fabs (gainDb);      // proportional Q
        default: return 0.707f;
    }
}
inline float effectiveFreq (const BandSettings& b, int band) { return b.stepped ? snapToSteps (eqSteps (b.model, band), b.freq) : b.freq; }
inline float effectiveGain (const BandSettings& b, int band) {
    float g = clampf (b.gain, -eqMaxGain (b.model, band), eqMaxGain (b.model, band));
    if (b.model == EqApi && b.stepped) g = apiGainStep (g);
    if (b.model == EqPultec && (band == 0 || band == 3)) g = std::max (0.0f, g);   // Pultec outer bands: boost only (atten = aux)
    return g;
}
inline bool bandIsShelf (const BandSettings& b, int band) {
    if (band != 0 && band != 3) return false;
    if (b.model == EqPultec) return band == 0;     // Pultec low = shelf-type boost, high = bell boost (+ shelf atten)
    if (b.model == EqNeveFC || b.model == EqNevePA) return true;   // Neve outer bands are shelves
    return b.shelf;
}

// design one band into up to 3 sections
inline void designBand (const BandSettings& b, int band, float fs, Section* s) {
    for (int i = 0; i < 3; ++i) s[i].off();
    if (! b.on) return;
    const float f = effectiveFreq (b, band), g = effectiveGain (b, band);
    const float qs = clampf (b.qScale, 0.1f, 10.0f);
    const bool shelf = bandIsShelf (b, band), low = band == 0;
    auto peak = [&] (Section& sec, float fr, float q, float db) { if (std::fabs (db) < 0.01f) return; sec.bq.peak (fs, fr, q, db); sec.active = true; };
    auto rbjShelf = [&] (Section& sec, bool lo, float fr, float db, float S) {
        if (std::fabs (db) < 0.01f) return;
        if (lo) sec.bq.lowShelf (fs, fr, db, S); else sec.bq.highShelf (fs, fr, db, S);
        sec.active = true;
    };
    auto firstShelf = [&] (Section& sec, bool lo, float fm, float db) {
        if (std::fabs (db) < 0.01f) return;
        FirstOrder fo; if (lo) fo.lowShelf (fs, fm, db); else fo.highShelf (fs, fm, db);
        sec.fromFirst (fo);
    };
    switch (b.model) {
        case EqNeveFC:
            if (shelf) {   // measured: shelf corner x1.37 with a gentle dip ~7x above (below for HF)
                rbjShelf (s[0], low, low ? f * 1.37f : f / 1.37f, g * 1.02f, clampf (0.68f * qs, 0.1f, 1.5f));
                peak (s[1], low ? f * 7.0f : f / 7.0f, 0.66f, -0.105f * g);
            } else peak (s[0], f, nativeBellQ (b.model, band, g) * qs, g * 1.21f);   // measured overshoot of the marking
            break;
        case EqNevePA:
            if (shelf) {
                rbjShelf (s[0], low, low ? f * 2.37f : f / 2.37f, g * 0.93f, clampf (0.86f * qs, 0.1f, 1.5f));
                peak (s[1], low ? f * 10.2f : f / 10.2f, 0.81f, -0.079f * g);
                peak (s[2], low ? f * 0.64f : f / 0.64f, 1.1f, 0.06f * g);          // resonant inductor bump
            } else peak (s[0], f, nativeBellQ (b.model, band, g) * qs, g * 0.965f);
            break;
        case EqSslPA: case EqSslUAD:
            if (shelf) {
                if (std::fabs (qs - 1.0f) < 0.02f) firstShelf (s[0], low, low ? f * 2.41f : f / 2.41f, g * 1.08f);
                else rbjShelf (s[0], low, low ? f * 1.81f : f / 1.81f, g * 1.08f, clampf (0.74f * qs, 0.1f, 1.5f));
            } else peak (s[0], f, nativeBellQ (b.model, band, g) * qs, g * 1.03f);
            break;
        case EqPultec:
            if (band == 0) {      // boost (1st-order type shelf) and atten at the same CPS: the famous bump-and-dip
                firstShelf (s[0], true, f * 2.43f, g);
                if (b.aux < -0.01f) firstShelf (s[1], true, f * 2.43f * 1.9f, std::max (-17.5f, b.aux));
            } else if (band == 3) {
                peak (s[0], f, 0.93f * qs, g * 0.993f);
                if (b.aux < -0.01f) {
                    static const float att[] = { 5000, 10000, 20000 };
                    firstShelf (s[1], false, att[std::max (0, std::min (2, b.auxSel))] / 1.6f, std::max (-16.0f, b.aux));
                }
            } else peak (s[0], f, (g < 0 ? 0.8f : 0.7f) * qs, g);       // MEQ-5 style peak / dip
            break;
        case EqApi:
            if (shelf) { rbjShelf (s[0], low, f, g, clampf (0.9f * qs, 0.1f, 1.5f)); peak (s[1], low ? f * 3.5f : f / 3.5f, 0.9f, -0.06f * g); }
            else peak (s[0], f, nativeBellQ (b.model, band, g) * qs, g);
            break;
        default:
            if (shelf) rbjShelf (s[0], low, f, g, clampf (0.707f * qs, 0.1f, 1.5f));
            else peak (s[0], f, 0.707f * qs, g);
    }
}

// measured harmonic colour of each EQ model at -18 dBFS (drive 50 %): c2 = 2nd-order, c3 = 3rd-order
// (whole band), c3lf = 3rd-order on the lows only (transformer core).
struct EqColour { float c2, c3, c3lf; };
inline EqColour eqColour (int model) {
    switch (model) {
        case EqNevePA: return { 3.0e-4f, 0.0f, 0.28f };    // H3 -59 dB at 100 Hz, rising at LF only
        case EqSslPA:  return { 0.0f, 0.0159f, 0.0f };     // H3 -84 dB @ -18, -60 dB @ -6 (VCA, symmetric)
        case EqSslUAD: return { 2.5e-3f, 0.0f, 0.0f };     // H2 -76 dB @ -18, -64 dB @ -6
        case EqPultec: return { 8.9e-4f, 0.0f, 0.0f };     // tube make-up: H2 -85 / -73 dB
        case EqApi:    return { 0.0f, 0.01f, 0.02f };
        default:       return { 0, 0, 0 };                  // FC vintage and clean null at flat
    }
}

// ---------------------------------------------------------------- nonlinear stages
inline float softCubic (float x, float c3) {      // ~ x - c3 x^3 for small x, bounded
    if (c3 <= 0.0f) return x;
    const float g = std::sqrt (3.0f * c3);
    return std::tanh (g * x) / g;
}
inline float softSquare (float x, float c2) {     // ~ x + c2 x^2, bounded (DC removed afterwards)
    if (c2 == 0.0f) return x;
    const float q = x * x; return x + c2 * q / (1.0f + std::fabs (c2) * q);
}

struct ColourStage {
    float fs = 48000; float c2 = 0, c3 = 0, c3lf = 0;
    pv::OnePole lf[2]; Biquad dc[2];
    void prepare (float f) { fs = f; for (int c = 0; c < 2; ++c) { lf[c].setCut (fs, 160.0f); dc[c].highpass (fs, 6.0f, 0.707f); } }
    void reset() { for (int c = 0; c < 2; ++c) { lf[c].reset(); dc[c].reset(); } }
    void set (EqColour k, float amount) { c2 = k.c2 * amount; c3 = k.c3 * amount; c3lf = k.c3lf * amount; }
    inline float process (int ch, float x) {
        if (c2 == 0.0f && c3 == 0.0f && c3lf == 0.0f) return x;
        float y = x;
        if (c3lf > 0.0f) { const float l = lf[ch].lp (x); y += softCubic (l, c3lf) - l; }
        y = softCubic (y, c3);
        if (c2 != 0.0f) y = dc[ch].process (softSquare (y, c2));
        return y;
    }
};

// ---------------------------------------------------------------- preamp
enum PreModel { PreClean = 0, PreTube, Pre1073, Pre1080, Pre212, kNumPreModels };
inline const char* preModelName (int m) {
    static const char* n[] = { "Clean", "Tube Mic Pre (Voxbox-style)", "British 1073-style", "British 1080-style", "American 212-style" };
    return n[m];
}
struct Preamp {
    float fs = 48000; int model = PreTube; float drive = 1, out = 1;
    Biquad hp[2], lp[2]; pv::OnePole lf[2]; Biquad dc[2];
    void prepare (float f) { fs = f; set (model, 0, 0); reset(); }
    void reset() { for (int c = 0; c < 2; ++c) { hp[c].reset(); lp[c].reset(); lf[c].reset(); dc[c].reset(); } }
    void set (int m, float driveDb, float outDb) {
        model = m; drive = dbToGain (driveDb); out = dbToGain (outDb);
        float hpF = 5, lpF = 0.45f * fs;
        switch (m) {
            case PreTube: hpF = 8; lpF = 45000; break;
            case Pre1073: hpF = 10; lpF = 40000; break;     // 1073 spec: -3 dB at 40 kHz
            case Pre1080: hpF = 9; lpF = 50000; break;
            case Pre212:  hpF = 7; lpF = 70000; break;
            default: break;
        }
        for (int c = 0; c < 2; ++c) {
            hp[c].highpass (fs, hpF, 0.707f); lp[c].lowpass (fs, std::min (lpF, 0.45f * fs), 0.707f);
            lf[c].setCut (fs, 150.0f); dc[c].highpass (fs, 5.0f, 0.707f);
        }
    }
    inline float process (int c, float x) {
        if (model == PreClean) return x * drive * out;
        float u = x * drive * 0.4f;                         // 0 dBFS ~ +22 dBu into a stage that clips ~ +30 dBu
        switch (model) {
            case PreTube: u = pv::warmSat (u, 1.0f, 0.15f); break;                               // triode: mostly H2
            case Pre1073: { const float l = lf[c].lp (u); u += softCubic (l, 0.9f) - l; u = softSquare (softCubic (u, 0.05f), 0.02f); break; }
            case Pre1080: { const float l = lf[c].lp (u); u += softCubic (l, 0.5f) - l; u = softSquare (softCubic (u, 0.04f), 0.012f); break; }
            case Pre212:  { const float l = lf[c].lp (u); u += softCubic (l, 0.3f) - l; u = softCubic (u, 0.12f); break; }
            default: break;
        }
        float y = dc[c].process (u) / 0.4f;
        y = lp[c].process (hp[c].process (y));
        return y * out;                                   // drive is compensated: output level ~ input level
    }
};

// ---------------------------------------------------------------- filters (HPF / LPF)
struct CutFilter {
    // Butterworth (clean) or vintage (slight resonance) cascades: slope 6..36 dB/oct
    Biquad s[2][3]; FirstOrder o[2]; int nBq = 0; bool odd = false; bool active = false;
    void reset() { for (int c = 0; c < 2; ++c) { for (auto& q : s[c]) q.reset(); o[c].reset(); } }
    void set (bool hp, bool on, float fs, float f, int slopeDb, bool vintage) {
        active = on; if (! on) return;
        const int order = std::max (1, slopeDb / 6);
        nBq = order / 2; odd = (order % 2) == 1;
        for (int c = 0; c < 2; ++c) {
            for (int k = 0; k < nBq; ++k) {
                // Butterworth section Q: 1 / (2 sin((2k+1) pi / 2N))
                float q = 1.0f / (2.0f * std::sin ((2.0f * (float) k + 1.0f) * kPi / (2.0f * (float) order)));
                if (vintage && k == 0) q *= 1.25f;
                if (hp) s[c][k].highpass (fs, f, q); else s[c][k].lowpass (fs, f, q);
            }
            if (odd) { if (hp) o[c].highpass (fs, f); else o[c].lowpass (fs, f); }
        }
    }
    inline float process (int c, float x) {
        if (! active) return x;
        for (int k = 0; k < nBq; ++k) x = s[c][k].process (x);
        return odd ? o[c].process (x) : x;
    }
};

// ---------------------------------------------------------------- gate / expander
struct Gate {
    float fs = 48000; float env = 0, gain = 1, holdCnt = 0; bool open = false;
    Biquad khp[2], klp[2]; float grDb = 0;
    void prepare (float f) { fs = f; reset(); }
    void reset() { env = 0; gain = 1; holdCnt = 0; open = false; for (int c = 0; c < 2; ++c) { khp[c].reset(); klp[c].reset(); } }
    void setKey (float hpHz, float lpHz) { for (int c = 0; c < 2; ++c) { khp[c].highpass (fs, hpHz, 0.707f); klp[c].lowpass (fs, lpHz, 0.707f); } }
    struct P { bool expander = false; float thr = -50, range = 40, atkMs = 0.5f, holdMs = 50, relMs = 150, hyst = 3; bool listen = false; };
    inline void process (float& L, float& R, float kL, float kR, const P& p) {
        const float fl = klp[0].process (khp[0].process (kL)), fr = klp[1].process (khp[1].process (kR));
        if (p.listen) { L = fl; R = fr; grDb = 0; return; }
        const float lvl = std::max (std::fabs (fl), std::fabs (fr));
        const float rE = std::exp (-1.0f / (0.010f * fs));
        env = lvl > env ? lvl : rE * env + (1 - rE) * lvl;
        const float db = 20.0f * std::log10 (env + 1e-9f);
        const float floorG = dbToGain (-p.range);
        float target;
        if (p.expander) {
            const float below = std::min (0.0f, db - p.thr);         // 1:2 downward expansion
            target = std::max (floorG, dbToGain (below));
        } else {
            if (db > p.thr) { open = true; holdCnt = p.holdMs * 0.001f * fs; }
            else if (db < p.thr - p.hyst) { if (holdCnt > 0) holdCnt -= 1; else open = false; }
            target = open ? 1.0f : floorG;
        }
        const float a = std::exp (-1.0f / (std::max (0.01f, p.atkMs) * 0.001f * fs / 2.2f));
        const float r = std::exp (-1.0f / (std::max (1.0f, p.relMs) * 0.001f * fs / 2.2f));
        gain = target > gain ? a * gain + (1 - a) * target : r * gain + (1 - r) * target;
        L *= gain; R *= gain;
        grDb = -20.0f * std::log10 (gain + 1e-9f);
    }
};

// ---------------------------------------------------------------- compressor (PlateVerb) + model low pass
enum CompLpMode { LpModel = 0, LpClean };
struct CompModule {
    pv::Compressor comp; Biquad scHp[2]; Biquad lp1[2], lp2[2]; FirstOrder lp0[2]; bool lpOn = false; int lpKind = 0;
    float fs = 48000;
    void prepare (float f) { fs = f; comp.fs = f; comp.lastFs = -1; comp.reset(); for (int c = 0; c < 2; ++c) { scHp[c].reset(); lp1[c].reset(); lp2[c].reset(); lp0[c].reset(); } }
    void reset() { comp.reset(); for (int c = 0; c < 2; ++c) { scHp[c].reset(); lp1[c].reset(); lp2[c].reset(); lp0[c].reset(); } }
    void setScHp (float hz) { for (int c = 0; c < 2; ++c) scHp[c].highpass (fs, std::max (10.0f, hz), 0.707f); }
    // low pass with the sonics of each type: VCA = soft op-amp roll-off, FET = output transformer (slight
    // resonance), opto = tube stage + T4 (gentle 3rd-order); clean = Butterworth 12 dB/oct
    void setLp (int type, int mode, float hz) {
        lpOn = hz < 39000.0f && hz < 0.45f * fs;
        if (! lpOn) return;
        lpKind = mode == LpClean ? 0 : (1 + std::max (0, std::min (2, type)));
        for (int c = 0; c < 2; ++c) {
            switch (lpKind) {
                case 1: lp0[c].lowpass (fs, hz); lp1[c].lowpass (fs, hz * 1.2f, 0.5f); break;
                case 2: lp1[c].lowpass (fs, hz, 0.82f); break;
                case 3: lp1[c].lowpass (fs, hz, 0.6f); lp0[c].lowpass (fs, hz * 1.5f); break;
                default: lp1[c].lowpass (fs, hz, 0.7071f); break;
            }
        }
    }
    inline void process (float& L, float& R, float kL, float kR, const pv::CompParams& p, bool scHpOn) {
        if (scHpOn) { kL = scHp[0].process (kL); kR = scHp[1].process (kR); }
        comp.setTimes (p.atkMs, p.relMs);
        comp.process (L, R, kL, kR, p);
        if (lpOn) {
            switch (lpKind) {
                case 1: L = lp1[0].process (lp0[0].process (L)); R = lp1[1].process (lp0[1].process (R)); break;
                case 3: L = lp0[0].process (lp1[0].process (L)); R = lp0[1].process (lp1[1].process (R)); break;
                default: L = lp1[0].process (L); R = lp1[1].process (R); break;
            }
        }
    }
};

// ---------------------------------------------------------------- brickwall limiter (look-ahead)
struct Limiter {
    float fs = 48000; int la = 64; std::vector<float> dL, dR, gBuf, sum; int w = 0;
    std::vector<float> minQ; std::vector<int> minIdx; int qHead = 0, qTail = 0, n = 0;
    float relC = 0.999f, held = 1, boxSum = 0; float grDb = 0;
    void prepare (float f, float lookMs) {
        fs = f; la = std::max (4, (int) std::lround (lookMs * 0.001f * fs));
        dL.assign ((size_t) la, 0); dR.assign ((size_t) la, 0); gBuf.assign ((size_t) la, 1.0f);
        minQ.assign ((size_t) la * 2 + 4, 1.0f); minIdx.assign ((size_t) la * 2 + 4, 0);
        reset();
    }
    void reset() {
        std::fill (dL.begin(), dL.end(), 0.0f); std::fill (dR.begin(), dR.end(), 0.0f); std::fill (gBuf.begin(), gBuf.end(), 1.0f);
        w = 0; qHead = qTail = 0; n = 0; held = 1; boxSum = (float) la; grDb = 0;
    }
    int latency() const { return la - 1; }
    // sliding minimum (monotonic deque) of the required gain over the look-ahead window,
    // a release one-pole, then a box average over the window: no overshoot, smooth attack.
    inline void process (float& L, float& R, float ceil, float relMs) {
        relC = std::exp (-1.0f / (std::max (1.0f, relMs) * 0.001f * fs));
        const float pk = std::max (std::fabs (L), std::fabs (R));
        const float need = pk > ceil ? ceil / pk : 1.0f;
        const int cap = (int) minQ.size();
        while (qTail != qHead && minQ[(size_t) ((qTail - 1 + cap) % cap)] >= need) qTail = (qTail - 1 + cap) % cap;
        minQ[(size_t) qTail] = need; minIdx[(size_t) qTail] = n; qTail = (qTail + 1) % cap;
        while (minIdx[(size_t) qHead] <= n - la) qHead = (qHead + 1) % cap;
        const float m = minQ[(size_t) qHead];
        held = m < held ? m : m + (held - m) * relC;
        const int gi = n % la;
        boxSum += held - gBuf[(size_t) gi]; gBuf[(size_t) gi] = held;
        const float g = std::min (1.0f, boxSum / (float) la);
        const int ri = (w + 1) % la;
        const float oL = dL[(size_t) ri], oR = dR[(size_t) ri];
        dL[(size_t) w] = L; dR[(size_t) w] = R; w = ri;
        ++n; if (n > (1 << 30)) { n -= (1 << 29); for (auto& v : minIdx) v -= (1 << 29); }
        L = clampf (oL * g, -ceil, ceil); R = clampf (oR * g, -ceil, ceil);
        grDb = -20.0f * std::log10 (g + 1e-9f);
    }
};

// ---------------------------------------------------------------- broadband phase rotator
// Two allpass chains with ~90 deg phase difference over the audio band (Niemitalo's design);
// output = I cos(a) + Q sin(a): a constant phase shift of 'a' at all frequencies, flat magnitude.
struct PhaseRotator {
    struct AP { float a = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        inline float process (float x) { const float y = a * (x + y2) - x2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; } };
    AP p1[2][4], p2[2][4]; float d1[2] = { 0, 0 };
    PhaseRotator() {
        static const float c1[] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
        static const float c2[] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 4; ++i) { p1[c][i].a = c1[i]; p2[c][i].a = c2[i]; }
    }
    void reset() { for (int c = 0; c < 2; ++c) { for (int i = 0; i < 4; ++i) { p1[c][i] = AP { p1[c][i].a }; p2[c][i] = AP { p2[c][i].a }; } d1[c] = 0; } }
    inline float process (int c, float x, float cs, float sn) {
        float i = x, q = x;
        for (int k = 0; k < 4; ++k) { i = p1[c][k].process (i); q = p2[c][k].process (q); }
        const float iD = d1[c]; d1[c] = i;                  // one-sample delay on the first path
        return iD * cs + q * sn;
    }
};

} // namespace cs
