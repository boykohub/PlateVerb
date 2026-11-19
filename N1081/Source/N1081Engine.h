// N1081 — model of the Neve 1081 channel amplifier (line amp, filters, 4-band EQ, class A/B output)
// Plain C++ so it can be tested without JUCE. Runs at the oversampled rate set by the plugin.
#pragma once
#include <cmath>
#include <algorithm>

namespace n1081 {

constexpr double kPi = 3.14159265358979323846;

// ------------------------------------------------------------------ front-panel tables
// Published 1081 switch positions (AMS Neve / UA manual)
static const double kHpfFreqs[5] = { 27, 47, 82, 150, 270 };
static const double kLpfFreqs[5] = { 18000, 12000, 8200, 5600, 3900 };
static const double kLfFreqs [5] = { 33, 56, 100, 180, 330 };
static const double kLmfFreqs[10] = { 220, 270, 330, 390, 470, 560, 680, 820, 1000, 1200 };
static const double kHmfFreqs[10] = { 1500, 1800, 2200, 2700, 3300, 3900, 4700, 5600, 6800, 8200 };
static const double kHfFreqs [5] = { 3300, 4700, 6800, 10000, 15000 };
static const int    kLineGainSteps = 8;          // -15 ... +20 dB in 5 dB steps
inline double lineGainDb (int step) { return -15.0 + 5.0 * step; }

// ------------------------------------------------------------------ curve shapes
// The 1081 bands are inductor/capacitor networks inside amplifier feedback loops.
// That gives the "proportional Q" Neve behaviour: broad at small settings, narrowing as you boost/cut.
struct Shape {
    static constexpr double midQ    = 0.95;   // normal mid Q at full boost
    static constexpr double midHiQ  = 2.6;    // Hi-Q at full boost
    static constexpr double bellQ   = 0.75;   // HF/LF in peak mode
    static constexpr double shelfQ  = 0.58;   // gentle, slight overshoot-free shelf
    static constexpr double filtQ   = 0.80;   // 3-pole RC filters, softer knee than Butterworth (1.0)
    static double propQ (double q0, double gainDb) { return q0 * (0.62 + 0.38 * std::min (1.0, std::fabs (gainDb) / 18.0)); }
};

// ------------------------------------------------------------------ biquad (bilinear, prewarped)
struct Coefs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

// analog H(s) = (B0 + B1 s + B2 s^2) / (A0 + A1 s + A2 s^2), s normalised to w0
inline Coefs bilinear (double B0, double B1, double B2, double A0, double A1, double A2, double f0, double fs)
{
    const double f = std::min (f0, fs * 0.49);
    const double K = 1.0 / std::tan (kPi * f / fs);          // prewarp: s = K (1 - z^-1)/(1 + z^-1)
    const double K2 = K * K;
    const double a0 = A0 + A1 * K + A2 * K2;
    Coefs c;
    c.b0 = (B0 + B1 * K + B2 * K2) / a0;
    c.b1 = (2 * B0 - 2 * B2 * K2) / a0;
    c.b2 = (B0 - B1 * K + B2 * K2) / a0;
    c.a1 = (2 * A0 - 2 * A2 * K2) / a0;
    c.a2 = (A0 - A1 * K + A2 * K2) / a0;
    return c;
}

// first-order analog (B0 + B1 s)/(A0 + A1 s)
inline Coefs bilinear1 (double B0, double B1, double A0, double A1, double f0, double fs)
{
    const double f = std::min (f0, fs * 0.49);
    const double K = 1.0 / std::tan (kPi * f / fs);
    const double a0 = A0 + A1 * K;
    Coefs c;
    c.b0 = (B0 + B1 * K) / a0;
    c.b1 = (B0 - B1 * K) / a0;
    c.a1 = (A0 - A1 * K) / a0;
    return c;
}

struct Biquad {
    Coefs c; double z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline double process (double x) {
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }
};

// ------------------------------------------------------------------ analog prototypes
enum class Kind { Peak, LowShelf, HighShelf, Bypass };

struct Analog { double B0, B1, B2, A0, A1, A2; };

inline Analog proto (Kind k, double gainDb, double Q)
{
    const double A = std::pow (10.0, gainDb / 40.0);
    switch (k) {
        case Kind::Peak:      return { 1, A / Q, 1, 1, 1 / (A * Q), 1 };
        case Kind::LowShelf:  { const double r = std::sqrt (A) / Q; return { A * A, A * r, A, 1, r, A }; }   // A(s² + r s + A)/(A s² + r s + 1)
        case Kind::HighShelf: { const double r = std::sqrt (A) / Q; return { A, A * r, A * A, A, r, 1 }; }   // A(A s² + r s + 1)/(s² + r s + A)
        default:              return { 1, 0, 0, 1, 0, 0 };
    }
}

// magnitude of an analog section at frequency f (for the display — the analog target curve)
inline double protoMagDb (const Analog& p, double f, double f0)
{
    const double w = f / f0, w2 = w * w;
    const double nr = p.B0 - p.B2 * w2, ni = p.B1 * w;
    const double dr = p.A0 - p.A2 * w2, di = p.A1 * w;
    return 10.0 * std::log10 ((nr * nr + ni * ni) / (dr * dr + di * di) + 1e-30);
}

// ------------------------------------------------------------------ parameters
struct Params {
    int    lineStep = 3;          // 0 dB
    double outputDb = 0;
    bool   eqIn = true, phase = false;
    int    hpf = 0, lpf = 0;      // 0 = out, 1..5
    // bands: 0 LF, 1 LMF, 2 HMF, 3 HF
    int    freqIdx[4] = { 2, 4, 5, 3 };
    double gainDb [4] = { 0, 0, 0, 0 };
    bool   alt    [4] = { false, false, false, false };   // LF/HF: peak mode, LMF/HMF: Hi-Q
};

struct BandSpec { Kind kind; double f0; double gain; double Q; };

inline BandSpec bandSpec (const Params& p, int b)
{
    const double g = p.gainDb[b];
    switch (b) {
        case 0:  return p.alt[0] ? BandSpec { Kind::Peak, kLfFreqs[p.freqIdx[0]], g, Shape::propQ (Shape::bellQ, g) }
                                 : BandSpec { Kind::LowShelf, kLfFreqs[p.freqIdx[0]], g, Shape::shelfQ };
        case 1:  return { Kind::Peak, kLmfFreqs[p.freqIdx[1]], g, Shape::propQ (p.alt[1] ? Shape::midHiQ : Shape::midQ, g) };
        case 2:  return { Kind::Peak, kHmfFreqs[p.freqIdx[2]], g, Shape::propQ (p.alt[2] ? Shape::midHiQ : Shape::midQ, g) };
        default: return p.alt[3] ? BandSpec { Kind::Peak, kHfFreqs[p.freqIdx[3]], g, Shape::propQ (Shape::bellQ, g) }
                                 : BandSpec { Kind::HighShelf, kHfFreqs[p.freqIdx[3]], g, Shape::shelfQ };
    }
}

// whole analog EQ+filter response (display)
inline double responseDb (const Params& p, double f)
{
    double db = 0;
    if (p.eqIn) for (int b = 0; b < 4; ++b) {
        const BandSpec s = bandSpec (p, b);
        if (std::fabs (s.gain) > 1e-3) db += protoMagDb (proto (s.kind, s.gain, s.Q), f, s.f0);
    }
    if (p.hpf > 0) {
        const double w = f / kHpfFreqs[p.hpf - 1];
        db += 10 * std::log10 (w * w / (1 + w * w));
        db += protoMagDb ({ 0, 0, 1, 1, 1 / Shape::filtQ, 1 }, f, kHpfFreqs[p.hpf - 1]);
    }
    if (p.lpf > 0) {
        const double w = f / kLpfFreqs[p.lpf - 1];
        db += 10 * std::log10 (1 / (1 + w * w));
        db += protoMagDb ({ 1, 0, 0, 1, 1 / Shape::filtQ, 1 }, f, kLpfFreqs[p.lpf - 1]);
    }
    return db;
}

// ------------------------------------------------------------------ transformer
// Core flux follows the integral of the voltage, so saturation grows at low frequencies and high level.
struct Transformer {
    double fs = 96000, lpA = 0, flux = 0, amount = 0.0, hpA = 0, hpZ = 0, hpX = 0;
    Biquad hf;
    void prepare (double rate, double fluxCornerHz, double hfCornerHz, double lfCornerHz, double amt)
    {
        fs = rate; amount = amt;
        lpA = std::exp (-2 * kPi * fluxCornerHz / fs);
        hpA = std::exp (-2 * kPi * lfCornerHz / fs);
        const Analog lp { 1, 0, 0, 1, 1 / 0.7071, 1 };
        hf.c = bilinear (lp.B0, lp.B1, lp.B2, lp.A0, lp.A1, lp.A2, hfCornerHz, fs);
        reset();
    }
    void reset() { flux = 0; hpZ = hpX = 0; hf.reset(); }
    inline double process (double x)
    {
        // LF roll-off of the winding inductance (one pole)
        const double h = hpA * (hpZ + x - hpX); hpX = x; hpZ = h;
        // flux ~ low-passed signal. Calibrated so a 0 dBFS (+18 dBu) 40 Hz sine gives roughly 0.2 % 3rd harmonic
        // at amount = 1, falling ~12 dB per octave upwards: negligible by 1 kHz
        flux = (1 - lpA) * h + lpA * flux;
        const double f = flux;
        const double sat = std::tanh (f) - f;                // odd-order, grows with flux
        const double y = h + amount * (0.30 * sat + 0.002 * f * std::fabs (f)); // small hysteresis-like even term
        return hf.process (y);
    }
};

// ------------------------------------------------------------------ channel
// hardware clips near +26 dBu = +8 dBFS (with 0 dBFS = +18 dBu)
constexpr double kVolt = 1.0 / 2.512;
// linear up to the knee, then rounds smoothly into a ceiling of 1.0
inline double softClip (double v)
{
    constexpr double k = 0.70;
    const double a = std::fabs (v);
    if (a <= k) return v;
    const double r = k + (1 - k) * std::tanh ((a - k) / (1 - k));
    return v < 0 ? -r : r;
}

struct Channel {
    double fs = 96000;
    Transformer inXf, outXf;
    Biquad hpA, lpA; double hp1z = 0, hp1x = 0, lp1z = 0;
    Coefs hp1, lp1;
    Biquad band[4];
    bool bandActive[4] = { false, false, false, false };
    double dcX = 0, dcY = 0;

