// PlateVerb DSP engine - no JUCE dependency.
// Five reverb slots (Plate, Room, Chamber, Hall, IR), each with its own reverb settings, 4-band EQ
// and tail compressor, tempo-syncable stereo delay with a gentle 3-band EQ,
// output transformer models and integer oversampling (1x, 2x, 3x, 4x).
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <array>
#include <cstdint>

namespace pv {

constexpr float kPi = 3.14159265358979f;
constexpr int kMaxOS = 4;
constexpr float kMaxSize = 1.5f;
constexpr float kMaxDelaySec = 3.0f;
constexpr int kNumSlots = 5;
enum Slot { Plate = 0, Room, Chamber, Hall, IR };

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float clampf (float x, float a, float b) { return std::min (b, std::max (a, x)); }

// ---------------------------------------------------------------- building blocks
struct Line {
    std::vector<float> b; int mask = 0, w = 0;
    void init (int maxLen) { int n = 1; while (n < maxLen + 8) n <<= 1; b.assign ((size_t) n, 0.0f); mask = n - 1; w = 0; }
    void clear() { std::fill (b.begin(), b.end(), 0.0f); w = 0; }
    inline void push (float x) { b[(size_t) w] = x; w = (w + 1) & mask; }
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
};

inline float allpass (Line& l, float d, float g, float x) {
    float z = l.read (d);
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
struct BandDecay {
    OnePole lo, hi; float gL = 1, gM = 1, gH = 1;
    void setX (float fs) { lo.setCut (fs, 300.0f); hi.setCut (fs, 4000.0f); }
    void setGains (float loopSec, float rtMid, float lowMul, float highMul) {
        auto g = [loopSec] (float rt) { return std::min (0.9995f, std::pow (10.0f, -3.0f * loopSec / std::max (0.05f, rt))); };
        gM = g (rtMid); gL = g (rtMid * lowMul); gH = g (rtMid * highMul);
    }
    void reset() { lo.reset(); hi.reset(); }
    inline float process (float x) { const float l = lo.lp (x), h = x - hi.lp (x); return l * gL + (x - l - h) * gM + h * gH; }
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
struct PlateAlgo {
    static constexpr int kEarly = 6;
    Line oa[2], ob[2], d1, d2, d3, d4, e1, e2, e3, e4, apL1, dL1, apL2, apL3, dL2, apR1, dR1, apR2, apR3, dR2, ea[kEarly], eb[kEarly];
    BandDecay bdL, bdR; RandLfo lfo[2];
    float fs = 48000, s = 1, lpL = 0, lpR = 0, tankL = 0, tankR = 0, eScale = 1;
    static constexpr float kEaMs[kEarly] = { 0.53f, 0.93f, 1.71f, 3.07f, 5.3f, 8.9f }, kEbMs[kEarly] = { 0.61f, 1.13f, 1.97f, 2.71f, 6.1f, 10.3f };
    static constexpr float kMaxEScale = 55.0f / 15.0f;
    static constexpr float kOutMs[4] = { 1.37f, 3.11f, 1.63f, 2.79f };
    void init (float maxFs) {
        float sm = maxFs / 29761.0f * kMaxSize;
        auto sz = [sm] (float n) { return (int) std::ceil (n * sm) + 64; };
        d1.init (sz (142)); d2.init (sz (107)); d3.init (sz (379)); d4.init (sz (277));
        e1.init (sz (153)); e2.init (sz (115)); e3.init (sz (406)); e4.init (sz (297));
        apL1.init (sz (704)); dL1.init (sz (4453)); apL2.init (sz (1800)); apL3.init (sz (419)); dL2.init (sz (3720));
        apR1.init (sz (940)); dR1.init (sz (4217)); apR2.init (sz (2656)); apR3.init (sz (557)); dR2.init (sz (3163));
        for (int i = 0; i < kEarly; ++i) { ea[i].init ((int) (kEaMs[i] * kMaxEScale * 0.001f * maxFs) + 16); eb[i].init ((int) (kEbMs[i] * kMaxEScale * 0.001f * maxFs) + 16); }
        lfo[0].init (11); lfo[1].init (23);
        for (int i = 0; i < 2; ++i) { oa[i].init ((int) (kOutMs[i] * 0.001f * maxFs * kMaxSize) + 16); ob[i].init ((int) (kOutMs[i + 2] * 0.001f * maxFs * kMaxSize) + 16); }
    }
    void clear() {
        const float sm = fs / 29761.0f * kMaxSize;
        auto sz = [sm] (float n) { return (int) std::ceil (n * sm) + 64; };
        d1.clearRecent (sz (142)); d2.clearRecent (sz (107)); d3.clearRecent (sz (379)); d4.clearRecent (sz (277));
        e1.clearRecent (sz (153)); e2.clearRecent (sz (115)); e3.clearRecent (sz (406)); e4.clearRecent (sz (297));
        apL1.clearRecent (sz (704)); dL1.clearRecent (sz (4453)); apL2.clearRecent (sz (1800)); apL3.clearRecent (sz (419)); dL2.clearRecent (sz (3720));
        apR1.clearRecent (sz (940)); dR1.clearRecent (sz (4217)); apR2.clearRecent (sz (2656)); apR3.clearRecent (sz (557)); dR2.clearRecent (sz (3163));
        for (int i = 0; i < kEarly; ++i) { ea[i].clearRecent ((int) (kEaMs[i] * kMaxEScale * 0.001f * fs) + 16); eb[i].clearRecent ((int) (kEbMs[i] * kMaxEScale * 0.001f * fs) + 16); }
        for (int i = 0; i < 2; ++i) { oa[i].clearRecent ((int) (kOutMs[i] * 0.001f * fs * kMaxSize) + 16); ob[i].clearRecent ((int) (kOutMs[i + 2] * 0.001f * fs * kMaxSize) + 16); }
        bdL.reset(); bdR.reset();
        lpL = lpR = tankL = tankR = 0;
    }
    float gOut = 0.5f, gIn1 = 0.75f, gIn2 = 0.625f, gT1 = 0.7f, gT2 = 0.5f, gT3 = 0.4f;
    void update (float rt, float size, float lowMul, float highMul, float buildMs, float diffusion) {
        s = fs / 29761.0f * size;
        eScale = clampf (buildMs / 15.0f, 0.2f, kMaxEScale);
        const float d = clampf (diffusion, 0.0f, 1.0f);
        gIn1 = 0.25f + 0.55f * d; gIn2 = 0.20f + 0.48f * d;
        gT1 = 0.45f + 0.30f * d; gT2 = 0.30f + 0.28f * d; gT3 = 0.25f + 0.30f * d; gOut = 0.65f * d;
        const float halfLoop = (10645.0f + 488.0f) * s / fs;
        bdL.setX (fs); bdR.setX (fs);
        bdL.setGains (halfLoop, rt, lowMul, highMul); bdR.setGains (halfLoop, rt, lowMul, highMul);
        lfo[0].setRate (fs, 0.83f); lfo[1].setRate (fs, 1.07f);
    }
    inline void process (float inL, float inR, float dampA, float mod, float early, float& oL, float& oR) {
        const float exc = mod * 14.0f * s;
        auto R = [this] (float n) { return std::round (n * s); };
        float xL = allpass (d1, R (142), gIn1, inL);  xL = allpass (d2, R (107), gIn1, xL);
        xL = allpass (d3, R (379), gIn2, xL); xL = allpass (d4, R (277), gIn2, xL);
        float xR = allpass (e1, R (153), gIn1, inR);  xR = allpass (e2, R (115), gIn1, xR);
        xR = allpass (e3, R (406), gIn2, xR); xR = allpass (e4, R (297), gIn2, xR);
        const float m1 = lfo[0].next(), m2 = lfo[1].next();
        float a = allpass (apL1, 672 * s + exc * (1 + m1), -gT1, xR + tankR);
        float t1 = dL1.read (R (4453)); dL1.push (a); lpL += dampA * (t1 - lpL);
        float b = allpass (apL2, R (1800), gT2, bdL.process (lpL)); b = allpass (apL3, R (419), gT3, b);
        float t2 = dL2.read (R (3720)); dL2.push (b);
        float c = allpass (apR1, 908 * s + exc * (1 + m2), -gT1, xL + tankL);
        float t3 = dR1.read (R (4217)); dR1.push (c); lpR += dampA * (t3 - lpR);
        float e = allpass (apR2, R (2656), gT2, bdR.process (lpR)); e = allpass (apR3, R (557), gT3, e);
        float t4 = dR2.read (R (3163)); dR2.push (e);
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
    static const TankConfig c[4] = {
        { 1.00f, 1.0f, 0.55f, 0.80f, 0.40f, 0.70f, 0.0f, 1.0f,  0, 0, 0, false, 0.20f, 0.9f, 0.0f },   // (plate, unused)
        { 0.85f, 1.0f, 0.00f, 0.78f, 0.20f, 0.65f, 1.06f, 1.06f, 18, 2.5f, 38.0f, false, 0.25f, 1.1f, 4.0f },  // room
        { 0.92f, 1.0f, 0.00f, 0.80f, 0.20f, 0.68f, 0.63f, 0.70f,  0, 1.0f, 60.0f, true,  0.30f, 0.9f, 2.0f },   // chamber
        { 1.10f, 1.0f, 0.00f, 0.72f, 0.15f, 0.62f, 0.6f, 1.19f, 14, 9.0f, 95.0f, false, 0.45f, 0.7f, 12.0f }   // hall
    };
    return c[std::min (std::max (algo, 0), 3)];
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
    OnePole damp[S]; BandDecay band[S]; RandLfo lfo[S];
    float fb[S] = {};
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
            ap1[i].init ((int) (kAp1[i] * km + modS) + 16); d1[i].init ((int) (kD1[i] * km) + 16);
            ap2[i].init ((int) (kAp2[i] * km) + 16); ap3[i].init ((int) (kAp3[i] * km) + 16); d2[i].init ((int) (kD2[i] * km) + 16);
            lfo[i].init ((uint32_t) (a * 17 + i * 5 + 3));
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
        erL.init ((int) (maxTap * kMaxSize * maxFs * 0.001f) + 16); erR.init ((int) (maxTap * kMaxSize * maxFs * 0.001f) + 16);
        lateL.init ((int) (c.lateOnsetMs * kMaxSize * maxFs * 0.001f) + 16); lateR.init ((int) (c.lateOnsetMs * kMaxSize * maxFs * 0.001f) + 16);
    }
    void clear() {
        const TankConfig& c = tankConfig (algo);
        const float km = fs * 0.001f * c.scale * kMaxSize, modS = (c.modMs * 2 + 1) * fs * 0.001f;
        for (int i = 0; i < S; ++i) {
            ap1[i].clearRecent ((int) (kAp1[i] * km + modS) + 16); d1[i].clearRecent ((int) (kD1[i] * km) + 16);
            ap2[i].clearRecent ((int) (kAp2[i] * km) + 16); ap3[i].clearRecent ((int) (kAp3[i] * km) + 16); d2[i].clearRecent ((int) (kD2[i] * km) + 16);
            damp[i].reset(); band[i].reset(); fb[i] = 0;
        }
        for (int i = 0; i < 4; ++i) { difL[i].clearRecent ((int) (kDif[i] * 1.1f * km) + 16); difR[i].clearRecent ((int) (kDif[i] * 1.1f * km) + 16); }
        erL.clearRecent ((int) (maxTap * kMaxSize * fs * 0.001f) + 16); erR.clearRecent ((int) (maxTap * kMaxSize * fs * 0.001f) + 16);
        lateL.clearRecent ((int) (c.lateOnsetMs * kMaxSize * fs * 0.001f) + 16); lateR.clearRecent ((int) (c.lateOnsetMs * kMaxSize * fs * 0.001f) + 16);
    }
    float len1[S] = {}, len2[S] = {}, lenA1[S] = {}, lenA2[S] = {}, lenA3[S] = {}, lateS = 1;
    void update (float rt, float dampHz, float size, float lowMul, float highMul, float buildMs, float diffusion) {
        const TankConfig& c = tankConfig (algo);
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
            band[i].setX (fs);
            band[i].setGains (secLen, rtA, lowMul, highMul);
            lfo[i].setRate (fs, c.modHz * (0.8f + 0.13f * (float) i));
        }
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
    }
    std::vector<int> tapS;
    float buildScale = 1; bool dampOn = false;
    inline void process (float inL, float inR, float mod, float erAmt, float& oL, float& oR) {
        const TankConfig& c = tankConfig (algo);
        erL.push (inL); erR.push (inR);
        float eL = 0, eR = 0;
        if (c.denseER) {
            for (size_t t = 0; t < tapS.size(); ++t) { const int d = tapS[t]; eL += erL.readI (d) * tapGL[t]; eR += erR.readI (d) * tapGR[t]; }
            eL *= erNormDense * 1.6f; eR *= erNormDense * 1.6f;
        } else if (! tapS.empty()) {
            for (size_t t = 0; t < tapS.size(); ++t) {
                const int d = tapS[t];
                const float a = erL.readI (d), b = erR.readI (d);
                const float m = (t & 1) ? 0.6f * b + 0.4f * a : 0.6f * a + 0.4f * b;
                eL += m * tapGL[t]; eR += m * tapGR[t];
            }
            const float nrm = 1.8f / std::sqrt ((float) tapS.size()); eL *= nrm; eR *= nrm;
        }
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
            x = allpass (ap1[i], lenA1[i] + md * (1.0f + lfo[i].next()) + 1.0f, gT, x);
            float y = d1[i].read (len1[i]); d1[i].push (x);
            if (dampOn && (i & 1) == 0) y = damp[i].lp (y);
            y = band[i].process (y);
            y = allpass (ap2[i], lenA2[i], -gT * 0.85f, y);
            y = allpass (ap3[i], lenA3[i], gT * 0.7f, y);
            o[i] = d2[i].read (len2[i]); d2[i].push (y);
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
        drive = dbToGain (2.0f + amount * 14.0f * m.sat);
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

// ---------------------------------------------------------------- parameters
// Every slot (Plate, Room, Chamber, Hall, IR) has its own reverb, EQ and compressor settings.
struct SlotParams {
    bool on = false; float level = 1.0f;
    float decaySec = 2.5f, lowMult = 1.0f, highMult = 1.0f, size = 1.0f, predelayMs = 10, buildMs = 15.0f;
    float early = 0.7f, damping = 0.35f, tone = 0.75f, mod = 0.5f, inWidth = 1.0f, diffusion = 0.7f;
    float eqF[4] = { 100, 400, 2500, 8000 }, eqG[4] = { 0, 0, 0, 0 };
    CompParams comp;
};

struct Params {
    int oversampling = 1;                  // 1,2,3,4
    SlotParams slot[kNumSlots];
    float rvMix = 0.3f;
    bool delayOn = false; float delayMsL = 375, delayMsR = 500, delayFb = 0.35f, delayMix = 0.0f, delayAttackMs = 0.0f;
    bool pingPong = false; float dlPanL = -1, dlPanR = 1, dlWidth = 1; float dlLowDb = 0, dlMidDb = 0, dlMidHz = 1000, dlHighDb = 0;
    bool trOn = false; int trModel = 0; float trFreq = 80, trAmount = 0.3f;
    float outGainDb = 0;
    Params() { slot[Plate].on = true; }
};

// ---------------------------------------------------------------- one slot's signal chain
struct SlotChain {
    Line preL, preR; OnePole tone[2]; Biquad eq[2][4]; Compressor comp;
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
        for (auto& s : chain) s.init (maxFs);
        irIn.init (baseFs);
        delay.init (maxFs);
        for (int c = 0; c < 2; ++c) { up[c].reserve (kMaxOS); dn[c].reserve (kMaxOS); irUp[c].reserve (kMaxOS); }
        setOversampling (1, true);
    }
    void setOversampling (int n, bool force = false) {
        if (n < 1 || n > kMaxOS) n = 1;
        if (n == os && ! force) return;
        os = n; fs = baseFs * (float) os;
        for (int c = 0; c < 2; ++c) { up[c].design (os); dn[c].design (os); irUp[c].design (os); }
        plate.fs = fs;
        for (int a = Room; a <= Hall; ++a) tank[a].fs = fs;
        for (auto& s : chain) { s.dirty = true; s.comp.fs = fs; s.comp.lastFs = -1; }
        delay.fs = fs; delayDirty = true; delay.curL = delay.curR = 0; firstBlock = true;
        tr.fs = fs; tr.clear();
    }
    int latency() const { return up[0].latencyBase(); }
    float compReductionDb (int s) const { return chain[s].comp.reductionDb(); }
    float compInputDb (int s) const { return chain[s].comp.inputDb(); }

    // IR slot input at the base rate: predelay, tone and input width, ready for the convolution
    void irInput (const float* L, const float* R, float* oL, float* oR, int n, const Params& p) {
        const SlotParams& s = p.slot[IR];
        irIn.update (baseFs, s);
        const float preS = std::max (1.0f, s.predelayMs * 0.001f * baseFs), inW = clampf (s.inWidth, 0.0f, 1.0f);
        for (int i = 0; i < n; ++i) irIn.input (L[i], R[i], preS, inW, s.tone < 0.999f, oL[i], oR[i]);
    }

    // irL/irR: the IR slot's convolution output for this block (base rate), or null
    void process (float* L, float* R, int n, const Params& p, const float* irL = nullptr, const float* irR = nullptr) {
        setOversampling (p.oversampling);
        for (int s = 0; s < kNumSlots; ++s) chain[s].update (fs, p.slot[s]);
        delay.setEq (p.dlLowDb, p.dlMidDb, p.dlMidHz, p.dlHighDb);
        delay.setAttack (p.delayAttackMs);
        tr.set (p.trModel, p.trFreq, p.trAmount);
        float plateDampA = 1, preS[kNumSlots], inW[kNumSlots];
        for (int s = 0; s < kNumSlots; ++s) {
            const SlotParams& sp = p.slot[s];
            preS[s] = std::max (1.0f, sp.predelayMs * 0.001f * fs); inW[s] = clampf (sp.inWidth, 0.0f, 1.0f);
            const float dampHz = 20000.0f * std::pow (0.05f, sp.damping);
            if (s == Plate) { plateDampA = 1.0f - std::exp (-2.0f * kPi * std::min (dampHz, 0.45f * fs) / fs); plate.update (sp.decaySec, sp.size, sp.lowMult, sp.highMult, sp.buildMs, sp.diffusion); }
            else if (s != IR) tank[s].update (sp.decaySec, dampHz, sp.size, sp.lowMult, sp.highMult, sp.buildMs, sp.diffusion);
        }
        const float irDiff = 0.6f * clampf (p.slot[IR].diffusion, 0.0f, 1.0f), irMod = clampf (p.slot[IR].mod, 0.0f, 1.0f) * 0.0004f * fs;
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

        int nOn = 0; for (int s = 0; s < kNumSlots; ++s) nOn += p.slot[s].on ? 1 : 0;
        const float norm = nOn > 0 ? 1.0f / std::sqrt ((float) nOn) : 0.0f;
        float tgtLvl[kNumSlots];
        for (int s = 0; s < kNumSlots; ++s) tgtLvl[s] = p.slot[s].on ? p.slot[s].level * norm : 0.0f;

        float bufL[kMaxOS], bufR[kMaxOS], irBL[kMaxOS], irBR[kMaxOS];
        for (int i = 0; i < n; ++i) {
            up[0].up (L[i], bufL); up[1].up (R[i], bufR);
            if (haveIr) { irUp[0].up (irL[i], irBL); irUp[1].up (irR[i], irBR); }
            for (int k = 0; k < os; ++k) {
                const float xL = bufL[k], xR = bufR[k];
                sRv += smA * (tgtRv - sRv); sDl += smA * (tgtDl - sDl); sOut += smA * (tgtOut - sOut);
                float wL = 0, wR = 0;
                for (int s = 0; s < kNumSlots; ++s) {
                    SlotChain& c = chain[s];
                    const SlotParams& sp = p.slot[s];
                    c.sLvl += smA * (tgtLvl[s] - c.sLvl);
                    if (! sp.on && c.sLvl < 1e-4f) {
                        if (c.running) { c.running = false; c.dirty = true; }     // cleared when switched back on
                        continue;
                    }
                    if (c.dirty) { c.clear (fs); if (s == Plate) plate.clear(); else if (s != IR) tank[s].clear(); c.dirty = false; }
                    c.running = true;
                    float oL = 0, oR = 0;
                    if (s == IR) {
                        if (haveIr) {
                            oL = irBL[k]; oR = irBR[k];
                            if (irMod > 0.0f) {
                                c.mdL.push (oL); c.mdR.push (oR);
                                oL = c.mdL.read (1.0f + irMod * (1.0f + c.mlfo[0].next())); oR = c.mdR.read (1.0f + irMod * (1.0f + c.mlfo[1].next()));
                            }
                            if (irDiff > 0.0f)
                                for (int j = 0; j < 2; ++j) {
                                    oL = allpass (c.dfL[j], std::round (SlotChain::kDfMs[j] * 0.001f * fs), irDiff, oL);
                                    oR = allpass (c.dfR[j], std::round (SlotChain::kDfMs[j + 2] * 0.001f * fs), irDiff, oR);
                                }
                        }
                    } else {
                        float pl, pr; c.input (xL, xR, preS[s], inW[s], true, pl, pr);
                        if (s == Plate) plate.process (pl, pr, plateDampA, sp.mod, sp.early, oL, oR);
                        else tank[s].process (pl, pr, sp.mod, sp.early, oL, oR);
                    }
                    c.post (oL, oR, xL, xR, sp.comp);
                    wL += oL * c.sLvl; wR += oR * c.sLvl;
                }
                // ---- delay
                float dL = 0, dR = 0;
                if (p.delayOn || sDl > 1e-4f) {
                    float rL, rR;
                    delay.process (xL, xR, tL, tR, p.delayFb, p.pingPong, smA * 0.2f, rL, rR);
                    float oL = rL * pLl + rR * pRl, oR = rL * pLr + rR * pRr;
                    float mid = 0.5f * (oL + oR), side = 0.5f * (oL - oR) * width;
                    dL = mid + side; dR = mid - side;
                }
                // ---- mix
                float yL = xL * (1 - sRv) + wL * sRv + dL * sDl;
                float yR = xR * (1 - sRv) + wR * sRv + dR * sDl;
                if (p.trOn) { yL = tr.process (0, yL); yR = tr.process (1, yR); }
                bufL[k] = yL * sOut; bufR[k] = yR * sOut;
            }
            L[i] = dn[0].down (bufL); R[i] = dn[1].down (bufR);
        }
    }

private:
    float baseFs = 48000, fs = 48000; int os = 0; bool firstBlock = true;
    Resampler up[2], dn[2], irUp[2];
    PlateAlgo plate; TankAlgo tank[4];
    SlotChain chain[kNumSlots], irIn;
    StereoDelay delay; Transformer tr;
    float sRv = 0.3f, sDl = 0, sOut = 1;
    bool delayDirty = true;
};

} // namespace pv
// PlateVerb DSP engine - no JUCE dependency.
// Reverb algorithms (Plate, Room, Chamber, Hall, Church, Stadium), 4-band reverb EQ,
// reverb-tail compressor, tempo-syncable stereo delay with a gentle 3-band EQ,
// output transformer models and integer oversampling (1x, 2x, 3x, 4x, 6x).
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <array>
#include <cstdint>

namespace pv {

constexpr float kPi = 3.14159265358979f;
constexpr int kMaxOS = 6;
constexpr float kMaxSize = 1.5f;
constexpr float kMaxDelaySec = 3.0f;
constexpr int kNumAlgos = 6;
enum Algo { Plate = 0, Room, Chamber, Hall, Church, Stadium };

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float clampf (float x, float a, float b) { return std::min (b, std::max (a, x)); }

// ---------------------------------------------------------------- building blocks
struct Line {
    std::vector<float> b; int mask = 0, w = 0;
    void init (int maxLen) { int n = 1; while (n < maxLen + 8) n <<= 1; b.assign ((size_t) n, 0.0f); mask = n - 1; w = 0; }
    void clear() { std::fill (b.begin(), b.end(), 0.0f); w = 0; }
    inline void push (float x) { b[(size_t) w] = x; w = (w + 1) & mask; }
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
    // sample written d samples ago (d >= 1), linear interpolation
    inline float read (float d) const {
        float r = (float) w - d; float fl = std::floor (r); int i = (int) fl; float f = r - fl;
        float a = b[(size_t) (i & mask)], c = b[(size_t) ((i + 1) & mask)];
        return a + (c - a) * f;
    }
};

inline float allpass (Line& l, float d, float g, float x) {
    float z = l.read (d);
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
struct BandDecay {
    OnePole lo, hi; float gL = 1, gM = 1, gH = 1;
    void setX (float fs) { lo.setCut (fs, 300.0f); hi.setCut (fs, 4000.0f); }
    void setGains (float loopSec, float rtMid, float lowMul, float highMul) {
        auto g = [loopSec] (float rt) { return std::min (0.9995f, std::pow (10.0f, -3.0f * loopSec / std::max (0.05f, rt))); };
        gM = g (rtMid); gL = g (rtMid * lowMul); gH = g (rtMid * highMul);
    }
    void reset() { lo.reset(); hi.reset(); }
    inline float process (float x) { const float l = lo.lp (x), h = x - hi.lp (x); return l * gL + (x - l - h) * gM + h * gH; }
};

// ---------------------------------------------------------------- plate (Dattorro tank, true-stereo input)
struct PlateAlgo {
    static constexpr int kEarly = 5;
    Line d1, d2, d3, d4, e1, e2, e3, e4, apL1, dL1, apL2, dL2, apR1, dR1, apR2, dR2, ea[kEarly], eb[kEarly];
    BandDecay bdL, bdR;
    float fs = 48000, s = 1, lpL = 0, lpR = 0, tankL = 0, tankR = 0, phase = 0, eScale = 1;
    // early-onset diffusers; 'build' stretches them (3 - 55 ms -> x0.2 - x3.7) to put more energy up front
    static constexpr float kEaMs[kEarly] = { 0.93f, 1.71f, 3.07f, 5.3f, 8.9f }, kEbMs[kEarly] = { 1.13f, 1.97f, 2.71f, 6.1f, 10.3f };
    static constexpr float kMaxEScale = 55.0f / 15.0f;
    void init (float maxFs) {
        float sm = maxFs / 29761.0f * kMaxSize;
        auto sz = [sm] (float n) { return (int) std::ceil (n * sm) + 64; };
        d1.init (sz (142)); d2.init (sz (107)); d3.init (sz (379)); d4.init (sz (277));
        e1.init (sz (153)); e2.init (sz (115)); e3.init (sz (406)); e4.init (sz (297));
        apL1.init (sz (704)); dL1.init (sz (4453)); apL2.init (sz (1800)); dL2.init (sz (3720));
        apR1.init (sz (940)); dR1.init (sz (4217)); apR2.init (sz (2656)); dR2.init (sz (3163));
        for (int i = 0; i < kEarly; ++i) { ea[i].init ((int) (kEaMs[i] * kMaxEScale * 0.001f * maxFs) + 16); eb[i].init ((int) (kEbMs[i] * kMaxEScale * 0.001f * maxFs) + 16); }
    }
    void clear() {
        const float sm = fs / 29761.0f * kMaxSize;
        auto sz = [sm] (float n) { return (int) std::ceil (n * sm) + 64; };
        d1.clearRecent (sz (142)); d2.clearRecent (sz (107)); d3.clearRecent (sz (379)); d4.clearRecent (sz (277));
        e1.clearRecent (sz (153)); e2.clearRecent (sz (115)); e3.clearRecent (sz (406)); e4.clearRecent (sz (297));
        apL1.clearRecent (sz (704)); dL1.clearRecent (sz (4453)); apL2.clearRecent (sz (1800)); dL2.clearRecent (sz (3720));
        apR1.clearRecent (sz (940)); dR1.clearRecent (sz (4217)); apR2.clearRecent (sz (2656)); dR2.clearRecent (sz (3163));
        for (int i = 0; i < kEarly; ++i) { ea[i].clearRecent ((int) (kEaMs[i] * kMaxEScale * 0.001f * fs) + 16); eb[i].clearRecent ((int) (kEbMs[i] * kMaxEScale * 0.001f * fs) + 16); }
        bdL.reset(); bdR.reset();
        lpL = lpR = tankL = tankR = phase = 0;
    }
    void update (float rt, float size, float lowMul, float highMul, float buildMs) {
        s = fs / 29761.0f * size;
        eScale = clampf (buildMs / 15.0f, 0.2f, kMaxEScale);
        const float halfLoop = 10645.0f * s / fs;        // seconds around one half of the tank
        bdL.setX (fs); bdR.setX (fs);
        bdL.setGains (halfLoop, rt, lowMul, highMul); bdR.setGains (halfLoop, rt, lowMul, highMul);
    }
    // inL/inR: separate inputs (true stereo); early: level of the fast, dense onset
    inline void process (float inL, float inR, float dampA, float mod, float early, float& oL, float& oR) {
        const float exc = mod * 16.0f * s;
        float xL = allpass (d1, 142 * s, 0.75f, inL);  xL = allpass (d2, 107 * s, 0.75f, xL);
        xL = allpass (d3, 379 * s, 0.625f, xL); xL = allpass (d4, 277 * s, 0.625f, xL);
        float xR = allpass (e1, 153 * s, 0.75f, inR);  xR = allpass (e2, 115 * s, 0.75f, xR);
        xR = allpass (e3, 406 * s, 0.625f, xR); xR = allpass (e4, 297 * s, 0.625f, xR);
        phase += 2 * kPi * 0.9f / fs; if (phase > 2 * kPi) phase -= 2 * kPi;
        const float m1 = std::sin (phase), m2 = std::cos (phase);
        float a = allpass (apL1, 672 * s + exc * m1 + exc, -0.7f, xR + tankR);
        float t1 = dL1.read (4453 * s); dL1.push (a); lpL += dampA * (t1 - lpL);
        float b = allpass (apL2, 1800 * s, 0.5f, bdL.process (lpL));
        float t2 = dL2.read (3720 * s); dL2.push (b);
        float c = allpass (apR1, 908 * s + exc * m2 + exc, -0.7f, xL + tankL);
        float t3 = dR1.read (4217 * s); dR1.push (c); lpR += dampA * (t3 - lpR);
        float e = allpass (apR2, 2656 * s, 0.5f, bdR.process (lpR));
        float t4 = dR2.read (3163 * s); dR2.push (e);
        tankL = t2; tankR = t4;
        auto T = [this] (const Line& l, float n) { return l.read (n * s); };
        oL = 0.6f * (T (dR1, 266) + T (dR1, 2974) - T (apR2, 1913) + T (dR2, 1996) - T (dL1, 1990) - T (apL2, 187) - T (dL2, 1066));
        oR = 0.6f * (T (dL1, 353) + T (dL1, 3627) - T (apL2, 1228) + T (dL2, 2673) - T (dR1, 2111) - T (apR2, 335) - T (dR2, 121));
        if (early > 0.0f) {                              // dense onset within ~1-5 ms, like a real plate's attack
            float qL = inL, qR = inR;
            for (int i = 0; i < kEarly; ++i) { qL = allpass (ea[i], kEaMs[i] * eScale * 0.001f * fs, 0.68f, qL); qR = allpass (eb[i], kEbMs[i] * eScale * 0.001f * fs, 0.68f, qR); }
            oL += early * 1.2f * qL; oR += early * 1.2f * qR;
        }
    }
};

// ---------------------------------------------------------------- room/chamber/hall/church/stadium (FDN)
struct FdnConfig {
    float size, rtMul, diff, dampMul, erLevel, lateLevel, erStartMs, erEndMs, modMs;
    int erTaps; float slapMs1, slapMs2, slapGain; bool denseER;
};

inline const FdnConfig& fdnConfig (int algo) {
    static const FdnConfig c[kNumAlgos] = {
        { 1.0f, 1.0f, 0.70f, 1.00f, 0.0f, 1.0f, 0, 0, 0, 0, 0, 0, 0, false },                  // plate (unused)
        { 0.42f, 0.45f, 0.72f, 0.95f, 0.85f, 0.85f, 2.7f, 34.0f, 0.25f, 14, 0, 0, 0, false },     // room
        { 0.62f, 1.00f, 0.85f, 0.70f, 0.90f, 1.00f, 1.0f, 60.0f, 0.30f, 0, 0, 0, 0, true },       // chamber: dense 480L-style ER burst
        { 1.15f, 1.00f, 0.75f, 1.00f, 0.45f, 1.00f, 11.0f, 96.0f, 0.45f, 12, 0, 0, 0, false },    // hall
        { 1.70f, 1.90f, 0.70f, 1.25f, 0.35f, 1.00f, 21.0f, 165.0f, 0.55f, 12, 0, 0, 0, false },   // church
        { 2.50f, 1.40f, 0.50f, 1.45f, 0.50f, 0.90f, 38.0f, 330.0f, 0.65f, 10, 185.0f, 270.0f, 0.55f, false } // stadium
    };
    return c[algo];
}

struct FdnAlgo {
    static constexpr int L = 8;
    Line lines[L], difL[4], difR[4], er;
    OnePole damp[L];
    BandDecay band[L];
    static constexpr int kDense = 128;
    float erNormDense = 1.0f;
    float jit[kDense] = {}, sgnL[kDense] = {}, sgnR[kDense] = {};
    float lenMs[L] = { 31.3f, 37.9f, 41.3f, 47.1f, 53.9f, 61.7f, 67.3f, 79.9f };
    float difMs[4] = { 4.77f, 3.59f, 12.73f, 9.30f };
    std::vector<float> tapMs, tapGL, tapGR;
    float fs = 48000, phase = 0, hpL = 0, hpR = 0;
    int algo = 1;

    void init (int a, float maxFs) {
        algo = a;
        const FdnConfig& c = fdnConfig (a);
        const float k = maxFs * 0.001f * c.size * kMaxSize;
        for (int i = 0; i < L; ++i) lines[i].init ((int) (lenMs[i] * k + c.modMs * 2 * maxFs * 0.001f) + 16);
        for (int i = 0; i < 4; ++i) { difL[i].init ((int) (difMs[i] * 1.07f * k) + 16); difR[i].init ((int) (difMs[i] * 1.07f * k) + 16); }
        // early reflection pattern (deterministic pseudo-random)
        tapMs.clear(); tapGL.clear(); tapGR.clear();
        uint32_t seed = 12345u + (uint32_t) a * 7919u;
        auto rnd = [&seed] { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) / 16777216.0f; };
        for (int t = 0; t < c.erTaps; ++t) {
            float f = (float) t / (float) std::max (1, c.erTaps - 1);
            float ms = c.erStartMs * std::pow (c.erEndMs / c.erStartMs, f) * (0.92f + 0.16f * rnd());
            float g = (1.0f - 0.7f * f) * (0.75f + 0.25f * rnd()) * ((t % 3 == 2) ? -1.0f : 1.0f);
            float pan = (t % 2 == 0) ? 0.25f + 0.2f * rnd() : 0.75f - 0.2f * rnd();
            tapMs.push_back (ms); tapGL.push_back (g * std::sqrt (1 - pan)); tapGR.push_back (g * std::sqrt (pan));
        }
        if (c.slapGain > 0) {
            tapMs.push_back (c.slapMs1); tapGL.push_back (c.slapGain); tapGR.push_back (c.slapGain * 0.5f);
            tapMs.push_back (c.slapMs2); tapGL.push_back (c.slapGain * 0.45f); tapGR.push_back (c.slapGain * 0.8f);
        }
        if (c.denseER) {
            // mostly positive reflections keep the low end; independent L/R patterns keep the stereo wide
            for (int k = 0; k < kDense; ++k) { jit[k] = rnd() * 2.0f - 1.0f; sgnL[k] = rnd() < 0.25f ? -0.6f : 1.0f; sgnR[k] = rnd() < 0.25f ? -0.6f : 1.0f; }
            tapMs.assign (kDense, 0.0f); tapGL.assign (kDense, 0.0f); tapGR.assign (kDense, 0.0f);
        }
        maxTap = 0; for (float m : tapMs) maxTap = std::max (maxTap, m);
        if (c.denseER) maxTap = 250.0f / kMaxSize;
        er.init ((int) (maxTap * kMaxSize * maxFs * 0.001f) + 16);
    }
    float maxTap = 0;
    void clear() {                      // only the span readable at the current rate
        const FdnConfig& c = fdnConfig (algo);
        const float k = fs * 0.001f * c.size * kMaxSize;
        for (int i = 0; i < L; ++i) lines[i].clearRecent ((int) (lenMs[i] * k + c.modMs * 2 * fs * 0.001f) + 16);
        for (int i = 0; i < 4; ++i) { difL[i].clearRecent ((int) (difMs[i] * 1.07f * k) + 16); difR[i].clearRecent ((int) (difMs[i] * 1.07f * k) + 16); }
        er.clearRecent ((int) (maxTap * kMaxSize * fs * 0.001f) + 16);
        for (auto& d : damp) d.reset();
        for (auto& bd : band) bd.reset();
        phase = hpL = hpR = 0;
    }

    float k = 1, kEr = 1, gain[L] = {}, len[L] = {};
    void update (float rt, float dampHz, float size, float lowMul, float highMul, float buildMs) {
        const FdnConfig& c = fdnConfig (algo);
        k = fs * 0.001f * c.size * size;
        kEr = fs * 0.001f * (c.denseER ? 1.0f : size);
        const float rtA = std::max (0.05f, rt * c.rtMul);
        for (int i = 0; i < L; ++i) {
            len[i] = lenMs[i] * k;
            gain[i] = 1.0f;
            damp[i].setCut (fs, dampHz / c.dampMul);
            band[i].setX (fs);
            band[i].setGains (len[i] / fs, rtA, lowMul, highMul);
        }
        if (c.denseER) {           // reflections every ~1.8 ms for 'build' ms (480L-style spread): quick rise, gentle sag, short fade
            const float len = clampf (buildMs, 3.0f, 230.0f), att = std::min (12.0f, len * 0.3f), spacing = 1.8f * (0.8f + 0.2f * size);
            float sumSq = 0.0f;
            for (int t = 0; t < kDense; ++t) {
                float ms = 0.3f + t * spacing * (1.0f + 0.15f * jit[t]);
                float env;
                if (ms < att) env = ms / att;
                else if (ms < len) env = 1.0f - 0.4f * (ms - att) / std::max (1.0f, len - att);
                else env = std::max (0.0f, 0.6f * (1.0f - (ms - len) / 4.0f));
                if (ms > 240.0f) env = 0.0f;
                tapMs[(size_t) t] = std::min (ms, 240.0f); tapGL[(size_t) t] = env * sgnL[t]; tapGR[(size_t) t] = env * sgnR[t];
                sumSq += env * env;
            }
            erNormDense = 1.0f / std::sqrt (std::max (1.0f, sumSq));
        }
    }
    inline void process (float inL, float inR, float mod, float erAmt, float& oL, float& oR) {
        const FdnConfig& c = fdnConfig (algo);
        // early reflections
        er.push (0.5f * (inL + inR));
        float eL = 0, eR = 0;
        for (size_t t = 0; t < tapMs.size(); ++t) { float v = er.read (std::max (1.0f, tapMs[t] * kEr)); eL += v * tapGL[t]; eR += v * tapGR[t]; }
        const float erNorm = c.denseER ? erNormDense * 1.4f : 1.0f / std::sqrt ((float) tapMs.size());
        eL *= erNorm * 1.6f; eR *= erNorm * 1.6f;
        // input diffusion
        // dense-ER (chamber): the tail is fed mostly by the reflection burst, so 'build' also shapes the bloom
        float xL = c.denseER ? 0.3f * inL + 0.9f * eL : inL + 0.35f * eL;
        float xR = c.denseER ? 0.3f * inR + 0.9f * eR : inR + 0.35f * eR;
        const float g1 = 0.75f * c.diff, g2 = 0.62f * c.diff;
        xL = allpass (difL[0], difMs[0] * k, g1, xL); xL = allpass (difL[1], difMs[1] * k, g1, xL);
        xL = allpass (difL[2], difMs[2] * k, g2, xL); xL = allpass (difL[3], difMs[3] * k, g2, xL);
        xR = allpass (difR[0], difMs[0] * 1.07f * k, g1, xR); xR = allpass (difR[1], difMs[1] * 1.07f * k, g1, xR);
        xR = allpass (difR[2], difMs[2] * 1.07f * k, g2, xR); xR = allpass (difR[3], difMs[3] * 1.07f * k, g2, xR);
        // feedback delay network
        phase += 2 * kPi * 0.63f / fs; if (phase > 2 * kPi) phase -= 2 * kPi;
        const float md = mod * c.modMs * fs * 0.001f;
        float y[L]; float sum = 0;
        for (int i = 0; i < L; ++i) {
            float m = (i == 1) ? md * (1 + std::sin (phase)) : (i == 6) ? md * (1 + std::cos (phase * 1.31f)) : 0.0f;
            float v = lines[i].read (len[i] + m + 1.0f);
            v = band[i].process (damp[i].lp (v));
            y[i] = v; sum += v;
        }
        sum *= 2.0f / L;
        for (int i = 0; i < L; ++i) lines[i].push (y[i] - sum + ((i & 1) ? xR : xL));
        float lL = (y[0] - y[2] + y[4] - y[6] + 0.7f * (y[1] - y[5])) * 0.42f;
        float lR = (y[1] - y[3] + y[5] - y[7] + 0.7f * (y[2] - y[6])) * 0.42f;
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

struct Compressor {
    // Time settings are "time to complete": attack = time to reach full gain reduction,
    // release = time for gain reduction to return to 0 dB (exponential, ~99.75 % at that time, then snapped to 0).
    // FET follows 1176 specs: attack 20 us - 800 us, release 50 ms - 1.1 s.
    // Opto follows LA-2A behaviour: fixed ~10 ms attack, about half the release in 60 ms,
    // the rest over 1.5 - 15 s depending on how long and hard it has been compressing.
    static constexpr float kFetAtkMin = 0.02f, kFetAtkMax = 0.8f, kFetRelMin = 50.0f, kFetRelMax = 1100.0f;   // ms
    float fs = 48000, aM = 0, inPk = 0;
    float aA = 0, aR = 0, aFA = 0, aFR = 0, aOA = 0, aOFast = 0, aOSlow1 = 0, aOSlow2 = 0, aMem = 0, aRms = 0;
    float envV = 0, envF = 0, envOf = 0, envOs = 0, mem = 0, rms = 0, grTotal = 0;
    Biquad tubeLp[2], tubeLow[2], dc[2];
    void reset() { envV = envF = envOf = envOs = mem = rms = grTotal = inPk = 0; for (int c = 0; c < 2; ++c) { tubeLp[c].reset(); tubeLow[c].reset(); dc[c].reset(); } }
    static float tc (float tauMs, float fs) { return std::exp (-1.0f / (std::max (0.001f, tauMs) * 0.001f * fs)); }
    static float done (float ms, float fs) { return tc (ms / 6.0f, fs); }        // reaches ~99.75 % in 'ms'
    void setTimes (float atkMs, float relMs) {
        aA = done (atkMs, fs); aR = done (relMs, fs);
        aFA = done (clampf (atkMs, kFetAtkMin, kFetAtkMax), fs);
        aFR = done (clampf (relMs, kFetRelMin, kFetRelMax), fs);
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
        return (target <= 0.0f && env < 0.05f) ? 0.0f : env;
    }
    inline float stepOpto (float db, float thr, float ratio) {
        float gr = gainComputer (db, thr, ratio, 12.0f);
        mem = aMem * mem + (1 - aMem) * clampf (gr / 10.0f, 0.0f, 1.0f);   // longer/harder compression = slower tail
        const float aSlow = aOSlow1 + (aOSlow2 - aOSlow1) * mem;
        envOf = follow (envOf, gr, aOA, aOFast);
        envOs = follow (envOs, gr, aOA, aSlow);
        return 0.6f * envOf + 0.4f * envOs;
    }
    // kL/kR = key (detector) signal: the reverb itself, or the dry input when ducking.
    inline void process (float& L, float& R, float kL, float kR, bool keyIsDry, int type, bool vintage, float thr, float ratio, float makeup, float mix) {
        const float lvl = meter (kL, kR);
        const float pkDb = 20.0f * std::log10 (lvl + 1e-9f);
        float yL = L, yR = R, gr = 0;
        if (type == CompVCA) {
            envV = follow (envV, gainComputer (pkDb, thr, ratio, 6.0f), aA, aR);
            gr = envV;
            float k = dbToGain (-gr); yL *= k; yR *= k;
            if (vintage) { yL = warmSat (yL, 1.5f, 0.05f); yR = warmSat (yR, 1.5f, 0.05f); }
        }
        if (type == CompFET || type == CompFetOpto) {
            const float t = type == CompFetOpto ? thr + 6.0f : thr;      // in the chain the FET only catches peaks
            envF = follow (envF, gainComputer (pkDb, t, ratio, 2.0f), aFA, aFR);
            gr += envF;
            float k = dbToGain (-envF); yL *= k; yR *= k;
            if (vintage) { float d = 1.2f + 0.12f * envF; yL = warmSat (yL, d, 0.12f); yR = warmSat (yR, d, 0.12f); }
        }
        if (type == CompOpto || type == CompFetOpto) {
            float l2 = keyIsDry ? lvl : std::max (std::fabs (yL), std::fabs (yR));
            rms = aRms * rms + (1 - aRms) * l2 * l2;                    // opto cell reacts to average level
            const float db = 10.0f * std::log10 (rms + 1e-12f) + 3.0f;
            float g = stepOpto (db, thr, type == CompFetOpto ? 3.0f : ratio);
            gr += g;
            float k = dbToGain (-g); yL *= k; yR *= k;
            if (vintage) {
                yL = tubeLp[0].process (tubeLow[0].process (warmSat (yL, 1.8f, 0.25f)));
                yR = tubeLp[1].process (tubeLow[1].process (warmSat (yR, 1.8f, 0.25f)));
            }
        }
        if (vintage) { yL = dc[0].process (yL); yR = dc[1].process (yR); }
        grTotal = gr;
        const float mk = dbToGain (makeup);
        L = L * (1 - mix) + yL * mk * mix;
        R = R * (1 - mix) + yR * mk * mix;
    }
    float reductionDb() const { return grTotal; }
    float inputDb() const { return 20.0f * std::log10 (inPk + 1e-9f); }
};

// ---------------------------------------------------------------- stereo delay with gentle analog EQ
struct StereoDelay {
    Line l, r; float curL = 1, curR = 1, fs = 48000, fbStateL = 0, fbStateR = 0;
    Biquad lo[2], mi[2], hi[2], hp[2], lp[2];
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
    }
    void service() {
        if (pendCount <= 0) return;
        const int c = std::min (pendCount, 65536);
        l.clearRange (pendStart, c); r.clearRange (pendStart, c);
        pendStart = (pendStart + c) & l.mask; pendCount -= c;
    }
    void clear() { const int n = (int) (kMaxDelaySec * fs) + 16; l.clearRecent (n); r.clearRecent (n); for (int c = 0; c < 2; ++c) { lo[c].reset(); mi[c].reset(); hi[c].reset(); hp[c].reset(); lp[c].reset(); } fbStateL = fbStateR = 0; }
    void setEq (float lowDb, float midDb, float midHz, float highDb) {
        for (int c = 0; c < 2; ++c) {
            lo[c].lowShelf (fs, 140.0f, lowDb, 0.5f);
            mi[c].peak (fs, midHz, 0.6f, midDb);
            hi[c].highShelf (fs, 5500.0f, highDb, 0.5f);
            hp[c].highpass (fs, 30.0f, 0.6f);
            lp[c].lowpass (fs, 16000.0f, 0.6f);
        }
    }
    static inline float soft (float x) { return std::tanh (x * 0.8f) / 0.8f; }   // very mild, only near clipping
    inline void process (float inL, float inR, float tL, float tR, float fb, bool ping, float smoothA, float& oL, float& oR) {
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
        drive = dbToGain (2.0f + amount * 14.0f * m.sat);     // saturation ceiling from about -2 dBFS down to -16 dBFS
        bias = m.asym * (0.5f + 1.5f * amount);              // asymmetry = even harmonics (warmth)
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

// ---------------------------------------------------------------- parameters & engine
struct Params {
    int oversampling = 1;                  // 1,2,3,4,6
    bool algoOn[kNumAlgos] = { true, false, false, false, false, false };
    float algoLevel[kNumAlgos] = { 1, 1, 1, 1, 1, 1 };
    float lowMult = 1.0f, highMult = 1.0f, buildMs = 15.0f, inWidth = 1.0f;   // inWidth: 0 = mono input, 1 = true stereo
    float decaySec = 2.5f, size = 1.0f, predelayMs = 10, damping = 0.35f, tone = 0.75f, mod = 0.5f, early = 0.7f;
    float rvMix = 0.3f;
    float eqF[4] = { 100, 400, 2500, 8000 }, eqG[4] = { 0, 0, 0, 0 };
    bool compOn = false; bool compDuck = false; int compType = 0; bool compVintage = false; float compThr = -30, compRatio = 4, compAtk = 10, compRel = 200, compMakeup = 0, compMix = 1;
    bool delayOn = false; float delayMsL = 375, delayMsR = 500, delayFb = 0.35f, delayMix = 0.0f;
    bool pingPong = false; float dlPanL = -1, dlPanR = 1, dlWidth = 1; float dlLowDb = 0, dlMidDb = 0, dlMidHz = 1000, dlHighDb = 0;
    bool trOn = false; int trModel = 0; float trFreq = 80, trAmount = 0.3f;
    float outGainDb = 0;
};

class Engine {
public:
    void prepare (double sampleRate) {
        baseFs = (float) sampleRate;
        const float maxFs = baseFs * kMaxOS;
        plate.init (maxFs);
        for (int a = 1; a < kNumAlgos; ++a) fdn[a].init (a, maxFs);
        preL.init ((int) (0.26f * maxFs) + 16); preR.init ((int) (0.26f * maxFs) + 16);
        delay.init (maxFs);
        for (int c = 0; c < 2; ++c) { up[c].reserve (kMaxOS); dn[c].reserve (kMaxOS); }
        setOversampling (1, true);
    }
    void setOversampling (int n, bool force = false) {
        if (n != 1 && n != 2 && n != 3 && n != 4 && n != 6) n = 1;
        if (n == os && ! force) return;
        os = n; fs = baseFs * (float) os;
        for (int c = 0; c < 2; ++c) { up[c].design (os); dn[c].design (os); }
        plate.fs = fs;
        for (int a = 1; a < kNumAlgos; ++a) fdn[a].fs = fs;
        for (int a = 0; a < kNumAlgos; ++a) algoDirty[a] = true;         // cleared when next used, not all at once
        preL.clearRecent ((int) (0.26f * fs) + 16); preR.clearRecent ((int) (0.26f * fs) + 16); tone[0].reset(); tone[1].reset();
        for (int c = 0; c < 2; ++c) for (int b = 0; b < 4; ++b) eq[c][b].reset();
        comp.fs = fs; comp.reset();
        delay.fs = fs; delayDirty = true; delay.curL = delay.curR = 0; firstBlock = true;
        tr.fs = fs; tr.clear();
    }
    int latency() const { return up[0].latencyBase(); }
    float compReductionDb() const { return comp.reductionDb(); }
    float compInputDb() const { return comp.inputDb(); }

    void process (float* L, float* R, int n, const Params& p) {
        setOversampling (p.oversampling);
        // per-block coefficient updates at the oversampled rate
        for (int c = 0; c < 2; ++c) {
            eq[c][0].lowShelf (fs, p.eqF[0], p.eqG[0]);
            eq[c][1].peak (fs, p.eqF[1], 0.9f, p.eqG[1]);
            eq[c][2].peak (fs, p.eqF[2], 0.9f, p.eqG[2]);
            eq[c][3].highShelf (fs, p.eqF[3], p.eqG[3]);
            tone[c].setCut (fs, 400.0f * std::pow (50.0f, p.tone));
        }
        delay.setEq (p.dlLowDb, p.dlMidDb, p.dlMidHz, p.dlHighDb);
        tr.set (p.trModel, p.trFreq, p.trAmount);
        comp.setTimes (p.compAtk, p.compRel);
        const float dampHz = 20000.0f * std::pow (0.05f, p.damping);
        const float plateDampA = 1.0f - std::exp (-2.0f * kPi * std::min (dampHz, 0.45f * fs) / fs);
        plate.update (p.decaySec, p.size, p.lowMult, p.highMult, p.buildMs);
        for (int a = 1; a < kNumAlgos; ++a) fdn[a].update (p.decaySec, dampHz, p.size, p.lowMult, p.highMult, p.buildMs);
        const float preS = std::max (1.0f, p.predelayMs * 0.001f * fs);
        const float tL = clampf (p.delayMsL * 0.001f * fs, 1.0f, kMaxDelaySec * fs - 4);
        const float tR = clampf (p.delayMsR * 0.001f * fs, 1.0f, kMaxDelaySec * fs - 4);
        if (firstBlock) { delay.curL = tL; delay.curR = tR; firstBlock = false; }
        if (delayDirty && (p.delayOn || sDl > 1e-4f)) { delay.clearSmart (std::max (tL, tR)); delayDirty = false; }
        delay.service();
        const float smA = 1.0f - std::exp (-1.0f / (0.03f * fs));      // ~30 ms parameter smoothing
        // delay pan (equal power) for each delay channel
        auto panGains = [] (float pan, float& gl, float& gr) { float a = (clampf (pan, -1, 1) + 1) * 0.25f * kPi; gl = std::cos (a); gr = std::sin (a); };
        float pLl, pLr, pRl, pRr; panGains (p.dlPanL, pLl, pLr); panGains (p.dlPanR, pRl, pRr);
        const float width = clampf (p.dlWidth, 0.0f, 2.0f);
        const float inW = clampf (p.inWidth, 0.0f, 1.0f);
        const float tgtRv = p.rvMix, tgtDl = p.delayOn ? p.delayMix : 0.0f, tgtOut = dbToGain (p.outGainDb);

        int nOn = 0; for (int a = 0; a < kNumAlgos; ++a) nOn += p.algoOn[a] ? 1 : 0;
        const float algoNorm = nOn > 0 ? 1.0f / std::sqrt ((float) nOn) : 0.0f;
        float tgtAlgo[kNumAlgos];
        for (int a = 0; a < kNumAlgos; ++a) tgtAlgo[a] = p.algoOn[a] ? p.algoLevel[a] * algoNorm : 0.0f;

        float bufL[kMaxOS], bufR[kMaxOS];
        for (int i = 0; i < n; ++i) {
            up[0].up (L[i], bufL); up[1].up (R[i], bufR);
            for (int k = 0; k < os; ++k) {
                float xL = bufL[k], xR = bufR[k];
                sRv += smA * (tgtRv - sRv); sDl += smA * (tgtDl - sDl); sOut += smA * (tgtOut - sOut);
                // ---- reverb
                float pl = preL.read (preS), pr = preR.read (preS);
                preL.push (xL); preR.push (xR);
                pl = tone[0].lp (pl); pr = tone[1].lp (pr);
                { const float mid = 0.5f * (pl + pr), side = 0.5f * (pl - pr) * inW; pl = mid + side; pr = mid - side; }
                float wL = 0, wR = 0;
                for (int a = 0; a < kNumAlgos; ++a) {
                    sAlgo[a] += smA * (tgtAlgo[a] - sAlgo[a]);
                    if (! p.algoOn[a] && sAlgo[a] < 1e-4f) {
                        if (running[a]) { running[a] = false; algoDirty[a] = true; }   // cleared when switched back on
                        continue;
                    }
                    if (algoDirty[a]) { if (a == Plate) plate.clear(); else fdn[a].clear(); algoDirty[a] = false; }
                    running[a] = true;
                    float oL, oR;
                    if (a == Plate) plate.process (pl, pr, plateDampA, p.mod, p.early, oL, oR);
                    else fdn[a].process (pl, pr, p.mod, p.early, oL, oR);
                    wL += oL * sAlgo[a]; wR += oR * sAlgo[a];
                }
                for (int b = 0; b < 4; ++b) { wL = eq[0][b].process (wL); wR = eq[1][b].process (wR); }
                const float kL = p.compDuck ? xL : wL, kR = p.compDuck ? xR : wR;     // ducking listens to the dry input
                if (p.compOn) comp.process (wL, wR, kL, kR, p.compDuck, p.compType, p.compVintage, p.compThr, p.compRatio, p.compMakeup, p.compMix);
                else comp.meter (kL, kR);
                // ---- delay
                float dL = 0, dR = 0;
                if (p.delayOn || sDl > 1e-4f) {
                    float rL, rR;
                    delay.process (xL, xR, tL, tR, p.delayFb, p.pingPong, smA * 0.2f, rL, rR);
                    float oL = rL * pLl + rR * pRl, oR = rL * pLr + rR * pRr;
                    float mid = 0.5f * (oL + oR), side = 0.5f * (oL - oR) * width;
                    dL = mid + side; dR = mid - side;
                }
                // ---- mix
                float yL = xL * (1 - sRv) + wL * sRv + dL * sDl;
                float yR = xR * (1 - sRv) + wR * sRv + dR * sDl;
                if (p.trOn) { yL = tr.process (0, yL); yR = tr.process (1, yR); }
                bufL[k] = yL * sOut; bufR[k] = yR * sOut;
            }
            L[i] = dn[0].down (bufL); R[i] = dn[1].down (bufR);
        }
    }

private:
    float baseFs = 48000, fs = 48000; int os = 0; bool firstBlock = true;
    Resampler up[2], dn[2];
    PlateAlgo plate; FdnAlgo fdn[kNumAlgos];
    Line preL, preR; OnePole tone[2];
    Biquad eq[2][4]; Compressor comp; StereoDelay delay; Transformer tr;
    float sRv = 0.3f, sDl = 0, sOut = 1, sAlgo[kNumAlgos] = { 0, 0, 0, 0, 0, 0 };
    bool running[kNumAlgos] = {}, algoDirty[kNumAlgos] = {}, delayDirty = true;
};

} // namespace pv
