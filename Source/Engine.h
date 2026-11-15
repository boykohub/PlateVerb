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

// ---------------------------------------------------------------- plate (Dattorro)
struct PlateAlgo {
    Line d1, d2, d3, d4, apL1, dL1, apL2, dL2, apR1, dR1, apR2, dR2;
    float fs = 48000, s = 1, lpL = 0, lpR = 0, tankL = 0, tankR = 0, phase = 0;
    void init (float maxFs) {
        float sm = maxFs / 29761.0f * kMaxSize;
        auto sz = [sm] (float n) { return (int) std::ceil (n * sm) + 64; };
        d1.init (sz (142)); d2.init (sz (107)); d3.init (sz (379)); d4.init (sz (277));
        apL1.init (sz (704)); dL1.init (sz (4453)); apL2.init (sz (1800)); dL2.init (sz (3720));
        apR1.init (sz (940)); dR1.init (sz (4217)); apR2.init (sz (2656)); dR2.init (sz (3163));
    }
    void clear() {
        for (Line* l : { &d1, &d2, &d3, &d4, &apL1, &dL1, &apL2, &dL2, &apR1, &dR1, &apR2, &dR2 }) l->clear();
        lpL = lpR = tankL = tankR = phase = 0;
    }
    // in: mono input; rt: seconds; damp: one-pole coefficient (1 = bright)
    float decay = 0.5f;
    void update (float rt, float size) {
        s = fs / 29761.0f * size;
        const float halfLoop = 0.363f * size;           // seconds per tank half
        decay = std::min (0.995f, std::pow (10.0f, -1.5f * halfLoop / std::max (0.05f, rt)));
    }
    inline void process (float in, float dampA, float mod, float& oL, float& oR) {
        const float exc = mod * 16.0f * s;
        float x = in;
        x = allpass (d1, 142 * s, 0.75f, x);  x = allpass (d2, 107 * s, 0.75f, x);
        x = allpass (d3, 379 * s, 0.625f, x); x = allpass (d4, 277 * s, 0.625f, x);
        phase += 2 * kPi * 0.9f / fs; if (phase > 2 * kPi) phase -= 2 * kPi;
        const float m1 = std::sin (phase), m2 = std::cos (phase);
        float a = allpass (apL1, 672 * s + exc * m1 + exc, -0.7f, x + decay * tankR);
        float t1 = dL1.read (4453 * s); dL1.push (a); lpL += dampA * (t1 - lpL);
        float b = allpass (apL2, 1800 * s, 0.5f, lpL * decay);
        float t2 = dL2.read (3720 * s); dL2.push (b);
        float c = allpass (apR1, 908 * s + exc * m2 + exc, -0.7f, x + decay * tankL);
        float t3 = dR1.read (4217 * s); dR1.push (c); lpR += dampA * (t3 - lpR);
        float e = allpass (apR2, 2656 * s, 0.5f, lpR * decay);
        float t4 = dR2.read (3163 * s); dR2.push (e);
        tankL = t2; tankR = t4;
        auto T = [this] (const Line& l, float n) { return l.read (n * s); };
        oL = 0.6f * (T (dR1, 266) + T (dR1, 2974) - T (apR2, 1913) + T (dR2, 1996) - T (dL1, 1990) - T (apL2, 187) - T (dL2, 1066));
        oR = 0.6f * (T (dL1, 353) + T (dL1, 3627) - T (apL2, 1228) + T (dL2, 2673) - T (dR1, 2111) - T (apR2, 335) - T (dR2, 121));
    }
};

// ---------------------------------------------------------------- room/chamber/hall/church/stadium (FDN)
struct FdnConfig {
    float size, rtMul, diff, dampMul, erLevel, lateLevel, erStartMs, erEndMs, modMs;
    int erTaps; float slapMs1, slapMs2, slapGain;
};

