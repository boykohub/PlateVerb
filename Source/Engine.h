// Flexiverb (formerly PlateVerb) DSP engine - no JUCE dependency.
// Six reverb slots (Plate, Room, Chamber, Hall, IR, 480 random hall), each with its own reverb settings, 4-band EQ
// and tail compressor, tempo-syncable stereo delay with a gentle 3-band EQ,
// output transformer models and integer oversampling (1x, 2x, 3x, 4x).
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <array>
#include <cstdint>
#include "Materials.h"

namespace pv {

constexpr float kPi = 3.14159265358979f;
constexpr int kMaxOS = 4;
constexpr float kMaxSize = 1.5f;
constexpr float kMaxDelaySec = 3.0f;
constexpr int kNumSlots = 6;
constexpr float kDirtDefault = 30.0f;
enum Slot { Plate = 0, Room, Chamber, Hall, IR, Lex480 };   // new slots go at the end: parameter IDs follow the index

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float clampf (float x, float a, float b) { return std::min (b, std::max (a, x)); }

// ---------------------------------------------------------------- building blocks
// Fast round-to-nearest for quantising (exact for |x| < 2^22; the 18-bit memory and converters stay far inside that).
// Adding and removing 1.5 * 2^23 makes the FPU round in its default mode - no library call per sample.
inline float roundFast (float x) { const float big = 12582912.0f; const float t = x + big; return t - big; }

// Word length of delay memory. 0 = float (the default, used by every plugin except where an engine sets it).
// The 480L's audio memory (HSP boards, 4464 DRAMs) stores 18-bit words, so Flexiverb sets 18 bits around its
// machines while the hardware model is on: every delay-line write is rounded to 1/131072 of full scale.
struct LineQuant { float step = 0.0f, inv = 0.0f; };
inline LineQuant& lineQuant() { static thread_local LineQuant q; return q; }
struct ScopedLineQuant {
    LineQuant saved;
    explicit ScopedLineQuant (int bits) : saved (lineQuant()) { auto& q = lineQuant(); if (bits > 0) { q.inv = (float) (1 << (bits - 1)); q.step = 1.0f / q.inv; } else q = {}; }
    ~ScopedLineQuant() { lineQuant() = saved; }
};

struct Line {
    std::vector<float> b; int mask = 0, w = 0;
    void init (int maxLen) { int n = 1; while (n < maxLen + 8) n <<= 1; b.assign ((size_t) n, 0.0f); mask = n - 1; w = 0; }
    void clear() { std::fill (b.begin(), b.end(), 0.0f); w = 0; }
    inline void push (float x) {
        const auto& q = lineQuant();
        if (q.step > 0.0f && std::fabs (x) < 16.0f) x = roundFast (x * q.inv) * q.step;
        b[(size_t) w] = x; w = (w + 1) & mask;
    }
    // zero only the n most recent samples (what can still be read at the current rate), not the whole buffer
    void clearRange (int start, int n) {          // zero n samples starting at ring index 'start'
        const int size = (int) b.size();
        n = std::min (std::max (n, 0), size); start &= mask;
        const int first = std::min (n, size - start);
        std::fill (b.begin() + start, b.begin() + start + first, 0.0f);
        std::fill (b.begin(), b.begin() + (n - first), 0.0f);
    }
    void clearRecent (int n) {
        const int size = (int) b.size();
        n = std::min (std::max (n, 0), size);
        const int start = (w - n) & mask;
        const int first = std::min (n, size - start);
        std::fill (b.begin() + start, b.begin() + start + first, 0.0f);
        std::fill (b.begin(), b.begin() + (n - first), 0.0f);
    }
    inline float readI (int d) const { return b[(size_t) ((w - d) & mask)]; }
    // sample written d samples ago (d >= 1), linear interpolation
    inline float read (float d) const {
        float r = (float) w - d; float fl = std::floor (r); int i = (int) fl; float f = r - fl;
        float a = b[(size_t) (i & mask)], c = b[(size_t) ((i + 1) & mask)];
        return a + (c - a) * f;
    }
    // 4-point Hermite (cubic) interpolation, d >= 2: smoother modulation, no interpolation noise
    inline float readC (float d) const {
        float r = (float) w - d; float fl = std::floor (r); int i = (int) fl; float f = r - fl;
        const float xm1 = b[(size_t) ((i - 1) & mask)], x0 = b[(size_t) (i & mask)], x1 = b[(size_t) ((i + 1) & mask)], x2 = b[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
};

inline float allpass (Line& l, float d, float g, float x) {
    float z = l.read (d);
    float v = x - g * z;
    l.push (v);
    return z + g * v;
}
// modulated allpass: cw = how much cubic interpolation is blended in (0 = linear, the original character)
inline float allpassM (Line& l, float d, float g, float x, float cw) {
    float z = l.read (d);
    if (cw > 0.0f) z += (l.readC (d) - z) * cw;
    float v = x - g * z;
    l.push (v);
    return z + g * v;
}

struct OnePole {
    float a = 1.0f, y = 0.0f;
    void setCut (float fs, float f) { a = 1.0f - std::exp (-2.0f * kPi * clampf (f, 1.0f, 0.49f * fs) / fs); }
    inline float lp (float x) { y += a * (x - y); return y; }
    void reset() { y = 0.0f; }
};

struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline float process (float x) { float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void set (float B0, float B1, float B2, float A0, float A1, float A2) { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }
    static float lim (float fs, float f) { return clampf (f, 5.0f, 0.45f * fs); }
    // one second-order section of a lowpass from an analog pole pair (sigma < 0, omega), already scaled
    // to the prewarped cutoff, through the bilinear transform; unity gain at DC
    void fromPoles (float sigma, float omega) {
        const float m = sigma * sigma + omega * omega;
        set (m, 2 * m, m, 1 - 2 * sigma + m, 2 * (m - 1), 1 + 2 * sigma + m);
    }
    void peak (float fs, float f, float Q, float dB) {
        float A = std::pow (10.0f, dB / 40.0f), w = 2 * kPi * lim (fs, f) / fs, al = std::sin (w) / (2 * Q), c = std::cos (w);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (float fs, float f, float dB, float S = 0.7f) {
        float A = std::pow (10.0f, dB / 40.0f), w = 2 * kPi * lim (fs, f) / fs, c = std::cos (w), s = std::sin (w);
        float al = s / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2), sA = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sA), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sA),
             (A + 1) + (A - 1) * c + sA, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sA);
    }
    void highShelf (float fs, float f, float dB, float S = 0.7f) {
        float A = std::pow (10.0f, dB / 40.0f), w = 2 * kPi * lim (fs, f) / fs, c = std::cos (w), s = std::sin (w);
        float al = s / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2), sA = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sA), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sA),
             (A + 1) - (A - 1) * c + sA, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sA);
    }
    void lowpass (float fs, float f, float Q) {
        float w = 2 * kPi * lim (fs, f) / fs, al = std::sin (w) / (2 * Q), c = std::cos (w);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (float fs, float f, float Q) {
        float w = 2 * kPi * lim (fs, f) / fs, al = std::sin (w) / (2 * Q), c = std::cos (w);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
};

// ---------------------------------------------------------------- oversampling
// Polyphase windowed-sinc up/down sampler for any integer factor.
struct Resampler {
    int N = 1, K = 48, T = 48;
    std::vector<float> h, hd;   // hd = h delayed by N-1 so decimation lands on whole samples
    std::vector<float> upHist;   // K newest input samples (index 0 = newest)
    std::vector<float> dnBuf;    // 2T ring (doubled) of oversampled samples
    int dnPos = 0, dnCount = 0;

    static float bessel0 (float x) { float s = 1, t = 1; for (int k = 1; k < 25; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; }

    void reserve (int maxFactor) {
        const size_t t = (size_t) (maxFactor * (K + 1));
        h.reserve (t); hd.reserve (t); upHist.reserve ((size_t) (K + 1)); dnBuf.reserve (2 * t);
    }
    void design (int factor) {
        // Tdesign = N*K + 1 symmetric taps, so up+down delay is exactly K base-rate samples.
        N = std::max (1, factor); T = N * (K + 1);
        const int Td = N * K + 1;
        h.assign ((size_t) T, 0.0f);
        const float fc = 0.5f / (float) N * 0.955f, beta = 7.0f, mid = (Td - 1) * 0.5f;
        float sum = 0;
        for (int n = 0; n < Td; ++n) {
            float x = (float) n - mid, sinc = x == 0 ? 2 * fc : std::sin (2 * kPi * fc * x) / (kPi * x);
            float r = x / (mid + 1.0f), win = bessel0 (beta * std::sqrt (std::max (0.0f, 1 - r * r))) / bessel0 (beta);
            h[(size_t) n] = sinc * win; sum += h[(size_t) n];
        }
        for (auto& v : h) v /= sum;
        hd.assign ((size_t) T, 0.0f);
        for (int n = 0; n < Td; ++n) hd[(size_t) (n + N - 1)] = h[(size_t) n];
        upHist.assign ((size_t) (K + 1), 0.0f);
        dnBuf.assign ((size_t) (2 * T), 0.0f); dnPos = 0; dnCount = 0;
    }
    int latencyBase() const { return N == 1 ? 0 : K; }

    void up (float x, float* out) {
        if (N == 1) { out[0] = x; return; }
        for (int m = K; m > 0; --m) upHist[(size_t) m] = upHist[(size_t) (m - 1)];
        upHist[0] = x;
        for (int p = 0; p < N; ++p) {
            float acc = 0;
            for (int m = 0; m <= K; ++m) acc += h[(size_t) (p + m * N)] * upHist[(size_t) m];
            out[p] = acc * (float) N;
        }
    }
    // feed N oversampled samples, get one base-rate sample
    float down (const float* in) {
        if (N == 1) return in[0];
        for (int i = 0; i < N; ++i) {
            dnBuf[(size_t) dnPos] = in[i]; dnBuf[(size_t) (dnPos + T)] = in[i];
            dnPos = (dnPos + 1) % T;
        }
        // newest sample at dnPos-1; oldest at dnPos. linear view: dnBuf[dnPos .. dnPos+T-1] oldest->newest
        const float* v = &dnBuf[(size_t) dnPos];
        float acc = 0;
        for (int t = 0; t < T; ++t) acc += hd[(size_t) t] * v[T - 1 - t];
        return acc;
    }
};

// ---------------------------------------------------------------- frequency-dependent decay
// Splits the loop signal at 300 Hz and 4 kHz and gives each band its own loop gain, so the
// lows can ring longer (Low Decay) and the highs die faster (High Decay) than the mids.
// Decay-time curve inside a feedback loop: a 10-band graphic EQ (octaves, 31.25 Hz - 16 kHz).
// For a loop of loopSec and wanted decay times T[] at the band centres, one pass must lose 60*loopSec/T dB.
// The band gains are solved through the EQ's interaction matrix (and refined twice, since peaking filters are not
// linear in dB); the result is then checked on a dense grid and the broadband gain lowered if any frequency would
// lose less than the longest wanted decay allows - so the loop can never gain energy.
// a peaking section in double precision: inside a reverb loop the rounding of a float filter at low frequencies
// is recirculated hundreds of times and comes back as rumble and hiss in long tails
struct BiquadD {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    inline float process (float xf) { const double x = xf, y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return (float) y; }
    void peak (double fs, double f, double Q, double dB) {
        const double A = std::pow (10.0, dB / 40.0), w = 2.0 * 3.141592653589793 * std::min (f, 0.45 * fs) / fs, al = std::sin (w) / (2.0 * Q), c = std::cos (w);
        const double a0 = 1.0 + al / A;
        b0 = (1.0 + al * A) / a0; b1 = -2.0 * c / a0; b2 = (1.0 - al * A) / a0; a1 = -2.0 * c / a0; a2 = (1.0 - al / A) / a0;
    }
};
struct DecayEq {
    static constexpr int N = 10;
    static constexpr float Q = 1.5f;
    BiquadD pk[N]; float g0 = 1; bool on = false; int nb = N;
    static float centre (int i) { return 31.25f * std::pow (2.0f, (float) i); }
    template <typename B> static float respDb (const B& b, float fs, float f) {
        const double w = 2.0 * 3.141592653589793 * f / fs, c1 = std::cos (w), s1 = std::sin (w), c2 = std::cos (2 * w), s2 = std::sin (2 * w);
        const double nr = b.b0 + b.b1 * c1 + b.b2 * c2, ni = -(b.b1 * s1 + b.b2 * s2);
        const double dr = 1.0 + b.a1 * c1 + b.a2 * c2, di = -(b.a1 * s1 + b.a2 * s2);
        return (float) (10.0 * std::log10 ((nr * nr + ni * ni) / (dr * dr + di * di) + 1e-30));
    }
    static bool solve (std::vector<double> A, std::vector<double> b, int n, std::vector<double>& x) {   // Gauss-Jordan
        for (int c = 0; c < n; ++c) {
            int p = c; for (int r = c + 1; r < n; ++r) if (std::fabs (A[(size_t) (r * n + c)]) > std::fabs (A[(size_t) (p * n + c)])) p = r;
            if (std::fabs (A[(size_t) (p * n + c)]) < 1e-12) return false;
            for (int k = 0; k < n; ++k) std::swap (A[(size_t) (c * n + k)], A[(size_t) (p * n + k)]); std::swap (b[(size_t) c], b[(size_t) p]);
            for (int r = 0; r < n; ++r) {
                if (r == c) continue;
                const double f = A[(size_t) (r * n + c)] / A[(size_t) (c * n + c)];
                for (int k = 0; k < n; ++k) A[(size_t) (r * n + k)] -= f * A[(size_t) (c * n + k)];
                b[(size_t) r] -= f * b[(size_t) c];
            }
        }
        x.resize ((size_t) n); for (int i = 0; i < n; ++i) x[(size_t) i] = b[(size_t) i] / A[(size_t) (i * n + i)];
        return true;
    }
    float lastFs = -1, lastLoop = -1, lastT[N] = {};
    void design (float fs, float loopSec, const float* T) {
        // the engine calls this every block: redesign only when something changed
        if (on && fs == lastFs && loopSec == lastLoop && std::equal (T, T + N, lastT)) return;
        lastFs = fs; lastLoop = loopSec; std::copy (T, T + N, lastT);
        for (auto& b : pk) { b.b0 = 1; b.b1 = b.b2 = b.a1 = b.a2 = 0; }
        nb = 0; while (nb < N && centre (nb) < 0.45f * fs) ++nb;
        std::vector<double> t ((size_t) nb), d ((size_t) nb), M ((size_t) (nb * nb)), gains ((size_t) nb, 0.0), sol;
        float tMax = -1e9f;
        for (int i = 0; i < nb; ++i) { t[(size_t) i] = -60.0 * loopSec / std::max (0.05f, T[i]); tMax = std::max (tMax, (float) t[(size_t) i]); }
        const double base = t[5 < nb ? 5 : (size_t) nb - 1];
        for (int i = 0; i < nb; ++i) d[(size_t) i] = t[(size_t) i] - base;
        for (int i = 0; i < nb; ++i) { BiquadD u; u.peak (fs, centre (i), Q, 1.0); for (int j = 0; j < nb; ++j) M[(size_t) (j * nb + i)] = respDb (u, fs, centre (j)); }
        for (int it = 0; it < 3; ++it) {
            std::vector<double> err ((size_t) nb);
            for (int j = 0; j < nb; ++j) { double r = 0; for (int i = 0; i < nb; ++i) r += respDb (pk[i], fs, centre (j)); err[(size_t) j] = it == 0 ? d[(size_t) j] : d[(size_t) j] - r; }
            if (! solve (M, err, nb, sol)) break;
            for (int i = 0; i < nb; ++i) { gains[(size_t) i] = clampf ((float) (gains[(size_t) i] + sol[(size_t) i]), -48.0f, 24.0f); pk[i].peak (fs, centre (i), Q, gains[(size_t) i]); }
        }
        for (int i = nb; i < N; ++i) pk[i] = BiquadD();
        // stability: nowhere may one pass lose less than the least-damped band wants (and never less than 0.01 dB)
        float worst = -1e9f;
        for (int k = 0; k <= 160; ++k) {
            const float f = 20.0f * std::pow (0.45f * fs / 20.0f, k / 160.0f);
            float r = (float) base; for (int i = 0; i < nb; ++i) r += respDb (pk[i], fs, f);
            worst = std::max (worst, r - std::min (tMax, -0.01f));
        }
        g0 = (float) std::pow (10.0, (base - std::max (0.0f, worst)) / 20.0);
        on = true;
    }
    void reset() { for (auto& b : pk) b.reset(); }
    inline float process (float x) { x *= g0; for (int i = 0; i < nb; ++i) x = pk[i].process (x); return x; }
};

struct BandDecay {
    OnePole lo, hi; float gL = 1, gM = 1, gH = 1; DecayEq eq;
    void setCurve (float fs, float loopSec, const float* T) { eq.design (fs, loopSec, T); }
    void curveOff() { eq.on = false; }
    void setX (float fs, float loHz = 300.0f) { lo.setCut (fs, loHz); hi.setCut (fs, 4000.0f); }
    void setGains (float loopSec, float rtMid, float lowMul, float highMul) {
        auto g = [loopSec] (float rt) { return std::min (0.9995f, std::pow (10.0f, -3.0f * loopSec / std::max (0.05f, rt))); };
        gM = g (rtMid); gL = g (rtMid * lowMul); gH = g (rtMid * highMul);
    }
    void setRT (float loopSec, float rtL, float rtM, float rtH) {
        auto g = [loopSec] (float rt) { return std::min (0.9995f, std::pow (10.0f, -3.0f * loopSec / std::max (0.05f, rt))); };
        gL = g (rtL); gM = g (rtM); gH = g (rtH);
    }
    void reset() { lo.reset(); hi.reset(); eq.reset(); }
    inline float process (float x) {
        if (eq.on) return eq.process (x);
        const float l = lo.lp (x), h = x - hi.lp (x); return l * gL + (x - l - h) * gM + h * gH;
    }
};

struct RandLfo {
    uint32_t seed = 1; float a = 0, b = 0, ph = 1, inc = 0.001f;
    void init (uint32_t s) { seed = s * 2654435761u + 12345u; a = rnd(); b = rnd(); ph = 0; }
    float rnd() { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) / 8388608.0f - 1.0f; }
    void setRate (float fs, float hz) { inc = hz / fs; }
    inline float next() {
        ph += inc; if (ph >= 1.0f) { ph -= 1.0f; a = b; b = rnd(); }
        const float w = 0.5f - 0.5f * std::cos (kPi * ph);
        return a + (b - a) * w;
    }
};

// ---------------------------------------------------------------- plate (Dattorro tank, true-stereo input)
// Diffusion sets the input/tank allpass gains: higher = smoother, denser onset (less grain), lower = more open and "zingy".
inline float& TankAlgo_wanderMsRef() { static float v = 1.5f; return v; }
inline float TankAlgo_wanderMs() { return TankAlgo_wanderMsRef(); }
struct PlateAlgo {
    static constexpr int kEarly = 6;
    Line oa[2], ob[2], d1, d2, d3, d4, e1, e2, e3, e4, apL1, dL1, apL2, apL3, dL2, apR1, dR1, apR2, apR3, dR2, ea[kEarly], eb[kEarly];
    BandDecay bdL, bdR; RandLfo lfo[2], wl[4];
    float fs = 48000, s = 1, lpL = 0, lpR = 0, tankL = 0, tankR = 0, eScale = 1;
    float wanderS = 0.0f;            // long decays: the four long delays wander slowly so the plate's resonances smear (0 when short)
    inline float wr (const Line& l, float n, int k) { return wanderS > 0.0f ? l.readC (n * s + wanderS * (1.0f + wl[k].next())) : l.read (std::round (n * s)); }
    static constexpr float kEaMs[kEarly] = { 0.53f, 0.93f, 1.71f, 3.07f, 5.3f, 8.9f }, kEbMs[kEarly] = { 0.61f, 1.13f, 1.97f, 2.71f, 6.1f, 10.3f };
    static constexpr float kMaxEScale = 55.0f / 15.0f;
    static constexpr float kOutMs[4] = { 1.37f, 3.11f, 1.63f, 2.79f };
    void init (float maxFs) {
        float sm = maxFs / 29761.0f * kMaxSize;
        auto sz = [sm] (float n) { return (int) std::ceil (n * sm) + 64; };
        d1.init (sz (142)); d2.init (sz (107)); d3.init (sz (379)); d4.init (sz (277));
        e1.init (sz (153)); e2.init (sz (115)); e3.init (sz (406)); e4.init (sz (297));
        const int wx = (int) (0.008f * maxFs);
        apL1.init (sz (704)); dL1.init (sz (4453) + wx); apL2.init (sz (1800)); apL3.init (sz (419)); dL2.init (sz (3720) + wx);
        apR1.init (sz (940)); dR1.init (sz (4217) + wx); apR2.init (sz (2656)); apR3.init (sz (557)); dR2.init (sz (3163) + wx);
        for (int k = 0; k < 4; ++k) wl[k].init ((uint32_t) (91 + k * 13));
        for (int i = 0; i < kEarly; ++i) { ea[i].init ((int) (kEaMs[i] * kMaxEScale * 0.001f * maxFs) + 16); eb[i].init ((int) (kEbMs[i] * kMaxEScale * 0.001f * maxFs) + 16); }
        lfo[0].init (11); lfo[1].init (23);
        for (int i = 0; i < 2; ++i) { oa[i].init ((int) (kOutMs[i] * 0.001f * maxFs * kMaxSize) + 16); ob[i].init ((int) (kOutMs[i + 2] * 0.001f * maxFs * kMaxSize) + 16); }
    }
    void clear() {
        const float sm = fs / 29761.0f * kMaxSize;
        auto sz = [sm] (float n) { return (int) std::ceil (n * sm) + 64; };
        d1.clearRecent (sz (142)); d2.clearRecent (sz (107)); d3.clearRecent (sz (379)); d4.clearRecent (sz (277));
        e1.clearRecent (sz (153)); e2.clearRecent (sz (115)); e3.clearRecent (sz (406)); e4.clearRecent (sz (297));
        const int wx = (int) (0.008f * fs);
        apL1.clearRecent (sz (704)); dL1.clearRecent (sz (4453) + wx); apL2.clearRecent (sz (1800)); apL3.clearRecent (sz (419)); dL2.clearRecent (sz (3720) + wx);
        apR1.clearRecent (sz (940)); dR1.clearRecent (sz (4217) + wx); apR2.clearRecent (sz (2656)); apR3.clearRecent (sz (557)); dR2.clearRecent (sz (3163) + wx);
        for (int i = 0; i < kEarly; ++i) { ea[i].clearRecent ((int) (kEaMs[i] * kMaxEScale * 0.001f * fs) + 16); eb[i].clearRecent ((int) (kEbMs[i] * kMaxEScale * 0.001f * fs) + 16); }
        for (int i = 0; i < 2; ++i) { oa[i].clearRecent ((int) (kOutMs[i] * 0.001f * fs * kMaxSize) + 16); ob[i].clearRecent ((int) (kOutMs[i + 2] * 0.001f * fs * kMaxSize) + 16); }
        bdL.reset(); bdR.reset();
        lpL = lpR = tankL = tankR = 0;
    }
    float gOut = 0.5f, gIn1 = 0.75f, gIn2 = 0.625f, gT1 = 0.7f, gT2 = 0.5f, gT3 = 0.4f;
    float cubicW = 0;
    const float* curveT = nullptr;   // decay-time curve (seconds at the 10 DecayEq bands); set before update(), nullptr = off
    void update (float rt, float size, float lowMul, float highMul, float buildMs, float diffusion, const MaterialEffect& me, float cubic) {
        cubicW = cubic;
        s = fs / 29761.0f * std::min (kMaxSize, size * me.sizeMul);
        eScale = clampf (buildMs / 15.0f * me.attackMul, 0.2f, kMaxEScale);
        const float d = clampf (diffusion + me.diffOffset, 0.0f, 1.0f);
        gIn1 = 0.25f + 0.55f * d; gIn2 = 0.20f + 0.48f * d;
        gT1 = 0.45f + 0.30f * d; gT2 = 0.30f + 0.28f * d; gT3 = 0.25f + 0.30f * d; gOut = 0.65f * d;
        const float halfLoop = (10645.0f + 488.0f) * s / fs;
        bdL.setX (fs); bdR.setX (fs);
        // knob decay per band, plus the plate material's own losses (1/RT adds)
        const float rtB[3] = { rt * lowMul, rt, rt * highMul };
        float r[3];
        for (int k = 0; k < 3; ++k) r[k] = 1.0f / std::max (0.02f, 1.0f / std::max (0.05f, rtB[k]) + me.extraLoss[k]);
        bdL.setRT (halfLoop, r[0], r[1], r[2]); bdR.setRT (halfLoop, r[0], r[1], r[2]);
        if (curveT) { bdL.setCurve (fs, halfLoop, curveT); bdR.setCurve (fs, halfLoop, curveT); }
        else { bdL.curveOff(); bdR.curveOff(); }
        lfo[0].setRate (fs, 0.83f); lfo[1].setRate (fs, 1.07f);
        for (int k = 0; k < 4; ++k) wl[k].setRate (fs, 0.21f + 0.067f * (float) k);
        float rtLong = std::max (rtB[0], std::max (rtB[1], rtB[2])); if (curveT) for (int b = 0; b < DecayEq::N; ++b) rtLong = std::max (rtLong, curveT[b]);
        wanderS = clampf ((rtLong - 1.5f) / 8.0f, 0.0f, 1.0f) * TankAlgo_wanderMs() * 0.001f * fs;
    }
    inline void process (float inL, float inR, float dampA, float mod, float early, float& oL, float& oR) {
        const float exc = mod * 14.0f * s;
        auto R = [this] (float n) { return std::round (n * s); };
        float xL = allpass (d1, R (142), gIn1, inL);  xL = allpass (d2, R (107), gIn1, xL);
        xL = allpass (d3, R (379), gIn2, xL); xL = allpass (d4, R (277), gIn2, xL);
        float xR = allpass (e1, R (153), gIn1, inR);  xR = allpass (e2, R (115), gIn1, xR);
        xR = allpass (e3, R (406), gIn2, xR); xR = allpass (e4, R (297), gIn2, xR);
        const float m1 = lfo[0].next(), m2 = lfo[1].next();
        float a = allpassM (apL1, 672 * s + exc * (1 + m1), -gT1, xR + tankR, cubicW);
        float t1 = wr (dL1, 4453, 0); dL1.push (a); lpL += dampA * (t1 - lpL);
        float b = allpass (apL2, R (1800), gT2, bdL.process (lpL)); b = allpass (apL3, R (419), gT3, b);
        float t2 = wr (dL2, 3720, 1); dL2.push (b);
        float c = allpassM (apR1, 908 * s + exc * (1 + m2), -gT1, xL + tankL, cubicW);
        float t3 = wr (dR1, 4217, 2); dR1.push (c); lpR += dampA * (t3 - lpR);
        float e = allpass (apR2, R (2656), gT2, bdR.process (lpR)); e = allpass (apR3, R (557), gT3, e);
        float t4 = wr (dR2, 3163, 3); dR2.push (e);
        tankL = t2; tankR = t4;
        auto T = [this] (const Line& l, float n) { return l.read (std::round (n * s)); };
        oL = 0.6f * (T (dR1, 266) + T (dR1, 2974) - T (apR2, 1913) + T (dR2, 1996) - T (dL1, 1990) - T (apL2, 187) - T (dL2, 1066));
        oR = 0.6f * (T (dL1, 353) + T (dL1, 3627) - T (apL2, 1228) + T (dL2, 2673) - T (dR1, 2111) - T (apR2, 335) - T (dR2, 121));
        // output diffusion smears the discrete output taps into a smooth, dense onset (less grain)
        const float sz = s * 29761.0f / fs;
        for (int i = 0; i < 2; ++i) { oL = allpass (oa[i], std::round (kOutMs[i] * 0.001f * fs * sz), gOut, oL); oR = allpass (ob[i], std::round (kOutMs[i + 2] * 0.001f * fs * sz), gOut, oR); }
        if (early > 0.0f) {                              // dense onset within a few ms, like a real plate's attack
            float qL = inL, qR = inR;
            for (int i = 0; i < kEarly; ++i) { qL = allpass (ea[i], kEaMs[i] * eScale * 0.001f * fs, 0.68f, qL); qR = allpass (eb[i], kEbMs[i] * eScale * 0.001f * fs, 0.68f, qR); }
            oL += early * 1.2f * qL; oR += early * 1.2f * qR;
        }
    }
};

// ---------------------------------------------------------------- Room / Chamber / Hall: 4-section ring tank
// A Lexicon-style ring of four sections (modulated allpass, delay, damping, two allpasses, delay), with
// early reflections modelled per space. Each section has its own frequency-dependent decay.
struct TankConfig {
    float scale;              // tank size (1 = hall)
    float rtMul;              // decay scaling so Decay reads as RT60
    float inDiffLo, inDiffHi; // input diffusion range (Diffusion 0..1)
    float tDiffLo, tDiffHi;   // tank diffusion range
    float erLevel, lateLevel;
    int erTaps; float erStartMs, erEndMs; bool denseER;
    float modMs, modHz;
    float lateOnsetMs;        // the tank is fed this much later than the first reflections
};

inline const TankConfig& tankConfig (int algo) {
    static const TankConfig c[5] = {
        { 1.00f, 1.0f, 0.55f, 0.80f, 0.40f, 0.70f, 0.0f, 1.0f,  0, 0, 0, false, 0.20f, 0.9f, 0.0f },   // (plate, unused)
        { 0.85f, 1.0f, 0.00f, 0.78f, 0.20f, 0.65f, 1.06f, 1.06f, 18, 2.5f, 38.0f, false, 0.25f, 1.1f, 4.0f },  // room
        { 0.92f, 1.0f, 0.00f, 0.80f, 0.20f, 0.68f, 0.63f, 0.70f,  0, 1.0f, 60.0f, true,  0.30f, 0.9f, 2.0f },   // chamber
        { 1.10f, 1.0f, 0.00f, 0.72f, 0.15f, 0.62f, 0.6f, 1.19f, 14, 9.0f, 95.0f, false, 0.45f, 0.7f, 12.0f },  // hall
        // 480 random hall (Griesinger style): larger randomly moving delays (wander) at a settable rate (spin)
        { 1.05f, 1.0f, 0.00f, 0.75f, 0.15f, 0.64f, 0.7f, 1.15f, 16, 4.0f, 80.0f, false, 1.10f, 0.8f, 8.0f }
    };
    return c[std::min (std::max (algo, 0), 4)];
}

struct TankAlgo {
    static constexpr int S = 4;
    // section lengths in ms at scale 1
    static constexpr float kAp1[S] = { 6.13f, 7.91f, 5.27f, 8.71f };
    static constexpr float kD1[S]  = { 41.3f, 47.9f, 37.1f, 44.9f };
    static constexpr float kAp2[S] = { 17.3f, 21.1f, 14.9f, 19.7f };
    static constexpr float kAp3[S] = { 4.31f, 3.67f, 5.03f, 2.93f };
    static constexpr float kD2[S]  = { 29.7f, 33.1f, 27.3f, 35.9f };
    static constexpr float kDif[4] = { 1.31f, 2.03f, 3.71f, 5.63f };
    Line ap1[S], d1[S], ap2[S], ap3[S], d2[S], difL[4], difR[4], erL, erR, lateL, lateR;
    OnePole damp[S]; BandDecay band[S]; RandLfo lfo[S], lfo2[S];
    float fb[S] = {};
    // long tails: the second delay of each section wanders slowly and randomly, more the longer the decay, so the
    // loop's resonances keep moving and smear into a smooth tail instead of ringing (0 for short decays)
    float wanderS = 0.0f;
    static constexpr int kDense = 128;
    float jit[kDense] = {}, sgnL[kDense] = {}, sgnR[kDense] = {}, erNormDense = 1;
    std::vector<float> tapMs, tapGL, tapGR;
    float fs = 48000, k = 48, kEr = 48, gIn = 0.6f, gT = 0.5f, maxTap = 0;
    int algo = 1;

    void init (int a, float maxFs) {
        algo = a;
        const TankConfig& c = tankConfig (a);
        const float km = maxFs * 0.001f * c.scale * kMaxSize, modS = (c.modMs * 2 + 1) * maxFs * 0.001f;
        for (int i = 0; i < S; ++i) {
            ap1[i].init ((int) (kAp1[i] * km + modS) + 16); d1[i].init ((int) (kD1[i] * km + 0.008f * maxFs) + 16);
            ap2[i].init ((int) (kAp2[i] * km) + 16); ap3[i].init ((int) (kAp3[i] * km) + 16); d2[i].init ((int) (kD2[i] * km + 0.008f * maxFs) + 16);
            lfo[i].init ((uint32_t) (a * 17 + i * 5 + 3)); lfo2[i].init ((uint32_t) (a * 29 + i * 11 + 7));
        }
        for (int i = 0; i < 4; ++i) { difL[i].init ((int) (kDif[i] * 1.1f * km) + 16); difR[i].init ((int) (kDif[i] * 1.1f * km) + 16); }
        tapMs.clear(); tapGL.clear(); tapGR.clear(); tapS.reserve (kDense);
        uint32_t seed = 4242u + (uint32_t) a * 7919u;
        auto rnd = [&seed] { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) / 16777216.0f; };
        for (int t = 0; t < c.erTaps; ++t) {
            const float f = (float) t / (float) std::max (1, c.erTaps - 1);
            const float ms = c.erStartMs * std::pow (c.erEndMs / c.erStartMs, f) * (0.9f + 0.2f * rnd());
            const float g = (1.0f - 0.65f * f) * (0.7f + 0.3f * rnd()) * ((t % 3 == 1) ? -1.0f : 1.0f);
            const float pan = (t % 2 == 0) ? 0.2f + 0.25f * rnd() : 0.8f - 0.25f * rnd();
            tapMs.push_back (ms); tapGL.push_back (g * std::sqrt (1 - pan)); tapGR.push_back (g * std::sqrt (pan));
        }
        if (c.denseER) {
            for (int t = 0; t < kDense; ++t) { jit[t] = 0.25f + 1.5f * rnd(); sgnL[t] = rnd() < 0.25f ? -0.6f : 1.0f; sgnR[t] = rnd() < 0.25f ? -0.6f : 1.0f; }
            tapMs.assign (kDense, 0.0f); tapGL.assign (kDense, 0.0f); tapGR.assign (kDense, 0.0f);
            maxTap = 250.0f / kMaxSize;
        } else { maxTap = 0; for (float m : tapMs) maxTap = std::max (maxTap, m); }
        erMaxS = (int) (maxTap * kMaxSize * maxFs * 0.001f + 0.25f * maxFs) + 8;      // room to move a reflection 250 ms later
        erL.init (erMaxS + 16); erR.init (erMaxS + 16);
        lateL.init ((int) (c.lateOnsetMs * kMaxSize * maxFs * 0.001f) + 16); lateR.init ((int) (c.lateOnsetMs * kMaxSize * maxFs * 0.001f) + 16);
    }
    void clear() {
        const TankConfig& c = tankConfig (algo);
        const float km = fs * 0.001f * c.scale * kMaxSize, modS = (c.modMs * 2 + 1) * fs * 0.001f;
        for (int i = 0; i < S; ++i) {
            ap1[i].clearRecent ((int) (kAp1[i] * km + modS) + 16); d1[i].clearRecent ((int) (kD1[i] * km + 0.008f * fs) + 16);
            ap2[i].clearRecent ((int) (kAp2[i] * km) + 16); ap3[i].clearRecent ((int) (kAp3[i] * km) + 16); d2[i].clearRecent ((int) (kD2[i] * km + 0.008f * fs) + 16);
            damp[i].reset(); band[i].reset(); fb[i] = 0;
        }
        for (int i = 0; i < 4; ++i) { difL[i].clearRecent ((int) (kDif[i] * 1.1f * km) + 16); difR[i].clearRecent ((int) (kDif[i] * 1.1f * km) + 16); }
        erL.clearRecent ((int) (maxTap * kMaxSize * fs * 0.001f) + 16); erR.clearRecent ((int) (maxTap * kMaxSize * fs * 0.001f) + 16);
        lateL.clearRecent ((int) (c.lateOnsetMs * kMaxSize * fs * 0.001f) + 16); lateR.clearRecent ((int) (c.lateOnsetMs * kMaxSize * fs * 0.001f) + 16);
    }
    float len1[S] = {}, len2[S] = {}, lenA1[S] = {}, lenA2[S] = {}, lenA3[S] = {}, lateS = 1;
    float cubicW = 0; bool erEq = false; Biquad erLo[2], erHi[2];
    float spinHz = -1.0f;      // >0: modulation rate override (480 'spin'); otherwise the config's rate
    const float* curveT = nullptr;   // decay-time curve (seconds at the 10 DecayEq bands); set before update(), nullptr = off
    void update (float rt, float dampHz, float size, float lowMul, float highMul, float buildMs, float diffusion, const MaterialEffect& me, float cubic,
                 float spin = -1.0f, float xoverHz = 300.0f) {
        const TankConfig& c = tankConfig (algo);
        spinHz = spin;
        cubicW = cubic;
        lowMul *= me.lowMul; highMul *= me.highMul; diffusion += me.diffOffset;
        erEq = std::abs (me.erLowDb) > 0.05f || std::abs (me.erHighDb) > 0.05f;
        for (int ch = 0; ch < 2; ++ch) { erLo[ch].lowShelf (fs, 300.0f, me.erLowDb); erHi[ch].highShelf (fs, 4000.0f, me.erHighDb); }
        k = fs * 0.001f * c.scale * std::sqrt (size);    // tank density stays high at small sizes; size mostly moves ER/onset
        kEr = fs * 0.001f * (c.denseER ? 1.0f : size);
        const float d = clampf (diffusion, 0.0f, 1.0f);
        gIn = c.inDiffLo + (c.inDiffHi - c.inDiffLo) * d;
        gT = c.tDiffLo + (c.tDiffHi - c.tDiffLo) * d;
        const float rtA = std::max (0.05f, rt * c.rtMul);
        for (int i = 0; i < S; ++i) {
            lenA1[i] = kAp1[i] * k; len1[i] = std::round (kD1[i] * k); lenA2[i] = std::round (kAp2[i] * k); lenA3[i] = std::round (kAp3[i] * k); len2[i] = std::round (kD2[i] * k);
            const float secLen = (lenA1[i] + len1[i] + lenA2[i] + lenA3[i] + len2[i]) / fs;
            damp[i].setCut (fs, dampHz);
            dampOn = dampHz < 17500.0f;
            band[i].setX (fs, xoverHz);
            band[i].setGains (secLen, rtA, lowMul, highMul);
            if (curveT) { float T[DecayEq::N]; for (int b = 0; b < DecayEq::N; ++b) T[b] = curveT[b] * c.rtMul; band[i].setCurve (fs, secLen, T); dampOn = false; }
            else band[i].curveOff();
            lfo[i].setRate (fs, (spinHz > 0.0f ? spinHz : c.modHz) * (0.8f + 0.13f * (float) i));
            lfo2[i].setRate (fs, 0.23f + 0.071f * (float) i);
        }
        { float rtLong = rtA; if (curveT) for (int b = 0; b < DecayEq::N; ++b) rtLong = std::max (rtLong, curveT[b] * c.rtMul);
          wanderS = clampf ((rtLong - 1.5f) / 8.0f, 0.0f, 1.0f) * TankAlgo_wanderMs() * 0.001f * fs * std::sqrt (std::max (0.5f, size)); }
        lateS = std::max (1.0f, c.lateOnsetMs * size * fs * 0.001f);
        if (c.denseER) {           // reflections every ~1.8 ms for 'build' ms: quick rise, gentle sag, short fade
            const float L = clampf (buildMs, 3.0f, 230.0f), att = std::min (12.0f, L * 0.3f), spacing = 1.8f * (0.8f + 0.2f * size);
            float sumSq = 0.0f;
            float ms = 0.3f;
            for (int t = 0; t < kDense; ++t) {
                ms += spacing * jit[t]; float env;           // random (Poisson-like) spacing: no comb colour
                if (ms < att) env = ms / att;
                else if (ms < L) env = 1.0f - 0.4f * (ms - att) / std::max (1.0f, L - att);
                else env = std::max (0.0f, 0.6f * (1.0f - (ms - L) / 4.0f));
                if (ms > 240.0f) env = 0.0f;
                tapMs[(size_t) t] = std::min (ms, 240.0f); tapGL[(size_t) t] = env * sgnL[t]; tapGR[(size_t) t] = env * sgnR[t];
                sumSq += env * env;
            }
            erNormDense = 1.0f / std::sqrt (std::max (1.0f, sumSq));
        } else {
            // 'build' spreads the reflection pattern in time (shorter = tighter cluster)
            buildScale = clampf (buildMs / 15.0f, 0.3f, 3.0f);
        }
        tapS.resize (tapMs.size());
        const int maxS = (int) (maxTap * kMaxSize * fs * 0.001f) + 8;
        for (size_t t = 0; t < tapMs.size(); ++t) tapS[t] = std::min (maxS, std::max (1, (int) std::lround (tapMs[t] * (c.denseER ? 1.0f : buildScale) * kEr)));
        // reflection attack: the early reflections fade in over erAttackMs instead of starting at full level
        tapS0 = tapS; tapEdG.assign (tapS.size(), 1.0f);
        if (! c.denseER && edMs != nullptr && edDb != nullptr)
            for (size_t t = 0; t < tapS.size() && (int) t < edMax; ++t)
            {
                tapS[t] = std::min (erMaxS, std::max (1, tapS[t] + (int) std::lround (edMs[t] * 0.001f * fs)));
                tapEdG[t] = edDb[t] <= -79.0f ? 0.0f : std::pow (10.0f, edDb[t] / 20.0f);
            }
        tapAtk.resize (tapS.size());
        for (size_t t = 0; t < tapS.size(); ++t) tapAtk[t] = (erAttackMs > 0.1f ? 1.0f - std::exp (-(tapS[t] / fs * 1000.0f) / erAttackMs) : 1.0f) * tapEdG[t];
    }
    std::vector<int> tapS, tapS0; std::vector<float> tapAtk, tapEdG;
    int erMaxS = 0;
    // your edits of these reflections (time offset ms, level dB; -80 = off), set before update(); nullptr = none
    const float* edMs = nullptr; const float* edDb = nullptr; int edMax = 0;
    // the reflections before your edits, for the screen: time from the tank input (ms) and level re the input (dB) times 'gain'
    int baseTaps (float* ms, float* db, int max, float offMs, float gain) const
    {
        const TankConfig& c = tankConfig (algo);
        if (c.denseER || tapS0.empty()) return 0;
        const int n = std::min ((int) tapS0.size(), max); const float nrm = 1.8f / std::sqrt ((float) tapS0.size());
        for (int t = 0; t < n; ++t)
        {
            ms[t] = offMs + (float) tapS0[(size_t) t] / fs * 1000.0f;
            db[t] = 20.0f * std::log10 (std::max (1e-6f, 0.5f * (std::abs (tapGL[(size_t) t]) + std::abs (tapGR[(size_t) t])) * nrm * c.erLevel * gain));
        }
        return n;
    }
    float buildScale = 1; bool dampOn = false;
    float erAttackMs = 0.0f;         // set before update(): softer attack of the early reflections
    inline void process (float inL, float inR, float mod, float erAmt, float& oL, float& oR) {
        const TankConfig& c = tankConfig (algo);
        erL.push (inL); erR.push (inR);
        float eL = 0, eR = 0;
        if (c.denseER) {
            for (size_t t = 0; t < tapS.size(); ++t) { const int d = tapS[t]; eL += erL.readI (d) * tapGL[t] * tapAtk[t]; eR += erR.readI (d) * tapGR[t] * tapAtk[t]; }
            eL *= erNormDense * 1.6f; eR *= erNormDense * 1.6f;
        } else if (! tapS.empty()) {
            for (size_t t = 0; t < tapS.size(); ++t) {
                const int d = tapS[t];
                const float a = erL.readI (d), b = erR.readI (d);
                const float m = (t & 1) ? 0.6f * b + 0.4f * a : 0.6f * a + 0.4f * b;
                eL += m * tapGL[t] * tapAtk[t]; eR += m * tapGR[t] * tapAtk[t];
            }
            const float nrm = 1.8f / std::sqrt ((float) tapS.size()); eL *= nrm; eR *= nrm;
        }
        if (erEq) { eL = erHi[0].process (erLo[0].process (eL)); eR = erHi[1].process (erLo[1].process (eR)); }
        // tank input: diffused source, a little later than the first reflections, plus part of the ER
        lateL.push (inL); lateR.push (inR);
        float xL = lateL.read (lateS) + 0.35f * eL, xR = lateR.read (lateS) + 0.35f * eR;
        if (c.denseER) { xL = 0.3f * xL + 0.9f * eL; xR = 0.3f * xR + 0.9f * eR; }
        for (int i = 0; i < 4; ++i) { xL = allpass (difL[i], std::round (kDif[i] * k), gIn, xL); xR = allpass (difR[i], std::round (kDif[i] * 1.1f * k), gIn, xR); }
        const float md = mod * c.modMs * fs * 0.001f;
        float o[S];
        for (int i = 0; i < S; ++i) {
            float x = fb[(i + S - 1) % S];
            if (i == 0) x += xL; else if (i == 2) x += xR;
            x = allpassM (ap1[i], lenA1[i] + md * (1.0f + lfo[i].next()) + 1.0f, gT, x, cubicW);
            float y = wanderS > 0.0f ? d1[i].readC (len1[i] + wanderS * (1.0f - lfo2[(i + 2) % S].next())) : d1[i].read (len1[i]); d1[i].push (x);
            if (dampOn && (i & 1) == 0) y = damp[i].lp (y);
            y = band[i].process (y);
            y = allpass (ap2[i], lenA2[i], -gT * 0.85f, y);
            y = allpass (ap3[i], lenA3[i], gT * 0.7f, y);
            o[i] = wanderS > 0.0f ? d2[i].readC (len2[i] + wanderS * (1.0f + lfo2[i].next())) : d2[i].read (len2[i]); d2[i].push (y);
        }
        for (int i = 0; i < S; ++i) fb[i] = o[i];
        auto T1 = [this] (int i, float f) { return d1[i].readI ((int) (len1[i] * f) + 1); };
        auto T2 = [this] (int i, float f) { return d2[i].readI ((int) (len2[i] * f) + 1); };
        const float lL = 0.55f * (T1 (0, 0.21f) - T2 (0, 0.67f) + T1 (1, 0.43f) - T2 (1, 0.11f) + T1 (2, 0.83f) - T2 (2, 0.29f) + T2 (3, 0.55f));
        const float lR = 0.55f * (T1 (2, 0.19f) - T2 (2, 0.71f) + T1 (3, 0.47f) - T2 (3, 0.13f) + T1 (0, 0.79f) - T2 (0, 0.31f) + T2 (1, 0.59f));
        oL = erAmt * c.erLevel * eL + c.lateLevel * lL;
        oR = erAmt * c.erLevel * eR + c.lateLevel * lR;
    }
};

