// N1081 — 1081 channel amplifier + 2254 / PlateVerb compressor. Parameters, oversampling, editor.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "N1081Engine.h"
#include "N1081Oversampler.h"
#include "Engine.h"            // PlateVerb's compressor (pv::Compressor), unchanged

using namespace juce;

namespace ids {
    static const char* line = "line";   static const char* out = "output";  static const char* inGain = "ingain";
    static const char* hpf = "hpf";     static const char* lpf = "lpf";
    static const char* eqIn = "eqin";   static const char* phase = "phase";
    static const char* freq[4] = { "lf_freq", "lmf_freq", "hmf_freq", "hf_freq" };
    static const char* gain[4] = { "lf_gain", "lmf_gain", "hmf_gain", "hf_gain" };
    static const char* alt [4] = { "lf_peak", "lmf_hiq", "hmf_hiq", "hf_peak" };
    static const char* cModel = "cmp_model"; static const char* cIn = "cmp_in"; static const char* cOrder = "cmp_order";
    static const char* cMix = "cmp_mix"; static const char* cSc = "cmp_schpf";
    static const char* kThr = "c_thr"; static const char* kRatio = "c_ratio"; static const char* kRec = "c_rec";
    static const char* kGain = "c_gain"; static const char* kFast = "c_fast"; static const char* kComp = "c_comp";
    static const char* kLim = "c_lim"; static const char* kLimLvl = "c_limlvl"; static const char* kLimRec = "c_limrec";
    static const char* pThr = "p_thr"; static const char* pRatio = "p_ratio"; static const char* pAtk = "p_atk";
    static const char* pRel = "p_rel"; static const char* pMk = "p_makeup"; static const char* pTone = "p_tone";
    static const char* os = "os"; static const char* osFilt = "osfilt";
}

static String hz (double f) { return f >= 1000 ? String (f / 1000.0, f < 10000 && std::fmod (f, 1000.0) != 0 ? 1 : 0) + "k" : String ((int) f); }
static StringArray freqNames (const double* t, int n) { StringArray a; for (int i = 0; i < n; ++i) a.add (hz (t[i])); return a; }
static String signedDb (double v, int dec = 0) { return (v > 0.04 ? "+" : "") + String (v, dec); }

static AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    AudioProcessorValueTreeState::ParameterLayout l;
    auto choice = [&] (const char* id, const String& name, const StringArray& items, int def, bool automatable = true) {
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def, AudioParameterChoiceAttributes().withAutomatable (automatable)));
    };
    auto flt = [&] (const char* id, const String& name, NormalisableRange<float> r, float def, const String& unit, int dec) {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                   AudioParameterFloatAttributes().withLabel (unit).withStringFromValueFunction ([dec] (float v, int) { return String (v, dec); })));
    };
    auto bln = [&] (const char* id, const String& name, bool def) { l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    flt (ids::inGain, "Input Gain", NormalisableRange<float> (-18.f, 14.f, 0.f), 0.f, "dB", 1);
    StringArray lineNames; for (int i = 0; i < n1081::kLineGainSteps; ++i) lineNames.add (signedDb (n1081::lineGainDb (i)) + " dB");
    choice (ids::line, "Line Gain", lineNames, 3);
    StringArray hp { "Off" }; hp.addArray (freqNames (n1081::kHpfFreqs, 5));
    StringArray lp { "Off" }; lp.addArray (freqNames (n1081::kLpfFreqs, 5));
    choice (ids::hpf, "High Pass", hp, 0);
    choice (ids::lpf, "Low Pass", lp, 0);
    const double* tabs[4] = { n1081::kLfFreqs, n1081::kLmfFreqs, n1081::kHmfFreqs, n1081::kHfFreqs };
    const int counts[4] = { 5, 10, 10, 5 }, defs[4] = { 2, 4, 5, 3 };
    const char* names[4] = { "LF", "Low Mid", "High Mid", "HF" };
    const char* altNames[4] = { "LF Peak", "Low Mid Hi-Q", "High Mid Hi-Q", "HF Peak" };
    for (int b = 0; b < 4; ++b) {
        choice (ids::freq[b], String (names[b]) + " Freq", freqNames (tabs[b], counts[b]), defs[b]);
        flt (ids::gain[b], String (names[b]) + " Gain", NormalisableRange<float> (-18.f, 18.f, 0.f), 0.f, "dB", 1);
        bln (ids::alt[b], altNames[b], false);
    }
    bln (ids::eqIn, "EQ In", true);
    bln (ids::phase, "Phase", false);
    flt (ids::out, "Output", NormalisableRange<float> (-24.f, 12.f, 0.f), 0.f, "dB", 1);

    // compressor
    choice (ids::cModel, "Comp Model", { "2254", "VCA", "FET", "Opto" }, 0);
    bln (ids::cIn, "Comp In", false);
    choice (ids::cOrder, "Comp Order", { "EQ > Comp", "Comp > EQ" }, 0);
    flt (ids::cMix, "Comp Mix", NormalisableRange<float> (0.f, 100.f, 0.f), 100.f, "%", 0);
    StringArray sc { "Off" }; for (int i = 1; i < 6; ++i) sc.add (String ((int) n1081::kScHpf[i]) + " Hz");
    choice (ids::cSc, "Comp SC HPF", sc, 0);
    StringArray thr; for (int i = 0; i < 16; ++i) thr.add (signedDb (n1081::compThrDbu (i)) + " dBu");
    choice (ids::kThr, "2254 Threshold", thr, 8);
    choice (ids::kRatio, "2254 Ratio", { "1.5:1", "2:1", "3:1", "4:1", "6:1" }, 2);
    choice (ids::kRec, "2254 Recovery", { "400 ms", "800 ms", "1.5 s", "Auto" }, 1);
    StringArray mk; for (int i = 0; i < 11; ++i) mk.add (signedDb (n1081::compGainDb (i)) + " dB");
    choice (ids::kGain, "2254 Gain", mk, 0);
    bln (ids::kFast, "2254 Fast Attack", false);
    bln (ids::kComp, "2254 Compress", true);
    bln (ids::kLim, "2254 Limit", false);
    StringArray ll; for (int i = 0; i < 9; ++i) ll.add (signedDb (n1081::limThrDbu (i)) + " dBu");
    choice (ids::kLimLvl, "2254 Limit Level", ll, 4);
    choice (ids::kLimRec, "2254 Limit Recovery", { "100 ms", "200 ms", "800 ms", "Auto" }, 1);
    // PlateVerb compressor: same ranges and feel as in PlateVerb (threshold linear -80 ... 0 dB)
    flt (ids::pThr, "Comp Threshold", NormalisableRange<float> (-80.f, 0.f, 0.f), -30.f, "dB", 1);
    { NormalisableRange<float> r (1.f, 20.f, 0.f); r.setSkewForCentre (4.f); flt (ids::pRatio, "Comp Ratio", r, 4.f, ":1", 1); }
    { NormalisableRange<float> r (0.02f, 300.f, 0.f); r.setSkewForCentre (10.f); flt (ids::pAtk, "Comp Attack", r, 10.f, "ms", 2); }
    { NormalisableRange<float> r (20.f, 10000.f, 0.f); r.setSkewForCentre (500.f); flt (ids::pRel, "Comp Release", r, 300.f, "ms", 0); }
    flt (ids::pMk, "Comp Makeup", NormalisableRange<float> (0.f, 24.f, 0.f), 0.f, "dB", 1);
    choice (ids::pTone, "Comp Tone", { "Clean", "Vintage" }, 0);

    // settings (not automatable)
    choice (ids::os, "Oversampling", { "Auto", "Off", "2x", "4x", "6x", "8x" }, 0, false);
    choice (ids::osFilt, "Oversampling Filter", { "Low latency", "Linear phase" }, 0, false);
    return l;
}

