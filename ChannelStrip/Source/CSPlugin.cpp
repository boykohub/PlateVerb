// ChannelStrip - preamp, filters, gate, compressor (PlateVerb), 4-band multi-model EQ and limiter,
// in any order, with input polarity / phase rotation and an optional external sidechain.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "CSEngine.h"

using namespace juce;
using APVTS = AudioProcessorValueTreeState;

static StringArray eqModelNames()  { StringArray s; for (int i = 0; i < cs::kNumEqModels; ++i) s.add (cs::eqModelName (i)); return s; }
static StringArray preModelNames() { StringArray s; for (int i = 0; i < cs::kNumPreModels; ++i) s.add (cs::preModelName (i)); return s; }
static StringArray compTypeNames() { return { "Clean VCA", "1176-style FET", "LA-2A-style Opto" }; }   // PlateVerb's types (combo removed)
static const char* const kBandNames[] = { "LF", "LMF", "HMF", "HF" };
static const float kBandDefaultHz[] = { 110.0f, 700.0f, 3200.0f, 12000.0f };
static String bid (int b, const char* base) { return "b" + String (b) + base; }

class ChannelStripProcessor : public AudioProcessor
{
public:
    ChannelStripProcessor()
        : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                           .withOutput ("Output", AudioChannelSet::stereo(), true)
                                           .withInput ("Sidechain", AudioChannelSet::stereo(), false)),
          apvts (*this, &undo, "PARAMS", createLayout())
    {
        auto P = [this] (const String& id) { return apvts.getRawParameterValue (id); };
        q.os = P ("os"); q.inGain = P ("inGain"); q.polL = P ("polL"); q.polR = P ("polR"); q.rotOn = P ("rotOn"); q.rotDeg = P ("rotDeg");
        q.inMode = P ("inMode"); q.outGain = P ("outGain"); q.order = P ("order");
        q.preOn = P ("preOn"); q.preModel = P ("preModel"); q.preDrive = P ("preDrive"); q.preOut = P ("preOut");
        q.fltOn = P ("fltOn"); q.hpOn = P ("hpOn"); q.hpFreq = P ("hpFreq"); q.hpSlope = P ("hpSlope"); q.hpMode = P ("hpMode");
        q.lpOn = P ("lpOn"); q.lpFreq = P ("lpFreq"); q.lpSlope = P ("lpSlope"); q.lpMode = P ("lpMode"); q.fltChar = P ("fltChar");
        q.gtOn = P ("gtOn"); q.gtMode = P ("gtMode"); q.gtThr = P ("gtThr"); q.gtRange = P ("gtRange"); q.gtAtk = P ("gtAtk"); q.gtHold = P ("gtHold");
        q.gtRel = P ("gtRel"); q.gtHyst = P ("gtHyst"); q.gtKey = P ("gtKey"); q.gtKeyHp = P ("gtKeyHp"); q.gtKeyLp = P ("gtKeyLp"); q.gtListen = P ("gtListen");
        q.eqOn = P ("eqOn"); q.eqDrive = P ("eqDrive");
        for (int b = 0; b < 4; ++b)
        {
            auto& e = q.band[b];
            e.on = P (bid (b, "On")); e.model = P (bid (b, "Model")); e.shape = P (bid (b, "Shape")); e.freq = P (bid (b, "Freq"));
            e.gain = P (bid (b, "Gain")); e.qs = P (bid (b, "Q")); e.step = P (bid (b, "Step")); e.aux = P (bid (b, "Aux")); e.auxSel = P (bid (b, "AuxSel"));
        }
        q.cpOn = P ("cpOn"); q.cpType = P ("cpType"); q.cpTone = P ("cpTone"); q.cpThr = P ("cpThr"); q.cpRatio = P ("cpRatio"); q.cpAtk = P ("cpAtk");
        q.cpRel = P ("cpRel"); q.cpMakeup = P ("cpMakeup"); q.cpMix = P ("cpMix"); q.cpKey = P ("cpKey"); q.cpScHp = P ("cpScHp");
        q.cpLpFreq = P ("cpLpFreq"); q.cpLpMode = P ("cpLpMode");
        q.lmOn = P ("lmOn"); q.lmDrive = P ("lmDrive"); q.lmCeil = P ("lmCeil"); q.lmRel = P ("lmRel");
    }

    static APVTS::ParameterLayout createLayout()
    {
        APVTS::ParameterLayout L;
        auto fp = [] (const String& id, const String& name, float lo, float hi, float def, float centre = -1.0f, const String& unit = {}, int dec = 1)
        {
            NormalisableRange<float> r (lo, hi);
            if (centre > lo && centre < hi) r.setSkewForCentre (centre);
            return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                AudioParameterFloatAttributes().withLabel (unit).withStringFromValueFunction ([dec] (float v, int) { return String (v, dec); }));
        };
        auto bp = [] (const String& id, const String& name, bool def) { return std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def); };
        auto cp = [] (const String& id, const String& name, const StringArray& items, int def) { return std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def); };

        L.add (cp ("os", "Oversampling", { "1x", "2x", "4x" }, 1));
        L.add (fp ("inGain", "Input Gain", -24.0f, 24.0f, 0.0f, -1, "dB"));
        L.add (bp ("polL", "Polarity L", false)); L.add (bp ("polR", "Polarity R", false));
        L.add (bp ("rotOn", "Phase Rotate", false));
        L.add (fp ("rotDeg", "Phase Angle", -180.0f, 180.0f, 0.0f, -1, "deg", 0));
        L.add (cp ("inMode", "Input Mode", { "Stereo", "Mono", "Left", "Right", "Swap" }, 0));
        L.add (fp ("outGain", "Output Gain", -24.0f, 24.0f, 0.0f, -1, "dB"));
        L.add (std::make_unique<AudioParameterInt> (ParameterID { "order", 1 }, "Module Order", 0, 719, cs::defaultOrderIndex()));

        L.add (bp ("preOn", "Pre On", true));
        L.add (cp ("preModel", "Pre Model", preModelNames(), cs::PreTube));
        L.add (fp ("preDrive", "Pre Drive", 0.0f, 30.0f, 0.0f, 10.0f, "dB"));
        L.add (fp ("preOut", "Pre Output", -24.0f, 24.0f, 0.0f, -1, "dB"));

        L.add (bp ("fltOn", "Filters On", true));
        L.add (bp ("hpOn", "Low Cut On", false));
        L.add (fp ("hpFreq", "Low Cut Freq", 10.0f, 1000.0f, 80.0f, 80.0f, "Hz", 0));
        L.add (cp ("hpSlope", "Low Cut Slope", { "6 dB", "12 dB", "18 dB", "24 dB", "36 dB" }, 2));
        L.add (cp ("hpMode", "Low Cut Steps", { "Variable", "1073 steps", "1084 steps" }, 1));
        L.add (bp ("lpOn", "High Cut On", false));
        L.add (fp ("lpFreq", "High Cut Freq", 1000.0f, 30000.0f, 18000.0f, 8000.0f, "Hz", 0));
        L.add (cp ("lpSlope", "High Cut Slope", { "6 dB", "12 dB", "18 dB", "24 dB" }, 1));
        L.add (cp ("lpMode", "High Cut Steps", { "Variable", "1084 steps" }, 0));
        L.add (cp ("fltChar", "Filter Character", { "Clean", "Vintage" }, 1));

        L.add (bp ("gtOn", "Gate On", false));
        L.add (cp ("gtMode", "Gate Mode", { "Gate", "Expander" }, 0));
        L.add (fp ("gtThr", "Gate Threshold", -80.0f, 0.0f, -50.0f, -1, "dB"));
        L.add (fp ("gtRange", "Gate Range", 0.0f, 80.0f, 40.0f, -1, "dB"));
        L.add (fp ("gtAtk", "Gate Attack", 0.02f, 100.0f, 0.5f, 5.0f, "ms", 2));
        L.add (fp ("gtHold", "Gate Hold", 0.0f, 1000.0f, 50.0f, 100.0f, "ms", 0));
        L.add (fp ("gtRel", "Gate Release", 5.0f, 4000.0f, 150.0f, 300.0f, "ms", 0));
        L.add (fp ("gtHyst", "Gate Hysteresis", 0.0f, 12.0f, 3.0f, -1, "dB"));
        L.add (cp ("gtKey", "Gate Key", { "Internal", "Sidechain" }, 0));
        L.add (fp ("gtKeyHp", "Gate Key HPF", 20.0f, 5000.0f, 20.0f, 300.0f, "Hz", 0));
        L.add (fp ("gtKeyLp", "Gate Key LPF", 200.0f, 20000.0f, 20000.0f, 4000.0f, "Hz", 0));
        L.add (bp ("gtListen", "Gate Key Listen", false));

        L.add (bp ("eqOn", "EQ On", true));
        L.add (fp ("eqDrive", "EQ Drive", 0.0f, 100.0f, 50.0f, -1, "%", 0));
        for (int b = 0; b < 4; ++b)
        {
            const String n = String (kBandNames[b]) + " ";
            L.add (bp (bid (b, "On"), n + "On", true));
            L.add (cp (bid (b, "Model"), n + "Model", eqModelNames(), b == 0 || b == 3 ? cs::EqNevePA : cs::EqNeveFC));
            L.add (cp (bid (b, "Shape"), n + "Shape", { "Bell", "Shelf" }, (b == 0 || b == 3) ? 1 : 0));
            L.add (fp (bid (b, "Freq"), n + "Freq", 20.0f, 20000.0f, kBandDefaultHz[b], 1000.0f, "Hz", 0));
            L.add (fp (bid (b, "Gain"), n + "Gain", -20.0f, 20.0f, 0.0f, -1, "dB"));
            L.add (fp (bid (b, "Q"), n + "Q", 0.25f, 4.0f, 1.0f, 1.0f, "x", 2));
            L.add (bp (bid (b, "Step"), n + "Original Steps", true));
            L.add (fp (bid (b, "Aux"), n + "Atten", -17.5f, 0.0f, 0.0f, -1, "dB"));
            L.add (cp (bid (b, "AuxSel"), n + "Atten Select", { "5k", "10k", "20k" }, 1));
        }

        L.add (bp ("cpOn", "Comp On", false));
        L.add (cp ("cpType", "Comp Type", compTypeNames(), 0));
        L.add (cp ("cpTone", "Comp Tone", { "Clean", "Vintage" }, 0));
        L.add (fp ("cpThr", "Comp Threshold", -80.0f, 0.0f, -20.0f, -1, "dB"));
        L.add (fp ("cpRatio", "Comp Ratio", 1.0f, 20.0f, 4.0f, 4.0f, ":1"));
        L.add (fp ("cpAtk", "Comp Attack", 0.00002f, 0.3f, 0.01f, 0.01f, "s", 5));
        L.add (fp ("cpRel", "Comp Release", 0.02f, 10.0f, 0.3f, 0.5f, "s", 3));
        L.add (fp ("cpMakeup", "Comp Makeup", 0.0f, 24.0f, 0.0f, -1, "dB"));
        L.add (fp ("cpMix", "Comp Mix", 0.0f, 100.0f, 100.0f, -1, "%", 0));
        L.add (cp ("cpKey", "Comp Key", { "Internal", "Sidechain" }, 0));
        L.add (fp ("cpScHp", "Comp SC HPF", 20.0f, 500.0f, 20.0f, 100.0f, "Hz", 0));
        L.add (fp ("cpLpFreq", "Comp Low Pass", 1000.0f, 40000.0f, 40000.0f, 10000.0f, "Hz", 0));
        L.add (cp ("cpLpMode", "Comp LP Sound", { "Model", "Clean" }, 0));

        L.add (bp ("lmOn", "Limiter On", false));
        L.add (fp ("lmDrive", "Limiter Drive", 0.0f, 18.0f, 0.0f, -1, "dB"));
        L.add (fp ("lmCeil", "Limiter Ceiling", -12.0f, 0.0f, -0.3f, -1, "dB"));
        L.add (fp ("lmRel", "Limiter Release", 1.0f, 1000.0f, 60.0f, 60.0f, "ms", 0));
        return L;
    }

    // -------------------------------------------------------------------- audio
    void prepareToPlay (double sr, int block) override
    {
        baseFs = (float) sr; maxBlock = jmax (1, block);
        for (int i = 0; i < 3; ++i)
        {
            os[i] = std::make_unique<dsp::Oversampling<float>> (2, (size_t) i, dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
            os[i]->initProcessing ((size_t) maxBlock);
        }
        scBuf.setSize (2, maxBlock * 4); workIn.setSize (2, maxBlock);
        curOs = -1; ensureRate (osIndex());
    }
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& l) const override
    {
        const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
        if (out != AudioChannelSet::stereo() && out != AudioChannelSet::mono()) return false;
        if (in != out) return false;
        if (l.inputBuses.size() > 1)
        {
            const auto sc = l.getChannelSet (true, 1);
            if (! sc.isDisabled() && sc != AudioChannelSet::mono() && sc != AudioChannelSet::stereo()) return false;
        }
        return true;
    }

    int osIndex() const { return jlimit (0, 2, (int) q.os->load()); }

    void ensureRate (int idx)
    {
        if (idx == curOs) return;
        curOs = idx;
        const float fs = baseFs * (float) (1 << idx);
        runFs = fs;
        pre.prepare (fs); hp.reset(); lp.reset(); gate.prepare (fs); comp.prepare (fs); colour.prepare (fs);
        lim.prepare (fs, 1.5f);
        for (auto& b : eqSec) for (auto& s : b) s.reset();
        rot.reset(); lastSig = {};
        os[idx]->reset();
        const int lat = (int) std::lround (os[idx]->getLatencyInSamples()) + (int) std::ceil ((float) lim.latency() / (float) (1 << idx));
        setLatencySamples (lat);
        // dry delay for nothing else: the whole signal goes through the oversampler at every setting
    }

    struct Sig {   // cached settings: filters are re-designed only when something changed
        float hpF = -1, lpF = -1; int hpS = -1, lpS = -1, ch = -1; bool hpOn = false, lpOn = false;
        int preM = -1; float preD = -1, preO = -1;
        float gKh = -1, gKl = -1, cSc = -1, cLp = -1; int cType = -1, cLpM = -1;
        std::array<cs::BandSettings, 4> band {}; std::array<bool, 4> bandValid { false, false, false, false };
        float eqDrive = -1; std::array<int, 4> models { -1, -1, -1, -1 };
    } lastSig;

    static bool same (const cs::BandSettings& a, const cs::BandSettings& b)
    {
        return a.on == b.on && a.model == b.model && a.shelf == b.shelf && a.freq == b.freq && a.gain == b.gain && a.qScale == b.qScale
            && a.stepped == b.stepped && a.aux == b.aux && a.auxSel == b.auxSel;
    }

    cs::BandSettings bandSettings (int b) const
    {
        const auto& e = q.band[b];
        cs::BandSettings s;
        s.on = e.on->load() > 0.5f; s.model = (int) e.model->load(); s.shelf = e.shape->load() > 0.5f; s.freq = e.freq->load();
        s.gain = e.gain->load(); s.qScale = e.qs->load(); s.stepped = e.step->load() > 0.5f; s.aux = e.aux->load(); s.auxSel = (int) e.auxSel->load();
        return s;
    }

    static float hpStep (int mode, float f)
    {
        static const std::vector<float> s73 { 50, 80, 160, 300 }, s84 { 45, 70, 160, 360 };
        return mode == 1 ? cs::snapToSteps (s73, f) : mode == 2 ? cs::snapToSteps (s84, f) : f;
    }
    static float lpStep (int mode, float f)
    {
        static const std::vector<float> s84 { 6000, 8000, 10000, 12000, 18000 };
        return mode == 1 ? cs::snapToSteps (s84, f) : f;
    }

    void updateSettings()
    {
        const float fs = runFs;
        // filters
        const int hpMode = (int) q.hpMode->load(), lpMode = (int) q.lpMode->load();
        const float hpF = hpStep (hpMode, q.hpFreq->load()), lpF = jmin (lpStep (lpMode, q.lpFreq->load()), 0.45f * fs);
        static const int hpSlopes[] = { 6, 12, 18, 24, 36 }, lpSlopes[] = { 6, 12, 18, 24 };
        const int hpS = hpMode > 0 ? 18 : hpSlopes[jlimit (0, 4, (int) q.hpSlope->load())];
        const int lpS = lpSlopes[jlimit (0, 3, (int) q.lpSlope->load())];
        const int chr = (int) q.fltChar->load(); const bool hpOn = q.hpOn->load() > 0.5f, lpOn = q.lpOn->load() > 0.5f;
        auto& s = lastSig;
        if (hpF != s.hpF || hpS != s.hpS || chr != s.ch || hpOn != s.hpOn) { hp.set (true, hpOn, fs, hpF, hpS, chr == 1); }
        if (lpF != s.lpF || lpS != s.lpS || chr != s.ch || lpOn != s.lpOn) { lp.set (false, lpOn, fs, lpF, lpS, chr == 1); }
        s.hpF = hpF; s.hpS = hpS; s.lpF = lpF; s.lpS = lpS; s.ch = chr; s.hpOn = hpOn; s.lpOn = lpOn;
        // pre
        const int pm = (int) q.preModel->load(); const float pd = q.preDrive->load(), po = q.preOut->load();
        if (pm != s.preM || pd != s.preD || po != s.preO) { pre.set (pm, pd, po); s.preM = pm; s.preD = pd; s.preO = po; }
        // gate key filters
        const float kh = q.gtKeyHp->load(), kl = jmin (q.gtKeyLp->load(), 0.45f * fs);
        if (kh != s.gKh || kl != s.gKl) { gate.setKey (kh, kl); s.gKh = kh; s.gKl = kl; }
        // comp side-chain HPF and low pass
        const float sc = q.cpScHp->load(), clp = q.cpLpFreq->load(); const int ct = (int) q.cpType->load(), clm = (int) q.cpLpMode->load();
        if (sc != s.cSc) { comp.setScHp (sc); s.cSc = sc; }
        if (clp != s.cLp || ct != s.cType || clm != s.cLpM) { comp.setLp (ct, clm, clp); s.cLp = clp; s.cType = ct; s.cLpM = clm; }
        // EQ bands
        bool modelsChanged = false;
        for (int b = 0; b < 4; ++b)
        {
            const auto bs = bandSettings (b);
            if (! s.bandValid[(size_t) b] || ! same (bs, s.band[(size_t) b]))
            {
                cs::designBand (bs, b, fs, eqSec[b]);
                for (int k = 0; k < 3; ++k)      // same coefficients for the right channel, its own state
                {
                    auto& d = eqSecR[b][k].bq; const auto& c = eqSec[b][k].bq;
                    d.b0 = c.b0; d.b1 = c.b1; d.b2 = c.b2; d.a1 = c.a1; d.a2 = c.a2; eqSecR[b][k].active = eqSec[b][k].active;
                }
                s.band[(size_t) b] = bs; s.bandValid[(size_t) b] = true;
            }
            const int m = bs.on ? bs.model : -1;
            if (m != s.models[(size_t) b]) { s.models[(size_t) b] = m; modelsChanged = true; }
        }
        const float ed = q.eqDrive->load();
        if (modelsChanged || ed != s.eqDrive)
        {
            // the colour of the models in use, weighted by how many bands use each
            cs::EqColour c { 0, 0, 0 }; int n = 0;
            for (int b = 0; b < 4; ++b) if (s.models[(size_t) b] >= 0) { auto k = cs::eqColour (s.models[(size_t) b]); c.c2 += k.c2; c.c3 += k.c3; c.c3lf += k.c3lf; ++n; }
            if (n > 0) { c.c2 /= (float) n; c.c3 /= (float) n; c.c3lf /= (float) n; }
            const float amt = std::pow (ed / 50.0f, 2.0f);
            colour.set (c, amt); s.eqDrive = ed;
        }
    }

    void processBlock (AudioBuffer<float>& buffer, MidiBuffer&) override
    {
        ScopedNoDenormals nd;
        const int n = buffer.getNumSamples();
        const int nIn = getMainBusNumInputChannels(), nOut = getMainBusNumOutputChannels();
        if (n <= 0) return;
        if (n > maxBlock) { prepareToPlay (getSampleRate(), n); }
        ensureRate (osIndex());
        updateSettings();

        // ---- side chain (base rate), held for each oversampled sample
        const bool scBus = getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled();
        AudioBuffer<float> sc = scBus ? getBusBuffer (buffer, true, 1) : AudioBuffer<float>();
        const bool haveSc = scBus && sc.getNumChannels() > 0;
        // ---- input stage at base rate
        auto main = getBusBuffer (buffer, false, 0);
        float* L = main.getWritePointer (0);
        float* R = nOut > 1 ? main.getWritePointer (1) : nullptr;
        workIn.setSize (2, n, false, false, true);
        float* xL = workIn.getWritePointer (0); float* xR = workIn.getWritePointer (1);
        const int mode = (int) q.inMode->load();
        const float ig = dbToGainF (q.inGain->load());
        const float pl = q.polL->load() > 0.5f ? -1.0f : 1.0f, pr = q.polR->load() > 0.5f ? -1.0f : 1.0f;
        const bool rotOn = q.rotOn->load() > 0.5f;
        const float ang = q.rotDeg->load() * MathConstants<float>::pi / 180.0f, cs_ = std::cos (ang), sn = std::sin (ang);
        for (int i = 0; i < n; ++i)
        {
            float a = L[i], b = (R != nullptr && nIn > 1) ? R[i] : L[i];
            switch (mode) { case 1: a = b = 0.5f * (a + b); break; case 2: b = a; break; case 3: a = b; break; case 4: std::swap (a, b); break; default: break; }
            a *= ig * pl; b *= ig * pr;
            if (rotOn) { a = rot.process (0, a, cs_, sn); b = rot.process (1, b, cs_, sn); }
            xL[i] = a; xR[i] = b;
        }
        for (int i = 0; i < n; ++i) { inPeak = jmax (inPeak * 0.9995f, std::fabs (xL[i]), std::fabs (xR[i])); }

        // ---- oversampled module chain
        dsp::AudioBlock<float> blk (workIn);
        auto& O = *os[(size_t) curOs];
        auto up = curOs > 0 ? O.processSamplesUp (blk) : blk;
        const int f = 1 << curOs; const int m = (int) up.getNumSamples();
        float* uL = up.getChannelPointer (0); float* uR = up.getChannelPointer (1);
        const float* sL = haveSc ? sc.getReadPointer (0) : nullptr;
        const float* sR = haveSc ? sc.getReadPointer (sc.getNumChannels() > 1 ? 1 : 0) : nullptr;

        const auto order = cs::orderFromIndex ((int) q.order->load());
        const bool on[] = { q.preOn->load() > 0.5f, q.fltOn->load() > 0.5f, q.gtOn->load() > 0.5f, q.cpOn->load() > 0.5f, q.eqOn->load() > 0.5f, q.lmOn->load() > 0.5f };
        cs::Gate::P gp; gp.expander = q.gtMode->load() > 0.5f; gp.thr = q.gtThr->load(); gp.range = q.gtRange->load(); gp.atkMs = q.gtAtk->load();
        gp.holdMs = q.gtHold->load(); gp.relMs = q.gtRel->load(); gp.hyst = q.gtHyst->load(); gp.listen = q.gtListen->load() > 0.5f;
        const bool gtExt = q.gtKey->load() > 0.5f && haveSc, cpExt = q.cpKey->load() > 0.5f && haveSc;
        pv::CompParams cp; cp.on = true; cp.duck = false; cp.type = jlimit (0, 2, (int) q.cpType->load()); cp.vintage = q.cpTone->load() > 0.5f;
        cp.thr = q.cpThr->load(); cp.ratio = q.cpRatio->load(); cp.atkMs = q.cpAtk->load() * 1000.0f; cp.relMs = q.cpRel->load() * 1000.0f;
        cp.makeup = q.cpMakeup->load(); cp.mix = q.cpMix->load() * 0.01f;
        const bool scHpOn = q.cpScHp->load() > 20.5f;
        const float lmDrive = dbToGainF (q.lmDrive->load()), lmCeil = dbToGainF (q.lmCeil->load()), lmRel = q.lmRel->load();
        float gGr = 0, cGr = 0, lGr = 0;

        for (int i = 0; i < m; ++i)
        {
            float a = uL[i], b = uR[i];
            const int bi = jmin (n - 1, i / f);
            for (int k = 0; k < cs::kNumModules; ++k)
            {
                const int mod = order[(size_t) k];
                if (! on[mod]) continue;
                switch (mod)
                {
                    case cs::ModPre: a = pre.process (0, a); b = pre.process (1, b); break;
                    case cs::ModFilter: a = lp.process (0, hp.process (0, a)); b = lp.process (1, hp.process (1, b)); break;
                    case cs::ModGate:
                    {
                        const float kL = gtExt ? sL[bi] : a, kR = gtExt ? sR[bi] : b;
                        gate.process (a, b, kL, kR, gp); gGr = jmax (gGr, gate.grDb); break;
                    }
                    case cs::ModComp:
                    {
                        const float kL = cpExt ? sL[bi] : a, kR = cpExt ? sR[bi] : b;
                        comp.process (a, b, kL, kR, cp, scHpOn); cGr = jmax (cGr, comp.comp.reductionDb()); break;
                    }
                    case cs::ModEq:
                        for (int band = 0; band < 4; ++band)
                            for (int s = 0; s < 3; ++s) { a = eqSec[band][s].process (a); }
                        for (int band = 0; band < 4; ++band)
                            for (int s = 0; s < 3; ++s) { b = eqSecR[band][s].process (b); }
                        a = colour.process (0, a); b = colour.process (1, b);
                        break;
                    case cs::ModLim: a *= lmDrive; b *= lmDrive; lim.process (a, b, lmCeil, lmRel); lGr = jmax (lGr, lim.grDb); break;
                    default: break;
                }
            }
            if (! on[cs::ModLim]) lim.process (a, b, 1.0e6f, lmRel);   // keep the look-ahead delay so latency stays constant
            uL[i] = a; uR[i] = b;
        }
        if (curOs > 0) O.processSamplesDown (blk);

        const float og = dbToGainF (q.outGain->load());
        for (int i = 0; i < n; ++i)
        {
            const float a = xL[i] * og, b = xR[i] * og;
            if (R != nullptr) { L[i] = a; R[i] = b; } else L[i] = 0.5f * (a + b);
            outPeak = jmax (outPeak * 0.9995f, std::fabs (a), std::fabs (b));
            outRms = outRms * 0.9995f + 0.0005f * 0.5f * (a * a + b * b);
        }
        gateGr.store (gGr); compGr.store (cGr); limGr.store (lGr);
        inMeter.store (inPeak); outMeter.store (outPeak); vuMeter.store (std::sqrt (outRms));
        compIn.store (comp.comp.inputDb());
    }

    static float dbToGainF (float db) { return std::pow (10.0f, db * 0.05f); }

    // -------------------------------------------------------------------- state
    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const String getName() const override { return "ChannelStrip"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const String&) override {}
    void getStateInformation (MemoryBlock& d) override { if (auto x = apvts.copyState().createXml()) copyXmlToBinary (*x, d); }
    void setStateInformation (const void* d, int s) override
    {
        if (auto x = getXmlFromBinary (d, s)) if (x->hasTagName (apvts.state.getType())) apvts.replaceState (ValueTree::fromXml (*x));
    }

    UndoManager undo;
    APVTS apvts;
    std::atomic<float> gateGr { 0 }, compGr { 0 }, limGr { 0 }, inMeter { 0 }, outMeter { 0 }, vuMeter { 0 }, compIn { -100 };
    float sampleRateForUi() const { return runFs; }

