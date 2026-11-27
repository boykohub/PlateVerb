// 1081+ — 1081 channel amplifier, 2254 / VCA / 88R compressor and 88R gate, arrangeable left to right.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "N1081Engine.h"
#include "N1081Oversampler.h"
#include "FlexFonts.h"
#include "Engine.h"            // PlateVerb's compressor (pv::Compressor), unchanged — "Velvet" here

using namespace juce;

namespace ids {
    static const char* line = "line";   static const char* out = "output";  static const char* inGain = "ingain";
    static const char* hpf = "hpf";     static const char* lpf = "lpf";
    static const char* eqIn = "eqin";   static const char* phase = "phase";
    static const char* freq[4] = { "lf_freq", "lmf_freq", "hmf_freq", "hf_freq" };
    static const char* gain[4] = { "lf_gain", "lmf_gain", "hmf_gain", "hf_gain" };
    static const char* alt [4] = { "lf_peak", "lmf_hiq", "hmf_hiq", "hf_peak" };
    static const char* q   [4] = { "lf_q", "lmf_q", "hmf_q", "hf_q" };
    static const char* order = "order";
    static const char* cModel = "cmp_model"; static const char* cIn = "cmp_in"; static const char* cDrive = "cmp_drive";
    static const char* cMix = "cmp_mix"; static const char* cSc = "cmp_schpf";
    static const char* kThr = "c_thr"; static const char* kRatio = "c_ratio"; static const char* kRec = "c_rec";
    static const char* kGain = "c_gain"; static const char* kFast = "c_fast"; static const char* kComp = "c_comp";
    static const char* kLim = "c_lim"; static const char* kLimLvl = "c_limlvl"; static const char* kLimRec = "c_limrec";
    static const char* pThr = "p_thr"; static const char* pRatio = "p_ratio"; static const char* pAtk = "p_atk";
    static const char* pRel = "p_rel"; static const char* pMk = "p_makeup"; static const char* pTone = "p_tone";
    static const char* gIn = "g_in"; static const char* gThr = "g_thr"; static const char* gRange = "g_range";
    static const char* gRel = "g_rel"; static const char* gFast = "g_fast"; static const char* gHyst = "g_hyst"; static const char* gPad = "g_pad";
    static const char* rThr = "r_thr"; static const char* rPad = "r_pad"; static const char* rRatio = "r_ratio"; static const char* rFast = "r_fast";
    static const char* rAtk = "r_atk"; static const char* rRel = "r_rel"; static const char* rAuto = "r_auto"; static const char* rGain = "r_gain";
    static const char* os = "os"; static const char* osFilt = "osfilt";
    static const char* analog = "analog";
}

// module order: left to right on screen = signal order
enum Module { ModEq = 0, ModComp = 1, ModGate = 2 };
static const int kPerms[6][3] = { { 0, 1, 2 }, { 0, 2, 1 }, { 1, 0, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 2, 1, 0 } };
static int permIndex (const int o[3]) { for (int i = 0; i < 6; ++i) if (kPerms[i][0] == o[0] && kPerms[i][1] == o[1] && kPerms[i][2] == o[2]) return i; return 4; }