//==============================================================================
class N1081Processor : public AudioProcessor
{
public:
    N1081Processor()
        : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                           .withOutput ("Output", AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "N1081", makeLayout())
    {
        auto p = [this] (const char* id) { return apvts.getRawParameterValue (id); };
        pLine = p (ids::line); pOut = p (ids::out); pIn = p (ids::inGain); pHpf = p (ids::hpf); pLpf = p (ids::lpf);
        pEq = p (ids::eqIn); pPhase = p (ids::phase);
        for (int b = 0; b < 4; ++b) { pFreq[b] = p (ids::freq[b]); pGain[b] = p (ids::gain[b]); pAlt[b] = p (ids::alt[b]); }
        cModel = p (ids::cModel); cIn = p (ids::cIn); cOrder = p (ids::cOrder); cMix = p (ids::cMix); cSc = p (ids::cSc);
        kThr = p (ids::kThr); kRatio = p (ids::kRatio); kRec = p (ids::kRec); kGain = p (ids::kGain); kFast = p (ids::kFast);
        kComp = p (ids::kComp); kLim = p (ids::kLim); kLimLvl = p (ids::kLimLvl); kLimRec = p (ids::kLimRec);
        qThr = p (ids::pThr); qRatio = p (ids::pRatio); qAtk = p (ids::pAtk); qRel = p (ids::pRel); qMk = p (ids::pMk); qTone = p (ids::pTone);
        pOs = p (ids::os); pOsFilt = p (ids::osFilt);
    }

    const String getName() const override { return "N1081"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const String getProgramName (int) override { return {}; }
    void changeProgramName (int, const String&) override {}
    bool supportsDoublePrecisionProcessing() const override { return true; }

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        const auto& o = l.getMainOutputChannelSet();
        return (o == AudioChannelSet::mono() || o == AudioChannelSet::stereo()) && o == l.getMainInputChannelSet();
    }

    // { factor, linear phase }
    std::pair<int, bool> osChoice (double sr) const
    {
        const int sel = (int) pOs->load();
        const bool lin = pOsFilt->load() > 0.5f;
        switch (sel) {
            case 1: return { 1, false };
            case 2: return { 2, lin };
            case 3: return { 4, lin };
            case 4: return { 6, true };            // 6x exists only as linear phase
            case 5: return { 8, lin };
            default: return { sr >= 88000.0 ? 2 : 4, lin };   // Auto: circuit runs at 176.4 / 192 kHz
        }
    }

    void prepareToPlay (double sr, int block) override
    {
        baseRate = sr;
        maxBlock = jmax (1, block);
        for (int i = 0; i < 3; ++i) {
            iir[i] = std::make_unique<dsp::Oversampling<double>> ((size_t) 2, (size_t) (i + 1),
                         dsp::Oversampling<double>::filterHalfBandPolyphaseIIR, true, false);
            iir[i]->initProcessing ((size_t) maxBlock);
        }
        const int firF[4] = { 2, 4, 6, 8 };
        for (int i = 0; i < 4; ++i) fir[i].prepare (firF[i], maxBlock, 2);
        work.setSize (2, maxBlock);
        curFactor = -1;
        applyOsSetting (true);
        lineSm.reset (sr, 0.03); lineSm.setCurrentAndTargetValue ((float) Decibels::decibelsToGain (n1081::lineGainDb ((int) pLine->load())));
        outSm.reset (sr, 0.03);  outSm.setCurrentAndTargetValue ((float) Decibels::decibelsToGain (pOut->load()));
        inSm.reset (sr, 0.03);   inSm.setCurrentAndTargetValue ((float) Decibels::decibelsToGain (pIn->load()));
    }
    void releaseResources() override {}

    void processBlock (AudioBuffer<float>& b, MidiBuffer&) override  { processAny (b); }
    void processBlock (AudioBuffer<double>& b, MidiBuffer&) override { processAny (b); }

    n1081::Params currentParams() const
    {
        n1081::Params p;
        p.lineStep = (int) pLine->load(); p.outputDb = pOut->load();
        p.eqIn = pEq->load() > 0.5f; p.phase = pPhase->load() > 0.5f;
        p.hpf = (int) pHpf->load(); p.lpf = (int) pLpf->load();
        for (int b = 0; b < 4; ++b) { p.freqIdx[b] = (int) pFreq[b]->load(); p.gainDb[b] = pGain[b]->load(); p.alt[b] = pAlt[b]->load() > 0.5f; }
        return p;
    }

    bool hasEditor() const override { return true; }
    AudioProcessorEditor* createEditor() override;

    void getStateInformation (MemoryBlock& d) override
    {
        auto st = apvts.copyState();
        st.setProperty ("uiScale", uiScale, nullptr);
        st.setProperty ("compOpen", compOpen, nullptr);
        st.setProperty ("vuMode", vuMode, nullptr);
        if (auto x = st.createXml()) copyXmlToBinary (*x, d);
    }
    void setStateInformation (const void* d, int size) override
    {
        if (auto x = getXmlFromBinary (d, size)) {
            auto st = ValueTree::fromXml (*x);
            if (st.hasType (apvts.state.getType())) {
                uiScale = (float) (double) st.getProperty ("uiScale", 1.0);
                compOpen = (bool) st.getProperty ("compOpen", false);
                vuMode = (int) st.getProperty ("vuMode", 2);
                apvts.replaceState (st);
            }
        }
    }

    AudioProcessorValueTreeState apvts;
    std::atomic<float> meter { 0 }, vuIn { 0 }, vuOut { 0 }, vuGr { 0 };
    std::atomic<int> activeFactor { 1 };
    float uiScale = 1.0f; bool compOpen = false; int vuMode = 2;   // VU: 0 in, 1 out, 2 gain reduction

private:
    struct PvUnit {
        pv::Compressor c; pv::Biquad sc[2]; float fs = 96000; double scHz = -1;
        void prepare (double f) { fs = (float) f; c.fs = fs; c.lastFs = -1; c.reset(); for (auto& b : sc) b.reset(); scHz = -1; }
    };

    void applyOsSetting (bool force)
    {
        const auto ch0 = osChoice (baseRate);
        const int f = ch0.first; const bool lin = ch0.second;
        if (! force && f == curFactor && lin == curLinear) return;
        curFactor = f; curLinear = lin;
        osRate = baseRate * f;
        for (auto& c : ch) c.prepare (osRate);
        comp.prepare (osRate);
        pv.prepare (osRate);
        for (int i = 0; i < 4; ++i) fir[i].reset();
        for (auto& o : iir) if (o) o->reset();
        int lat = 0;
        if (f > 1) lat = lin ? fir[firIndex (f)].latency() : (int) std::round (iir[iirIndex (f)]->getLatencyInSamples());
        setLatencySamples (lat);
        for (int b = 0; b < 4; ++b) { gainSm[b].reset (osRate, 0.03); gainSm[b].setCurrentAndTargetValue (pEq->load() > 0.5f ? pGain[b]->load() : 0.f); }
        lastHpf = lastLpf = -1; for (auto& k : lastKey) k = -1;
        updateCoefs (true);
        activeFactor = f;
    }
    static int firIndex (int f) { return f == 2 ? 0 : f == 4 ? 1 : f == 6 ? 2 : 3; }
    static int iirIndex (int f) { return f == 2 ? 0 : f == 4 ? 1 : 2; }

