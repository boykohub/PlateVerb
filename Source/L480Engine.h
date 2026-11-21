// Flexiverb 480 engine: two 480L machines (A, B) in the 480L's configurations, then the drawer
// (EQ, compressor, delay: now on the LARC's CTRL pages), the 480L hardware model and the output stage. No JUCE dependency.
#pragma once
#include "L480Dsp.h"

namespace l480 {

enum Cfg { kSingle = 0, kCascade, kStereoSplit, kMonoSplit };

struct EngineParams {
    int prog[2] = { 0, 0 };                 // program index per machine
    float val[2][kMaxParams] = {};          // LARC values per machine (units)
    int config = kSingle;
    bool curveOn[2] = { false, false }; float curveMul[2][pv::DecayEq::N] = {};   // RT graph per machine
    bool mute = false;
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
        comp.fs = fs; comp.reset(); comp.lastFs = -1;
        delay.fs = fs; delay.init (fs); delay.clearSmart (fs); delay.curL = delay.curR = 0; firstBlock = true;
        hwIn.prepare (fs, fs); hwA.prepare (fs, fs); hwB.prepare (fs, fs);
        tr.fs = fs; tr.clear();
        for (auto& c : eq) for (auto& b : c) b.reset();
        sOut = 1; sMute = 0;
    }
    float meterIn() const { return mIn; }
    float meterOut() const { return mOut; }
    float compGr() const { return comp.reductionDb(); }
    const Machine& machine (int i) const { return mach[i]; }
    Machine& machine (int i) { return mach[i]; }

    void process (float* L, float* R, int n, const EngineParams& p) {
        for (int m = 0; m < 2; ++m) { const auto& pr = programs()[(size_t) p.prog[m]]; mach[m].update (pr.algo, pr.bank, p.val[m], pr.trimDb, p.curveOn[m] ? p.curveMul[m] : nullptr); }
        for (int c = 0; c < 2; ++c) {
            eq[c][0].lowShelf (fs, p.eqF[0], p.eqG[0]); eq[c][1].peak (fs, p.eqF[1], 0.9f, p.eqG[1]);
            eq[c][2].peak (fs, p.eqF[2], 0.9f, p.eqG[2]); eq[c][3].highShelf (fs, p.eqF[3], p.eqG[3]);
        }
        comp.setTimes (p.comp.atkMs, p.comp.relMs);
        delay.setEq (p.dlLow, 0.0f, 1000.0f, p.dlHigh); delay.setAttack (0.0f);
        tr.set (p.trModel, p.trFreq, p.trAmt);
        const float tL = pv::clampf (p.dlMsL * 0.001f * fs, 1.0f, pv::kMaxDelaySec * fs - 4), tR = pv::clampf (p.dlMsR * 0.001f * fs, 1.0f, pv::kMaxDelaySec * fs - 4);
        if (firstBlock) { delay.curL = tL; delay.curR = tR; firstBlock = false; }
        delay.service();
        const float smA = 1.0f - std::exp (-1.0f / (0.02f * fs));
        const float tgtOut = dbToGain (p.outDb), tgtDl = p.dlOn ? p.dlMix : 0.0f, tgtMute = p.mute ? 1.0f : 0.0f;
        const bool hwInOn = p.hw.on && p.hw.inAnalog, hwOutOn = p.hw.on && p.hw.outAnalog;
        float pkIn = 0, pkOut = 0;
        for (int i = 0; i < n; ++i) {
            const float xL = L[i], xR = R[i];
            pkIn = std::max (pkIn, std::max (std::fabs (xL), std::fabs (xR)));
            sOut += smA * (tgtOut - sOut); sDl += smA * (tgtDl - sDl); sMute += smA * (tgtMute - sMute);
            float hL = xL, hR = xR;
            if (hwInOn) hwIn.input (hL, hR, p.hw);
            Ctx c; c.dryL = xL; c.dryR = xR;
            float yL, yR;
            {
            pv::ScopedLineQuant mem (p.hw.on ? 18 : 0);    // the 480L's 18-bit audio memory, machines only
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
            // drawer: EQ and compressor on the 480L's output
            if (p.eqOn) for (int b = 0; b < 4; ++b) { yL = eq[0][b].process (yL); yR = eq[1][b].process (yR); }
            if (p.comp.on) comp.process (yL, yR, p.comp.duck ? xL : yL, p.comp.duck ? xR : yR, p.comp);
            else comp.meter (yL, yR);
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
    float fs = 48000, sOut = 1, sDl = 0, sMute = 0, mIn = 0, mOut = 0; bool firstBlock = true;
    Machine mach[2];
    Biquad eq[2][4]; pv::Compressor comp; pv::StereoDelay delay; pv::Transformer tr; pv::HwPath hwIn, hwA, hwB;
};

} // namespace l480