    void prepare (double rate)
    {
        fs = rate;
        // Input: Marinair-type line/mic input transformer. Together with the output side: -3 dB near 7 Hz and ~38 kHz,
        // within ±0.5 dB 20 Hz-20 kHz (published 1081 response)
        inXf.prepare (fs, 18.0, 52000.0, 4.0, 1.0);
        // Output: LO1166-type output transformer after the BA340 class A/B stage
        outXf.prepare (fs, 14.0, 46000.0, 3.5, 0.7);
        reset();
    }
    void reset()
    {
        inXf.reset(); outXf.reset(); hpA.reset(); lpA.reset();
        hp1z = hp1x = lp1z = 0; dcX = dcY = 0;
        for (auto& b : band) b.reset();
    }

    void setFilters (int hpf, int lpf)
    {
        if (hpf > 0) {
            const double f = kHpfFreqs[hpf - 1];
            hpA.c = bilinear (0, 0, 1, 1, 1 / Shape::filtQ, 1, f, fs);
            hp1 = bilinear1 (0, 1, 1, 1, f, fs);
        }
        if (lpf > 0) {
            const double f = kLpfFreqs[lpf - 1];
            lpA.c = bilinear (1, 0, 0, 1, 1 / Shape::filtQ, 1, f, fs);
            lp1 = bilinear1 (1, 0, 1, 1, f, fs);
        }
    }