    template <typename T>
    void processAny (AudioBuffer<T>& whole)
    {
        ScopedNoDenormals nd;
        applyOsSetting (false);
        const int total = whole.getNumSamples();
        for (int start = 0; start < total; start += maxBlock)
            processChunk (whole, start, jmin (maxBlock, total - start));
    }

    template <typename T>
    void processChunk (AudioBuffer<T>& buf, int start, int n)
    {
        const int nCh = jmin (2, buf.getNumChannels());
        for (int c = nCh; c < getTotalNumOutputChannels(); ++c) buf.clear (c, start, n);
        if (nCh == 0) return;

        // clean input gain, 64-bit from here on
        inSm.setTargetValue ((float) Decibels::decibelsToGain (pIn->load()));
        for (int i = 0; i < n; ++i) {
            const double g = inSm.getNextValue();
            for (int c = 0; c < 2; ++c) work.setSample (c, i, (double) buf.getSample (jmin (c, nCh - 1), start + i) * g);
        }

        lineSm.setTargetValue ((float) Decibels::decibelsToGain (n1081::lineGainDb ((int) pLine->load())));
        outSm.setTargetValue ((float) Decibels::decibelsToGain (pOut->load()));
        for (int b = 0; b < 4; ++b) gainSm[b].setTargetValue (pEq->load() > 0.5f ? pGain[b]->load() : 0.f);
        const int hpf = (int) pHpf->load(), lpf = (int) pLpf->load();
        if (hpf != lastHpf || lpf != lastLpf) { for (auto& c : ch) c.setFilters (hpf, lpf); lastHpf = hpf; lastLpf = lpf; }

        // up
        const int f = curFactor;
        double* hi[2] {};
        int m = n;
        dsp::AudioBlock<double> wblock (work.getArrayOfWritePointers(), 2, (size_t) n);
        if (f == 1) { hi[0] = work.getWritePointer (0); hi[1] = work.getWritePointer (1); }
        else if (curLinear) {
            auto& o = fir[firIndex (f)];
            for (int c = 0; c < 2; ++c) { o.up (c, work.getReadPointer (c), n); hi[c] = o.channel (c); }
            m = n * f;
        } else {
            auto up = iir[iirIndex (f)]->processSamplesUp (wblock);
            hi[0] = up.getChannelPointer (0); hi[1] = up.getChannelPointer (1);
            m = (int) up.getNumSamples();
        }

        // circuit
        const float lineStart = lineSm.getCurrentValue(); lineSm.skip (n); const float lineEnd = lineSm.getCurrentValue();
        const bool compOn = cIn->load() > 0.5f, compFirst = cOrder->load() > 0.5f;
        const int model = (int) cModel->load();
        if (model != lastModel) { comp.reset(); pv.c.reset(); lastModel = model; }
        const n1081::CompParams2254 kp = params2254();
        const pv::CompParams pp = paramsPv();
        updatePvSc();
        double sIn = 0, sOut = 0, grMax = 0;

        constexpr int sub = 32;
        for (int s0 = 0; s0 < m; s0 += sub) {
            const int len = jmin (sub, m - s0);
            updateCoefs (false, len);
            for (int i = 0; i < len; ++i) {
                const int k = s0 + i;
                const double lg = lineStart + (lineEnd - lineStart) * (double) k / (double) m;
                double L = hi[0][k], R = hi[1][k];
                if (! compFirst) { L = ch[0].process (L, lg, hpf, lpf); R = ch[1].process (R, lg, hpf, lpf); }
                if (compOn) {
                    sIn += L * L + R * R;
                    double gr = 0;
                    if (model == 0) { comp.process (L, R, kp); gr = comp.gr; }
                    else {
                        float fl = (float) L, fr = (float) R, kl = fl, kr = fr;
                        if (pv.scHz > 0) { kl = pv.sc[0].process (kl); kr = pv.sc[1].process (kr); }
                        pv.c.setTimes (pp.atkMs, pp.relMs);
                        pv.c.process (fl, fr, kl, kr, pp);
                        L = fl; R = fr; gr = pv.c.reductionDb();
                    }
                    grMax = jmax (grMax, gr);
                    sOut += L * L + R * R;
                }
                if (compFirst) { L = ch[0].process (L, lg, hpf, lpf); R = ch[1].process (R, lg, hpf, lpf); }
                hi[0][k] = L; hi[1][k] = R;
            }
        }

        // down
        if (f > 1) {
            if (curLinear) { auto& o = fir[firIndex (f)]; for (int c = 0; c < 2; ++c) o.down (c, work.getWritePointer (c), n); }
            else iir[iirIndex (f)]->processSamplesDown (wblock);
        }

        const bool flip = pPhase->load() > 0.5f;
        float peak = 0;
        for (int i = 0; i < n; ++i) {
            const double g = outSm.getNextValue() * (flip ? -1.0 : 1.0);
            for (int c = 0; c < nCh; ++c) {
                const double v = work.getSample (c, i) * g;
                buf.setSample (c, start + i, (T) v);
                peak = jmax (peak, (float) std::abs (v));
            }
        }
        meter.store (jmax (peak, meter.load() * 0.85f));
        vuIn.store (compOn ? (float) std::sqrt (sIn / (2.0 * m)) : 0.f);
        vuOut.store (compOn ? (float) std::sqrt (sOut / (2.0 * m)) : 0.f);
        vuGr.store (compOn ? (float) grMax : 0.f);
    }

    n1081::CompParams2254 params2254() const
    {
        n1081::CompParams2254 p;
        p.compOn = kComp->load() > 0.5f; p.limOn = kLim->load() > 0.5f; p.fastAtk = kFast->load() > 0.5f;
        p.thr = (int) kThr->load(); p.ratio = (int) kRatio->load(); p.rec = (int) kRec->load(); p.gain = (int) kGain->load();
        p.lim = (int) kLimLvl->load(); p.limRec = (int) kLimRec->load(); p.scHpf = (int) cSc->load();
        p.mix = cMix->load() * 0.01;
        return p;
    }
    pv::CompParams paramsPv() const
    {
        pv::CompParams p; p.on = true; p.duck = false;
        p.type = jlimit (0, 2, (int) cModel->load() - 1);
        p.vintage = qTone->load() > 0.5f;
        p.thr = qThr->load(); p.ratio = qRatio->load(); p.atkMs = qAtk->load(); p.relMs = qRel->load();
        p.makeup = qMk->load(); p.mix = cMix->load() * 0.01f;
        return p;
    }
    void updatePvSc()
    {
        const int s = (int) cSc->load();
        const double hzv = s > 0 ? n1081::kScHpf[s] : 0.0;
        if (hzv != pv.scHz) { pv.scHz = hzv; if (hzv > 0) for (auto& b : pv.sc) b.highpass (pv.fs, (float) hzv, 0.707f); }
    }

