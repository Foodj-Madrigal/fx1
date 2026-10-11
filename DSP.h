#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include <algorithm>

// Moteur audio des 4 effets. Aucun accès à l'interface : testable seul.
namespace fx
{
constexpr float kTwoPi = 6.28318530718f;

struct OnePoleLP
{
    void setCutoff (float fc, double sr) { a = 1.0f - std::exp (-kTwoPi * fc / (float) sr); }
    float process (float x) { z += a * (x - z); return z; }
    void reset() { z = 0.0f; }
    float a = 0.1f, z = 0.0f;
};

//==============================================================================
// WAH : filtre passe-bande résonant. Position "pedal" (automatable dans Cubase)
// + balayage automatique par LFO.
class Wah
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        juce::dsp::ProcessSpec spec { sr, 512, 2 };
        filter.prepare (spec);
        filter.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        filter.reset();
        pedalSm.reset (sr, 0.03);
        pedalSm.setCurrentAndTargetValue (0.5f);
        phase = 0.0f;
    }

    void process (juce::AudioBuffer<float>& buf, float pedal, float autoAmt,
                  float rate, float q, float mix)
    {
        const int n  = buf.getNumSamples();
        const int nc = std::min (buf.getNumChannels(), 2);
        pedalSm.setTargetValue (pedal);
        filter.setResonance (q);
        const float inc    = rate / (float) sampleRate;
        const float makeup = 2.0f / q;   // le pic du passe-bande vaut Q -> gain ~ +6 dB au pic

        for (int i = 0; i < n; ++i)
        {
            const float p   = pedalSm.getNextValue();
            const float lfo = std::sin (kTwoPi * phase);
            phase += inc;
            if (phase >= 1.0f) phase -= 1.0f;

            const float pos = juce::jlimit (0.0f, 1.0f, p + autoAmt * 0.5f * lfo);
            filter.setCutoffFrequency (400.0f * std::pow (6.25f, pos));   // 400 Hz -> 2.5 kHz

            for (int c = 0; c < nc; ++c)
            {
                float* d = buf.getWritePointer (c);
                const float x   = d[i];
                const float wet = filter.processSample ((size_t) c, x) * makeup;
                d[i] = x + mix * (wet - x);
            }
        }
    }

private:
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::SmoothedValue<float> pedalSm;
    double sampleRate = 44100.0;
    float phase = 0.0f;
};

//==============================================================================
// HARMONIZER : 2 voix de transposition (décalage de hauteur à lignes à retard
// modulées, avec fenêtrage de Hann). Le son original est conservé (Dry).
class Harmonizer
{
public:
    void prepare (double sr)
    {
        windowSamples = (float) (0.045 * sr);
        int sz = 1;
        while (sz < (int) (0.25 * sr)) sz <<= 1;
        mask = sz - 1;
        for (auto& c : chan)
        {
            c.buf.assign ((size_t) sz, 0.0f);
            c.w = 0;
            c.phase[0] = 0.0f;
            c.phase[1] = 0.37f;
        }
    }

    void process (juce::AudioBuffer<float>& buf, float semi1, float lvl1,
                  float semi2, float lvl2, float dry)
    {
        const int n  = buf.getNumSamples();
        const int nc = std::min (buf.getNumChannels(), 2);
        const float ratio[2] = { std::pow (2.0f, semi1 / 12.0f), std::pow (2.0f, semi2 / 12.0f) };
        const float lvl[2]   = { lvl1 * 0.8f, lvl2 * 0.8f };
        const float rate[2]  = { (1.0f - ratio[0]) / windowSamples, (1.0f - ratio[1]) / windowSamples };
        const int size = mask + 1;

        for (int c = 0; c < nc; ++c)
        {
            auto& ch = chan[(size_t) c];
            float* d = buf.getWritePointer (c);

            for (int i = 0; i < n; ++i)
            {
                const float x = d[i];
                ch.buf[(size_t) ch.w] = x;
                float out = dry * x;

                for (int v = 0; v < 2; ++v)
                {
                    ch.phase[v] += rate[v];
                    if (ch.phase[v] >= 1.0f) ch.phase[v] -= 1.0f;
                    else if (ch.phase[v] < 0.0f) ch.phase[v] += 1.0f;

                    float acc = 0.0f;
                    for (int k = 0; k < 2; ++k)
                    {
                        float p = ch.phase[v] + 0.5f * (float) k;
                        if (p >= 1.0f) p -= 1.0f;

                        const float delay = p * windowSamples + 1.0f;
                        const float pos   = (float) (ch.w + size) - delay;
                        const int   i0    = (int) pos;
                        const float fr    = pos - (float) i0;
                        const float a = ch.buf[(size_t) (i0 & mask)];
                        const float b = ch.buf[(size_t) ((i0 + 1) & mask)];
                        const float s = a + fr * (b - a);
                        const float win = 0.5f - 0.5f * std::cos (kTwoPi * p);
                        acc += s * win;
                    }
                    out += lvl[v] * acc;
                }

                ch.w = (ch.w + 1) & mask;
                d[i] = out;
            }
        }
    }

private:
    struct Chan { std::vector<float> buf; int w = 0; float phase[2] = { 0.0f, 0.37f }; };
    Chan chan[2];
    int mask = 0;
    float windowSamples = 2000.0f;
};

