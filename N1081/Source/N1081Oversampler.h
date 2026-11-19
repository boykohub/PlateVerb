// Linear-phase polyphase FIR oversampler for any integer factor (2, 4, 6, 8 ...), 64-bit.
// Kaiser-windowed sinc, ~100 dB stopband, pass band to 0.45 x base rate.
// Latency is exactly kTapsPerPhase base-rate samples (up + down together).
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace n1081 {

class FirOversampler
{
public:
    static constexpr int kTapsPerPhase = 64;

    void prepare (int factor, int maxBaseBlock, int channels)
    {
        L = std::max (1, factor);
        nCh = channels;
        if (L == 1) return;
        N = L * kTapsPerPhase + 1;                       // odd length: integer group delay
        design();
        upHist.assign ((size_t) nCh, std::vector<double> ((size_t) 2 * kTapsPerPhase + 2, 0.0));
        upPos.assign ((size_t) nCh, 0);
        R = N + L;                                        // keeps the last N + L - 1 high-rate samples
        dnHist.assign ((size_t) nCh, std::vector<double> ((size_t) 2 * R, 0.0));
        dnPos.assign ((size_t) nCh, 0);
        buf.assign ((size_t) nCh, std::vector<double> ((size_t) maxBaseBlock * L, 0.0));
    }
    void reset()
    {
        for (auto& h : upHist) std::fill (h.begin(), h.end(), 0.0);
        for (auto& h : dnHist) std::fill (h.begin(), h.end(), 0.0);
        std::fill (upPos.begin(), upPos.end(), 0); std::fill (dnPos.begin(), dnPos.end(), 0);
    }
    int factor() const { return L; }
    int latency() const { return L == 1 ? 0 : kTapsPerPhase; }
    double* channel (int c) { return buf[(size_t) c].data(); }

    // base -> high rate into the internal buffer
    void up (int c, const double* in, int n)
    {
        auto& h = upHist[(size_t) c]; int& pos = upPos[(size_t) c];
        double* out = buf[(size_t) c].data();
        const int T = kTapsPerPhase + 1;                 // per-phase length (ceil(N/L))
        for (int i = 0; i < n; ++i) {
            pos = (pos == 0 ? T : pos) - 1;
            h[(size_t) pos] = h[(size_t) pos + T] = in[i];    // mirrored ring: contiguous reads
            const double* x = h.data() + pos;
            for (int p = 0; p < L; ++p) {
                const double* k = poly[(size_t) p].data();
                double s = 0;
                for (int j = 0; j < T; ++j) s += k[j] * x[j];
                out[i * L + p] = s;
            }
        }
    }
    // high rate (internal buffer) -> base rate
    void down (int c, double* out, int n)
    {
        auto& h = dnHist[(size_t) c]; int& pos = dnPos[(size_t) c];
        const double* in = buf[(size_t) c].data();
        for (int i = 0; i < n; ++i) {
            for (int p = 0; p < L; ++p) {
                pos = (pos == 0 ? R : pos) - 1;
                h[(size_t) pos] = h[(size_t) pos + R] = in[i * L + p];
            }
            const double* x = h.data() + pos + (L - 1);       // align to phase 0: integer latency
            double s = 0;
            for (int j = 0; j < N; ++j) s += taps[(size_t) j] * x[j];
            out[i] = s;
        }
    }

private:
    static double bessel0 (double x)
    {
        double s = 1, t = 1;
        for (int k = 1; k < 40; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; }
        return s;
    }
    void design()
    {
        const double fc = 0.5 / L, beta = 10.0, M = (N - 1) * 0.5;
        taps.assign ((size_t) N, 0.0);
        double sum = 0;
        for (int i = 0; i < N; ++i) {
            const double t = i - M;
            const double sinc = t == 0 ? 2 * fc : std::sin (2 * 3.14159265358979323846 * fc * t) / (3.14159265358979323846 * t);
            const double r = t / M;
            const double w = bessel0 (beta * std::sqrt (std::max (0.0, 1 - r * r))) / bessel0 (beta);
            taps[(size_t) i] = sinc * w; sum += taps[(size_t) i];
        }
        for (auto& v : taps) v /= sum;                   // unity DC gain (down-sampler)
        const int T = kTapsPerPhase + 1;
        poly.assign ((size_t) L, std::vector<double> ((size_t) T, 0.0));
        for (int p = 0; p < L; ++p)
            for (int j = 0; j < T; ++j) {
                const int idx = p + j * L;
                poly[(size_t) p][(size_t) j] = idx < N ? taps[(size_t) idx] * L : 0.0;   // gain L for zero-stuffing
            }
    }

    int L = 1, N = 1, R = 1, nCh = 2;
    std::vector<double> taps;
    std::vector<std::vector<double>> poly, upHist, dnHist, buf;
    std::vector<int> upPos, dnPos;
};

} // namespace n1081
