// Test hors-ligne du moteur audio (Source/DSP.h). Compiler avec -DBUILD_DSP_TEST=ON
#include "../Source/DSP.h"
#include <cstdio>

using namespace fx;
static const double SR = 48000.0;

static juce::AudioBuffer<float> sine (float f, int n, float amp = 0.5f)
{
    juce::AudioBuffer<float> b (2, n);
    for (int i = 0; i < n; ++i)
        for (int c = 0; c < 2; ++c)
            b.setSample (c, i, amp * std::sin (kTwoPi * f * (float) i / (float) SR));
    return b;
}

template <typename Fn>
static void inBlocks (juce::AudioBuffer<float>& b, Fn fn)
{
    const int blk = 512;
    for (int s = 0; s < b.getNumSamples(); s += blk)
    {
        const int len = std::min (blk, b.getNumSamples() - s);
        juce::AudioBuffer<float> v (b.getArrayOfWritePointers(), b.getNumChannels(), s, len);
        fn (v);
    }
}

static float peak (const juce::AudioBuffer<float>& b, int from, int to)
{
    float m = 0.0f;
    for (int i = from; i < to; ++i) m = std::max (m, std::abs (b.getSample (0, i)));
    return m;
}

static float rms (const juce::AudioBuffer<float>& b, int from, int to)
{
    double s = 0.0;
    for (int i = from; i < to; ++i) s += b.getSample (0, i) * b.getSample (0, i);
    return (float) std::sqrt (s / (to - from));
}

static float measureFreq (const juce::AudioBuffer<float>& b, int from, int to)
{
    int crossings = 0;
    for (int i = from + 1; i < to; ++i)
        if (b.getSample (0, i - 1) <= 0.0f && b.getSample (0, i) > 0.0f) ++crossings;
    return (float) crossings * (float) SR / (float) (to - from);
}

int main()
{
    // ---- Harmonizer : une seule voix active, dry = 0
    for (int semi : { 7, 4, -12, 12 })
    {
        Harmonizer h; h.prepare (SR);
        auto b = sine (1000.0f, (int) (3 * SR));
        inBlocks (b, [&] (auto& v) { h.process (v, (float) semi, 1.0f, 0.0f, 0.0f, 0.0f); });
        std::printf ("HARM %+3d st : mesure %.1f Hz, attendu %.1f Hz\n", semi,
                     measureFreq (b, (int) (2 * SR), (int) (3 * SR)),
                     1000.0f * std::pow (2.0f, (float) semi / 12.0f));
    }

    // ---- Wah : pedal 0.5 -> centre ~1 kHz
    for (float f : { 100.0f, 1000.0f, 5000.0f })
    {
        Wah w; w.prepare (SR);
        auto b = sine (f, (int) SR);
        inBlocks (b, [&] (auto& v) { w.process (v, 0.5f, 0.0f, 1.0f, 4.0f, 1.0f); });
        std::printf ("WAH  %6.0f Hz : gain %.2f\n", f, peak (b, (int) (0.5 * SR), (int) SR) / 0.5f);
    }
    {   // wah auto : le niveau d'un sinus 1 kHz doit varier dans le temps
        Wah w; w.prepare (SR);
        auto b = sine (1000.0f, (int) (2 * SR));
        inBlocks (b, [&] (auto& v) { w.process (v, 0.5f, 1.0f, 2.0f, 4.0f, 1.0f); });
        float lo = 1e9f, hi = 0.0f;
        for (int s = (int) SR; s < (int) (2 * SR); s += 1200)
        { float p = peak (b, s, s + 1200); lo = std::min (lo, p); hi = std::max (hi, p); }
        std::printf ("WAH auto : niveau min %.3f / max %.3f (doit varier)\n", lo, hi);
    }

    // ---- Delay : impulsion, 100 ms, feedback 50 %
    {
        StereoDelay d; d.prepare (SR);
        juce::AudioBuffer<float> b (2, (int) SR);
        b.clear(); b.setSample (0, 0, 1.0f); b.setSample (1, 0, 1.0f);
        inBlocks (b, [&] (auto& v) { d.process (v, (float) (0.1 * SR), 0.5f, 20000.0f, false, 1.0f); });
        std::printf ("DELAY pics :");
        for (int i = 1; i < b.getNumSamples() - 1; ++i)
            if (b.getSample (0, i) > 0.1f && b.getSample (0, i) >= b.getSample (0, i - 1)
                                          && b.getSample (0, i) >= b.getSample (0, i + 1))
                std::printf (" [%d: %.2f]", i, b.getSample (0, i));
        std::printf ("  (attendu ~4800:1.0 9600:0.5 14400:0.25)\n");
    }

    // ---- Spring : rafale de bruit puis silence, stabilité et décroissance
    for (float decay : { 0.5f, 1.0f })
    {
        SpringReverb s; s.prepare (SR);
        juce::AudioBuffer<float> b (2, (int) (12 * SR));
        b.clear();
        juce::Random rnd (1);
        for (int i = 0; i < (int) (0.1 * SR); ++i)
            for (int c = 0; c < 2; ++c) b.setSample (c, i, 0.5f * (rnd.nextFloat() * 2.0f - 1.0f));
        inBlocks (b, [&] (auto& v) { s.process (v, decay, 5000.0f, 1.0f, 1.0f); });
        bool finite = true; float mx = 0.0f;
        for (int i = 0; i < b.getNumSamples(); ++i)
        { float x = b.getSample (0, i); if (! std::isfinite (x)) finite = false; mx = std::max (mx, std::abs (x)); }
        std::printf ("SPRING decay %.1f : fini=%d max=%.2f rms[0.2-0.7s]=%.4f rms[1-1.5s]=%.4f rms[10-12s]=%.6f\n",
                     decay, (int) finite, mx, rms (b, (int) (0.2 * SR), (int) (0.7 * SR)),
                     rms (b, (int) (1 * SR), (int) (1.5 * SR)), rms (b, (int) (10 * SR), (int) (12 * SR)));
    }
    return 0;
}