    void updateCoefs (bool force, int advance = 0)
    {
        n1081::Params p = currentParams();
        for (int b = 0; b < 4; ++b) {
            if (advance > 0) gainSm[b].skip (advance);
            p.gainDb[b] = gainSm[b].getCurrentValue();
            const int key = p.freqIdx[b] * 2 + (p.alt[b] ? 1 : 0);
            if (force || gainSm[b].isSmoothing() || key != lastKey[b] || std::abs (p.gainDb[b] - lastGain[b]) > 1e-4f) {
                const auto s = n1081::bandSpec (p, b);
                for (auto& c : ch) c.setBand (b, s, true);   // EQ in/out is a smoothed fade of the band gains
                lastKey[b] = key; lastGain[b] = (float) p.gainDb[b];
            }
        }
    }

    std::atomic<float>* pLine {}, * pOut {}, * pIn {}, * pHpf {}, * pLpf {}, * pEq {}, * pPhase {};
    std::atomic<float>* pFreq[4] {}, * pGain[4] {}, * pAlt[4] {};
    std::atomic<float>* cModel {}, * cIn {}, * cOrder {}, * cMix {}, * cSc {};
    std::atomic<float>* kThr {}, * kRatio {}, * kRec {}, * kGain {}, * kFast {}, * kComp {}, * kLim {}, * kLimLvl {}, * kLimRec {};
    std::atomic<float>* qThr {}, * qRatio {}, * qAtk {}, * qRel {}, * qMk {}, * qTone {};
    std::atomic<float>* pOs {}, * pOsFilt {};

    std::unique_ptr<dsp::Oversampling<double>> iir[3];
    n1081::FirOversampler fir[4];
    AudioBuffer<double> work;
    int maxBlock = 512, curFactor = -1, lastModel = -1; bool curLinear = false;
    double baseRate = 48000, osRate = 192000;
    n1081::Channel ch[2];
    n1081::Comp2254 comp;
    PvUnit pv;
    SmoothedValue<float> gainSm[4], lineSm, outSm, inSm;
    int lastHpf = -1, lastLpf = -1, lastKey[4] { -1, -1, -1, -1 };
    float lastGain[4] { 99, 99, 99, 99 };
};

//==============================================================================
namespace col {
    static const Colour panel  (0xff4f5b66), panelDark (0xff3c4650), well (0xff2b3239);
    static const Colour comp   (0xff46525c), compDark (0xff353f48);
    static const Colour legend (0xffece6d6), dim (0xffa9b0b4);
    static const Colour capRed (0xffb8322c), capBlue (0xff2f5d8c), capGrey (0xff8c9196), capBlack (0xff23272b);
    static const Colour lampOn (0xffffb43c), curve (0xffffc864), vuFace (0xfff1e6c8);
}

class KnobLnF : public LookAndFeel_V4
{
public:
    KnobLnF()
    {
        setColour (PopupMenu::backgroundColourId, col::well);
        setColour (PopupMenu::textColourId, col::legend);
        setColour (PopupMenu::highlightedBackgroundColourId, col::capBlue);
        setColour (PopupMenu::headerTextColourId, col::dim);
        setColour (ComboBox::backgroundColourId, col::well);
        setColour (ComboBox::textColourId, col::legend);
        setColour (ComboBox::outlineColourId, Colours::transparentBlack);
        setColour (ComboBox::arrowColourId, col::legend);
    }
    void drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s) override
    {
        const auto cap = s.findColour (Slider::rotarySliderFillColourId);
        const bool stepped = s.getProperties()["stepped"];
        auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3);
        const float d = jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (d, d);
        const auto c = r.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        const int ticks = stepped ? jmin (17, (int) std::round (s.getMaximum() - s.getMinimum()) + 1) : 11;
        g.setColour (col::dim);
        for (int i = 0; i < ticks; ++i) {
            const float t = a0 + (a1 - a0) * (float) i / (float) jmax (1, ticks - 1);
            g.drawLine ({ c.getPointOnCircumference (d * 0.5f, t), c.getPointOnCircumference (d * 0.44f, t) }, 1.2f);
        }
        auto body = r.reduced (d * 0.11f);
        g.setColour (Colours::black.withAlpha (0.45f)); g.fillEllipse (body.translated (0, d * 0.03f));
        g.setGradientFill (ColourGradient (col::capBlack.brighter (0.4f), body.getTopLeft(), col::capBlack, body.getBottomRight(), false));
        g.fillEllipse (body);
        auto top = body.reduced (d * 0.085f);
        g.setGradientFill (ColourGradient (cap.brighter (0.35f), top.getTopLeft(), cap.darker (0.3f), top.getBottomRight(), false));
        g.fillEllipse (top);
        g.setColour (col::legend);
        g.drawLine ({ c.getPointOnCircumference (d * 0.08f, ang), c.getPointOnCircumference (body.getWidth() * 0.5f - 1, ang) }, 2.4f);
    }
    void drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        if ((bool) b.getProperties()["flat"]) {
            g.setColour (col::well.withAlpha (over ? 0.95f : 0.75f)); g.fillRoundedRectangle (r, 4);
            if (b.getToggleState()) { g.setColour (col::lampOn); g.drawRoundedRectangle (r, 4, 1.2f); }
            return;
        }
        const bool on = b.getToggleState();
        g.setColour (col::well); g.fillRoundedRectangle (r, 3);
        auto face = r.reduced (2.0f);
        g.setGradientFill (ColourGradient (on ? col::lampOn : Colour (0xffd9d4c6), face.getTopLeft(),
                                           on ? col::lampOn.darker (0.35f) : Colour (0xff9e998c), face.getBottomLeft(), false));
        g.fillRoundedRectangle (face, 2);
        if (over || down) { g.setColour (Colours::white.withAlpha (down ? 0.25f : 0.1f)); g.fillRoundedRectangle (face, 2); }
    }
    void drawButtonText (Graphics& g, TextButton& b, bool, bool) override
    {
        const bool flat = b.getProperties()["flat"];
        g.setColour (flat ? col::legend : Colour (0xff23272b));
        g.setFont (FontOptions (flat ? 12.0f : 11.0f, Font::bold));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds(), Justification::centred, 1);
    }
    void drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox&) override
    {
        auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (col::well); g.fillRoundedRectangle (r, 4);
        Path p; const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
        p.startNewSubPath (ax - 4, ay - 2); p.lineTo (ax, ay + 2); p.lineTo (ax + 4, ay - 2);
        g.setColour (col::legend); g.strokePath (p, PathStrokeType (1.6f));
    }
    Font getComboBoxFont (ComboBox&) override { return FontOptions (12.5f, Font::bold); }
    void positionComboBoxText (ComboBox& b, Label& l) override { l.setBounds (6, 0, b.getWidth() - 22, b.getHeight()); l.setFont (getComboBoxFont (b)); }
};