private:
    struct BandPtr { std::atomic<float>* on, * model, * shape, * freq, * gain, * qs, * step, * aux, * auxSel; };
    struct Ptrs {
        std::atomic<float>* os, * inGain, * polL, * polR, * rotOn, * rotDeg, * inMode, * outGain, * order;
        std::atomic<float>* preOn, * preModel, * preDrive, * preOut;
        std::atomic<float>* fltOn, * hpOn, * hpFreq, * hpSlope, * hpMode, * lpOn, * lpFreq, * lpSlope, * lpMode, * fltChar;
        std::atomic<float>* gtOn, * gtMode, * gtThr, * gtRange, * gtAtk, * gtHold, * gtRel, * gtHyst, * gtKey, * gtKeyHp, * gtKeyLp, * gtListen;
        std::atomic<float>* eqOn, * eqDrive; BandPtr band[4];
        std::atomic<float>* cpOn, * cpType, * cpTone, * cpThr, * cpRatio, * cpAtk, * cpRel, * cpMakeup, * cpMix, * cpKey, * cpScHp, * cpLpFreq, * cpLpMode;
        std::atomic<float>* lmOn, * lmDrive, * lmCeil, * lmRel;
    } q {};

    float baseFs = 48000, runFs = 96000; int maxBlock = 512, curOs = -1;
    std::unique_ptr<dsp::Oversampling<float>> os[3];
    AudioBuffer<float> scBuf, workIn;
    cs::Preamp pre; cs::CutFilter hp, lp; cs::Gate gate; cs::CompModule comp; cs::ColourStage colour; cs::Limiter lim; cs::PhaseRotator rot;
    cs::Section eqSec[4][3], eqSecR[4][3];
    float inPeak = 0, outPeak = 0, outRms = 0;
};

#include "CSEditor.h"

AudioProcessorEditor* ChannelStripProcessor::createEditor() { return new ChannelStripEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChannelStripProcessor(); }