    void setBand (int b, const BandSpec& s, bool eqIn)
    {
        bandActive[b] = eqIn && std::fabs (s.gain) > 0.01;
        const Analog a = proto (s.kind, s.gain, s.Q);
        band[b].c = bilinear (a.B0, a.B1, a.B2, a.A0, a.A1, a.A2, s.f0, fs);
    }

    // x in "dBFS volts": 1.0 = 0 dBFS = +18 dBu. Hardware clips near +26 dBu (= +8 dBFS)
    inline double process (double x, double lineGain, int hpf, int lpf)
    {
        double y = inXf.process (x);

        // BA338 line/mic amp: single-ended class A stage, mostly 2nd harmonic, grows with level
        y *= lineGain;
        {
            const double v = y * kVolt;                      // v = 1 at clipping (~ +26 dBu)
            y = softClip (v + 0.004 * v * v) / kVolt;        // ~0.05 % 2nd harmonic at +20 dBu, rises when driven
        }

        if (hpf > 0) {
            const double h = hp1.b0 * y + hp1.b1 * hp1x - hp1.a1 * hp1z; hp1x = y; hp1z = h;
            y = hpA.process (h);
        }
        if (lpf > 0) {
            const double l = lp1.b0 * y + lp1z; lp1z = lp1.b1 * y - lp1.a1 * l;
            y = lpA.process (l);
        }

        for (int b = 0; b < 4; ++b) {
            const double v = band[b].process (y);   // always run so state stays continuous
            if (bandActive[b]) y = v;
        }

        // BA340 complementary push-pull emitter follower (class A/B): tiny crossover step at very low level,
        // symmetric (odd-order) clipping at the top
        {
            const double x0 = 0.0015;
            y -= 0.004 * y * std::exp (-(y * y) / (x0 * x0));
            y = softClip (y * kVolt) / kVolt;
        }

        y = outXf.process (y);

        // remove DC from the even-order terms
        const double d = y - dcX + 0.99995 * dcY; dcX = y; dcY = d;
        return d;
    }
};


// ================================================================== 2254-style diode-bridge limiter/compressor
// Published 2254 behaviour: input transformer -> ~40 dB pad -> diode-bridge gain cell -> class A make-up amp ->
// output transformer; separate compressor and limiter sidechains acting on the same bridge; feedback detection;
// soft knee over ~10 dB; compressor attack 5 ms, recovery 400/800/1500 ms/auto; limiter recovery 100/200/800 ms/auto.
static const double kCompRatios[5]  = { 1.5, 2, 3, 4, 6 };
static const double kCompRecMs[3]   = { 400, 800, 1500 };      // index 3 = auto
static const double kLimRecMs[3]    = { 100, 200, 800 };       // index 3 = auto
static const double kScHpf[6]       = { 0, 50, 80, 120, 160, 250 };
constexpr double kDbuOffset = 18.0;                              // 0 dBFS = +18 dBu
inline double compThrDbu (int step) { return -20.0 + 2.0 * step; }   // 16 steps, -20 ... +10 dBu
inline double limThrDbu  (int step) { return  4.0 + 2.0 * step; }    //  9 steps,  +4 ... +20 dBu
inline double compGainDb (int step) { return  2.0 * step; }          // 11 steps,   0 ... +20 dB

struct CompParams2254 {
    bool compOn = true, limOn = false, fastAtk = false;
    int thr = 8, ratio = 2, rec = 1, gain = 0, lim = 4, limRec = 1, scHpf = 0;
    double mix = 1.0;
};

struct Comp2254 {
    double fs = 96000;
    Transformer inXf[2], outXf[2];
    double envC = 0, envCs = 0, envL = 0, envLs = 0, lastOut[2] = { 0, 0 }, gr = 0;
    double hpZ1[2] = { 0, 0 }, hpZ2[2] = { 0, 0 };
    Biquad sc[2]; double scHz = 0;
    double dcX[2] = { 0, 0 }, dcY[2] = { 0, 0 };
    double rect = 0, rectIn = 0, rectDecay = 0.9999;