// PlateVerb-style knob: smooth linear drag, Alt/Option = ten times finer, gentle wheel
class FineKnob : public Slider
{
public:
    FineKnob() : Slider (Slider::RotaryHorizontalVerticalDrag, Slider::TextBoxBelow)
    {
        setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
        setVelocityBasedMode (false);
        setMouseDragSensitivity (320);
    }
    void mouseDown (const MouseEvent& e) override { Slider::mouseDown (e); last = e.position; prop = valueToProportionOfLength (getValue()); }
    void mouseDrag (const MouseEvent& e) override
    {
        if (e.mods.isAltDown()) {
            const auto d = e.position - last; last = e.position;
            prop = jlimit (0.0, 1.0, prop + (double) (d.x - d.y) / 3200.0);
            setValue (proportionOfLengthToValue (prop), sendNotificationSync);
            return;
        }
        Slider::mouseDrag (e); last = e.position; prop = valueToProportionOfLength (getValue());
    }
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        auto w2 = w; const float k = e.mods.isAltDown() ? 0.1f : 0.5f; w2.deltaY *= k; w2.deltaX *= k; Slider::mouseWheelMove (e, w2);
    }
private:
    Point<float> last; double prop = 0.0;
};

//==============================================================================
class Graph : public Component
{
public:
    explicit Graph (N1081Processor& p) : proc (p) {}
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::well); g.fillRoundedRectangle (r, 5);
        auto mr = r.removeFromRight (10).reduced (2, 6);
        r = r.reduced (8, 6);
        auto xOf = [&] (double f) { return r.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * r.getWidth(); };
        auto yOf = [&] (double db) { return r.getCentreY() - (float) (db / 24.0) * r.getHeight() * 0.5f; };
        g.setFont (FontOptions (10.f));
        for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 }) {
            g.setColour (Colours::white.withAlpha (0.07f)); g.drawVerticalLine ((int) xOf (f), r.getY(), r.getBottom());
            g.setColour (col::dim.withAlpha (0.7f)); g.drawText (hz (f), (int) xOf (f) + 2, (int) r.getBottom() - 12, 30, 12, Justification::left);
        }
        for (double db : { -18.0, -12.0, -6.0, 6.0, 12.0, 18.0 }) {
            g.setColour (Colours::white.withAlpha (0.05f)); g.drawHorizontalLine ((int) yOf (db), r.getX(), r.getRight());
            g.setColour (col::dim.withAlpha (0.6f)); g.drawText (String ((int) db), (int) r.getX() + 2, (int) yOf (db) - 6, 24, 12, Justification::left);
        }
        g.setColour (Colours::white.withAlpha (0.18f)); g.drawHorizontalLine ((int) yOf (0), r.getX(), r.getRight());
        const auto p = proc.currentParams();
        Path path; const int N = (int) r.getWidth();
        for (int i = 0; i <= N; ++i) {
            const double f = 20.0 * std::pow (1000.0, (double) i / N);
            const float y = jlimit (r.getY(), r.getBottom(), yOf (n1081::responseDb (p, f)));
            if (i == 0) path.startNewSubPath (xOf (f), y); else path.lineTo (xOf (f), y);
        }
        Path fill (path); fill.lineTo (r.getRight(), yOf (0)); fill.lineTo (r.getX(), yOf (0)); fill.closeSubPath();
        g.setColour (col::curve.withAlpha (p.eqIn ? 0.12f : 0.04f)); g.fillPath (fill);
        g.setColour (p.eqIn ? col::curve : col::curve.withAlpha (0.35f)); g.strokePath (path, PathStrokeType (2.f));
        const float db = (float) Decibels::gainToDecibels (proc.meter.load(), -60.0f);
        const float frac = jlimit (0.f, 1.f, (db + 60.f) / 60.f);
        g.setColour (Colours::black.withAlpha (0.4f)); g.fillRect (mr);
        g.setColour (db > -1.f ? Colours::red : db > -12.f ? col::lampOn : Colour (0xff7fc97f));
        g.fillRect (mr.withTop (mr.getBottom() - mr.getHeight() * frac));
    }
private:
    N1081Processor& proc;
};

// small analog VU: IN / OUT read 0 VU = +4 dBu; GR reads gain reduction down from the 0 mark
class VuMeter : public Component
{
public:
    explicit VuMeter (N1081Processor& p) : proc (p) {}
    void tick()
    {
        double target;
        if (proc.vuMode == 2) target = -proc.vuGr.load();
        else {
            const double rms = proc.vuMode == 0 ? proc.vuIn.load() : proc.vuOut.load();
            target = 20.0 * std::log10 (rms + 1e-9) + n1081::kDbuOffset - 4.0;
        }
        target = jlimit (-22.0, 4.0, target);
        needle += (target - needle) * 0.16;              // ~300 ms VU ballistics at 30 Hz
        repaint();
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::well); g.fillRoundedRectangle (r, 5);
        auto face = r.reduced (4);
        g.setColour (col::vuFace); g.fillRoundedRectangle (face, 3);
        const Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.55f);
        const float rad = face.getHeight() * 1.18f;
        auto angOf = [] (double vu) { const double t = (vu + 20.0) / 23.0; return (float) (-0.78 + 1.56 * jlimit (0.0, 1.0, t)); };
        Path arc; arc.addCentredArc (pivot.x, pivot.y, rad, rad, 0, angOf (-20), angOf (0), true);
        g.setColour (Colour (0xff2a2a2a)); g.strokePath (arc, PathStrokeType (1.2f));
        Path red; red.addCentredArc (pivot.x, pivot.y, rad, rad, 0, angOf (0), angOf (3), true);
        g.setColour (Colour (0xffc0392b)); g.strokePath (red, PathStrokeType (2.5f));
        g.setFont (FontOptions (9.5f, Font::bold));
        for (double v : { -20.0, -10.0, -7.0, -5.0, -3.0, -1.0, 0.0, 1.0, 2.0, 3.0 }) {
            const float a = angOf (v);
            g.setColour (v > 0 ? Colour (0xffc0392b) : Colour (0xff2a2a2a));
            g.drawLine ({ pivot.getPointOnCircumference (rad, a), pivot.getPointOnCircumference (rad + 5, a) }, 1.1f);
            if (v == -20 || v == -10 || v == -5 || v == -3 || v == 0 || v == 3) {
                const String txt = proc.vuMode == 2 ? (v > 0 ? String() : String ((int) v)) : String ((int) std::abs (v));
                g.drawText (txt, Rectangle<float> (24, 11).withCentre (pivot.getPointOnCircumference (rad + 12, a)), Justification::centred);
            }
        }
        g.setColour (Colour (0xff555555)); g.setFont (FontOptions (9.5f));
        g.drawText (proc.vuMode == 2 ? "GR dB" : "VU", face.withTrimmedBottom (3), Justification::centredBottom);
        g.saveState(); g.reduceClipRegion (face.toNearestInt());
        g.setColour (Colour (0xff1a1a1a));
        g.drawLine ({ pivot, pivot.getPointOnCircumference (rad + 6, angOf (needle)) }, 1.4f);
        g.restoreState();
    }
private:
    N1081Processor& proc; double needle = -22;
};

//==============================================================================
struct Caption { String text; Rectangle<int> r; };

static void styleKnob (FineKnob& s, Colour cap, bool stepped)
{
    s.setTextBoxStyle (Slider::TextBoxBelow, false, 76, 16);
    s.setColour (Slider::rotarySliderFillColourId, cap);
    s.setColour (Slider::textBoxTextColourId, col::legend);
    s.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    s.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    s.getProperties().set ("stepped", stepped);
    if (stepped) s.setMouseDragSensitivity (220);
}