// ---------------------------------------------------------------- compressor (stereo linked)
// Types: 0 = clean VCA, 1 = FET (1176-style), 2 = opto (LA-2A-style), 3 = FET into opto (both).
// Tone: clean, or vintage (FET: distortion that grows with gain reduction; opto: tube make-up warmth).
enum CompType { CompVCA = 0, CompFET, CompOpto, CompFetOpto };

inline float gainComputer (float db, float thr, float ratio, float knee) {
    float over = db - thr;
    if (over <= -knee * 0.5f) return 0.0f;
    if (over >= knee * 0.5f) return over * (1.0f - 1.0f / ratio);
    float t = over + knee * 0.5f;
    return (1.0f - 1.0f / ratio) * t * t / (2.0f * knee);
}
inline float warmSat (float x, float k, float b) {        // unity small-signal gain, asymmetric soft clip
    float tb = std::tanh (b);
    return (std::tanh (k * x + b) - tb) / (k * (1.0f - tb * tb));
}

// Sliding peak over the last 'hold' .. 2x'hold' samples: the level does not collapse at every zero
// crossing, so the gain stays steady and the release time is what actually brings the gain back.
struct PeakHold {
    float cur = 0, prev = 0; int cnt = 0, N = 240;
    void set (float fs, float ms) { N = std::max (1, (int) (ms * 0.001f * fs)); }
    void reset() { cur = prev = 0; cnt = 0; }
    inline float process (float x) { cur = std::max (cur, x); if (++cnt >= N) { prev = cur; cur = 0; cnt = 0; } return std::max (prev, cur); }
};

