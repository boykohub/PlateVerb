// ChannelStrip 2 DSP engine - no JUCE dependency.
// Modules (any order): PRE, EQ, DS2, COMP, GATE, INSERT (host plugin, handled by the processor), LIMIT.
// EQ and filters: 64-bit, analog-matched biquads (poles by impulse invariance, numerator matched to the
// analog magnitude at DC, the band frequency and Nyquist), so curves keep their analog shape up to 20 kHz
// at any sample rate. The compressor is PlateVerb's (pv::Compressor, ../../Source/Engine.h), unchanged.
#pragma once
#include "Engine.h"
#include <vector>
#include <array>
#include <cmath>
#include <complex>
#include <algorithm>

namespace cs {

using pv::clampf;
using pv::dbToGain;
constexpr double kPiD = 3.14159265358979323846;
constexpr float kPi = 3.14159265358979f;

enum Module { ModPre = 0, ModEq, ModDs, ModComp, ModGate, ModInsert, ModLim, kNumModules };
inline const char* moduleName (int m) { static const char* n[] = { "PRE", "EQ", "DS2", "COMP", "GATE", "INSERT", "LIMIT" }; return n[m]; }
enum Route { RouteST = 0, RouteDUO, RouteMS };

// permutation <-> index (Lehmer code): the module order is one host-saved integer parameter
inline int factorial (int n) { int f = 1; for (int i = 2; i <= n; ++i) f *= i; return f; }
inline std::array<int, kNumModules> orderFromIndex (int idx) {
    std::array<int, kNumModules> out {}; std::vector<int> pool; for (int i = 0; i < kNumModules; ++i) pool.push_back (i);
    idx = std::max (0, std::min (idx, factorial (kNumModules) - 1));
    for (int i = 0; i < kNumModules; ++i) {
        const int f = factorial (kNumModules - 1 - i), k = idx / f; idx %= f;
        out[(size_t) i] = pool[(size_t) k]; pool.erase (pool.begin() + k);
    }
    return out;
}
inline int indexFromOrder (const std::array<int, kNumModules>& o) {
    int idx = 0; std::vector<int> pool; for (int i = 0; i < kNumModules; ++i) pool.push_back (i);
    for (int i = 0; i < kNumModules; ++i) {
        const int k = (int) (std::find (pool.begin(), pool.end(), o[(size_t) i]) - pool.begin());
        idx += k * factorial (kNumModules - 1 - i); pool.erase (pool.begin() + k);
    }
    return idx;
}
inline int defaultOrderIndex() { return indexFromOrder ({ ModPre, ModEq, ModDs, ModComp, ModGate, ModInsert, ModLim }); }

// ---------------------------------------------------------------- 64-bit biquad
struct DBiquad {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline double process (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void setCoefs (const DBiquad& o) { b0 = o.b0; b1 = o.b1; b2 = o.b2; a1 = o.a1; a2 = o.a2; }
    double magDb (double f, double fs) const {
        const double w = 2.0 * kPiD * f / fs;
        const std::complex<double> z1c = std::polar (1.0, -w), z2c = std::polar (1.0, -2.0 * w);
        const auto n = b0 + b1 * z1c + b2 * z2c, d = 1.0 + a1 * z1c + a2 * z2c;
        return 20.0 * std::log10 (std::abs (n) / std::max (1e-30, std::abs (d)) + 1e-30);
    }
};

// analog prototype: H(s) = (n2 s^2 + n1 s + n0) / (d2 s^2 + d1 s + d0), s in rad/s
struct Analog {
    double n2 = 0, n1 = 0, n0 = 1, d2 = 0, d1 = 0, d0 = 1; double fMatch = 1000;
    double mag2 (double f) const {
        const double w = 2.0 * kPiD * f;
        const double nr = n0 - n2 * w * w, ni = n1 * w, dr = d0 - d2 * w * w, di = d1 * w;
        return (nr * nr + ni * ni) / std::max (1e-300, dr * dr + di * di);
    }
    static Analog peak (double f, double Q, double dB) {
        const double A = std::pow (10.0, dB / 40.0), w = 2 * kPiD * f; Analog a;
        a.n2 = 1; a.n1 = A / Q * w; a.n0 = w * w; a.d2 = 1; a.d1 = w / (A * Q); a.d0 = w * w; a.fMatch = f; return a;
    }
    static double shelfInvQ (double A, double S) { return std::sqrt (std::max (1e-9, (A + 1 / A) * (1 / S - 1) + 2)); }
    static Analog lowShelf (double f, double dB, double S) {
        const double A = std::pow (10.0, dB / 40.0), w = 2 * kPiD * f, k = std::sqrt (A) * shelfInvQ (A, S); Analog a;
        a.n2 = A; a.n1 = A * k * w; a.n0 = A * A * w * w; a.d2 = A; a.d1 = k * w; a.d0 = w * w; a.fMatch = f; return a;
    }
    static Analog highShelf (double f, double dB, double S) {
        const double A = std::pow (10.0, dB / 40.0), w = 2 * kPiD * f, k = std::sqrt (A) * shelfInvQ (A, S); Analog a;
        a.n2 = A * A; a.n1 = A * k * w; a.n0 = A * w * w; a.d2 = 1; a.d1 = k * w; a.d0 = A * w * w; a.fMatch = f; return a;
    }
    static Analog lowShelf1 (double fm, double dB) {      // 1st order, gain at DC, geometric mid frequency fm
        const double g = std::pow (10.0, dB / 20.0), w = 2 * kPiD * fm; Analog a;
        a.n1 = 1; a.n0 = w * std::sqrt (g); a.d1 = 1; a.d0 = w / std::sqrt (g); a.fMatch = fm; return a;
    }
    static Analog highShelf1 (double fm, double dB) {
        const double g = std::pow (10.0, dB / 20.0), w = 2 * kPiD * fm; Analog a;
        a.n1 = g; a.n0 = g * w / std::sqrt (g); a.d1 = 1; a.d0 = w * std::sqrt (g); a.fMatch = fm; return a;
    }
    static Analog hp2 (double f, double Q) { const double w = 2 * kPiD * f; Analog a; a.n2 = 1; a.n1 = 0; a.n0 = 0; a.d2 = 1; a.d1 = w / Q; a.d0 = w * w; a.fMatch = f; return a; }
    static Analog lp2 (double f, double Q) { const double w = 2 * kPiD * f; Analog a; a.n2 = 0; a.n1 = 0; a.n0 = w * w; a.d2 = 1; a.d1 = w / Q; a.d0 = w * w; a.fMatch = f; return a; }
    static Analog hp1 (double f) { const double w = 2 * kPiD * f; Analog a; a.n1 = 1; a.n0 = 0; a.d1 = 1; a.d0 = w; a.fMatch = f; return a; }
    static Analog lp1 (double f) { const double w = 2 * kPiD * f; Analog a; a.n1 = 0; a.n0 = w; a.d1 = 1; a.d0 = w; a.fMatch = f; return a; }
    static Analog bandpass (double f, double Q) { const double w = 2 * kPiD * f; Analog a; a.n2 = 0; a.n1 = w / Q; a.n0 = 0; a.d2 = 1; a.d1 = w / Q; a.d0 = w * w; a.fMatch = f; return a; }
};

// bilinear fallback (prewarped at fMatch) for prototypes whose poles sit above ~0.45 fs
inline DBiquad bilinear (const Analog& a, double fs) {
    const double wm = 2 * kPiD * std::min (a.fMatch, 0.45 * fs);
    const double K = wm / std::tan (wm / (2 * fs)), K2 = K * K;
    const double B0 = a.n2 * K2 + a.n1 * K + a.n0, B1 = 2 * (a.n0 - a.n2 * K2), B2 = a.n2 * K2 - a.n1 * K + a.n0;
    const double A0 = a.d2 * K2 + a.d1 * K + a.d0, A1 = 2 * (a.d0 - a.d2 * K2), A2 = a.d2 * K2 - a.d1 * K + a.d0;
    DBiquad q; q.b0 = B0 / A0; q.b1 = B1 / A0; q.b2 = B2 / A0; q.a1 = A1 / A0; q.a2 = A2 / A0; return q;
}

// magnitude-matched design (after Vicanek, "Matched Second Order Digital Filters")
inline DBiquad matched (const Analog& a, double fs) {
    const double T = 1.0 / fs, nyq = 0.5 * fs;
    DBiquad q;
    if (a.d2 == 0.0) {                                   // first order
        const double p = -a.d0 / a.d1;
        if (-p > 2 * kPiD * 0.45 * fs) return bilinear (a, fs);
        q.a1 = -std::exp (p * T); q.a2 = 0;
        const double h0 = std::sqrt (a.mag2 (0.0)), hn = std::sqrt (a.mag2 (nyq));
        const double k0 = h0 * (1 + q.a1), kn = hn * (1 - q.a1);
        q.b0 = 0.5 * (k0 + kn); q.b1 = 0.5 * (k0 - kn); q.b2 = 0;
        return q;
    }
    const double disc = a.d1 * a.d1 - 4 * a.d2 * a.d0;
    const double wn = std::sqrt (a.d0 / a.d2);
    if (wn > 2 * kPiD * 0.45 * fs) return bilinear (a, fs);
    if (disc < 0) {
        const double sg = -a.d1 / (2 * a.d2), om = std::sqrt (-disc) / (2 * a.d2);
        q.a1 = -2 * std::exp (sg * T) * std::cos (om * T); q.a2 = std::exp (2 * sg * T);
    } else {
        const double r1 = (-a.d1 + std::sqrt (disc)) / (2 * a.d2), r2 = (-a.d1 - std::sqrt (disc)) / (2 * a.d2);
        q.a1 = -(std::exp (r1 * T) + std::exp (r2 * T)); q.a2 = std::exp ((r1 + r2) * T);
    }
    const double A0 = (1 + q.a1 + q.a2) * (1 + q.a1 + q.a2), A1 = (1 - q.a1 + q.a2) * (1 - q.a1 + q.a2), A2 = -4 * q.a2;
    const double fm = std::min (a.fMatch, 0.42 * fs);
    const double w = 2 * kPiD * fm / fs, s2 = std::sin (w / 2) * std::sin (w / 2);
    const double ph0 = 1 - s2, ph1 = s2, ph2 = 4 * ph0 * ph1;
    const double B0 = a.mag2 (0.0) * A0, B1 = a.mag2 (nyq) * A1;
    const double B2 = (a.mag2 (fm) * (A0 * ph0 + A1 * ph1 + A2 * ph2) - B0 * ph0 - B1 * ph1) / std::max (1e-300, ph2);
    const double p = std::sqrt (std::max (0.0, B0)), m = std::sqrt (std::max (0.0, B1));
    const double W = 0.5 * (p + m);
    q.b1 = 0.5 * (p - m);
    q.b0 = 0.5 * (W + std::sqrt (std::max (0.0, W * W + B2)));
    q.b2 = q.b0 > 1e-300 ? -B2 / (4 * q.b0) : 0.0;
    return q;
}

// ---------------------------------------------------------------- EQ models
enum EqModel { EqClean = 0, EqNeveFC, EqNevePA, EqSslPA, EqSslUAD, EqPultec, EqApi, kNumEqModels };
inline const char* eqModelName (int m) {
    static const char* n[] = { "Clean Digital", "British 73 - FC", "British 73/84 - PA", "E-Series 4K - PA",
                               "E-Series 4K - UAD", "Passive Tube EQP/MEQ - UAD", "American 550 - UAD" };
    return n[m];
}
enum Band { BandLow = 0, BandTone, BandMid, BandHigh };
inline const char* bandName (int b) { static const char* n[] = { "LOW", "TONE", "MID", "HIGH" }; return n[b]; }
// each model's classic frequencies, set when the model is chosen (all bands stay free 20 Hz - 20 kHz)
inline float modelDefaultFreq (int model, int band) {
    static const float t[kNumEqModels][4] = {
        { 100, 600, 3000, 10000 }, { 110, 700, 3200, 12000 }, { 110, 700, 3200, 12000 }, { 100, 800, 3000, 10000 },
        { 100, 800, 3000, 10000 }, { 60, 500, 3000, 10000 }, { 100, 500, 3000, 10000 } };
    return t[std::max (0, std::min (kNumEqModels - 1, model))][std::max (0, std::min (3, band))];
}
inline bool eqHasAux (int model, int band) { return model == EqPultec && (band == BandLow || band == BandHigh); }

struct BandSettings { bool on = true; bool shelf = true; float freq = 1000, gain = 0, qScale = 1, aux = 0; int auxSel = 1; };

// native bell Q of each model (RBJ definition, fitted to the measured reference plugins); TONE is wider,
// MID more focused
inline float nativeBellQ (int model, float gainDb) {
    switch (model) {
        case EqNeveFC: return 0.54f;
        case EqNevePA: return 0.46f;
        case EqSslPA: case EqSslUAD: return 0.44f;
        case EqPultec: return 0.7f;
        case EqApi: return 0.35f + 0.1f * std::fabs (gainDb);
        default: return 0.707f;
    }
}
inline float bandQ (int model, int band, float gainDb) {
    const float q = nativeBellQ (model, gainDb);
    return band == BandTone ? q * 0.6f : band == BandMid ? q * 1.7f : q;
}
inline bool bandIsShelf (int model, const BandSettings& b, int band) {
    if (band != BandLow && band != BandHigh) return false;
    if (model == EqPultec) return band == BandLow;
    return b.shelf;
}
inline float bandMaxGain (int model, int band) {
    switch (model) {
        case EqNeveFC: case EqNevePA: return band == BandLow || band == BandHigh ? 16.0f : 18.0f;
        case EqSslPA: case EqSslUAD: return 15.0f;
        case EqPultec: return band == BandLow ? 16.3f : band == BandHigh ? 20.0f : 10.0f;
        case EqApi: return 12.0f;
        default: return 18.0f;
    }
}

// one band -> up to 3 analog sections
inline int bandAnalog (int model, const BandSettings& b, int band, Analog* out) {
    if (! b.on) return 0;
    const double f = clampf (b.freq, 10.0f, 30000.0f);
    const double g = clampf (b.gain, -bandMaxGain (model, band), bandMaxGain (model, band));
    const double qs = clampf (b.qScale, 0.1f, 10.0f);
    const bool shelf = bandIsShelf (model, b, band), low = band == BandLow;
    int n = 0;
    auto peak = [&] (double fr, double q, double db) { if (std::fabs (db) > 1e-3) out[n++] = Analog::peak (fr, q, db); };
    auto shelf2 = [&] (bool lo, double fr, double db, double S) { if (std::fabs (db) > 1e-3) out[n++] = lo ? Analog::lowShelf (fr, db, std::min (1.5, S)) : Analog::highShelf (fr, db, std::min (1.5, S)); };
    auto shelf1 = [&] (bool lo, double fm, double db) { if (std::fabs (db) > 1e-3) out[n++] = lo ? Analog::lowShelf1 (fm, db) : Analog::highShelf1 (fm, db); };
    const double bq = bandQ (model, band, (float) g) * qs;
    switch (model) {
        case EqNeveFC:
            if (shelf) { shelf2 (low, low ? f * 1.37 : f / 1.37, g * 1.02, 0.68 * qs); peak (low ? f * 7.0 : f / 7.0, 0.66, -0.105 * g); }
            else peak (f, bq, g * 1.21);
            break;
        case EqNevePA:
            if (shelf) { shelf2 (low, low ? f * 2.37 : f / 2.37, g * 0.93, 0.86 * qs); peak (low ? f * 10.2 : f / 10.2, 0.81, -0.079 * g); peak (low ? f * 0.64 : f / 0.64, 1.1, 0.06 * g); }
            else peak (f, bq, g * 0.965);
            break;
        case EqSslPA: case EqSslUAD:
            if (shelf) { if (std::fabs (qs - 1.0) < 0.02) shelf1 (low, low ? f * 2.41 : f / 2.41, g * 1.08); else shelf2 (low, low ? f * 1.81 : f / 1.81, g * 1.08, 0.74 * qs); }
            else peak (f, bq, g * 1.03);
            break;
        case EqPultec:
            if (band == BandLow) {          // boost and atten at the same frequency: the bump-and-dip
                if (g > 0) shelf1 (true, f * 2.43, g);
                if (b.aux < -0.01f) shelf1 (true, f * 2.43 * 1.9, std::max (-17.5, (double) b.aux));
            } else if (band == BandHigh) {
                if (g > 0) peak (f, 0.93 * qs, g * 0.993);
                if (b.aux < -0.01f) { static const double att[] = { 5000, 10000, 20000 }; shelf1 (false, att[std::max (0, std::min (2, b.auxSel))] / 1.6, std::max (-16.0, (double) b.aux)); }
            } else peak (f, (g < 0 ? 0.8 / 0.7 : 1.0) * bq, g);
            break;
        case EqApi:
            if (shelf) { shelf2 (low, f, g, 0.9 * qs); peak (low ? f * 3.5 : f / 3.5, 0.9, -0.06 * g); }
            else peak (f, bq, g);
            break;
        default:
            if (shelf) shelf2 (low, f, g, 0.707 * qs); else peak (f, bq, g);
    }
    return n;
}

// harmonic colour of each model at -18 dBFS, drive 50 % (measured on the reference plugins)
struct EqColour { float c2, c3, c3lf; };
inline EqColour eqColour (int model) {
    switch (model) {
        case EqNevePA: return { 3.0e-4f, 0.0f, 0.28f };
        case EqSslPA:  return { 0.0f, 0.0159f, 0.0f };
        case EqSslUAD: return { 2.5e-3f, 0.0f, 0.0f };
        case EqPultec: return { 8.9e-4f, 0.0f, 0.0f };
        case EqApi:    return { 0.0f, 0.01f, 0.02f };
        default:       return { 0, 0, 0 };
    }
}

inline float softCubic (float x, float c3) { if (c3 <= 0.0f) return x; const float g = std::sqrt (3.0f * c3); return std::tanh (g * x) / g; }
inline float softSquare (float x, float c2) { if (c2 == 0.0f) return x; const float q = x * x; return x + c2 * q / (1.0f + std::fabs (c2) * q); }

// ---------------------------------------------------------------- smoothed EQ bank (64-bit, matched)
// Targets move toward the knob values (~25 ms); coefficients are re-matched every 16 samples while moving.
struct EqBank {
    double fs = 48000; int model = EqNeveFC; std::array<BandSettings, 4> target {}, cur {};
    DBiquad sec[2][4][3]; int nSec[4] = { 0, 0, 0, 0 }; bool dirty = true, first = true;
    EqColour col { 0, 0, 0 }; pv::OnePole lf[2]; DBiquad dc[2];
    void prepare (double f) {
        fs = f; first = true; dirty = true;
        for (auto& c : sec) for (auto& b : c) for (auto& s : b) s.reset();
        for (int c = 0; c < 2; ++c) { lf[c].setCut ((float) fs, 160.0f); lf[c].reset(); dc[c].setCoefs (matched (Analog::hp2 (6.0, 0.707), fs)); dc[c].reset(); }
    }
    void reset() { for (auto& c : sec) for (auto& b : c) for (auto& s : b) s.reset(); for (int c = 0; c < 2; ++c) { lf[c].reset(); dc[c].reset(); } }
    void set (int m, const std::array<BandSettings, 4>& t, float drive01) {
        if (m != model) { model = m; first = true; }
        target = t; dirty = true;
        const auto k = eqColour (model); const float amt = (drive01 / 0.5f) * (drive01 / 0.5f);
        col = { k.c2 * amt, k.c3 * amt, k.c3lf * amt };
    }
    static float approach (float c, float t, float a) { return c + (t - c) * a; }
    void design() {
        for (int b = 0; b < 4; ++b) {
            Analog an[3]; const int n = bandAnalog (model, cur[(size_t) b], b, an);
            for (int k = 0; k < n; ++k) { const auto q = matched (an[k], fs); for (int c = 0; c < 2; ++c) sec[c][b][k].setCoefs (q); }
            if (n > nSec[b]) for (int k = nSec[b]; k < n; ++k) for (int c = 0; c < 2; ++c) sec[c][b][k].reset();
            nSec[b] = n;
        }
    }
    void step() {      // call every 16 samples
        if (! dirty) return;
        if (first) { cur = target; first = false; design(); dirty = false; return; }
        const float a = 1.0f - std::exp (-16.0f / (0.025f * (float) fs));
        bool moving = false;
        for (int b = 0; b < 4; ++b) {
            auto& c = cur[(size_t) b]; const auto& t = target[(size_t) b];
            c.on = t.on; c.shelf = t.shelf; c.auxSel = t.auxSel;
            const float nf = std::exp (approach (std::log (c.freq), std::log (t.freq), a));
            c.freq = std::fabs (nf - t.freq) < 0.0005f * t.freq ? t.freq : nf;
            c.gain = std::fabs (c.gain - t.gain) < 0.005f ? t.gain : approach (c.gain, t.gain, a);
            c.qScale = std::fabs (c.qScale - t.qScale) < 0.0005f ? t.qScale : approach (c.qScale, t.qScale, a);
            c.aux = std::fabs (c.aux - t.aux) < 0.005f ? t.aux : approach (c.aux, t.aux, a);
            moving |= c.freq != t.freq || c.gain != t.gain || c.qScale != t.qScale || c.aux != t.aux;
        }
        design();
        dirty = moving;
    }
    inline float process (int ch, float x) {
        double y = x;
        for (int b = 0; b < 4; ++b) for (int k = 0; k < nSec[b]; ++k) y = sec[ch][b][k].process (y);
        float o = (float) y;
        if (col.c3lf > 0.0f) { const float l = lf[ch].lp (o); o += softCubic (l, col.c3lf) - l; }
        if (col.c3 > 0.0f) o = softCubic (o, col.c3);
        if (col.c2 != 0.0f) o = (float) dc[ch].process (softSquare (o, col.c2));
        return o;
    }
    static double bandMagDb (int model, const BandSettings& b, int band, double f, double fsDisp) {
        Analog an[3]; const int n = bandAnalog (model, b, band, an); double m = 0;
        for (int k = 0; k < n; ++k) m += matched (an[k], fsDisp).magDb (f, fsDisp);
        return m;
    }
};

// ---------------------------------------------------------------- cut filters (HPF / LPF, 6-24 dB/oct + resonance)
struct CutFilter {
    DBiquad s[2][2]; int n = 0; bool active = false;
    void reset() { for (auto& c : s) for (auto& q : c) q.reset(); }
    void set (bool hp, bool on, double fs, double f, int slopeDb, double reso) {
        active = on; if (! on) return;
        const int order = std::max (1, std::min (4, slopeDb / 6));
        Analog secs[2]; int k = 0;
        const double r = reso * 4.0;      // resonance raises the Q of the last 2nd-order section
        if (order == 1) secs[k++] = hp ? Analog::hp1 (f) : Analog::lp1 (f);
        else if (order == 2) secs[k++] = hp ? Analog::hp2 (f, 0.7071 + r) : Analog::lp2 (f, 0.7071 + r);
        else if (order == 3) { secs[k++] = hp ? Analog::hp1 (f) : Analog::lp1 (f); secs[k++] = hp ? Analog::hp2 (f, 1.0 + r) : Analog::lp2 (f, 1.0 + r); }
        else { secs[k++] = hp ? Analog::hp2 (f, 0.5412) : Analog::lp2 (f, 0.5412); secs[k++] = hp ? Analog::hp2 (f, 1.3066 + r) : Analog::lp2 (f, 1.3066 + r); }
        n = k;
        for (int i = 0; i < n; ++i) { const auto q = matched (secs[i], fs); for (int c = 0; c < 2; ++c) s[c][i].setCoefs (q); }
    }
    inline float process (int c, float x) { if (! active) return x; double y = x; for (int k = 0; k < n; ++k) y = s[c][k].process (y); return (float) y; }
    double magDb (double f, double fs) const { if (! active) return 0; double m = 0; for (int k = 0; k < n; ++k) m += s[0][k].magDb (f, fs); return m; }
};

// ---------------------------------------------------------------- preamp: model + saturation colour + thump
enum PreModel { PreClean = 0, PreTube, Pre1073, Pre1080, Pre212, kNumPreModels };
inline const char* preModelName (int m) {
    static const char* n[] = { "Clean", "Tube Mic Pre (Voxbox-style)", "British 1073-style", "British 1080-style", "American 212-style" };
    return n[m];
}
enum PreColour { ColOdd = 0, ColEven, ColHeavy, ColCrush };
struct Preamp {
    double fs = 48000; int model = PreTube, colour = ColEven; float drive = 1, out = 1; bool thumpOn = false; float lfSat = 0.0f;
    DBiquad hp[2], lp[2], thump[2]; pv::OnePole lf[2]; DBiquad dc[2];
    void prepare (double f) { fs = f; set (model, colour, 0, 0, false, 70); reset(); }
    void reset() { for (int c = 0; c < 2; ++c) { hp[c].reset(); lp[c].reset(); thump[c].reset(); lf[c].reset(); dc[c].reset(); } }
    void set (int m, int col, float driveDb, float outDb, bool th, double thumpHz) {
        model = m; colour = col; drive = dbToGain (driveDb); out = dbToGain (outDb); thumpOn = th;
        double hpF = 5, lpF = 0.45 * fs;
        switch (m) {
            case PreTube: hpF = 8; lpF = 45000; lfSat = 0.15f; break;
            case Pre1073: hpF = 10; lpF = 40000; lfSat = 0.9f; break;      // 1073 spec: -3 dB at 40 kHz
            case Pre1080: hpF = 9; lpF = 50000; lfSat = 0.5f; break;
            case Pre212:  hpF = 7; lpF = 70000; lfSat = 0.3f; break;
            default: lfSat = 0; break;
        }
        for (int c = 0; c < 2; ++c) {
            hp[c].setCoefs (matched (Analog::hp2 (hpF, 0.7071), fs));
            lp[c].setCoefs (matched (Analog::lp2 (std::min (lpF, 0.45 * fs), 0.7071), fs));
            thump[c].setCoefs (matched (Analog::peak (thumpHz, 1.4, 3.0), fs));
            lf[c].setCut ((float) fs, 150.0f); dc[c].setCoefs (matched (Analog::hp2 (5.0, 0.7071), fs));
        }
    }
    inline float sat (float u) const {
        switch (colour) {
            case ColOdd:   return softCubic (u, 0.12f);                                   // odd harmonics
            case ColEven:  return pv::warmSat (u, 1.0f, 0.18f);                           // even harmonics
            case ColHeavy: return pv::warmSat (softCubic (u * 1.6f, 0.25f), 1.2f, 0.25f) / 1.6f;
            default: {     // CRUSH: asymmetric hard clip with a little fold-back
                const float v = u * 2.5f + 0.08f;
                float c = v > 1.0f ? 2.0f - v : v < -1.0f ? -2.0f - v : v;
                c = std::max (-1.0f, std::min (1.0f, c));
                return (c - 0.08f) / 2.5f;
            }
        }
    }
    inline float process (int c, float x) {
        if (model == PreClean && drive <= 1.0001f) { double y = x; if (thumpOn) y = thump[c].process (y); return (float) y * out; }
        float u = x * drive * 0.4f;                                     // 0 dBFS ~ +22 dBu into the stage
        if (lfSat > 0.0f) { const float l = lf[c].lp (u); u += softCubic (l, lfSat) - l; }
        u = sat (u);
        double y = dc[c].process (u) / 0.4 / drive;                     // level compensated
        y = lp[c].process (hp[c].process (y));
        if (thumpOn) y = thump[c].process (y);
        return (float) y * out;
    }
};

// ---------------------------------------------------------------- DS2: two de-essers (wide or split band)
struct DeEsser {
    struct P { bool on = true; float freq = 6500, thr = -24, range = 8, q = 1.5f; };
    double fs = 48000; DBiquad det[2], cut[2]; float env[2] = { 0, 0 }, gr[2] = { 0, 0 }, lastCut[2] = { 99, 99 }; float aA = 0, aR = 0;
    void prepare (double f) { fs = f; aA = std::exp (-1.0f / (0.0005f * (float) fs)); aR = std::exp (-1.0f / (0.06f * (float) fs)); reset(); }
    void reset() { for (int c = 0; c < 2; ++c) { det[c].reset(); cut[c].reset(); env[c] = 0; gr[c] = 0; lastCut[c] = 99; } }
    void setDetector (const P& p) { const auto q = matched (Analog::bandpass (p.freq, p.q), fs); for (int c = 0; c < 2; ++c) det[c].setCoefs (q); }
};
struct DS2 {
    double fs = 48000; DeEsser d[2]; DeEsser::P p[2]; bool split = false, listen = false; float grDb = 0; int cnt = 0;
    float lastF[2] = { -1, -1 }, lastQ[2] = { -1, -1 };
    void prepare (double f) { fs = f; for (auto& x : d) x.prepare (f); lastF[0] = lastF[1] = -1; }
    void reset() { for (auto& x : d) x.reset(); }
    void set (const DeEsser::P& a, const DeEsser::P& b, bool sp, bool li) {
        p[0] = a; p[1] = b; split = sp; listen = li;
        for (int k = 0; k < 2; ++k) if (p[k].freq != lastF[k] || p[k].q != lastQ[k]) { d[k].setDetector (p[k]); lastF[k] = p[k].freq; lastQ[k] = p[k].q; d[k].lastCut[0] = d[k].lastCut[1] = 99; }
    }
    inline void process (float& L, float& R, bool linked) {
        float outL = L, outR = R, lisL = 0, lisR = 0, maxGr = 0;
        for (int k = 0; k < 2; ++k) {
            if (! p[k].on) continue;
            auto& e = d[k];
            const float dl = (float) e.det[0].process (L), dr = (float) e.det[1].process (R);
            lisL += dl; lisR += dr;
            float lv[2] = { std::fabs (dl), std::fabs (dr) };
            if (linked) lv[0] = lv[1] = std::max (lv[0], lv[1]);
            for (int c = 0; c < 2; ++c) {
                e.env[c] = lv[c] > e.env[c] ? e.aA * e.env[c] + (1 - e.aA) * lv[c] : e.aR * e.env[c] + (1 - e.aR) * lv[c];
                const float db = 20.0f * std::log10 (e.env[c] + 1e-9f);
                e.gr[c] = std::min (p[k].range, (float) pv::gainComputer (db, p[k].thr, 4.0f, 6.0f));
                maxGr = std::max (maxGr, e.gr[c]);
            }
            if (split) {      // dynamic bell cut at the de-ess frequency, re-matched every 16 samples
                if ((cnt & 15) == 0)
                    for (int c = 0; c < 2; ++c)
                        if (std::fabs (e.gr[c] - e.lastCut[c]) > 0.05f) { e.cut[c].setCoefs (matched (Analog::peak (p[k].freq, p[k].q * 0.8, -e.gr[c]), fs)); e.lastCut[c] = e.gr[c]; }
                if (e.lastCut[0] > 0.01f && e.lastCut[0] < 98.0f) outL = (float) e.cut[0].process (outL);
                if (e.lastCut[1] > 0.01f && e.lastCut[1] < 98.0f) outR = (float) e.cut[1].process (outR);
            } else { outL *= dbToGain (-e.gr[0]); outR *= dbToGain (-e.gr[1]); }
        }
        ++cnt; grDb = maxGr;
        if (listen) { L = lisL; R = lisR; } else { L = outL; R = outR; }
    }
};

// ---------------------------------------------------------------- gate / expander
struct Gate {
    float fs = 48000; float env = 0, gain = 1, holdCnt = 0; bool open = false;
    pv::Biquad khp[2], klp[2]; float grDb = 0;
    void prepare (float f) { fs = f; reset(); }
    void reset() { env = 0; gain = 1; holdCnt = 0; open = false; for (int c = 0; c < 2; ++c) { khp[c].reset(); klp[c].reset(); } }
    void setKey (float hpHz, float lpHz) { for (int c = 0; c < 2; ++c) { khp[c].highpass (fs, hpHz, 0.707f); klp[c].lowpass (fs, std::min (lpHz, 0.45f * fs), 0.707f); } }
    struct P { bool expander = false; float thr = -50, range = 40, atkMs = 0.5f, holdMs = 50, relMs = 150, hyst = 3; bool listen = false; };
    inline void process (float& L, float& R, float kL, float kR, const P& p) {
        const float fl = klp[0].process (khp[0].process (kL)), fr = klp[1].process (khp[1].process (kR));
        if (p.listen) { L = fl; R = fr; grDb = 0; return; }
        const float lvl = std::max (std::fabs (fl), std::fabs (fr));
        const float rE = std::exp (-1.0f / (0.010f * fs));
        env = lvl > env ? lvl : rE * env + (1 - rE) * lvl;
        const float db = 20.0f * std::log10 (env + 1e-9f), floorG = dbToGain (-p.range);
        float target;
        if (p.expander) target = std::max (floorG, (float) dbToGain (std::min (0.0f, db - p.thr)));
        else {
            if (db > p.thr) { open = true; holdCnt = p.holdMs * 0.001f * fs; }
            else if (db < p.thr - p.hyst) { if (holdCnt > 0) holdCnt -= 1; else open = false; }
            target = open ? 1.0f : floorG;
        }
        const float a = std::exp (-1.0f / (std::max (0.01f, p.atkMs) * 0.001f * fs / 2.2f));
        const float r = std::exp (-1.0f / (std::max (1.0f, p.relMs) * 0.001f * fs / 2.2f));
        gain = target > gain ? a * gain + (1 - a) * target : r * gain + (1 - r) * target;
        L *= gain; R *= gain; grDb = -20.0f * std::log10 (gain + 1e-9f);
    }
};

// ---------------------------------------------------------------- brickwall limiter (look-ahead, or none)
struct Limiter {
    float fs = 48000; int la = 64; std::vector<float> dL, dR, gBuf; int w = 0;
    std::vector<float> minQ; std::vector<int> minIdx; int qHead = 0, qTail = 0, n = 0;
    float relC = 0.999f, held = 1, boxSum = 0; float grDb = 0;
    void prepare (float f, float lookMs) {
        fs = f; la = std::max (1, (int) std::lround (lookMs * 0.001f * fs));
        dL.assign ((size_t) la, 0); dR.assign ((size_t) la, 0); gBuf.assign ((size_t) la, 1.0f);
        minQ.assign ((size_t) la * 2 + 4, 1.0f); minIdx.assign ((size_t) la * 2 + 4, 0); reset();
    }
    void reset() { std::fill (dL.begin(), dL.end(), 0.0f); std::fill (dR.begin(), dR.end(), 0.0f); std::fill (gBuf.begin(), gBuf.end(), 1.0f); w = 0; qHead = qTail = 0; n = 0; held = 1; boxSum = (float) la; grDb = 0; }
    int latency() const { return la - 1; }
    // sliding minimum of the required gain over the look-ahead window, release one-pole, box average
    inline void process (float& L, float& R, float ceil, float relMs) {
        relC = std::exp (-1.0f / (std::max (1.0f, relMs) * 0.001f * fs));
        const float pk = std::max (std::fabs (L), std::fabs (R)), need = pk > ceil ? ceil / pk : 1.0f;
        const int cap = (int) minQ.size();
        while (qTail != qHead && minQ[(size_t) ((qTail - 1 + cap) % cap)] >= need) qTail = (qTail - 1 + cap) % cap;
        minQ[(size_t) qTail] = need; minIdx[(size_t) qTail] = n; qTail = (qTail + 1) % cap;
        while (minIdx[(size_t) qHead] <= n - la) qHead = (qHead + 1) % cap;
        const float m = minQ[(size_t) qHead];
        held = m < held ? m : m + (held - m) * relC;
        const int gi = n % la; boxSum += held - gBuf[(size_t) gi]; gBuf[(size_t) gi] = held;
        const float g = std::min (1.0f, boxSum / (float) la);
        float oL = L, oR = R;
        if (la > 1) { const int ri = (w + 1) % la; oL = dL[(size_t) ri]; oR = dR[(size_t) ri]; dL[(size_t) w] = L; dR[(size_t) w] = R; w = ri; }
        ++n; if (n > (1 << 30)) { n -= (1 << 29); for (auto& v : minIdx) v -= (1 << 29); }
        L = clampf (oL * g, -ceil, ceil); R = clampf (oR * g, -ceil, ceil); grDb = -20.0f * std::log10 (g + 1e-9f);
    }
};

// ---------------------------------------------------------------- broadband phase rotator
// Two allpass chains ~90 deg apart over the audio band (Niemitalo); out = I cos(a) + Q sin(a).
struct PhaseRotator {
    struct AP { double a = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        inline double process (double x) { const double y = a * (x + y2) - x2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; } };
    AP p1[2][4], p2[2][4]; double d1[2] = { 0, 0 };
    PhaseRotator() { init(); }
    void init() {
        static const double c1[] = { 0.6923878, 0.9360654322959, 0.9882295226860, 0.9987488452737 };
        static const double c2[] = { 0.4021921162426, 0.8561710882420, 0.9722909545651, 0.9952884791278 };
        for (int c = 0; c < 2; ++c) { for (int i = 0; i < 4; ++i) { p1[c][i] = AP(); p2[c][i] = AP(); p1[c][i].a = c1[i]; p2[c][i].a = c2[i]; } d1[c] = 0; }
    }
    void reset() { init(); }
    inline float process (int c, float x, float cs, float sn) {
        double i = x, q = x;
        for (int k = 0; k < 4; ++k) { i = p1[c][k].process (i); q = p2[c][k].process (q); }
        const double iD = d1[c]; d1[c] = i;
        return (float) (iD * cs + q * sn);
    }
};

} // namespace cs