static String hz (double f) { return f >= 1000 ? String (f / 1000.0, f < 10000 && std::fmod (f, 1000.0) != 0 ? 1 : 0) + "k" : String ((int) f); }
static StringArray freqNames (const double* t, int n) { StringArray a; for (int i = 0; i < n; ++i) a.add (hz (t[i])); return a; }
static String signedDb (double v, int dec = 0) { return (v > 0.04 ? "+" : "") + String (v, dec); }
static String msText (double v) { return v < 1 ? String (v * 1000.0, 0) + " us" : v < 1000 ? String (v, v < 10 ? 1 : 0) + " ms" : String (v / 1000.0, 2) + " s"; }

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
    auto skewed = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi, 0.f); r.setSkewForCentre (centre); return r; };
    auto bln = [&] (const char* id, const String& name, bool def) { l.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    flt (ids::inGain, "1081 Input Trim", NormalisableRange<float> (-18.f, 14.f, 0.f), 0.f, "dB", 1);
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
        flt (ids::q[b], String (names[b]) + " Q", skewed (0.25f, 4.f, 1.f), 1.f, "x", 2);
    }
    bln (ids::eqIn, "EQ In", true);
    bln (ids::phase, "Phase", false);
    flt (ids::out, "1081 Output", NormalisableRange<float> (-24.f, 12.f, 0.f), 0.f, "dB", 1);
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::analog, 1 }, "1081 Analog", NormalisableRange<float> (-50.f, 50.f, 0.f), 0.f,
               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return signedDb (v, 0); })));
    choice (ids::order, "Module Order", { "EQ > Comp > Gate", "EQ > Gate > Comp", "Comp > EQ > Gate",
                                          "Comp > Gate > EQ", "Gate > EQ > Comp", "Gate > Comp > EQ" }, 1);

    // compressor
    choice (ids::cModel, "Comp Type", { "2254", "VCA", "88R" }, 0);          // old FET / Opto sessions (index 2 / 3) open on the 88R
    bln (ids::cIn, "Comp In", false);
    flt (ids::cDrive, "Comp Drive", NormalisableRange<float> (-18.f, 18.f, 0.f), 0.f, "dB", 1);
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
    // Velvet = PlateVerb's compressor: same ranges and feel as in PlateVerb (threshold linear -80 ... 0 dB)
    flt (ids::pThr, "Velvet Threshold", NormalisableRange<float> (-80.f, 0.f, 0.f), -30.f, "dB", 1);
    flt (ids::pRatio, "Velvet Ratio", skewed (1.f, 20.f, 4.f), 4.f, ":1", 1);
    flt (ids::pAtk, "Velvet Attack", skewed (0.02f, 300.f, 10.f), 10.f, "ms", 2);
    flt (ids::pRel, "Velvet Release", skewed (20.f, 10000.f, 500.f), 300.f, "ms", 0);
    flt (ids::pMk, "Velvet Makeup", NormalisableRange<float> (0.f, 24.f, 0.f), 0.f, "dB", 1);
    choice (ids::pTone, "Velvet Tone", { "Clean", "Vintage" }, 0);
    // 88R: the Neve 88R channel compressor's controls and ranges
    flt (ids::rThr, "88R Threshold", NormalisableRange<float> (-10.f, 20.f, 0.f), 0.f, "dBu", 1);
    bln (ids::rPad, "88R Threshold -20 dB", false);
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::rRatio, 1 }, "88R Ratio", skewed (1.f, 30.f, 4.f), 4.f,
               AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v >= 29.95f ? String ("inf") : String (v, 1) + ":1"; })));
    bln (ids::rFast, "88R Fast Attack", false);
    flt (ids::rAtk, "88R Attack", NormalisableRange<float> (3.f, 7.f, 0.f), 5.f, "ms", 1);
    flt (ids::rRel, "88R Release", skewed (10.f, 3000.f, 200.f), 300.f, "ms", 0);
    bln (ids::rAuto, "88R Auto Release", false);
    flt (ids::rGain, "88R Gain", NormalisableRange<float> (0.f, 30.f, 0.f), 0.f, "dB", 1);

    // gate
    bln (ids::gIn, "Gate In", false);
    // gate / expander after the Neve 88R: threshold -25 ... +15 dBu (-40 dB switch), range 0 ... 60 dB, attack 500 us (FAST 50 us),
    // release 10 ms ... 3 s, hysteresis 0 ... 25 dB (fully down = 1:2 expander)
    flt (ids::gThr, "Gate Threshold", NormalisableRange<float> (-25.f, 15.f, 0.f), 8.f, "dBu", 1);
    bln (ids::gPad, "Gate Threshold -40 dB", true);
    flt (ids::gRange, "Gate Range", NormalisableRange<float> (0.f, 60.f, 0.f), 40.f, "dB", 1);
    bln (ids::gFast, "Gate Fast Attack", false);
    flt (ids::gRel, "Gate Release", skewed (10.f, 3000.f, 150.f), 150.f, "ms", 0);
    flt (ids::gHyst, "Gate Hysteresis", NormalisableRange<float> (0.f, 25.f, 0.f), 3.f, "dB", 1);

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
        pEq = p (ids::eqIn); pPhase = p (ids::phase); pOrder = p (ids::order);
        for (int b = 0; b < 4; ++b) { pFreq[b] = p (ids::freq[b]); pGain[b] = p (ids::gain[b]); pAlt[b] = p (ids::alt[b]); pQ[b] = p (ids::q[b]); }
        cModel = p (ids::cModel); cIn = p (ids::cIn); cDrive = p (ids::cDrive); cMix = p (ids::cMix); cSc = p (ids::cSc);
        kThr = p (ids::kThr); kRatio = p (ids::kRatio); kRec = p (ids::kRec); kGain = p (ids::kGain); kFast = p (ids::kFast);
        kComp = p (ids::kComp); kLim = p (ids::kLim); kLimLvl = p (ids::kLimLvl); kLimRec = p (ids::kLimRec);
        qThr = p (ids::pThr); qRatio = p (ids::pRatio); qAtk = p (ids::pAtk); qRel = p (ids::pRel); qMk = p (ids::pMk); qTone = p (ids::pTone);
        gIn = p (ids::gIn); gThr = p (ids::gThr); gRange = p (ids::gRange); gRel = p (ids::gRel); gFast = p (ids::gFast); gHyst = p (ids::gHyst); gPad = p (ids::gPad);
        rThr = p (ids::rThr); rPad = p (ids::rPad); rRatio = p (ids::rRatio); rFast = p (ids::rFast); rAtk = p (ids::rAtk); rRel = p (ids::rRel); rAuto = p (ids::rAuto); rGain = p (ids::rGain);
        pOs = p (ids::os); pOsFilt = p (ids::osFilt); pAnalog = p (ids::analog);
    }

    const String getName() const override { return "1081+"; }
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
            default: return { sr >= 88000.0 ? 2 : 4, lin };
        }
    }

    void prepareToPlay (double sr, int block) override
    {
        baseRate = sr; sampleRateUi = sr;
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
        drvSm.reset (sr, 0.03);  drvSm.setCurrentAndTargetValue ((float) Decibels::decibelsToGain (cDrive->load()));
        anSm.reset (sr, 0.05);   anSm.setCurrentAndTargetValue (pAnalog->load());
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
        for (int b = 0; b < 4; ++b) {
            p.freqIdx[b] = (int) pFreq[b]->load(); p.gainDb[b] = pGain[b]->load(); p.alt[b] = pAlt[b]->load() > 0.5f; p.qMul[b] = pQ[b]->load();
        }
        return p;
    }
    void currentOrder (int o[3]) const { const int i = jlimit (0, 5, (int) pOrder->load()); for (int k = 0; k < 3; ++k) o[k] = kPerms[i][k]; }

    bool hasEditor() const override { return true; }
    AudioProcessorEditor* createEditor() override;

    void getStateInformation (MemoryBlock& d) override
    {
        auto st = apvts.copyState();
        st.setProperty ("uiScale", uiScale, nullptr);
        st.setProperty ("compOpen", compOpen, nullptr);
        st.setProperty ("gateOpen", gateOpen, nullptr);
        st.setProperty ("vuMode", vuMode, nullptr);
        st.setProperty ("compactEq", compactEq, nullptr);
        for (int b = 0; b < 4; ++b) {
            st.setProperty ("qOpen" + String (b), qOpen[b], nullptr);
            st.setProperty ("qLast" + String (b), qLast[b], nullptr);
        }
        if (auto x = st.createXml()) copyXmlToBinary (*x, d);
    }
    void setStateInformation (const void* d, int size) override
    {
        if (auto x = getXmlFromBinary (d, size)) {
            auto st = ValueTree::fromXml (*x);
            if (st.hasType (apvts.state.getType())) {
                uiScale = (float) (double) st.getProperty ("uiScale", 1.0);
                compOpen = (bool) st.getProperty ("compOpen", false);
                gateOpen = (bool) st.getProperty ("gateOpen", false);
                compOpen = compOpen || gateOpen;           // one dynamics panel now
                vuMode = (int) st.getProperty ("vuMode", 2);
                compactEq = (bool) st.getProperty ("compactEq", false);
                for (int b = 0; b < 4; ++b) {
                    qOpen[b] = (bool) st.getProperty ("qOpen" + String (b), false);
                    qLast[b] = (float) (double) st.getProperty ("qLast" + String (b), 1.0);
                }
                for (auto* k : { "uiScale", "compOpen", "gateOpen", "vuMode", "compactEq" }) st.removeProperty (k, nullptr);
                for (int b = 0; b < 4; ++b) { st.removeProperty ("qOpen" + String (b), nullptr); st.removeProperty ("qLast" + String (b), nullptr); }
                apvts.replaceState (st);
                uiStateVersion++;
            }
        }
    }

    AudioProcessorValueTreeState apvts;
    std::atomic<float> meter { 0 }, vuIn { 0 }, vuOut { 0 }, vuGr { 0 }, gateAtten { 0 };
    std::atomic<int> activeFactor { 1 };
    std::atomic<double> sampleRateUi { 48000 };
    float uiScale = 1.0f; bool compOpen = false /* dynamics panel shown */, gateOpen = false; int vuMode = 2;   // VU: 0 in, 1 out, 2 GR
    bool compactEq = false;                              // settings: concentric EQ / filter knobs (smaller plugin)
    bool qOpen[4] { false, false, false, false };        // Q button turned into a Q knob (right-click / Ctrl+click)
    float qLast[4] { 1, 1, 1, 1 };                       // last user Q of a knob that was closed, brought back on reopen
    std::atomic<int> uiStateVersion { 0 };               // bumped when a song is loaded, so an open editor re-reads the above
    static constexpr int kSpecSize = 8192;                  // output for the live spectrum (mono, base rate)
    float spec[kSpecSize] {}; std::atomic<int> specPos { 0 };

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
        comp.prepare (osRate); c88.prepare (osRate); pv.prepare (osRate); gate.prepare (osRate);
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

        for (int i = 0; i < n; ++i)
            for (int c = 0; c < 2; ++c) work.setSample (c, i, (double) buf.getSample (jmin (c, nCh - 1), start + i));

        // per-block targets; gains ramp linearly across the block
        auto ramp = [n] (SmoothedValue<float>& s, float target) { s.setTargetValue (target); const float a = s.getCurrentValue(); s.skip (n); return std::make_pair (a, s.getCurrentValue()); };
        const auto inG  = ramp (inSm,  (float) Decibels::decibelsToGain (pIn->load()));
        const auto line = ramp (lineSm, (float) Decibels::decibelsToGain (n1081::lineGainDb ((int) pLine->load())));
        const auto outG = ramp (outSm, (float) Decibels::decibelsToGain (pOut->load()));
        const auto drv  = ramp (drvSm, (float) Decibels::decibelsToGain (cDrive->load()));
        const auto an   = ramp (anSm, pAnalog->load());
        const bool anOn = std::abs (an.first) > 1e-4f || std::abs (an.second) > 1e-4f;   // dead centre: untouched
        const auto anD  = std::make_pair ((float) n1081::analogDrive (an.first), (float) n1081::analogDrive (an.second));
        for (int b = 0; b < 4; ++b) gainSm[b].setTargetValue (pEq->load() > 0.5f ? pGain[b]->load() : 0.f);
        const int hpf = (int) pHpf->load(), lpf = (int) pLpf->load();
        if (hpf != lastHpf || lpf != lastLpf) { for (auto& c : ch) c.setFilters (hpf, lpf); lastHpf = hpf; lastLpf = lpf; }

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

        const bool compOn = cIn->load() > 0.5f, gateOn = gIn->load() > 0.5f;
        const int model = (int) cModel->load();
        if (model != lastModel) { comp.reset(); c88.reset(); pv.c.reset(); lastModel = model; }
        const n1081::CompParams2254 kp = params2254();
        const pv::CompParams pp = paramsPv();
        const n1081::Comp88Params rp = params88();
        n1081::GateParams gp;
        gp.thrDb = gThr->load() - (gPad->load() > 0.5f ? 40.0 : 0.0) - n1081::kDbuOffset;     // dBu -> dBFS
        gp.rangeDb = gRange->load(); gp.atkMs = gFast->load() > 0.5f ? 0.05 : 0.5;
        gp.holdMs = 0; gp.relMs = gRel->load(); gp.keyHpf = 0;
        gp.hystDb = gHyst->load(); gp.expander = gp.hystDb < 0.1;
        updatePvSc();
        int order[3]; currentOrder (order);
        double sIn = 0, sOut = 0, grMax = 0, gateMax = 0;

        constexpr int sub = 32;
        for (int s0 = 0; s0 < m; s0 += sub) {
            const int len = jmin (sub, m - s0);
            updateCoefs (false, len);
            if (anOn || ch[0].colour != 1.0) {
                const double t = (s0 + 0.5 * len) / (double) m;
                const double k = anOn ? n1081::analogColour (an.first + (an.second - an.first) * t) : 1.0;
                for (auto& c : ch) c.setColour (k);
            }
            for (int i = 0; i < len; ++i) {
                const int k = s0 + i;
                const double t = (double) k / (double) m;
                auto lerp = [t] (const std::pair<float, float>& p) { return (double) p.first + ((double) p.second - (double) p.first) * t; };
                double L = hi[0][k], R = hi[1][k];
                for (int stage = 0; stage < 3; ++stage) {
                    switch (order[stage]) {
                        case ModEq: {                        // 1081: clean trim -> circuit -> output trim, which feeds the next module
                            const double gi = lerp (inG), lg = lerp (line), go = lerp (outG);
                            if (anOn) {                      // ANALOG: drive the stages, then take the same gain back off
                                const double d = lerp (anD);
                                L = ch[0].process (L * gi * d, lg, hpf, lpf) * go / d;
                                R = ch[1].process (R * gi * d, lg, hpf, lpf) * go / d;
                            } else {
                                L = ch[0].process (L * gi, lg, hpf, lpf) * go;
                                R = ch[1].process (R * gi, lg, hpf, lpf) * go;
                            }
                        } break;
                        case ModComp: if (compOn) {          // clean drive into the compressor
                            const double d = lerp (drv); L *= d; R *= d;
                            sIn += L * L + R * R;
                            double gr = 0;
                            if (model == 0) { comp.process (L, R, kp); gr = comp.gr; }
                            else if (model == 2) { c88.process (L, R, rp); gr = c88.gr; }
                            else {
                                float fl = (float) L, fr = (float) R, kl = fl, kr = fr;
                                if (pv.scHz > 0) { kl = pv.sc[0].process (kl); kr = pv.sc[1].process (kr); }
                                pv.c.setTimes (pp.atkMs, pp.relMs);
                                pv.c.process (fl, fr, kl, kr, pp);
                                L = fl; R = fr; gr = pv.c.reductionDb();
                            }
                            grMax = jmax (grMax, gr);
                            sOut += L * L + R * R;
                        } break;
                        case ModGate: if (gateOn) { double a = 0; gate.process (L, R, gp, a); gateMax = jmax (gateMax, a); } break;
                        default: break;
                    }
                }
                hi[0][k] = L; hi[1][k] = R;
            }
        }

        if (f > 1) {
            if (curLinear) { auto& o = fir[firIndex (f)]; for (int c = 0; c < 2; ++c) o.down (c, work.getWritePointer (c), n); }
            else iir[iirIndex (f)]->processSamplesDown (wblock);
        }

        const double sign = pPhase->load() > 0.5f ? -1.0 : 1.0;
        float peak = 0;
        int sp = specPos.load();
        for (int i = 0; i < n; ++i) {
            double mono = 0;
            for (int c = 0; c < nCh; ++c) {
                const double v = work.getSample (c, i) * sign;
                buf.setSample (c, start + i, (T) v);
                peak = jmax (peak, (float) std::abs (v));
                mono += v;
            }
            spec[sp] = (float) (mono / nCh); sp = (sp + 1) & (kSpecSize - 1);
        }
        specPos.store (sp);
        meter.store (jmax (peak, meter.load() * 0.85f));
        vuIn.store (compOn ? (float) std::sqrt (sIn / (2.0 * m)) : 0.f);
        vuOut.store (compOn ? (float) std::sqrt (sOut / (2.0 * m)) : 0.f);
        vuGr.store (compOn ? (float) grMax : 0.f);
        gateAtten.store (gateOn ? (float) gateMax : 0.f);
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
    n1081::Comp88Params params88() const
    {
        n1081::Comp88Params p;
        p.thrDbu = rThr->load() - (rPad->load() > 0.5f ? 20.0 : 0.0);
        p.ratio = rRatio->load();
        const double a = rAtk->load();                   // 3 ... 7 ms; FAST stretches the range down to 1 ms
        p.atkMs = rFast->load() > 0.5f ? 1.0 + (a - 3.0) * 1.5 : a;
        p.relMs = rRel->load(); p.autoRel = rAuto->load() > 0.5f;
        p.gainDb = rGain->load(); p.mix = cMix->load() * 0.01; p.scHpf = (int) cSc->load();
        return p;
    }
    pv::CompParams paramsPv() const
    {
        pv::CompParams p; p.on = true; p.duck = false;
        p.type = 0;                                       // the clean VCA (with its Vintage colour)
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
            const double key = p.freqIdx[b] * 2 + (p.alt[b] ? 1 : 0) + 100.0 * p.qMul[b];
            if (force || gainSm[b].isSmoothing() || key != lastKey[b] || std::abs (p.gainDb[b] - lastGain[b]) > 1e-4f) {
                const auto s = n1081::bandSpec (p, b);
                for (auto& c : ch) c.setBand (b, s, true);   // EQ in/out is a smoothed fade of the band gains
                lastKey[b] = key; lastGain[b] = (float) p.gainDb[b];
            }
        }
    }

    std::atomic<float>* pLine {}, * pOut {}, * pIn {}, * pHpf {}, * pLpf {}, * pEq {}, * pPhase {}, * pOrder {};
    std::atomic<float>* pFreq[4] {}, * pGain[4] {}, * pAlt[4] {}, * pQ[4] {};
    std::atomic<float>* cModel {}, * cIn {}, * cDrive {}, * cMix {}, * cSc {};
    std::atomic<float>* kThr {}, * kRatio {}, * kRec {}, * kGain {}, * kFast {}, * kComp {}, * kLim {}, * kLimLvl {}, * kLimRec {};
    std::atomic<float>* qThr {}, * qRatio {}, * qAtk {}, * qRel {}, * qMk {}, * qTone {};
    std::atomic<float>* gIn {}, * gThr {}, * gRange {}, * gRel {}, * gFast {}, * gHyst {}, * gPad {};
    std::atomic<float>* rThr {}, * rPad {}, * rRatio {}, * rFast {}, * rAtk {}, * rRel {}, * rAuto {}, * rGain {};
    std::atomic<float>* pOs {}, * pOsFilt {}, * pAnalog {};

    std::unique_ptr<dsp::Oversampling<double>> iir[3];
    n1081::FirOversampler fir[4];
    AudioBuffer<double> work;
    int maxBlock = 512, curFactor = -1, lastModel = -1; bool curLinear = false;
    double baseRate = 48000, osRate = 192000;
    n1081::Channel ch[2];
    n1081::Comp2254 comp;
    n1081::Comp88 c88;
    n1081::Gate gate;
    PvUnit pv;
    SmoothedValue<float> gainSm[4], lineSm, outSm, inSm, drvSm, anSm;
    int lastHpf = -1, lastLpf = -1;
    double lastKey[4] { -1, -1, -1, -1 };
    float lastGain[4] { 99, 99, 99, 99 };
};