struct CompParams {
    bool on = false, duck = false; int type = 0; bool vintage = false;
    float thr = -30, ratio = 4, atkMs = 10, relMs = 200, makeup = 0, mix = 1;
};

struct Compressor {
    // Time settings are "time to complete": attack = time to reach full gain reduction,
    // release = time for the gain reduction to return to 0 dB once the signal drops below threshold.
    // FET follows 1176 specs: attack 20 us - 800 us, release 50 ms - 1.1 s.
    // Opto follows LA-2A behaviour: fixed ~10 ms attack, about half the release in 60 ms,
    // the rest over 1.5 - 15 s depending on how long and hard it has been compressing.
    static constexpr float kFetAtkMin = 0.02f, kFetAtkMax = 0.8f, kFetRelMin = 50.0f, kFetRelMax = 1100.0f;   // ms
    static constexpr float kHoldMs = 6.0f;
    float fs = 48000, aM = 0, inPk = 0;
    float aA = 0, aR = 0, aFA = 0, aFR = 0, aOA = 0, aOFast = 0, aOSlow1 = 0, aOSlow2 = 0, aMem = 0, aRms = 0;
    float envV = 0, envF = 0, envOf = 0, envOs = 0, mem = 0, rms = 0, grTotal = 0;
    PeakHold hold;
    Biquad tubeLp[2], tubeLow[2], dc[2];
    void reset() { envV = envF = envOf = envOs = mem = rms = grTotal = inPk = 0; hold.reset(); tV.reset(); tF.reset(); for (int c = 0; c < 2; ++c) { tubeLp[c].reset(); tubeLow[c].reset(); dc[c].reset(); } }
    static float tc (float tauMs, float fs) { return std::exp (-1.0f / (std::max (0.001f, tauMs) * 0.001f * fs)); }
    static float done (float ms, float fs) { return tc (ms / 6.0f, fs); }        // reaches ~99.75 % in 'ms'
    float lastA = -1, lastR = -1, lastFs = -1;
    void setTimes (float atkMs, float relMs) {
        if (atkMs == lastA && relMs == lastR && fs == lastFs) return;
        lastA = atkMs; lastR = relMs; lastFs = fs;
        hold.set (fs, kHoldMs);
        const float holdComp = kHoldMs * 1.5f;                    // the peak window adds this much on average
        aA = done (atkMs, fs); aR = done (std::max (1.0f, relMs - holdComp), fs);
        relV = std::max (1.0f, relMs - holdComp); relF = std::max (1.0f, clampf (relMs, kFetRelMin, kFetRelMax) - holdComp);
        aFA = done (clampf (atkMs, kFetAtkMin, kFetAtkMax), fs);
        aFR = done (std::max (1.0f, clampf (relMs, kFetRelMin, kFetRelMax) - holdComp), fs);
        aOA = done (10.0f, fs);
        aOFast = tc (33.0f, fs);                         // with the 60/40 blend: ~50 % back in 60 ms
        aOSlow1 = tc (300.0f, fs); aOSlow2 = tc (6000.0f, fs);
        aMem = tc (2000.0f, fs); aRms = tc (5.0f, fs);
        aM = tc (300.0f, fs);                            // meter fall
        for (int c = 0; c < 2; ++c) {
            tubeLp[c].lowpass (fs, 17000.0f, 0.6f);
            tubeLow[c].peak (fs, 90.0f, 0.7f, 0.8f);
            dc[c].highpass (fs, 10.0f, 0.7f);
        }
    }
    inline float meter (float L, float R) {
        float lvl = std::max (std::fabs (L), std::fabs (R));
        inPk = lvl > inPk ? lvl : inPk * aM;
        return lvl;
    }
    static inline float follow (float env, float target, float att, float rel) {
        env = target > env ? att * env + (1 - att) * target : rel * env + (1 - rel) * target;
        return (target <= 0.0f && env < 0.02f) ? 0.0f : env;
    }
    // Release that lands exactly on time: when the gain reduction starts to recover, the curve is
    // set so that it is back at 0 dB after 'relMs', however deep it was.
    struct Timed {
        float env = 0, coef = 0, startGr = 0; bool rel = false;
        void reset() { env = startGr = 0; rel = false; }
        inline float step (float target, float att, float relMs, float fs) {
            if (target > env) { env = att * env + (1 - att) * target; rel = false; }
            else {
                if (! rel) {                                          // recovery starts here
                    rel = true; startGr = env;
                    const float n = std::max (1.0f, relMs * 0.001f * fs);
                    const float span = std::log (std::max (1.0001f, (env - target + 0.02f) / 0.02f));
                    coef = std::exp (-span / n);
                }
                env = target + (env - target) * coef;
                if (target <= 0.0f && env < 0.02f) env = 0.0f;
                if (env - target < 0.02f) rel = false;               // settled: next recovery recomputes
            }
            return env;
        }
    };
    Timed tV, tF; float relV = 200, relF = 200;
    inline float stepOpto (float db, float thr, float ratio) {
        float gr = gainComputer (db, thr, ratio, 12.0f);
        mem = aMem * mem + (1 - aMem) * clampf (gr / 10.0f, 0.0f, 1.0f);   // longer/harder compression = slower tail
        const float aSlow = aOSlow1 + (aOSlow2 - aOSlow1) * mem;
        envOf = follow (envOf, gr, aOA, aOFast);
        envOs = follow (envOs, gr, aOA, aSlow);
        return 0.6f * envOf + 0.4f * envOs;
    }
    // kL/kR = key (detector) signal: the reverb itself, or the dry input when ducking.
    inline void process (float& L, float& R, float kL, float kR, const CompParams& p) {
        const float lvl = meter (kL, kR);
        const float pkDb = 20.0f * std::log10 (hold.process (lvl) + 1e-9f);
        float yL = L, yR = R, gr = 0;
        const int type = p.type; const bool vintage = p.vintage;
        if (type == CompVCA) {
            envV = tV.step (gainComputer (pkDb, p.thr, p.ratio, 6.0f), aA, relV, fs);
            gr = envV;
            float k = dbToGain (-gr); yL *= k; yR *= k;
            if (vintage) { yL = warmSat (yL, 1.5f, 0.05f); yR = warmSat (yR, 1.5f, 0.05f); }
        }
        if (type == CompFET || type == CompFetOpto) {
            const float t = type == CompFetOpto ? p.thr + 6.0f : p.thr;      // in the chain the FET only catches peaks
            envF = tF.step (gainComputer (pkDb, t, p.ratio, 2.0f), aFA, relF, fs);
            gr += envF;
            float k = dbToGain (-envF); yL *= k; yR *= k;
            if (vintage) { float d = 1.2f + 0.12f * envF; yL = warmSat (yL, d, 0.12f); yR = warmSat (yR, d, 0.12f); }
        }
        if (type == CompOpto || type == CompFetOpto) {
            float l2 = p.duck ? lvl : std::max (std::fabs (yL), std::fabs (yR));
            rms = aRms * rms + (1 - aRms) * l2 * l2;                    // opto cell reacts to average level
            const float db = 10.0f * std::log10 (rms + 1e-12f) + 3.0f;
            float g = stepOpto (db, p.thr, type == CompFetOpto ? 3.0f : p.ratio);
            gr += g;
            float k = dbToGain (-g); yL *= k; yR *= k;
            if (vintage) {
                yL = tubeLp[0].process (tubeLow[0].process (warmSat (yL, 1.8f, 0.25f)));
                yR = tubeLp[1].process (tubeLow[1].process (warmSat (yR, 1.8f, 0.25f)));
            }
        }
        if (vintage) { yL = dc[0].process (yL); yR = dc[1].process (yR); }
        grTotal = gr;
        const float mk = dbToGain (p.makeup);
        L = L * (1 - p.mix) + yL * mk * p.mix;
        R = R * (1 - p.mix) + yR * mk * p.mix;
    }
    float reductionDb() const { return grTotal; }
    float inputDb() const { return 20.0f * std::log10 (inPk + 1e-9f); }
};

