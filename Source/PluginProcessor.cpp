// Flexiverb 480 - a 480L-style multi-effects system with a LARC-style remote.
// Two machines (A, B) run any of the 26 banks (the 480L's 21 plus the Surround/HD Cart) in Single, Cascade, Stereo Split
// or Mono Split. The LARC's CTRL pages hold the configuration, the 480L hardware model, EQ, compressor and delay.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "L480Engine.h"

using namespace juce;
using APVTS = AudioProcessorValueTreeState;

static const float kDivBeats[] = { 0.0625f, 1.0f / 12.0f, 0.125f, 1.0f / 6.0f, 0.25f, 0.375f, 1.0f / 3.0f, 0.5f,
                                   0.75f, 2.0f / 3.0f, 1.0f, 1.5f, 4.0f / 3.0f, 2.0f, 3.0f, 4.0f };
static StringArray divNames() { return { "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2T", "1/2", "1/2D", "1/1" }; }
static StringArray trNames()  { StringArray s; for (int i = 0; i < pv::kNumTransformers; ++i) s.add (pv::transformerModel (i).name); return s; }
static String slotId (int m, int i) { return String (m == 0 ? "a" : "b") + String (i); }

static File registerFolder()
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Flexiverb").getChildFile ("Registers");
}

class Flexiverb480Processor : public AudioProcessor, private Timer
{
public:
    Flexiverb480Processor()
        : AudioProcessor (BusesProperties().withInput ("In", AudioChannelSet::stereo(), true).withOutput ("Out", AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "PARAMS", createLayout (*this))
    {
        for (int m = 0; m < 2; ++m)
        {
            pProg[m] = apvts.getRawParameterValue (m == 0 ? "progA" : "progB");
            for (int i = 0; i < l480::kMaxParams; ++i) pSlot[m][i] = apvts.getRawParameterValue (slotId (m, i));
        }
        auto P = [this] (const char* id) { return apvts.getRawParameterValue (id); };
        pCfg = P ("config"); pOut = P ("out");
        pEqOn = P ("eqOn"); for (int b = 0; b < 4; ++b) { pEqF[b] = P (("eqF" + String (b)).toRawUTF8()); pEqG[b] = P (("eqG" + String (b)).toRawUTF8()); }
        pCpOn = P ("cpOn"); pCpType = P ("cpType"); pCpColor = P ("cpColor"); pCpDuck = P ("cpDuck"); pCpThr = P ("cpThr"); pCpRatio = P ("cpRatio");
        pCpAtk = P ("cpAtk"); pCpRel = P ("cpRel"); pCpMakeup = P ("cpMakeup"); pCpMix = P ("cpMix");
        pDlOn = P ("dlOn"); pDlSync = P ("dlSync"); pDlPing = P ("dlPing"); pDlTimeL = P ("dlTimeL"); pDlTimeR = P ("dlTimeR");
        pDlDivL = P ("dlDivL"); pDlDivR = P ("dlDivR"); pDlFb = P ("dlFb"); pDlMix = P ("dlMix"); pDlLow = P ("dlLow"); pDlHigh = P ("dlHigh");
        pHwOn = P ("hwOn"); pHwIn = P ("hwIn"); pHwOut = P ("hwOut"); pHwXfIn = P ("hwXfIn"); pHwXfOut = P ("hwXfOut"); pHwNoise = P ("hwNoise");
        pTrOn = P ("trOn"); pTrModel = P ("trModel"); pTrFreq = P ("trFreq"); pTrAmt = P ("trAmt");
        for (int m = 0; m < 2; ++m) { lastProg[m] = -1; for (auto& v : lastPitch[m]) v = 1.0e9f; }
        startTimerHz (20);
    }
    ~Flexiverb480Processor() override { stopTimer(); }

    // ------------------------------------------------------------------ parameters
    static std::unique_ptr<AudioParameterFloat> fp (const String& id, const String& name, float lo, float hi, float def, bool logScale = false,
                                                    const String& unit = {}, int dec = 0)
    {
        NormalisableRange<float> r (lo, hi);
        if (logScale && lo > 0.0f)
            r = NormalisableRange<float> (lo, hi, [] (float a, float b, float t) { return a * std::pow (b / a, t); },
                                                  [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); });
        const bool pct = unit == "%";
        auto attr = AudioParameterFloatAttributes().withLabel (pct ? String() : unit)
            .withStringFromValueFunction ([pct, dec, unit] (float v, int)
            {
                if (pct) return String (roundToInt (v * 100.0f)) + "%";
                if (unit == "Hz" && v >= 1000.0f) return String (v / 1000.0f, 2) + " kHz";
                if (unit == "ms" && v >= 1000.0f) return String (v / 1000.0f, 2) + " s";
                return String (v, dec) + (unit.isNotEmpty() ? (unit == ":1" ? unit : " " + unit) : String());
            })
            .withValueFromStringFunction ([pct] (const String& t) { return pct ? t.getFloatValue() / 100.0f : t.getFloatValue(); });
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 3 }, name, r, def, attr);
    }
    static std::unique_ptr<AudioParameterBool> bp (const String& id, const String& name, bool def) { return std::make_unique<AudioParameterBool> (ParameterID { id, 3 }, name, def); }
    static std::unique_ptr<AudioParameterChoice> cp (const String& id, const String& name, const StringArray& c, int def) { return std::make_unique<AudioParameterChoice> (ParameterID { id, 3 }, name, c, def); }

    static APVTS::ParameterLayout createLayout (Flexiverb480Processor& self)
    {
        APVTS::ParameterLayout l;
        StringArray progNames; for (int i = 0; i < l480::numPrograms(); ++i) progNames.add (l480::programLabel (i));
        const int def = l480::findProgram (1, 1);
        for (int m = 0; m < 2; ++m)
        {
            const String M = m == 0 ? "A" : "B";
            l.add (cp (m == 0 ? "progA" : "progB", "Machine " + M + " Program", progNames, m == 0 ? def : l480::findProgram (4, 1)));
            auto g = std::make_unique<AudioProcessorParameterGroup> ("mach" + M, "Machine " + M, "|");
            const auto& dp = l480::programs()[(size_t) (m == 0 ? def : l480::findProgram (4, 1))];
            for (int i = 0; i < l480::kMaxParams; ++i)
            {
                const auto& ds = l480::algoDef (dp.algo).p[i];
                const float dv = (ds.lab && ds.unit != l480::uKey && ds.alias < 0) ? l480::toNorm (ds, l480::programValue (dp, i)) : 0.0f;
                // the slider's text follows the program loaded in that machine
                auto attr = AudioParameterFloatAttributes()
                    .withStringFromValueFunction ([&self, m, i] (float t, int) { return self.slotText (m, i, t); });
                g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { slotId (m, i), 3 },
                                                                    M + " Page " + String (i / 6 + 1) + " Slider " + String (i % 6 + 1),
                                                                    NormalisableRange<float> (0.0f, 1.0f), dv, attr));
            }
            l.add (std::move (g));
        }
        l.add (cp ("config", "Configuration", StringArray { "Single", "Cascade", "Stereo Split", "Mono Split" }, 0));
        l.add (fp ("out", "Output", -24.0f, 12.0f, 0.0f, false, "dB", 1));
        // drawer: EQ
        l.add (bp ("eqOn", "EQ On", false));
        const char* bn[] = { "Low", "Low Mid", "High Mid", "High" };
        const float fLo[] = { 20, 100, 800, 2000 }, fHi[] = { 500, 2000, 8000, 20000 }, fDef[] = { 100, 400, 2500, 8000 };
        for (int b = 0; b < 4; ++b)
        {
            l.add (fp ("eqF" + String (b), String ("EQ ") + bn[b] + " Freq", fLo[b], fHi[b], fDef[b], true, "Hz"));
            l.add (fp ("eqG" + String (b), String ("EQ ") + bn[b] + " Gain", -15.0f, 15.0f, 0.0f, false, "dB", 1));
        }
        // drawer: compressor (VCA / FET / Opto; COLOR adds what that type's hardware adds)
        l.add (bp ("cpOn", "Comp On", false));
        l.add (cp ("cpType", "Comp Type", StringArray { "VCA", "FET", "Opto" }, 0));
        l.add (bp ("cpColor", "Comp Color", false));
        l.add (bp ("cpDuck", "Comp Ducking", false));
        l.add (fp ("cpThr", "Comp Threshold", -60.0f, 0.0f, -24.0f, false, "dB", 1));
        l.add (fp ("cpRatio", "Comp Ratio", 1.0f, 20.0f, 4.0f, true, ":1", 1));
        l.add (fp ("cpAtk", "Comp Attack", 0.05f, 300.0f, 10.0f, true, "ms", 2));
        l.add (fp ("cpRel", "Comp Release", 20.0f, 5000.0f, 300.0f, true, "ms", 0));
        l.add (fp ("cpMakeup", "Comp Makeup", 0.0f, 24.0f, 0.0f, false, "dB", 1));
        l.add (fp ("cpMix", "Comp Mix", 0.0f, 1.0f, 1.0f, false, "%"));
        // drawer: delay
        l.add (bp ("dlOn", "Delay On", false));
        l.add (bp ("dlSync", "Delay Sync", true));
        l.add (bp ("dlPing", "Delay Ping-Pong", false));
        l.add (fp ("dlTimeL", "Delay Time L", 1.0f, 2990.0f, 375.0f, true, "ms"));
        l.add (fp ("dlTimeR", "Delay Time R", 1.0f, 2990.0f, 500.0f, true, "ms"));
        l.add (cp ("dlDivL", "Delay Div L", divNames(), 10));
        l.add (cp ("dlDivR", "Delay Div R", divNames(), 8));
        l.add (fp ("dlFb", "Delay Feedback", 0.0f, 0.95f, 0.35f, false, "%"));
        l.add (fp ("dlMix", "Delay Mix", 0.0f, 1.0f, 0.3f, false, "%"));
        l.add (fp ("dlLow", "Delay Low", -12.0f, 12.0f, 0.0f, false, "dB", 1));
        l.add (fp ("dlHigh", "Delay High", -12.0f, 12.0f, 0.0f, false, "dB", 1));
        // setup: 480L hardware model and output
        l.add (bp ("hwOn", "Analog Hardware", true));
        l.add (cp ("hwIn", "Input", StringArray { "Analog", "Digital" }, 0));
        l.add (cp ("hwOut", "Output Type", StringArray { "Analog", "Digital" }, 0));
        l.add (bp ("hwXfIn", "Input Transformers", false));
        l.add (bp ("hwXfOut", "Output Transformers", false));
        l.add (bp ("hwNoise", "Converter Noise", false));
        l.add (bp ("trOn", "Transformer On", false));
        l.add (cp ("trModel", "Transformer Model", trNames(), 0));
        l.add (fp ("trFreq", "Transformer Freq", 20.0f, 400.0f, 80.0f, true, "Hz"));
        l.add (fp ("trAmt", "Transformer Amount", 0.0f, 1.0f, 0.3f, false, "%"));
        return l;
    }

    // ------------------------------------------------------------------ programs and LARC values
    int programOf (int m) const { return jlimit (0, l480::numPrograms() - 1, (int) pProg[m]->load()); }
    const l480::Program& prog (int m) const { return l480::programs()[(size_t) programOf (m)]; }
    int algoOf (int m) const { return prog (m).algo; }
    const l480::PSpec& spec (int m, int i) const { return l480::algoDef (algoOf (m)).p[i]; }
    float slotNorm (int m, int i) const { return pSlot[m][l480::realIndex (algoOf (m), i)]->load(); }
    float slotValue (int m, int i) const { return l480::toValue (spec (m, i), slotNorm (m, i)); }
    String slotText (int m, int i, float t) const
    {
        if (pProg[m] == nullptr) return String (t, 2);
        const auto& s = spec (m, i);
        if (! s.lab) return "-";
        return String (l480::formatValue (s, l480::toValue (s, t)));
    }

    // load a factory program into a machine (message thread): one program change plus that machine's sliders
    void loadProgram (int m, int index)
    {
        index = jlimit (0, l480::numPrograms() - 1, index);
        if (auto* pp = apvts.getParameter (m == 0 ? "progA" : "progB"))
            pp->setValueNotifyingHost (pp->convertTo0to1 ((float) index));
        const auto& p = l480::programs()[(size_t) index];
        for (int i = 0; i < l480::kMaxParams; ++i)
        {
            const auto& s = l480::algoDef (p.algo).p[i];
            const float t = (s.lab && s.unit != l480::uKey && s.alias < 0) ? l480::toNorm (s, l480::programValue (p, i)) : 0.0f;
            if (auto* sp = apvts.getParameter (slotId (m, i))) if (std::abs (sp->getValue() - t) > 1.0e-6f) sp->setValueNotifyingHost (t);
        }
        if (m == 0) currentHostProgram = index;
        registerName[m] = {}; variation[m] = 1;
        setCurve (m, {}, false);                    // a curve is an edit of the loaded program
        lastProg[m] = index;
        programLoaded.store (true);
    }
    void setSlot (int m, int i, float t)
    {
        const int ri = l480::realIndex (algoOf (m), i);
        if (auto* sp = apvts.getParameter (slotId (m, ri))) sp->setValueNotifyingHost (jlimit (0.0f, 1.0f, t));
    }
    void beginSlot (int m, int i) { if (auto* sp = apvts.getParameter (slotId (m, l480::realIndex (algoOf (m), i)))) sp->beginChangeGesture(); }
    void endSlot (int m, int i)   { if (auto* sp = apvts.getParameter (slotId (m, l480::realIndex (algoOf (m), i)))) sp->endChangeGesture(); }

    // slider-display keys that trigger events (REC, PLAY ...), handled on the audio thread
    void pressKey (int m, int slot) { pendingKey[m].store (slot); }
    String machineStatus (int m) const { if (auto* a = engine.machine (m).current()) return String (a->status()); return {}; }
    bool isEdited (int m) const
    {
        const auto& p = prog (m);
        for (int i = 0; i < l480::kMaxParams; ++i)
        {
            const auto& s = l480::algoDef (p.algo).p[i];
            if (! s.lab || s.unit == l480::uKey || s.alias >= 0) continue;
            if (std::abs (pSlot[m][i]->load() - l480::toNorm (s, l480::programValue (p, i))) > 0.004f) return true;
        }
        return false;
    }

    // registers (REG / STO): the user's own programs, 10 banks x 10 numbers like the 480L's register memory.
    // Variations (VAR): per factory program, 1 = the factory values, 2-0 = the user's stored versions.
    static File registerFile (int bank, int num)   { return registerFolder().getChildFile ("R" + String (bank) + "-" + String (num) + ".flxreg"); }
    static File variationFile (int prog, int v)    { return registerFolder().getParentDirectory().getChildFile ("Variations").getChildFile ("P" + String (prog) + "-V" + String (v) + ".flxvar"); }
    bool writeSnapshot (int m, const File& f) const
    {
        f.getParentDirectory().createDirectory();
        XmlElement x ("FLEXIVERB_REGISTER");
        x.setAttribute ("program", programOf (m));
        x.setAttribute ("name", String (prog (m).name));
        for (int i = 0; i < l480::kMaxParams; ++i) x.setAttribute ("s" + String (i), (double) pSlot[m][i]->load());
        x.setAttribute ("curve", curveToString (curvePts[m])); x.setAttribute ("curveOn", curveOn[m]);
        return x.writeTo (f);
    }
    bool readSnapshot (int m, const File& f)
    {
        auto x = parseXML (f);
        if (x == nullptr) return false;
        loadProgram (m, x->getIntAttribute ("program"));
        for (int i = 0; i < l480::kMaxParams; ++i)
            if (auto* sp = apvts.getParameter (slotId (m, i))) sp->setValueNotifyingHost ((float) x->getDoubleAttribute ("s" + String (i), sp->getValue()));
        setCurve (m, curveFromString (x->getStringAttribute ("curve")), x->getBoolAttribute ("curveOn"));
        return true;
    }
    static String snapshotName (const File& f) { if (auto x = parseXML (f)) return x->getStringAttribute ("name"); return {}; }
    bool registerExists (int bank, int num) const { return registerFile (bank, num).existsAsFile(); }
    String registerTitle (int bank, int num) const { return snapshotName (registerFile (bank, num)); }
    bool storeRegister (int m, int bank, int num)
    {
        if (! writeSnapshot (m, registerFile (bank, num))) return false;
        registerName[m] = String (bank) + "." + String (num); variation[m] = 0;
        return true;
    }
    bool recallRegister (int m, int bank, int num)
    {
        if (! readSnapshot (m, registerFile (bank, num))) return false;
        registerName[m] = String (bank) + "." + String (num); variation[m] = 0;
        return true;
    }
    bool variationExists (int m, int v) const { return v == 1 || variationFile (programOf (m), v).existsAsFile(); }
    bool storeVariation (int m, int v)
    {
        if (v < 2 || ! writeSnapshot (m, variationFile (programOf (m), v))) return false;
        registerName[m] = {}; variation[m] = v;
        return true;
    }
    bool recallVariation (int m, int v)
    {
        const int pr = programOf (m);
        if (v == 1) { loadProgram (m, pr); return true; }
        if (! readSnapshot (m, variationFile (pr, v))) return false;
        variation[m] = v;
        return true;
    }
    // ------------------------------------------------------------------ decay-time curve (RT graph), per machine
    // Points are (frequency Hz, multiplier of the program's main reverb time), joined in log-log, flat beyond the ends.
    struct CurvePt { float f, m; };
    std::vector<CurvePt> curvePts[2]; bool curveOn[2] = { false, false };
    static float curveEval (const std::vector<CurvePt>& pts, float f)
    {
        if (pts.empty()) return 1.0f;
        if (f <= pts.front().f) return pts.front().m;
        if (f >= pts.back().f) return pts.back().m;
        for (size_t i = 1; i < pts.size(); ++i)
            if (f <= pts[i].f)
            {
                const float t = std::log (f / pts[i - 1].f) / std::log (pts[i].f / pts[i - 1].f);
                return std::exp (std::log (pts[i - 1].m) + t * (std::log (pts[i].m) - std::log (pts[i - 1].m)));
            }
        return pts.back().m;
    }
    static String curveToString (const std::vector<CurvePt>& pts) { String s; for (auto& p : pts) s << String (p.f, 1) << ":" << String (p.m, 4) << ";"; return s; }
    static std::vector<CurvePt> curveFromString (const String& s)
    {
        std::vector<CurvePt> v; StringArray t; t.addTokens (s, ";", "");
        for (auto& k : t) if (k.contains (":")) v.push_back ({ jlimit (20.0f, 20000.0f, k.upToFirstOccurrenceOf (":", false, false).getFloatValue()),
                                                               jlimit (0.02f, 20.0f, k.fromFirstOccurrenceOf (":", false, false).getFloatValue()) });
        std::sort (v.begin(), v.end(), [] (const CurvePt& a, const CurvePt& b) { return a.f < b.f; });
        return v;
    }
    void setCurve (int m, std::vector<CurvePt> pts, bool on)
    {
        std::sort (pts.begin(), pts.end(), [] (const CurvePt& a, const CurvePt& b) { return a.f < b.f; });
        curvePts[m] = pts; curveOn[m] = on && ! pts.empty();
        for (int b = 0; b < pv::DecayEq::N; ++b) aCurveMul[m][b].store (curveEval (pts, pv::DecayEq::centre (b)));
        aCurveOn[m].store (curveOn[m]);
        apvts.state.setProperty (m == 0 ? "curveA" : "curveB", curveToString (pts), nullptr);
        apvts.state.setProperty (m == 0 ? "curveOnA" : "curveOnB", curveOn[m], nullptr);
    }
    bool hasDecayCurve (int m) const { return l480::algoHasDecayCurve (algoOf (m)); }
    float mainRt (int m) const { return slotValue (m, l480::algoMainRtSlot (algoOf (m))); }
    // the program's own decay shape (BAS/XOV and the RT high cut), as a multiplier of the main reverb time
    float programDecayMul (int m, float f) const
    {
        const int a = algoOf (m);
        float bas = 1, xov = 500, hf = 21000;
        if (a == l480::aRev6 || a == l480::aRev2 || a == l480::aRHall || a == l480::aHD)
        { bas = slotValue (m, a == l480::aHD ? 6 : 6); xov = slotValue (m, 7); hf = slotValue (m, 8); }
        else if (a == l480::aCHall || a == l480::aRPlate) { bas = slotValue (m, 0) / jmax (0.05f, slotValue (m, 1)); xov = slotValue (m, 2); hf = slotValue (m, 3); }
        else if (a == l480::aAmb) hf = 9000;
        const float w = 1.0f / (1.0f + (f / xov) * (f / xov));
        float mul = bas * w + (1.0f - w);
        if (hf < 20500.0f) mul /= 1.0f + 0.5f * (f / hf) * (f / hf);
        return mul;
    }
    std::vector<CurvePt> programCurve (int m) const
    {
        std::vector<CurvePt> v;
        for (float f : { 40.0f, 125.0f, 400.0f, 1000.0f, 2500.0f, 6300.0f, 14000.0f }) v.push_back ({ f, programDecayMul (m, f) });
        return v;
    }
    std::atomic<bool> aCurveOn[2] { false, false };
    std::atomic<float> aCurveMul[2][pv::DecayEq::N] {};

    String registerName[2];   // "bank.num" when the machine holds a recalled register
    int variation[2] = { 1, 1 };   // 1 = factory, 2-10 = user variation, 0 = from a register

    // ------------------------------------------------------------------ audio
    void prepareToPlay (double sampleRate, int) override
    {
        engine.prepare (sampleRate);
        sampleRateUi.store (sampleRate);
    }
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& b) const override
    {
        const auto in = b.getMainInputChannelSet(), out = b.getMainOutputChannelSet();
        return out == AudioChannelSet::stereo() && (in == AudioChannelSet::stereo() || in == AudioChannelSet::mono());
    }
    void processBlock (AudioBuffer<float>& buffer, MidiBuffer&) override
    {
        ScopedNoDenormals noDenormals;
        const int n = buffer.getNumSamples();
        if (buffer.getNumChannels() < 2 || n == 0) return;
        if (getTotalNumInputChannels() == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
        fillParams (prm);
        float* L = buffer.getWritePointer (0); float* R = buffer.getWritePointer (1);
        for (int pos = 0; pos < n; pos += 256)
        {
            const int k = jmin (256, n - pos);
            for (int m = 0; m < 2; ++m)
            {
                const int key = pendingKey[m].exchange (-1);
                if (key >= 0) if (auto* a = engine.machine (m).current()) a->key (key);
            }
            engine.process (L + pos, R + pos, k, prm);
        }
        {   // live display feed: the output, mono
            int w = scopeWrite.load (std::memory_order_relaxed);
            for (int i = 0; i < n; ++i) { scope[(size_t) w] = 0.5f * (L[i] + R[i]); w = (w + 1) & (kScopeSize - 1); }
            scopeWrite.store (w, std::memory_order_release);
        }
        meterIn.store (jmax (meterIn.load() * 0.85f, engine.meterIn()));
        meterOut.store (jmax (meterOut.load() * 0.85f, engine.meterOut()));
        compGr.store (prm.comp.on ? engine.compGr() : 0.0f);
    }
    void fillParams (l480::EngineParams& p)
    {
        auto f = [] (std::atomic<float>* a) { return a->load(); };
        auto on = [] (std::atomic<float>* a) { return a->load() > 0.5f; };
        for (int m = 0; m < 2; ++m)
        {
            p.prog[m] = programOf (m);
            const int a = l480::programs()[(size_t) p.prog[m]].algo;
            for (int i = 0; i < l480::kMaxParams; ++i) p.val[m][i] = l480::toValue (l480::algoDef (a).p[i], pSlot[m][l480::realIndex (a, i)]->load());
        }
        p.config = jlimit (0, 3, (int) f (pCfg)); p.mute = mute.load();
        for (int m = 0; m < 2; ++m) { p.curveOn[m] = aCurveOn[m].load(); for (int b = 0; b < pv::DecayEq::N; ++b) p.curveMul[m][b] = aCurveMul[m][b].load(); }
        p.eqOn = on (pEqOn); for (int b = 0; b < 4; ++b) { p.eqF[b] = f (pEqF[b]); p.eqG[b] = f (pEqG[b]); }
        auto& c = p.comp;
        c.on = on (pCpOn); c.type = jlimit (0, 2, (int) f (pCpType)); c.vintage = on (pCpColor); c.duck = on (pCpDuck);
        c.thr = f (pCpThr); c.ratio = f (pCpRatio); c.atkMs = f (pCpAtk); c.relMs = f (pCpRel); c.makeup = f (pCpMakeup); c.mix = f (pCpMix);
        p.dlOn = on (pDlOn); p.dlPing = on (pDlPing); p.dlFb = f (pDlFb); p.dlMix = f (pDlMix); p.dlLow = f (pDlLow); p.dlHigh = f (pDlHigh);
        if (on (pDlSync))
        {
            double bpm = 120.0;
            if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto b = pos->getBpm()) bpm = *b;
            bpm = jlimit (20.0, 400.0, bpm);
            p.dlMsL = jmin (2990.0f, (float) (kDivBeats[jlimit (0, 15, (int) f (pDlDivL))] * 60000.0 / bpm));
            p.dlMsR = jmin (2990.0f, (float) (kDivBeats[jlimit (0, 15, (int) f (pDlDivR))] * 60000.0 / bpm));
        }
        else { p.dlMsL = f (pDlTimeL); p.dlMsR = f (pDlTimeR); }
        p.hw.on = on (pHwOn); p.hw.inAnalog = f (pHwIn) < 0.5f; p.hw.outAnalog = f (pHwOut) < 0.5f;
        p.hw.xfIn = on (pHwXfIn); p.hw.xfOut = on (pHwXfOut); p.hw.noise = on (pHwNoise);
        p.trOn = on (pTrOn); p.trModel = jlimit (0, pv::kNumTransformers - 1, (int) f (pTrModel)); p.trFreq = f (pTrFreq); p.trAmt = f (pTrAmt);
        p.outDb = f (pOut);
    }

    // message thread: the Pitch program's glide sliders follow PCH/FIN like the hardware's
    void timerCallback() override
    {
        for (int m = 0; m < 2; ++m)
        {
            const int pr = programOf (m);
            if (pr != lastProg[m]) { lastProg[m] = pr; for (auto& v : lastPitch[m]) v = 1.0e9f; }
            if (algoOf (m) != l480::aPitch) continue;
            const float pl = slotValue (m, 2) + slotValue (m, 3) * 0.01f, prr = slotValue (m, 4) + slotValue (m, 5) * 0.01f;
            if (lastPitch[m][0] < 1.0e8f && std::abs (pl - lastPitch[m][0]) > 1.0e-4f) setSlot (m, 16, l480::toNorm (spec (m, 16), pl));
            if (lastPitch[m][1] < 1.0e8f && std::abs (prr - lastPitch[m][1]) > 1.0e-4f) setSlot (m, 22, l480::toNorm (spec (m, 22), prr));
            lastPitch[m][0] = pl; lastPitch[m][1] = prr;
        }
    }

    // ------------------------------------------------------------------ host programs = the factory programs (machine A)
    int getNumPrograms() override { return l480::numPrograms(); }
    int getCurrentProgram() override { return programOf (0); }
    void setCurrentProgram (int i) override { if (i >= 0 && i < l480::numPrograms() && i != programOf (0)) loadProgram (0, i); }
    const String getProgramName (int i) override { return l480::programLabel (i); }
    void changeProgramName (int, const String&) override {}

    // ------------------------------------------------------------------ state
    void getStateInformation (MemoryBlock& dest) override
    {
        auto st = apvts.copyState();
        st.setProperty ("regA", registerName[0], nullptr); st.setProperty ("regB", registerName[1], nullptr);
        st.setProperty ("varA", variation[0], nullptr); st.setProperty ("varB", variation[1], nullptr);
        if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
    }
    void setStateInformation (const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary (data, size))
            if (xml->hasTagName (apvts.state.getType()))
            {
                apvts.replaceState (ValueTree::fromXml (*xml));
                registerName[0] = apvts.state.getProperty ("regA").toString(); registerName[1] = apvts.state.getProperty ("regB").toString();
                // registers saved by older versions were named, not numbered: drop those labels
                for (auto& r : registerName) if (! r.containsOnly ("0123456789.")) r = {};
                variation[0] = (int) apvts.state.getProperty ("varA", 1); variation[1] = (int) apvts.state.getProperty ("varB", 1);
                setCurve (0, curveFromString (apvts.state.getProperty ("curveA").toString()), (bool) apvts.state.getProperty ("curveOnA", false));
                setCurve (1, curveFromString (apvts.state.getProperty ("curveB").toString()), (bool) apvts.state.getProperty ("curveOnB", false));
                for (int m = 0; m < 2; ++m) lastProg[m] = programOf (m);
                programLoaded.store (true);
            }
    }
    void setUiProperty (const Identifier& id, const var& v) { apvts.state.setProperty (id, v, nullptr); }
    var getUiProperty (const Identifier& id, const var& def) const { return apvts.state.getProperty (id, def); }

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const String getName() const override { return "Flexiverb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    APVTS apvts;
    std::atomic<bool> mute { false }, programLoaded { true };
    std::atomic<float> meterIn { 0 }, meterOut { 0 }, compGr { 0 };
    std::atomic<double> sampleRateUi { 48000.0 };
    static constexpr int kScopeSize = 8192;
    std::array<float, kScopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };

