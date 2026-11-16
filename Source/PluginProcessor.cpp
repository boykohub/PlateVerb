// PlateVerb - five reverb slots (Plate, Room, Chamber, Hall, IR), each with its own reverb, EQ and
// tail compressor, plus stereo delay and output transformer.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Engine.h"
#include "IRRender.h"
#include "PartConv.h"
#include "Presets.h"

using namespace juce;
using APVTS = AudioProcessorValueTreeState;

static const char* const kSlotNames[] = { "PLATE", "ROOM", "CHAMBER", "HALL", "IR" };
static const int kOsValues[] = { 1, 2, 3, 4 };
static const float kDivBeats[] = { 0.0625f, 1.0f / 12.0f, 0.125f, 1.0f / 6.0f, 0.25f, 0.375f, 1.0f / 3.0f, 0.5f,
                                   0.75f, 2.0f / 3.0f, 1.0f, 1.5f, 4.0f / 3.0f, 2.0f, 3.0f, 4.0f };

static StringArray osNames()  { return { "1x", "2x", "3x", "4x" }; }
static StringArray divNames() { return { "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8",
                                         "1/8D", "1/4T", "1/4", "1/4D", "1/2T", "1/2", "1/2D", "1/1" }; }
static StringArray trNames()  { StringArray s; for (int i = 0; i < pv::kNumTransformers; ++i) s.add (pv::transformerModel (i).name); return s; }
static StringArray compTypeNames() { return { "Clean VCA", "1176-style FET", "LA-2A-style Opto", "1176 + LA-2A" }; }
static StringArray materialNames (int slot)
{
    StringArray s;
    if (slot == pv::Plate) for (int i = 0; i < pv::kNumPlates; ++i) s.add (pv::plateMaterial (i).name);
    else for (int i = 0; i < pv::kNumSurfaces; ++i) s.add (pv::surfaceMaterial (i).name);
    return s;
}

static String fmtSeconds (float v)
{
    const int d = jlimit (2, 6, (int) std::ceil (-std::log10 (jmax (1.0e-6f, v))) + 1);
    return String (v, d) + " s";
}
static String sid (int slot, const String& base) { return "s" + String (slot) + "_" + base; }
static String nid (const char* prefix, int i) { return String (prefix) + String (i); }
static String slotTitle (int s) { String n = String (kSlotNames[s]).toLowerCase(); return s == pv::IR ? String ("IR") : n.substring (0, 1).toUpperCase() + n.substring (1); }

static const char* const kSlotFloatIds[] = { "matMix", "dirt", "hiCut", "lv", "decay", "lowMult", "highMult", "size", "predelay", "build", "early", "diff", "damping",
                                             "tone", "mod", "inWidth", "eqF0", "eqF1", "eqF2", "eqF3", "eqG0", "eqG1", "eqG2", "eqG3",
                                             "cpThr", "cpRatio", "cpAtk", "cpRel", "cpMakeup", "cpMix" };
static const char* const kSlotOtherIds[] = { "on", "cpOn", "cpDuck", "cpType", "cpTone", "matA", "matB", "conv" };
static const StringArray kConverterNames { "24-bit (modern)", "12-bit", "15-bit (EMT 250)", "16-bit (Lexicon 224)" };

static File userPresetFolder()
{
    return File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("PlateVerb").getChildFile ("Presets");
}

// "X.L.wav" <-> "X.R.wav", also _L/_R, " L"/" R", -L/-R, (L)/(R) and Left/Right in any of those forms
static const char* const kIrSideTags[][2] = { { "L", "R" }, { "Left", "Right" }, { "LEFT", "RIGHT" }, { "left", "right" }, { "l", "r" } };
static File findPartner (const File& f, bool& isLeft)
{
    const String stem = f.getFileNameWithoutExtension();
    for (auto& tag : kIrSideTags)
        for (auto* sep : { ".", "_", " ", "-", "(" })
            for (int side = 0; side < 2; ++side)
            {
                const String close = String (sep) == "(" ? ")" : "";
                const String me = String (sep) + tag[side] + close, other = String (sep) + tag[1 - side] + close;
                if (stem.endsWith (me))
                {
                    File p = f.getSiblingFile (stem.dropLastCharacters (me.length()) + other + f.getFileExtension());
                    if (p.existsAsFile()) { isLeft = side == 0; return p; }
                }
            }
    return {};
}
// 0 = named as a left file, 1 = right, -1 = can't tell
static int irSide (const File& f)
{
    const String stem = f.getFileNameWithoutExtension().toUpperCase();
    for (auto* sep : { ".", "_", " ", "-", "(" })
    {
        const String close = String (sep) == "(" ? ")" : "";
        for (auto* l : { "L", "LEFT" }) if (stem.endsWith (String (sep) + l + close)) return 0;
        for (auto* r : { "R", "RIGHT" }) if (stem.endsWith (String (sep) + r + close)) return 1;
    }
    return -1;
}