class EqPanel : public Component
{
public:
    static constexpr int kW = 840, kH = 446;
    EqPanel (N1081Processor& p, KnobLnF& l) : proc (p), graph (p)
    {
        setLookAndFeel (&l);
        addAndMakeVisible (graph);
        knob (inGain, ids::inGain, col::capGrey, false);
        knob (line, ids::line, col::capRed, true);
        knob (hpf, ids::hpf, col::capGrey, true);
        knob (lpf, ids::lpf, col::capGrey, true);
        const Colour caps[4] = { col::capBlue, col::capGrey, col::capGrey, col::capBlue };
        for (int b = 0; b < 4; ++b) {
            knob (freq[b], ids::freq[b], caps[b], true);
            knob (gain[b], ids::gain[b], col::capBlack.brighter (0.25f), false);
            button (alt[b], ids::alt[b], b == 0 || b == 3 ? "PEAK" : "HI-Q");
        }
        knob (out, ids::out, col::capGrey, false);
        for (auto* s : { &gain[0], &gain[1], &gain[2], &gain[3], &out, &inGain }) { s->textFromValueFunction = [] (double v) { return signedDb (v, 1) + " dB"; }; s->updateText(); }
        button (eqIn, ids::eqIn, "EQ");
        button (phase, ids::phase, "PH");
        setSize (kW, kH);
    }
    ~EqPanel() override { setLookAndFeel (nullptr); }

    Rectangle<int> toolArea() const { return { kW - 152, 14, 140, 112 }; }
    void refresh() { graph.repaint(); }

    void paint (Graphics& g) override
    {
        g.setGradientFill (ColourGradient (col::panel.brighter (0.06f), 0, 0, col::panelDark, 0, (float) kH, false));
        g.fillAll();
        for (auto pt : { Point<float> (12, 12), Point<float> (12, kH - 12.f) }) {
            g.setColour (Colour (0xff9aa1a6)); g.fillEllipse (Rectangle<float> (9, 9).withCentre (pt));
            g.setColour (Colour (0xff4a4f53)); g.drawLine (pt.x - 3, pt.y, pt.x + 3, pt.y, 1.3f);
        }
        g.setColour (col::legend); g.setFont (FontOptions (30.f, Font::bold)); g.drawText ("1081", 26, 30, 120, 34, Justification::left);
        g.setFont (FontOptions (11.f)); g.setColour (col::dim);
        g.drawText ("CHANNEL AMPLIFIER", 26, 68, 130, 14, Justification::left);
        g.drawText ("CIRCUIT MODEL", 26, 84, 130, 14, Justification::left);
        for (auto& s : sections) {
            g.setColour (Colours::black.withAlpha (0.18f)); g.fillRoundedRectangle (s.r.toFloat(), 6);
            g.setColour (col::legend); g.setFont (FontOptions (12.f, Font::bold));
            g.drawText (s.text, s.r.getX(), s.r.getY() + 5, s.r.getWidth(), 14, Justification::centred);
        }
        g.setFont (FontOptions (10.f, Font::bold)); g.setColour (col::dim);
        for (auto& c : captions) g.drawText (c.text, c.r, Justification::centred);
    }

    void resized() override
    {
        sections.clear(); captions.clear();
        graph.setBounds (156, 14, kW - 156 - 162, 112);
        const int top = 138, h = kH - top - 12;
        int x = 14;
        auto section = [&] (const String& t, int w) { Rectangle<int> r (x, top, w, h); sections.push_back ({ t, r }); x += w + 6; return r; };
        auto put = [] (Slider& s, int cx, int y, int size) { s.setBounds (cx - (size + 16) / 2, y, size + 16, size + 18); };

        auto r = section ("INPUT", 104);
        put (inGain, r.getCentreX(), r.getY() + 22, 54);  captions.push_back ({ "TRIM (CLEAN)", { r.getX(), r.getY() + 94, r.getWidth(), 12 } });
        put (line, r.getCentreX(), r.getY() + 112, 70);   captions.push_back ({ "LINE GAIN", { r.getX(), r.getY() + 200, r.getWidth(), 12 } });
        phase.setBounds (r.getCentreX() - 26, r.getY() + 226, 52, 26);
        captions.push_back ({ "PHASE", { r.getX(), r.getY() + 254, r.getWidth(), 12 } });

        r = section ("FILTERS", 96);
        put (hpf, r.getCentreX(), r.getY() + 26, 60); captions.push_back ({ "HIGH PASS", { r.getX(), r.getY() + 106, r.getWidth(), 12 } });
        put (lpf, r.getCentreX(), r.getY() + 136, 60); captions.push_back ({ "LOW PASS", { r.getX(), r.getY() + 216, r.getWidth(), 12 } });

        const char* titles[4] = { "LF", "LOW MID", "HIGH MID", "HF" };
        for (int b = 0; b < 4; ++b) {
            r = section (titles[b], 114);
            put (freq[b], r.getCentreX(), r.getY() + 20, 80);
            put (gain[b], r.getCentreX(), r.getY() + 122, 80);
            alt[b].setBounds (r.getCentreX() - 30, r.getY() + 228, 60, 26);
            captions.push_back ({ b == 0 || b == 3 ? "SHELF / PEAK" : "WIDE / HI-Q", { r.getX(), r.getY() + 256, r.getWidth(), 12 } });
        }
        r = section ("OUTPUT", kW - x - 14);
        put (out, r.getCentreX(), r.getY() + 26, 64); captions.push_back ({ "TRIM", { r.getX(), r.getY() + 110, r.getWidth(), 12 } });
        eqIn.setBounds (r.getCentreX() - 26, r.getY() + 226, 52, 26);
        captions.push_back ({ "EQ IN", { r.getX(), r.getY() + 254, r.getWidth(), 12 } });
    }

private:
    void knob (FineKnob& s, const char* id, Colour cap, bool stepped)
    {
        styleKnob (s, cap, stepped);
        addAndMakeVisible (s);
        attachments.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, s));
        if (auto* p = proc.apvts.getParameter (id)) s.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        s.onValueChange = [this] { graph.repaint(); };
    }
    void button (TextButton& b, const char* id, const String& text)
    {
        b.setButtonText (text); b.setClickingTogglesState (true);
        addAndMakeVisible (b);
        buttonAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, b));
    }
    N1081Processor& proc;
    Graph graph;
    FineKnob inGain, line, hpf, lpf, out, freq[4], gain[4];
    TextButton alt[4], eqIn, phase;
    std::vector<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>> attachments;
    std::vector<std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>> buttonAtt;
    std::vector<Caption> sections, captions;
};

