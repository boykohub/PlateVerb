// Flexiverb 480: DSP for every 480L algorithm (no JUCE dependency).
// Each algorithm gets its values in the units shown on the LARC (s, ms, Hz, dB, 0-255 ...), see L480Defs.h.
#pragma once
#include "Engine.h"
#include "L480Defs.h"
#include <memory>
#include <atomic>

namespace l480 {

// Resolution of the modulated delays in every reverb: 1 = 4-point (Hermite) interpolation instead of linear.
// Linear interpolation of a moving delay dulls and roughens the highs a little each time round the loop -
// heard as grain or a metallic edge in long tails. The 480L's own delays move smoothly.
constexpr double kSmooth = 1.0;
using pv::Line; using pv::OnePole; using pv::Biquad; using pv::RandLfo; using pv::clampf; using pv::dbToGain; using pv::kPi;

inline double lvGain (double db) { return db <= -47.9 ? 0.0 : dbToGain (db); }       // LARC level: bottom = Off
inline void sinCos (double pct, double& dry, double& wet) { const double a = clampf (pct, 0, 100) * 0.01 * 0.5 * kPi; dry = std::cos (a); wet = std::sin (a); }
inline double msToS (double ms, double fs) { return std::max (1.0, ms * 0.001 * fs); }

// first-order filters (bilinear): low shelf / high shelf with gain at DC / HF; gain 0 = full cut (high-/low-pass)
struct FirstOrder {
    double b0 = 1, b1 = 0, a1 = 0, z = 0;
    void reset() { z = 0; }
    inline double process (double x) { const double y = b0 * x + z; z = b1 * x - a1 * y; return y; }
    void set (double fs, double f, double s1, double s0, double t1, double t0) {   // H(s) = (s1 s + s0 w) / (t1 s + t0 w)
        const double w = 2.0 * fs * std::tan (kPi * clampf (f, 5.0, 0.45 * fs) / fs), k = 2.0 * fs;
        const double B0 = s1 * k + s0 * w, B1 = -s1 * k + s0 * w, A0 = t1 * k + t0 * w, A1 = -t1 * k + t0 * w;
        b0 = B0 / A0; b1 = B1 / A0; a1 = A1 / A0;
    }
    void lowShelf (double fs, double f, double g) { set (fs, f, 1, g, 1, 1); }
    void highShelf (double fs, double f, double g) { set (fs, f, g, 1, 1, 1); }
    void lowpass (double fs, double f) { set (fs, f, 0, 1, 1, 1); }
    void highpass (double fs, double f) { set (fs, f, 1, 0, 1, 1); }
};

// side information for an algorithm: the plugin's dry input, and machine A's output when this is machine B in Cascade
struct Ctx { double dryL = 0, dryR = 0, revL = 0, revR = 0; bool hasRev = false; };

// Decay-time curve (the editor's RT graph): multipliers of the program's main reverb time at the DecayEq band centres.
inline bool algoHasDecayCurve (int a) { return a == aRev6 || a == aRev2 || a == aRHall || a == aAmb || a == aCHall || a == aRPlate || a == aHD || a == aLegacy; }
inline int algoMainRtSlot (int a) { return a == aCHall || a == aRPlate ? 1 : a == aHD ? 2 : 0; }

struct AlgoBase {
    double fs = 48000;
    const double* curveMul = nullptr;                 // set by the Machine before set(); nullptr = the program's own decay
    double erAttackMs = 0.0;                         // reflection attack (ms), set by the Machine before set()
    double curveT[pv::DecayEq::N] = {};
    const double* curveFor (double rt) { if (! curveMul) return nullptr; for (int b = 0; b < pv::DecayEq::N; ++b) curveT[b] = rt * curveMul[b]; return curveT; }
    virtual ~AlgoBase() = default;
    virtual void prepare (double sampleRate) { fs = sampleRate; }
    virtual void reset() {}
    virtual void set (const double* v, int bank) { (void) v; (void) bank; }   // called once per block with the LARC values
    virtual void process (double& L, double& R, const Ctx& c) = 0;
    // split: the algorithm gives back only the reverb in L/R and its dry part in dryOutL/R, so everything after it
    // (tone EQ, ANALOG, channel strip) works on the reverb only and the dry signal passes untouched
    bool split = false; double dryOutL = 0, dryOutR = 0;
    inline void mixOut (double& L, double& R, double xL, double xR, double wL, double wR, double md, double mw)
    {
        if (split) { dryOutL = xL * md; dryOutR = xR * md; L = wL * mw; R = wR * mw; }
        else { L = xL * md + wL * mw; R = xR * md + wR * mw; }
    }
    virtual void key (int slot) { (void) slot; }                             // a slider-display key that is a trigger (PLAY, REC ...)
    virtual std::string status() const { return {}; }                        // shown in the LARC's main display
    // early-reflection taps you edit on the screen: time offset (ms) and level change (dB) per tap; -80 dB = off
    static constexpr int kMaxTaps = 64;
    double tapEdMs[kMaxTaps] = {}, tapEdDb[kMaxTaps] = {}; int tapEdVer = 0;
    // the algorithm's own early taps before your edits: time from the input (ms) and level re the input (dB)
    virtual int baseTaps (double* ms, double* db, int max) const { (void) ms; (void) db; (void) max; return 0; }
};

// ============================================================================ reverb (banks 1-4, Random Hall, Ambience, Classic Cart halls/plates)
// level trims: the tank's raw level depends on the decay time; these bring every program near the same loudness
inline double tankTrimDb (double rt) { return 8.0 - 1.5 * std::log2 (std::max (1.0, rt / 1.2)); }

struct Preechoes {      // up to six diffused echo clusters taken from the input (not from the predelay)
    Line inL, inR, dfL[2], dfR[2]; double g[6] = {}, d[6] = {}; int n = 0; double dif = 0.5; OnePole lp[2];
    void prepare (double fs) { inL.init ((int) (0.9 * fs)); inR.init ((int) (0.9 * fs)); for (int i = 0; i < 2; ++i) { dfL[i].init ((int) (0.01 * fs)); dfR[i].init ((int) (0.01 * fs)); } }
    void reset() { inL.clear(); inR.clear(); for (int i = 0; i < 2; ++i) { dfL[i].clear(); dfR[i].clear(); } }
    double bG[6] = {}, bMs[6] = {};
    void set (double fs, const double* lv, const double* dl, int num, double diffusion, const double* edMs = nullptr, const double* edDb = nullptr) {
        n = num; dif = clampf (diffusion, 0, 1);
        for (int i = 0; i < n; ++i)
        {
            bG[i] = lvGain (lv[i]); bMs[i] = dl[i];
            const double em = edMs ? edMs[i] : 0.0, ed = edDb ? edDb[i] : 0.0;
            g[i] = bG[i] * (ed <= -79.0 ? 0.0 : std::pow (10.0, ed / 20.0));
            d[i] = msToS (clampf (dl[i] + em, 0.1, 880.0), fs);
        }
    }
    // routing: 1 L>L, 2 R>R, 3 R>L, 4 L>R, 5 L>L, 6 R>R
    inline void process (double xL, double xR, double fs, double& oL, double& oR) {
        inL.push (xL); inR.push (xR);
        double eL = 0, eR = 0; bool any = false;
        for (int i = 0; i < n; ++i) {
            if (g[i] <= 0.0) continue;
            any = true;
            switch (i) {
                case 0: case 4: eL += g[i] * inL.read (d[i]); break;
                case 1: case 5: eR += g[i] * inR.read (d[i]); break;
                case 2: eL += g[i] * inR.read (d[i]); break;
                default: eR += g[i] * inL.read (d[i]); break;
            }
        }
        if (! any) { oL = oR = 0; return; }
        const double ga = 0.7 * dif;                 // each echo is a diffused cluster; DIFFUSION sets its density
        static constexpr double ms[4] = { 1.13, 2.71, 1.37, 2.39 };
        for (int i = 0; i < 2; ++i) { eL = pv::allpass (dfL[i], ms[i] * 0.001 * fs, ga, eL); eR = pv::allpass (dfR[i], ms[i + 2] * 0.001 * fs, ga, eR); }
        oL = eL; oR = eR;
    }
};

// ---------------------------------------------------------------- Shape and Spread: the reverb's build-up envelope
// On the 480L, SPREAD sets how long the reverb keeps building (with SIZE) and SHAPE sets the contour of that build:
// low = front-loaded (an immediate, dense start), high = a swell whose energy arrives late. Measured on a real 480L,
// a Large Hall peaks ~160 ms after the hit and a Large Church ~315 ms; a plate at SHAPE 0 / SPREAD 0 peaks at once.
// The build-up feeds the tank through jittered taps spread over that time, with gains following the contour.
// The constants below were fitted against impulse responses of a real 480L: 28 factory programs at once, their
// manual settings, energy envelope of the first 400 ms in 5 ms frames (average error 8.7 -> 7.3 dB).
struct EnvCal {
    double tMaxMs   = 256.0;   // build-up length at SPREAD 255, SIZE 40
    double sprPow   = 0.2;     // how SPREAD maps to length
    double sizeMin  = 0.075;     // share of the length that does not depend on SIZE
    double shapeA   = 2.2;     // how strongly SHAPE tilts the contour (front-loaded <-> swell)
    double shapeMid = 0.25;   // SHAPE (0..1) at which the contour is flat
    double directW  = 0.4;   // weight of the immediate part at SHAPE 0
    double basQ     = 1.0;     // effective bass multiplier = BAS ^ basQ (1: the manual's value)
    double erScale  = 0.4375;  // early reflections level
    // echo density at the start: 1 = fills in at once (plates), lower = separate echoes for longer (big halls).
    // density = densBase + densDif * (DIF/99 - 0.8) - densSize * SIZE/40 - densSpr * SPREAD/255 (0.05 .. 1)
    double densBase = 0.61, densDif = -0.4, densSize = 0.225, densSpr = 0.0;
};
inline EnvCal& envCal() { static EnvCal c; return c; }

struct EnvBuilder {
    static constexpr int K = 20;
    Line dl[2]; double tapL[K] = {}, tapR[K] = {}, g[K] = {}, gD = 1, jitL[K] = {}, jitR[K] = {}; bool on = false; double fs = 48000;
    double last[4] = { -1, -1, -1, -1 }; int n = K;
    const double* edMs = nullptr; const double* edDb = nullptr; int edVer = 0, lastVer = -1;
    double bL[K] = {}, bR[K] = {}, bG[K] = {};                             // the taps before your edits
    void prepare (double sr) {
        fs = sr; for (auto& d : dl) d.init ((int) (0.8 * fs) + 16);
        uint32_t seed = 480u;
        auto rnd = [&seed] { seed = seed * 1664525u + 1013904223u; return (double) (seed >> 8) / 16777216.0; };
        for (int k = 0; k < K; ++k) { jitL[k] = rnd(); jitR[k] = rnd(); }
        last[0] = -1;
    }
    void reset() { for (auto& d : dl) d.clear(); }
    void set (double shp, double spr, double siz, double dens = 1.0) {
        if (shp == last[0] && spr == last[1] && siz == last[2] && dens == last[3] && edVer == lastVer) return;
        lastVer = edVer;
        last[0] = shp; last[1] = spr; last[2] = siz; last[3] = dens;
        n = std::max (3, std::min (K, (int) std::lround (K * dens)));      // fewer, wider-spaced taps = a grainier start
        const auto& c = envCal();
        const double sh = clampf (shp / 255.0, 0, 1), sp = clampf (spr / 255.0, 0, 1), sz = clampf (siz / 40.0, 0, 1.2);
        const double T = c.tMaxMs * std::pow (sp, c.sprPow) * (c.sizeMin + (1.0 - c.sizeMin) * sz);
        on = T > 3.0;
        if (! on) { gD = 1; return; }
        const double a = c.shapeA * (sh - c.shapeMid);                    // contour tilt: < 0 front-loaded, > 0 swell
        double sum = 0;
        gD = c.directW * std::pow (1.0 - sh, 2.0);                      // the immediate part fades as SHAPE rises
        sum += gD * gD;
        for (int k = 0; k < n; ++k) {
            const double u = (k + 0.5) / n;
            g[k] = std::exp (a * (u - 0.5)) / std::sqrt ((double) n);
            sum += g[k] * g[k];
            tapL[k] = std::max (1.0, (k + 0.25 + 0.5 * jitL[k]) / n * T * 0.001 * fs); tapR[k] = std::max (1.0, (k + 0.25 + 0.5 * jitR[k]) / n * T * 0.001 * fs);
        }
        const double nrm = 1.0 / std::sqrt (std::max (1e-6, sum));     // the same energy goes in, only spread in time
        gD *= nrm; for (int k = 0; k < n; ++k) g[k] *= nrm;
        for (int k = 0; k < n; ++k)
        {
            bL[k] = tapL[k]; bR[k] = tapR[k]; bG[k] = g[k];
            if (edMs && edDb)
            {
                const double d = edMs[k] * 0.001 * fs;
                tapL[k] = std::max (1.0, std::min (0.79 * fs, tapL[k] + d)); tapR[k] = std::max (1.0, std::min (0.79 * fs, tapR[k] + d));
                g[k] *= edDb[k] <= -79.0 ? 0.0 : std::pow (10.0, edDb[k] / 20.0);
            }
        }
    }
    int baseTaps (double* ms, double* db, int max, double offsetMs) const
    {
        if (! on) return 0;
        const int c = std::min (n, max);
        for (int k = 0; k < c; ++k) { ms[k] = offsetMs + 0.5 * (bL[k] + bR[k]) / fs * 1000.0; db[k] = 20.0 * std::log10 (std::max (1e-6, bG[k])); }
        return c;
    }
    inline void process (double& L, double& R) {
        if (! on) return;
        dl[0].push (L); dl[1].push (R);
        double oL = L * gD, oR = R * gD;
        for (int k = 0; k < n; ++k) { oL += g[k] * dl[0].readI ((int) tapL[k]); oR += g[k] * dl[1].readI ((int) tapR[k]); }
        L = oL; R = oR;
    }
};

struct ReverbAlgo : AlgoBase {       // aRev6 / aRev2 / aRHall
    pv::TankAlgo tank; pv::PlateAlgo plate; Preechoes pe; Line pdL, pdR; OnePole hfc[2]; Biquad shelf[2]; EnvBuilder env;
    // every reflection you can edit: the build-up taps (0-19), the preechoes (20-25), the hall's own early taps (26-)
    static constexpr int kPe = 20, kTk = 26;
    int baseTaps (double* ms, double* db, int max) const override
    {
        if (max < kTk) return 0;
        for (int k = 0; k < kTk; ++k) { ms[k] = 0.0; db[k] = -200.0; }
        env.baseTaps (ms, db, EnvBuilder::K, pdS / fs * 1000.0);
        for (int i = 0; i < pe.n; ++i) if (pe.bG[i] > 0.0) { ms[kPe + i] = pe.bMs[i]; db[kPe + i] = 20.0 * std::log10 (pe.bG[i]); }
        int n = kTk;
        if (! usePlate) n += tank.baseTaps (ms + kTk, db + kTk, max - kTk, pdS / fs * 1000.0, early * wetG);
        return n;
    }
    bool usePlate = false, random = false, shelfOn = false; double pdS = 1, mixD = 0, mixW = 1, mod = 0.5, early = 0.7, dampA = 1, wetG = 1;
    int algo;
    explicit ReverbAlgo (int a) : algo (a) {}
    void prepare (double sr) override {
        fs = sr; tank.init (4, fs); tank.fs = fs; plate.init (fs); plate.fs = fs; pe.prepare (fs); env.prepare (fs);
        pdL.init ((int) (0.55 * fs)); pdR.init ((int) (0.55 * fs));
    }
    void reset() override { tank.clear(); plate.clear(); pe.reset(); pdL.clear(); pdR.clear(); env.reset(); hfc[0].reset(); hfc[1].reset(); shelf[0].reset(); shelf[1].reset(); }
    void set (const double* v, int bank) override {
        random = algo == aRHall;
        const bool wasPlate = usePlate; usePlate = bank == 4;
        if (wasPlate != usePlate) reset();
        double rt = v[0];
        if (random && v[0] >= 69.9) rt = 1.0e4;                       // Infinite
        const double shp = v[1], spr = v[2], siz = v[3], hfcHz = v[4];
        pdS = msToS (v[5], fs);
        const auto& cal = envCal();
        const double bas = std::pow (std::max (0.05, v[6]), cal.basQ), xov = v[7], rtc = v[8], dif = v[9] / 99.0;
        const double size = clampf (0.55 + (siz - 10.0) / 28.0 * 0.6, 0.5, 1.4);
        const double build = clampf (3.0 + spr / 255.0 * 60.0, 3, 230);
        early = clampf ((1.0 - shp / 255.0 * 0.8) * cal.erScale, 0.0, 1.5);
        const double dens = clampf (cal.densBase + cal.densDif * (dif - 0.8) - cal.densSize * siz / 40.0 - cal.densSpr * spr / 255.0, 0.05, 1.0);
        env.edMs = tapEdMs; env.edDb = tapEdDb; env.edVer = tapEdVer;
        env.set (shp, spr, siz, dens);
        const double dampHz = rtc >= 21000.0 ? 20000.0 : rtc;
        double spin = -1.0; mod = 0.5;
        if (random) { spin = std::max (0.05, v[16] / 50.0); mod = clampf (v[17] / 10.0, 0.02, 1.0); }
        if (usePlate) {
            dampA = curveMul ? 1.0 : 1.0 - std::exp (-2.0 * kPi * std::min (dampHz, 0.45 * fs) / fs);
            plate.curveT = curveFor (std::min (rt, 70.0));
            plate.update (rt, size, bas, 1.0, build, dif * dens, pv::plateEffect (0, 0, 0.0), kSmooth);
        } else {
            tank.curveT = curveFor (std::min (rt, 70.0)); tank.erAttackMs = erAttackMs;
            tank.edMs = tapEdMs + kTk; tank.edDb = tapEdDb + kTk; tank.edMax = kMaxTaps - kTk;
            tank.update (rt, dampHz, size, bas, 1.0, build, dif * dens, pv::surfaceEffect (0, 0, 0.0, 3.0 * size), kSmooth, spin, xov);
        }
        for (auto& h : hfc) h.setCut (fs, hfcHz >= 21000.0 ? 0.45 * fs : hfcHz);
        sinCos (v[11], mixD, mixW);
        double lv[6], dl[6]; int npe = 0;
        if (algo == aRev6) { npe = 6; for (int i = 0; i < 6; ++i) { lv[i] = v[12 + i]; dl[i] = v[18 + i]; } }
        else if (algo == aRev2) { npe = 2; for (int i = 0; i < 2; ++i) { lv[i] = v[12 + i]; dl[i] = v[18 + i]; } }
        else { npe = 4; for (int i = 0; i < 4; ++i) { lv[i] = v[12 + i]; dl[i] = v[18 + i]; } }
        pe.set (fs, lv, dl, npe, dif, tapEdMs + kPe, tapEdDb + kPe);
        wetG = dbToGain ((usePlate ? 4.0 : tankTrimDb (std::min (rt, 30.0))));
        shelfOn = false;
        if (random) {
            wetG *= v[23] / 160.0;                                      // RLV: 160 = nominal
            shelfOn = v[22] > 0.5;
            if (shelfOn) for (auto& s : shelf) s.highShelf (fs, 3000.0, -12.0 * v[22] / 255.0, 0.7);
        }
    }
    void process (double& L, double& R, const Ctx&) override {
        const double xL = L, xR = R;
        double pl = pdL.read (pdS), pr = pdR.read (pdS);
        pdL.push (xL); pdR.push (xR);
        env.process (pl, pr);                                            // SHAPE / SPREAD build-up
        double oL, oR;
        if (usePlate) plate.process (pl, pr, dampA, mod, early, oL, oR);
        else tank.process (pl, pr, mod, early, oL, oR);
        double eL, eR; pe.process (xL, xR, fs, eL, eR);
        double wL = oL * wetG + eL, wR = oR * wetG + eR;
        if (shelfOn) { wL = shelf[0].process (wL); wR = shelf[1].process (wR); }
        wL = hfc[0].lp (wL); wR = hfc[1].lp (wR);                      // HF CUTOFF: preechoes and reverb
        { const double xL_ = xL, xR_ = xR, wL_ = wL, wR_ = wR; mixOut (L, R, xL_, xR_, wL_, wR_, mixD, mixW); }
    }
};

// ---------------------------------------------------------------- Ambience: a dense, modulated cloud plus a short tail
struct AmbienceAlgo : AlgoBase {
    static constexpr int kN = 6;
    pv::TankAlgo tank; Line pdL, pdR, inD[2], cl[2][kN]; RandLfo lfo[2][kN]; OnePole rol[2];
    double len[kN] = {}, pdS = 1, indS = 1, wanS = 0, gDif = 0.6, late = 0.7, mixD = 0, mixW = 1, trim = 1;
    static constexpr double kMs[kN] = { 3.1, 4.7, 6.3, 8.9, 11.3, 14.9 };
    void prepare (double sr) override {
        fs = sr; tank.init (4, fs); tank.fs = fs;
        pdL.init ((int) (0.32 * fs)); pdR.init ((int) (0.32 * fs)); inD[0].init ((int) (0.32 * fs)); inD[1].init ((int) (0.32 * fs));
        for (int c = 0; c < 2; ++c) for (int i = 0; i < kN; ++i) { cl[c][i].init ((int) ((kMs[i] * 2.2 + 45.0) * 0.001 * fs)); lfo[c][i].init ((uint32_t) (c * 31 + i * 7 + 5)); }
    }
    void reset() override { tank.clear(); pdL.clear(); pdR.clear(); inD[0].clear(); inD[1].clear(); for (auto& c : cl) for (auto& l : c) l.clear(); rol[0].reset(); rol[1].reset(); }
    void set (const double* v, int) override {
        const double rt = v[0], siz = v[2];
        const double sc = clampf (siz / 18.0, 0.06, 2.2);
        for (int i = 0; i < kN; ++i) len[i] = kMs[i] * sc * 0.001 * fs;
        gDif = 0.35 + 0.4 * v[4] / 99.0;
        late = v[1] / 100.0;
        tank.curveT = curveFor (rt); tank.erAttackMs = erAttackMs;
        tank.edMs = tapEdMs; tank.edDb = tapEdDb; tank.edMax = kMaxTaps;
        tank.update (rt, 9000.0, clampf (0.5 + siz / 40.0 * 0.8, 0.5, 1.3), 1.0, 1.0, clampf (siz * 2.0, 3.0, 80.0), v[4] / 99.0,
                     pv::surfaceEffect (0, 0, 0.0, 2.0), kSmooth, std::max (0.05, v[6] / 50.0), 500.0);
        for (auto& r : rol) r.setCut (fs, v[3] >= 21000.0 ? 0.45 * fs : v[3]);
        const double rate = std::max (0.05, v[6] / 40.0);
        for (auto& c : lfo) for (int i = 0; i < kN; ++i) c[i].setRate (fs, rate * (0.7 + 0.1 * (double) i));
        wanS = v[7] * 0.001 * fs;
        pdS = msToS (v[8], fs); indS = msToS (v[9], fs);
        sinCos (v[5], mixD, mixW);
        trim = dbToGain (tankTrimDb (rt) - 2.0);
    }
    int baseTaps (double* ms, double* db, int max) const override { return tank.baseTaps (ms, db, max, pdS / fs * 1000.0, 0.4 * late * trim); }
    void process (double& L, double& R, const Ctx&) override {
        inD[0].push (L); inD[1].push (R);
        const double dL = inD[0].read (indS), dR = inD[1].read (indS);
        double a = pdL.read (pdS), b = pdR.read (pdS); pdL.push (L); pdR.push (R);
        for (int i = 0; i < kN; ++i) {
            a = pv::allpass (cl[0][i], len[i] + wanS * (1.0 + lfo[0][i].next()) + 1.0, gDif, a);
            b = pv::allpass (cl[1][i], len[i] * 1.07 + wanS * (1.0 + lfo[1][i].next()) + 1.0, gDif, b);
        }
        double tL, tR; tank.process (a, b, 0.4, 0.3, tL, tR);
        double wL = 0.9 * a + late * tL * trim, wR = 0.9 * b + late * tR * trim;
        wL = rol[0].lp (wL); wR = rol[1].lp (wR);
        { const double xL_ = dL, xR_ = dR, wL_ = wL, wR_ = wR; mixOut (L, R, xL_, xR_, wL_, wR_, mixD, mixW); }
    }
};

// ---------------------------------------------------------------- Classic Cart halls and plates (224XL-style)
struct ClassicAlgo : AlgoBase {
    pv::TankAlgo tank; pv::PlateAlgo plate; bool isPlate; Line pdL, pdR; OnePole hfc[2];
    double pdS = 1, mixD = 0, mixW = 1, mod = 0.4, early = 0.4, dampA = 1, wetG = 1;
    explicit ClassicAlgo (bool p) : isPlate (p) {}
    void prepare (double sr) override { fs = sr; tank.init (pv::Hall, fs); tank.fs = fs; plate.init (fs); plate.fs = fs; pdL.init ((int) (0.55 * fs)); pdR.init ((int) (0.55 * fs)); }
    void reset() override { tank.clear(); plate.clear(); pdL.clear(); pdR.clear(); hfc[0].reset(); hfc[1].reset(); }
    void set (const double* v, int) override {
        const double bas = v[0], mid = v[1], xov = v[2], tdc = v[3] >= 21000.0 ? 20000.0 : v[3];
        early = v[4] / 99.0 * 1.2; pdS = msToS (v[5], fs);
        const double dif = v[6] / 99.0; mod = v[7] / 99.0;
        const double size = clampf (0.6 + (v[8] - 8.0) / 32.0 * 0.8, 0.5, 1.4);
        const double lowMul = clampf (bas / std::max (0.05, mid), 0.25, 4.0);
        plate.curveT = tank.curveT = curveFor (mid); tank.erAttackMs = erAttackMs;
        if (isPlate) { dampA = curveMul ? 1.0 : 1.0 - std::exp (-2.0 * kPi * std::min (tdc, 0.45 * fs) / fs); plate.update (mid, size, lowMul, 1.0, 8.0, dif, pv::plateEffect (0, 0, 0.0), kSmooth); }
        else { tank.edMs = tapEdMs; tank.edDb = tapEdDb; tank.edMax = kMaxTaps;
               tank.update (mid, tdc, size, lowMul, 1.0, 15.0 + 40.0 * v[4] / 99.0, dif, pv::surfaceEffect (0, 0, 0.0, 3.0 * size), kSmooth, -1.0, xov); }
        for (auto& h : hfc) h.setCut (fs, v[9] >= 21000.0 ? 0.45 * fs : v[9]);
        sinCos (v[11], mixD, mixW);
        wetG = dbToGain (isPlate ? 4.5 : tankTrimDb (mid) - 2.0);
    }
    int baseTaps (double* ms, double* db, int max) const override { return isPlate ? 0 : tank.baseTaps (ms, db, max, pdS / fs * 1000.0, early * wetG); }
    void process (double& L, double& R, const Ctx&) override {
        const double pl = pdL.read (pdS), pr = pdR.read (pdS); pdL.push (L); pdR.push (R);
        double oL, oR;
        if (isPlate) plate.process (pl, pr, dampA, mod, early, oL, oR); else tank.process (pl, pr, mod, early, oL, oR);
        oL = hfc[0].lp (oL * wetG); oR = hfc[1].lp (oR * wetG);
        { const double xL_ = L, xR_ = R, wL_ = oL, wR_ = oR; mixOut (L, R, xL_, xR_, wL_, wR_, mixD, mixW); }
    }
};

// ============================================================================ Effects (bank 5): up to 40 randomly wandering delay voices
struct EffectsAlgo : AlgoBase {
    static constexpr int kV = 40;
    Line line[2], fbLine[2], ind[2], df[2][4]; RandLfo lfo[kV]; Biquad hp[2]; bool hpOn[2] = {};
    double base[kV] = {}, gain[kV] = {}, wanS = 0, fbG = 0, fbS = 1, indS = 1, gDif = 0, mixD = 0, mixW = 1, fbL = 0, fbR = 0, tremRate = 0, tremPh = 0;
    int num = 40, mon = 0; bool tremolo = false;
    static constexpr double kDf[4] = { 1.7, 3.1, 4.3, 6.1 };
    void prepare (double sr) override {
        fs = sr;
        for (int c = 0; c < 2; ++c) { line[c].init ((int) (1.6 * fs)); fbLine[c].init ((int) (0.6 * fs)); ind[c].init ((int) (0.65 * fs)); for (int i = 0; i < 4; ++i) df[c][i].init ((int) (0.012 * fs)); }
        for (int i = 0; i < kV; ++i) lfo[i].init ((uint32_t) (i * 13 + 7));
    }
    void reset() override { for (int c = 0; c < 2; ++c) { line[c].clear(); fbLine[c].clear(); ind[c].clear(); for (auto& d : df[c]) d.clear(); hp[c].reset(); } fbL = fbR = 0; }
    void set (const double* v, int) override {
        const double spn = v[0], slp = v[1] / 255.0, lng = v[2], pdl = v[5];
        num = std::max (1, std::min (kV, (int) std::lround (v[4])));
        wanS = v[3] * 0.001 * fs;
        double rate = 0.08 * std::pow (2.0, spn / 8.0) * std::sqrt (40.0 / (double) num);
        rate = std::min (rate, 20.0);
        for (int i = 0; i < kV; ++i) lfo[i].setRate (fs, rate * (0.85 + 0.3 * (double) ((i * 7) % 11) / 10.0));
        double sumSq = 0;
        for (int j = 0; j < num; ++j) {
            const double f = (double) (j + 1) / (double) num;
            base[j] = (pdl + lng * f) * 0.001 * fs + 2.0;
            const double db = -30.0 * ((1.0 - slp) * f + slp * (1.0 - f));     // SLOPE: down = decaying, up = building
            gain[j] = dbToGain (db) * ((v[14] < 0.5 && (j & 1)) ? -1.0 : 1.0);   // SIGNS 0 = alternating
            sumSq += gain[j] * gain[j];
        }
        const double nrm = 1.0 / std::sqrt (std::max (1.0e-6, sumSq * 0.5));     // constant loudness
        for (int j = 0; j < num; ++j) gain[j] *= nrm * (v[14] > 0.5 ? 1.4 : 1.0);
        mon = (int) v[6];
        fbG = lvGain (v[7]); fbS = msToS (v[8], fs);
        gDif = 0.7 * v[9] / 99.0;
        indS = msToS (v[10], fs);
        sinCos (v[11], mixD, mixW);
        for (int c = 0; c < 2; ++c) { hpOn[c] = v[12 + c] > 5.0; if (hpOn[c]) hp[c].highpass (fs, v[12 + c], 0.707); }
        tremolo = lng < 0.5 && v[3] < 0.001;
        tremRate = 0.15 * std::pow (2.0, spn / 8.0) / 8.0;
    }
    void process (double& L, double& R, const Ctx&) override {
        double xL = L, xR = R;
        ind[0].push (L); ind[1].push (R);
        const double dL = ind[0].read (indS), dR = ind[1].read (indS);
        if (hpOn[0]) xL = hp[0].process (xL);
        if (hpOn[1]) xR = hp[1].process (xR);
        const double m = 0.5 * (xL + xR);
        double iL = xL, iR = xR;
        switch (mon) { case 1: iL = iR = m; break; case 2: iL = m; iR = -m; break; case 3: iL = xR; iR = xL; break; default: break; }
        if (tremolo) {                                   // Tremolo L&R: the mono blend moves between the outputs
            tremPh += tremRate / fs; if (tremPh >= 1) tremPh -= 1;
            const double a = 0.5 + 0.5 * std::sin (2.0 * kPi * tremPh);
            const double wL = m * std::sqrt (a) * 1.4, wR = m * std::sqrt (1 - a) * 1.4;
            { const double xL_ = dL, xR_ = dR, wL_ = wL, wR_ = wR; mixOut (L, R, xL_, xR_, wL_, wR_, mixD, mixW); } return;
        }
        iL += fbG * fbLine[0].read (fbS); iR += fbG * fbLine[1].read (fbS);
        for (int i = 0; i < 4; ++i) { iL = pv::allpass (df[0][i], kDf[i] * 0.001 * fs, gDif, iL); iR = pv::allpass (df[1][i], kDf[i] * 1.13 * 0.001 * fs, gDif, iR); }
        line[0].push (iL); line[1].push (iR);
        double wL = 0, wR = 0;
        for (int j = 0; j < num; ++j) {
            const double d = std::max (1.0, base[j] + wanS * lfo[j].next());
            if ((j & 1) == 0) wL += gain[j] * line[0].read (d); else wR += gain[j] * line[1].read (d);
        }
        if (num == 1) wR = wL;
        fbLine[0].push (wL); fbLine[1].push (wR);
        { const double xL_ = dL, xR_ = dR, wL_ = wL, wR_ = wR; mixOut (L, R, xL_, xR_, wL_, wR_, mixD, mixW); }
    }
};

// ============================================================================ Twin Delays (bank 6)
struct TwinAlgo : AlgoBase {
    Line ln[4]; OnePole rol[2]; bool rolOn[2] = {};
    double tgt[4] = {}, cur[4] = {}, lv[4] = {}, fb[4] = {}, dry[2] = {}, pan[2][2] = {}, sm = 0.001;
    void prepare (double sr) override { fs = sr; for (auto& l : ln) l.init ((int) (3.1 * fs)); sm = 1.0 - std::exp (-1.0 / (0.05 * fs)); }
    void reset() override { for (auto& l : ln) l.clear(); rol[0].reset(); rol[1].reset(); for (int i = 0; i < 4; ++i) cur[i] = tgt[i]; }
    void set (const double* v, int) override {
        dry[0] = lvGain (v[0]); dry[1] = lvGain (v[1]);
        for (int c = 0; c < 2; ++c) { rolOn[c] = v[2 + c] < 21000.0; rol[c].setCut (fs, rolOn[c] ? v[2 + c] : 0.45 * fs); }
        for (int c = 0; c < 2; ++c) { const double a = clampf (v[4 + c], 0, 100) * 0.01 * 0.5 * kPi; pan[c][0] = std::cos (a); pan[c][1] = std::sin (a); }
        const double mst = v[20];
        const double fine[4] = { v[18], v[19], v[18], v[19] };
        for (int i = 0; i < 4; ++i) {
            const int b = 6 + i * 3;
            tgt[i] = std::max (1.0, std::min (3.0 * fs, v[b] * mst * 0.001 * fs + fine[i]));
            lv[i] = lvGain (v[b + 1]); fb[i] = v[b + 2] * 0.01;
        }
    }
    void process (double& L, double& R, const Ctx&) override {
        for (int i = 0; i < 4; ++i) cur[i] += sm * (tgt[i] - cur[i]);
        double o1 = ln[0].read (cur[0]), o2 = ln[1].read (cur[1]);
        if (rolOn[0]) o1 = rol[0].lp (o1);
        if (rolOn[1]) o2 = rol[1].lp (o2);
        const double o3 = ln[2].read (cur[2]), o4 = ln[3].read (cur[3]);
        ln[0].push (L + fb[0] * o1); ln[1].push (R + fb[1] * o2);
        ln[2].push (L + fb[3] * o4); ln[3].push (R + fb[2] * o3);       // FB3/FB4 are cross-panned
        const double a = lv[0] * o1, b = lv[1] * o2;
        const double oL = dry[0] * L + a * pan[0][0] + b * pan[1][0] + lv[2] * o3;
        const double oR = dry[1] * R + a * pan[0][1] + b * pan[1][1] + lv[3] * o4;
        L = oL; R = oR;
    }
};

// ============================================================================ Samplers (bank 7)
struct SamplerAlgo : AlgoBase {
    enum State { Idle, Armed, Recording, Ready };
    std::vector<double> buf[2]; int cap = 0;                  // circular while armed, then the captured sample [0, len)
    std::atomic<int> state { Idle }, playing { 0 }; int w = 0, len = 0, recLeft = 0, preRoll = 0, markS = 0; bool markTail = false, capKey = false, immediate = true;
    double trigLvl = 0, playTrig = 0, rate = 1, posF = 0, posR = 0, holdS = 0, holdLeft = 0, fadeIn = 1, fadeStep = 1;
    int repeats = 1, repLeft = 0, headS = 0, tailS = 0, chn = 0, fwr = 0; bool loop = false, contPlay = false, manualPlay = true, dub = false, wholeSample = false, playArmed = false;
    double lvF = 1, lvR = 1, pF[2] = { 1, 1 }, pR[2] = { 1, 1 }, order = 100; int pendKey = -1;
    void prepare (double sr) override {
        fs = sr; cap = (int) (21.9 * fs) + 16;
        for (auto& b : buf) b.assign ((size_t) cap, 0.0);
    }
    void reset() override { state = Idle; playing = 0; }
    void set (const double* v, int) override {
        markS = (int) (v[2] * 0.001 * fs); markTail = v[2] >= 999.0;
        immediate = v[3] <= -60.5; capKey = v[3] >= 0.5; trigLvl = dbToGain (v[3]);
        headS = (int) (v[6] * 0.001 * fs); tailS = (int) (v[7] * 0.001 * fs);
        fadeStep = v[8] > 0.5 ? 1.0 : 1.0 / (0.005 * fs);
        rate = v[9] * 0.01;
        loop = v[10] >= 99.5; repeats = std::max (1, (int) v[10]);
        contPlay = v[13] <= -60.5; manualPlay = v[13] >= 0.5; playTrig = dbToGain (v[13]);
        holdS = v[14] * 0.001 * fs;
        lvF = lvGain (v[15]); { const double a = v[16] * 0.01 * 0.5 * kPi; pF[0] = std::cos (a) * 1.41; pF[1] = std::sin (a) * 1.41; }
        order = v[17];
        lvR = lvGain (v[18]); { const double a = v[19] * 0.01 * 0.5 * kPi; pR[0] = std::cos (a) * 1.41; pR[1] = std::sin (a) * 1.41; }
        const int want = std::min (cap - 8, (int) (v[20] * fs));
        if (want != len && state != Recording) { len = want; }
        chn = (int) v[21]; fwr = (int) v[22];
    }
    void key (int slot) override { pendKey = slot; }      // handled on the audio thread
    void startPlay (bool whole) {
        if (len <= 0) return;
        wholeSample = whole;
        const int a = whole ? 0 : std::min (len - 1, headS), b = whole ? len : std::max (a + 1, len - tailS);
        posF = rate >= 0 ? (double) a : (double) (b - 1); posR = (double) (b - 1);
        repLeft = loop ? 1 << 30 : repeats; fadeIn = fadeStep >= 1.0 ? 1.0 : 0.0; playing = 1;
    }
    std::string status() const override {
        switch (state.load()) {
            case Armed: return "ARMED - WAITING";
            case Recording: return "CAPTURING ******";
            default: return playing.load() ? "PLAYING" : (len > 0 && state.load() == Ready ? "SAMPLE READY" : "PRESS REC");
        }
    }
    void process (double& L, double& R, const Ctx&) override {
        const double inL = L, inR = chn == 1 ? L : R;
        if (pendKey >= 0) {
            const int k = pendKey; pendKey = -1;
            if (k == 1) { state = Armed; w = 0; preRoll = 0; playing = 0; if (immediate) { state = Recording; recLeft = len; w = 0; } }   // REC
            else if (k == 4 && state == Armed) { state = Recording; recLeft = markTail ? 0 : std::max (1, len - std::min (markS, preRoll)); }   // CAP
            else if (k == 5) startPlay (true);                                               // CHK
            else if (k == 11) startPlay (false);                                             // CUE
            else if (k == 0) { dub = true; startPlay (true); }                               // DUB
            else if (k == 12) { if (manualPlay) startPlay (false); else playArmed = true; }   // PLAY
        }
        // ---- recording
        const int st = state;
        if (st == Armed || st == Recording) {
            buf[0][(size_t) (w % std::max (1, len))] = inL; buf[1][(size_t) (w % std::max (1, len))] = inR;
            ++w; ++preRoll;
            if (st == Armed && ! capKey && ! immediate && std::fabs (inL) > trigLvl) {
                if (markTail) { recLeft = 0; state = Recording; }
                else { state = Recording; recLeft = std::max (1, len - std::min (markS, preRoll)); }
            }
            if (state == Recording && --recLeft <= 0) {
                // rotate so the sample starts at its first captured frame
                const int n = std::max (1, len), start = preRoll >= n ? w % n : 0;
                if (start != 0) for (auto& b : buf) std::rotate (b.begin(), b.begin() + start, b.begin() + n);
                state = Ready; dub = false;
            }
        }
        // ---- playback triggers
        if (! playing.load() && state == Ready && len > 0) {
            if (holdLeft > 0) holdLeft -= 1;
            else if (contPlay) startPlay (false);
            else if (playArmed && std::fabs (inL) > playTrig) { startPlay (false); holdLeft = holdS; }
        }
        double oL = 0, oR = 0;
        if (playing.load()) {
            const int a = wholeSample ? 0 : std::min (len - 1, headS), b = wholeSample ? len : std::max (a + 1, len - tailS);
            auto rd = [this] (int c, double p) { const int i = (int) p; const double f = p - (double) i; const double x0 = buf[c][(size_t) i], x1 = buf[c][(size_t) std::min (len - 1, i + 1)]; return x0 + (x1 - x0) * f; };
            fadeIn = std::min (1.0, fadeIn + fadeStep);
            const double fadeOut = std::min (1.0, (rate >= 0 ? (double) b - posF : posF - (double) a) / (0.005 * fs));
            const double env = fadeIn * clampf (fadeOut, 0, 1);
            const bool fwdOn = fwr == 0 || order >= 25.0 || posR < (double) a;   // fwd/rev ordering
            if (fwdOn && posF >= a && posF < b) {
                const double sL = rd (0, posF), sR = rd (chn == 1 ? 0 : 1, posF);
                if (fwr == 1) { const double m = 0.5 * (sL + sR) * lvF * env; oL += m * pF[0]; oR += m * pF[1]; }
                else { oL += sL * env; oR += sR * env; }
            }
            if (fwr == 1 && (order <= 75.0 || posF >= b) && posR >= a) {
                const double m = 0.5 * (rd (0, posR) + rd (chn == 1 ? 0 : 1, posR)) * lvR * env;
                oL += m * pR[0]; oR += m * pR[1];
                posR -= std::fabs (rate);
            }
            if (dub && posF >= a && posF < b) { const int i = (int) posF; buf[0][(size_t) i] += inL; buf[1][(size_t) i] += inR; }
            if (fwdOn || fwr == 0) posF += fwr == 1 ? std::fabs (rate) : rate;
            const bool doneF = rate >= 0 ? posF >= b : posF < a;
            const bool doneR = fwr == 0 || posR < a;
            if (doneF && doneR) {
                if (--repLeft > 0) { posF = rate >= 0 ? (double) a : (double) (b - 1); posR = (double) (b - 1); }
                else { playing = 0; dub = false; if (! contPlay && ! manualPlay) playArmed = true; }
            }
        }
        L = inL + oL; R = inR + oR;
    }
};

// ============================================================================ Pitch Change (bank 8)
struct PitchAlgo : AlgoBase {
    Line pre[2], sh[2]; double ph[2] = {}, ratio[2] = { 1, 1 }, winS = 1600, pdS[2] = { 1, 1 }, fb[2] = {}, last[2] = {}, mixD = 0, mixW = 1;
    void prepare (double sr) override { fs = sr; for (int c = 0; c < 2; ++c) { pre[c].init ((int) (0.85 * fs)); sh[c].init ((int) (0.25 * fs)); } }
    void reset() override { for (int c = 0; c < 2; ++c) { pre[c].clear(); sh[c].clear(); last[c] = 0; ph[c] = 0; } }
    void key (int slot) override { if (slot == 1) { ph[0] = ph[1] = 0; } }          // SNC: resync
    void set (const double* v, int) override {
        const bool mono = v[0] > 0.5;
        const double gl = v[16], gr = v[22];                                         // glide = the running pitch
        const double semL = gl, semR = mono ? gr : gl;
        ratio[0] = std::pow (2.0, semL / 12.0); ratio[1] = std::pow (2.0, semR / 12.0);
        winS = (8.0 + v[17] * 1.5) * 0.001 * fs;
        pdS[0] = msToS (v[6], fs); pdS[1] = msToS (v[7], fs);
        fb[0] = v[8] * 0.01; fb[1] = v[9] * 0.01;
        sinCos (v[11], mixD, mixW);
    }
    void process (double& L, double& R, const Ctx&) override {
        double x[2] = { L, R }, y[2];
        for (int c = 0; c < 2; ++c) {
            pre[c].push (x[c] + fb[c] * last[c]);
            const double p = pre[c].read (pdS[c]);
            sh[c].push (p);
            ph[c] += (1.0 - ratio[c]) / winS; ph[c] -= std::floor (ph[c]);
            const double p2 = ph[c] + 0.5 - std::floor (ph[c] + 0.5);
            const double a = sh[c].read (2.0 + ph[c] * winS), b = sh[c].read (2.0 + p2 * winS);
            y[c] = a * std::sin (kPi * ph[c]) + b * std::sin (kPi * p2);
            last[c] = y[c];
        }
        { const double xL_ = x[0], xR_ = x[1], wL_ = y[0], wR_ = y[1]; mixOut (L, R, xL_, xR_, wL_, wR_, mixD, mixW); }
    }
};

// ============================================================================ Doppler (bank 8)
struct DopplerAlgo : AlgoBase {
    Line ln; std::atomic<int> run { 0 }; bool armed = false; double t = 0; double T = 6, v = 30, d = 30, dP = 30, zoom = 0, trig = 1, xf = 0; bool manual = true; int pendKey = -1;
    void prepare (double sr) override { fs = sr; ln.init ((int) (3.0 * fs)); }
    void reset() override { ln.clear(); run = 0; armed = false; xf = 0; }
    void key (int slot) override { if (slot == 0 || slot == 6) pendKey = slot; }
    void set (const double* p, int) override {
        zoom = p[1]; T = p[4]; v = p[3] / 3.6; d = std::max (0.05, p[5]); dP = d * std::max (0.1, p[2]) / 64.0;
        manual = p[11] >= 0.5; trig = dbToGain (p[11]);
    }
    std::string status() const override { return run.load() ? "DOPPLER RUNNING" : (armed ? "ARMED" : "PRESS PLAY"); }
    void process (double& L, double& R, const Ctx&) override {
        const double m = 0.5 * (L + R);
        ln.push (m);
        if (pendKey >= 0) { pendKey = -1; if (manual) { run = 1; t = 0; } else armed = true; }
        if (armed && std::fabs (m) > trig) { armed = false; run = 1; t = 0; }
        double oL = L, oR = R;
        if (run.load()) {
            const double x = (double) (v * (t - T));
            const double r = std::sqrt (x * x + d * d), rp = std::sqrt (x * x + dP * dP);
            const double delayS = std::min (2.9 * fs, std::max (1.0, rp / 343.0 * fs));
            double g = d / r; if (zoom > 0.5) g *= g;
            const double s = ln.read (delayS) * g;
            const double a = (0.5 + 0.5 * x / r) * 0.5 * kPi;
            xf = std::min (1.0, xf + 1.0 / (0.02 * fs));
            oL = L * (1 - xf) + s * std::cos (a) * 1.41 * xf; oR = R * (1 - xf) + s * std::sin (a) * 1.41 * xf;
            t += 1.0 / fs;
            if (t >= 2.0 * T) run = 0;
        } else if (xf > 0) { xf = std::max (0.0, xf - 1.0 / (0.02 * fs)); }
        L = oL; R = oR;
    }
};

// ============================================================================ Mastering (bank 9)
struct InOutAlgo : AlgoBase { void process (double&, double&, const Ctx&) override {} };

inline void balGains (double bal, double& gL, double& gR) {     // sine/cosine balance, unity at centre
    const double a = (clampf (bal, -1, 1) + 1.0) * 0.25 * kPi;
    gL = std::min (1.0, std::cos (a) * 1.41421); gR = std::min (1.0, std::sin (a) * 1.41421);
}
inline double shelfGain (double db) { return db <= -17.9 ? 0.0 : dbToGain (db); }

struct StereoAdjAlgo : AlgoBase {          // Stereo Adjust and PONS Adjust
    bool pons; FirstOrder bas[2], trb[2], tb[2], spcLp[2], emp[2], shufLp; OnePole dcB[2];
    double g = 1, gL = 1, gR = 1, rc = 1, rs = 0, spc = 0, shuf = 0, dcL = 0, dcR = 0, frac = 0, prevL = 0, err[2][2] = {};
    int inv = 0; bool dly = false, empOn = false, autoDc = false, dither = false; uint32_t seed = 99;
    explicit StereoAdjAlgo (bool isPons) : pons (isPons) {}
    void reset() override { for (int c = 0; c < 2; ++c) { bas[c].reset(); trb[c].reset(); tb[c].reset(); spcLp[c].reset(); emp[c].reset(); dcB[c].reset(); } shufLp.reset(); prevL = 0; }
    void set (const double* v, int) override {
        g = (v[0] <= -71.9 ? 0.0 : dbToGain (v[0] + v[1]));
        balGains (v[2], gL, gR);
        const double a = v[3] * kPi / 180.0; rc = std::cos (a); rs = std::sin (a);
        if (! pons) {
            const double xov = v[6], hfc = v[7], hfi = v[8];
            for (int c = 0; c < 2; ++c) { bas[c].lowShelf (fs, xov, shelfGain (v[4])); trb[c].highShelf (fs, hfc, shelfGain (v[5])); tb[c].highShelf (fs, hfi, shelfGain (v[10 + c])); spcLp[c].lowpass (fs, xov); }
            spc = v[9] / 99.0; dly = v[12] > 0.5; empOn = v[13] > 0.5; inv = (int) v[14];
            shufLp.lowpass (fs, 100.0 * std::pow (2.0, v[16] / 64.0)); shuf = v[17] / 99.0;
            autoDc = (int) v[18] == 3; dcL = v[19] * 0.01 * 0.063; dcR = v[20] * 0.01 * 0.063;
            for (auto& e : emp) e.highShelf (fs, 3183.0, 0.3);   // 50/15 us de-emphasis: pole 3.18 kHz, zero 10.6 kHz
        } else {
            const double xov = v[6], hfc = v[7];
            for (int c = 0; c < 2; ++c) { bas[c].lowShelf (fs, xov, shelfGain (v[9 + c])); tb[c].highShelf (fs, hfc, shelfGain (v[4 + c])); trb[c].highShelf (fs, hfc, 1.0); spcLp[c].lowpass (fs, xov); }
            spc = v[8] / 99.0; inv = (int) v[11]; dither = v[12] > 0.5;
            shufLp.lowpass (fs, 100.0 * std::pow (2.0, v[14] / 64.0)); shuf = v[15] / 99.0;
            dly = empOn = autoDc = false; dcL = dcR = 0;
        }
        frac = 11.0e-6 * fs;
        for (auto& d : dcB) d.setCut (fs, 3.0);
    }
    void process (double& L, double& R, const Ctx&) override {
        double l = L, r = R;
        switch (inv) { case 1: r = -r; break; case 2: std::swap (l, r); break; case 3: std::swap (l, r); l = -l; break; default: break; }
        if (dly) { const double y = prevL + (l - prevL) * (1.0 - frac); prevL = l; l = y; }
        if (empOn) { l = emp[0].process (l); r = emp[1].process (r); }
        l *= g * gL; r *= g * gR;
        { const double a = l * rc - r * rs, b = r * rc + l * rs; l = a; r = b; }
        for (int c = 0; c < 2; ++c) { double& x = c ? r : l; x = tb[c].process (trb[c].process (bas[c].process (x))); }
        if (spc != 0.0) { const double cl = spcLp[0].process (l), cr = spcLp[1].process (r); l -= spc * cr; r -= spc * cl; }
        if (shuf > 0.0) { const double s = 0.5 * (l - r), m = 0.5 * (l + r), sb = s + 2.0 * shuf * shufLp.process (s); l = m + sb; r = m - sb; }
        if (autoDc) { l -= dcB[0].lp (l); r -= dcB[1].lp (r); }
        l += dcL; r += dcR;
        if (dither) {      // 16-bit with TPDF dither and 2nd-order error-feedback noise shaping
            const double q = 1.0 / 32768.0;
            auto rnd = [this] { seed = seed * 1664525u + 1013904223u; return (double) (seed >> 8) / 16777216.0; };
            for (int c = 0; c < 2; ++c) {
                double& x = c ? r : l;
                const double shaped = x - (2.0 * err[c][0] - err[c][1]);
                const double y = std::round ((shaped + (rnd() - rnd()) * q) / q) * q;
                err[c][1] = err[c][0]; err[c][0] = y - shaped; x = y;
            }
        }
        L = l; R = r;
    }
};

inline double qValue (int idx, bool page2) {
    static const double q2[] = { 0, 0.5, 0.7, 1.0, 1.5, 2.0, 3.0, 5.0, 7.0, 16, 32 };
    static const double q3[] = { 0.5, 0.7, 1.0, 1.5, 2.0, 3.0, 5.0, 7.0, 0 };
    return page2 ? q2[std::max (0, std::min (10, idx))] : q3[std::max (0, std::min (8, idx))];
}
// one parametric band: Q 0 on page two = low shelf, on page three = high shelf; notch Qs allow +-36 dB
inline void peqBand (Biquad& b, double fs, double f, int qi, double lev, bool page2) {
    const double q = qValue (qi, page2);
    f = clampf (f, 20.0, 0.45 * fs);
    if (q == 0.0) { if (page2) b.lowShelf (fs, f, clampf (lev, -12, 12), 0.7); else b.highShelf (fs, f, clampf (lev, -12, 12), 0.7); }
    else b.peak (fs, f, q, q >= 16 ? clampf (lev, -36, 36) : clampf (lev, -12, 12));
}
struct PeqAlgo : AlgoBase {
    bool mono; Biquad f[2][4]; double g = 1, gL = 1, gR = 1;
    explicit PeqAlgo (bool m) : mono (m) {}
    void reset() override { for (auto& c : f) for (auto& b : c) b.reset(); }
    void set (const double* v, int) override {
        g = v[0] <= -71.9 ? 0.0 : dbToGain (v[0] + v[1]);
        if (mono) {
            gL = gR = 1;
            const double f1 = std::pow (2.0, v[4] / 1200.0), f2 = std::pow (2.0, v[5] / 1200.0);
            for (int c = 0; c < 2; ++c) {
                peqBand (f[c][0], fs, v[6] * f1, (int) v[7], v[8], true);
                peqBand (f[c][1], fs, v[9] * f2, (int) v[10], v[11], true);
                peqBand (f[c][2], fs, v[12], (int) v[13], v[14], false);
                peqBand (f[c][3], fs, v[15], (int) v[16], v[17], false);
            }
        } else {
            balGains (v[2], gL, gR);
            const bool link = v[3] > 0.5;
            const double fL = std::pow (2.0, v[4] / 1200.0), fR = std::pow (2.0, (link ? v[4] : v[5]) / 1200.0);
            peqBand (f[0][0], fs, v[6] * fL, (int) v[7], v[8], true);
            peqBand (f[0][1], fs, v[12], (int) v[13], v[14], false);
            if (link) { peqBand (f[1][0], fs, v[6] * fR, (int) v[7], v[8], true); peqBand (f[1][1], fs, v[12], (int) v[13], v[14], false); }
            else { peqBand (f[1][0], fs, v[9] * fR, (int) v[10], v[11], true); peqBand (f[1][1], fs, v[15], (int) v[16], v[17], false); }
            for (int c = 0; c < 2; ++c) { f[c][2].set (1, 0, 0, 1, 0, 0); f[c][3].set (1, 0, 0, 1, 0, 0); }
        }
    }
    void process (double& L, double& R, const Ctx&) override {
        double l = L * g * gL, r = R * g * gR;
        for (int b = 0; b < 4; ++b) { l = f[0][b].process (l); r = f[1][b].process (r); }
        L = l; R = r;
    }
};

// Panorama: crosstalk cancellation (width) plus an L-R bass boost
struct PanoramaAlgo : AlgoBase {
    Line dl[2]; OnePole shadow[2]; FirstOrder lfb; double c1 = 1, c2 = 1, eff = 1, itd = 10, lfbG = 0, mixD = 0, mixW = 1, yL = 0, yR = 0;
    void prepare (double sr) override { fs = sr; dl[0].init ((int) (0.01 * fs)); dl[1].init ((int) (0.01 * fs)); }
    void reset() override { dl[0].clear(); dl[1].clear(); shadow[0].reset(); shadow[1].reset(); lfb.reset(); yL = yR = 0; }
    void set (const double* v, int) override {
        c1 = v[0] / 254.0; c2 = v[1] / 254.0; eff = 0.85 * v[2] / 254.0;
        const double half = 0.5 * v[3] * kPi / 180.0;
        itd = std::max (1.0, 0.0875 * (half + std::sin (half)) / 343.0 * fs);
        for (auto& s : shadow) s.setCut (fs, 2500.0);
        lfb.lowpass (fs, 250.0); lfbG = dbToGain (20.0 * v[4] / 254.0) - 1.0;
        sinCos (v[5], mixD, mixW);
    }
    void process (double& L, double& R, const Ctx&) override {
        const double xL = L * c1, xR = R * c2;
        const double cL = shadow[0].lp (dl[1].read (itd)), cR = shadow[1].lp (dl[0].read (itd));
        yL = xL - eff * cL; yR = xR - eff * cR;
        dl[0].push (yL); dl[1].push (yR);
        double oL = yL, oR = yR;
        if (lfbG > 0.0) { const double s = 0.5 * (oL - oR), b = lfbG * lfb.process (s); oL += b; oR -= b; }
        oL = clampf (oL, -4, 4); oR = clampf (oR, -4, 4);
        { const double xL_ = L, xR_ = R, wL_ = oL, wR_ = oR; mixOut (L, R, xL_, xR_, wL_, wR_, mixD, mixW); }
    }
};

// ============================================================================ Compressor/Expander (bank 10): unity at high levels, gain added below the threshold
struct CompAlgo : AlgoBase {
    Line dl[2]; double dS = 1, ratio = 2, maxG = 9, thr = -21, atkA = 0.99, relDbS = 0.01, exS = 1, exG = 0, exT = -48, ms = 0, gDb = 0, msA = 0.999;
    std::atomic<double> grUi { 0 };
    void prepare (double sr) override { fs = sr; dl[0].init ((int) (0.21 * fs)); dl[1].init ((int) (0.21 * fs)); }
    void reset() override { dl[0].clear(); dl[1].clear(); ms = 0; gDb = 0; }
    void set (const double* v, int) override {
        dS = msToS (v[0], fs); ratio = v[1]; maxG = v[2]; thr = -std::fabs (v[3]);
        const double atkMs = 7.5 * std::pow (2.0, std::floor (v[4]));
        atkA = 1.0 - std::exp (-1.0 / (atkMs * 0.001 * fs));
        relDbS = 6.0 / (std::max (5.0, v[5]) * 0.001) / fs;                 // constant dB per second
        exS = v[6]; exG = v[7]; exT = -std::fabs (v[8]);
        msA = std::exp (-1.0 / (0.01 * fs));
    }
    std::string status() const override { char b[40]; std::snprintf (b, sizeof b, "GAIN %+.1 dB", (double) grUi.load()); return b; }
    void process (double& L, double& R, const Ctx&) override {
        ms = msA * ms + (1 - msA) * 0.5 * (L * L + R * R);
        const double lv = 10.0 * std::log10 (ms + 1.0e-12) + 3.0;
        double tgt = 0.0;
        if (lv < thr) tgt += std::min (maxG, (thr - lv) * (1.0 - 1.0 / std::max (1.0, ratio)));
        if (exS > 1.001 && lv < exT) tgt -= std::min (exG, (exT - lv) * (exS - 1.0));
        if (tgt < gDb) gDb += atkA * (tgt - gDb);                               // level rising: gain comes down (attack)
        else gDb = std::min (tgt, gDb + relDbS);                               // release: constant dB/s
        grUi.store (gDb);
        const double g = dbToGain (gDb);
        dl[0].push (L); dl[1].push (R);
        L = dl[0].read (dS) * g; R = dl[1].read (dS) * g;
    }
};

// ============================================================================ Prime Time III (bank 15): two gliding delay lines
struct PrimeAlgo : AlgoBase {
    Line ln[2], ind[2]; OnePole trb[2];
    double ph[2] = {}, inc[2] = {}, amp[2] = {}, base[2] = {}, fb[2] = {}, cfb[2] = {}, pan[2][2] = {}, indS[2] = { 1, 1 }, out[2] = {}, mixD = 0, mixW = 1;
    void prepare (double sr) override { fs = sr; for (int c = 0; c < 2; ++c) { ln[c].init ((int) (0.8 * fs)); ind[c].init ((int) (0.11 * fs)); } }
    void reset() override { for (int c = 0; c < 2; ++c) { ln[c].clear(); ind[c].clear(); trb[c].reset(); out[c] = 0; } }
    void set (const double* v, int) override {
        for (int c = 0; c < 2; ++c) {
            double f = 0.05 * std::pow (2.0, v[c] / 40.0);
            f = std::min (f, fs / (4.0 * std::max (1.0, v[4 + c])));
            const double cents = v[2 + c] * 6.0, slope = 1.0 - std::pow (2.0, -cents / 1200.0);
            inc[c] = f / fs; amp[c] = slope * fs / (2.0 * kPi * f);
            base[c] = v[6 + c] * 0.001 * fs + amp[c] + 2.0;
            indS[c] = msToS (v[8 + c], fs);
            fb[c] = v[12 + c] * 0.01; cfb[c] = v[14 + c] * 0.01;
            const double a = clampf (v[16 + c], 0, 100) * 0.01 * 0.5 * kPi; pan[c][0] = std::cos (a); pan[c][1] = std::sin (a);
            trb[c].setCut (fs, v[10]);
        }
        sinCos (v[11], mixD, mixW);
    }
    void process (double& L, double& R, const Ctx&) override {
        const double x[2] = { L, R };
        ln[0].push (x[0] + fb[0] * out[0] + cfb[1] * out[1]);
        ln[1].push (x[1] + fb[1] * out[1] + cfb[0] * out[0]);
        for (int c = 0; c < 2; ++c) {
            ph[c] += inc[c]; if (ph[c] >= 1) ph[c] -= 1;
            out[c] = ln[c].read (std::max (1.0, base[c] + amp[c] * std::sin (2.0 * kPi * ph[c])));
            ind[c].push (x[c]);
        }
        const double a = trb[0].lp (out[0]), b = trb[1].lp (out[1]);
        const double wL = a * pan[0][0] + b * pan[1][0], wR = a * pan[0][1] + b * pan[1][1];
        { const double xL_ = ind[0].read (indS[0]), xR_ = ind[1].read (indS[1]), wL_ = wL, wR_ = wR; mixOut (L, R, xL_, xR_, wL_, wR_, mixD, mixW); }
    }
};

// ============================================================================ Freq. Stuff (bank 16): split-band upward compressors (+ distortion)
struct BandComp {        // upward compressor: below 'rot' the gain rises at (1 - 1/ratio), up to 'maxG'
    double ms = 0, gDb = 0, ratio = 2, rot = -20, maxG = 6, atkA = 0.1, relA = 0.001, msA = 0.999;
    void set (double fs, double slope, double rotDb, double maxDb, double atc, double rtc) {
        ratio = 1.0 + std::max (0.0, slope) / 128.0; rot = -std::fabs (rotDb); maxG = maxDb;
        atkA = 1.0 - std::exp (-1.0 / (std::pow (2.0, atc) * 0.001 * fs));
        relA = 1.0 - std::exp (-1.0 / (10.0 * std::pow (2.0, rtc) * 0.001 * fs));
        msA = std::exp (-1.0 / (0.005 * fs));
    }
    void reset() { ms = 0; gDb = 0; }
    inline double gain (double x) {
        ms = msA * ms + (1 - msA) * x * x;
        const double lv = 10.0 * std::log10 (ms + 1.0e-12) + 3.0;
        const double tgt = lv < rot ? std::min (maxG, (rot - lv) * (1.0 - 1.0 / ratio)) : 0.0;
        gDb += (tgt < gDb ? atkA : relA) * (tgt - gDb);
        return dbToGain (gDb);
    }
};
struct FreqAlgo : AlgoBase {
    bool dist; Line dl[2]; OnePole lo[2], hfc[2], env[2]; FirstOrder dhp[2]; Biquad hi[2]; BandComp cl[2], ch[2];
    double dS = 1, gLow = 1, gHigh = 1, gate = -94, gateG[2] = { 1, 1 }, gateA = 0.01, dstD = 0, dstW = 1, exm = 1, drive = 1, expS = 0, susA = 0.001, expEnv[2] = {};
    bool hfcOn = false;
    explicit FreqAlgo (bool d) : dist (d) {}
    void prepare (double sr) override { fs = sr; dl[0].init ((int) (0.035 * fs)); dl[1].init ((int) (0.035 * fs)); }
    void reset() override { for (int c = 0; c < 2; ++c) { dl[c].clear(); lo[c].reset(); hfc[c].reset(); hi[c].reset(); dhp[c].reset(); cl[c].reset(); ch[c].reset(); gateG[c] = 1; expEnv[c] = 0; } }
    void set (const double* v, int) override {
        double xov, hf, xom, tsl, tbr, tbm, lsl, lfr, lmg, atc, rtc, dly, gt;
        if (! dist) { tsl = v[0]; tbr = v[1]; tbm = v[2]; xov = v[3]; hf = v[4]; xom = v[5]; lsl = v[6]; lfr = v[7]; lmg = v[8]; atc = v[9]; rtc = v[10]; dly = v[11]; gt = v[12]; }
        else {
            xov = v[2]; hf = v[3]; xom = v[4]; tsl = v[6]; tbr = v[7]; tbm = v[8]; dly = v[12]; lsl = v[13]; lfr = v[14]; lmg = v[15]; atc = v[16]; rtc = v[17]; gt = v[18];
            const double a = v[0] / 255.0 * 0.5 * kPi; dstD = std::cos (a); dstW = std::sin (a);
            exm = v[1] / 63.0; drive = 1.0 + v[5] / 255.0 * 24.0; expS = v[9] / 127.0 * 3.0;
            susA = 1.0 - std::exp (-1.0 / ((5.0 + v[10] * 8.0) * 0.001 * fs));
            for (auto& d : dhp) d.highpass (fs, v[11]);
        }
        for (int c = 0; c < 2; ++c) {
            lo[c].setCut (fs, xov); hi[c].highpass (fs, xov, 0.707); env[c].setCut (fs, 30.0);
            hfc[c].setCut (fs, hf >= 21000.0 ? 0.45 * fs : hf);
            cl[c].set (fs, lsl, lfr, lmg, atc, rtc); ch[c].set (fs, tsl, tbr, tbm, atc, rtc);
        }
        hfcOn = hf < 21000.0;
        const double a = clampf (xom, 0, 100) * 0.01 * 0.5 * kPi;
        gLow = std::min (1.0, std::cos (a) * 1.41421); gHigh = std::min (1.0, std::sin (a) * 1.41421);
        dS = msToS (dly, fs); gate = -gt; gateA = 1.0 - std::exp (-1.0 / (0.01 * fs));
    }
    void process (double& L, double& R, const Ctx&) override {
        dl[0].push (L); dl[1].push (R);
        const int nch = dist ? 1 : 2;                        // Distression: mono in (left), dual mono out
        double y[2] = {};
        for (int c = 0; c < nch; ++c) {
            const double x = dl[c].read (dS);
            const double lowB = lo[c].lp (x), highB = hi[c].process (x);
            const double l = lowB * cl[c].gain (lowB), h = highB * ch[c].gain (highB);
            double hOut = h;
            if (dist) {
                double d = std::tanh (h * drive) / std::sqrt (drive);
                d = dhp[c].process (d);
                const double e = std::fabs (d); expEnv[c] += susA * (e - expEnv[c]);
                const double eDb = 20.0 * std::log10 (expEnv[c] + 1.0e-9);
                const double eg = eDb < -24.0 ? dbToGain (std::max (-60.0, (eDb + 24.0) * expS)) : 1.0;
                d *= exm * eg;
                hOut = dstD * h + dstW * d * 2.0;
            }
            if (hfcOn) hOut = hfc[c].lp (hOut);
            double o = gLow * l + gHigh * hOut;
            const double lv = 20.0 * std::log10 (std::fabs (x) + 1.0e-9);
            gateG[c] += gateA * ((lv > gate ? 1.0 : 0.0) - gateG[c]);
            y[c] = o * gateG[c];
        }
        if (dist) y[1] = y[0];
        L = y[0]; R = y[1];
    }
};

// ============================================================================ Test & Reference (bank 17)
struct OscAlgo : AlgoBase {
    double ph = 0; double inc = 0, g = 0.25; int wav = 0; std::atomic<double> inLvl { -90 }; double ms = 0;
    void set (const double* v, int) override { g = dbToGain (v[0]); inc = v[2] / fs; wav = (int) v[3]; }
    std::string status() const override { char b[40]; std::snprintf (b, sizeof b, "INPUT %.2 dB", (double) inLvl.load()); return b; }
    void process (double& L, double& R, const Ctx&) override {
        ms = 0.9995 * ms + 0.0005 * 0.5 * (L * L + R * R);
        inLvl.store (10.0 * std::log10 (ms + 1.0e-12) + 3.0);
        ph += inc; ph -= std::floor (ph);
        const double p = (double) ph;
        double s = wav == 0 ? std::sin (2.0 * kPi * p) : wav == 1 ? (p < 0.5 ? 4.0 * p - 1.0 : 3.0 - 4.0 * p) : (p < 0.5 ? 1.0 : -1.0);
        L = R = s * g;
    }
};
struct PinkAlgo : AlgoBase {
    double b[7] = {}, g = 0.25; uint32_t seed = 12345;
    void set (const double* v, int) override { g = dbToGain (v[0]) * 0.11; }
    void process (double& L, double& R, const Ctx&) override {
        seed = seed * 1664525u + 1013904223u; const double w = (double) (seed >> 8) / 8388608.0 - 1.0;   // Paul Kellet's pink filter
        b[0] = 0.99886 * b[0] + w * 0.0555179; b[1] = 0.99332 * b[1] + w * 0.0750759; b[2] = 0.96900 * b[2] + w * 0.1538520;
        b[3] = 0.86650 * b[3] + w * 0.3104856; b[4] = 0.55000 * b[4] + w * 0.5329522; b[5] = -0.7616 * b[5] - w * 0.0168980;
        const double p = b[0] + b[1] + b[2] + b[3] + b[4] + b[5] + b[6] + w * 0.5362; b[6] = w * 0.115926;
        L = R = p * g;
    }
};
struct BinauralAlgo : AlgoBase {
    struct Widen { Line l, r; OnePole lpL, lpR; double d = 1; bool on = false;
        void prepare (double fs) { l.init ((int) (0.003 * fs)); r.init ((int) (0.003 * fs)); }
        void set (double fs, double width, double hz) { on = width > 0.5 && hz > 100.5; d = std::max (1.0, width * 10.0e-6 * fs); lpL.setCut (fs, hz); lpR.setCut (fs, hz); }
        inline void process (double& L, double& R) { l.push (L); r.push (R); if (! on) return; const double cL = lpL.lp (r.read (d)), cR = lpR.lp (l.read (d)); L += 0.7 * cL; R += 0.7 * cR; }
    } wd, ww, we;
    Preechoes pe; OnePole rol[2]; double gDry = 1, gWet = 0, gEr = 0, cal = 1;
    void prepare (double sr) override { fs = sr; wd.prepare (fs); ww.prepare (fs); we.prepare (fs); pe.prepare (fs); }
    void reset() override { pe.reset(); }
    void set (const double* v, int) override {
        gDry = lvGain (v[0]); gWet = lvGain (v[1]); gEr = lvGain (v[2]);
        double lv[6], dl[6]; for (int i = 0; i < 6; ++i) { lv[i] = v[6 + i]; dl[i] = v[12 + i]; }
        pe.set (fs, lv, dl, 6, v[19] > 0.5 ? 0.0 : v[18] / 99.0);
        wd.set (fs, v[3], v[20]); ww.set (fs, v[4], v[21]); we.set (fs, v[5], v[22]);
        cal = dbToGain (v[23]);
        for (auto& r : rol) r.setCut (fs, v[24] >= 21000.0 ? 0.45 * fs : v[24]);
    }
    void process (double& L, double& R, const Ctx& c) override {
        double dL = L * gDry, dR = R * gDry; wd.process (dL, dR);
        double eL, eR; pe.process (L, R, fs, eL, eR); eL = rol[0].lp (eL) * gEr; eR = rol[1].lp (eR) * gEr; we.process (eL, eR);
        double rL = 0, rR = 0;
        if (c.hasRev) { rL = c.revL * gWet * cal; rR = c.revR * gWet * cal; ww.process (rL, rR); }
        L = dL + eL + rL; R = dR + eR + rR;
    }
};

// ============================================================================ Classic Cart: 6-Voice Chorus and Multiband Delays
struct ChorusAlgo : AlgoBase {
    Line ln[2], df[2][2]; OnePole hfc[2]; double base[6] = {}, g[6] = {}, dep = 0, ph = 0, inc = 0, fb = 0, gDif = 0, last[2] = {}, mixD = 0, mixW = 1;
    void prepare (double sr) override { fs = sr; for (int c = 0; c < 2; ++c) { ln[c].init ((int) (1.6 * fs)); df[c][0].init ((int) (0.01 * fs)); df[c][1].init ((int) (0.01 * fs)); } }
    void reset() override { for (int c = 0; c < 2; ++c) { ln[c].clear(); df[c][0].clear(); df[c][1].clear(); hfc[c].reset(); last[c] = 0; } }
    void set (const double* v, int) override {
        dep = v[0] * 0.001 * fs; inc = v[1] / fs; fb = v[2] * 0.01; gDif = 0.7 * v[3] / 99.0;
        for (auto& h : hfc) h.setCut (fs, v[4] >= 21000.0 ? 0.45 * fs : v[4]);
        sinCos (v[5], mixD, mixW);
        double sg[2] = {};
        for (int i = 0; i < 6; ++i) { base[i] = v[6 + i] * 0.001 * fs + 2.0; g[i] = lvGain (v[12 + i]) * 0.9; sg[i & 1] += std::max (0.0, g[i]); }
        // keep the regeneration loop below unity gain: without the hardware model nothing else bounds it
        const double lg = std::abs (fb) * std::max (sg[0], sg[1]);
        if (lg > 0.95) fb *= 0.95 / lg;
    }
    void process (double& L, double& R, const Ctx&) override {
        double iL = L + fb * last[0], iR = R + fb * last[1];
        iL = pv::allpass (df[0][0], 0.0013 * fs, gDif, pv::allpass (df[0][1], 0.0029 * fs, gDif, iL));
        iR = pv::allpass (df[1][0], 0.0017 * fs, gDif, pv::allpass (df[1][1], 0.0031 * fs, gDif, iR));
        ln[0].push (iL); ln[1].push (iR);
        ph += inc; if (ph >= 1) ph -= 1;
        double w[2] = {};
        for (int i = 0; i < 6; ++i) {
            if (g[i] <= 0.0) continue;
            const double m = 0.5 + 0.5 * std::sin (2.0 * kPi * (ph + (double) i / 6.0));
            w[i & 1] += g[i] * ln[i & 1].read (std::max (1.0, base[i] + dep * m));
        }
        last[0] = std::tanh (w[0]); last[1] = std::tanh (w[1]);
        const double oL = hfc[0].lp (w[0]), oR = hfc[1].lp (w[1]);
        { const double xL_ = L, xR_ = R, wL_ = oL, wR_ = oR; mixOut (L, R, xL_, xR_, wL_, wR_, mixD, mixW); }
    }
};
struct MBandAlgo : AlgoBase {
    Line ln; Biquad bp[6]; OnePole hfc[2]; double base[6] = {}, g[6] = {}, fr[6] = {}, q = 1, swp = 0, swd = 0, ph = 0, fb = 0, last = 0, mixD = 0, mixW = 1; int cnt = 0;
    void prepare (double sr) override { fs = sr; ln.init ((int) (1.6 * fs)); }
    void reset() override { ln.clear(); for (auto& b : bp) b.reset(); hfc[0].reset(); hfc[1].reset(); last = 0; }
    void set (const double* v, int) override {
        fb = v[0] * 0.01; q = 0.7 + v[1] / 99.0 * 12.0; swp = v[2]; swd = v[3];
        for (auto& h : hfc) h.setCut (fs, v[4] >= 21000.0 ? 0.45 * fs : v[4]);
        sinCos (v[5], mixD, mixW);
        double sg = 0;
        for (int i = 0; i < 6; ++i) { base[i] = v[6 + i] * 0.001 * fs + 1.0; g[i] = lvGain (v[12 + i]) * std::sqrt (q) * 2.2; fr[i] = v[18 + i]; sg += std::max (0.0, g[i]); }
        const double lg = std::abs (fb) * 0.5 * sg;   // band-pass peak gain is 1, so this is the loop gain bound
        if (lg > 0.95) fb *= 0.95 / lg;
        cnt = 0;
    }
    void process (double& L, double& R, const Ctx&) override {
        ln.push (0.5 * (L + R) + fb * last);
        ph += swp / fs; if (ph >= 1) ph -= 1;
        if (cnt-- <= 0) {
            cnt = 32;
            for (int i = 0; i < 6; ++i) {
                const double f = clampf (fr[i] * std::pow (2.0, swd * std::sin (2.0 * kPi * (ph + (double) i / 6.0))), 30.0, 0.45 * fs);
                const double w0 = 2.0 * kPi * f / fs, al = std::sin (w0) / (2.0 * q), cs = std::cos (w0);
                const double z1 = bp[i].z1, z2 = bp[i].z2;
                bp[i].set (al, 0, -al, 1 + al, -2 * cs, 1 - al);
                bp[i].z1 = z1; bp[i].z2 = z2;
            }
        }
        double w[2] = {}, sum = 0;
        for (int i = 0; i < 6; ++i) {
            if (g[i] <= 0.0) continue;
            const double y = g[i] * bp[i].process (ln.read (base[i]));
            sum += y; w[i & 1] += y * 0.85; w[1 - (i & 1)] += y * 0.35;
        }
        last = std::tanh (sum * 0.5);
        const double oL = hfc[0].lp (w[0]), oR = hfc[1].lp (w[1]);
        { const double xL_ = L, xR_ = R, wL_ = oL, wR_ = oR; mixOut (L, R, xL_, xR_, wL_, wR_, mixD, mixW); }
    }
};

// ============================================================================ one 480L machine

// ---------------------------------------------------------------- Surround/HD Cart (banks 22-26)
// Two separately levelled parts, as the cart's manual describes: a randomly varying early-reflection pattern in the
// 20-50 ms range (ELV) and a low-coloration late reverb (RLV). Twelve reflections, half of them "rear": the rear ones get
// RDL extra delay, the ELR roll-off and the ELRA attenuation. Surround folds the rear outputs into the stereo pair at
// -3 dB; HD sums them at full level (the two-channel algorithm sums all four outputs, twice the echo density).
// LFB adds a delayed (LBD) low-frequency boost below LBF to the reverb; HFC is a 6 dB/oct low-pass on the reverb outputs.
struct HDAlgo : AlgoBase {
    static constexpr int kT = 12;
    static constexpr double kMs[kT]  = { 20.7, 23.3, 26.1, 28.9, 31.7, 34.3, 37.9, 40.3, 43.1, 45.7, 48.3, 50.0 };
    static constexpr double kG[kT]   = { 1.0, 0.86, 0.92, 0.78, 0.82, 0.70, 0.66, 0.71, 0.60, 0.56, 0.52, 0.47 };
    static constexpr int   kSide[kT] = { 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1 };
    static constexpr bool  kRear[kT] = { false, false, true, false, true, true, false, true, false, true, true, false };
    pv::TankAlgo tank; Line pdL, pdR, erIn[2], lbL, lbR; OnePole hfc[2], elr[2], lbf[2]; RandLfo lfo[kT];
    double tapB[kT] = {}, edG[kT] = {}, tapS[kT] = {}, atk[kT] = {}, wanS = 0, rpdS = 1, lbdS = 1, elv = 1, rlv = 1, lfbG = 0, rearG = 0.5, fold = 0.707, trim = 1;
    void prepare (double sr) override {
        fs = sr; tank.init (4, fs); tank.fs = fs;
        pdL.init ((int) (0.21 * fs)); pdR.init ((int) (0.21 * fs)); lbL.init ((int) (0.21 * fs)); lbR.init ((int) (0.21 * fs));
        for (auto& l : erIn) l.init ((int) (0.40 * fs));
        for (int i = 0; i < kT; ++i) lfo[i].init ((uint32_t) (101 + i * 13));
    }
    void reset() override {
        tank.clear(); pdL.clear(); pdR.clear(); lbL.clear(); lbR.clear(); for (auto& l : erIn) l.clear();
        for (int c = 0; c < 2; ++c) { hfc[c].reset(); elr[c].reset(); lbf[c].reset(); }
    }
    void set (const double* v, int) override {
        elv = lvGain (v[0]); rlv = lvGain (v[1]);
        const double rt = v[2], spr = v[3], siz = v[10];
        for (auto& h : hfc) h.setCut (fs, v[4] >= 21000.0 ? 0.45 * fs : v[4]);
        lfbG = dbToGain (6.0 * v[5] / 512.0) - 1.0;
        const double size = clampf (0.5 + siz / 31.0 * 0.45, 0.5, 1.4);
        tank.curveT = curveFor (rt); tank.erAttackMs = erAttackMs;
        tank.update (rt, v[8] >= 21000.0 ? 20000.0 : v[8], size, v[6], 1.0, clampf (3.0 + spr / 255.0 * 200.0, 3, 230), v[14] / 99.0,
                     pv::surfaceEffect (0, 0, 0.0, 3.0 * size), kSmooth, std::max (0.05, v[9] / 50.0), v[7]);
        rpdS = msToS (v[11], fs);
        lbdS = msToS (v[12], fs);
        for (auto& f : lbf) f.setCut (fs, v[13]);
        const double sc = clampf (siz / 31.0, 0.3, 2.0), eld = v[18] * 0.001 * fs, rdl = v[22] * 0.001 * fs;
        for (int i = 0; i < kT; ++i)
        {
            tapB[i] = std::max (1.0, kMs[i] * sc * 0.001 * fs + eld + (kRear[i] ? rdl : 0.0));
            tapS[i] = std::max (1.0, std::min (0.39 * fs, tapB[i] + tapEdMs[i] * 0.001 * fs));
            edG[i] = tapEdDb[i] <= -79.0 ? 0.0 : std::pow (10.0, tapEdDb[i] / 20.0);
        }
        for (int i = 0; i < kT; ++i) atk[i] = erAttackMs > 0.1 ? 1.0 - std::exp (-(kMs[i] * sc) / erAttackMs) : 1.0;
        wanS = 0.0004 * fs * v[19] / 50.0;                              // early spin: up to about +-0.8 ms of movement
        for (int i = 0; i < kT; ++i) lfo[i].setRate (fs, std::max (0.05, v[19] / 40.0) * (0.8 + 0.04 * (double) i));
        for (auto& e : elr) e.setCut (fs, v[20] >= 21000.0 ? 0.45 * fs : v[20]);
        const bool hd = v[23] > 0.5;
        rearG = dbToGain (v[21]); fold = hd ? 1.0 : 0.707;
        trim = dbToGain (tankTrimDb (rt));
    }
    int baseTaps (double* ms, double* db, int max) const override
    {
        const int c = std::min (kT, max);
        for (int i = 0; i < c; ++i) { ms[i] = tapB[i] / fs * 1000.0; db[i] = 20.0 * std::log10 (std::max (1e-6, kG[i] * elv * 0.42 * (kRear[i] ? rearG * fold : 1.0))); }
        return c;
    }
    void process (double& L, double& R, const Ctx&) override {
        const double xL = L, xR = R;
        erIn[0].push (0.8 * xL + 0.2 * xR); erIn[1].push (0.8 * xR + 0.2 * xL);
        double f[2] = {}, r[2] = {};
        for (int i = 0; i < kT; ++i) {
            const int c = kSide[i];
            const double t = erIn[c].read (tapS[i] + wanS * (1.0 + lfo[i].next())) * kG[i] * atk[i] * edG[i];
            if (kRear[i]) r[c] += t; else f[c] += t;
        }
        const double eg = elv * 0.42;
        const double eL = (f[0] + elr[0].lp (r[0]) * rearG * fold) * eg, eR = (f[1] + elr[1].lp (r[1]) * rearG * fold) * eg;
        const double pl = pdL.read (rpdS), pr = pdR.read (rpdS); pdL.push (xL); pdR.push (xR);
        double oL, oR; tank.process (pl, pr, 0.5, 0.0, oL, oR);
        oL *= trim; oR *= trim;
        const double bL = lbL.read (lbdS), bR = lbR.read (lbdS); lbL.push (lbf[0].lp (oL)); lbR.push (lbf[1].lp (oR));
        oL += bL * lfbG; oR += bR * lfbG;
        L = eL + hfc[0].lp (oL) * rlv; R = eR + hfc[1].lp (oR) * rlv;
    }
};

// ---------------------------------------------------------------- the earlier Flexiverb's reverb ("Classic"), for the 480L II banks
// The same signal path as its engine ran for one slot: predelay, input tone and width, converter, the plate or the
// 480-style tank, converter, high cut and EQ - so the presets sound as they did there.
struct LegacyAlgo : AlgoBase {
    pv::PlateAlgo plate; pv::TankAlgo tank; pv::SlotChain ch; pv::SlotParams sp; bool isPlate = true; double preS = 1, inW = 1, dampA = 1;
    void prepare (double sr) override { fs = sr; plate.init (fs); plate.fs = fs; tank.init (4, fs); tank.fs = fs; ch.init (fs); }
    void reset() override { plate.clear(); tank.clear(); ch.clear (fs); }
    void set (const double* v, int) override {
        const bool wasPlate = isPlate; isPlate = v[22] < 0.5;
        if (wasPlate != isPlate) reset();
        sp.decaySec = v[0]; sp.lowMult = v[1]; sp.highMult = v[2]; sp.size = v[3]; sp.predelayMs = v[4]; sp.buildMs = v[5];
        sp.early = v[6]; sp.diffusion = v[7]; sp.damping = v[8]; sp.tone = v[9]; sp.mod = v[10]; sp.inWidth = v[11];
        sp.spinHz = v[12]; sp.xoverHz = v[13]; sp.hiCutHz = v[14] >= 19990.0 ? 20000.0 : v[14]; sp.converter = (int) v[15];
        static constexpr double eqF[4] = { 250.0, 400.0, 2500.0, 8000.0 };
        for (int b = 0; b < 4; ++b) { sp.eqF[b] = eqF[b]; sp.eqG[b] = v[18 + b]; }
        ch.update (fs, sp);
        preS = std::max (1.0, sp.predelayMs * 0.001 * fs); inW = clampf (sp.inWidth, 0.0, 1.0);
        const double dampHz = 20000.0 * std::pow (0.05, sp.damping);
        if (isPlate) {
            dampA = curveMul ? 1.0 : 1.0 - std::exp (-2.0 * kPi * std::min (dampHz, 0.45 * fs) / fs);
            plate.curveT = curveFor (sp.decaySec);
            plate.update (sp.decaySec, sp.size, sp.lowMult, sp.highMult, sp.buildMs, sp.diffusion, pv::plateEffect (0, 0, 0.0), kSmooth);
        } else {
            tank.curveT = curveFor (sp.decaySec); tank.erAttackMs = erAttackMs;
            tank.edMs = tapEdMs; tank.edDb = tapEdDb; tank.edMax = kMaxTaps;
            tank.update (sp.decaySec, dampHz, sp.size, sp.lowMult, sp.highMult, sp.buildMs, sp.diffusion,
                         pv::surfaceEffect (0, 0, 0.0, 3.0 * sp.size), kSmooth, std::max (0.05, sp.spinHz), sp.xoverHz);
        }
    }
    int baseTaps (double* ms, double* db, int max) const override { return isPlate ? 0 : tank.baseTaps (ms, db, max, sp.predelayMs, sp.early); }
    void process (double& L, double& R, const Ctx&) override {
        double pl, pr; ch.input (L, R, preS, inW, true, pl, pr);
        if (sp.converter != pv::Conv24) { pl = pv::convert (pl, sp.converter); pr = pv::convert (pr, sp.converter); }
        double oL, oR;
        if (isPlate) plate.process (pl, pr, dampA, sp.mod, sp.early, oL, oR);
        else tank.process (pl, pr, sp.mod, sp.early, oL, oR);
        if (sp.converter != pv::Conv24) { oL = pv::convert (oL, sp.converter); oR = pv::convert (oR, sp.converter); }
        ch.post (oL, oR, oL, oR, sp.comp);
        L = oL; R = oR;
    }
};

inline std::unique_ptr<AlgoBase> makeAlgo (int a) {
    switch (a) {
        case aRev6: case aRev2: case aRHall: return std::make_unique<ReverbAlgo> (a);
        case aAmb: return std::make_unique<AmbienceAlgo>();
        case aHD: return std::make_unique<HDAlgo>();
        case aLegacy: return std::make_unique<LegacyAlgo>();
        case aFx: return std::make_unique<EffectsAlgo>();
        case aTwin: return std::make_unique<TwinAlgo>();
        case aSamp: return std::make_unique<SamplerAlgo>();
        case aPitch: return std::make_unique<PitchAlgo>();
        case aDopp: return std::make_unique<DopplerAlgo>();
        case aInOut: return std::make_unique<InOutAlgo>();
        case aSAdj: return std::make_unique<StereoAdjAlgo> (false);
        case aPons: return std::make_unique<StereoAdjAlgo> (true);
        case aSPeq: return std::make_unique<PeqAlgo> (false);
        case aMPeq: return std::make_unique<PeqAlgo> (true);
        case aPano: return std::make_unique<PanoramaAlgo>();
        case aComp: return std::make_unique<CompAlgo>();
        case aPrime: return std::make_unique<PrimeAlgo>();
        case aFDyn: return std::make_unique<FreqAlgo> (false);
        case aDist: return std::make_unique<FreqAlgo> (true);
        case aOsc: return std::make_unique<OscAlgo>();
        case aPink: return std::make_unique<PinkAlgo>();
        case aBin: return std::make_unique<BinauralAlgo>();
        case aCHall: return std::make_unique<ClassicAlgo> (false);
        case aRPlate: return std::make_unique<ClassicAlgo> (true);
        case aChorus: return std::make_unique<ChorusAlgo>();
        case aMBand: return std::make_unique<MBandAlgo>();
        default: return std::make_unique<InOutAlgo>();
    }
}

// A machine owns one instance of every algorithm (allocated in prepare), so loading a program never allocates.
// your own early reflections: up to 32 taps of the machine's input (time, level re the input, side), added to its output
struct UserRefl {
    static constexpr int K = 32;
    Line dl; int n = 0, d[K] = {}; double gL[K] = {}, gR[K] = {}, fs = 48000, key[K * 3 + 1] = {};
    void prepare (double sr) { fs = sr; dl.init ((int) (0.32 * fs) + 8); dl.clear(); key[0] = -1; }
    void set (int cnt, const double* ms, const double* db, const double* pan) {
        cnt = std::max (0, std::min (K, cnt));
        bool same = key[0] == (double) cnt;
        for (int k = 0; k < cnt && same; ++k) same = key[1 + 3 * k] == ms[k] && key[2 + 3 * k] == db[k] && key[3 + 3 * k] == pan[k];
        if (same) return;
        key[0] = (double) cnt;
        for (int k = 0; k < cnt; ++k) {
            key[1 + 3 * k] = ms[k]; key[2 + 3 * k] = db[k]; key[3 + 3 * k] = pan[k];
            d[k] = std::max (1, std::min ((int) (0.3 * fs), (int) std::lround (ms[k] * 0.001 * fs)));
            const double g = db[k] <= -79.0 ? 0.0 : std::pow (10.0, db[k] / 20.0), p = clampf (pan[k], -1, 1);
            gL[k] = g * std::sqrt (0.5 * (1 - p)); gR[k] = g * std::sqrt (0.5 * (1 + p));
        }
        n = cnt;
    }
    inline void process (double inL, double inR, double& L, double& R) {
        dl.push (0.5 * (inL + inR));
        if (n == 0) return;
        for (int k = 0; k < n; ++k) { const double x = dl.readI (d[k]); L += gL[k] * x; R += gR[k] * x; }
    }
};

// The 480L's long recirculation ("stone / cave depth"): its tail returns at fixed intervals that grow with the room
// (about 137 ms in Large Hall, 45 ms in Short Plate), measured from real 480L impulse responses. Flexiverb's own tank
// repeats much faster and more smoothly, so this adds the measured returns: a few feedback taps on the machine's output,
// crossing sides on alternate taps, level-compensated so the loudness and the decay time stay as they were.
struct LoopSpec { int n = 0; double ms[3] = {}, g[3] = {}; };
struct LoopStage {
    Line l, r; int n = 0, d[3] = {}; double g[3] = {}, norm = 1, fs = 48000, amt = -1; const LoopSpec* cur = nullptr;
    // each return drifts very slightly (a few hundredths of a millisecond, below a second per cycle), read with
    // 4-point interpolation: the returns stay where they are measured but never ring as a fixed comb
    double ph[3] = { 0.0, 0.33, 0.71 }, inc[3] = {}, dep = 0;
    void prepare (double sr) { fs = sr; const double hz[3] = { 0.31, 0.47, 0.59 }; for (int k = 0; k < 3; ++k) inc[k] = hz[k] / sr; dep = 0.00006 * sr; l.init ((int) (0.4 * sr) + 8); r.init ((int) (0.4 * sr) + 8); l.clear(); r.clear(); cur = nullptr; amt = -1; n = 0; }
    void reset() { l.clear(); r.clear(); }
    void set (const LoopSpec* s, double amount) {
        if (s == cur && amount == amt) return;
        if (s != cur) reset();
        cur = s; amt = amount; n = (s && amount > 0.001) ? s->n : 0;
        double sum = 0, sq = 0;
        for (int k = 0; k < n; ++k) { d[k] = std::max (4, std::min ((int) (0.39 * fs), (int) std::lround (s->ms[k] * 0.001 * fs))); g[k] = s->g[k] * amount; sum += std::fabs (g[k]); }
        if (sum > 0.85) for (int k = 0; k < n; ++k) g[k] *= 0.85 / sum;          // always stable
        for (int k = 0; k < n; ++k) sq += g[k] * g[k];
        norm = std::sqrt (std::max (0.05, 1.0 - sq));
    }
    inline void process (double& L, double& R) {
        if (n == 0) return;
        double yL = L, yR = R;
        for (int k = 0; k < n; ++k) {
            ph[k] += inc[k]; if (ph[k] >= 1.0) ph[k] -= 1.0;
            const double dd = (double) d[k] + dep * std::sin (6.2831853 * ph[k]);
            const double a = l.readC (dd), b = r.readC (dd);
            if (k & 1) { yL += g[k] * b; yR += g[k] * a; } else { yL += g[k] * a; yR += g[k] * b; }
        }
        l.push (yL); r.push (yR);
        L = yL * norm; R = yR * norm;
    }
};

struct Machine {
    std::unique_ptr<AlgoBase> algos[kNumAlgos];
    UserRefl refl;
    int cur = -1, bank = 1; double fs = 48000;
    double fadeG = 1; int fadeState = 0, nextAlgo = -1, nextBank = 1;   // 10 ms fade out/in on program change
    LoopStage loop;
    void prepare (double sr) {
        fs = sr; refl.prepare (sr); loop.prepare (sr);
        for (auto& d : shapeDb) d = 999.0;                      // redesign the shaping EQ at the new rate
        for (auto& c : shapeEq) for (auto& b : c) b.reset();
        for (int a = 0; a < kNumAlgos; ++a) { if (! algos[a]) algos[a] = makeAlgo (a); algos[a]->prepare (fs); algos[a]->reset(); }
    }
    AlgoBase* current() const { return cur >= 0 ? algos[cur].get() : nullptr; }
    // values: the LARC values of the current program (kMaxParams)
    double trimG = 1, trimS = 1;                                     // program output trim (target, smoothed)
    Biquad shapeEq[2][4]; double shapeDb[4] = { 0, 0, 0, 0 }; bool shapeOn = false;
    void setShape (const double* db) {
        bool same = true; for (int b = 0; b < 4; ++b) same = same && db[b] == shapeDb[b];
        if (same) return;
        shapeOn = false;
        for (int b = 0; b < 4; ++b) { shapeDb[b] = db[b]; shapeOn = shapeOn || std::fabs (db[b]) > 0.01; }
        for (int c = 0; c < 2; ++c) {
            shapeEq[c][0].lowShelf (fs, 100.0, db[0]); shapeEq[c][1].peak (fs, 400.0, 0.9, db[1]);
            shapeEq[c][2].peak (fs, 2500.0, 0.9, db[2]); shapeEq[c][3].highShelf (fs, 8000.0, db[3]);
        }
    }
    double erAtk = 0.0;
    double edMs[AlgoBase::kMaxTaps] = {}, edDb[AlgoBase::kMaxTaps] = {}; int edVer = 0;
    void setTapEdits (int n, const double* ms, const double* db)
    {
        bool same = true;
        for (int k = 0; k < AlgoBase::kMaxTaps; ++k) { const double m = k < n ? ms[k] : 0.0, d = k < n ? db[k] : 0.0; same = same && m == edMs[k] && d == edDb[k]; edMs[k] = m; edDb[k] = d; }
        if (! same) ++edVer;
    }
    int baseTaps (double* ms, double* db, int max) const { return cur >= 0 ? algos[cur]->baseTaps (ms, db, max) : 0; }
    void update (int algo, int bnk, const double* v, double trimDb = 0.0, const double* curve = nullptr, const double* shape = nullptr) {
        if (cur >= 0) algos[cur]->erAttackMs = erAtk;
        trimG = dbToGain (trimDb);
        { static const double zero[4] = { 0, 0, 0, 0 }; setShape (shape ? shape : zero); }
        if (cur >= 0) algos[cur]->curveMul = algoHasDecayCurve (cur) ? curve : nullptr;
        if (algo != cur || bnk != bank) {
            if (cur < 0) { cur = algo; bank = bnk; algos[cur]->reset(); fadeState = 0; fadeG = 1; trimS = trimG; }
            else if (fadeState == 0 || nextAlgo != algo || nextBank != bnk) { nextAlgo = algo; nextBank = bnk; fadeState = 1; }
        }
        if (fadeState == 1 && fadeG <= 0.0) {                  // faded out: switch and fade back in
            cur = nextAlgo; bank = nextBank;
            algos[cur]->reset();
            algos[cur]->curveMul = algoHasDecayCurve (cur) ? curve : nullptr; algos[cur]->erAttackMs = erAtk;
            fadeState = 2;
        }
        if (cur >= 0 && fadeState != 1)
        {
            auto* a = algos[cur].get();
            if (a->tapEdVer != edVer) { std::copy (edMs, edMs + AlgoBase::kMaxTaps, a->tapEdMs); std::copy (edDb, edDb + AlgoBase::kMaxTaps, a->tapEdDb); a->tapEdVer = edVer; }
            a->set (v, bank);
        }   // while fading out, the values belong to the next program
    }
    inline void process (double& L, double& R, const Ctx& c) {
        if (cur < 0) return;
        const double iL = L, iR = R;
        auto* al = algos[cur].get(); al->split = split; al->dryOutL = al->dryOutR = 0.0;
        algos[cur]->process (L, R, c);
        double dL = al->dryOutL, dR = al->dryOutR;
        if (fadeState != 1) loop.process (L, R);
        trimS += (trimG - trimS) * 0.0005; L *= trimS; R *= trimS;
        if (shapeOn) for (int b = 0; b < 4; ++b) { L = shapeEq[0][b].process (L); R = shapeEq[1][b].process (R); }
        refl.process (iL, iR, L, R);
        if (fadeState != 0) {
            const double step = 1.0 / (0.01 * fs);
            if (fadeState == 1) fadeG = std::max (0.0, fadeG - step);
            else { fadeG = std::min (1.0, fadeG + step); if (fadeG >= 1.0) fadeState = 0; }
            L *= fadeG; R *= fadeG;
        }
        // an engine without its own Wet/Dry Mix (the Classic engine): the always-visible REVERB MIX blends here
        if (extMix >= 0.0)
        {
            extS += (extMix - extS) * 0.002;
            if (split) { dL += iL * (1.0 - extS); dR += iR * (1.0 - extS); L *= extS; R *= extS; }
            else { L = iL + (L - iL) * extS; R = iR + (R - iR) * extS; }
        }
        dryL = dL; dryR = dR;
    }
    double extMix = -1.0, extS = 1.0;
    bool split = false; double dryL = 0, dryR = 0;            // split: L/R = reverb only, dryL/R = the untouched dry part
};

} // namespace l480