//==============================================================================
// the text: Inter, thinner than before (medium where it was bold, regular elsewhere), same sizes
static Font uiFont (float size, bool medium)
{
    static Typeface::Ptr reg = Typeface::createSystemTypefaceFor (flexfonts::InterRegular, (size_t) flexfonts::InterRegularSize);
    static Typeface::Ptr med = Typeface::createSystemTypefaceFor (flexfonts::InterMedium, (size_t) flexfonts::InterMediumSize);
    return Font (FontOptions (medium ? med : reg).withHeight (size));
}
namespace col {
    static const Colour panel  (0xff4f5b66), panelDark (0xff3c4650), well (0xff2b3239);
    static const Colour comp   (0xff46525c), compDark (0xff353f48);
    static const Colour legend (0xffece6d6), dim (0xffa9b0b4);
    static const Colour capRed (0xffb8322c), capBlue (0xff2f5d8c), capGrey (0xff8c9196), capBlack (0xff23272b);
    static const Colour lampOn (0xffffb43c), curve (0xffffc864), vuFace (0xfff1e6c8), cream (0xffd9d4c6), ink (0xff23272b);
}
constexpr int kBtnW = 60, kBtnH = 26;     // one size for every push button (same as the EQ's PEAK / HI-Q)
constexpr int kSmallW = 34;               // ON/OFF and the VU IN / OUT / GR buttons: just as wide as their text

// curve symbols on the EQ shape buttons (as on a console's switch legends)
static Path shapeIcon (int icon, bool alt, Rectangle<float> r)
{
    Path p;
    const float x0 = r.getX(), x1 = r.getRight(), top = r.getY(), base = r.getBottom(), cx = r.getCentreX(), w = r.getWidth();
    auto bell = [&] (float half) {                     // half = half-width of the bell
        p.startNewSubPath (x0, base); p.lineTo (cx - half, base);
        p.cubicTo (cx - half * 0.45f, base, cx - half * 0.35f, top, cx, top);
        p.cubicTo (cx + half * 0.35f, top, cx + half * 0.45f, base, cx + half, base); p.lineTo (x1, base);
    };
    if (icon == 3) bell (alt ? w * 0.18f : w * 0.48f);                  // mids: wide / sharp
    else if (alt) bell (w * 0.36f);                                       // LF / HF peak
    else if (icon == 1) {                                                 // LF shelf
        p.startNewSubPath (x0, top); p.lineTo (x0 + w * 0.25f, top);
        p.cubicTo (x0 + w * 0.5f, top, x0 + w * 0.5f, base, x0 + w * 0.75f, base); p.lineTo (x1, base);
    } else {                                                              // HF shelf
        p.startNewSubPath (x0, base); p.lineTo (x0 + w * 0.25f, base);
        p.cubicTo (x0 + w * 0.5f, base, x0 + w * 0.5f, top, x0 + w * 0.75f, top); p.lineTo (x1, top);
    }
    return p;
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
        const bool ring = s.getProperties()["ring"], centre = s.getProperties()["centre"];
        auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3);
        const float d = jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (d, d);
        const auto c = r.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        const int ticks = centre ? 0 : stepped ? jmin (17, (int) std::round (s.getMaximum() - s.getMinimum()) + 1) : 11;
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
        const float rOut = body.getWidth() * 0.5f - 1;
        if (ring) g.drawLine ({ c.getPointOnCircumference (rOut * 0.74f, ang), c.getPointOnCircumference (rOut, ang) }, 3.0f);   // pointer on the skirt
        else g.drawLine ({ c.getPointOnCircumference (d * 0.08f, ang), c.getPointOnCircumference (rOut, ang) }, 2.4f);
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
        if ((int) b.getProperties()["icon"] != 0 || (bool) b.getProperties()["text"]) {     // no key: the wave or the word alone
            if (over || down) { g.setColour (Colours::white.withAlpha (down ? 0.10f : 0.05f)); g.fillRoundedRectangle (r, 4); }
            return;
        }
        g.setColour (col::well); g.fillRoundedRectangle (r, 3);
        auto face = r.reduced (2.0f);
        g.setGradientFill (ColourGradient (on ? col::lampOn : col::cream, face.getTopLeft(),
                                           on ? col::lampOn.darker (0.35f) : Colour (0xff9e998c), face.getBottomLeft(), false));
        g.fillRoundedRectangle (face, 2);
        if (over || down) { g.setColour (Colours::white.withAlpha (down ? 0.25f : 0.1f)); g.fillRoundedRectangle (face, 2); }
    }
    void drawButtonText (Graphics& g, TextButton& b, bool, bool) override
    {
        const bool flat = b.getProperties()["flat"];
        g.setColour (flat ? col::legend : col::ink);
        if (const int icon = b.getProperties()["icon"]) {                       // the wave alone: white, yellow when on
            const auto r = b.getLocalBounds().toFloat().reduced (b.getWidth() * 0.18f, b.getHeight() * 0.26f);
            g.setColour (b.getToggleState() ? col::lampOn : Colours::white.withAlpha (0.92f));
            g.strokePath (shapeIcon (icon, b.getToggleState(), r), PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
            return;
        }
        if ((bool) b.getProperties()["text"]) {                                 // the word alone: white, yellow when on
            g.setColour (b.getToggleState() ? col::lampOn : col::legend);
            g.setFont (uiFont (11.5f, true));
            g.drawFittedText (b.getButtonText(), b.getLocalBounds(), Justification::centred, 1);
            return;
        }
        const auto text = b.getButtonText();
        g.setFont (uiFont (text.length() == 1 ? 15.0f : 11.0f, true));
        g.drawFittedText (text, b.getLocalBounds(), Justification::centred, 1);
    }
    void drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box) override
    {
        auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
        const bool asButton = box.getProperties()["button"];
        if (asButton) {                                         // looks like the cream push buttons
            g.setColour (col::well); g.fillRoundedRectangle (r, 3);
            auto face = r.reduced (2.0f);
            g.setGradientFill (ColourGradient (col::cream, face.getTopLeft(), Colour (0xff9e998c), face.getBottomLeft(), false));
            g.fillRoundedRectangle (face, 2);
        } else { g.setColour (col::well); g.fillRoundedRectangle (r, 4); }
        Path p; const float ax = (float) w - 11.0f, ay = (float) h * 0.5f;
        p.startNewSubPath (ax - 3.5f, ay - 2); p.lineTo (ax, ay + 2); p.lineTo (ax + 3.5f, ay - 2);
        g.setColour (asButton ? col::ink : col::legend); g.strokePath (p, PathStrokeType (1.6f));
    }
    Label* createSliderTextBox (Slider& s) override
    {
        auto* l = LookAndFeel_V4::createSliderTextBox (s);
        if ((bool) s.getProperties()["ring"] || (bool) s.getProperties()["small"]) l->setFont (uiFont (11.5f, true));
        return l;
    }
    Font getComboBoxFont (ComboBox&) override { return uiFont (11.5f, true); }
    void positionComboBoxText (ComboBox& b, Label& l) override { l.setBounds (4, 0, b.getWidth() - 18, b.getHeight()); l.setFont (getComboBoxFont (b)); l.setJustificationType (Justification::centred); }
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

// centre knob of a concentric pair: only the round cap takes the mouse, the ring around it belongs to the outer knob
class CentreKnob : public FineKnob
{
public:
    CentreKnob() { getProperties().set ("centre", true); setTextBoxStyle (Slider::NoTextBox, false, 0, 0); }
    bool hitTest (int x, int y) override { const auto c = getLocalBounds().getCentre(); return c.getDistanceFrom ({ x, y }) <= getWidth() / 2 - 3; }
};

// Q knob that replaces a shape button; double-click closes it again
class QKnob : public FineKnob
{
public:
    std::function<void()> onClose;
    void mouseDoubleClick (const MouseEvent&) override { if (onClose) onClose(); }
};

// shape button: right-click or Ctrl+click asks for the Q knob instead of switching
class CurveButton : public TextButton
{
public:
    std::function<void()> onSecondary;
    void mouseDown (const MouseEvent& e) override
    {
        secondary = e.mods.isPopupMenu() || e.mods.isCtrlDown();
        if (secondary) { if (onSecondary) onSecondary(); return; }
        TextButton::mouseDown (e);
    }
    void mouseUp (const MouseEvent& e) override { if (secondary) { secondary = false; return; } TextButton::mouseUp (e); }
    void mouseDrag (const MouseEvent& e) override { if (! secondary) TextButton::mouseDrag (e); }
private:
    bool secondary = false;
};

struct Caption { String text; Rectangle<int> r; };