// ---------------------------------------------------------------- stereo delay with gentle analog EQ
struct StereoDelay {
    Line l, r; float curL = 1, curR = 1, fs = 48000, fbStateL = 0, fbStateR = 0;
    Biquad lo[2], mi[2], hi[2], hp[2], lp[2];
    // attack softener: a slow and a fast level follower; their ratio fades each new note in over 'attack'
    float envFast = 0, envSlow = 0, aFastA = 0, aRel = 0, aSlowA = 0; bool soften = false; PeakHold ph;
    void init (float maxFs) { l.init ((int) (kMaxDelaySec * maxFs) + 16); r.init ((int) (kMaxDelaySec * maxFs) + 16); }
    // After a rate change: clear what the current delay time can read right away, and the rest
    // of the old audio in 64k-sample chunks over the next blocks (no CPU spike).
    int pendStart = 0, pendCount = 0;
    void clearSmart (float needSamples) {
        const int total = (int) (kMaxDelaySec * fs) + 16;
        const int now = std::min (total, (int) needSamples + 4096);
        l.clearRecent (now); r.clearRecent (now);
        pendCount = total - now; pendStart = (l.w - total) & l.mask;
        for (int c = 0; c < 2; ++c) { lo[c].reset(); mi[c].reset(); hi[c].reset(); hp[c].reset(); lp[c].reset(); }
        envFast = envSlow = 0;
    }
    void service() {
        if (pendCount <= 0) return;
        const int c = std::min (pendCount, 65536);
        l.clearRange (pendStart, c); r.clearRange (pendStart, c);
        pendStart = (pendStart + c) & l.mask; pendCount -= c;
    }
    void setEq (float lowDb, float midDb, float midHz, float highDb) {
        for (int c = 0; c < 2; ++c) {
            lo[c].lowShelf (fs, 140.0f, lowDb, 0.5f);
            mi[c].peak (fs, midHz, 0.6f, midDb);
            hi[c].highShelf (fs, 5500.0f, highDb, 0.5f);
            hp[c].highpass (fs, 30.0f, 0.6f);
            lp[c].lowpass (fs, 16000.0f, 0.6f);
        }
    }
    void setAttack (float ms) {
        soften = ms > 0.5f; ph.set (fs, 6.0f);
        aFastA = Compressor::done (0.2f, fs); aSlowA = Compressor::done (std::max (0.5f, ms), fs); aRel = Compressor::tc (60.0f, fs);
    }
    static inline float soft (float x) { return std::tanh (x * 0.8f) / 0.8f; }   // very mild, only near clipping
    inline void process (float inL, float inR, float tL, float tR, float fb, bool ping, float smoothA, float& oL, float& oR) {
        if (soften) {
            const float lv = ph.process (std::max (std::fabs (inL), std::fabs (inR)));
            envFast = lv > envFast ? aFastA * envFast + (1 - aFastA) * lv : aRel * envFast + (1 - aRel) * lv;
            envSlow = lv > envSlow ? aSlowA * envSlow + (1 - aSlowA) * lv : aRel * envSlow + (1 - aRel) * lv;
            const float g = envFast > 1e-7f ? std::min (1.0f, envSlow / envFast) : 1.0f;
            inL *= g; inR *= g;
        }
        curL += smoothA * (tL - curL); curR += smoothA * (tR - curR);
        float dL = l.read (std::max (1.0f, curL)), dR = r.read (std::max (1.0f, curR));
        float eL = soft (lp[0].process (hp[0].process (hi[0].process (mi[0].process (lo[0].process (dL))))));
        float eR = soft (lp[1].process (hp[1].process (hi[1].process (mi[1].process (lo[1].process (dR))))));
        if (ping) { l.push (0.5f * (inL + inR) + fb * eR); r.push (fb * eL); }
        else      { l.push (inL + fb * eL); r.push (inR + fb * eR); }
        oL = eL; oR = eR;
    }
};

