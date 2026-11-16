// PlateVerb zero-latency, low-CPU convolution (no JUCE dependency).
//
// Non-uniformly partitioned: the first 128 IR samples run as a direct FIR, then three FFT stages with
// partitions of 128, 1024 and 8192 samples. Small partitions keep the latency at zero, big ones keep the
// cost low for long tails. The big stages have time to spare before their output is due, so their work
// (including every FFT butterfly pass) is spread evenly over the audio blocks: no CPU spikes.
// Two inputs, two outputs, up to four IR paths (true stereo); empty paths cost nothing.
#pragma once
#include <vector>
#include <cmath>
#include <memory>
#include <algorithm>
#include "IRRender.h"

namespace pv {

// iterative radix-2 complex FFT whose passes can be run one at a time
struct StepFFT {
    int n = 0, log2n = 0;
    std::vector<int> rev; std::vector<float> twr, twi;
    void init (int size) {
        n = size; log2n = 0; while ((1 << log2n) < n) ++log2n;
        rev.resize ((size_t) n);
        for (int i = 0; i < n; ++i) { int r = 0; for (int b = 0; b < log2n; ++b) if (i & (1 << b)) r |= 1 << (log2n - 1 - b); rev[(size_t) i] = r; }
        twr.resize ((size_t) n / 2); twi.resize ((size_t) n / 2);
        for (int k = 0; k < n / 2; ++k) { const double a = -2.0 * 3.14159265358979323846 * k / n; twr[(size_t) k] = (float) std::cos (a); twi[(size_t) k] = (float) std::sin (a); }
    }
    int numPasses() const { return log2n; }
    void bitReverse (float* re, float* im) const {
        for (int i = 0; i < n; ++i) { const int r = rev[(size_t) i]; if (r > i) { std::swap (re[i], re[r]); std::swap (im[i], im[r]); } }
    }
    // pass p (0..log2n-1); inverse uses conjugate twiddles
    void pass (float* re, float* im, int p, bool inverse) const {
        const int half = 1 << p, span = half << 1, step = n / span;
        const float sg = inverse ? -1.0f : 1.0f;
        for (int start = 0; start < n; start += span)
            for (int k = 0; k < half; ++k) {
                const float wr = twr[(size_t) (k * step)], wi = sg * twi[(size_t) (k * step)];
                const int a = start + k, b = a + half;
                const float tr = re[b] * wr - im[b] * wi, ti = re[b] * wi + im[b] * wr;
                re[b] = re[a] - tr; im[b] = im[a] - ti; re[a] += tr; im[a] += ti;
            }
    }
    void run (float* re, float* im, bool inverse) const { bitReverse (re, im); for (int p = 0; p < log2n; ++p) pass (re, im, p, inverse); }
};

class PartitionedConvolver {
public:
    static constexpr int kTick = 128;                       // all scheduling happens on 128-sample boundaries
    static constexpr int kHead = 128;                       // direct FIR length
    static constexpr int kRing = 32768;                     // output accumulator (power of two, > 2 x biggest partition)