    void prepare (double rate)
    {
        fs = rate;
        rectDecay = std::exp (-1.0 / (0.025 * fs));          // ~25 ms reservoir: ripple-free from 40 Hz up
        for (int c = 0; c < 2; ++c) {
            inXf[c].prepare (fs, 16.0, 60000.0, 3.0, 1.2);    // 2254 input transformer
            outXf[c].prepare (fs, 14.0, 55000.0, 3.0, 0.9);   // output transformer
        }
        scHz = -1; reset();
    }
    void reset()
    {
        envC = envCs = envL = envLs = gr = rect = rectIn = 0;
        for (int c = 0; c < 2; ++c) { inXf[c].reset(); outXf[c].reset(); sc[c].reset(); lastOut[c] = 0; dcX[c] = dcY[c] = 0; }
    }
    double coef (double ms) const { return 1.0 - std::exp (-1.0 / (std::max (0.01, ms) * 0.001 * fs)); }

    static double knee (double over, double slope, double k = 10.0)   // soft knee over k dB
    {
        if (over <= -k * 0.5) return 0;
        if (over >= k * 0.5) return over * slope;
        const double t = over + k * 0.5;
        return slope * t * t / (2 * k);
    }

    // L/R in/out, "volts" (1.0 = 0 dBFS). Linked stereo like a strapped pair.
    inline void process (double& L, double& R, const CompParams2254& p)
    {
        if (p.scHpf > 0 && scHz != kScHpf[p.scHpf]) {
            scHz = kScHpf[p.scHpf];
            for (auto& b : sc) b.c = bilinear (0, 0, 1, 1, 1 / 0.707, 1, scHz, fs);
        }
        const double dryL = L, dryR = R;
        double x[2] = { inXf[0].process (L), inXf[1].process (R) };

        // Feedback detection: the sidechain hears the bridge output. Computed as the held input peak times
        // the present gain, so the loop reacts to what comes out now (no stale peaks -> no overshoot).
        double kL = x[0], kR = x[1];
        if (p.scHpf > 0) { kL = sc[0].process (kL); kR = sc[1].process (kR); }
        const double det = std::max (std::fabs (kL), std::fabs (kR));
        rect = det > rect ? det : rect * rectDecay;
        const double detDbu = 20.0 * std::log10 (rect + 1e-9) + kDbuOffset - 3.0 - envC;   // sine peak -> rms reading
        const double inPk = std::max (std::fabs (x[0]), std::fabs (x[1]));
        rectIn = inPk > rectIn ? inPk : rectIn * rectDecay;
        const double inDbu = 20.0 * std::log10 (rectIn + 1e-9) + kDbuOffset - 3.0;

        // compressor: feedback form, GR = (out - thr) * (ratio - 1)
        double grC = 0;
        if (p.compOn) {
            const double tgt = knee (detDbu - compThrDbu (p.thr), kCompRatios[p.ratio] - 1.0);   // feedback: GR = (out - thr)(R - 1)
            const double aA = coef (p.fastAtk ? 1.5 : 5.0);
            if (p.rec < 3) {
                const double aR = coef (kCompRecMs[p.rec] / 4.6);       // time to recover ~99 %
                envC += (tgt - envC) * (tgt > envC ? aA : aR);
                grC = envC;
            } else {                                                      // auto: fast 50 ms + slow 5 s composite
                envC  += (tgt - envC)  * (tgt > envC  ? aA : coef (50.0 / 4.6));
                envCs += (tgt - envCs) * (tgt > envCs ? coef (300.0) : coef (5000.0 / 4.6));
                grC = std::max (envC, envCs);
            }
        }
        // limiter: fast feed-back peak catcher, > 100:1
        double grL = 0;
        if (p.limOn) {
            const double tgt = knee (inDbu - limThrDbu (p.lim), 0.99, 2.0);           // > 100:1
            const double aA = coef (p.fastAtk ? 0.3 : 2.0);
            if (p.limRec < 3) { envL += (tgt - envL) * (tgt > envL ? aA : coef (kLimRecMs[p.limRec] / 4.6)); grL = envL; }
            else {
                envL  += (tgt - envL)  * (tgt > envL  ? aA : coef (50.0 / 4.6));
                envLs += (tgt - envLs) * (tgt > envLs ? coef (200.0) : coef (5000.0 / 4.6));
                grL = std::max (envL, envLs);
            }
        }
        gr = std::max (0.0, grC + std::max (0.0, grL - grC * 0.5));      // both sidechains drive one bridge
        gr = std::min (gr, 40.0);
        const double g = std::pow (10.0, -gr / 20.0);
        const double grN = std::min (1.0, gr / 20.0);
        const double mk = std::pow (10.0, compGainDb (p.gain) / 20.0);

        double y[2];
        for (int c = 0; c < 2; ++c) {
            // diode bridge: the diodes see the padded input; their bending grows with the control current
            // (gain reduction), slight imbalance adds even order. ~0.2 % at +15 dBu with heavy reduction.
            const double v = x[c] * 0.8;
            const double a3 = 0.002 + 0.010 * grN, a2 = 0.001 + 0.004 * grN;
            double u = g * (x[c] + (a2 * v * std::fabs (v) - a3 * v * v * v) / 0.8);
            // class A make-up amp + output transformer
            double w = u * mk;
            w = softClip (w * kVolt + 0.003 * (w * kVolt) * (w * kVolt)) / kVolt;
            w = outXf[c].process (w);
            const double d = w - dcX[c] + 0.99995 * dcY[c]; dcX[c] = w; dcY[c] = d;
            y[c] = d;
        }
        L = dryL * (1 - p.mix) + y[0] * p.mix;
        R = dryR * (1 - p.mix) + y[1] * p.mix;
    }
};

} // namespace n1081