// shared base for the three module panels
class ModulePanel : public Component
{
public:
    ModulePanel (N1081Processor& p, KnobLnF& l) : proc (p) { setLookAndFeel (&l); }
    ~ModulePanel() override { setLookAndFeel (nullptr); }
    std::function<void (int dir)> onMove;          // -1 = left (earlier), +1 = right (later)
protected:
    void knob (FineKnob& s, const char* id, Colour cap, bool stepped, std::function<String (double)> text = {})
    {
        s.setColour (Slider::rotarySliderFillColourId, cap);
        s.setColour (Slider::textBoxTextColourId, col::legend);
        s.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        s.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        s.getProperties().set ("stepped", stepped);
        if (stepped) s.setMouseDragSensitivity (220);
        addAndMakeVisible (s);
        attachments.push_back (std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, s));
        if (auto* p = proc.apvts.getParameter (id)) s.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        if (text) { s.textFromValueFunction = text; s.updateText(); }
    }
    void button (TextButton& b, const char* id, const String& text)
    {
        b.setButtonText (text); b.setClickingTogglesState (true);
        addAndMakeVisible (b);
        buttonAtt.push_back (std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, b));
    }
    // IN switch shown as ON / OFF, only as wide as its text
    void onOff (TextButton& b, const char* id)
    {
        button (b, id, "OFF");
        b.onStateChange = [&b] { const String t = b.getToggleState() ? "ON" : "OFF"; if (b.getButtonText() != t) b.setButtonText (t); };
        b.onStateChange();
    }
    void arrows (int x, int y)
    {
        for (int i = 0; i < 2; ++i) {
            mv[i].setButtonText (i == 0 ? "<" : ">"); mv[i].getProperties().set ("flat", true);
            mv[i].setTooltip (i == 0 ? "Move left (earlier in the chain)" : "Move right (later in the chain)");
            mv[i].onClick = [this, i] { if (onMove) onMove (i == 0 ? -1 : 1); };
            mv[i].setBounds (x + i * 24, y, 22, 20);          // not shown: the swap sign between the panels does it
        }
    }
    // knob centred at cx, top at y; the value box is exactly as wide as the knob column
    void put (Slider& s, const String& cap, int cx, int y, int size, int colW = 72)
    {
        s.setTextBoxStyle (Slider::TextBoxBelow, false, colW, 16);
        s.setBounds (cx - colW / 2, y, colW, size + 18);
        if (cap.isNotEmpty()) captions.push_back ({ cap, { cx - colW / 2, y + size + 18, colW, 12 } });
    }
    // concentric pair: outer ring + centre cap on one spot; one read-only readout "outer / inner" below
    void putConcentric (Slider& outer, Slider& inner, const String& cap, int cx, int y, int size, int colW)
    {
        outer.setTextBoxStyle (Slider::TextBoxBelow, true, colW, 16);
        outer.setBounds (cx - colW / 2, y, colW, size + 18);
        const int d = (int) std::round (size * 0.58f);
        inner.setBounds (cx - d / 2, y + size / 2 - d / 2, d, d);
        if (cap.isNotEmpty()) captions.push_back ({ cap, { cx - colW / 2, y + size + 18, colW, 12 } });
    }
    // concentric pair on two parameters; the outer slider's read-only box reads "outer / inner"
    void concentric (FineKnob& outer, CentreKnob& inner, const char* idO, const char* idI, Colour capO, Colour capI,
                     bool stepO, bool stepI, std::function<String (double)> tO = {}, std::function<String (double)> tI = {})
    {
        knob (outer, idO, capO, stepO);
        knob (inner, idI, capI, stepI);
        outer.getProperties().set ("ring", true);
        inner.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        auto* po = proc.apvts.getParameter (idO); auto* pi = proc.apvts.getParameter (idI);
        auto fmt = [] (RangedAudioParameter* prm, const std::function<String (double)>& f, double v) {
            return f ? f (v) : prm->getText (prm->convertTo0to1 ((float) v), 0); };
        outer.textFromValueFunction = [po, pi, tO, tI, fmt] (double v) { return fmt (po, tO, v) + " / " + fmt (pi, tI, pi->convertFrom0to1 (pi->getValue())); };
        inner.onValueChange = [&outer] { outer.updateText(); };
        outer.updateText();
    }
    // concentric pair with its readout to the right (one short row)
    void putConcentricRight (Slider& outer, Slider& inner, int x, int y, int size, int textW)
    {
        outer.setTextBoxStyle (Slider::TextBoxRight, true, textW, 16);
        outer.setBounds (x, y, size + textW, size);
        const int d = (int) std::round (size * 0.58f);
        inner.setBounds (x + size / 2 - d / 2, y + size / 2 - d / 2, d, d);
    }
    void paintPanel (Graphics& g, Colour top, Colour bottom, const String& title, bool centredTitles = false)
    {
        g.setGradientFill (ColourGradient (top, 0, 0, bottom, 0, (float) getHeight(), false));
        g.fillAll();
        g.setColour (Colours::black.withAlpha (0.35f)); g.drawVerticalLine (0, 0, (float) getHeight());
        for (auto pt : { Point<float> (getWidth() - 12.f, getHeight() - 12.f), Point<float> (12.f, getHeight() - 12.f) }) {
            g.setColour (Colour (0xff9aa1a6)); g.fillEllipse (Rectangle<float> (9, 9).withCentre (pt));
            g.setColour (Colour (0xff4a4f53)); g.drawLine (pt.x - 3, pt.y, pt.x + 3, pt.y, 1.3f);
        }
        if (title.isNotEmpty()) { g.setColour (col::legend); g.setFont (uiFont (20.f, true)); g.drawText (title, titleX, 10, 110, 26, Justification::left); }
        for (auto& s : rows) {
            g.setColour (Colours::black.withAlpha (0.17f)); g.fillRoundedRectangle (s.r.toFloat(), 6);
            g.setColour (col::legend); g.setFont (uiFont (centredTitles ? 12.f : 11.f, true));
            if (centredTitles) g.drawText (s.text, s.r.getX(), s.r.getY() + 5, s.r.getWidth(), 14, Justification::centred);
            else g.drawText (s.text, s.r.getX() + 8, s.r.getY() + 4, 200, 13, Justification::left);
        }
        g.setFont (uiFont (10.f, true)); g.setColour (col::dim);
        for (auto& c : captions) g.drawText (c.text, c.r, Justification::centred);
    }

    N1081Processor& proc;
    int titleX = 14;
    TextButton mv[2];
    std::vector<std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>> attachments;
    std::vector<std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>> buttonAtt;
    std::vector<Caption> rows, captions;
};

//==============================================================================
// EQ graph: response curve, live output spectrum, draggable band dots (wheel = free Q)
class Graph : public Component, private Timer
{
public:
    explicit Graph (N1081Processor& p) : proc (p), fft (12) { startTimerHz (60); }

    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::well); g.fillRoundedRectangle (r, 5);
        auto mr = r.removeFromRight (10).reduced (2, 6);
        plot = r.reduced (8, 6);
        g.setFont (uiFont (10.f, false));
        for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 }) {
            g.setColour (Colours::white.withAlpha (0.07f)); g.drawVerticalLine ((int) xOf (f), plot.getY(), plot.getBottom());
            g.setColour (col::dim.withAlpha (0.7f)); g.drawText (hz (f), (int) xOf (f) + 2, (int) plot.getBottom() - 12, 30, 12, Justification::left);
        }
        for (double db : { -18.0, -12.0, -6.0, 6.0, 12.0, 18.0 }) {
            g.setColour (Colours::white.withAlpha (0.05f)); g.drawHorizontalLine ((int) yOf (db), plot.getX(), plot.getRight());
            g.setColour (col::dim.withAlpha (0.6f)); g.drawText (String ((int) db), (int) plot.getX() + 2, (int) yOf (db) - 6, 24, 12, Justification::left);
        }
        {   // live spectrum of the output, -90 ... 0 dBFS over the full height
            Path sp; bool started = false;
            const double sr = proc.sampleRateUi.load();
            const int W = (int) plot.getWidth();
            for (int x = 0; x <= W; x += 2) {
                const double f0 = 20.0 * std::pow (1000.0, (double) x / W), f1 = 20.0 * std::pow (1000.0, (double) (x + 2) / W);
                const int b0 = (int) (f0 / sr * kN), b1 = jmax (b0 + 1, (int) (f1 / sr * kN));
                float mx = -120.f;
                for (int b = jmax (1, b0); b < jmin (kN / 2, b1); ++b) mx = jmax (mx, smooth[b]);
                const float y = plot.getBottom() - jlimit (0.f, 1.f, (mx + 90.f) / 90.f) * plot.getHeight();
                if (! started) { sp.startNewSubPath (plot.getX() + x, plot.getBottom()); started = true; }
                sp.lineTo (plot.getX() + x, y);
            }
            sp.lineTo (plot.getRight(), plot.getBottom()); sp.closeSubPath();
            g.setColour (Colour (0xff7fa7c9).withAlpha (0.22f)); g.fillPath (sp);
        }
        g.setColour (Colours::white.withAlpha (0.18f)); g.drawHorizontalLine ((int) yOf (0), plot.getX(), plot.getRight());
        const auto p = proc.currentParams();
        Path path; const int N = (int) plot.getWidth();
        for (int i = 0; i <= N; ++i) {
            const double f = 20.0 * std::pow (1000.0, (double) i / N);
            const float y = jlimit (plot.getY(), plot.getBottom(), yOf (n1081::responseDb (p, f)));
            if (i == 0) path.startNewSubPath (xOf (f), y); else path.lineTo (xOf (f), y);
        }
        Path fill (path); fill.lineTo (plot.getRight(), yOf (0)); fill.lineTo (plot.getX(), yOf (0)); fill.closeSubPath();
        g.setColour (col::curve.withAlpha (p.eqIn ? 0.12f : 0.04f)); g.fillPath (fill);
        g.setColour (p.eqIn ? col::curve : col::curve.withAlpha (0.35f)); g.strokePath (path, PathStrokeType (2.f));
        const Colour dots[4] = { Colour (0xff6fa0d8), Colour (0xffd0d3d6), Colour (0xffd0d3d6), Colour (0xff6fa0d8) };
        for (int b = 0; b < 4; ++b) {
            const auto c = dotPos (p, b);
            const bool hot = b == hover || b == drag;
            const float d = hot ? 13.f : 10.f;
            g.setColour (dots[b].withAlpha (hot ? 1.f : 0.8f)); g.fillEllipse (Rectangle<float> (d, d).withCentre (c));
            g.setColour (col::ink); g.drawEllipse (Rectangle<float> (d, d).withCentre (c), 1.f);
            if (hot) {
                const auto s = n1081::bandSpec (p, b);
                const String txt = hz (s.f0) + "   " + signedDb (p.gainDb[b], 1) + " dB   Q " + String (s.Q, 2)
                                 + (std::abs (p.qMul[b] - 1.0) < 0.005 ? "  (hardware)" : "   (wheel: Q, double-click: hardware)");
                g.setColour (col::legend); g.setFont (uiFont (11.f, true));
                g.drawText (txt, plot.withHeight (14).translated (0, 2), Justification::centredTop);
            }
        }
        const float db = (float) Decibels::gainToDecibels (proc.meter.load(), -60.0f);
        const float frac = jlimit (0.f, 1.f, (db + 60.f) / 60.f);
        g.setColour (Colours::black.withAlpha (0.4f)); g.fillRect (mr);
        g.setColour (db > -1.f ? Colours::red : db > -12.f ? col::lampOn : Colour (0xff7fc97f));
        g.fillRect (mr.withTop (mr.getBottom() - mr.getHeight() * frac));
    }

    // drag a dot: up/down = gain, left/right = frequency (hardware steps); mouse wheel = free Q
    void mouseMove (const MouseEvent& e) override { const int h = hit (e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const MouseEvent& e) override
    {
        drag = hit (e.position);
        if (drag >= 0) for (auto* id : { ids::gain[drag], ids::freq[drag] }) proc.apvts.getParameter (id)->beginChangeGesture();
    }
    void mouseDrag (const MouseEvent& e) override
    {
        if (drag < 0) return;
        const double gain = jlimit (-18.0, 18.0, ((plot.getCentreY() - e.position.y) / (plot.getHeight() * 0.5)) * 24.0);
        auto* gp = proc.apvts.getParameter (ids::gain[drag]);
        gp->setValueNotifyingHost (gp->convertTo0to1 ((float) gain));
        const double f = 20.0 * std::pow (1000.0, (e.position.x - plot.getX()) / plot.getWidth());
        const double* tab = drag == 0 ? n1081::kLfFreqs : drag == 1 ? n1081::kLmfFreqs : drag == 2 ? n1081::kHmfFreqs : n1081::kHfFreqs;
        const int cnt = drag == 1 || drag == 2 ? 10 : 5;
        int best = 0; for (int i = 1; i < cnt; ++i) if (std::abs (std::log (tab[i] / f)) < std::abs (std::log (tab[best] / f))) best = i;
        auto* fp = proc.apvts.getParameter (ids::freq[drag]);
        fp->setValueNotifyingHost (fp->convertTo0to1 ((float) best));
        repaint();
    }
    void mouseUp (const MouseEvent&) override
    {
        if (drag >= 0) for (auto* id : { ids::gain[drag], ids::freq[drag] }) proc.apvts.getParameter (id)->endChangeGesture();
        drag = -1;
    }
    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        const int b = hit (e.position, 24.f);
        if (b < 0) return;
        auto* qp = proc.apvts.getParameter (ids::q[b]);
        const float cur = qp->convertFrom0to1 (qp->getValue());
        const float next = jlimit (0.25f, 4.0f, cur * std::exp (w.deltaY * (e.mods.isAltDown() ? 0.15f : 0.6f)));
        qp->beginChangeGesture(); qp->setValueNotifyingHost (qp->convertTo0to1 (next)); qp->endChangeGesture();
        hover = b; repaint();
    }
    void mouseDoubleClick (const MouseEvent& e) override        // double-click a dot: back to the hardware Q
    {
        const int b = hit (e.position);
        if (b < 0) return;
        auto* qp = proc.apvts.getParameter (ids::q[b]);
        qp->beginChangeGesture(); qp->setValueNotifyingHost (qp->convertTo0to1 (1.0f)); qp->endChangeGesture();
    }