    // builds everything (call on a background thread)
    void build (const TrueStereoIR& ir) {
        const int len = ir.length();
        const std::vector<float>* src[4] = { &ir.aL, &ir.aR, &ir.bL, &ir.bR };
        for (int p = 0; p < 4; ++p) {
            active[p] = false;
            for (float v : *src[p]) if (v != 0.0f) { active[p] = true; break; }
            head[p].assign ((size_t) kHead, 0.0f);
            for (int i = 0; i < std::min (kHead, len); ++i) head[p][(size_t) (kHead - 1 - i)] = (*src[p])[(size_t) i];   // reversed for the dot product
        }
        const int parts[3] = { 128, 1024, 8192 }, offs[3] = { 128, 2048, 16384 }, ends[3] = { 2048, 16384, 1 << 30 };
        numStages = 0;
        for (int s = 0; s < 3; ++s) {
            if (len <= offs[s]) break;
            Stage& st = stages[numStages++];
            st.P = parts[s]; st.N = 2 * parts[s]; st.offset = offs[s];
            const int stop = std::min (len, ends[s]);
            st.S = (stop - offs[s] + st.P - 1) / st.P;
            st.fft.init (st.N);
            const int bins = st.P + 1;
            for (int p = 0; p < 4; ++p) {
                st.hRe[p].clear(); st.hIm[p].clear();
                if (! active[p]) continue;
                st.hRe[p].assign ((size_t) (st.S * bins), 0.0f); st.hIm[p].assign ((size_t) (st.S * bins), 0.0f);
                std::vector<float> re ((size_t) st.N), im ((size_t) st.N);
                for (int j = 0; j < st.S; ++j) {
                    std::fill (re.begin(), re.end(), 0.0f); std::fill (im.begin(), im.end(), 0.0f);
                    for (int i = 0; i < st.P; ++i) { const int k = st.offset + j * st.P + i; if (k < stop) re[(size_t) i] = (*src[p])[(size_t) k]; }
                    st.fft.run (re.data(), im.data(), false);
                    const float sc = 1.0f / (float) st.N;           // inverse FFT scaling folded into the IR
                    for (int b = 0; b < bins; ++b) { st.hRe[p][(size_t) (j * bins + b)] = re[(size_t) b] * sc; st.hIm[p][(size_t) (j * bins + b)] = im[(size_t) b] * sc; }
                }
            }
            for (int c = 0; c < 2; ++c) { st.xRe[c].assign ((size_t) (st.S * bins), 0.0f); st.xIm[c].assign ((size_t) (st.S * bins), 0.0f); }
            for (int c = 0; c < 2; ++c) { st.wRe[c].assign ((size_t) st.N, 0.0f); st.wIm[c].assign ((size_t) st.N, 0.0f); st.aRe[c].assign ((size_t) st.N, 0.0f); st.aIm[c].assign ((size_t) st.N, 0.0f); }
            st.inHist[0].assign ((size_t) st.P, 0.0f); st.inHist[1].assign ((size_t) st.P, 0.0f);
            st.pos = 0; st.busy = false;
        }
        for (int c = 0; c < 2; ++c) { hist[c].assign ((size_t) (2 * kHead), 0.0f); acc[c].assign ((size_t) kRing, 0.0f); blockIn[c].assign ((size_t) 8192, 0.0f); }
        hpos = 0; t = 0;
        // paths per output: out L <- aL (in L), bL (in R); out R <- aR (in L), bR (in R)
    }

    void reset() {
        for (int c = 0; c < 2; ++c) { std::fill (hist[c].begin(), hist[c].end(), 0.0f); std::fill (acc[c].begin(), acc[c].end(), 0.0f); }
        for (int s = 0; s < numStages; ++s) {
            Stage& st = stages[s];
            for (int c = 0; c < 2; ++c) { std::fill (st.xRe[c].begin(), st.xRe[c].end(), 0.0f); std::fill (st.xIm[c].begin(), st.xIm[c].end(), 0.0f); }
            st.busy = false; st.pos = 0;
        }
        t = 0; hpos = 0;
    }

    // inL/inR -> add the wet signal into outL/outR (they are overwritten)
    void process (const float* inL, const float* inR, float* outL, float* outR, int n) {
        for (int i = 0; i < n; ++i) {
            const float x[2] = { inL[i], inR[i] };
            // input history for the direct head (mirrored so the last kHead samples are contiguous)
            for (int c = 0; c < 2; ++c) { hist[c][(size_t) hpos] = x[c]; hist[c][(size_t) (hpos + kHead)] = x[c]; }
            const float* hL = &hist[0][(size_t) (hpos + 1)];
            const float* hR = &hist[1][(size_t) (hpos + 1)];
            hpos = (hpos + 1) % kHead;
            float yL = 0, yR = 0;
            if (active[0]) yL += dot (hL, head[0].data());
            if (active[2]) yL += dot (hR, head[2].data());
            if (active[1]) yR += dot (hL, head[1].data());
            if (active[3]) yR += dot (hR, head[3].data());
            const int ri = (int) (t & (kRing - 1));
            outL[i] = yL + acc[0][(size_t) ri]; outR[i] = yR + acc[1][(size_t) ri];
            acc[0][(size_t) ri] = 0.0f; acc[1][(size_t) ri] = 0.0f;
            const int bi = (int) (t & 8191);
            blockIn[0][(size_t) bi] = x[0]; blockIn[1][(size_t) bi] = x[1];
            ++t;
            if ((t & (kTick - 1)) == 0) tick();
        }
    }