// ============================================================================ processor
class PlateVerbProcessor : public AudioProcessor, private Thread
{
public:
    PlateVerbProcessor()
        : AudioProcessor (BusesProperties()
              .withInput ("In", AudioChannelSet::stereo(), true)
              .withOutput ("Out", AudioChannelSet::stereo(), true)),
          Thread ("PlateVerb IR"),
          apvts (*this, nullptr, "PARAMS", createLayout())
    {
        formats.registerBasicFormats();
        auto P = [this] (const String& id) { return apvts.getRawParameterValue (id); };
        for (int s = 0; s < pv::kNumSlots; ++s)
        {
            auto& q = sp[s];
            q.on = P (sid (s, "on")); q.lv = P (sid (s, "lv")); q.decay = P (sid (s, "decay")); q.low = P (sid (s, "lowMult"));
            q.high = P (sid (s, "highMult")); q.size = P (sid (s, "size")); q.pre = P (sid (s, "predelay")); q.build = P (sid (s, "build"));
            q.early = P (sid (s, "early")); q.diff = P (sid (s, "diff")); q.damp = P (sid (s, "damping")); q.tone = P (sid (s, "tone"));
            q.mod = P (sid (s, "mod")); q.width = P (sid (s, "inWidth"));
            q.matA = P (sid (s, "matA")); q.matB = P (sid (s, "matB")); q.matMix = P (sid (s, "matMix")); q.dirt = P (sid (s, "dirt")); q.hiCut = P (sid (s, "hiCut")); q.conv = P (sid (s, "conv"));
            for (int b = 0; b < 4; ++b) { q.eqF[b] = P (sid (s, nid ("eqF", b))); q.eqG[b] = P (sid (s, nid ("eqG", b))); }
            q.cpOn = P (sid (s, "cpOn")); q.cpDuck = P (sid (s, "cpDuck")); q.cpType = P (sid (s, "cpType")); q.cpTone = P (sid (s, "cpTone"));
            q.cpThr = P (sid (s, "cpThr")); q.cpRatio = P (sid (s, "cpRatio")); q.cpAtk = P (sid (s, "cpAtk")); q.cpRel = P (sid (s, "cpRel"));
            q.cpMakeup = P (sid (s, "cpMakeup")); q.cpMix = P (sid (s, "cpMix"));
        }
        pOs = P ("os"); pMix = P ("mix");
        pDlPanL = P ("dlPanL"); pDlPanR = P ("dlPanR"); pDlWidth = P ("dlWidth"); pDlAtk = P ("dlAtk");
        pDlOn = P ("dlOn"); pDlSync = P ("dlSync"); pDlPing = P ("dlPing"); pDlTimeL = P ("dlTimeL"); pDlTimeR = P ("dlTimeR");
        pDlDivL = P ("dlDivL"); pDlDivR = P ("dlDivR"); pDlFb = P ("dlFb"); pDlMix = P ("dlMix");
        pDlLow = P ("dlLow"); pDlMid = P ("dlMid"); pDlMidF = P ("dlMidF"); pDlHigh = P ("dlHigh");
        pTrOn = P ("trOn"); pTrModel = P ("trModel"); pTrFreq = P ("trFreq"); pTrAmt = P ("trAmt"); pOut = P ("out");
        pIrStart = P ("irStart"); pIrXover = P ("irXover"); pIrFade = P ("irFadeOut"); pIrXfeed = P ("irXfeed");
        pIrXdelay = P ("irXdelay"); pIrAsym = P ("irAsym"); pIrReverse = P ("irReverse"); pIrPitch = P ("irPitch");
        startThread (Thread::Priority::low);
    }
    ~PlateVerbProcessor() override { alive.reset(); stopThread (4000); }