//==============================================================================
// Side module: 2254 diode-bridge limiter/compressor, or PlateVerb's VCA / FET / Opto
class CompPanel : public Component
{
public:
    static constexpr int kW = 316;
    CompPanel (N1081Processor& p, KnobLnF& l) : proc (p), vu (p)
    {
        setLookAndFeel (&l);
        model.addItemList ({ "2254 diode bridge", "PlateVerb VCA", "PlateVerb FET", "PlateVerb Opto" }, 1);
        addAndMakeVisible (model);
        modelAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ids::cModel, model);
        model.onChange = [this] { layoutModel(); };
        button (compIn, ids::cIn, "IN");
        addAndMakeVisible (vu);
        const char* vuNames[3] = { "IN", "OUT", "GR" };
        for (int i = 0; i < 3; ++i) {
            vuBtn[i].setButtonText (vuNames[i]); vuBtn[i].setRadioGroupId (77); vuBtn[i].setClickingTogglesState (true);
            vuBtn[i].getProperties().set ("flat", true);
            vuBtn[i].setToggleState (proc.vuMode == i, dontSendNotification);
            vuBtn[i].onClick = [this, i] { if (vuBtn[i].getToggleState()) proc.vuMode = i; };
            addAndMakeVisible (vuBtn[i]);
        }
        knob (kLimLvl, ids::kLimLvl, col::capGrey, true);
        knob (kLimRec, ids::kLimRec, col::capGrey, true);
        button (kLim, ids::kLim, "LIMIT");
        knob (kThr, ids::kThr, col::capBlue, true);
        knob (kRatio, ids::kRatio, col::capGrey, true);
        knob (kRec, ids::kRec, col::capGrey, true);
        knob (kGain, ids::kGain, col::capRed, true);
        button (kComp, ids::kComp, "COMP");
        button (kFast, ids::kFast, "FAST");
        knob (pThr, ids::pThr, col::capBlue, false);
        knob (pRatio, ids::pRatio, col::capGrey, false);
        knob (pAtk, ids::pAtk, col::capGrey, false);
        knob (pRel, ids::pRel, col::capGrey, false);
        knob (pMk, ids::pMk, col::capRed, false);
        button (tone, ids::pTone, "VINTAGE");
        pThr.textFromValueFunction = [] (double v) { return String (v, 1) + " dB"; };
        pRatio.textFromValueFunction = [] (double v) { return String (v, 1) + ":1"; };
        pAtk.textFromValueFunction = [] (double v) { return v < 1 ? String (v * 1000.0, 0) + " us" : String (v, v < 10 ? 1 : 0) + " ms"; };
        pRel.textFromValueFunction = [] (double v) { return v < 1000 ? String (v, 0) + " ms" : String (v / 1000.0, 2) + " s"; };
        pMk.textFromValueFunction = [] (double v) { return signedDb (v, 1) + " dB"; };
        knob (sc, ids::cSc, col::capGrey, true);
        knob (mix, ids::cMix, col::capGrey, false);
        mix.textFromValueFunction = [] (double v) { return String ((int) std::round (v)) + " %"; };
        for (auto* s : { &pThr, &pRatio, &pAtk, &pRel, &pMk, &mix }) s->updateText();
        setSize (kW, EqPanel::kH);
        layoutModel();
    }
    ~CompPanel() override { setLookAndFeel (nullptr); }
    void tick() { vu.tick(); }

    void paint (Graphics& g) override
    {
        g.setGradientFill (ColourGradient (col::comp.brighter (0.05f), 0, 0, col::compDark, 0, (float) getHeight(), false));
        g.fillAll();
        g.setColour (Colours::black.withAlpha (0.35f)); g.drawVerticalLine (0, 0, (float) getHeight());
        for (auto pt : { Point<float> (kW - 12.f, 12), Point<float> (kW - 12.f, getHeight() - 12.f) }) {
            g.setColour (Colour (0xff9aa1a6)); g.fillEllipse (Rectangle<float> (9, 9).withCentre (pt));
            g.setColour (Colour (0xff4a4f53)); g.drawLine (pt.x - 3, pt.y, pt.x + 3, pt.y, 1.3f);
        }
        g.setColour (col::legend); g.setFont (FontOptions (20.f, Font::bold));
        g.drawText (is2254() ? "2254" : "COMP", 16, 12, 70, 26, Justification::left);
        for (auto& s : rows) {
            g.setColour (Colours::black.withAlpha (0.16f)); g.fillRoundedRectangle (s.r.toFloat(), 6);
            g.setColour (col::legend); g.setFont (FontOptions (11.f, Font::bold));
            g.drawText (s.text, s.r.getX() + 8, s.r.getY() + 4, 200, 13, Justification::left);
        }
        g.setFont (FontOptions (10.f, Font::bold)); g.setColour (col::dim);
        for (auto& c : captions) g.drawText (c.text, c.r, Justification::centred);
    }

    void layoutModel()
    {
        const bool k = is2254();
        for (auto* c : std::initializer_list<Component*> { &kLimLvl, &kLimRec, &kLim, &kThr, &kRatio, &kRec, &kGain, &kComp, &kFast }) c->setVisible (k);
        for (auto* c : std::initializer_list<Component*> { &pThr, &pRatio, &pAtk, &pRel, &pMk, &tone }) c->setVisible (! k);
        resized(); repaint();
    }

    void resized() override
    {
        rows.clear(); captions.clear();
        model.setBounds (78, 14, 160, 24);
        compIn.setBounds (kW - 70, 13, 50, 26);
        vu.setBounds (20, 48, 216, 96);
        for (int i = 0; i < 3; ++i) vuBtn[i].setBounds (246, 52 + i * 31, 50, 26);
        auto put = [this] (Slider& s, const String& cap, int cx, int y, int size) {
            s.setBounds (cx - (size + 22) / 2, y, size + 22, size + 18);
            captions.push_back ({ cap, { cx - 40, y + size + 18, 80, 12 } });
        };
        if (is2254()) {
            Rectangle<int> lim (12, 154, kW - 24, 104); rows.push_back ({ "LIMITER", lim });
            put (kLimLvl, "LEVEL", 64, lim.getY() + 18, 52);
            put (kLimRec, "RECOVERY", 148, lim.getY() + 18, 52);
            kLim.setBounds (218, lim.getY() + 36, 74, 26);
            Rectangle<int> cmp (12, 264, kW - 24, 172); rows.push_back ({ "COMPRESSOR", cmp });
            const int y1 = cmp.getY() + 18;
            put (kThr, "THRESHOLD", 50, y1, 48); put (kRatio, "RATIO", 120, y1, 48);
            put (kRec, "RECOVERY", 192, y1, 48); put (kGain, "GAIN", 262, y1, 48);
            const int y2 = cmp.getY() + 104;
            kComp.setBounds (22, y2 + 16, 62, 26); kFast.setBounds (90, y2 + 16, 58, 26);
            put (sc, "SC HPF", 196, y2, 38); put (mix, "MIX", 262, y2, 38);
        } else {
            Rectangle<int> a (12, 154, kW - 24, 282); rows.push_back ({ "PLATEVERB COMPRESSOR", a });
            const int y1 = a.getY() + 20;
            put (pThr, "THRESHOLD", 62, y1, 64); put (pRatio, "RATIO", 160, y1, 54); put (pAtk, "ATTACK", 252, y1, 54);
            const int y2 = a.getY() + 120;
            put (pRel, "RELEASE", 62, y2, 54); put (pMk, "MAKEUP", 160, y2, 54);
            tone.setBounds (218, y2 + 20, 74, 26);
            const int y3 = a.getY() + 206;
            put (sc, "SC HPF", 160, y3, 40); put (mix, "MIX", 252, y3, 40);
        }
    }