    int numActivePaths() const { int k = 0; for (bool a : active) k += a ? 1 : 0; return k; }

private:
    static inline float dot (const float* a, const float* b) { float s0 = 0, s1 = 0, s2 = 0, s3 = 0; for (int k = 0; k < kHead; k += 4) { s0 += a[k] * b[k]; s1 += a[k + 1] * b[k + 1]; s2 += a[k + 2] * b[k + 2]; s3 += a[k + 3] * b[k + 3]; } return (s0 + s1) + (s2 + s3); }

    struct Stage {
        int P = 0, N = 0, offset = 0, S = 0;
        StepFFT fft;
        std::vector<float> hRe[4], hIm[4];          // IR partition spectra, [segment][bin]
        std::vector<float> xRe[2], xIm[2];          // input spectra ring (frequency-domain delay line)
        std::vector<float> wRe[2], wIm[2];          // FFT work buffers (input)
        std::vector<float> aRe[2], aIm[2];          // accumulators / inverse FFT (output)
        std::vector<float> inHist[2];
        int pos = 0;                                // ring index of the newest input spectrum
        // job state
        bool busy = false; long long start = 0, deadline = 0; int step = 0, total = 0;
    };
    Stage stages[3]; int numStages = 0;
    bool active[4] = { false, false, false, false };
    std::vector<float> head[4], hist[2], acc[2], blockIn[2];
    int hpos = 0; long long t = 0;

    // job steps: [in L bitrev + passes][in R ...] [MAC chunks] [out L bitrev + passes + add][out R ...]
    static constexpr int kBinChunk = 1024;                 // complex multiply-adds per path per step
    int binChunks (const Stage& st) const { return (st.P + 1 + kBinChunk - 1) / kBinChunk; }
    int macChunks (const Stage& st) const { return st.S * binChunks (st); }
    int totalSteps (const Stage& st) const { const int f = st.fft.numPasses() + 1; return 2 * f + macChunks (st) + 2 * (f + 1); }

