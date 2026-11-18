// ChannelStrip 2 - PRE (models + ODD/EVEN/HEAVY/CRUSH colours, thump, resonant filters), 64-bit EQ
// (one model for all bands, free frequencies), DS2 dual de-esser, PlateVerb compressor, gate/expander,
// VST3 insert slot and limiter, in any order, with ST / DUO / MS routing per module, A/B/C/D slots,
// presets, input polarity / phase rotation and an external sidechain.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "CSEngine.h"

using namespace juce;
using APVTS = AudioProcessorValueTreeState;

static StringArray eqModelNames()  { StringArray s; for (int i = 0; i < cs::kNumEqModels; ++i) s.add (cs::eqModelName (i)); return s; }
static StringArray preModelNames() { StringArray s; for (int i = 0; i < cs::kNumPreModels; ++i) s.add (cs::preModelName (i)); return s; }
static StringArray compTypeNames() { return { "VCA", "FET", "OPTO" }; }       // PlateVerb's compressor types
static StringArray routeNames()    { return { "ST", "DUO", "MS" }; }
static String bid (int b, const char* base) { return "b" + String (b) + base; }
static String did (int d, const char* base) { return "ds" + String (d + 1) + base; }
static const char* const kRouteIds[cs::kNumModules] = { "preRoute", "eqRoute", "dsRoute", "cpRoute", "gtRoute", "", "" };
static const char* const kOnIds[cs::kNumModules] = { "preOn", "eqOn", "dsOn", "cpOn", "gtOn", "insOn", "lmOn" };

// ============================================================================ factory presets
struct PresetDef { const char* name; std::vector<std::pair<const char*, float>> v; };
static const std::vector<PresetDef>& factoryPresets()
{
    static const std::vector<PresetDef> p {
        { "Default", {} },
        { "Vocal - warm and present", { { "preColor", 1 }, { "preDrive", 6 }, { "hpOn", 1 }, { "hpFreq", 90 }, { "b0Gain", -1.5f }, { "b1Gain", 1.5f }, { "b2Freq", 3200 }, { "b2Gain", 2 },
            { "b3Gain", 3 }, { "dsOn", 1 }, { "ds1Thr", -26 }, { "cpOn", 1 }, { "cpType", 2 }, { "cpThr", -22 }, { "cpRatio", 3 }, { "cpMakeup", 3 } } },
        { "Kick - thump and click", { { "preColor", 0 }, { "preDrive", 8 }, { "preThump", 1 }, { "hpOn", 1 }, { "hpFreq", 30 }, { "b0Freq", 60 }, { "b0Gain", 4 }, { "b1Freq", 350 }, { "b1Gain", -4 },
            { "b2Freq", 3500 }, { "b2Gain", 3 }, { "cpOn", 1 }, { "cpType", 1 }, { "cpThr", -16 }, { "cpRatio", 4 }, { "cpAtk", 0.01f }, { "cpRel", 0.12f }, { "cpMakeup", 2 } } },
        { "Snare - crack", { { "preColor", 2 }, { "preDrive", 10 }, { "hpOn", 1 }, { "hpFreq", 80 }, { "b0Freq", 200 }, { "b0Gain", 2 }, { "b2Freq", 5000 }, { "b2Gain", 3 }, { "gtOn", 1 }, { "gtThr", -38 },
            { "cpOn", 1 }, { "cpType", 1 }, { "cpThr", -18 }, { "cpRatio", 6 }, { "cpAtk", 0.003f }, { "cpMakeup", 3 } } },
        { "Bass DI - solid", { { "preModel", 2 }, { "preColor", 0 }, { "preDrive", 9 }, { "hpOn", 1 }, { "hpFreq", 35 }, { "b0Freq", 80 }, { "b0Gain", 2 }, { "b1Freq", 700 }, { "b1Gain", 1.5f },
            { "cpOn", 1 }, { "cpType", 2 }, { "cpThr", -20 }, { "cpRatio", 4 }, { "cpMakeup", 3 } } },
        { "Acoustic guitar - airy", { { "preModel", 3 }, { "hpOn", 1 }, { "hpFreq", 100 }, { "b1Freq", 250 }, { "b1Gain", -2 }, { "b3Freq", 12000 }, { "b3Gain", 3 }, { "cpOn", 1 }, { "cpType", 0 },
            { "cpThr", -20 }, { "cpRatio", 2.5f }, { "cpMakeup", 2 } } },
        { "Drum bus - glue", { { "preColor", 2 }, { "preDrive", 5 }, { "cpOn", 1 }, { "cpType", 0 }, { "cpThr", -14 }, { "cpRatio", 2 }, { "cpAtk", 0.03f }, { "cpRel", 0.3f }, { "cpScHp", 100 },
            { "cpMix", 70 }, { "cpMakeup", 2 }, { "lmOn", 1 }, { "lmCeil", -0.5f } } },
        { "Sax - smooth", { { "preModel", 1 }, { "preColor", 1 }, { "preDrive", 5 }, { "hpOn", 1 }, { "hpFreq", 70 }, { "b1Freq", 500 }, { "b1Gain", 1 }, { "b2Freq", 2800 }, { "b2Gain", -2 },
            { "b3Gain", 2 }, { "dsOn", 1 }, { "ds1Freq", 5500 }, { "cpOn", 1 }, { "cpType", 2 }, { "cpThr", -20 }, { "cpRatio", 3 }, { "cpMakeup", 2.5f } } },
    };
    return p;
}