private:
    l480::Engine engine; l480::EngineParams prm;
    std::atomic<float>* pProg[2] = {}; std::atomic<float>* pSlot[2][l480::kMaxParams] = {};
    std::atomic<float> *pCfg, *pOut, *pEqOn, *pEqF[4], *pEqG[4], *pCpOn, *pCpType, *pCpColor, *pCpDuck, *pCpThr, *pCpRatio, *pCpAtk, *pCpRel, *pCpMakeup, *pCpMix;
    std::atomic<float> *pDlOn, *pDlSync, *pDlPing, *pDlTimeL, *pDlTimeR, *pDlDivL, *pDlDivR, *pDlFb, *pDlMix, *pDlLow, *pDlHigh;
    std::atomic<float> *pHwOn, *pHwIn, *pHwOut, *pHwXfIn, *pHwXfOut, *pHwNoise, *pTrOn, *pTrModel, *pTrFreq, *pTrAmt;
    std::atomic<int> pendingKey[2] { -1, -1 };
    int lastProg[2] = { -1, -1 }, currentHostProgram = 0; float lastPitch[2][2] = {};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Flexiverb480Processor)
};

#include "PluginEditor.h"

AudioProcessorEditor* Flexiverb480Processor::createEditor() { return new Flexiverb480Editor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Flexiverb480Processor(); }
