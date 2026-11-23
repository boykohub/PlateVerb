// Flexiverb 480 engine: two 480L machines (A, B) in the 480L's configurations, then the drawer
// (EQ, compressor, delay: now on the LARC's CTRL pages), the 480L hardware model and the output stage. No JUCE dependency.
#pragma once
#include "L480Dsp.h"
#include "CSEngine.h"          // the ChannelStrip preamp (ANALOG)
#include "L480Loops.h"        // the measured long recirculation per preset

namespace l480 {

enum Cfg { kSingle = 0, kCascade, kStereoSplit, kMonoSplit };

// ---------------------------------------------------------------- oversampling for the ANALOG / EQ / compressor chain
// Linear-phase polyphase FIR (Kaiser-windowed sinc, 24 taps per phase, pass band to 0.45 of the base rate, about
// -90 dB stop band). Factors 2, 4, 6 and 8. Latency: (taps - 1) / factor base samples in total (up + down).
struct PolyFir {
    int L = 1, P = 24; std::vector<double> h;
    static double bessel0 (double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; }
    void design (int factor) {
        L = std::max (1, factor); const int N = L * P; h.assign ((size_t) N, 0.0);
        const double fc = 0.47 / L, beta = 9.0, mid = (N - 1) * 0.5;
        for (int k = 0; k < N; ++k) {
            const double t = k - mid, x = 2 * fc * t;
            const double sinc = std::fabs (x) < 1e-12 ? 1.0 : std::sin (3.141592653589793 * x) / (3.141592653589793 * x);
            const double r = (k - mid) / mid, w = bessel0 (beta * std::sqrt (std::max (0.0, 1 - r * r))) / bessel0 (beta);
            h[(size_t) k] = 2 * fc * sinc * w;
        }
        double sum = 0; for (double v : h) sum += v; for (auto& v : h) v /= sum;      // unity DC gain
    }
    int latencyBase() const { return L > 1 ? (L * P - 1) / L : 0; }
};
struct Upsampler {                // one base-rate sample in, L samples out
    PolyFir f; std::vector<double> x; int pos = 0;
    void design (int L) { f.design (L); x.assign ((size_t) f.P, 0.0); pos = 0; }
    inline void process (float in, float* out) {
        pos = (pos + 1) % f.P; x[(size_t) pos] = in;
        for (int j = 0; j < f.L; ++j) {
            double y = 0;
            for (int p = 0, i = pos; p < f.P; ++p, i = (i == 0 ? f.P - 1 : i - 1)) y += f.h[(size_t) (p * f.L + j)] * x[(size_t) i];
            out[j] = (float) (y * f.L);
        }
    }
};
struct Downsampler {              // L samples in, one base-rate sample out
    PolyFir f; std::vector<double> x; int pos = 0, N = 1;
    void design (int L) { f.design (L); N = f.L * f.P; x.assign ((size_t) N, 0.0); pos = 0; }
    inline float process (const float* in) {
        for (int j = 0; j < f.L; ++j) { pos = (pos + 1) % N; x[(size_t) pos] = in[j]; }
        double y = 0;
        for (int k = 0, i = pos; k < N; ++k, i = (i == 0 ? N - 1 : i - 1)) y += f.h[(size_t) k] * x[(size_t) i];
        return (float) y;
    }
};
// EQ band in 64-bit (the 64-bit option)
struct BiquadD {
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void reset() { z1 = z2 = 0; }
    void from (const Biquad& f) { b0 = f.b0; b1 = f.b1; b2 = f.b2; a1 = f.a1; a2 = f.a2; }
    inline double process (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

struct EngineParams {
    int prog[2] = { 0, 0 };                 // program index per machine
    float val[2][kMaxParams] = {};          // LARC values per machine (units)
    int config = kSingle;
    int oversampling = 1; bool precision64 = false;      // ANALOG / EQ / compressor chain
    int eqModel = 1; float eqDrive = 0.5f;               // EQ character: the ChannelStrip models (0 = clean)
    float erAtkMs = 0.0f, dlAtkMs = 0.0f;                // reflection attack, delay attack
    // ANALOG: the ChannelStrip preamp on the reverb, on the EQ, or both
    bool anOn = false; int anTarget = 0, anModel = 1, anColour = 1; float anDrive = 6.0f, anOut = 0.0f;   // anTarget 0 reverb, 1 EQ, 2 both
    bool curveOn[2] = { false, false }; float curveMul[2][pv::DecayEq::N] = {};   // RT graph per machine
    int reflN[2] = { 0, 0 }; float reflMs[2][32] = {}, reflDb[2][32] = {}, reflPan[2][32] = {};   // your own early reflections
    int tapN[2] = { 0, 0 }; float tapMs[2][32] = {}, tapDb[2][32] = {};                         // your edits of the algorithm's own taps
    bool mute = false;
    float loopAmt = 1.0f;
    int addOn = 0;                           // machine B as an add-on to A: 0 off, 1 beside, 2 before (B feeds A), 3 after (A feeds B)                    // the 480L's long recirculation (stone depth): 0 off, 1 as measured, 2 double
    // drawer: EQ
    bool eqOn = false; float eqF[4] = { 100, 400, 2500, 8000 }, eqG[4] = {};
    // drawer: compressor
    pv::CompParams comp;
    // drawer: delay
    bool dlOn = false, dlPing = false; float dlMsL = 375, dlMsR = 500, dlFb = 0.35f, dlMix = 0.3f, dlLow = 0, dlHigh = 0;
    // setup
    pv::HwParams hw; bool trOn = false; int trModel = 0; float trFreq = 80, trAmt = 0.3f; float outDb = 0;
};

class Engine {
public:
    void prepare (double sampleRate) {
        fs = (float) sampleRate;
        for (auto& m : mach) m.prepare (fs);
        os = 0; setOversampling (1);
        delay.fs = fs; delay.init (fs); delay.clearSmart (fs); delay.curL = delay.curR = 0; firstBlock = true;
        hwIn.prepare (fs, fs); hwA.prepare (fs, fs); hwB.prepare (fs, fs);
        tr.fs = fs; tr.clear();
        sOut = 1; sMute = 0;
    }
    // the ANALOG / EQ / compressor chain runs at fs * os
    void setOversampling (int n) {
        n = (n == 2 || n == 4 || n == 6 || n == 8) ? n : 1;
        if (n == os) return;
        os = n; fsOs = fs * (float) os;
        comp.fs = fsOs; comp.reset(); comp.lastFs = -1;
        anVerb.prepare (fsOs); anEq.prepare (fsOs); anM = -1;
        for (auto& c : eq) for (auto& b : c) b.reset();
        for (auto& c : eqD) for (auto& b : c) b.reset();
        eqBank.prepare (fsOs);
        for (auto& u : up) u.design (os);
        for (auto& d : dn) d.design (os);
    }
    int latency() const { return os > 1 ? up[0].f.latencyBase() : 0; }
    // silence everything (the editor's response renderer reuses one engine for many short renders)
    void resetState() {
        for (auto& m : mach) { for (auto& a : m.algos) if (a) a->reset(); m.cur = -1; m.fadeState = 0; m.fadeG = 1; }
        hwIn.reset(); hwA.reset(); hwB.reset(); comp.reset(); firstBlock = true;
    }
    float meterIn() const { return mIn; }
    float meterOut() const { return mOut; }
    float compGr() const { return comp.reductionDb(); }
    int baseTaps (int m, float* ms, float* db, int max) const { return mach[m].baseTaps (ms, db, max); }
    const Machine& machine (int i) const { return mach[i]; }
    Machine& machine (int i) { return mach[i]; }

    void process (float* L, float* R, int n, const EngineParams& p) {
        setOversampling (p.oversampling);
        for (auto& m : mach) m.erAtk = p.erAtkMs;
        for (int m = 0; m < 2; ++m) { const auto& pr = programs()[(size_t) p.prog[m]]; mach[m].update (pr.algo, pr.bank, p.val[m], pr.trimDb, p.curveOn[m] ? p.curveMul[m] : nullptr, pr.shape);
                                      mach[m].refl.set (p.reflN[m], p.reflMs[m], p.reflDb[m], p.reflPan[m]); mach[m].setTapEdits (p.tapN[m], p.tapMs[m], p.tapDb[m]);
                                      if (mach[m].fadeState != 1) mach[m].loop.set (loopFor (p.prog[m]), p.loopAmt); }
        if (p.anModel != anM || p.anColour != anC || p.anDrive != anD || p.anOut != anO)
        { anVerb.set (p.anModel, p.anColour, p.anDrive, p.anOut, false, 70.0); anEq.set (p.anModel, p.anColour, p.anDrive, p.anOut, false, 70.0);
          anM = p.anModel; anC = p.anColour; anD = p.anDrive; anO = p.anOut; }
        const bool anV = p.anOn && (p.anTarget == 0 || p.anTarget == 2), anE = p.anOn && (p.anTarget == 1 || p.anTarget == 2);
        for (int c = 0; c < 2; ++c) {
            eq[c][0].lowShelf (fsOs, p.eqF[0], p.eqG[0]); eq[c][1].peak (fsOs, p.eqF[1], 0.9f, p.eqG[1]);
            eq[c][2].peak (fsOs, p.eqF[2], 0.9f, p.eqG[2]); eq[c][3].highShelf (fsOs, p.eqF[3], p.eqG[3]);
            for (int b = 0; b < 4; ++b) eqD[c][b].from (eq[c][b]);
        }
        comp.setTimes (p.comp.atkMs, p.comp.relMs);
        delay.setEq (p.dlLow, 0.0f, 1000.0f, p.dlHigh); delay.setAttack (p.dlAtkMs);
        {   // EQ with the analog character of the chosen model
            std::array<cs::BandSettings, 4> bs {};
            for (int b = 0; b < 4; ++b) { bs[(size_t) b].on = true; bs[(size_t) b].shelf = (b == 0 || b == 3); bs[(size_t) b].freq = p.eqF[b]; bs[(size_t) b].gain = p.eqG[b]; bs[(size_t) b].qScale = 1.0f; }
            eqBank.set (p.eqModel, bs, p.eqDrive);
        }
        tr.set (p.trModel, p.trFreq, p.trAmt);
        const float tL = pv::clampf (p.dlMsL * 0.001f * fs, 1.0f, pv::kMaxDelaySec * fs - 4), tR = pv::clampf (p.dlMsR * 0.001f * fs, 1.0f, pv::kMaxDelaySec * fs - 4);
        if (firstBlock) { delay.curL = tL; delay.curR = tR; firstBlock = false; }
        delay.service();
        const float smA = 1.0f - std::exp (-1.0f / (0.02f * fs));
        const float tgtOut = dbToGain (p.outDb), tgtDl = p.dlOn ? p.dlMix : 0.0f, tgtMute = p.mute ? 1.0f : 0.0f;
        const bool hwInOn = p.hw.on && p.hw.inAnalog, hwOutOn = p.hw.on && p.hw.outAnalog;
        float pkIn = 0, pkOut = 0;
        pv::ScopedLineQuant mem (p.hw.on ? 18 : 0);        // the 480L's 18-bit audio memory, set once per block
        for (int i = 0; i < n; ++i) {
            const float xL = L[i], xR = R[i];
            pkIn = std::max (pkIn, std::max (std::fabs (xL), std::fabs (xR)));
            float hL = xL, hR = xR;
            if (hwInOn) hwIn.input (hL, hR, p.hw);
            Ctx c; c.dryL = xL; c.dryR = xR;
            float yL, yR;
            if (p.addOn > 0) {   // a delay or chorus (machine B) together with the reverb (machine A)
                float aL = hL, aR = hR, bL = hL, bR = hR;
                if (p.addOn == 2) { mach[1].process (bL, bR, c); aL = bL; aR = bR; mach[0].process (aL, aR, c); yL = aL + bL; yR = aR + bR; }   // B feeds A
                else if (p.addOn == 3) { mach[0].process (aL, aR, c); bL = aL; bR = aR; mach[1].process (bL, bR, c); yL = bL; yR = bR; }      // A feeds B
                else { mach[0].process (aL, aR, c); mach[1].process (bL, bR, c); yL = aL + bL; yR = aR + bR; }                                 // side by side
                if (hwOutOn) hwA.output (yL, yR, p.hw);
            } else
            {
            switch (p.config) {
                case kCascade: {
                    float aL = hL, aR = hR; mach[0].process (aL, aR, c);
                    if (hwOutOn) hwA.output (aL, aR, p.hw);
                    c.revL = aL; c.revR = aR; c.hasRev = true;
                    float bL = aL, bR = aR; mach[1].process (bL, bR, c);
                    yL = bL; yR = bR; break;
                }
                case kStereoSplit: {          // A on Main, B mixed in (the plugin has one stereo output)
                    float aL = hL, aR = hR, bL = hL, bR = hR;
                    mach[0].process (aL, aR, c); mach[1].process (bL, bR, c);
                    yL = 0.5f * (aL + bL); yR = 0.5f * (aR + bR);
                    if (hwOutOn) hwA.output (yL, yR, p.hw);
                    break;
                }
                case kMonoSplit: {            // A gets the left input, B the right; A -> left out, B -> right out
                    float aL = hL, aR = hL, bL = hR, bR = hR;
                    mach[0].process (aL, aR, c); mach[1].process (bL, bR, c);
                    yL = 0.5f * (aL + aR); yR = 0.5f * (bL + bR);
                    if (hwOutOn) hwA.output (yL, yR, p.hw);
                    break;
                }
                default: {
                    float aL = hL, aR = hR; mach[0].process (aL, aR, c);
                    if (hwOutOn) hwA.output (aL, aR, p.hw);
                    yL = aL; yR = aR; break;
                }
            }
            }
            wL[i] = yL; wR[i] = yR;
        }
        // ---- ANALOG on the reverb, the EQ (with ANALOG on the EQ) and the compressor, at fs * os
        const bool anyPost = anV || anE || p.eqOn || p.comp.on;
        (void) anyPost;
        if (os == 1) {                         // with oversampling the resamplers always run: the latency stays constant
            for (int i = 0; i < n; ++i) post (wL[i], wR[i], L[i], R[i], p, anV, anE);
        } else {
            float uL[8], uR[8], kL[8], kR[8];
            for (int i = 0; i < n; ++i) {
                up[0].process (wL[i], uL); up[1].process (wR[i], uR); up[2].process (L[i], kL); up[3].process (R[i], kR);
                for (int k = 0; k < os; ++k) post (uL[k], uR[k], kL[k], kR[k], p, anV, anE);
                wL[i] = dn[0].process (uL); wR[i] = dn[1].process (uR);
            }
        }
        // ---- delay, transformer, mute and output at the base rate
        for (int i = 0; i < n; ++i) {
            const float xL = L[i], xR = R[i];
            float yL = wL[i], yR = wR[i];
            sOut += smA * (tgtOut - sOut); sDl += smA * (tgtDl - sDl); sMute += smA * (tgtMute - sMute);
            if (p.dlOn || sDl > 1e-4f) {
                float dL, dR; delay.process (xL, xR, tL, tR, p.dlFb, p.dlPing, smA * 0.2f, dL, dR);
                yL += dL * sDl; yR += dR * sDl;
            }
            if (p.trOn) { yL = tr.process (0, yL); yR = tr.process (1, yR); }
            // MUTE removes the processed audio while held: the dry input passes
            yL = yL * (1.0f - sMute) + xL * sMute; yR = yR * (1.0f - sMute) + xR * sMute;
            L[i] = yL * sOut; R[i] = yR * sOut;
            pkOut = std::max (pkOut, std::max (std::fabs (L[i]), std::fabs (R[i])));
        }
        mIn = pkIn; mOut = pkOut;
    }

private:
    inline void post (float& yL, float& yR, float kL, float kR, const EngineParams& p, bool anV, bool anE) {
        if (anV) { yL = anVerb.process (0, yL); yR = anVerb.process (1, yR); }
        if (p.eqOn) {
            if (++eqStep >= 16) { eqStep = 0; eqBank.step(); }
            yL = eqBank.process (0, yL); yR = eqBank.process (1, yR);         // 64-bit filters with the model's analog colour
        }
        if (anE) { yL = anEq.process (0, yL); yR = anEq.process (1, yR); }
        if (p.comp.on) comp.process (yL, yR, p.comp.duck ? kL : yL, p.comp.duck ? kR : yR, p.comp);
        else comp.meter (yL, yR);
    }
    cs::EqBank eqBank; int eqStep = 0;
    int os = 1; float fsOs = 48000; Upsampler up[4]; Downsampler dn[2]; BiquadD eqD[2][4];
    float wL[512] = {}, wR[512] = {};
    float fs = 48000, sOut = 1, sDl = 0, sMute = 0, mIn = 0, mOut = 0; bool firstBlock = true;
    Machine mach[2];
    cs::Preamp anVerb, anEq; int anM = -1, anC = -1; float anD = -1, anO = -99;
    Biquad eq[2][4]; pv::Compressor comp; pv::StereoDelay delay; pv::Transformer tr; pv::HwPath hwIn, hwA, hwB;
};

} // namespace l480