// ---------------------------------------------------------------- output transformer models
struct TransformerModel { const char* name; float sat, asym, hfHz, hfQ, bumpHz, bumpDb; };
constexpr int kNumTransformers = 6;
inline const TransformerModel& transformerModel (int i) {
    static const TransformerModel m[kNumTransformers] = {
        { "Marinair-style (British console)",  1.00f, 0.10f, 24000, 0.85f,  70, 1.5f },
        { "Carnhill-style (British reissue)",  0.90f, 0.08f, 28000, 0.80f,  80, 1.2f },
        { "Jensen-style (US clean)",           0.45f, 0.02f, 45000, 0.71f,  40, 0.4f },
        { "UTC-style (US vintage)",            0.95f, 0.16f, 16000, 1.20f, 100, 2.0f },
        { "Lundahl-style (Scandinavian)",      0.60f, 0.05f, 38000, 0.72f,  50, 0.8f },
        { "Haufe-style (German broadcast)",    0.80f, 0.13f, 20000, 0.95f,  90, 1.4f }
    };
    return m[std::min (std::max (i, 0), kNumTransformers - 1)];
}

// Core saturation with low-frequency pre-emphasis / de-emphasis: the linear parts cancel,
// so the distortion is concentrated in the lows below 'freq', like real transformer iron.
struct Transformer {
    float fs = 48000, drive = 1, bias = 0, biasOff = 0, norm = 1;
    Biquad pre[2], post[2], bump[2], hf[2], dc[2];
    void clear() { for (int c = 0; c < 2; ++c) { pre[c].reset(); post[c].reset(); bump[c].reset(); hf[c].reset(); dc[c].reset(); } }
    void set (int model, float freq, float amount) {
        const auto& m = transformerModel (model);
        const float emph = 12.0f * amount;
        drive = 0.05f + 2.5f * amount * m.sat;            // linear drive: amount 0 = practically clean
        bias = m.asym * (0.5f + 1.5f * amount);
        biasOff = std::tanh (bias);
        norm = 1.0f / (drive * (1.0f - biasOff * biasOff));
        for (int c = 0; c < 2; ++c) {
            pre[c].lowShelf (fs, freq, emph, 0.7f);
            post[c].lowShelf (fs, freq, -emph, 0.7f);
            bump[c].peak (fs, m.bumpHz * (freq / 80.0f), 0.8f, m.bumpDb * (0.3f + amount));
            hf[c].lowpass (fs, std::min (m.hfHz, 0.45f * fs), m.hfQ);
            dc[c].highpass (fs, 10.0f, 0.7f);
        }
    }
    inline float process (int c, float x) {
        float u = pre[c].process (x);
        float v = (std::tanh (drive * u + bias) - biasOff) * norm;
        float y = post[c].process (v);
        y = hf[c].process (bump[c].process (y));
        return dc[c].process (y);
    }
};