private:
    static constexpr int kN = 4096;
    float xOf (double f) const { return plot.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * plot.getWidth(); }
    float yOf (double db) const { return plot.getCentreY() - (float) (db / 24.0) * plot.getHeight() * 0.5f; }
    Point<float> dotPos (const n1081::Params& p, int b) const
    {
        const auto s = n1081::bandSpec (p, b);
        return { xOf (s.f0), jlimit (plot.getY() + 6, plot.getBottom() - 6, yOf (p.gainDb[b])) };
    }
    int hit (Point<float> pt, float radius = 12.f) const
    {
        const auto p = proc.currentParams();
        int best = -1; float bd = radius;
        for (int b = 0; b < 4; ++b) { const float d = dotPos (p, b).getDistanceFrom (pt); if (d < bd) { bd = d; best = b; } }
        return best;
    }
    void timerCallback() override
    {
        const int pos = proc.specPos.load();
        for (int i = 0; i < kN; ++i) {
            const float w = 0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * (float) i / (float) (kN - 1));
            fftData[i] = proc.spec[(pos - kN + i) & (N1081Processor::kSpecSize - 1)] * w;
        }
        std::fill (fftData + kN, fftData + 2 * kN, 0.f);
        fft.performFrequencyOnlyForwardTransform (fftData);
        for (int b = 0; b < kN / 2; ++b) {
            const float db = Decibels::gainToDecibels (fftData[b] * 4.0f / (float) kN, -120.f);
            smooth[b] = db > smooth[b] ? db : smooth[b] * 0.894f + db * 0.106f;  // fast rise, gentle fall (same speed at 60 redraws a second)
        }
        repaint();
    }
    N1081Processor& proc;
    dsp::FFT fft;
    float fftData[2 * kN] {}, smooth[kN / 2] {};
    Rectangle<float> plot;
    int hover = -1, drag = -1;
};

// slim LED-style meter: IN / OUT in VU (0 VU = +4 dBu), GR in dB of gain reduction
class LedMeter : public Component
{
public:
    explicit LedMeter (N1081Processor& p) : proc (p) {}
    void reset() { level = proc.vuMode == 2 ? 0.0 : -30.0; repaint(); }
    void tick()
    {
        double target;
        if (proc.vuMode == 2) target = proc.vuGr.load();
        else target = jlimit (-30.0, 6.0, 20.0 * std::log10 ((proc.vuMode == 0 ? proc.vuIn.load() : proc.vuOut.load()) + 1e-9) + n1081::kDbuOffset - 4.0);
        level += (target - level) * 0.2;
        repaint();
    }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::well); g.fillRoundedRectangle (r, 3);
        auto in = r.reduced (2.0f);
        const bool gr = proc.vuMode == 2;
        const float frac = gr ? jlimit (0.f, 1.f, (float) level / 20.f) : jlimit (0.f, 1.f, (float) (level + 30.0) / 36.f);
        const float zero = in.getX() + in.getWidth() * (30.f / 36.f);
        auto fill = in.withWidth (in.getWidth() * frac);
        if (gr) { g.setColour (col::lampOn); g.fillRect (fill); }
        else {
            g.setColour (Colour (0xff7fc97f)); g.fillRect (fill.withRight (jmin (fill.getRight(), zero)));
            if (fill.getRight() > zero) { g.setColour (Colour (0xffc0392b)); g.fillRect (fill.withLeft (zero)); }
        }
        g.setColour (col::well);
        for (float x = in.getX() + 5; x < in.getRight(); x += 5) g.drawVerticalLine ((int) x, in.getY(), in.getBottom());
        g.setColour (col::legend); g.setFont (uiFont (10.5f, true));
        const String t = gr ? "GR " + String (-level, 1) : (level <= -29.9 ? String ("-30 VU") : signedDb (level, 1) + " VU");
        g.drawText (t, getLocalBounds().reduced (5, 0), Justification::centredRight);
    }
private:
    N1081Processor& proc; double level = 0;
};

//==============================================================================
class EqPanel : public ModulePanel
{
public:
    static constexpr int kOutX = 704, kOutW = 102;           // OUTPUT column: the header tools use the same x and width
    static constexpr int kW = kOutX + kOutW + 14, kH = 446;
    static constexpr int kHc = 340;                          // height with concentric knobs (Settings > Layout)
    EqPanel (N1081Processor& p, KnobLnF& l) : ModulePanel (p, l), graph (p)
    {
        addAndMakeVisible (graph);
        auto db = [] (double v) { return signedDb (v, 1) + " dB"; };
        knob (inGain, ids::inGain, col::capGrey, false, db);
        knob (line, ids::line, col::capRed, true);
        knob (hpf, ids::hpf, col::capGrey, true);
        knob (lpf, ids::lpf, col::capGrey, true);
        const Colour caps[4] = { col::capBlue, col::capGrey, col::capGrey, col::capBlue };
        for (int b = 0; b < 4; ++b) {
            knob (freq[b], ids::freq[b], caps[b], true);
            knob (gain[b], ids::gain[b], col::capBlack.brighter (0.25f), false, db);
            button (alt[b], ids::alt[b], {});
            alt[b].getProperties().set ("icon", b == 0 ? 1 : b == 3 ? 2 : 3);
            alt[b].setTooltip (String (b == 0 || b == 3 ? "Shelf / peak" : "Wide / sharp (Hi-Q)") + ".  Right-click or Ctrl+click: Q knob");
            // pressing the button always lands on the hardware curve for that setting
            alt[b].onClick = [this, b] { setQ (b, 1.0f); };
            alt[b].onSecondary = [this, b] { openQ (b); };
            knob (qk[b], ids::q[b], col::capGrey, false, [this, b] (double v) {
                auto prm = proc.currentParams(); prm.qMul[b] = v;
                const auto sp = n1081::bandSpec (prm, b);
                return sp.kind == n1081::Kind::Peak ? "Q " + String (sp.Q, 2) : String (v, 2) + "x"; });
            qk[b].setDoubleClickReturnValue (false, 1.0);
            qk[b].setTooltip ("Q range.  Double-click: back to the 1081 curve (right-click the button again to get this setting back)");
            qk[b].onClose = [this, b] { closeQ (b); };
        }
        knob (out, ids::out, col::capGrey, false, db);
        button (eqIn, ids::eqIn, "EQ");
        button (phase, ids::phase, String (CharPointer_UTF8 ("\xc3\x98")));
        phase.setTooltip ("Phase (polarity) invert");

        // concentric pairs for the compact layout: frequency = outer ring, gain = centre cap; HPF ring + LPF cap
        concentric (cHpf, cLpf, ids::hpf, ids::lpf, col::capGrey, col::capGrey.darker (0.25f), true, true);
        for (int b = 0; b < 4; ++b) concentric (cFreq[b], cGain[b], ids::freq[b], ids::gain[b], caps[b], col::capBlack.brighter (0.25f), true, false, {}, db);

        knob (analog, ids::analog, col::capGrey, false, [] (double v) { return "ANALOG " + signedDb (v, 0); });
        analog.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        analog.setPopupDisplayEnabled (true, true, nullptr, 1200);
        analog.setTooltip ("ANALOG: 0 = the circuit as modelled.  Down to -50 cleaner (no harmonics), up to +50 more colour.  "
                           "Level compensated.  Double-click: 0");
        arrows (26, 104);
        setSize (kW, kH);
    }
    Rectangle<int> toolArea() const { return { kOutX, 14, kOutW, 112 }; }
    void refresh() { for (int b = 0; b < 4; ++b) qk[b].updateText(); resized(); repaint(); }