// ============================================================================ processor
class ChannelStripProcessor : public AudioProcessor, private AudioProcessorValueTreeState::Listener
{
public:
    ChannelStripProcessor()
        : AudioProcessor (BusesProperties().withInput ("Input", AudioChannelSet::stereo(), true)
                                           .withOutput ("Output", AudioChannelSet::stereo(), true)
                                           .withInput ("Sidechain", AudioChannelSet::stereo(), false)),
          apvts (*this, &undo, "PARAMS", createLayout())
    {
        formats.addFormat (new VST3PluginFormat());
        auto P = [this] (const String& id) { return apvts.getRawParameterValue (id); };
        for (auto* id : { "inGain", "polL", "polR", "rotOn", "rotDeg", "inMode", "outGain", "order", "os", "zeroLat",
                          "preOn", "preRoute", "preModel", "preColor", "preDrive", "preOut", "preThump", "hpOn", "hpFreq", "hpSlope", "hpRes", "lpOn", "lpFreq", "lpSlope", "lpRes",
                          "eqOn", "eqRoute", "eqModel", "eqDrive", "dsOn", "dsRoute", "dsMode", "dsListen",
                          "cpOn", "cpRoute", "cpType", "cpTone", "cpThr", "cpRatio", "cpAtk", "cpRel", "cpMakeup", "cpMix", "cpScHp", "cpKey",
                          "gtOn", "gtRoute", "gtMode", "gtThr", "gtRange", "gtAtk", "gtHold", "gtRel", "gtHyst", "gtKey", "gtKeyHp", "gtKeyLp", "gtListen",
                          "insOn", "lmOn", "lmDrive", "lmCeil", "lmRel" })
            raw[id] = P (id);
        for (int b = 0; b < 4; ++b) for (auto* k : { "On", "Gain", "Freq", "Q", "Shape", "Aux", "AuxSel" }) raw[bid (b, k)] = P (bid (b, k));
        for (int d = 0; d < 2; ++d) for (auto* k : { "On", "Freq", "Thr", "Range", "Q" }) raw[did (d, k)] = P (did (d, k));
        apvts.addParameterListener ("eqModel", this);
        apvts.state.setProperty ("theme", "Graphite", nullptr);
        apvts.state.setProperty ("depth", "3D", nullptr);
        apvts.state.setProperty ("scale", 1.0, nullptr);
    }
    ~ChannelStripProcessor() override { apvts.removeParameterListener ("eqModel", this); }