// ---------------------------------------------------------------- 480L hardware model (from the M480L motherboard schematics)
// Analog in:  XLR -> optional input transformer -> LM833 balanced receiver (10k / 100 pF) -> input level ->
//             9-pole 20 kHz low-pass module (LPF6) -> A/D.
// A/D:        successive approximation around a PCM53 16-bit DAC with LM311 comparators and a gain-ranging
//             window: 16 bits plus two 6 dB ranges = "18 bit equivalent".
// D/A:        PCM53 (16 bit) plus two extra LSBs through a resistor ladder (12.1k / 24k) = 18 bit, NE5534 I/V,
//             SD5000 sample-and-hold deglitcher (zero-order hold: the sin(x)/x droop towards Nyquist),
//             9-pole 20 kHz low-pass module (LPF2), LM833, output level, LLA5200 balanced line driver,
//             optional output transformer. Main and Aux outputs are built the same way.
// Digital I/O (AES) skips the analog stages and the converters.

// 9th-order Chebyshev low-pass (0.1 dB ripple, edge 'f'), unity DC gain: one real pole + four pole pairs
struct LowPass9 {
    Biquad s[2][4]; float b0 = 1, b1 = 0, a1 = 0, z[2] = {}; float hz = -1, rate = -1;
    void reset() { for (auto& c : s) for (auto& b : c) b.reset(); z[0] = z[1] = 0; }
    void set (float fs, float f) {
        f = std::min (f, 0.45f * fs);
        if (f == hz && fs == rate) return;
        hz = f; rate = fs;
        const int N = 9; const float rip = 0.1f;
        const float eps = std::sqrt (std::pow (10.0f, 0.1f * rip) - 1.0f), v = std::asinh (1.0f / eps) / (float) N;
        const float W = std::tan (kPi * f / fs);
        for (int k = 0; k < 4; ++k) {
            const float th = kPi * (float) (2 * k + 1) / (float) (2 * N);
            const float sg = -std::sinh (v) * std::sin (th) * W, om = std::cosh (v) * std::cos (th) * W;
            for (int c = 0; c < 2; ++c) s[c][k].fromPoles (sg, om);
        }
        const float a = std::sinh (v) * W;           // the real pole, through the bilinear transform
        b0 = a / (1.0f + a); b1 = b0; a1 = (a - 1.0f) / (1.0f + a);
    }
    inline float process (int c, float x) {
        const float y = b0 * x + z[c]; z[c] = b1 * x - a1 * y;
        float o = y; for (auto& b : s[c]) o = b.process (o);
        return o;
    }
};

// op-amp / line-driver headroom: clean up to about +6 dBFS, then a hard-ish knee (rails)
// below half scale the curve differs from a straight line by less than 2e-6 (-114 dB): skip the three square roots there
inline float railClip (float x) {
    if (std::fabs (x) < 0.5f) return x;
    const float r = x * 0.5f, r2 = r * r, r4 = r2 * r2; return x / std::sqrt (std::sqrt (std::sqrt (1.0f + r4 * r4)));
}

// 16-bit converter with gain ranging: below half scale the signal is switched up 6 dB (twice at most)
inline float adc18 (float x) {
    float step = 1.0f / 32768.0f; const float a = std::fabs (x);
    if (a < 0.5f) { step *= 0.5f; if (a < 0.25f) step *= 0.5f; }
    return std::fabs (x) < 16.0f ? roundFast (x / step) * step : x;      // no dither: silence stays silent
}
inline float dac18 (float x) { return std::fabs (x) < 16.0f ? roundFast (x * 131072.0f) * (1.0f / 131072.0f) : x; }

struct HwParams {
    bool on = true;                 // ANALOG master: off = no hardware model at all
    bool inAnalog = true, outAnalog = true, xfIn = false, xfOut = false, noise = false;
};

// one stereo analog input (with its A/D), or one stereo analog output (D/A onwards)
// first-order high-pass (a coupling capacitor into its load)
struct CouplingHp {
    float a = 1, x1 = 0, y1 = 0;
    void set (float fs, float f) { a = 1.0f / (1.0f + 2.0f * kPi * f / fs); }
    void reset() { x1 = y1 = 0; }
    inline float process (float x) { y1 = a * (y1 + x - x1); x1 = x; return y1; }
};

// Analog paths as drawn in the 480L motherboard schematics (060-04377 rev 7):
//  in:  balanced LM833 receiver -> 9-pole 20 kHz LPF module -> 0.68 uF / 33.2k coupling (7 Hz) -> second 0.68 uF
//       coupling into the A/D buffer (7 Hz) -> sample-and-hold -> PCM53 successive-approximation A/D with gain ranging
//  out: PCM53 + 2 extra LSBs (18 bit) -> 5534 I/V with 4.22k/510 pF (74 kHz pole) -> SD5000 deglitcher (hold)
//       -> LM833 2-pole low-pass (~31 kHz) -> 9-pole 20 kHz LPF module -> LLA5200 line driver -> optional transformer
struct HwPath {
    float fs = 48000, baseFs = 48000; LowPass9 lpf; Biquad zoh[2], post[2]; OnePole i2v[2]; CouplingHp cpl[2][2];
    Transformer xf; uint32_t seed = 777u; bool postReal = false;
    bool xfReady = false;
    void prepare (float internalFs, float hostFs) {
        fs = internalFs; baseFs = hostFs;
        while (baseFs > 50000.0f) baseFs *= 0.5f;                // the 480L itself runs at 44.1 or 48 kHz
        lpf.set (fs, 20000.0f);
        // zero-order hold droop of the deglitcher at the hardware rate (sin x / x): about -3.9 dB at Nyquist,
        // -1.6 dB at 15 kHz (44.1 kHz); a high shelf follows that curve closely up to 20 kHz
        for (int c = 0; c < 2; ++c) zoh[c].highShelf (fs, 0.36f * baseFs, -3.9f, 0.55f);
        // post-deglitch filters (31 kHz 2-pole + 74 kHz pole): about -1 dB at 20 kHz, -0.4 dB at 15 kHz.
        // At high host rates they are run as drawn; at 44.1/48 kHz they lie above Nyquist, so a shelf gives their in-band droop.
        postReal = fs > 88000.0f;
        for (int c = 0; c < 2; ++c)
        {
            if (postReal) { post[c].lowpass (fs, 31000.0f, 0.707f); i2v[c].setCut (fs, 74000.0f); }
            else post[c].highShelf (fs, 0.42f * fs, -1.0f, 0.6f);
            for (auto& k : cpl[c]) k.set (fs, 7.0f);
        }
        xf.fs = fs; xf.set (2, 60.0f, 0.15f); xf.clear(); xfReady = true;     // Jensen-style, nearly clean
        reset();
    }
    void reset() { lpf.reset(); for (auto& b : zoh) b.reset(); for (auto& b : post) b.reset(); for (auto& o : i2v) o.reset(); for (auto& c : cpl) for (auto& k : c) k.reset(); xf.clear(); }
    inline float rnd() { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) / 8388608.0f - 1.0f; }
    // analog in -> A/D
    inline void input (float& L, float& R, const HwParams& h) {
        if (h.xfIn) { L = xf.process (0, L); R = xf.process (1, R); }
        L = lpf.process (0, railClip (L)); R = lpf.process (1, railClip (R));
        L = cpl[0][1].process (cpl[0][0].process (L)); R = cpl[1][1].process (cpl[1][0].process (R));
        L = adc18 (L); R = adc18 (R);
    }
    // D/A -> analog out
    inline void output (float& L, float& R, const HwParams& h) {
        if (h.noise) { const float n = 1.3e-5f; L += n * (rnd() + rnd()); R += n * (rnd() + rnd()); }   // about -98 dBFS
        L = dac18 (L); R = dac18 (R);
        if (postReal) { L = i2v[0].lp (L); R = i2v[1].lp (R); }
        L = zoh[0].process (L); R = zoh[1].process (R);
        L = post[0].process (L); R = post[1].process (R);
        L = railClip (lpf.process (0, L)); R = railClip (lpf.process (1, R));
        if (h.xfOut) { L = xf.process (0, L); R = xf.process (1, R); }
    }
};

// 480L configurations: two machines (A, B) and two stereo outputs (Main, Aux)
enum Config { CfgSingle = 0, CfgCascade, CfgStereoSplit, CfgMonoSplit };

// ---------------------------------------------------------------- parameters
// Every slot (Plate, Room, Chamber, Hall, IR) has its own reverb, EQ and compressor settings.
struct SlotParams {
    bool on = false; float level = 1.0f;
    float decaySec = 2.5f, lowMult = 1.0f, highMult = 1.0f, size = 1.0f, predelayMs = 10, buildMs = 15.0f;
    float early = 0.7f, damping = 0.35f, tone = 0.75f, mod = 0.5f, inWidth = 1.0f, diffusion = 0.7f;
    float eqF[4] = { 100, 400, 2500, 8000 }, eqG[4] = { 0, 0, 0, 0 };
    CompParams comp;
    int matA = 0, matB = 0; float matBlend = 0.0f;     // surface (rooms, IR) or plate material, see Materials.h
    float dirt = kDirtDefault;                          // 1..100; 30 = the original sound
    float hiCutHz = 20000.0f;                           // steep high cut like early digital reverbs' bandwidth; >= 19.5 kHz = off
    int converter = 0;                                  // ConverterMode: A/D + D/A resolution around the reverb
    float richness = 0.0f;                              // 0..1 analog harmonic colour on this slot's reverb only; 0 = clean
    float spinHz = 0.8f, xoverHz = 300.0f;              // 480: rate of the random delay movement; bass-multiply crossover
    float trimDb = 0.0f;                                // output trim (presets use it to play at the level of the IR they were fitted to)
};

// Converters of early digital reverbs. 0 = modern 24-bit (no effect). 1 = plain 12-bit.
// 2 and 3 = 12-bit converters with gain ranging ("flying comma"): the signal is switched up by 6 dB steps
// before conversion when it is quiet, so the 12-bit grain follows the signal level and the quiet tail stays
// clean down to 15-bit (EMT 250: three switch steps) or 16-bit (Lexicon 224: four steps) resolution.
enum ConverterMode { Conv24 = 0, Conv12, Conv15Ranging, Conv16Ranging };
inline float convert (float x, int mode) {
    if (mode == Conv24) return x;
    // no clamp at full scale: the reverb's internal level is not the hardware's converter level, and
    // clipping here distorted loud tails (up to 20 % THD); only the bit grain is modelled
    float step = 1.0f / 2048.0f;                        // 12 bits over -1..+1
    if (mode != Conv12) {
        const int maxShift = mode == Conv15Ranging ? 3 : 4;
        const float a = std::fabs (x); float lim = 0.5f;
        for (int e = 0; e < maxShift && a < lim; ++e) { step *= 0.5f; lim *= 0.5f; }
    }
    return std::round (x / step) * step;                // no dither: silence stays silent
}

// 8th-order Chebyshev (0.5 dB ripple) lowpass: about 17 dB down 11 % above the cutoff and 37 dB down at 1.3x,
// close to the anti-alias filters of 1970s/80s digital reverbs (EMT 250: 10.8 kHz, Lexicon 224: 8 kHz)
struct HiCut {
    Biquad s[2][4]; float hz = -1, rate = -1, gain = 1; bool on = false;
    void reset() { for (auto& c : s) for (auto& b : c) b.reset(); }
    void set (float fs, float f) {
        const bool want = f < 19500.0f && f < 0.45f * fs;
        if (want == on && f == hz && fs == rate) return;
        if (! on && want) reset();
        on = want; hz = f; rate = fs;
        if (! on) return;
        const int N = 8; const float rip = 0.5f;
        const float eps = std::sqrt (std::pow (10.0f, 0.1f * rip) - 1.0f), v = std::asinh (1.0f / eps) / (float) N;
        const float W = std::tan (kPi * f / fs);
        for (int k = 0; k < N / 2; ++k) {
            const float th = kPi * (float) (2 * k + 1) / (float) (2 * N);
            const float sg = -std::sinh (v) * std::sin (th) * W, om = std::cosh (v) * std::cos (th) * W;
            for (int c = 0; c < 2; ++c) s[c][k].fromPoles (sg, om);
        }
        gain = 1.0f / std::sqrt (1.0f + eps * eps);       // even order: DC sits at the bottom of the ripple
    }
    inline float process (int c, float x) { if (! on) return x; for (auto& b : s[c]) x = b.process (x); return x * gain; }
};

