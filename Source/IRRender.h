// PlateVerb IR slot: prepares a loaded impulse response for convolution (no JUCE dependency).
// Every reverb parameter acts on the IR: decay / low / high decay / damping reshape the decay per band,
// size stretches the tail (or the whole IR with pitch), build fades in, early sets the early-to-late balance,
// plus Open-AIR-style start trim, ER/LR crossover, fade-out, cross-feed, cross-delay, asymmetry and reverse.
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace pv {

// four responses: a = left input -> (L out, R out), b = right input -> (L out, R out)
struct TrueStereoIR {
    std::vector<float> aL, aR, bL, bR;
    int length() const { return (int) aL.size(); }
    void resize (int n) { aL.resize ((size_t) n, 0.0f); aR.resize ((size_t) n, 0.0f); bL.resize ((size_t) n, 0.0f); bR.resize ((size_t) n, 0.0f); }
    std::vector<float>* ch (int i) { return i == 0 ? &aL : i == 1 ? &aR : i == 2 ? &bL : &bR; }
    const std::vector<float>* ch (int i) const { return i == 0 ? &aL : i == 1 ? &aR : i == 2 ? &bL : &bR; }
};

enum IrLayout { IrNone = 0, IrMono, IrStereo, IrTrueStereo };

struct IRSettings {
    float decaySec = 0;           // target RT60 (mid); <= 0 = as recorded
    float lowMult = 1, highMult = 1, damping = 0, size = 1;
    float buildMs = 3;            // fade-in
    float early = 0.7f;           // 0.7 = as recorded
    float startMs = 0, xoverMs = 80, fadeOutMs = 0;
    float crossFeed = 0, crossDelayMs = 0, asym = 0;
    bool reverse = false, stretchPitch = false;
    bool operator== (const IRSettings& o) const {
        return decaySec == o.decaySec && lowMult == o.lowMult && highMult == o.highMult && damping == o.damping && size == o.size
            && buildMs == o.buildMs && early == o.early && startMs == o.startMs && xoverMs == o.xoverMs && fadeOutMs == o.fadeOutMs
            && crossFeed == o.crossFeed && crossDelayMs == o.crossDelayMs && asym == o.asym && reverse == o.reverse && stretchPitch == o.stretchPitch;
    }
    bool operator!= (const IRSettings& o) const { return ! (*this == o); }
};

namespace irdsp {

// windowed-sinc resampling for offline use (any ratio)
inline std::vector<float> resample (const std::vector<float>& x, double ratio) {   // ratio = out rate / in rate
    if (x.empty() || std::abs (ratio - 1.0) < 1e-9) return x;
    const int n = (int) std::ceil ((double) x.size() * ratio);
    std::vector<float> y ((size_t) n, 0.0f);
    const double cut = std::min (1.0, ratio) * 0.97;            // anti-alias when going down
    const int half = 32;
    const double step = 1.0 / ratio;
    for (int i = 0; i < n; ++i) {
        const double pos = i * step; const int c = (int) std::floor (pos);
        double acc = 0;
        for (int k = c - half + 1; k <= c + half; ++k) {
            if (k < 0 || k >= (int) x.size()) continue;
            const double t = pos - k, a = t * cut;
            const double sinc = std::abs (a) < 1e-9 ? 1.0 : std::sin (3.14159265358979 * a) / (3.14159265358979 * a);
            const double w = 0.5 + 0.5 * std::cos (3.14159265358979 * t / (half + 1));
            acc += x[(size_t) k] * sinc * w * cut;
        }
        y[(size_t) i] = (float) acc;
    }
    return y;
}

struct BQ {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    void lp (double fs, double f) { double w = 2 * 3.14159265358979 * f / fs, al = std::sin (w) / (2 * 0.7071), c = std::cos (w), a0 = 1 + al;
        b0 = (1 - c) / 2 / a0; b1 = (1 - c) / a0; b2 = (1 - c) / 2 / a0; a1 = -2 * c / a0; a2 = (1 - al) / a0; }
    void hp (double fs, double f) { double w = 2 * 3.14159265358979 * f / fs, al = std::sin (w) / (2 * 0.7071), c = std::cos (w), a0 = 1 + al;
        b0 = (1 + c) / 2 / a0; b1 = -(1 + c) / a0; b2 = (1 + c) / 2 / a0; a1 = -2 * c / a0; a2 = (1 - al) / a0; }
    void run (std::vector<float>& x, bool backward) const {
        double z1 = 0, z2 = 0; const int n = (int) x.size();
        for (int j = 0; j < n; ++j) { const int i = backward ? n - 1 - j : j;
            const double v = x[(size_t) i], y = b0 * v + z1; z1 = b1 * v - a1 * y + z2; z2 = b2 * v - a2 * y; x[(size_t) i] = (float) y; }
    }
    void zeroPhase (std::vector<float>& x) const { run (x, false); run (x, true); }
};

// split into low (<300 Hz), mid, high (>4 kHz) with zero-phase filters: low + mid + high == input exactly
inline void split3 (const std::vector<float>& x, double fs, std::vector<float>& lo, std::vector<float>& mi, std::vector<float>& hi) {
    BQ l, h; l.lp (fs, 300.0); h.hp (fs, std::min (4000.0, fs * 0.4));
    lo = x; l.zeroPhase (lo);
    hi = x; h.zeroPhase (hi);
    mi.resize (x.size());
    for (size_t i = 0; i < x.size(); ++i) mi[i] = x[i] - lo[i] - hi[i];
}

// Schroeder RT60 (-5..-35 dB, or -5..-25 dB) of an energy sum
inline float rt60 (const std::vector<double>& e2, double fs) {
    const int n = (int) e2.size(); if (n < 16) return 0.0f;
    std::vector<double> E ((size_t) n); double acc = 0;
    for (int i = n - 1; i >= 0; --i) { acc += e2[(size_t) i]; E[(size_t) i] = acc; }
    if (E[0] <= 0) return 0.0f;
    auto at = [&] (double db) { const double t = E[0] * std::pow (10.0, db / 10.0); for (int i = 0; i < n; ++i) if (E[(size_t) i] < t) return i; return n - 1; };
    for (double end : { -35.0, -25.0, -15.0 }) {
        const int i0 = at (-5.0), i1 = at (end);
        if (i1 > i0 + 16) {
            double sx = 0, sy = 0, sxx = 0, sxy = 0; int m = 0;
            for (int i = i0; i < i1; i += 4) { const double x = i / fs, y = 10.0 * std::log10 (E[(size_t) i] / E[0] + 1e-30); sx += x; sy += y; sxx += x * x; sxy += x * y; ++m; }
            const double sl = (m * sxy - sx * sy) / std::max (1e-30, m * sxx - sx * sx);
            if (sl < 0) return (float) (-60.0 / sl);
        }
    }
    return 0.0f;
}

} // namespace irdsp

struct IRAnalysis { float rtLow = 0, rtMid = 0, rtHigh = 0; };

inline IRAnalysis analyseIR (const TrueStereoIR& ir, double fs) {
    IRAnalysis a; const int n = ir.length(); if (n < 64) return a;
    std::vector<float> m ((size_t) n);
    for (int i = 0; i < n; ++i) m[(size_t) i] = ir.aL[(size_t) i] + ir.bL[(size_t) i] + ir.aR[(size_t) i] + ir.bR[(size_t) i];
    std::vector<float> lo, mi, hi; irdsp::split3 (m, fs, lo, mi, hi);
    auto rt = [&] (const std::vector<float>& x) { std::vector<double> e ((size_t) n); for (int i = 0; i < n; ++i) e[(size_t) i] = (double) x[(size_t) i] * x[(size_t) i]; return irdsp::rt60 (e, fs); };
    a.rtLow = rt (lo); a.rtMid = rt (mi); a.rtHigh = rt (hi);
    return a;
}

// Render the IR actually used for convolution. 'src' is at the host rate; 'base' its analysis.
inline TrueStereoIR renderIR (const TrueStereoIR& src, const IRAnalysis& base, const IRSettings& s, double fs) {
    TrueStereoIR ir = src;
    int n = ir.length(); if (n < 8) return ir;
    // start trim
    const int trim = std::min (n - 8, (int) (std::max (0.0f, s.startMs) * 0.001 * fs));
    if (trim > 0) for (int c = 0; c < 4; ++c) ir.ch (c)->erase (ir.ch (c)->begin(), ir.ch (c)->begin() + trim);
    n = ir.length();
    // reverse (after dropping the silent end, so the reversed IR starts right away)
    if (s.reverse) {
        float pk = 0; for (int c = 0; c < 4; ++c) for (float v : *ir.ch (c)) pk = std::max (pk, std::abs (v));
        int last = n - 1; const float thr = pk * 1e-4f;
        while (last > 0) { bool loud = false; for (int c = 0; c < 4; ++c) if (std::abs ((*ir.ch (c))[(size_t) last]) > thr) loud = true; if (loud) break; --last; }
        ir.resize (last + 1); n = ir.length();
        for (int c = 0; c < 4; ++c) std::reverse (ir.ch (c)->begin(), ir.ch (c)->end());
    }
    // size: with pitch = resample the whole IR; without = stretch the decay
    float sizeDecay = std::max (0.25f, s.size);
    if (s.stretchPitch && std::abs (s.size - 1.0f) > 1e-3f) {
        for (int c = 0; c < 4; ++c) *ir.ch (c) = irdsp::resample (*ir.ch (c), std::max (0.25f, s.size));
        n = ir.length(); sizeDecay = 1.0f;
    }
    // normalise on the shaped-in-time but not yet re-enveloped IR, so the loudness follows the envelope controls naturally
    double eL = 0, eR = 0;
    for (int i = 0; i < n; ++i) { const double l = ir.aL[(size_t) i] + ir.bL[(size_t) i], r = ir.aR[(size_t) i] + ir.bR[(size_t) i]; eL += l * l; eR += r * r; }
    // +12 dB over the original 0.63 energy target: at the old level an IR needed the output turned up ~12 dB
    // to sound as loud as the same IR in Studio One's Open AIR
    const float norm = (float) std::sqrt (0.63 * 15.85 / std::max (1e-12, 0.5 * (eL + eR)));
    const float stretch = s.stretchPitch ? std::max (0.25f, s.size) : 1.0f;
    // decay per band: exponential re-envelope that turns the recorded RT into the requested one
    const float rt0[3] = { base.rtLow * stretch, base.rtMid * stretch, base.rtHigh * stretch };
    const float want = s.decaySec > 0 ? s.decaySec : base.rtMid * stretch;
    const float hiMul = s.highMult * (1.0f - 0.6f * std::min (1.0f, std::max (0.0f, s.damping)));
    const float scale = want / std::max (0.01f, rt0[1]) * sizeDecay;          // every band keeps its recorded ratio to the mids
    const float tgt[3] = { rt0[0] * scale * s.lowMult, rt0[1] * scale, rt0[2] * scale * hiMul };
    bool reshape = ! s.reverse;
    for (int b = 0; b < 3; ++b) if (rt0[b] <= 0.01f) reshape = false;
    int newLen = n;
    if (reshape) {
        float k[3]; bool any = false;
        for (int b = 0; b < 3; ++b) { k[b] = 6.9078f * (1.0f / std::max (0.03f, tgt[b]) - 1.0f / rt0[b]); if (std::abs (k[b]) > 1e-3f) any = true; }
        if (any) {
            // when shortening, the IR can end where the slowest band is 100 dB down
            float slow = 1e9f; for (int b = 0; b < 3; ++b) slow = std::min (slow, k[b] + 6.9078f / rt0[b]);
            if (slow > 0) newLen = std::min (n, (int) (11.5f / slow * fs) + 64);
            for (int c = 0; c < 4; ++c) {
                std::vector<float> lo, mi, hi; irdsp::split3 (*ir.ch (c), fs, lo, mi, hi);
                auto& x = *ir.ch (c);
                for (int i = 0; i < n; ++i) {
                    const float t = (float) (i / fs);
                    auto g = [t] (float kk) { return std::min (63.0f, std::exp (-kk * t)); };   // at most +36 dB
                    x[(size_t) i] = lo[(size_t) i] * g (k[0]) + mi[(size_t) i] * g (k[1]) + hi[(size_t) i] * g (k[2]);
                }
            }
        }
    }
    if (newLen < n) { ir.resize (newLen); n = newLen; }
    // early / late balance around the crossover (10 ms crossfade)
    const float erG = std::max (0.0f, s.early) / 0.7f;
    if (std::abs (erG - 1.0f) > 1e-3f) {
        const float x0 = std::max (0.0f, s.xoverMs) * 0.001f, w = 0.010f;
        for (int i = 0; i < n; ++i) {
            const float t = (float) (i / fs);
            float g = 1.0f;
            if (t < x0 - w * 0.5f) g = erG;
            else if (t < x0 + w * 0.5f) { const float u = (t - (x0 - w * 0.5f)) / w; g = erG + (1.0f - erG) * (0.5f - 0.5f * std::cos (3.14159265f * u)); }
            if (g != 1.0f) for (int c = 0; c < 4; ++c) (*ir.ch (c))[(size_t) i] *= g;
        }
    }
    // build = fade-in
    if (s.buildMs > 3.01f) {
        const int f = std::min (n, (int) (s.buildMs * 0.001 * fs));
        for (int i = 0; i < f; ++i) { const float g = 0.5f - 0.5f * std::cos (3.14159265f * (float) i / (float) f); for (int c = 0; c < 4; ++c) (*ir.ch (c))[(size_t) i] *= g; }
    }
    // fade-out at the end
    if (s.fadeOutMs > 0.5f) {
        const int f = std::min (n, (int) (s.fadeOutMs * 0.001 * fs));
        for (int j = 0; j < f; ++j) { const int i = n - f + j; const float g = 0.5f + 0.5f * std::cos (3.14159265f * (float) j / (float) f); for (int c = 0; c < 4; ++c) (*ir.ch (c))[(size_t) i] *= g; }
    }
    // cross-feed: each input also reaches the other side's response, delayed (asymmetry tilts the delay L/R)
    if (s.crossFeed > 0.001f) {
        const float a = std::min (1.0f, std::max (-1.0f, s.asym));
        const int dA = (int) (std::max (0.0f, s.crossDelayMs) * (1.0f + a) * 0.001 * fs), dB = (int) (std::max (0.0f, s.crossDelayMs) * (1.0f - a) * 0.001 * fs);
        const int m = n + std::max (dA, dB);
        TrueStereoIR o = ir; o.resize (m);
        for (int i = 0; i < n; ++i) {
            o.aL[(size_t) (i + dA)] += s.crossFeed * ir.bL[(size_t) i]; o.aR[(size_t) (i + dA)] += s.crossFeed * ir.bR[(size_t) i];
            o.bL[(size_t) (i + dB)] += s.crossFeed * ir.aL[(size_t) i]; o.bR[(size_t) (i + dB)] += s.crossFeed * ir.aR[(size_t) i];
        }
        ir = std::move (o); n = ir.length();
    }
    for (int c = 0; c < 4; ++c) for (auto& v : *ir.ch (c)) v *= norm;
    return ir;
}

// Peak envelope (one value per 'binMs') of a stereo response, plus low / mid / high band envelopes, for the display
struct WaveData {
    std::vector<float> l, r, lo, mid, hi; float binMs = 1.0f, rt = 0, xoverMs = -1; int slot = 0; bool valid = false, predelayIncluded = true;
};
inline WaveData makeWave (const std::vector<float>& L, const std::vector<float>& R, double fs, float binMs) {
    WaveData w; w.binMs = binMs;
    const int bin = std::max (1, (int) (binMs * 0.001 * fs)), n = (int) std::min (L.size(), R.size());
    std::vector<float> m ((size_t) n);
    for (int i = 0; i < n; ++i) m[(size_t) i] = 0.5f * (L[(size_t) i] + R[(size_t) i]);
    std::vector<float> bl, bm, bh; irdsp::split3 (m, fs, bl, bm, bh);
    float pk = 1e-9f;
    for (int i = 0; i < n; i += bin) {
        float a = 0, b = 0, x = 0, y = 0, z = 0;
        for (int j = i; j < std::min (n, i + bin); ++j) {
            a = std::max (a, std::abs (L[(size_t) j])); b = std::max (b, std::abs (R[(size_t) j]));
            x = std::max (x, std::abs (bl[(size_t) j])); y = std::max (y, std::abs (bm[(size_t) j])); z = std::max (z, std::abs (bh[(size_t) j]));
        }
        w.l.push_back (a); w.r.push_back (b); w.lo.push_back (x); w.mid.push_back (y); w.hi.push_back (z);
        pk = std::max (pk, std::max (a, b));
    }
    for (auto* v : { &w.l, &w.r, &w.lo, &w.mid, &w.hi }) for (auto& e : *v) e /= pk;
    std::vector<double> e ((size_t) n); for (int i = 0; i < n; ++i) e[(size_t) i] = 0.5 * ((double) L[(size_t) i] * L[(size_t) i] + (double) R[(size_t) i] * R[(size_t) i]);
    w.rt = irdsp::rt60 (e, fs); w.valid = true;
    return w;
}

} // namespace pv