    static APVTS::ParameterLayout createLayout()
    {
        APVTS::ParameterLayout L;
        auto fp = [] (const String& id, const String& name, float lo, float hi, float def, float centre = -1.0f, const String& unit = {}, int dec = 1, float step = 0.0f)
        {
            NormalisableRange<float> r (lo, hi, step);
            if (centre > lo && centre < hi) r.setSkewForCentre (centre);
            return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                AudioParameterFloatAttributes().withLabel (unit).withStringFromValueFunction ([dec] (float v, int) { return String (v, dec); }));
        };
        auto bp = [] (const String& id, const String& name, bool def) { return std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def); };
        auto cp = [] (const String& id, const String& name, const StringArray& items, int def) { return std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def); };

        L.add (fp ("inGain", "Input Gain", -24.0f, 24.0f, 0.0f, -1, "dB", 1, 0.01f));
        L.add (bp ("polL", "Polarity L", false)); L.add (bp ("polR", "Polarity R", false));
        L.add (bp ("rotOn", "Phase Rotate", false));
        L.add (fp ("rotDeg", "Phase Angle", -180.0f, 180.0f, 0.0f, -1, "deg", 0));
        L.add (cp ("inMode", "Input Mode", { "Stereo", "Mono", "Left", "Right", "Swap" }, 0));
        L.add (fp ("outGain", "Output Gain", -24.0f, 24.0f, 0.0f, -1, "dB", 1, 0.01f));
        L.add (std::make_unique<AudioParameterInt> (ParameterID { "order", 1 }, "Module Order", 0, cs::factorial (cs::kNumModules) - 1, cs::defaultOrderIndex()));
        L.add (cp ("os", "Oversampling", { "1x", "2x", "4x" }, 1));
        L.add (bp ("zeroLat", "Zero Latency", false));

        L.add (bp ("preOn", "Pre On", true));
        L.add (cp ("preRoute", "Pre Routing", routeNames(), 0));
        L.add (cp ("preModel", "Pre Model", preModelNames(), cs::PreTube));
        L.add (cp ("preColor", "Pre Colour", { "ODD", "EVEN", "HEAVY", "CRUSH" }, cs::ColEven));
        L.add (fp ("preDrive", "Pre Drive", 0.0f, 30.0f, 0.0f, 10.0f, "dB", 1, 0.01f));
        L.add (fp ("preOut", "Pre Output", -24.0f, 24.0f, 0.0f, -1, "dB", 1, 0.01f));
        L.add (bp ("preThump", "Thump", false));
        L.add (bp ("hpOn", "Low Cut On", false));
        L.add (fp ("hpFreq", "Low Cut Freq", 10.0f, 1000.0f, 80.0f, 80.0f, "Hz", 0));
        L.add (cp ("hpSlope", "Low Cut Slope", { "6", "12", "18", "24" }, 2));
        L.add (fp ("hpRes", "Low Cut Resonance", 0.0f, 100.0f, 0.0f, -1, "%", 0));
        L.add (bp ("lpOn", "High Cut On", false));
        L.add (fp ("lpFreq", "High Cut Freq", 1000.0f, 22000.0f, 18000.0f, 8000.0f, "Hz", 0));
        L.add (cp ("lpSlope", "High Cut Slope", { "6", "12", "18", "24" }, 1));
        L.add (fp ("lpRes", "High Cut Resonance", 0.0f, 100.0f, 0.0f, -1, "%", 0));

        L.add (bp ("eqOn", "EQ On", true));
        L.add (cp ("eqRoute", "EQ Routing", routeNames(), 0));
        L.add (cp ("eqModel", "EQ Model", eqModelNames(), cs::EqNevePA));
        L.add (fp ("eqDrive", "EQ Drive", 0.0f, 100.0f, 50.0f, -1, "%", 0));
        static const float defF[] = { 110, 700, 3200, 12000 };
        for (int b = 0; b < 4; ++b)
        {
            const String n = String (cs::bandName (b)) + " ";
            L.add (bp (bid (b, "On"), n + "On", true));
            L.add (fp (bid (b, "Gain"), n + "Gain", -20.0f, 20.0f, 0.0f, -1, "dB", 1, 0.01f));
            L.add (fp (bid (b, "Freq"), n + "Freq", 20.0f, 20000.0f, defF[b], 1000.0f, "Hz", 0));
            L.add (fp (bid (b, "Q"), n + "Q", 0.25f, 4.0f, 1.0f, 1.0f, "x", 2));
            L.add (cp (bid (b, "Shape"), n + "Shape", { "Bell", "Shelf" }, (b == 0 || b == 3) ? 1 : 0));
            L.add (fp (bid (b, "Aux"), n + "Atten", -17.5f, 0.0f, 0.0f, -1, "dB", 1));
            L.add (cp (bid (b, "AuxSel"), n + "Atten Select", { "5k", "10k", "20k" }, 1));
        }

        L.add (bp ("dsOn", "DS2 On", false));
        L.add (cp ("dsRoute", "DS2 Routing", routeNames(), 0));
        L.add (cp ("dsMode", "DS2 Mode", { "WIDE", "SPLIT" }, 0));
        L.add (bp ("dsListen", "DS2 Listen", false));
        for (int d = 0; d < 2; ++d)
        {
            const String n = "DS" + String (d + 1) + " ";
            L.add (bp (did (d, "On"), n + "On", d == 0));
            L.add (fp (did (d, "Freq"), n + "Freq", 1000.0f, 16000.0f, d == 0 ? 6500.0f : 9500.0f, 5000.0f, "Hz", 0));
            L.add (fp (did (d, "Thr"), n + "Threshold", -60.0f, 0.0f, d == 0 ? -24.0f : -20.0f, -1, "dB"));
            L.add (fp (did (d, "Range"), n + "Range", 0.0f, 24.0f, 8.0f, -1, "dB"));
            L.add (fp (did (d, "Q"), n + "Width", 0.4f, 6.0f, 1.5f, 1.5f, "", 2));
        }

        L.add (bp ("cpOn", "Comp On", false));
        L.add (cp ("cpRoute", "Comp Routing", routeNames(), 0));
        L.add (cp ("cpType", "Comp Type", compTypeNames(), 1));
        L.add (cp ("cpTone", "Comp Tone", { "Clean", "Vintage" }, 0));
        L.add (fp ("cpThr", "Comp Threshold", -80.0f, 0.0f, -20.0f, -1, "dB", 1, 0.01f));
        L.add (fp ("cpRatio", "Comp Ratio", 1.0f, 20.0f, 4.0f, 4.0f, ":1", 1, 0.01f));
        L.add (fp ("cpAtk", "Comp Attack", 0.00002f, 0.3f, 0.01f, 0.01f, "s", 5));
        L.add (fp ("cpRel", "Comp Release", 0.02f, 10.0f, 0.3f, 0.5f, "s", 3));
        L.add (fp ("cpMakeup", "Comp Makeup", 0.0f, 24.0f, 0.0f, -1, "dB", 1, 0.01f));
        L.add (fp ("cpMix", "Comp Mix", 0.0f, 100.0f, 100.0f, -1, "%", 0));
        L.add (fp ("cpScHp", "Comp SC HPF", 0.0f, 400.0f, 0.0f, 100.0f, "Hz", 0));
        L.add (cp ("cpKey", "Comp Key", { "Internal", "Sidechain" }, 0));

        L.add (bp ("gtOn", "Gate On", false));
        L.add (cp ("gtRoute", "Gate Routing", routeNames(), 0));
        L.add (cp ("gtMode", "Gate Mode", { "GATE", "EXP" }, 0));
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

        L.add (bp ("insOn", "Insert On", true));
        L.add (bp ("lmOn", "Limiter On", false));
        L.add (fp ("lmDrive", "Limiter Drive", 0.0f, 18.0f, 0.0f, -1, "dB", 1, 0.01f));
        L.add (fp ("lmCeil", "Limiter Ceiling", -12.0f, 0.0f, -0.3f, -1, "dB", 1, 0.01f));
        L.add (fp ("lmRel", "Limiter Release", 1.0f, 1000.0f, 60.0f, 60.0f, "ms", 0));
        return L;
    }

    // choosing an EQ model jumps the bands to that unit's classic frequencies (they stay free afterwards)
    void parameterChanged (const String& id, float val) override
    {
        if (id != "eqModel" || restoring) return;
        const int m = (int) val;
        MessageManager::callAsync ([this, m] {
            for (int b = 0; b < 4; ++b)
                if (auto* p = apvts.getParameter (bid (b, "Freq"))) p->setValueNotifyingHost (p->convertTo0to1 (cs::modelDefaultFreq (m, b)));
        });
    }

    float v (const char* id) const { return raw.at (id)->load(); }
    float v (const String& id) const { return raw.at (id)->load(); }
    bool on (const char* id) const { return v (id) > 0.5f; }

    // -------------------------------------------------------------------- audio
    void prepareToPlay (double sr, int block) override
    {
        baseFs = sr; maxBlock = jmax (1, block);
        for (int i = 0; i < 3; ++i)
        {
            os[i] = std::make_unique<dsp::Oversampling<float>> (2, (size_t) i, dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
            os[i]->initProcessing ((size_t) maxBlock);
        }
        work.setSize (2, maxBlock);
        eq.prepare (sr); ds.prepare (sr);
        for (auto& u : comp) u.prepare ((float) sr);
        for (auto& g : gate) g.prepare ((float) sr);
        limLA.prepare ((float) sr, 1.5f); limZero.prepare ((float) sr, 0.0f);
        rot.reset(); curOs = -1; curZero = -1;
        smThr = v ("cpThr"); smMk = v ("cpMakeup"); smMix = v ("cpMix") * 0.01f;
        prepareInsert();
        ensureRate();
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

    int osFactorIndex() const { return on ("zeroLat") ? 0 : jlimit (0, 2, (int) v ("os")); }

    void ensureRate()
    {
        const int idx = osFactorIndex(); const int z = on ("zeroLat") ? 1 : 0;
        if (idx == curOs && z == curZero && insLatency == lastInsLatency) return;
        const bool rateChanged = idx != curOs;
        curOs = idx; curZero = z; lastInsLatency = insLatency;
        if (rateChanged)
        {
            pre.prepare (baseFs * (double) (1 << idx));
            hp.reset(); lp.reset(); sig = {};
            os[idx]->reset();
        }
        const int osLat = idx > 0 ? (int) std::lround (os[idx]->getLatencyInSamples()) : 0;
        setLatencySamples (osLat + (z ? 0 : limLA.latency()) + insLatency);
    }

    struct Sig { float hpF = -1, lpF = -1, hpR = -1, lpR = -1; int hpS = -1, lpS = -1; bool hpOn = false, lpOn = false;
                 int preM = -1, preC = -1; float preD = -1, preO = -1, preHp = -1; bool th = false; float gKh = -1, gKl = -1, cSc = -1; } sig;

    void updateSettings()
    {
        const double fsOs = baseFs * (double) (1 << curOs);
        auto& s = sig;
        // PRE filters run inside the oversampled PRE
        static const int slopes[] = { 6, 12, 18, 24 };
        const float hpF = v ("hpFreq"), lpF = (float) std::min ((double) v ("lpFreq"), 0.45 * fsOs);
        const int hpS = slopes[jlimit (0, 3, (int) v ("hpSlope"))], lpS = slopes[jlimit (0, 3, (int) v ("lpSlope"))];
        const float hr = v ("hpRes") * 0.01f, lr = v ("lpRes") * 0.01f; const bool hpOn = on ("hpOn"), lpOn = on ("lpOn");
        if (hpF != s.hpF || hpS != s.hpS || hr != s.hpR || hpOn != s.hpOn) hp.set (true, hpOn, fsOs, hpF, hpS, hr);
        if (lpF != s.lpF || lpS != s.lpS || lr != s.lpR || lpOn != s.lpOn) lp.set (false, lpOn, fsOs, lpF, lpS, lr);
        s.hpF = hpF; s.hpS = hpS; s.hpR = hr; s.hpOn = hpOn; s.lpF = lpF; s.lpS = lpS; s.lpR = lr; s.lpOn = lpOn;
        const int pm = (int) v ("preModel"), pc = (int) v ("preColor"); const float pd = v ("preDrive"), po = v ("preOut"); const bool th = on ("preThump");
        const float thumpHz = hpOn ? hpF * 1.3f : 70.0f;
        if (pm != s.preM || pc != s.preC || pd != s.preD || po != s.preO || th != s.th || thumpHz != s.preHp)
        {
            pre.set (pm, pc, pd, po, th, thumpHz);
            s.preM = pm; s.preC = pc; s.preD = pd; s.preO = po; s.th = th; s.preHp = thumpHz;
        }
        // EQ
        std::array<cs::BandSettings, 4> bands {};
        for (int b = 0; b < 4; ++b)
        {
            auto& x = bands[(size_t) b];
            x.on = v (bid (b, "On")) > 0.5f; x.gain = v (bid (b, "Gain")); x.freq = v (bid (b, "Freq")); x.qScale = v (bid (b, "Q"));
            x.shelf = v (bid (b, "Shape")) > 0.5f; x.aux = v (bid (b, "Aux")); x.auxSel = (int) v (bid (b, "AuxSel"));
        }
        const int em = (int) v ("eqModel"); const float ed = v ("eqDrive") * 0.01f;
        eq.set (em, bands, ed);
        // DS2
        cs::DeEsser::P d[2];
        for (int k = 0; k < 2; ++k)
        {
            d[k].on = v (did (k, "On")) > 0.5f; d[k].freq = std::min (v (did (k, "Freq")), (float) (0.42 * baseFs));
            d[k].thr = v (did (k, "Thr")); d[k].range = v (did (k, "Range")); d[k].q = v (did (k, "Q"));
        }
        ds.set (d[0], d[1], v ("dsMode") > 0.5f, on ("dsListen"));
        // gate key filters, comp side-chain HPF (detector only)
        const float kh = v ("gtKeyHp"), kl = v ("gtKeyLp");
        if (kh != s.gKh || kl != s.gKl) { for (auto& g : gate) g.setKey (kh, kl); s.gKh = kh; s.gKl = kl; }
        const float sc = v ("cpScHp");
        if (sc != s.cSc) { for (auto& u : comp) u.setScHp (sc); s.cSc = sc; }
    }

    // stereo <-> mid/side
    static inline void toMS (float& a, float& b) { const float m = 0.5f * (a + b), s = 0.5f * (a - b); a = m; b = s; }
    static inline void fromMS (float& a, float& b) { const float l = a + b, r = a - b; a = l; b = r; }

    // PlateVerb compressor, one linked unit or two unlinked (DUO / MS), with a side-chain-only HPF
    struct CompUnit
    {
        pv::Compressor c[2]; pv::Biquad sc[2]; bool scOn = false; float fs = 48000;
        void prepare (float f) { fs = f; for (auto& x : c) { x.fs = f; x.lastFs = -1; x.reset(); } for (auto& b : sc) b.reset(); }
        void reset() { for (auto& x : c) x.reset(); for (auto& b : sc) b.reset(); }
        void setScHp (float hz) { scOn = hz > 1.0f; for (auto& b : sc) b.highpass (fs, std::max (10.0f, hz), 0.707f); }
        inline void process (float& L, float& R, float kL, float kR, const pv::CompParams& p, bool linked)
        {
            if (scOn) { kL = sc[0].process (kL); kR = sc[1].process (kR); }   // filters what the compressor hears, not the audio
            c[0].setTimes (p.atkMs, p.relMs);
            if (linked) { c[0].process (L, R, kL, kR, p); return; }
            c[1].setTimes (p.atkMs, p.relMs);
            float l2 = L, r2 = R;
            c[0].process (L, l2, kL, kL, p); c[1].process (R, r2, kR, kR, p);
        }
        float gr() const { return std::max (c[0].reductionDb(), c[1].reductionDb()); }
    };

    void processBlock (AudioBuffer<float>& buffer, MidiBuffer&) override
    {
        ScopedNoDenormals nd;
        const int n = buffer.getNumSamples();
        if (n <= 0) return;
        if (n > maxBlock) prepareToPlay (getSampleRate(), n);
        ensureRate(); updateSettings();

        const int nIn = getMainBusNumInputChannels(), nOut = getMainBusNumOutputChannels();
        auto main = getBusBuffer (buffer, false, 0);
        float* L = main.getWritePointer (0);
        float* R = nOut > 1 ? main.getWritePointer (1) : nullptr;
        const bool scBus = getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled();
        AudioBuffer<float> sc = scBus ? getBusBuffer (buffer, true, 1) : AudioBuffer<float>();
        const bool haveSc = scBus && sc.getNumChannels() > 0;

        work.setSize (2, n, false, false, true);
        float* xL = work.getWritePointer (0); float* xR = work.getWritePointer (1);
        // ---- input stage
        const int mode = (int) v ("inMode");
        const float ig = std::pow (10.0f, v ("inGain") * 0.05f);
        const float pl = on ("polL") ? -1.0f : 1.0f, pr = on ("polR") ? -1.0f : 1.0f;
        const bool rotOn = on ("rotOn");
        const float ang = v ("rotDeg") * MathConstants<float>::pi / 180.0f, cs_ = std::cos (ang), sn = std::sin (ang);
        for (int i = 0; i < n; ++i)
        {
            float a = L[i], b = (R != nullptr && nIn > 1) ? R[i] : L[i];
            switch (mode) { case 1: a = b = 0.5f * (a + b); break; case 2: b = a; break; case 3: a = b; break; case 4: std::swap (a, b); break; default: break; }
            a *= ig * pl; b *= ig * pr;
            if (rotOn) { a = rot.process (0, a, cs_, sn); b = rot.process (1, b, cs_, sn); }
            xL[i] = a; xR[i] = b;
            inPeak = jmax (inPeak * 0.9995f, std::fabs (a), std::fabs (b));
        }

        // ---- modules in the chosen order
        const auto order = cs::orderFromIndex ((int) v ("order"));
        float gGr = 0, cGr = 0, lGr = 0, dGr = 0;
        for (int k = 0; k < cs::kNumModules; ++k)
        {
            const int m = order[(size_t) k];
            const int route = kRouteIds[m][0] != 0 ? (int) v (kRouteIds[m]) : cs::RouteST;
            const bool modOn = on (kOnIds[m]);
            const bool ms = route == cs::RouteMS, linked = route == cs::RouteST;
            if (m == cs::ModPre) { processPre (n, modOn, ms); continue; }      // the PRE oversampler always runs: constant latency
            if (! modOn) continue;
            if (m == cs::ModInsert) { processInsert (n); continue; }
            if (ms) for (int i = 0; i < n; ++i) toMS (xL[i], xR[i]);
            switch (m)
            {
                case cs::ModEq:
                    for (int i = 0; i < n; ++i)
                    {
                        if ((i & 15) == 0) eq.step();
                        xL[i] = eq.process (0, xL[i]); xR[i] = eq.process (1, xR[i]);
                    }
                    break;
                case cs::ModDs:
                    for (int i = 0; i < n; ++i) ds.process (xL[i], xR[i], linked);
                    dGr = ds.grDb;
                    break;
                case cs::ModComp: processComp (n, linked, ms, haveSc ? &sc : nullptr); cGr = curGr; break;
                case cs::ModGate:
                {
                    cs::Gate::P gp; gp.expander = v ("gtMode") > 0.5f; gp.thr = v ("gtThr"); gp.range = v ("gtRange"); gp.atkMs = v ("gtAtk");
                    gp.holdMs = v ("gtHold"); gp.relMs = v ("gtRel"); gp.hyst = v ("gtHyst"); gp.listen = on ("gtListen");
                    const bool ext = v ("gtKey") > 0.5f && haveSc;
                    const float* sL = ext ? sc.getReadPointer (0) : nullptr;
                    const float* sR = ext ? sc.getReadPointer (sc.getNumChannels() > 1 ? 1 : 0) : nullptr;
                    for (int i = 0; i < n; ++i)
                    {
                        float kL = ext ? sL[i] : xL[i], kR = ext ? sR[i] : xR[i];
                        if (ext && ms) toMS (kL, kR);
                        if (linked) gate[0].process (xL[i], xR[i], kL, kR, gp);
                        else { float d1 = xL[i], d2 = xR[i]; gate[0].process (xL[i], d1, kL, kL, gp); gate[1].process (xR[i], d2, kR, kR, gp); }
                    }
                    gGr = jmax (gate[0].grDb, linked ? 0.0f : gate[1].grDb);
                    break;
                }
                case cs::ModLim:
                {
                    const float drv = std::pow (10.0f, v ("lmDrive") * 0.05f), ceil = std::pow (10.0f, v ("lmCeil") * 0.05f), rel = v ("lmRel");
                    auto& lim = curZero ? limZero : limLA;
                    for (int i = 0; i < n; ++i) { xL[i] *= drv; xR[i] *= drv; lim.process (xL[i], xR[i], ceil, rel); }
                    lGr = lim.grDb;
                    break;
                }
                default: break;
            }
            if (ms) for (int i = 0; i < n; ++i) fromMS (xL[i], xR[i]);
        }
        // the look-ahead limiter's delay stays in the path when the limiter is off, so latency is constant
        if (! on ("lmOn") && ! curZero) for (int i = 0; i < n; ++i) limLA.process (xL[i], xR[i], 1.0e6f, 60.0f);

        const float og = std::pow (10.0f, v ("outGain") * 0.05f);
        for (int i = 0; i < n; ++i)
        {
            const float a = xL[i] * og, b = xR[i] * og;
            if (R != nullptr) { L[i] = a; R[i] = b; } else L[i] = 0.5f * (a + b);
            outPeak = jmax (outPeak * 0.9995f, std::fabs (a), std::fabs (b));
            outRms = outRms * 0.9995f + 0.0005f * 0.5f * (a * a + b * b);
        }
        gateGr.store (gGr); compGr.store (cGr); limGr.store (lGr); dsGr.store (dGr);
        inMeter.store (inPeak); outMeter.store (outPeak); vuMeter.store (std::sqrt (outRms));
    }

    void processPre (int n, bool modOn, bool ms)
    {
        dsp::AudioBlock<float> blk (work.getArrayOfWritePointers(), 2, (size_t) n);
        auto& O = *os[(size_t) curOs];
        auto up = curOs > 0 ? O.processSamplesUp (blk) : blk;
        if (modOn)
        {
            float* a = up.getChannelPointer (0); float* b = up.getChannelPointer (1);
            const int m = (int) up.getNumSamples();
            for (int i = 0; i < m; ++i)
            {
                float x = a[i], y = b[i];
                if (ms) toMS (x, y);
                x = pre.process (0, lp.process (0, hp.process (0, x)));
                y = pre.process (1, lp.process (1, hp.process (1, y)));
                if (ms) fromMS (x, y);
                a[i] = x; b[i] = y;
            }
        }
        if (curOs > 0) O.processSamplesDown (blk);
    }

    void processComp (int n, bool linked, bool ms, AudioBuffer<float>* sc)
    {
        float* xL = work.getWritePointer (0); float* xR = work.getWritePointer (1);
        const int type = jlimit (0, 2, (int) v ("cpType"));
        if (type != curType)
        {
            if (curType >= 0) { prevType = curType; activeUnit ^= 1; comp[activeUnit].reset(); fade = 0.0f; }
            curType = type;
        }
        pv::CompParams p; p.on = true; p.duck = false; p.type = type; p.vintage = v ("cpTone") > 0.5f;
        p.ratio = v ("cpRatio"); p.atkMs = v ("cpAtk") * 1000.0f; p.relMs = v ("cpRel") * 1000.0f;
        const float tThr = v ("cpThr"), tMk = v ("cpMakeup"), tMix = v ("cpMix") * 0.01f;
        const float a = 1.0f - std::exp (-1.0f / (0.02f * (float) baseFs));          // ~20 ms parameter smoothing
        const float fadeStep = 1.0f / (0.03f * (float) baseFs);                         // 30 ms crossfade on type change
        const bool ext = v ("cpKey") > 0.5f && sc != nullptr;
        const float* sL = ext ? sc->getReadPointer (0) : nullptr;
        const float* sR = ext ? sc->getReadPointer (sc->getNumChannels() > 1 ? 1 : 0) : nullptr;
        for (int i = 0; i < n; ++i)
        {
            smThr += (tThr - smThr) * a; smMk += (tMk - smMk) * a; smMix += (tMix - smMix) * a;
            p.thr = smThr; p.makeup = smMk; p.mix = smMix;
            float kL = ext ? sL[i] : xL[i], kR = ext ? sR[i] : xR[i];
            if (ext && ms) toMS (kL, kR);
            float l = xL[i], r = xR[i];
            comp[activeUnit].process (l, r, kL, kR, p, linked);
            if (fade < 1.0f)
            {
                auto q = p; q.type = prevType;
                float l2 = xL[i], r2 = xR[i];
                comp[activeUnit ^ 1].process (l2, r2, kL, kR, q, linked);
                l = l2 + (l - l2) * fade; r = r2 + (r - r2) * fade;
                fade = std::min (1.0f, fade + fadeStep);
            }
            xL[i] = l; xR[i] = r;
        }
        curGr = comp[activeUnit].gr();
    }

    // -------------------------------------------------------------------- insert slot (VST3 host)
    void processInsert (int n)
    {
        const ScopedTryLock sl (insLock);
        if (! sl.isLocked() || hosted == nullptr) return;
        float* ch[2] = { work.getWritePointer (0), work.getWritePointer (1) };
        AudioBuffer<float> b (ch, 2, n);
        MidiBuffer midi;
        hosted->processBlock (b, midi);
    }
    void prepareInsert()
    {
        const ScopedLock sl (insLock);
        if (hosted != nullptr) { hosted->setPlayConfigDetails (2, 2, baseFs, maxBlock); hosted->prepareToPlay (baseFs, maxBlock); insLatency = hosted->getLatencySamples(); }
    }

    bool loadInsert (const String& path, String& err, const MemoryBlock* stateData = nullptr)
    {
        OwnedArray<PluginDescription> found;
        VST3PluginFormat fmt; fmt.findAllTypesForFile (found, path);
        if (found.isEmpty()) { err = "No VST3 plugin found in that file."; return false; }
        auto inst = formats.createPluginInstance (*found[0], baseFs, maxBlock, err);
        if (inst == nullptr) return false;
        inst->enableAllBuses();
        inst->setPlayConfigDetails (2, 2, baseFs, maxBlock);
        inst->prepareToPlay (baseFs, maxBlock);
        if (stateData != nullptr && stateData->getSize() > 0) inst->setStateInformation (stateData->getData(), (int) stateData->getSize());
        {
            const ScopedLock sl (insLock);
            std::swap (hosted, inst);
            insDesc = *found[0];
            insLatency = hosted->getLatencySamples();
        }
        if (inst != nullptr) inst->releaseResources();
        insertChanged.sendChangeMessage();
        return true;
    }
    void removeInsert()
    {
        std::unique_ptr<AudioPluginInstance> old;
        { const ScopedLock sl (insLock); std::swap (old, hosted); insLatency = 0; }
        insertChanged.sendChangeMessage();
    }
    AudioPluginInstance* insertPlugin() const { return hosted.get(); }
    String insertName() const { return hosted != nullptr ? hosted->getName() : String(); }
    ChangeBroadcaster insertChanged;

    // -------------------------------------------------------------------- A/B/C/D, presets
    void selectSlot (int k)
    {
        if (k == curSlot || k < 0 || k > 3) return;
        slots[curSlot] = apvts.copyState();
        if (slots[k].isValid()) loadParams (slots[k]);
        curSlot = k;
    }
    int currentSlot() const { return curSlot; }
    void loadParams (const ValueTree& t)
    {
        const ScopedValueSetter<bool> svs (restoring, true);
        for (auto* p : getParameters())
            if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
            {
                const auto child = t.getChildWithProperty ("id", rp->paramID);
                if (child.isValid()) rp->setValueNotifyingHost (rp->convertTo0to1 ((float) child.getProperty ("value")));
            }
    }
    void applyFactory (int index)
    {
        const auto& list = factoryPresets();
        if (index < 0 || index >= (int) list.size()) return;
        const ScopedValueSetter<bool> svs (restoring, true);
        for (auto* p : getParameters())
            if (auto* rp = dynamic_cast<RangedAudioParameter*> (p)) if (rp->paramID != "order") rp->setValueNotifyingHost (rp->getDefaultValue());
        for (auto& [id, val] : list[(size_t) index].v)
            if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (val));
        presetName = list[(size_t) index].name;
    }
    static File userPresetFolder() { return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("ChannelStrip").getChildFile ("Presets"); }
    void saveUserPreset (const File& f)
    {
        if (auto x = apvts.copyState().createXml()) { f.getParentDirectory().createDirectory(); x->writeTo (f); presetName = f.getFileNameWithoutExtension(); }
    }
    void loadUserPreset (const File& f)
    {
        if (auto x = parseXML (f)) { loadParams (ValueTree::fromXml (*x)); presetName = f.getFileNameWithoutExtension(); }
    }
    String presetName { "Default" };

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
    const String getProgramName (int) override { return presetName; }
    void changeProgramName (int, const String&) override {}

    void getStateInformation (MemoryBlock& d) override
    {
        auto st = apvts.copyState();
        st.setProperty ("preset", presetName, nullptr); st.setProperty ("slot", curSlot, nullptr);
        ValueTree sl ("SLOTS");
        for (int k = 0; k < 4; ++k) if (slots[k].isValid() && k != curSlot) { auto c = slots[k].createCopy(); c.setProperty ("slotIndex", k, nullptr); sl.addChild (c, -1, nullptr); }
        st.addChild (sl, -1, nullptr);
        {
            const ScopedLock l (insLock);
            if (hosted != nullptr)
            {
                ValueTree ins ("INSERT");
                if (auto x = insDesc.createXml()) ins.setProperty ("desc", x->toString(), nullptr);
                MemoryBlock pm; hosted->getStateInformation (pm);
                ins.setProperty ("state", pm.toBase64Encoding(), nullptr);
                st.addChild (ins, -1, nullptr);
            }
        }
        if (auto x = st.createXml()) copyXmlToBinary (*x, d);
    }
    void setStateInformation (const void* d, int s) override
    {
        auto x = getXmlFromBinary (d, s);
        if (x == nullptr || ! x->hasTagName (apvts.state.getType())) return;
        auto st = ValueTree::fromXml (*x);
        auto sl = st.getChildWithName ("SLOTS"); auto ins = st.getChildWithName ("INSERT");
        for (int k = 0; k < 4; ++k) slots[k] = ValueTree();
        for (auto c : sl) { const int k = c.getProperty ("slotIndex", -1); if (k >= 0 && k < 4) slots[k] = c.createCopy(); }
        st.removeChild (sl, nullptr); st.removeChild (ins, nullptr);
        presetName = st.getProperty ("preset", "Default").toString(); curSlot = jlimit (0, 3, (int) st.getProperty ("slot", 0));
        { const ScopedValueSetter<bool> svs (restoring, true); apvts.replaceState (st); }
        if (ins.isValid())
        {
            const String desc = ins.getProperty ("desc").toString(), state = ins.getProperty ("state").toString();
            MessageManager::callAsync ([this, desc, state] {
                PluginDescription pd;
                if (auto dx = parseXML (desc))
                    if (pd.loadFromXml (*dx))
                    {
                        MemoryBlock mb; mb.fromBase64Encoding (state); String err;
                        loadInsert (pd.fileOrIdentifier, err, &mb);
                    }
            });
        }
    }

    UndoManager undo;
    APVTS apvts;
    std::atomic<float> gateGr { 0 }, compGr { 0 }, limGr { 0 }, dsGr { 0 }, inMeter { 0 }, outMeter { 0 }, vuMeter { 0 };
    double sampleRateForUi() const { return baseFs; }
    const ValueTree& slotTree (int k) const { return slots[k]; }

private:
    std::map<String, std::atomic<float>*> raw;
    double baseFs = 48000; int maxBlock = 512, curOs = -1, curZero = -1;
    std::unique_ptr<dsp::Oversampling<float>> os[3];
    AudioBuffer<float> work;
    cs::Preamp pre; cs::CutFilter hp, lp; cs::EqBank eq; cs::DS2 ds;
    CompUnit comp[2]; int activeUnit = 0, curType = -1, prevType = 0; float fade = 1.0f, curGr = 0, smThr = -20, smMk = 0, smMix = 1;
    cs::Gate gate[2]; cs::Limiter limLA, limZero; cs::PhaseRotator rot;
    float inPeak = 0, outPeak = 0, outRms = 0;
    AudioPluginFormatManager formats; std::unique_ptr<AudioPluginInstance> hosted; PluginDescription insDesc; CriticalSection insLock;
    int insLatency = 0, lastInsLatency = -1;
    ValueTree slots[4]; int curSlot = 0; bool restoring = false;
};

#include "CSEditor.h"

AudioProcessorEditor* ChannelStripProcessor::createEditor() { return new ChannelStripEditor (*this); }
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ChannelStripProcessor(); }