    void paint (Graphics& g) override
    {
        paintPanel (g, col::panel.brighter (0.06f), col::panelDark, {}, true);
        g.setColour (col::legend); g.setFont (uiFont (30.f, true)); g.drawText ("1081+", 26, 22, 120, 34, Justification::left);
        g.setFont (uiFont (11.f, false)); g.setColour (col::dim);
        g.drawText ("CHANNEL AMPLIFIER", 26, 60, 130, 14, Justification::left);
        g.drawText ("CIRCUIT MODEL", 26, 76, 130, 14, Justification::left);
        g.setFont (uiFont (9.5f, true));
        g.drawText ("ANALOG", 74, 102, 38, 12, Justification::right);
    }

    void resized() override
    {
        rows.clear(); captions.clear();
        const bool cmp = proc.compactEq;
        for (auto* c : std::initializer_list<Component*> { &hpf, &lpf, &freq[0], &freq[1], &freq[2], &freq[3], &gain[0], &gain[1], &gain[2], &gain[3] }) c->setVisible (! cmp);
        for (auto* c : std::initializer_list<Component*> { &cHpf, &cLpf, &cFreq[0], &cFreq[1], &cFreq[2], &cFreq[3], &cGain[0], &cGain[1], &cGain[2], &cGain[3] }) c->setVisible (cmp);
        const int lower = cmp ? 0 : 36;                       // classic knobs: the controls sit at the bottom of the dark panels; the graph gets the room
        graph.setBounds (156, 14, kOutX - 156 - 8, 112 + lower);
        analog.setBounds (114, 90, 36, 36);
        const int top = 138 + lower, h = getHeight() - top - 12;
        const int off = cmp ? jmax (0, (h - (kHc - top - 12)) / 2) : 0;     // compact knobs centred if the strip is taller
        const int btnY = cmp ? h - 32 : 228;                                  // all shape / phase / EQ buttons on one line
        int x = 14;
        auto section = [&] (const String& t, int w) { Rectangle<int> r (x, top, w, h); rows.push_back ({ t, r }); x += w + 6; return r; };
        auto btn = [&] (Button& b, const Rectangle<int>& r) { b.setBounds (r.getCentreX() - kBtnW / 2, r.getY() + btnY, kBtnW, kBtnH); };
        auto r = section ("INPUT", 102);
        if (cmp) { put (inGain, {}, r.getCentreX(), r.getY() + 18 + off, 40, r.getWidth() - 8); put (line, "GAIN", r.getCentreX(), r.getY() + 78 + off, 50, r.getWidth() - 8); }
        else     { put (inGain, {}, r.getCentreX(), r.getY() + 22, 54, r.getWidth() - 8);       put (line, "GAIN", r.getCentreX(), r.getY() + 112, 70, r.getWidth() - 8); }
        btn (phase, r);
        r = section ("FILTERS", 96);
        if (cmp) putConcentric (cHpf, cLpf, "HP / LP", r.getCentreX(), r.getY() + 24 + off, 80, r.getWidth() - 4);
        else {
            put (hpf, "HIGH PASS", r.getCentreX(), r.getY() + 26, 60, r.getWidth() - 8);
            put (lpf, "LOW PASS", r.getCentreX(), r.getY() + 136, 60, r.getWidth() - 8);
        }
        const char* titles[4] = { "LF", "LOW MID", "HIGH MID", "HF" };
        for (int b = 0; b < 4; ++b) {
            r = section (titles[b], 114);
            if (cmp) putConcentric (cFreq[b], cGain[b], {}, r.getCentreX(), r.getY() + 20 + off, 96, r.getWidth() - 4);
            else {
                put (freq[b], {}, r.getCentreX(), r.getY() + 20, 80, r.getWidth() - 8);
                put (gain[b], {}, r.getCentreX(), r.getY() + 122, 80, r.getWidth() - 8);
            }
            btn (alt[b], r);
            qk[b].setTextBoxStyle (Slider::TextBoxRight, true, 52, 18);
            qk[b].setBounds (r.getCentreX() - 42, r.getY() + btnY - 1, 84, 28);
            alt[b].setVisible (! proc.qOpen[b]); qk[b].setVisible (proc.qOpen[b]);
        }
        r = Rectangle<int> (kOutX, top, kOutW, h); rows.push_back ({ "OUTPUT", r });
        put (out, "TRIM", r.getCentreX(), r.getY() + (cmp ? 22 + off : 26), 64, r.getWidth() - 8);
        btn (eqIn, r);
        eqIn.setButtonText (cmp ? "EQ IN" : "EQ");
        if (! cmp) captions.push_back ({ "EQ IN", { r.getX(), r.getY() + btnY - 16, r.getWidth(), 12 } });
    }

private:
    RangedAudioParameter* qParam (int b) const { return proc.apvts.getParameter (ids::q[b]); }
    float qNow (int b) const { auto* q = qParam (b); return q->convertFrom0to1 (q->getValue()); }
    void setQ (int b, float v) { auto* q = qParam (b); q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 (v)); q->endChangeGesture(); }
    void openQ (int b)
    {
        if (proc.qOpen[b]) return;
        proc.qOpen[b] = true;
        if (std::abs (qNow (b) - 1.0f) < 0.005f && std::abs (proc.qLast[b] - 1.0f) > 0.005f) setQ (b, proc.qLast[b]);   // the user's last range
        qk[b].updateText(); resized(); repaint();
    }
    void closeQ (int b)
    {
        if (std::abs (qNow (b) - 1.0f) > 0.005f) proc.qLast[b] = qNow (b);       // remembered for the next right-click
        setQ (b, 1.0f);
        proc.qOpen[b] = false;
        resized(); repaint();
    }

    Graph graph;
    FineKnob inGain, line, hpf, lpf, out, freq[4], gain[4], analog, cHpf, cFreq[4];
    CentreKnob cLpf, cGain[4];
    QKnob qk[4];
    CurveButton alt[4];
    TextButton eqIn, phase;
};

//==============================================================================
// Dynamics: compressor (2254 diode bridge, VCA = PlateVerb's clean VCA, or the Neve 88R) and the 88R gate in one panel
static String msShort (double v) { return v < 1 ? String (v, 2) : v < 1000 ? String (v, v < 10 ? 1 : 0) : String (v / 1000.0, 1) + "s"; }

class DynPanel : public ModulePanel, private Timer
{
public:
    static constexpr int kCols = 6, kColW = 70;
    static constexpr int kW = 12 * 2 + kCols * kColW;
    std::function<void()> onGateOrder;                   // flip gate before / after the compressor
    std::function<bool()> gateFirst;
    DynPanel (N1081Processor& p, KnobLnF& l) : ModulePanel (p, l), meter (p)
    {
        presets.setTextWhenNothingSelected ("PRESETS");
        const auto& list = presetList();
        for (int i = 0; i < (int) list.size(); ++i) presets.addItem (list[(size_t) i].name, i + 1);
        presets.onChange = [this] { const int i = presets.getSelectedItemIndex(); if (i >= 0) { applyPreset (presetList()[(size_t) i]); presets.setSelectedId (0, dontSendNotification); } };
        addAndMakeVisible (presets);

        type.addItemList ({ "2254", "VCA", "88R" }, 1);
        type.getProperties().set ("button", true);
        type.setColour (ComboBox::textColourId, col::ink);
        type.setTooltip ("Compressor type");
        addAndMakeVisible (type);
        typeAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, ids::cModel, type);
        type.onChange = [this] { layoutModel(); };

        onOff (compIn, ids::cIn);
        titleX = 14 + kSmallW + 8;
        addAndMakeVisible (meter);
        meterMode.onClick = [this] { proc.vuMode = (proc.vuMode + 1) % 3; syncVu(); meter.reset(); };
        meterMode.setTooltip ("Meter: IN / OUT level of the compressor, or GR (gain reduction)");
        addAndMakeVisible (meterMode);

        const Colour dark = col::capGrey.darker (0.35f), red = col::capRed, blue = col::capBlue, grey = col::capGrey;
        auto num = [] (double v) { return signedDb (v, 1); };
        auto pct = [] (double v) { return String ((int) std::round (v)) + "%"; };
        // shared by all three types
        knob (mixK, ids::cMix, grey, false, pct);
        knob (scK, ids::cSc, dark, true, [] (double v) { const int i = jlimit (0, 5, (int) std::round (v)); return i == 0 ? String ("OFF") : String ((int) n1081::kScHpf[i]) + " Hz"; });
        scK.setTooltip ("Sidechain high-pass: keeps the low end from pumping the compressor");
        knob (drvK, ids::cDrive, red, false, num);
        drvK.setTooltip ("Clean drive into the compressor");
        // 2254
        concentric (kThr, kRatio, ids::kThr, ids::kRatio, blue, dark, true, true, [] (double v) { return signedDb (n1081::compThrDbu ((int) std::round (v))); });
        concentric (kRec, kGain, ids::kRec, ids::kGain, grey, red, true, true, {}, [] (double v) { return signedDb (n1081::compGainDb ((int) std::round (v))); });
        concentric (kLim, kLimRec, ids::kLimLvl, ids::kLimRec, grey, dark, true, true, [] (double v) { return signedDb (n1081::limThrDbu ((int) std::round (v))); });
        kLim.setTooltip ("Limiter level in dBu (ring) / limiter recovery (cap)");
        button (kComp, ids::kComp, "COMP");
        button (kFast, ids::kFast, "FAST");
        button (kLimit, ids::kLim, "LIMIT");
        // VCA
        concentric (vThr, vRatio, ids::pThr, ids::pRatio, blue, dark, false, false, num, [] (double v) { return String (v, 1) + ":1"; });
        concentric (vAtk, vRel, ids::pAtk, ids::pRel, grey, dark, false, false, msShort, msShort);
        knob (vMk, ids::pMk, red.darker (0.3f), false, num);
        button (vTone, ids::pTone, "VINTAGE");
        // 88R
        auto* rPadP = proc.apvts.getRawParameterValue (ids::rPad);
        concentric (rThr, rRatio, ids::rThr, ids::rRatio, blue, dark, false, false,
                    [rPadP] (double v) { return signedDb (v - (rPadP->load() > 0.5f ? 20.0 : 0.0), 1); },
                    [] (double v) { return v >= 29.95 ? String ("inf") : String (v, 1) + ":1"; });
        rThr.setTooltip ("Threshold in dBu (ring, +20 ... -10; with -20 dB: 0 ... -30) / ratio (cap, 1:1 ... inf)");
        concentric (rAtk, rRel, ids::rAtk, ids::rRel, grey, dark, false, false,
                    [] (double v) { return String (v, 1); }, msShort);
        rAtk.setTooltip ("Attack 3 ... 7 ms (FAST: 1 ... 7 ms) (ring) / release 10 ms ... 3 s (cap); AUTO: programme dependent release");
        knob (rGain, ids::rGain, red.darker (0.3f), false, num);
        button (rFast, ids::rFast, "FAST");
        button (rAuto, ids::rAuto, "AUTO");
        button (rPad, ids::rPad, "-20 dB");
        rPad.onStateChange = [this] { rThr.updateText(); };
        // gate (88R gate / expander)
        onOff (gIn, ids::gIn);
        auto* gPadP = proc.apvts.getRawParameterValue (ids::gPad);
        concentric (gThr, gRange, ids::gThr, ids::gRange, blue, dark, false, false,
                    [gPadP] (double v) { return signedDb (v - (gPadP->load() > 0.5f ? 40.0 : 0.0), 1); },
                    [] (double v) { return String (v, 1); });
        gThr.setTooltip ("Threshold in dBu (ring, -25 ... +15; with -40 dB: -65 ... -25) / range (cap, 0 ... 60 dB)");
        concentric (gRel, gHyst, ids::gRel, ids::gHyst, grey, dark, false, false, msShort,
                    [] (double v) { return v < 0.1 ? String ("EXP") : String (v, 1); });
        gRel.setTooltip ("Release 10 ms ... 3 s (ring) / hysteresis 0 ... 25 dB (cap); fully down = 1:2 expander");
        button (gFast, ids::gFast, "FAST");
        gFast.setTooltip ("Attack 500 us, FAST: 50 us");
        button (gPad, ids::gPad, "-40 dB");
        gPad.onStateChange = [this] { gThr.updateText(); };
        gOrder.setTooltip ("Gate before (PRE) or after (POST) the compressor");
        gOrder.onClick = [this] { if (onGateOrder) onGateOrder(); };
        gOrder.setButtonText ("POST");
        addAndMakeVisible (gOrder);

