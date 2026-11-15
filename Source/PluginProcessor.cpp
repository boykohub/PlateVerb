#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <vector>

struct Delay {
    std::vector<float> buf; int w = 0;
    void setMax (int n) { buf.assign ((size_t) n + 4, 0.0f); w = 0; }
    void push (float x) { buf[(size_t) w] = x; if (++w >= (int) buf.size()) w = 0; }
    float tap (float d) const {
        float r = (float) w - 1.0f - d;
        while (r < 0) r += (float) buf.size();
        int i = (int) r; float f = r - (float) i;
        int j = i + 1; if (j >= (int) buf.size()) j = 0;
        return buf[(size_t) i] + (buf[(size_t) j] - buf[(size_t) i]) * f;
    }
};

static float ap (Delay& d, float len, float g, float x) {
    float z = d.tap (len - 1.0f);
    float v = x - g * z;
    d.push (v);
    return z + g * v;
}

class PlateVerb : public juce::AudioProcessor {
public:
    PlateVerb()
        : AudioProcessor (BusesProperties()
              .withInput ("In", juce::AudioChannelSet::stereo(), true)
              .withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, "P", layout()) {}

    static juce::AudioProcessorValueTreeState::ParameterLayout layout() {
        using P = juce::AudioParameterFloat;
        juce::AudioProcessorValueTreeState::ParameterLayout l;
        l.add (std::make_unique<P> (juce::ParameterID { "decay", 1 }, "Decay", 0.0f, 0.98f, 0.75f));
        l.add (std::make_unique<P> (juce::ParameterID { "predelay", 1 }, "Pre-Delay ms", 0.0f, 200.0f, 10.0f));
        l.add (std::make_unique<P> (juce::ParameterID { "damping", 1 }, "Damping", 0.0f, 0.95f, 0.35f));
        l.add (std::make_unique<P> (juce::ParameterID { "tone", 1 }, "Input Tone", 0.1f, 1.0f, 0.75f));
        l.add (std::make_unique<P> (juce::ParameterID { "mod", 1 }, "Modulation", 0.0f, 1.0f, 0.5f));
        l.add (std::make_unique<P> (juce::ParameterID { "mix", 1 }, "Mix", 0.0f, 1.0f, 0.3f));
        return l;
    }

    void prepareToPlay (double sampleRate, int) override {
        sr = sampleRate; s = (float) (sr / 29761.0);
        auto sz = [this] (double n) { return (int) std::ceil (n * s) + 64; };
        pre.setMax ((int) (0.25 * sr));
        d1.setMax (sz (142)); d2.setMax (sz (107)); d3.setMax (sz (379)); d4.setMax (sz (277));
        apL1.setMax (sz (672 + 32)); dL1.setMax (sz (4453)); apL2.setMax (sz (1800)); dL2.setMax (sz (3720));
        apR1.setMax (sz (908 + 32)); dR1.setMax (sz (4217)); apR2.setMax (sz (2656)); dR2.setMax (sz (3163));
        bw = lpL = lpR = tankL = tankR = 0.0f; phase = 0.0f;
        inc = (float) (juce::MathConstants<double>::twoPi * 0.9 / sr);
    }
    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& b) const override {
        return b.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override {
        juce::ScopedNoDenormals nd;
        auto* L = buffer.getWritePointer (0);
        auto* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : L;
        const float decay = apvts.getRawParameterValue ("decay")->load();
        const float preS  = apvts.getRawParameterValue ("predelay")->load() * 0.001f * (float) sr;
        const float damp  = apvts.getRawParameterValue ("damping")->load();
        const float tone  = apvts.getRawParameterValue ("tone")->load();
        const float exc   = apvts.getRawParameterValue ("mod")->load() * 16.0f * s;
        const float mix   = apvts.getRawParameterValue ("mix")->load();
        auto T = [this] (const Delay& d, float n) { return d.tap (n * s); };

        for (int n = 0; n < buffer.getNumSamples(); ++n) {
            const float inL = L[n], inR = R[n];
            float x = 0.5f * (inL + inR);
            pre.push (x); x = pre.tap (preS);
            bw += tone * (x - bw); x = bw;
            x = ap (d1, 142 * s, 0.75f, x);  x = ap (d2, 107 * s, 0.75f, x);
            x = ap (d3, 379 * s, 0.625f, x); x = ap (d4, 277 * s, 0.625f, x);

            const float m1 = std::sin (phase), m2 = std::cos (phase);
            phase += inc; if (phase > juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi;

            float a = ap (apL1, 672 * s + exc * m1, -0.7f, x + decay * tankR);
            dL1.push (a); lpL += (1.0f - damp) * (dL1.tap (4453 * s - 1) - lpL);
            float b = ap (apL2, 1800 * s, 0.5f, lpL * decay);
            dL2.push (b); float newL = dL2.tap (3720 * s - 1);

            float c = ap (apR1, 908 * s + exc * m2, -0.7f, x + decay * tankL);
            dR1.push (c); lpR += (1.0f - damp) * (dR1.tap (4217 * s - 1) - lpR);
            float e = ap (apR2, 2656 * s, 0.5f, lpR * decay);
            dR2.push (e); float newR = dR2.tap (3163 * s - 1);
            tankL = newL; tankR = newR;

            float yL = T (dR1, 266) + T (dR1, 2974) - T (apR2, 1913) + T (dR2, 1996)
                     - T (dL1, 1990) - T (apL2, 187) - T (dL2, 1066);
            float yR = T (dL1, 353) + T (dL1, 3627) - T (apL2, 1228) + T (dL2, 2673)
                     - T (dR1, 2111) - T (apR2, 335) - T (dR2, 121);
            yL *= 0.6f; yR *= 0.6f;

            if (R != L) { L[n] = inL * (1 - mix) + yL * mix; R[n] = inR * (1 - mix) + yR * mix; }
            else L[n] = inL * (1 - mix) + 0.5f * (yL + yR) * mix;
        }
    }

    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor (*this); }
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "PlateVerb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& dest) override {
        if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
    }
    void setStateInformation (const void* data, int size) override {
        if (auto xml = getXmlFromBinary (data, size)) apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

private:
    juce::AudioProcessorValueTreeState apvts;
    Delay pre, d1, d2, d3, d4, apL1, dL1, apL2, dL2, apR1, dR1, apR2, dR2;
    double sr = 44100.0; float s = 1.0f;
    float bw = 0, lpL = 0, lpR = 0, tankL = 0, tankR = 0, phase = 0, inc = 0;
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PlateVerb(); }