// Richness: transformer/tape-style harmonic colour on the wet signal of one slot.
// Only the part of the signal below ~1.5 kHz drives the nonlinearity (like iron, whose distortion is
// strongest in the lows), and only the distortion products are added, so the tone is unchanged:
// y = x + (sat (lo) - lo). Mostly 3rd harmonic with some 2nd. 0 = bypassed (bit-exact clean).
// Measured on the plate with a -12 dBFS sine, THD at 1 kHz / 100 Hz: 25 % = 0.07 / 0.4 %,
// 50 % = 0.3 / 2 %, 100 % = 1.4 / 11 %. High frequencies stay almost clean.
struct Richness {
    OnePole lo[2]; Biquad dc[2]; float d = 0, b = 0, tb = 0, nrm = 1, last = -1, lastFs = -1; bool on = false;
    void reset() { for (int c = 0; c < 2; ++c) { lo[c].reset(); dc[c].reset(); } }
    void set (float fs, float r) {
        r = clampf (r, 0.0f, 1.0f);
        if (r == last && fs == lastFs) return;
        if (! on && r > 0.0f) reset();
        last = r; lastFs = fs; on = r > 0.0f;
        d = 3.0f * std::pow (r, 1.5f) + 1.0e-4f;
        b = 0.08f * std::sqrt (r);
        tb = std::tanh (b); nrm = 1.0f / (d * (1.0f - tb * tb));
        for (int c = 0; c < 2; ++c) { lo[c].setCut (fs, 1500.0f); dc[c].highpass (fs, 8.0f, 0.7f); }
    }
    inline float process (int c, float x) {
        if (! on) return x;
        const float l = lo[c].lp (x);
        const float nl = (std::tanh (d * l + b) - tb) * nrm;
        return x + dc[c].process (nl - l);
    }
};

struct Params {
    int oversampling = 1;                  // 1,2,3,4
    SlotParams slot[kNumSlots];
    float rvMix = 0.3f;
    bool delayOn = false; float delayMsL = 375, delayMsR = 500, delayFb = 0.35f, delayMix = 0.0f, delayAttackMs = 0.0f;
    bool pingPong = false; float dlPanL = -1, dlPanR = 1, dlWidth = 1; float dlLowDb = 0, dlMidDb = 0, dlMidHz = 1000, dlHighDb = 0;
    bool trOn = false; int trModel = 0; float trFreq = 80, trAmount = 0.3f;
    float outGainDb = 0;
    HwParams hw;
    int config = CfgSingle, machA = Lex480, machB = Hall; bool auxToMain = true;
    Params() { slot[Plate].on = true; }
};

// ---------------------------------------------------------------- one slot's signal chain
struct SlotChain {
    Line preL, preR; OnePole tone[2]; Biquad eq[2][4]; HiCut hc; Compressor comp; Richness rich;
    Line dfL[2], dfR[2], mdL, mdR; RandLfo mlfo[2];               // IR slot: diffusion and modulation
    float sLvl = 0; bool running = false, dirty = true;
    static constexpr float kDfMs[4] = { 1.37f, 3.11f, 1.63f, 2.79f };
    void init (float maxFs) {
        preL.init ((int) (0.26f * maxFs) + 16); preR.init ((int) (0.26f * maxFs) + 16);
        for (int i = 0; i < 2; ++i) { dfL[i].init ((int) (kDfMs[i] * 0.001f * maxFs) + 16); dfR[i].init ((int) (kDfMs[i + 2] * 0.001f * maxFs) + 16); }
        mdL.init ((int) (0.002f * maxFs) + 16); mdR.init ((int) (0.002f * maxFs) + 16);
        mlfo[0].init (101); mlfo[1].init (202);
    }
    void clear (float fs) {
        preL.clearRecent ((int) (0.26f * fs) + 16); preR.clearRecent ((int) (0.26f * fs) + 16);
        for (int i = 0; i < 2; ++i) { dfL[i].clearRecent ((int) (kDfMs[i] * 0.001f * fs) + 16); dfR[i].clearRecent ((int) (kDfMs[i + 2] * 0.001f * fs) + 16); }
        mdL.clearRecent ((int) (0.002f * fs) + 16); mdR.clearRecent ((int) (0.002f * fs) + 16);
        tone[0].reset(); tone[1].reset();
        for (int c = 0; c < 2; ++c) for (int b = 0; b < 4; ++b) eq[c][b].reset();
        hc.reset(); rich.reset();
        comp.reset();
    }
    void update (float fs, const SlotParams& p) {
        for (int c = 0; c < 2; ++c) {
            eq[c][0].lowShelf (fs, p.eqF[0], p.eqG[0]);
            eq[c][1].peak (fs, p.eqF[1], 0.9f, p.eqG[1]);
            eq[c][2].peak (fs, p.eqF[2], 0.9f, p.eqG[2]);
            eq[c][3].highShelf (fs, p.eqF[3], p.eqG[3]);
            tone[c].setCut (fs, 400.0f * std::pow (50.0f, p.tone));
        }
        hc.set (fs, p.hiCutHz);
        rich.set (fs, p.richness);
        comp.fs = fs; comp.setTimes (p.comp.atkMs, p.comp.relMs);
        mlfo[0].setRate (fs, 0.7f); mlfo[1].setRate (fs, 0.9f);
    }
    // predelay, input tone and input width
    inline void input (float xL, float xR, float preS, float inW, bool toneOn, float& pl, float& pr) {
        pl = preL.read (preS); pr = preR.read (preS);
        preL.push (xL); preR.push (xR);
        if (toneOn) { pl = tone[0].lp (pl); pr = tone[1].lp (pr); }
        const float mid = 0.5f * (pl + pr), side = 0.5f * (pl - pr) * inW; pl = mid + side; pr = mid - side;
    }
    inline void post (float& L, float& R, float kL, float kR, const CompParams& cp) {
        L = hc.process (0, L); R = hc.process (1, R);
        for (int b = 0; b < 4; ++b) { L = eq[0][b].process (L); R = eq[1][b].process (R); }
        if (cp.on) comp.process (L, R, cp.duck ? kL : L, cp.duck ? kR : R, cp);
        else comp.meter (cp.duck ? kL : L, cp.duck ? kR : R);
    }
};

// ---------------------------------------------------------------- engine
class Engine {
public:
    void prepare (double sampleRate) {
        baseFs = (float) sampleRate;
        const float maxFs = baseFs * kMaxOS;
        plate.init (maxFs);
        for (int a = Room; a <= Hall; ++a) tank[a].init (a, maxFs);
        tank480.init (4, maxFs);
        for (auto& s : chain) s.init (maxFs);
        irIn.init (baseFs);
        delay.init (maxFs);
        for (int c = 0; c < 2; ++c) { up[c].reserve (kMaxOS); dn[c].reserve (kMaxOS); dnAux[c].reserve (kMaxOS); irUp[c].reserve (kMaxOS); }
        setOversampling (1, true);
    }
    void setOversampling (int n, bool force = false) {
        if (n < 1 || n > kMaxOS) n = 1;
        if (n == os && ! force) return;
        os = n; fs = baseFs * (float) os;
        for (int c = 0; c < 2; ++c) { up[c].design (os); dn[c].design (os); dnAux[c].design (os); irUp[c].design (os); }
        plate.fs = fs;
        for (int a = Room; a <= Hall; ++a) tank[a].fs = fs;
        tank480.fs = fs;
        for (auto& s : chain) { s.dirty = true; s.comp.fs = fs; s.comp.lastFs = -1; }
        delay.fs = fs; delayDirty = true; delay.curL = delay.curR = 0; firstBlock = true;
        tr.fs = fs; tr.clear();
        hwIn.prepare (fs, baseFs); hwMain.prepare (fs, baseFs); hwAux.prepare (fs, baseFs);
    }
    int latency() const { return up[0].latencyBase(); }
    float compReductionDb (int s) const { return chain[s].comp.reductionDb(); }
    float compInputDb (int s) const { return chain[s].comp.inputDb(); }
    float meterIn() const { return mIn; }
    float meterMain() const { return mMain; }
    float meterAux() const { return mAux; }

    // IR slot input at the base rate: predelay, tone and input width, ready for the convolution
    void irInput (const float* L, const float* R, float* oL, float* oR, int n, const Params& p) {
        const SlotParams& s = p.slot[IR];
        irIn.update (baseFs, s);
        const float preS = std::max (1.0f, s.predelayMs * 0.001f * baseFs), inW = clampf (s.inWidth, 0.0f, 1.0f);
        for (int i = 0; i < n; ++i) irIn.input (L[i], R[i], preS, inW, s.tone < 0.999f, oL[i], oR[i]);
    }