    // ------------------------------------------------------------------ parameters
    static constexpr float kLog = -2.0f;
    static std::unique_ptr<AudioParameterFloat> fp (const String& id, const String& name, float lo, float hi,
                                                    float def, float centre = -1.0f, const String& unit = "%", int decimals = 0,
                                                    const String& zeroText = {})
    {
        NormalisableRange<float> r (lo, hi);
        if (centre == kLog && lo > 0.0f)
            r = NormalisableRange<float> (lo, hi,
                    [] (float a, float b, float t) { return a * std::pow (b / a, t); },
                    [] (float a, float b, float v) { return std::log (v / a) / std::log (b / a); });
        else if (centre != -1.0f && centre > lo && centre < hi) r.setSkewForCentre (centre);   // -1 = linear
        const bool pct = unit == "%", pan = unit == "pan", cut = unit == "cut";
        auto attr = AudioParameterFloatAttributes()
            .withLabel (pan || cut ? String() : unit)
            .withStringFromValueFunction ([pct, pan, cut, decimals, zeroText, lo] (float v, int)
            {
                if (zeroText.isNotEmpty() && v <= lo + 1.0e-4f) return zeroText;
                if (cut) return v >= 19500.0f ? String ("OFF") : String (v / 1000.0f, 1) + " kHz";
                if (pan) { const int p = roundToInt (v * 100.0f); return p == 0 ? String ("C") : (p < 0 ? "L" + String (-p) : "R" + String (p)); }
                if (decimals < 0) return fmtSeconds (v).dropLastCharacters (2);
                return pct ? String (roundToInt (v * 100.0f)) : String (v, decimals);
            })
            .withValueFromStringFunction ([pct, pan, cut] (const String& t)
            {
                if (cut) { if (t.trim().equalsIgnoreCase ("off")) return 20000.0f; const float v = t.getFloatValue(); return t.containsIgnoreCase ("k") || v < 100.0f ? v * 1000.0f : v; }
                if (pan) { auto u = t.trim().toUpperCase(); float v = u.substring (1).getFloatValue() / 100.0f; return u.startsWith ("L") ? -v : (u.startsWith ("R") ? v : 0.0f); }
                return pct ? t.getFloatValue() / 100.0f : t.getFloatValue();
            });
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 2 }, name, r, def, attr);
    }
    static std::unique_ptr<AudioParameterBool> bp (const String& id, const String& name, bool def)
    {
        return std::make_unique<AudioParameterBool> (ParameterID { id, 2 }, name, def);
    }
    static std::unique_ptr<AudioParameterChoice> cp (const String& id, const String& name, const StringArray& c, int def)
    {
        return std::make_unique<AudioParameterChoice> (ParameterID { id, 2 }, name, c, def);
    }

    static APVTS::ParameterLayout createLayout()
    {
        APVTS::ParameterLayout l;
        l.add (cp ("os", "Oversampling", osNames(), 0));
        l.add (fp ("mix", "Reverb Mix", 0.0f, 1.0f, 0.3f));
        const char* bandNames[] = { "Low", "Low Mid", "High Mid", "High" };
        const float fLo[] = { 20, 100, 800, 2000 }, fHi[] = { 500, 2000, 8000, 20000 }, fDef[] = { 100, 400, 2500, 8000 };
        for (int s = 0; s < pv::kNumSlots; ++s)
        {
            auto g = std::make_unique<AudioProcessorParameterGroup> ("slot" + String (s), slotTitle (s), "|");
            const String n = slotTitle (s) + " ";
            const bool ir = s == pv::IR;
            g->addChild (bp (sid (s, "on"), n + "On", s == 0));
            g->addChild (fp (sid (s, "lv"), n + "Level", 0.0f, 1.0f, 1.0f));
            g->addChild (fp (sid (s, "decay"), n + "Decay", 0.05f, 20.0f, 2.5f, 2.5f, "s", 2));
            g->addChild (fp (sid (s, "lowMult"), n + "Low Decay", 0.3f, 3.0f, 1.0f, kLog, "x", 2));
            g->addChild (fp (sid (s, "highMult"), n + "High Decay", 0.15f, 1.5f, 1.0f, kLog, "x", 2));
            g->addChild (fp (sid (s, "size"), n + "Size", 0.5f, 1.5f, 1.0f, -1.0f, "x", 2));
            g->addChild (fp (sid (s, "predelay"), n + "Pre-Delay", 0.0f, 250.0f, ir ? 0.0f : 10.0f, 40.0f, "ms"));
            g->addChild (fp (sid (s, "build"), n + "Build", 3.0f, 230.0f, ir ? 3.0f : 15.0f, kLog, "ms"));
            g->addChild (fp (sid (s, "early"), n + "Early Reflections", 0.0f, 1.5f, 0.7f));
            g->addChild (fp (sid (s, "diff"), n + "Diffusion", 0.0f, 1.0f, ir ? 0.0f : 0.7f));
            g->addChild (fp (sid (s, "damping"), n + "Damping", 0.0f, 1.0f, ir ? 0.0f : 0.35f));
            g->addChild (fp (sid (s, "tone"), n + "Input Tone", 0.0f, 1.0f, ir ? 1.0f : 0.75f));
            g->addChild (fp (sid (s, "mod"), n + "Modulation", 0.0f, 1.0f, ir ? 0.0f : 0.5f));
            g->addChild (fp (sid (s, "inWidth"), n + "Input Width", 0.0f, 1.0f, 1.0f));
            g->addChild (cp (sid (s, "matA"), n + "Material A", materialNames (s), 0));
            g->addChild (cp (sid (s, "matB"), n + "Material B", materialNames (s), 0));
            g->addChild (fp (sid (s, "matMix"), n + "Material Blend", 0.0f, 1.0f, 0.0f));
            g->addChild (fp (sid (s, "dirt"), n + "Dirt", 1.0f, 100.0f, pv::kDirtDefault, -1.0f, "", 0));
            g->addChild (fp (sid (s, "hiCut"), n + "High Cut", 2000.0f, 20000.0f, 20000.0f, kLog, "cut"));
            g->addChild (cp (sid (s, "conv"), n + "Converter", kConverterNames, 0));
            for (int b = 0; b < 4; ++b)
            {
                g->addChild (fp (sid (s, nid ("eqF", b)), n + "EQ " + bandNames[b] + " Freq", fLo[b], fHi[b], fDef[b], kLog, "Hz"));
                g->addChild (fp (sid (s, nid ("eqG", b)), n + "EQ " + bandNames[b] + " Gain", -15.0f, 15.0f, 0.0f, -1.0f, "dB", 1));
            }
            g->addChild (bp (sid (s, "cpOn"), n + "Comp On", false));
            g->addChild (bp (sid (s, "cpDuck"), n + "Comp Ducking", false));
            g->addChild (cp (sid (s, "cpType"), n + "Comp Type", compTypeNames(), 0));
            g->addChild (cp (sid (s, "cpTone"), n + "Comp Tone", StringArray { "Clean", "Vintage" }, 0));
            g->addChild (fp (sid (s, "cpThr"), n + "Comp Threshold", -80.0f, 0.0f, -30.0f, -1.0f, "dB", 1));
            g->addChild (fp (sid (s, "cpRatio"), n + "Comp Ratio", 1.0f, 20.0f, 4.0f, 4.0f, ":1", 1));
            g->addChild (fp (sid (s, "cpAtk"), n + "Comp Attack", 0.00002f, 0.3f, 0.01f, kLog, "s", -1));
            g->addChild (fp (sid (s, "cpRel"), n + "Comp Release", 0.02f, 10.0f, 0.5f, kLog, "s", -1));
            g->addChild (fp (sid (s, "cpMakeup"), n + "Comp Makeup", 0.0f, 24.0f, 0.0f, -1.0f, "dB", 1));
            g->addChild (fp (sid (s, "cpMix"), n + "Comp Mix", 0.0f, 1.0f, 1.0f));
            l.add (std::move (g));
        }
        l.add (fp ("irStart", "IR Start Trim", 0.0f, 150.0f, 0.0f, 30.0f, "ms", 1));
        l.add (fp ("irXover", "IR ER/LR Crossover", 0.0f, 500.0f, 80.0f, 100.0f, "ms"));
        l.add (fp ("irFadeOut", "IR Fade-Out", 0.0f, 2000.0f, 0.0f, 300.0f, "ms"));
        l.add (fp ("irXfeed", "IR Cross-Feed", 0.0f, 1.0f, 0.0f));
        l.add (fp ("irXdelay", "IR Cross-Delay", 0.0f, 25.0f, 0.0f, 5.0f, "ms", 1));
        l.add (fp ("irAsym", "IR Asymmetry", -1.0f, 1.0f, 0.0f, -1.0f, "pan"));
        l.add (bp ("irReverse", "IR Reverse", false));
        l.add (bp ("irPitch", "IR Stretch With Pitch", false));
        l.add (bp ("dlOn", "Delay On", false));
        l.add (bp ("dlSync", "Delay Sync", true));
        l.add (bp ("dlPing", "Delay Ping-Pong", false));
        l.add (fp ("dlTimeL", "Delay Time L", 1.0f, 2990.0f, 375.0f, 400.0f, "ms"));
        l.add (fp ("dlTimeR", "Delay Time R", 1.0f, 2990.0f, 500.0f, 400.0f, "ms"));
        l.add (cp ("dlDivL", "Delay Div L", divNames(), 10));
        l.add (cp ("dlDivR", "Delay Div R", divNames(), 8));
        l.add (fp ("dlFb", "Delay Feedback", 0.0f, 0.95f, 0.35f));
        l.add (fp ("dlMix", "Delay Mix", 0.0f, 1.0f, 0.3f));
        l.add (fp ("dlAtk", "Delay Attack", 0.0f, 500.0f, 0.0f, 60.0f, "ms", 0, "LIVE"));
        l.add (fp ("dlPanL", "Delay Pan L", -1.0f, 1.0f, -1.0f, -1.0f, "pan"));
        l.add (fp ("dlPanR", "Delay Pan R", -1.0f, 1.0f, 1.0f, -1.0f, "pan"));
        l.add (fp ("dlWidth", "Delay Width", 0.0f, 2.0f, 1.0f));
        l.add (fp ("dlLow", "Delay EQ Low", -12.0f, 12.0f, 0.0f, -1.0f, "dB", 1));
        l.add (fp ("dlMid", "Delay EQ Mid", -12.0f, 12.0f, 0.0f, -1.0f, "dB", 1));
        l.add (fp ("dlMidF", "Delay EQ Mid Freq", 200.0f, 5000.0f, 1000.0f, kLog, "Hz"));
        l.add (fp ("dlHigh", "Delay EQ High", -12.0f, 12.0f, 0.0f, -1.0f, "dB", 1));
        l.add (bp ("trOn", "Transformer On", false));
        l.add (cp ("trModel", "Transformer Model", trNames(), 0));
        l.add (fp ("trFreq", "Transformer Freq", 20.0f, 400.0f, 80.0f, kLog, "Hz"));
        l.add (fp ("trAmt", "Transformer Amount", 0.0f, 1.0f, 0.3f));
        l.add (fp ("out", "Output", -24.0f, 12.0f, 0.0f, -1.0f, "dB", 1));
        return l;
    }

    // ------------------------------------------------------------------ audio
    void prepareToPlay (double sampleRate, int maxBlock) override
    {
        sampleRateHz = sampleRate;
        blockMax = jmax (64, maxBlock);
        engine.prepare (sampleRate);
        fillParams (prm, true);
        engine.setOversampling (prm.oversampling, true);
        setLatencySamples (engine.latency());
        for (auto* b : { &inL, &inR, &irL, &irR, &nxL, &nxR }) b->assign ((size_t) blockMax, 0.0f);
        trash.reserve (8);
        hostRate.store (sampleRate);
        notify();
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
        fillParams (prm, true);
        const auto t0 = Time::getHighResolutionTicks();
        {
            const GenericScopedTryLock<SpinLock> tl (swapLock);
            if (tl.isLocked() && pending != nullptr && next == nullptr)
            {
                if (cur == nullptr) cur = std::move (pending);
                else { next = std::move (pending); xfade = 0; }
            }
        }
        const bool irOn = prm.slot[pv::IR].on && irReady.load() && cur != nullptr;
        if (irOn && ! irWasOn) cur->reset();
        irWasOn = irOn;
        float* L = buffer.getWritePointer (0); float* R = buffer.getWritePointer (1);
        for (int pos = 0; pos < n; pos += blockMax)
        {
            const int m = jmin (blockMax, n - pos);
            if (irOn)
            {
                engine.irInput (L + pos, R + pos, inL.data(), inR.data(), m, prm);
                cur->process (inL.data(), inR.data(), irL.data(), irR.data(), m);
                if (next != nullptr)                       // new IR: 50 ms crossfade from the old one
                {
                    next->process (inL.data(), inR.data(), nxL.data(), nxR.data(), m);
                    const int fadeLen = jmax (64, (int) (0.05 * sampleRateHz));
                    for (int i = 0; i < m; ++i)
                    {
                        const float g = jmin (1.0f, (float) (xfade + i) / (float) fadeLen);
                        irL[(size_t) i] += (nxL[(size_t) i] - irL[(size_t) i]) * g;
                        irR[(size_t) i] += (nxR[(size_t) i] - irR[(size_t) i]) * g;
                    }
                    xfade += m;
                    if (xfade >= fadeLen)
                    {
                        const GenericScopedTryLock<SpinLock> tl (swapLock);
                        if (tl.isLocked()) { trash.push_back (std::move (cur)); cur = std::move (next); }   // freed by the background thread
                    }
                }
                engine.process (L + pos, R + pos, m, prm, irL.data(), irR.data());
            }
            else engine.process (L + pos, R + pos, m, prm);
        }
        const double used = Time::highResolutionTicksToSeconds (Time::getHighResolutionTicks() - t0);
        const float load = (float) (100.0 * used / (n / sampleRateHz));
        dspLoad.store (dspLoad.load() * 0.9f + load * 0.1f);
        for (int s = 0; s < pv::kNumSlots; ++s)
        {
            compGr[s].store (prm.slot[s].comp.on ? engine.compReductionDb (s) : 0.0f);
            compIn[s].store (engine.compInputDb (s));
        }
        const int lat = engine.latency();
        if (lat != getLatencySamples()) setLatencySamples (lat);
    }

    void fillParams (pv::Params& p, bool audioThread)
    {
        auto f = [] (std::atomic<float>* a) { return a->load(); };
        auto on = [] (std::atomic<float>* a) { return a->load() > 0.5f; };
        p.oversampling = kOsValues[jlimit (0, 3, (int) f (pOs))];
        p.rvMix = f (pMix);
        for (int s = 0; s < pv::kNumSlots; ++s)
        {
            auto& q = sp[s]; auto& d = p.slot[s];
            d.on = on (q.on); d.level = f (q.lv); d.decaySec = f (q.decay); d.lowMult = f (q.low); d.highMult = f (q.high);
            d.size = f (q.size); d.predelayMs = f (q.pre); d.buildMs = f (q.build); d.early = f (q.early); d.diffusion = f (q.diff);
            d.damping = f (q.damp); d.tone = f (q.tone); d.mod = f (q.mod); d.inWidth = f (q.width);
            for (int b = 0; b < 4; ++b) { d.eqF[b] = f (q.eqF[b]); d.eqG[b] = f (q.eqG[b]); }
            auto& c = d.comp;
            c.on = on (q.cpOn); c.duck = on (q.cpDuck); c.type = jlimit (0, 3, (int) f (q.cpType)); c.vintage = f (q.cpTone) > 0.5f;
            c.thr = f (q.cpThr); c.ratio = f (q.cpRatio); c.atkMs = f (q.cpAtk) * 1000.0f; c.relMs = f (q.cpRel) * 1000.0f;
            c.makeup = f (q.cpMakeup); c.mix = f (q.cpMix);
            d.matA = (int) f (q.matA); d.matB = (int) f (q.matB); d.matBlend = f (q.matMix); d.dirt = f (q.dirt); d.hiCutHz = f (q.hiCut); d.converter = jlimit (0, 3, (int) f (q.conv));
        }
        p.dlPanL = f (pDlPanL); p.dlPanR = f (pDlPanR); p.dlWidth = f (pDlWidth); p.delayAttackMs = f (pDlAtk);
        p.delayOn = on (pDlOn); p.pingPong = on (pDlPing); p.delayFb = f (pDlFb); p.delayMix = f (pDlMix);
        p.dlLowDb = f (pDlLow); p.dlMidDb = f (pDlMid); p.dlMidHz = f (pDlMidF); p.dlHighDb = f (pDlHigh);
        if (on (pDlSync))
        {
            double bpm = 120.0;
            if (audioThread)
                if (auto* ph = getPlayHead())
                    if (auto pos = ph->getPosition())
                        if (auto b = pos->getBpm())
                            bpm = *b;
            bpm = jlimit (20.0, 400.0, bpm);
            const int dl = jlimit (0, 15, (int) f (pDlDivL)), dr = jlimit (0, 15, (int) f (pDlDivR));
            p.delayMsL = jmin (2990.0f, (float) (kDivBeats[dl] * 60000.0 / bpm));
            p.delayMsR = jmin (2990.0f, (float) (kDivBeats[dr] * 60000.0 / bpm));
        }
        else { p.delayMsL = f (pDlTimeL); p.delayMsR = f (pDlTimeR); }
        p.trOn = on (pTrOn); p.trModel = jlimit (0, pv::kNumTransformers - 1, (int) f (pTrModel));
        p.trFreq = f (pTrFreq); p.trAmount = f (pTrAmt); p.outGainDb = f (pOut);
    }

    // ------------------------------------------------------------------ IR files
    // 'separate' = two files, one per input (L file = response to the left input). Otherwise one file
    // (mono or stereo), or a true-stereo pair found automatically (X.L.wav + X.R.wav).
    void requestIrLoad (const File& a, const File& b, bool resetSettings)
    {
        {
            const ScopedLock sl (irLock);
            pendingA = a; pendingB = b; pendingReset = resetSettings;
        }
        apvts.state.setProperty ("irA", a.getFullPathName(), nullptr);
        apvts.state.setProperty ("irB", b.getFullPathName(), nullptr);
        loadPending.store (true);
        notify();
    }
    void clearIr()
    {
        apvts.state.setProperty ("irA", "", nullptr); apvts.state.setProperty ("irB", "", nullptr);
        { const ScopedLock sl (irLock); pendingA = File(); pendingB = File(); }
        loadPending.store (true); notify();
    }
    File irFileA() const { const ScopedLock sl (irLock); return curA; }
    String irInfo() const { const ScopedLock sl (irLock); return irText; }
    int irLayout() const { return layout.load(); }
    float irRecordedRt() const { return recRt.load(); }

    // wave display data, rendered on the background thread
    pv::WaveData getWave (int slot) const { const ScopedLock sl (waveLock); return waves[jlimit (0, pv::kNumSlots - 1, slot)]; }
    std::atomic<int> waveVersion { 0 }, viewSlot { 0 }, presetSlotRequest { -1 };

    // ------------------------------------------------------------------ programs / presets
    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const String getName() const override { return "PlateVerb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 20.0; }
    int getNumPrograms() override { return kNumPresets + 1; }
    int getCurrentProgram() override { return currentPreset; }
    const String getProgramName (int i) override { return i == 0 ? String ("Default") : String (kPresets[jlimit (0, kNumPresets - 1, i - 1)].name); }
    void setParam (const String& id, float v) { if (auto* q = apvts.getParameter (id)) q->setValueNotifyingHost (q->convertTo0to1 (v)); }
    void resetParam (const String& id) { if (auto* q = apvts.getParameter (id)) q->setValueNotifyingHost (q->getDefaultValue()); }
    void setCurrentProgram (int i) override
    {
        i = jlimit (0, kNumPresets, i);
        currentPreset = i; userPresetName = {};
        if (i == 0)
        {
            for (auto* prmPtr : getParameters())
                if (auto* r = dynamic_cast<RangedAudioParameter*> (prmPtr)) r->setValueNotifyingHost (r->getDefaultValue());
            presetSlotRequest.store (0);
            return;
        }
        const auto& p = kPresets[i - 1];
        const int s = p.slot;
        for (int k = 0; k < pv::kNumSlots; ++k) { setParam (sid (k, "on"), k == s ? 1.0f : 0.0f); setParam (sid (k, "lv"), 1.0f); }
        for (auto* id : kSlotFloatIds) resetParam (sid (s, id));
        for (auto* id : kSlotOtherIds) if (String (id) != "on") resetParam (sid (s, id));
        setParam (sid (s, "decay"), p.decay); setParam (sid (s, "lowMult"), p.low); setParam (sid (s, "highMult"), p.high);
        setParam (sid (s, "size"), p.size); setParam (sid (s, "predelay"), p.pre); setParam (sid (s, "build"), p.build);
        setParam (sid (s, "early"), p.early); setParam (sid (s, "diff"), p.diff); setParam (sid (s, "damping"), p.damp);
        setParam (sid (s, "tone"), p.tone); setParam (sid (s, "mod"), p.mod); setParam (sid (s, "inWidth"), p.width);
        setParam (sid (s, "hiCut"), p.hiCut);
        setParam (sid (s, "conv"), (float) p.conv);
        const float eqF[] = { 250.0f, 400.0f, 2500.0f, 8000.0f };     // presets were fitted with the low shelf at 250 Hz
        for (int b = 0; b < 4; ++b) { setParam (sid (s, nid ("eqF", b)), eqF[b]); setParam (sid (s, nid ("eqG", b)), p.eq[b]); }
        presetSlotRequest.store (s);
    }
    void changeProgramName (int, const String&) override {}

    // user presets: the full state (all slots, delay, IR file paths) as an XML file
    bool saveUserPreset (const String& name)
    {
        auto dir = userPresetFolder(); dir.createDirectory();
        auto f = dir.getChildFile (File::createLegalFileName (name) + ".pvpreset");
        auto st = apvts.copyState();
        if (auto xml = st.createXml())
            if (xml->writeTo (f)) { userPresetName = f.getFileNameWithoutExtension(); currentPreset = 0; return true; }
        return false;
    }
    bool loadUserPreset (const File& f)
    {
        if (auto xml = parseXML (f))
            if (xml->hasTagName (apvts.state.getType()))
            {
                applyState (ValueTree::fromXml (*xml));
                userPresetName = f.getFileNameWithoutExtension(); currentPreset = 0;
                presetSlotRequest.store ((int) apvts.state.getProperty ("editSlot", 0));
                return true;
            }
        return false;
    }
    String userPresetName;

    void getStateInformation (MemoryBlock& dest) override
    {
        if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
    }
    void setStateInformation (const void* data, int size) override
    {
        if (auto xml = getXmlFromBinary (data, size))
            if (xml->hasTagName (apvts.state.getType()))
                applyState (ValueTree::fromXml (*xml));
    }
    void applyState (const ValueTree& t)
    {
        apvts.replaceState (t);
        const File a (apvts.state.getProperty ("irA").toString()), b (apvts.state.getProperty ("irB").toString());
        const bool haveA = apvts.state.getProperty ("irA").toString().isNotEmpty();
        {
            const ScopedLock sl (irLock);
            pendingA = haveA ? a : File(); pendingB = apvts.state.getProperty ("irB").toString().isNotEmpty() ? b : File(); pendingReset = false;
        }
        loadPending.store (true); notify();
    }

    APVTS apvts;
    int currentPreset = 0;
    std::atomic<float> compGr[pv::kNumSlots] {}, compIn[pv::kNumSlots] {}, dspLoad { 0.0f };

private:
    // ------------------------------------------------------------------ background thread: IR loading/rendering, wave display
    void run() override
    {
        pv::IRSettings lastSet; bool haveRendered = false; double lastRate = 0;
        std::vector<float> lastView; int lastViewSlot = -1; uint32 changedAt = 0; std::vector<float> pendingView;
        pv::Engine viewEngine;
        while (! threadShouldExit())
        {
            wait (40);
            const double rate = hostRate.load();
            if (rate <= 0) continue;
            if (rate != lastRate) { lastRate = rate; loadPending.store (true); viewEngine.prepare (rate); lastViewSlot = -1; }
            if (loadPending.exchange (false)) { loadFiles (rate); haveRendered = false; }
            { const SpinLock::ScopedLockType sl (swapLock); trash.clear(); }        // old convolvers die here, not on the audio thread
            // IR render when its settings change (after they have been still for 120 ms)
            const uint32 now = Time::getMillisecondCounter();
            if (layout.load() != pv::IrNone)
            {
                const auto set = irSettings();
                if (set != pendingSet) { pendingSet = set; irChangedAt = now; }
                if (! haveRendered || (set != lastSet && now - irChangedAt > 120))
                {
                    renderAndLoad (set, rate); lastSet = set; haveRendered = true;
                }
            }
            // algorithm impulse for the wave display
            const int v = viewSlot.load();
            if (v != pv::IR)
            {
                pv::Params p; fillParams (p, false);
                auto key = viewKey (p.slot[v]);
                if (key != pendingView) { pendingView = key; changedAt = now; }
                if (v != lastViewSlot || (key != lastView && now - changedAt > 100))
                {
                    renderView (viewEngine, p, v, rate); lastView = key; lastViewSlot = v;
                }
            }
            else lastViewSlot = -1;
        }
    }
    static std::vector<float> viewKey (const pv::SlotParams& s)
    {
        std::vector<float> k { s.decaySec, s.lowMult, s.highMult, s.size, s.predelayMs, s.buildMs, s.early, s.damping, s.tone, s.mod, s.inWidth, s.diffusion,
                               (float) s.matA, (float) s.matB, s.matBlend, s.dirt, s.hiCutHz, (float) s.converter };
        for (int b = 0; b < 4; ++b) { k.push_back (s.eqF[b]); k.push_back (s.eqG[b]); }
        return k;
    }
    void renderView (pv::Engine& e, const pv::Params& src, int slot, double rate)
    {
        pv::Params p; p.oversampling = 1; p.rvMix = 1.0f;
        for (int s = 0; s < pv::kNumSlots; ++s) p.slot[s].on = false;
        p.slot[slot] = src.slot[slot]; p.slot[slot].on = true; p.slot[slot].level = 1.0f; p.slot[slot].comp.on = false;
        e.prepare (rate);
        const float secs = jlimit (1.0f, 12.0f, p.slot[slot].decaySec * 1.6f + p.slot[slot].predelayMs * 0.001f + 0.3f);
        const int n = (int) (secs * rate);
        std::vector<float> L ((size_t) n, 0.0f), R ((size_t) n, 0.0f); L[0] = R[0] = 1.0f;
        std::vector<float> zL (4096, 0.0f), zR (4096, 0.0f);
        for (int k = 0; k < (int) (0.3 * rate / 4096) + 1; ++k) e.process (zL.data(), zR.data(), 4096, p);     // settle the mix/level smoothing
        L[0] = R[0] = 1.0f;
        for (int o = 0; o < n && ! threadShouldExit(); o += 512) e.process (L.data() + o, R.data() + o, jmin (512, n - o), p);
        auto w = pv::makeWave (L, R, rate, 0.5f); w.slot = slot;
        { const ScopedLock sl (waveLock); waves[slot] = std::move (w); }
        ++waveVersion;
    }
    pv::IRSettings irSettings()
    {
        auto& q = sp[pv::IR]; pv::IRSettings s;
        s.decaySec = q.decay->load(); s.lowMult = q.low->load(); s.highMult = q.high->load(); s.damping = q.damp->load();
        s.size = q.size->load(); s.buildMs = q.build->load(); s.early = q.early->load();
        s.startMs = pIrStart->load(); s.xoverMs = pIrXover->load(); s.fadeOutMs = pIrFade->load();
        s.crossFeed = pIrXfeed->load(); s.crossDelayMs = pIrXdelay->load(); s.asym = pIrAsym->load();
        s.reverse = pIrReverse->load() > 0.5f; s.stretchPitch = pIrPitch->load() > 0.5f;
        const auto me = pv::surfaceEffect ((int) q.matA->load(), (int) q.matB->load(), q.matMix->load(), 2.0f);
        s.lowMult *= me.lowMul; s.highMult *= me.highMul;
        return s;
    }
    bool readFile (const File& f, double rate, std::vector<float>& l, std::vector<float>& r, int& chans)
    {
        std::unique_ptr<AudioFormatReader> rd (formats.createReaderFor (f));
        if (rd == nullptr) return false;
        const int len = (int) jmin ((int64) (rd->sampleRate * 20.0), rd->lengthInSamples);    // up to 20 s
        if (len <= 0) return false;
        chans = (int) jmin (2u, rd->numChannels);
        AudioBuffer<float> b (2, len);
        rd->read (&b, 0, len, 0, true, chans > 1);
        l.assign (b.getReadPointer (0), b.getReadPointer (0) + len);
        r.assign (b.getReadPointer (chans > 1 ? 1 : 0), b.getReadPointer (chans > 1 ? 1 : 0) + len);
        const double ratio = rate / rd->sampleRate;
        l = pv::irdsp::resample (l, ratio); r = pv::irdsp::resample (r, ratio);
        return true;
    }
    void loadFiles (double rate)
    {
        File a, b; bool reset;
        { const ScopedLock sl (irLock); a = pendingA; b = pendingB; reset = pendingReset; }
        pv::TrueStereoIR ir; int lay = pv::IrNone; String info;
        if (a.existsAsFile())
        {
            std::vector<float> l1, r1; int c1 = 0;
            if (readFile (a, rate, l1, r1, c1))
            {
                if (b.existsAsFile())
                {
                    std::vector<float> l2, r2; int c2 = 0;
                    if (readFile (b, rate, l2, r2, c2))
                    {
                        const size_t n = jmax (l1.size(), l2.size());
                        l1.resize (n); r1.resize (n); l2.resize (n); r2.resize (n);
                        if (c1 == 1) std::fill (r1.begin(), r1.end(), 0.0f);      // mono per input: own side only
                        if (c2 == 1) std::fill (l2.begin(), l2.end(), 0.0f);
                        ir.aL = l1; ir.aR = r1; ir.bL = l2; ir.bR = r2; lay = pv::IrTrueStereo;
                        info = a.getFileName() + " + " + b.getFileName();
                    }
                }
                else
                {
                    const size_t n = l1.size();
                    ir.aL = l1; ir.bR = r1; ir.aR.assign (n, 0.0f); ir.bL.assign (n, 0.0f);
                    lay = c1 > 1 ? pv::IrStereo : pv::IrMono;
                    info = a.getFileName();
                }
            }
            else info = "Could not read " + a.getFileName();
        }
        pv::IRAnalysis an;
        if (lay != pv::IrNone)
        {
            // drop leading silence (up to the first sound at -60 dB of the peak), keep 1 ms before it
            float pk = 0; for (int c = 0; c < 4; ++c) for (float v : *ir.ch (c)) pk = jmax (pk, std::abs (v));
            int first = 0; const float thr = pk * 0.001f;
            for (int i = 0; i < ir.length(); ++i) { bool hit = false; for (int c = 0; c < 4; ++c) if (std::abs ((*ir.ch (c))[(size_t) i]) > thr) hit = true; if (hit) { first = i; break; } }
            first = jmax (0, first - (int) (0.001 * rate));
            if (first > 0) for (int c = 0; c < 4; ++c) ir.ch (c)->erase (ir.ch (c)->begin(), ir.ch (c)->begin() + first);
            an = pv::analyseIR (ir, rate);
        }
        {
            const ScopedLock sl (irLock);
            source = std::move (ir); analysis = an; curA = a; curB = b;
            irText = info;
        }
        layout.store (lay); recRt.store (an.rtMid);
        if (lay == pv::IrNone) { irReady.store (false); const ScopedLock sl (waveLock); waves[pv::IR] = {}; ++waveVersion; return; }
        if (reset)
        {
            // a freshly loaded IR starts as recorded
            const float rt = jlimit (0.05f, 20.0f, an.rtMid > 0.05f ? an.rtMid : 2.0f);
            std::weak_ptr<bool> w = alive;
            MessageManager::callAsync ([this, rt, w]
            {
                if (w.expired()) return;
                for (auto* id : { "decay", "lowMult", "highMult", "size", "predelay", "build", "early", "diff", "damping", "tone", "mod", "inWidth" })
                    resetParam (sid (pv::IR, id));
                setParam (sid (pv::IR, "decay"), rt);
                for (auto* id : { "irStart", "irFadeOut", "irXfeed", "irXdelay", "irAsym", "irReverse", "irPitch" }) resetParam (id);
            });
        }
    }
    void renderAndLoad (pv::IRSettings set, double rate)
    {
        pv::TrueStereoIR src; pv::IRAnalysis an;
        { const ScopedLock sl (irLock); src = source; an = analysis; }
        if (src.length() < 8) return;
        // a setting identical to the recording's RT is "as recorded" (no re-enveloping)
        if (std::abs (set.decaySec - an.rtMid) < 0.011f) set.decaySec = 0.0f;
        auto ir = pv::renderIR (src, an, set, rate);
        pv::trimIRTail (ir, rate);                         // the recorded noise floor below -80 dB only costs CPU
        const int n = ir.length();
        auto conv = std::make_unique<pv::PartitionedConvolver>();
        conv->build (ir);
        {
            // handed to the audio thread, which swaps it in with a short crossfade
            const SpinLock::ScopedLockType sl (swapLock);
            trash.clear();
            pending = std::move (conv);
        }
        irReady.store (true);
        // display: response to a centred source
        std::vector<float> L ((size_t) n), R ((size_t) n);
        for (int i = 0; i < n; ++i) { L[(size_t) i] = ir.aL[(size_t) i] + ir.bL[(size_t) i]; R[(size_t) i] = ir.aR[(size_t) i] + ir.bR[(size_t) i]; }
        auto w = pv::makeWave (L, R, rate, 0.5f); w.slot = pv::IR; w.xoverMs = pIrXover->load(); w.predelayIncluded = false;
        { const ScopedLock sl (waveLock); waves[pv::IR] = std::move (w); }
        ++waveVersion;
    }

    struct SlotPtrs {
        std::atomic<float> *on = nullptr, *lv = nullptr, *decay = nullptr, *low = nullptr, *high = nullptr, *size = nullptr, *pre = nullptr,
            *build = nullptr, *early = nullptr, *diff = nullptr, *damp = nullptr, *tone = nullptr, *mod = nullptr, *width = nullptr,
            *eqF[4] = {}, *eqG[4] = {}, *matA = nullptr, *matB = nullptr, *matMix = nullptr, *dirt = nullptr, *hiCut = nullptr, *conv = nullptr, *cpOn = nullptr, *cpDuck = nullptr, *cpType = nullptr, *cpTone = nullptr, *cpThr = nullptr,
            *cpRatio = nullptr, *cpAtk = nullptr, *cpRel = nullptr, *cpMakeup = nullptr, *cpMix = nullptr;
    } sp[pv::kNumSlots];

    pv::Engine engine;
    pv::Params prm;
    std::unique_ptr<pv::PartitionedConvolver> cur, next, pending;
    std::vector<std::unique_ptr<pv::PartitionedConvolver>> trash;
    int xfade = 0;
    std::vector<float> inL, inR, irL, irR, nxL, nxR;
    int blockMax = 512; bool irWasOn = false;
    std::atomic<bool> irReady { false }, loadPending { false };
    std::atomic<double> hostRate { 0.0 };
    std::atomic<int> layout { pv::IrNone };
    std::atomic<float> recRt { 0.0f };
    AudioFormatManager formats;
    mutable CriticalSection irLock, waveLock;
    File pendingA, pendingB, curA, curB; bool pendingReset = false; String irText;
    pv::TrueStereoIR source; pv::IRAnalysis analysis;
    pv::IRSettings pendingSet; uint32 irChangedAt = 0;
    pv::WaveData waves[pv::kNumSlots];
    SpinLock swapLock;
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    std::atomic<float> *pOs = nullptr, *pMix = nullptr, *pDlOn = nullptr, *pDlSync = nullptr, *pDlPing = nullptr,
        *pDlTimeL = nullptr, *pDlTimeR = nullptr, *pDlDivL = nullptr, *pDlDivR = nullptr, *pDlFb = nullptr, *pDlMix = nullptr,
        *pDlLow = nullptr, *pDlMid = nullptr, *pDlMidF = nullptr, *pDlHigh = nullptr, *pTrOn = nullptr, *pTrModel = nullptr,
        *pTrFreq = nullptr, *pTrAmt = nullptr, *pOut = nullptr, *pDlPanL = nullptr, *pDlPanR = nullptr, *pDlWidth = nullptr, *pDlAtk = nullptr,
        *pIrStart = nullptr, *pIrXover = nullptr, *pIrFade = nullptr, *pIrXfeed = nullptr, *pIrXdelay = nullptr, *pIrAsym = nullptr,
        *pIrReverse = nullptr, *pIrPitch = nullptr;
    double sampleRateHz = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlateVerbProcessor)
};

#include "PluginEditor.h"

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PlateVerbProcessor(); }