        for (auto* s : std::initializer_list<Slider*> { &mixK, &scK, &drvK, &vMk, &rGain }) s->getProperties().set ("small", true);
        for (auto* b : std::initializer_list<Button*> { &compIn, &meterMode, &kComp, &kFast, &kLimit, &vTone, &rFast, &rAuto, &rPad, &gIn, &gFast, &gPad, &gOrder })
            b->getProperties().set ("text", true);

        arrows (14, 44);
        setSize (kW, EqPanel::kHc);
        syncVu();
        layoutModel();
        startTimerHz (30);
    }
    void syncVu() { const char* n[3] = { "IN", "OUT", "GR" }; meterMode.setButtonText (n[jlimit (0, 2, proc.vuMode)]); }

    void paint (Graphics& g) override
    {
        const char* names[3] = { "2254", "VCA", "88R" };
        paintPanel (g, col::comp.brighter (0.05f), col::compDark, names[model()]);
        g.setColour (col::legend); g.setFont (uiFont (14.f, true));
        g.drawText ("GATE", gateTitle, Justification::left);
        const float a = proc.gateAtten.load();
        const auto bar = gateBar.toFloat();
        g.setColour (col::well); g.fillRoundedRectangle (bar, 3);
        const float frac = jlimit (0.f, 1.f, a / jmax (1.f, proc.apvts.getRawParameterValue (ids::gRange)->load()));
        g.setColour (col::lampOn.withAlpha (0.85f)); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * frac).reduced (2), 2);
        const bool open = proc.apvts.getRawParameterValue (ids::gIn)->load() > 0.5f && a < 1.0f;
        g.setColour (open ? Colour (0xff7fc97f) : col::well.brighter (0.2f));
        g.fillEllipse (Rectangle<float> (9, 9).withCentre ({ bar.getRight() - 9, bar.getCentreY() }));
    }

    void layoutModel()
    {
        const int m = model();
        for (auto* c : std::initializer_list<Component*> { &kThr, &kRatio, &kRec, &kGain, &kLim, &kLimRec, &kComp, &kFast, &kLimit }) c->setVisible (m == 0);
        for (auto* c : std::initializer_list<Component*> { &vThr, &vRatio, &vAtk, &vRel, &vMk, &vTone }) c->setVisible (m == 1);
        for (auto* c : std::initializer_list<Component*> { &rThr, &rRatio, &rAtk, &rRel, &rGain, &rFast, &rAuto, &rPad }) c->setVisible (m == 2);
        resized(); repaint();
    }

    void resized() override
    {
        rows.clear(); captions.clear();
        const int m = model();
        const int h = getHeight();
        // every knob the same size: as big as the panel height allows, up to the classic THR / RATIO size
        const int ks = jmin (66, (h - 218) / 2);
        const int spare = jmax (0, h - 10 - (208 + 2 * ks)), gap = spare / 4;
        compIn.setBounds (14, 13, kSmallW, kBtnH);
        presets.setBounds (kW - 14 - 84, 13, 84, kBtnH);
        meter.setBounds (66, 44, kW - 14 - kSmallW - 6 - 66, 20);
        meterMode.setBounds (kW - 14 - kSmallW, 43, kSmallW, 22);

        int cx[kCols]; for (int i = 0; i < kCols; ++i) cx[i] = 12 + kColW / 2 + i * kColW;
        const int colW = kColW - 2;
        const int y0 = 68 + gap, ra = y0 + 6, top = ra + 24 + 6 + gap / 2;
        // type selector, and the type's switches above the knobs they belong to
        type.setBounds (cx[0] - 30, ra, 60, 24);
        auto sw = [&] (Button& b, int centre, int w) { b.setBounds (centre - w / 2, ra, w, 24); };
        if (m == 0)      { sw (kComp, cx[2], 56); sw (kFast, cx[3], 56); sw (kLimit, cx[4], 56); }
        else if (m == 1) { sw (vTone, cx[2], 66); }
        else             { sw (rPad, cx[2], 56); sw (rFast, cx[3] - 18, 36); sw (rAuto, cx[3] + 18, 36); }
        // MIX under the type selector, SC HPF next to it, then the type's own knobs, DRIVE last
        put (mixK, "MIX", cx[0], top, ks, colW);
        put (scK, "SC HPF", cx[1], top, ks, colW);
        if (m == 0) {
            putConcentric (kThr, kRatio, "THR / RATIO", cx[2], top, ks, colW);
            putConcentric (kRec, kGain, "REC / GAIN", cx[3], top, ks, colW);
            putConcentric (kLim, kLimRec, "LIMIT / REC", cx[4], top, ks, colW);
        } else if (m == 1) {
            putConcentric (vThr, vRatio, "THR / RATIO", cx[2], top, ks, colW);
            putConcentric (vAtk, vRel, "ATK / REL ms", cx[3], top, ks, colW);
            put (vMk, "MAKEUP", cx[4], top, ks, colW);
        } else {
            putConcentric (rThr, rRatio, "THR / RATIO", cx[2], top, ks, colW);
            putConcentric (rAtk, rRel, "ATK / REL ms", cx[3], top, ks, colW);
            put (rGain, "GAIN", cx[4], top, ks, colW);
        }
        put (drvK, "DRIVE", cx[5], top, ks, colW);
        const int cBottom = top + ks + 30 + 4;
        rows.push_back ({ {}, { 10, y0, kW - 20, cBottom - y0 } });

        const int gy = cBottom + 6 + gap;
        Rectangle<int> gbox (10, gy, kW - 20, jmax (0, h - 10 - gap - gy)); rows.push_back ({ {}, gbox });
        gIn.setBounds (16, gy + 4, kSmallW, 22);
        gateTitle = { 16 + kSmallW + 6, gy + 4, 44, 22 };
        gOrder.setBounds (kW - 16 - 44, gy + 4, 44, 22);
        gateBar = { gateTitle.getRight() + 2, gy + 8, gOrder.getX() - 6 - gateTitle.getRight() - 2, 14 };
        const int gt = gy + 30 + jmax (0, (gbox.getBottom() - gy - 30 - (ks + 30 + 4)) / 2);
        putConcentric (gThr, gRange, "THR / RANGE", cx[0], gt, ks, colW);
        putConcentric (gRel, gHyst, "REL / HYST", cx[1], gt, ks, colW);
        gPad.setBounds (cx[2] - 28, gt + ks / 2 - 12, 56, 24);
        gFast.setBounds (cx[3] - 28, gt + ks / 2 - 12, 56, 24);
    }

private:
    void timerCallback() override
    {
        meter.tick();
        const String t = gateFirst && gateFirst() ? "PRE" : "POST";
        if (gOrder.getButtonText() != t) gOrder.setButtonText (t);
        repaint (gateBar.expanded (4));
    }
    struct Preset { String name; std::vector<std::pair<const char*, float>> values; };
    static const std::vector<Preset>& presetList()
    {
        static const std::vector<Preset> p = {
            { "2254 - Vocal smooth",     { { ids::cModel, 0 }, { ids::kComp, 1 }, { ids::kLim, 0 }, { ids::kThr, 6 }, { ids::kRatio, 2 }, { ids::kRec, 3 }, { ids::kGain, 3 }, { ids::kFast, 0 }, { ids::cSc, 2 }, { ids::cMix, 100 } } },
            { "2254 - Drum bus glue",    { { ids::cModel, 0 }, { ids::kComp, 1 }, { ids::kLim, 0 }, { ids::kThr, 8 }, { ids::kRatio, 1 }, { ids::kRec, 0 }, { ids::kGain, 2 }, { ids::kFast, 0 }, { ids::cSc, 3 }, { ids::cMix, 70 } } },
            { "2254 - Bass even",        { { ids::cModel, 0 }, { ids::kComp, 1 }, { ids::kLim, 1 }, { ids::kLimLvl, 2 }, { ids::kLimRec, 1 }, { ids::kThr, 5 }, { ids::kRatio, 3 }, { ids::kRec, 1 }, { ids::kGain, 3 }, { ids::cSc, 0 }, { ids::cMix, 100 } } },
            { "2254 - Mix bus",          { { ids::cModel, 0 }, { ids::kComp, 1 }, { ids::kLim, 0 }, { ids::kThr, 10 }, { ids::kRatio, 0 }, { ids::kRec, 3 }, { ids::kGain, 1 }, { ids::kFast, 0 }, { ids::cSc, 3 }, { ids::cMix, 100 } } },
            { "2254 - Parallel smash",   { { ids::cModel, 0 }, { ids::kComp, 1 }, { ids::kLim, 0 }, { ids::kThr, 0 }, { ids::kRatio, 4 }, { ids::kRec, 0 }, { ids::kGain, 7 }, { ids::kFast, 1 }, { ids::cSc, 2 }, { ids::cMix, 35 } } },
            { "2254 - Peak limiter",     { { ids::cModel, 0 }, { ids::kComp, 0 }, { ids::kLim, 1 }, { ids::kLimLvl, 3 }, { ids::kLimRec, 1 }, { ids::kFast, 1 }, { ids::kGain, 0 }, { ids::cMix, 100 } } },
            { "VCA - Mix glue",          { { ids::cModel, 1 }, { ids::pThr, -18 }, { ids::pRatio, 2 }, { ids::pAtk, 30 }, { ids::pRel, 300 }, { ids::pMk, 2 }, { ids::pTone, 0 }, { ids::cSc, 3 }, { ids::cMix, 100 } } },
        };
        return p;
    }
    void applyPreset (const Preset& pr)
    {
        auto set = [this] (const char* id, float v) { if (auto* q = proc.apvts.getParameter (id)) { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 (v)); q->endChangeGesture(); } };
        set (ids::cIn, 1); set (ids::cDrive, 0);
        for (auto& v : pr.values) set (v.first, v.second);
    }
    int model() const { return jlimit (0, 2, type.getSelectedItemIndex()); }

    LedMeter meter;
    ComboBox presets, type;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt;
    TextButton compIn, meterMode, kComp, kFast, kLimit, vTone, rFast, rAuto, rPad, gIn, gFast, gPad, gOrder;
    FineKnob mixK, scK, drvK, kThr, kRec, kLim, vThr, vAtk, vMk, rThr, rAtk, rGain, gThr, gRel;
    CentreKnob kRatio, kGain, kLimRec, vRatio, vRel, rRatio, rRel, gRange, gHyst;
    Rectangle<int> gateTitle, gateBar;
};