    // irL/irR: the IR slot's convolution output for this block (base rate), or null.
    // auxL/auxR: the Aux output (machine B, or A in Cascade), or null when the host has no Aux bus.
    void process (float* L, float* R, int n, const Params& p, const float* irL = nullptr, const float* irR = nullptr,
                  float* auxL = nullptr, float* auxR = nullptr) {
        setOversampling (p.oversampling);
        for (int s = 0; s < kNumSlots; ++s) chain[s].update (fs, p.slot[s]);
        delay.setEq (p.dlLowDb, p.dlMidDb, p.dlMidHz, p.dlHighDb);
        delay.setAttack (p.delayAttackMs);
        tr.set (p.trModel, p.trFreq, p.trAmount);
        for (int s = 0; s < kNumSlots; ++s) {
            const SlotParams& sp = p.slot[s];
            preS[s] = std::max (1.0f, sp.predelayMs * 0.001f * fs); inW[s] = clampf (sp.inWidth, 0.0f, 1.0f);
            const float dampHz = 20000.0f * std::pow (0.05f, sp.damping);
            // Dirt: 30 = the original sound. Lower = cubic interpolation of the modulated delays and a little
            // more diffusion (cleaner, smoother); higher = less diffusion, modulation jitter and converter grit.
            const float dirt = clampf (sp.dirt, 1.0f, 100.0f);
            const float clean = dirt < kDirtDefault ? (kDirtDefault - dirt) / (kDirtDefault - 1.0f) : 0.0f;
            grit[s] = s == IR ? 0.0f : (dirt > kDirtDefault ? (dirt - kDirtDefault) / (100.0f - kDirtDefault) : 0.0f);
            const float diffD = sp.diffusion + 0.15f * clean - 0.2f * grit[s];
            gritQ[s] = grit[s] > 0.0f ? std::pow (2.0f, -(23.0f - 11.0f * grit[s])) : 0.0f;    // 24-bit -> 12-bit
            cubicW[s] = clean;
            if (s == Plate) {
                plateDampA = 1.0f - std::exp (-2.0f * kPi * std::min (dampHz, 0.45f * fs) / fs);
                plate.update (sp.decaySec, sp.size, sp.lowMult, sp.highMult, sp.buildMs, diffD, plateEffect (sp.matA, sp.matB, sp.matBlend), clean);
            }
            else if (s == Lex480) {
                tank480.update (sp.decaySec, dampHz, sp.size, sp.lowMult, sp.highMult, sp.buildMs, diffD,
                                surfaceEffect (sp.matA, sp.matB, sp.matBlend, 3.0f * sp.size), clean, std::max (0.05f, sp.spinHz), sp.xoverHz);
            }
            else if (s != IR) {
                const float vOverS = (s == Room ? 0.8f : s == Chamber ? 1.0f : 3.0f) * sp.size;
                tank[s].update (sp.decaySec, dampHz, sp.size, sp.lowMult, sp.highMult, sp.buildMs, diffD, surfaceEffect (sp.matA, sp.matB, sp.matBlend, vOverS), clean,
                                -1.0f, sp.xoverHz);
            }
        }
        irDiff = 0.6f * clampf (p.slot[IR].diffusion, 0.0f, 1.0f); irMod = clampf (p.slot[IR].mod, 0.0f, 1.0f) * 0.0004f * fs;
        const bool haveIr = irL != nullptr && irR != nullptr;
        const float tL = clampf (p.delayMsL * 0.001f * fs, 1.0f, kMaxDelaySec * fs - 4);
        const float tR = clampf (p.delayMsR * 0.001f * fs, 1.0f, kMaxDelaySec * fs - 4);
        if (firstBlock) { delay.curL = tL; delay.curR = tR; firstBlock = false; }
        if (delayDirty && (p.delayOn || sDl > 1e-4f)) { delay.clearSmart (std::max (tL, tR)); delayDirty = false; }
        delay.service();
        const float smA = 1.0f - std::exp (-1.0f / (0.03f * fs));      // ~30 ms parameter smoothing
        auto panGains = [] (float pan, float& gl, float& gr) { float a = (clampf (pan, -1, 1) + 1) * 0.25f * kPi; gl = std::cos (a); gr = std::sin (a); };
        float pLl, pLr, pRl, pRr; panGains (p.dlPanL, pLl, pLr); panGains (p.dlPanR, pRl, pRr);
        const float width = clampf (p.dlWidth, 0.0f, 2.0f);
        const float tgtRv = p.rvMix, tgtDl = p.delayOn ? p.delayMix : 0.0f, tgtOut = dbToGain (p.outGainDb);

        const int cfg = std::min (std::max (p.config, 0), 3);
        const int mA = std::min (std::max (p.machA, 0), kNumSlots - 1), mB = std::min (std::max (p.machB, 0), kNumSlots - 1);
        if (cfg != lastCfg) { lastCfg = cfg; for (auto& c : chain) c.dirty = true; }
        float tgtLvl[kNumSlots];
        if (cfg == CfgSingle) {
            int nOn = 0; for (int s = 0; s < kNumSlots; ++s) nOn += p.slot[s].on ? 1 : 0;
            const float norm = nOn > 0 ? 1.0f / std::sqrt ((float) nOn) : 0.0f;
            for (int s = 0; s < kNumSlots; ++s) tgtLvl[s] = p.slot[s].on ? p.slot[s].level * dbToGain (p.slot[s].trimDb) * norm : 0.0f;
        } else
            for (int s = 0; s < kNumSlots; ++s) tgtLvl[s] = (s == mA || s == mB) ? p.slot[s].level * dbToGain (p.slot[s].trimDb) : 0.0f;
        const HwParams& hw = p.hw;
        const bool hwInOn = hw.on && hw.inAnalog, hwOutOn = hw.on && hw.outAnalog;
        const bool wantAux = auxL != nullptr && auxR != nullptr;
        float pkIn = 0, pkMain = 0, pkAux = 0;

        float bufL[kMaxOS], bufR[kMaxOS], irBL[kMaxOS], irBR[kMaxOS], axL[kMaxOS], axR[kMaxOS];
        for (int i = 0; i < n; ++i) {
            up[0].up (L[i], bufL); up[1].up (R[i], bufR);
            if (haveIr) { irUp[0].up (irL[i], irBL); irUp[1].up (irR[i], irBR); }
            for (int k = 0; k < os; ++k) {
                const float xL = bufL[k], xR = bufR[k];
                pkIn = std::max (pkIn, std::max (std::fabs (xL), std::fabs (xR)));
                // parameter smoothing that lands exactly on its target: a plain one-pole stalls ~4e-5 short of it
                // in float, which left the dry signal leaking about -88 dB into a 100 % wet reverb
                smooth (sRv, tgtRv, smA); smooth (sDl, tgtDl, smA); smooth (sOut, tgtOut, smA);
                for (int s = 0; s < kNumSlots; ++s) smooth (chain[s].sLvl, tgtLvl[s], smA);
                // the algorithms hear the input through the 480L's analog input and A/D (the dry signal does not)
                float hL = xL, hR = xR;
                if (hwInOn) hwIn.input (hL, hR, hw);
                const float m = sRv, dry = 1.0f - sRv;
                float yL, yR, aL = 0, aR = 0;
                if (cfg == CfgSingle) {
                    float gL = 0, gR = 0, iL = 0, iR = 0;           // algorithms (through the hardware) and IR (clean)
                    for (int s = 0; s < kNumSlots; ++s) {
                        float oL, oR;
                        if (! runSlot (s, p.slot[s], s == IR ? 0.0f : hL, s == IR ? 0.0f : hR, xL, xR, haveIr, irBL[k], irBR[k], oL, oR)) continue;
                        if (s == IR) { iL += oL; iR += oR; } else { gL += oL; gR += oR; }
                    }
                    if (hwOutOn) hwMain.output (gL, gR, hw);
                    yL = xL * dry + (gL + iL) * m; yR = xR * dry + (gR + iR) * m;
                    aL = yL; aR = yR;
                } else {
                    float waL = 0, waR = 0, wbL = 0, wbR = 0;
                    const bool same = mA == mB;
                    // machine A
                    float inAL = hL, inAR = hR;
                    if (cfg == CfgMonoSplit) inAR = hL;
                    runMachine (mA, p, inAL, inAR, xL, xR, haveIr, irBL[k], irBR[k], waL, waR);
                    // machine B
                    float inBL = hL, inBR = hR;
                    if (cfg == CfgMonoSplit) inBL = hR;
                    if (cfg == CfgCascade) { inBL = hL * dry + waL * m; inBR = hR * dry + waR * m; }    // A's output feeds B
                    if (same) { wbL = waL; wbR = waR; }
                    else runMachine (mB, p, inBL, inBR, xL, xR, haveIr, irBL[k], irBR[k], wbL, wbR);
                    float dAL = waL, dAR = waR, dBL = wbL, dBR = wbR;
                    if (hwOutOn) { hwMain.output (cfg == CfgCascade ? dBL : dAL, cfg == CfgCascade ? dBR : dAR, hw);
                                   hwAux.output (cfg == CfgCascade ? dAL : dBL, cfg == CfgCascade ? dAR : dBR, hw); }
                    if (cfg == CfgCascade) {
                        // Main = B (fed by A), Aux = A only
                        const float aoL = xL * dry + dAL * m, aoR = xR * dry + dAR * m;
                        yL = aoL * dry + dBL * m; yR = aoR * dry + dBR * m;
                        aL = aoL; aR = aoR;
                    } else {
                        // Main = A, Aux = B
                        yL = xL * dry + dAL * m; yR = xR * dry + dAR * m;
                        aL = xL * dry + dBL * m; aR = xR * dry + dBR * m;
                        if (! wantAux && p.auxToMain) { yL += dBL * m; yR += dBR * m; }   // no Aux bus: hear B in Main
                    }
                }
                // ---- delay (Main only)
                if (p.delayOn || sDl > 1e-4f) {
                    float rL, rR;
                    delay.process (xL, xR, tL, tR, p.delayFb, p.pingPong, smA * 0.2f, rL, rR);
                    float oL = rL * pLl + rR * pRl, oR = rL * pLr + rR * pRr;
                    float mid = 0.5f * (oL + oR), side = 0.5f * (oL - oR) * width;
                    yL += (mid + side) * sDl; yR += (mid - side) * sDl;
                }
                if (p.trOn) { yL = tr.process (0, yL); yR = tr.process (1, yR); }
                bufL[k] = yL * sOut; bufR[k] = yR * sOut;
                axL[k] = aL * sOut; axR[k] = aR * sOut;
            }
            L[i] = dn[0].down (bufL); R[i] = dn[1].down (bufR);
            pkMain = std::max (pkMain, std::max (std::fabs (L[i]), std::fabs (R[i])));
            if (wantAux) {
                auxL[i] = dnAux[0].down (axL); auxR[i] = dnAux[1].down (axR);
                pkAux = std::max (pkAux, std::max (std::fabs (auxL[i]), std::fabs (auxR[i])));
            }
        }
        mIn = pkIn; mMain = pkMain; mAux = wantAux ? pkAux : (cfg == CfgSingle ? pkMain : 0.0f);
    }

private:
    static inline void smooth (float& v, float tgt, float a) { const float d = tgt - v; v = std::fabs (d) < 2.0e-4f ? tgt : v + a * d; }
    // one machine of the split/cascade configurations: its slot's output (wet only)
    inline void runMachine (int s, const Params& p, float inL, float inR, float keyL, float keyR, bool haveIr, float irS_L, float irS_R, float& oL, float& oR) {
        oL = oR = 0.0f;
        runSlot (s, p.slot[s], s == IR ? 0.0f : inL, s == IR ? 0.0f : inR, keyL, keyR, haveIr, irS_L, irS_R, oL, oR, true);
    }
    // one slot for one sample: returns false when the slot is silent and skipped
    inline bool runSlot (int s, const SlotParams& sp, float inL, float inR, float keyL, float keyR, bool haveIr, float irS_L, float irS_R,
                         float& oL, float& oR, bool forceOn = false) {
        SlotChain& c = chain[s];
        oL = oR = 0.0f;
        if (! forceOn && ! sp.on && c.sLvl < 1e-4f) {
            if (c.running) { c.running = false; c.dirty = true; }     // cleared when switched back on
            return false;
        }
        if (c.dirty) { c.clear (fs); if (s == Plate) plate.clear(); else if (s == Lex480) tank480.clear(); else if (s != IR) tank[s].clear(); c.dirty = false; }
        c.running = true;
        if (s == IR) {
            // the IR path stays clean: no converter, grit, richness or high cut - only the IR and the user's EQ/comp
            if (! haveIr) return true;                             // no IR loaded: true silence
            oL = irS_L; oR = irS_R;
            if (irMod > 0.0f) {
                c.mdL.push (oL); c.mdR.push (oR);
                const float dl = 2.0f + irMod * (1.0f + c.mlfo[0].next()), dr = 2.0f + irMod * (1.0f + c.mlfo[1].next());
                oL = c.mdL.read (dl); oR = c.mdR.read (dr);
                if (cubicW[s] > 0.0f) { oL += (c.mdL.readC (dl) - oL) * cubicW[s]; oR += (c.mdR.readC (dr) - oR) * cubicW[s]; }
            }
            if (irDiff > 0.0f)
                for (int j = 0; j < 2; ++j) {
                    oL = allpass (c.dfL[j], std::round (SlotChain::kDfMs[j] * 0.001f * fs), irDiff, oL);
                    oR = allpass (c.dfR[j], std::round (SlotChain::kDfMs[j + 2] * 0.001f * fs), irDiff, oR);
                }
            for (int b = 0; b < 4; ++b) { oL = c.eq[0][b].process (oL); oR = c.eq[1][b].process (oR); }
            if (sp.comp.on) c.comp.process (oL, oR, sp.comp.duck ? keyL : oL, sp.comp.duck ? keyR : oR, sp.comp);
            else c.comp.meter (sp.comp.duck ? keyL : oL, sp.comp.duck ? keyR : oR);
            oL *= c.sLvl; oR *= c.sLvl;
            return true;
        }
        float pl, pr; c.input (inL, inR, preS[s], inW[s], true, pl, pr);
        if (sp.converter != Conv24) { pl = convert (pl, sp.converter); pr = convert (pr, sp.converter); }   // A/D
        if (s == Plate) plate.process (pl, pr, plateDampA, sp.mod, sp.early, oL, oR);
        else if (s == Lex480) tank480.process (pl, pr, sp.mod, sp.early, oL, oR);
        else tank[s].process (pl, pr, sp.mod, sp.early, oL, oR);
        if (sp.converter != Conv24) { oL = convert (oL, sp.converter); oR = convert (oR, sp.converter); }       // D/A
        if (grit[s] > 0.0f) {                     // converter grit: requantised (like early digital reverbs)
            const float q = gritQ[s];
            oL = std::round (oL / q) * q; oR = std::round (oR / q) * q;   // no dither: silence stays silent
        }
        oL = c.rich.process (0, oL); oR = c.rich.process (1, oR);
        c.post (oL, oR, keyL, keyR, sp.comp);
        oL *= c.sLvl; oR *= c.sLvl;
        return true;
    }

    float baseFs = 48000, fs = 48000; int os = 0; bool firstBlock = true; int lastCfg = -1;
    Resampler up[2], dn[2], dnAux[2], irUp[2];
    PlateAlgo plate; TankAlgo tank[4], tank480;
    SlotChain chain[kNumSlots], irIn;
    StereoDelay delay; Transformer tr;
    HwPath hwIn, hwMain, hwAux;
    float sRv = 0.3f, sDl = 0, sOut = 1;
    float grit[kNumSlots] = {}, gritQ[kNumSlots] = {}, cubicW[kNumSlots] = {}, preS[kNumSlots] = {}, inW[kNumSlots] = {};
    float plateDampA = 1, irDiff = 0, irMod = 0;
    float mIn = 0, mMain = 0, mAux = 0;
    bool delayDirty = true;
};

} // namespace pv