private:
    bool is2254() const { return model.getSelectedItemIndex() <= 0; }
    void knob (FineKnob& s, const char* id, Colour cap, bool stepped)
    {
        styleKnob (s, cap, stepped);
        addAndMakeVisible (s);
        attachments.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, s));
        if (auto* p = proc.apvts.getParameter (id)) s.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    }
    void button (TextButton& b, const char* id, const String& text)
    {
        b.setButtonText (text); b.setClickingTogglesState (true);
        addAndMakeVisible (b);
        buttonAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, b));
    }

    N1081Processor& proc;
    VuMeter vu;
    ComboBox model; std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> modelAtt;
    TextButton compIn, vuBtn[3], kLim, kComp, kFast, tone;
    FineKnob kLimLvl, kLimRec, kThr, kRatio, kRec, kGain, pThr, pRatio, pAtk, pRel, pMk, sc, mix;
    std::vector<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>> attachments;
    std::vector<std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>> buttonAtt;
    std::vector<Caption> rows, captions;
};

//==============================================================================
class N1081Editor : public AudioProcessorEditor, private Timer
{
public:
    explicit N1081Editor (N1081Processor& p) : AudioProcessorEditor (p), proc (p), eq (p, lnf), comp (p, lnf)
    {
        addAndMakeVisible (eq);
        addChildComponent (comp);
        for (auto* b : { &zoomOut, &zoomIn, &zoomPct, &settings, &compBtn, &orderBtn }) {
            b->getProperties().set ("flat", true); b->setLookAndFeel (&lnf); addAndMakeVisible (*b);
        }
        zoomOut.setButtonText ("-"); zoomIn.setButtonText ("+"); settings.setButtonText ("SETTINGS");
        zoomOut.onClick = [this] { setScale (scale - 0.1f); };
        zoomIn.onClick  = [this] { setScale (scale + 0.1f); };
        zoomPct.onClick = [this] { zoomMenu(); };
        settings.onClick = [this] { settingsMenu(); };
        compBtn.onClick = [this] { proc.compOpen = ! proc.compOpen; applySize(); };
        orderBtn.onClick = [this] {
            if (auto* prm = proc.apvts.getParameter (ids::cOrder)) prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
        };
        scale = jlimit (0.6f, 2.0f, proc.uiScale);
        setResizable (true, true);
        applySize();
        ready = true;
        timerCallback();
        startTimerHz (30);
    }
    ~N1081Editor() override { for (auto* b : { &zoomOut, &zoomIn, &zoomPct, &settings, &compBtn, &orderBtn }) b->setLookAndFeel (nullptr); }

    void resized() override
    {
        const float s = (float) getWidth() / (float) baseWidth();
        if (ready && std::abs (s - scale) > 0.004f) { scale = jlimit (0.6f, 2.0f, s); proc.uiScale = scale; }
        const auto t = AffineTransform::scale (scale);
        eq.setTransform (t);
        comp.setVisible (proc.compOpen);
        comp.setBounds (EqPanel::kW, 0, CompPanel::kW, EqPanel::kH);
        comp.setTransform (t);
        const auto a = eq.toolArea();
        auto place = [&] (Button& b, Rectangle<int> r) { b.setBounds (r); b.setTransform (t); };
        place (zoomOut, { a.getX(), a.getY(), 28, 24 });
        place (zoomPct, { a.getX() + 32, a.getY(), a.getWidth() - 64, 24 });
        place (zoomIn,  { a.getRight() - 28, a.getY(), 28, 24 });
        place (settings, { a.getX(), a.getY() + 30, a.getWidth(), 24 });
        place (compBtn,  { a.getX(), a.getY() + 60, a.getWidth(), 24 });
        place (orderBtn, { a.getX(), a.getY() + 88, a.getWidth(), 24 });
        zoomPct.setButtonText (String ((int) std::round (scale * 100.0f)) + "%");
    }

private:
    int baseWidth() const { return EqPanel::kW + (proc.compOpen ? CompPanel::kW : 0); }
    void applySize()
    {
        const int bw = baseWidth();
        if (auto* c = getConstrainer()) {
            c->setFixedAspectRatio ((double) bw / EqPanel::kH);
            c->setSizeLimits ((int) (bw * 0.6f), (int) (EqPanel::kH * 0.6f), bw * 2, EqPanel::kH * 2);
        }
        setSize ((int) std::round (bw * scale), (int) std::round (EqPanel::kH * scale));
        resized();
        timerCallback();
    }
    void setScale (float s)
    {
        scale = jlimit (0.6f, 2.0f, std::round (s * 10.0f) / 10.0f);
        proc.uiScale = scale;
        applySize();
    }
    void zoomMenu()
    {
        PopupMenu m; m.setLookAndFeel (&lnf);
        for (int pc : { 70, 80, 90, 100, 110, 120, 130, 150, 175, 200 }) m.addItem (pc, String (pc) + "%", true, (int) std::round (scale * 100) == pc);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&zoomPct), [this] (int r) { if (r > 0) setScale (r / 100.0f); });
    }
    void settingsMenu()
    {
        auto* os = proc.apvts.getParameter (ids::os);
        auto* filt = proc.apvts.getParameter (ids::osFilt);
        const int cur = roundToInt (os->convertFrom0to1 (os->getValue()));
        const bool lin = filt->getValue() > 0.5f;
        PopupMenu m; m.setLookAndFeel (&lnf);
        m.addSectionHeader ("OVERSAMPLING");
        const char* names[] = { "Auto  (4x at 44.1/48k, 2x at 88.2/96k)", "Off  (1x)", "2x", "4x", "6x  (always linear phase)", "8x" };
        for (int i = 0; i < 6; ++i) m.addItem (1 + i, names[i], true, cur == i);
        m.addSectionHeader ("FILTERS");
        m.addItem (20, "Low latency", cur != 1 && cur != 4, ! lin && cur != 4);
        m.addItem (21, "Linear phase  (64 samples latency)", cur != 1, lin || cur == 4);
        m.addSeparator();
        m.addItem (30, "Running at " + String (proc.activeFactor.load()) + "x, 64-bit processing", false, false);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&settings), [os, filt] (int r) {
            if (r >= 1 && r <= 6) os->setValueNotifyingHost (os->convertTo0to1 ((float) (r - 1)));
            if (r == 20) filt->setValueNotifyingHost (0.0f);
            if (r == 21) filt->setValueNotifyingHost (1.0f);
        });
    }
    void timerCallback() override
    {
        eq.refresh();
        if (proc.compOpen) comp.tick();
        const bool compFirst = proc.apvts.getRawParameterValue (ids::cOrder)->load() > 0.5f;
        const String o = compFirst ? "ORDER: COMP > EQ" : "ORDER: EQ > COMP";
        if (orderBtn.getButtonText() != o) orderBtn.setButtonText (o);
        const bool on = proc.apvts.getRawParameterValue (ids::cIn)->load() > 0.5f;
        const String c = String (on ? "COMP  (ON)" : "COMP  (OFF)") + (proc.compOpen ? "   <" : "   >");
        if (compBtn.getButtonText() != c) compBtn.setButtonText (c);
        compBtn.setToggleState (on, dontSendNotification);
    }

    N1081Processor& proc;
    KnobLnF lnf;
    EqPanel eq;
    CompPanel comp;
    TextButton zoomOut, zoomIn, zoomPct, settings, compBtn, orderBtn;
    float scale = 1.0f; bool ready = false;
};

AudioProcessorEditor* N1081Processor::createEditor() { return new N1081Editor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new N1081Processor(); }