inline const FdnConfig& fdnConfig (int algo) {
    static const FdnConfig c[kNumAlgos] = {
        { 1.0f, 1.0f, 0.70f, 1.00f, 0.0f, 1.0f, 0, 0, 0, 0, 0, 0, 0 },                  // plate (unused)
        { 0.42f, 0.45f, 0.72f, 0.95f, 0.85f, 0.85f, 2.7f, 34.0f, 0.25f, 14, 0, 0, 0 },     // room
        { 0.62f, 0.70f, 0.80f, 0.70f, 0.60f, 1.00f, 4.5f, 58.0f, 0.30f, 14, 0, 0, 0 },     // chamber
        { 1.15f, 1.00f, 0.75f, 1.00f, 0.45f, 1.00f, 11.0f, 96.0f, 0.45f, 12, 0, 0, 0 },    // hall
        { 1.70f, 1.90f, 0.70f, 1.25f, 0.35f, 1.00f, 21.0f, 165.0f, 0.55f, 12, 0, 0, 0 },   // church
        { 2.50f, 1.40f, 0.50f, 1.45f, 0.50f, 0.90f, 38.0f, 330.0f, 0.65f, 10, 185.0f, 270.0f, 0.55f } // stadium
    };
    return c[algo];
}

struct FdnAlgo {
    static constexpr int L = 8;
    Line lines[L], difL[4], difR[4], er;
    OnePole damp[L];
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
        float maxTap = 0; for (float m : tapMs) maxTap = std::max (maxTap, m);
        er.init ((int) (maxTap * kMaxSize * maxFs * 0.001f) + 16);
    }
    void clear() {
        for (auto& l : lines) l.clear();
        for (int i = 0; i < 4; ++i) { difL[i].clear(); difR[i].clear(); }
        er.clear(); for (auto& d : damp) d.reset(); phase = hpL = hpR = 0;
    }

    float k = 1, kEr = 1, gain[L] = {}, len[L] = {};
    void update (float rt, float dampHz, float size) {
        const FdnConfig& c = fdnConfig (algo);
        k = fs * 0.001f * c.size * size;
        kEr = fs * 0.001f * size;
        const float rtA = std::max (0.05f, rt * c.rtMul);
        for (int i = 0; i < L; ++i) {
            len[i] = lenMs[i] * k;
            gain[i] = std::pow (10.0f, -3.0f * (len[i] / fs) / rtA);
            damp[i].setCut (fs, dampHz / c.dampMul);
        }
    }
    inline void process (float inL, float inR, float mod, float erAmt, float& oL, float& oR) {
        const FdnConfig& c = fdnConfig (algo);
        // early reflections
        er.push (0.5f * (inL + inR));
        float eL = 0, eR = 0;
        for (size_t t = 0; t < tapMs.size(); ++t) { float v = er.read (std::max (1.0f, tapMs[t] * kEr)); eL += v * tapGL[t]; eR += v * tapGR[t]; }
        const float erNorm = 1.0f / std::sqrt ((float) tapMs.size());
        eL *= erNorm * 1.6f; eR *= erNorm * 1.6f;
        // input diffusion
        float xL = inL + 0.35f * eL, xR = inR + 0.35f * eR;
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
            v = damp[i].lp (v) * gain[i];
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
struct Compressor {
    float env = 0, fs = 48000, aA = 0, aR = 0;
    void setTimes (float atkMs, float relMs) {
        aA = std::exp (-1.0f / (std::max (0.05f, atkMs) * 0.001f * fs));
        aR = std::exp (-1.0f / (std::max (1.0f, relMs) * 0.001f * fs));
    }
    inline void process (float& L, float& R, float thr, float ratio, float makeup, float mix) {
        float lvl = std::max (std::fabs (L), std::fabs (R));
        float db = 20.0f * std::log10 (lvl + 1e-9f);
        float over = db - thr, knee = 6.0f, gr;
        if (over <= -knee / 2) gr = 0;
        else if (over >= knee / 2) gr = over * (1 - 1 / ratio);
        else { float t = over + knee / 2; gr = (1 - 1 / ratio) * t * t / (2 * knee); }
        env = gr > env ? aA * env + (1 - aA) * gr : aR * env + (1 - aR) * gr;
        float g = dbToGain (makeup - env);
        L = L * (1 - mix) + L * g * mix;
        R = R * (1 - mix) + R * g * mix;
    }
    float reductionDb() const { return env; }
};

// ---------------------------------------------------------------- stereo delay with gentle analog EQ
struct StereoDelay {
    Line l, r; float curL = 1, curR = 1, fs = 48000, fbStateL = 0, fbStateR = 0;
    Biquad lo[2], mi[2], hi[2], hp[2], lp[2];
    void init (float maxFs) { l.init ((int) (kMaxDelaySec * maxFs) + 16); r.init ((int) (kMaxDelaySec * maxFs) + 16); }
    void clear() { l.clear(); r.clear(); for (int c = 0; c < 2; ++c) { lo[c].reset(); mi[c].reset(); hi[c].reset(); hp[c].reset(); lp[c].reset(); } fbStateL = fbStateR = 0; }
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
struct TransformerModel { const char* name; float sat, asym, hfHz, hfQ, bumpHz, bumpDb, full; };
constexpr int kNumTransformers = 6;
inline const TransformerModel& transformerModel (int i) {
    static const TransformerModel m[kNumTransformers] = {
        { "Marinair-style (British console)",  1.00f, 0.12f, 32000, 0.80f,  70, 0.8f, 0.30f },
        { "Carnhill-style (British reissue)",  0.85f, 0.10f, 36000, 0.75f,  80, 0.6f, 0.25f },
        { "Jensen-style (US clean)",           0.35f, 0.03f, 60000, 0.71f,  40, 0.2f, 0.10f },
        { "UTC-style (US vintage)",            0.90f, 0.18f, 19000, 1.10f, 100, 1.0f, 0.35f },
        { "Lundahl-style (Scandinavian)",      0.50f, 0.06f, 50000, 0.70f,  50, 0.4f, 0.15f },
        { "Haufe-style (German broadcast)",    0.75f, 0.15f, 24000, 0.90f,  90, 0.7f, 0.30f }
    };
    return m[std::min (std::max (i, 0), kNumTransformers - 1)];
}

struct Transformer {
    float fs = 48000; OnePole split[2]; Biquad bump[2], hf[2], dc[2];
    void clear() { for (int c = 0; c < 2; ++c) { split[c].reset(); bump[c].reset(); hf[c].reset(); dc[c].reset(); } }
    void set (int model, float freq, float amount) {
        const auto& m = transformerModel (model);
        for (int c = 0; c < 2; ++c) {
            split[c].setCut (fs, freq);
            bump[c].peak (fs, m.bumpHz * (freq / 80.0f), 0.9f, m.bumpDb * (0.4f + amount));
            hf[c].lowpass (fs, std::min (m.hfHz, 0.45f * fs), m.hfQ);
            dc[c].highpass (fs, 8.0f, 0.7f);
        }
    }
    inline float process (int c, float x, int model, float amount) {
        const auto& m = transformerModel (model);
        float low = split[c].lp (x);
        float drive = 1.0f + amount * m.sat * 7.0f, bias = m.asym * amount;
        float sat = (std::tanh (drive * (low + bias)) - std::tanh (drive * bias)) / drive;
        float y = (x - low) + low + amount * (sat - low);
        float k = 1.0f + 2.0f * amount * m.full;
        y = y + amount * m.full * (std::tanh (y * k) / k - y);
        y = hf[c].process (bump[c].process (y));
        return dc[c].process (y);
    }
};

// ---------------------------------------------------------------- parameters & engine
struct Params {
    int oversampling = 1;                  // 1,2,3,4,6
    bool algoOn[kNumAlgos] = { true, false, false, false, false, false };
    float algoLevel[kNumAlgos] = { 1, 1, 1, 1, 1, 1 };
    float decaySec = 2.5f, size = 1.0f, predelayMs = 10, damping = 0.35f, tone = 0.75f, mod = 0.5f, early = 0.7f;
    float rvMix = 0.3f;
    float eqF[4] = { 100, 400, 2500, 8000 }, eqG[4] = { 0, 0, 0, 0 };
    bool compOn = false; float compThr = -20, compRatio = 3, compAtk = 10, compRel = 200, compMakeup = 0, compMix = 1;
    bool delayOn = false; float delayMsL = 375, delayMsR = 500, delayFb = 0.35f, delayMix = 0.0f;
    bool pingPong = false; float dlLowDb = 0, dlMidDb = 0, dlMidHz = 1000, dlHighDb = 0;
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
        setOversampling (1, true);
    }
    void setOversampling (int n, bool force = false) {
        if (n != 1 && n != 2 && n != 3 && n != 4 && n != 6) n = 1;
        if (n == os && ! force) return;
        os = n; fs = baseFs * (float) os;
        for (int c = 0; c < 2; ++c) { up[c].design (os); dn[c].design (os); }
        plate.fs = fs; plate.clear();
        for (int a = 1; a < kNumAlgos; ++a) { fdn[a].fs = fs; fdn[a].clear(); }
        preL.clear(); preR.clear(); tone[0].reset(); tone[1].reset();
        for (int c = 0; c < 2; ++c) for (int b = 0; b < 4; ++b) eq[c][b].reset();
        comp.fs = fs; comp.env = 0;
        delay.fs = fs; delay.clear(); delay.curL = delay.curR = 0; firstBlock = true;
        tr.fs = fs; tr.clear();
    }
    int latency() const { return up[0].latencyBase(); }
    float compReductionDb() const { return comp.reductionDb(); }

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
        plate.update (p.decaySec, p.size);
        for (int a = 1; a < kNumAlgos; ++a) fdn[a].update (p.decaySec, dampHz, p.size);
        const float preS = std::max (1.0f, p.predelayMs * 0.001f * fs);
        const float tL = clampf (p.delayMsL * 0.001f * fs, 1.0f, kMaxDelaySec * fs - 4);
        const float tR = clampf (p.delayMsR * 0.001f * fs, 1.0f, kMaxDelaySec * fs - 4);
        if (firstBlock) { delay.curL = tL; delay.curR = tR; firstBlock = false; }
        const float smA = 1.0f - std::exp (-1.0f / (0.03f * fs));      // ~30 ms parameter smoothing
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
                float wL = 0, wR = 0;
                for (int a = 0; a < kNumAlgos; ++a) {
                    sAlgo[a] += smA * (tgtAlgo[a] - sAlgo[a]);
                    if (! p.algoOn[a] && sAlgo[a] < 1e-4f) {
                        if (running[a]) { running[a] = false; if (a == Plate) plate.clear(); else fdn[a].clear(); }
                        continue;
                    }
                    running[a] = true;
                    float oL, oR;
                    if (a == Plate) plate.process (0.5f * (pl + pr), plateDampA, p.mod, oL, oR);
                    else fdn[a].process (pl, pr, p.mod, p.early, oL, oR);
                    wL += oL * sAlgo[a]; wR += oR * sAlgo[a];
                }
                for (int b = 0; b < 4; ++b) { wL = eq[0][b].process (wL); wR = eq[1][b].process (wR); }
                if (p.compOn) comp.process (wL, wR, p.compThr, p.compRatio, p.compMakeup, p.compMix);
                // ---- delay
                float dL = 0, dR = 0;
                if (p.delayOn || sDl > 1e-4f) delay.process (xL, xR, tL, tR, p.delayFb, p.pingPong, smA * 0.2f, dL, dR);
                // ---- mix
                float yL = xL * (1 - sRv) + wL * sRv + dL * sDl;
                float yR = xR * (1 - sRv) + wR * sRv + dR * sDl;
                if (p.trOn) { yL = tr.process (0, yL, p.trModel, p.trAmount); yR = tr.process (1, yR, p.trModel, p.trAmount); }
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
    bool running[kNumAlgos] = {};
};

} // namespace pv