//==============================================================================
// DELAY : stéréo, feedback filtré (Damp), ping-pong, synchro tempo (gérée par le processeur).
class StereoDelay
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        size = (int) (2.6 * sr) + 8;
        for (auto& b : buf) b.assign ((size_t) size, 0.0f);
        w = 0;
        timeSm.reset (sr, 0.12);
        timeSm.setCurrentAndTargetValue ((float) (0.3 * sr));
        firstBlock = true;
        for (auto& l : lp) l.reset();
    }

    void process (juce::AudioBuffer<float>& b, float timeSamples, float fb,
                  float toneHz, bool pingPong, float mix)
    {
        const int n  = b.getNumSamples();
        const int nc = std::min (b.getNumChannels(), 2);
        const float target = juce::jlimit (16.0f, (float) sampleRate * 2.5f, timeSamples);
        if (firstBlock) { timeSm.setCurrentAndTargetValue (target); firstBlock = false; }   // pas de glissement au démarrage
        else            timeSm.setTargetValue (target);
        for (auto& l : lp) l.setCutoff (toneHz, sampleRate);

        float* l = b.getWritePointer (0);
        float* r = nc > 1 ? b.getWritePointer (1) : l;

        for (int i = 0; i < n; ++i)
        {
            const float t   = timeSm.getNextValue();
            const float pos = (float) (w + size) - t;
            const int   i0  = (int) pos;
            const float fr  = pos - (float) i0;
            const int   a   = i0 % size;
            const int   c   = (i0 + 1) % size;

            const float outL = buf[0][(size_t) a] + fr * (buf[0][(size_t) c] - buf[0][(size_t) a]);
            const float outR = buf[1][(size_t) a] + fr * (buf[1][(size_t) c] - buf[1][(size_t) a]);
            const float fL = lp[0].process (outL);
            const float fR = lp[1].process (outR);

            const float inL = l[i], inR = r[i];
            float wL, wR;
            if (pingPong) { const float x = 0.5f * (inL + inR); wL = x + fb * fR; wR = fb * fL; }
            else          { wL = inL + fb * fL; wR = inR + fb * fR; }

            buf[0][(size_t) w] = wL;
            buf[1][(size_t) w] = wR;
            if (++w >= size) w = 0;

            l[i] = inL + mix * outL;
            if (nc > 1) r[i] = inR + mix * outR;
        }
    }

private:
    std::vector<float> buf[2];
    OnePoleLP lp[2];
    juce::SmoothedValue<float> timeSm;
    double sampleRate = 44100.0;
    int size = 1, w = 0;
    bool firstBlock = true;
};

//==============================================================================
// SPRING REVERB : 2 "ressorts" par canal. Chaque ressort = ligne à retard bouclée
// avec une cascade d'allpass (dispersion -> le "boing") et un filtre d'amortissement.
class SpringReverb
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        static const float lenMs[2][2] = { { 23.7f, 36.1f }, { 25.9f, 39.3f } };
        for (int c = 0; c < 2; ++c)
        {
            for (int k = 0; k < 2; ++k)
                lines[c][k].prepare ((int) (lenMs[c][k] * sr / 1000.0));
            hp[c].reset();
            hp[c].setCutoff (150.0f, sr);
        }
    }

    void process (juce::AudioBuffer<float>& buf, float decay, float toneHz, float boing, float mix)
    {
        const int n  = buf.getNumSamples();
        const int nc = std::min (buf.getNumChannels(), 2);
        const float fb     = 0.35f + 0.60f * decay;
        const float a      = -(0.45f + 0.40f * boing);
        const float inGain = 0.6f * std::sqrt (1.0f - fb);
        for (auto& row : lines) for (auto& ln : row) ln.damp.setCutoff (toneHz, sampleRate);

        for (int c = 0; c < nc; ++c)
        {
            float* d = buf.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                const float x  = d[i];
                const float xi = (x - hp[c].process (x)) * inGain;
                const float y  = lines[c][0].process (xi, fb, a) + lines[c][1].process (xi, fb, a);
                d[i] = x + mix * 2.0f * y;
            }
        }
    }

private:
    struct Line
    {
        static constexpr int N = 16;
        void prepare (int len_)
        {
            len = std::max (len_, 8);
            buf.assign ((size_t) len, 0.0f);
            w = 0;
            for (int k = 0; k < N; ++k) x1[k] = y1[k] = 0.0f;
            damp.reset();
        }
        float process (float in, float fb, float a)
        {
            float y = buf[(size_t) w];
            y = damp.process (y);
            for (int k = 0; k < N; ++k)                // allpass 1er ordre : y = a*x + x1 - a*y1
            {
                const float o = a * y + x1[k] - a * y1[k];
                x1[k] = y; y1[k] = o; y = o;
            }
            buf[(size_t) w] = in + fb * y;
            if (++w >= len) w = 0;
            return y;
        }
        std::vector<float> buf;
        float x1[N] {}, y1[N] {};
        OnePoleLP damp;
        int len = 8, w = 0;
    };

    Line lines[2][2];
    OnePoleLP hp[2];
    double sampleRate = 44100.0;
};

} // namespace fx