    void tick() {
        for (int s = 0; s < numStages; ++s) {
            Stage& st = stages[s];
            if (st.busy) advance (st, false);
            if ((t % st.P) == 0) {
                if (st.busy) advance (st, true);                // must be done before a new block starts
                startJob (st);
            }
            if (st.busy) advance (st, false);
        }
    }
    void startJob (Stage& st) {
        st.pos = (st.pos + 1) % st.S;
        st.busy = true; st.step = 0; st.total = totalSteps (st);
        st.start = t; st.deadline = t - st.P + st.offset;   // first output sample of this block's contribution
        for (int c = 0; c < 2; ++c) {
            std::fill (st.wRe[c].begin(), st.wRe[c].end(), 0.0f); std::fill (st.wIm[c].begin(), st.wIm[c].end(), 0.0f);
            for (int i = 0; i < st.P; ++i) st.wRe[c][(size_t) i] = blockIn[c][(size_t) ((t - st.P + i) & 8191)];
        }
    }
    void advance (Stage& st, bool finish) {
        int target = st.total;
        if (! finish && t < st.deadline) {
            const double frac = (double) (t - st.start + kTick) / (double) std::max<long long> (kTick, st.deadline - st.start);
            target = std::min (st.total, (int) std::ceil (frac * st.total));
        }
        while (st.busy && st.step < target) doStep (st);
    }
    void doStep (Stage& st) {
        const int f = st.fft.numPasses() + 1, bins = st.P + 1;
        int k = st.step++;
        if (k < 2 * f) {                                       // forward FFT of the new input block, one pass per step
            const int c = k / f, q = k % f;
            if (q == 0) st.fft.bitReverse (st.wRe[c].data(), st.wIm[c].data());
            else st.fft.pass (st.wRe[c].data(), st.wIm[c].data(), q - 1, false);
            if (q == f - 1) {
                float* xr = &st.xRe[c][(size_t) (st.pos * bins)]; float* xi = &st.xIm[c][(size_t) (st.pos * bins)];
                std::copy (st.wRe[c].begin(), st.wRe[c].begin() + bins, xr); std::copy (st.wIm[c].begin(), st.wIm[c].begin() + bins, xi);
            }
            return;
        }
        k -= 2 * f;
        const int mc = macChunks (st);
        if (k < mc) {                                          // complex multiply-accumulate: one segment, one bin range per step
            if (k == 0) for (int c = 0; c < 2; ++c) { std::fill (st.aRe[c].begin(), st.aRe[c].end(), 0.0f); std::fill (st.aIm[c].begin(), st.aIm[c].end(), 0.0f); }
            const int nbc = binChunks (st), j = k / nbc, b0 = (k % nbc) * kBinChunk, b1 = std::min (bins, b0 + kBinChunk);
            {
                const int xs = ((st.pos - j) % st.S + st.S) % st.S;
                for (int p = 0; p < 4; ++p) {
                    if (! active[p]) continue;
                    const int in = p < 2 ? 0 : 1, out = (p & 1);
                    const float* xr = &st.xRe[in][(size_t) (xs * bins)]; const float* xi = &st.xIm[in][(size_t) (xs * bins)];
                    const float* hr = &st.hRe[p][(size_t) (j * bins)]; const float* hi = &st.hIm[p][(size_t) (j * bins)];
                    float* ar = st.aRe[out].data(); float* ai = st.aIm[out].data();
                    for (int b = b0; b < b1; ++b) { ar[b] += xr[b] * hr[b] - xi[b] * hi[b]; ai[b] += xr[b] * hi[b] + xi[b] * hr[b]; }
                }
            }
            return;
        }
        k -= mc;
        const int fo = f + 1;                                   // per output: bitrev + passes + overlap-add
        const int c = k / fo, q = k % fo;
        float* re = st.aRe[c].data(); float* im = st.aIm[c].data();
        if (q == 0) {
            for (int b = 1; b < st.P; ++b) { re[st.N - b] = re[b]; im[st.N - b] = -im[b]; }   // Hermitian: real output
            st.fft.bitReverse (re, im);
        }
        else if (q < f) st.fft.pass (re, im, q - 1, true);
        else {
            float* a = acc[c].data();
            for (int i = 0; i < st.N; ++i) a[(size_t) ((st.deadline + i) & (kRing - 1))] += re[i];
        }
        if (st.step >= st.total) st.busy = false;
    }
};

// Cut the IR where the reverb has sunk into the recording's noise floor (or 80 dB below the peak):
// what is left after that is hiss, which only costs CPU and adds noise.
inline void trimIRTail (TrueStereoIR& ir, double fs, float floorDb = -80.0f) {
    const int n = ir.length(); if (n < 4096) return;
    const int win = std::max (32, (int) (0.010 * fs));
    std::vector<float> env;
    for (int s = 0; s < n; s += win) {
        double e = 0; int cnt = 0;
        for (int c = 0; c < 4; ++c) for (int i = s; i < std::min (n, s + win); ++i) { const double v = (*ir.ch (c))[(size_t) i]; e += v * v; ++cnt; }
        env.push_back ((float) std::sqrt (e / std::max (1, cnt)));
    }
    float pk = 0; size_t pkAt = 0; for (size_t i = 0; i < env.size(); ++i) if (env[i] > pk) { pk = env[i]; pkAt = i; }
    if (pk <= 0) return;
    // noise floor: the quietest stretch among the last 20 % of the file
    const size_t tailStart = env.size() - std::max<size_t> (3, env.size() / 5);
    std::vector<float> tail (env.begin() + (long) tailStart, env.end()); std::sort (tail.begin(), tail.end());
    const float noise = tail[tail.size() / 2];
    const float thr = std::max (pk * std::pow (10.0f, floorDb / 20.0f), noise * 2.0f);   // 6 dB above the hiss
    size_t last = pkAt;
    for (size_t i = pkAt; i < env.size(); ++i) {
        // the first window that stays under the threshold for the next 50 ms
        bool quiet = true;
        for (size_t k = i; k < std::min (env.size(), i + 5); ++k) if (env[k] > thr) { quiet = false; break; }
        if (quiet) { last = i; break; }
        last = i;
    }
    const int fade = std::max (64, (int) (0.06 * fs));
    const int keep = std::min (n, (int) (last * (size_t) win) + fade);
    if (keep >= n) return;
    ir.resize (keep);
    for (int j = 0; j < fade && keep - fade + j >= 0; ++j) {
        const int i = keep - fade + j; const float g = 0.5f + 0.5f * std::cos (3.14159265f * (float) j / (float) fade);
        for (int c = 0; c < 4; ++c) (*ir.ch (c))[(size_t) i] *= g;
    }
}

} // namespace pv