//==============================================================================
// the swap sign between the EQ and the dynamics: click it and they change places (and order in the signal chain)
struct SwapButton : public Button
{
    SwapButton() : Button ("Swap") { setTooltip ("Swap: EQ and dynamics change places, and their order in the signal chain"); }
    void paintButton (Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (Colour (0xff1c2024).withAlpha (over ? 1.0f : 0.9f)); g.fillEllipse (r);
        g.setColour (Colour (0xffece6d6).withAlpha (down ? 0.6f : 0.9f)); g.drawEllipse (r, 1.0f);
        const auto c = r.getCentre(); const float w = r.getWidth() * 0.27f, h = r.getHeight() * 0.13f;
        Path p;
        p.startNewSubPath (c.x - w, c.y - h); p.lineTo (c.x + w, c.y - h);
        p.startNewSubPath (c.x + w * 0.45f, c.y - h * 2.2f); p.lineTo (c.x + w, c.y - h); p.lineTo (c.x + w * 0.45f, c.y + h * 0.2f);
        p.startNewSubPath (c.x + w, c.y + h); p.lineTo (c.x - w, c.y + h);
        p.startNewSubPath (c.x - w * 0.45f, c.y - h * 0.2f); p.lineTo (c.x - w, c.y + h); p.lineTo (c.x - w * 0.45f, c.y + h * 2.2f);
        g.strokePath (p, PathStrokeType (1.5f, PathStrokeType::curved, PathStrokeType::rounded));
    }
};

class N1081Editor : public AudioProcessorEditor, private Timer
{
public:
    explicit N1081Editor (N1081Processor& p) : AudioProcessorEditor (p), proc (p), eq (p, lnf), dyn (p, lnf)
    {
        addAndMakeVisible (eq); addChildComponent (dyn);
        eq.onMove  = [this] (int d) { if (proc.compOpen) placeDyn (d > 0); };     // EQ right -> dynamics left
        dyn.onMove = [this] (int d) { placeDyn (d < 0); };
        dyn.gateFirst = [this] { int o[3]; proc.currentOrder (o); return pos (o, ModGate) < pos (o, ModComp); };
        dyn.onGateOrder = [this] { int o[3]; proc.currentOrder (o); setOrder (pos (o, ModComp) < pos (o, ModEq), ! dyn.gateFirst()); };
        for (auto* b : { &zoomOut, &zoomIn, &zoomPct, &settings, &dynBtn }) {
            b->getProperties().set ("flat", true); b->setLookAndFeel (&lnf); addAndMakeVisible (*b);
        }
        zoomOut.setButtonText ("-"); zoomIn.setButtonText ("+"); settings.setButtonText ("SETTINGS");
        zoomOut.onClick = [this] { setScale (scale - 0.1f); };
        zoomIn.onClick  = [this] { setScale (scale + 0.1f); };
        zoomPct.onClick = [this] { zoomMenu(); };
        settings.onClick = [this] { settingsMenu(); };
        dynBtn.setTooltip ("Show / hide the compressor and gate (lit when either is on)");
        addChildComponent (swap);
        swap.onClick = [this] { int o[3]; proc.currentOrder (o); placeDyn (! (pos (o, ModComp) < pos (o, ModEq))); };
        dynBtn.onClick = [this] { proc.compOpen = ! proc.compOpen; applySize(); };
        scale = jlimit (0.6f, 2.0f, proc.uiScale);
        lastState = proc.uiStateVersion.load();
        setResizable (true, true);
        applySize();
        ready = true;
        timerCallback();
        startTimerHz (30);
    }
    ~N1081Editor() override { for (auto* b : { &zoomOut, &zoomIn, &zoomPct, &settings, &dynBtn }) b->setLookAndFeel (nullptr); }

    void resized() override
    {
        const float s = (float) getWidth() / (float) baseWidth();
        if (ready && std::abs (s - scale) > 0.004f) { scale = jlimit (0.6f, 2.0f, s); proc.uiScale = scale; }
        const auto t = AffineTransform::scale (scale);
        int o[3]; proc.currentOrder (o);
        const bool dynLeft = proc.compOpen && pos (o, ModComp) < pos (o, ModEq);
        dyn.setVisible (proc.compOpen);
        const int eqX = dynLeft ? DynPanel::kW : 0;
        eq.setBounds (eqX, 0, EqPanel::kW, baseHeight()); eq.setTransform (t);
        if (proc.compOpen) { dyn.setBounds (dynLeft ? 0 : EqPanel::kW, 0, DynPanel::kW, baseHeight()); dyn.setTransform (t); }
        swap.setVisible (proc.compOpen);
        if (proc.compOpen) { const int jx = dynLeft ? DynPanel::kW : EqPanel::kW; swap.setBounds (jx - 13, 1, 26, 26); swap.setTransform (t); swap.toFront (false); }
        // header tools: exactly the OUTPUT column's x and width
        const auto a = eq.toolArea().translated (eqX, 0);
        const int h = 22, gap = 6;
        auto place = [&] (Button& b, Rectangle<int> r) { b.setBounds (r); b.setTransform (t); };
        place (zoomOut, { a.getX(), a.getY(), 24, h });
        place (zoomPct, { a.getX() + 27, a.getY(), a.getWidth() - 54, h });
        place (zoomIn,  { a.getRight() - 24, a.getY(), 24, h });
        place (settings, { a.getX(), a.getY() + (h + gap), a.getWidth(), h });
        place (dynBtn,   { a.getX(), a.getY() + 2 * (h + gap), a.getWidth(), h });
        zoomPct.setButtonText (String ((int) std::round (scale * 100.0f)) + "%");
    }

private:
    static int pos (const int o[3], int m) { for (int i = 0; i < 3; ++i) if (o[i] == m) return i; return 0; }
    int baseWidth() const { return EqPanel::kW + (proc.compOpen ? DynPanel::kW : 0); }
    int baseHeight() const { return proc.compactEq ? EqPanel::kHc : EqPanel::kH; }
    // signal order follows the screen: dynamics left or right of the EQ; inside it the gate PRE or POST the compressor
    void setOrder (bool dynLeft, bool gateFirst)
    {
        const int a = gateFirst ? ModGate : ModComp, b = gateFirst ? ModComp : ModGate;
        const int o[3] = { dynLeft ? a : ModEq, dynLeft ? b : a, dynLeft ? ModEq : b };
        if (auto* prm = proc.apvts.getParameter (ids::order)) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) permIndex (o)));
        resized();
    }
    void placeDyn (bool left) { setOrder (left, dyn.gateFirst()); }
    void applySize()
    {
        const int bw = baseWidth(), bh = baseHeight();
        if (auto* c = getConstrainer()) {
            c->setFixedAspectRatio ((double) bw / bh);
            c->setSizeLimits ((int) (bw * 0.6f), (int) (bh * 0.6f), bw * 2, bh * 2);
        }
        setSize ((int) std::round (bw * scale), (int) std::round (bh * scale));
        resized();
        timerCallback();
    }
    void setScale (float s) { scale = jlimit (0.6f, 2.0f, std::round (s * 10.0f) / 10.0f); proc.uiScale = scale; applySize(); }
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
        m.addSectionHeader ("LAYOUT");
        m.addItem (40, "Classic knobs", true, ! proc.compactEq);
        m.addItem (41, "Concentric knobs  (frequency ring + gain cap, smaller plugin)", true, proc.compactEq);
        m.addSeparator();
        m.addItem (30, "Running at " + String (proc.activeFactor.load()) + "x, 64-bit processing", false, false);
        Component::SafePointer<N1081Editor> safe (this);
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&settings), [os, filt, safe] (int r) {
            if (r >= 1 && r <= 6) os->setValueNotifyingHost (os->convertTo0to1 ((float) (r - 1)));
            if (r == 20) filt->setValueNotifyingHost (0.0f);
            if (r == 21) filt->setValueNotifyingHost (1.0f);
            if ((r == 40 || r == 41) && safe != nullptr) { safe->proc.compactEq = r == 41; safe->eq.refresh(); safe->applySize(); }
        });
    }
    void timerCallback() override
    {
        const bool on = proc.apvts.getRawParameterValue (ids::cIn)->load() > 0.5f || proc.apvts.getRawParameterValue (ids::gIn)->load() > 0.5f;
        const String t = "DYNAMICS";
        if (dynBtn.getButtonText() != t) dynBtn.setButtonText (t);
        dynBtn.setToggleState (on, dontSendNotification);
        const int ord = (int) proc.apvts.getRawParameterValue (ids::order)->load();
        if (ord != lastOrder) { lastOrder = ord; resized(); }
        const int st = proc.uiStateVersion.load();
        if (st != lastState && ready) { lastState = st; scale = jlimit (0.6f, 2.0f, proc.uiScale); dyn.syncVu(); eq.refresh(); applySize(); }
    }

    N1081Processor& proc;
    KnobLnF lnf;
    EqPanel eq;
    DynPanel dyn;
    TextButton zoomOut, zoomIn, zoomPct, settings, dynBtn;
    SwapButton swap;
    TooltipWindow tips { this, 700 };
    float scale = 1.0f; bool ready = false; int lastOrder = -1, lastState = 0;
};

AudioProcessorEditor* N1081Processor::createEditor() { return new N1081Editor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new N1081Processor(); }
