// Offline sanity test for the SmoothVT-7 compressor core (no JUCE needed)
#include "../Source/DSP.h"
#include <cstdio>
#include <vector>
using namespace smooth;

static float runSine (CoreSettings s, float levelDb, float freq, double fs, float* grOut, float* thdOut = nullptr)
{
    CompressorCore core; core.prepare (fs); core.update (s);
    const int n = (int) (fs * 2.0);
    const float a = dbToGain (levelDb);
    double sumOut = 0, sumIn = 0; int cnt = 0;
    std::vector<float> tail;
    for (int i = 0; i < n; ++i)
    {
        float v = a * std::sin (2.0f * kPi * freq * (float) i / (float) fs);
        float fr[2] = { v, v };
        core.processFrame (fr, 2);
        if (i > n / 2) { sumOut += fr[0] * fr[0]; sumIn += v * v; ++cnt; tail.push_back (fr[0]); }
    }
    *grOut = core.getGainReductionDb (0);
    if (thdOut)
    {   // 2nd harmonic level relative to fundamental (single-bin DFT)
        auto bin = [&] (float f) { double re = 0, im = 0; for (size_t k = 0; k < tail.size(); ++k) { double ph = 2 * 3.14159265358979 * f * (double) k / fs; re += tail[k] * std::cos (ph); im += tail[k] * std::sin (ph);} return std::sqrt (re*re+im*im); };
        *thdOut = gainToDb ((float) (bin (2 * freq) / bin (freq)));
    }
    return gainToDb ((float) std::sqrt (sumOut / sumIn));
}

int main()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what); if (!ok) ++fails; };
    const double fs = 96000.0;
    CoreSettings s; s.ch[0].makeupDb = s.ch[1].makeupDb = 0; s.ch[0].thresholdDb = s.ch[1].thresholdDb = -18;

    float gr, thd;
    float g = runSine (s, -40, 1000, fs, &gr);
    std::printf ("  -40 dBFS in: gain %.2f dB, GR %.2f dB\n", g, gr);
    check (std::abs (g) < 0.5f && gr < 0.05f, "below threshold: unity gain, no reduction");

    for (float t : { 0.0f, 0.5f, 1.0f })
    {
        s.ch[0].tube = s.ch[1].tube = t;
        g = runSine (s, -6, 1000, fs, &gr, &thd);
        std::printf ("  tube=%.1f  -6 dBFS in: gain %.2f dB, GR %.2f dB, H2 %.1f dB\n", t, g, gr, thd);
    }
    s.ch[0].tube = s.ch[1].tube = 0.0f; float grHard; runSine (s, -6, 1000, fs, &grHard, &thd); float h2Hard = thd;
    s.ch[0].tube = s.ch[1].tube = 1.0f; float grSoft; runSine (s, -6, 1000, fs, &grSoft, &thd); float h2Soft = thd;
    check (grHard > grSoft + 2.0f, "Harder compresses more than Softer");
    check (h2Hard > h2Soft, "Harder adds more 2nd harmonic than Softer");
    check (grHard > 5.0f && grHard < 15.0f, "Harder: sensible reduction for 12 dB over threshold");

    s.compIn = false; g = runSine (s, -6, 1000, fs, &gr);
    check (gr == 0.0f && std::abs (g) < 1.5f, "Comp OUT: no gain reduction, tube colour only");
    s.compIn = true;

    s.ch[0].makeupDb = s.ch[1].makeupDb = 6; s.ch[0].tube = s.ch[1].tube = 1.0f;
    g = runSine (s, -40, 1000, fs, &gr);
    check (std::abs (g - 6.0f) < 0.6f, "make-up gain applied");

    // Link HPF: a 40 Hz tone should trigger far less reduction than in plain Link
    s.ch[0].makeupDb = s.ch[1].makeupDb = 0; s.scHpfIdx = 5; // 200 Hz
    s.stereo = StereoMode::link;    float grLink; runSine (s, -6, 40, fs, &grLink);
    s.stereo = StereoMode::linkHpf; float grHpf;  runSine (s, -6, 40, fs, &grHpf);
    std::printf ("  40 Hz tone: GR link %.2f dB, link HPF %.2f dB\n", grLink, grHpf);
    check (grHpf < grLink * 0.5f, "Link HPF keeps bass from pumping the compressor");

    // Linking: hot left channel should pull right channel down in Link, not in Dual
    {
        for (auto mode : { StereoMode::dualMono, StereoMode::link })
        {
            CoreSettings t = s; t.stereo = mode; CompressorCore core; core.prepare (fs); core.update (t);
            for (int i = 0; i < (int) fs; ++i) { float v = 0.5f * std::sin (2.0f * kPi * 1000.0f * i / (float) fs); float fr[2] = { v, 0.01f * v }; core.processFrame (fr, 2); }
            std::printf ("  %s: GR L %.2f R %.2f\n", mode == StereoMode::link ? "link" : "dual", core.getGainReductionDb (0), core.getGainReductionDb (1));
            if (mode == StereoMode::link) check (std::abs (core.getGainReductionDb (0) - core.getGainReductionDb (1)) < 0.01f, "Link: both channels share reduction");
            else check (core.getGainReductionDb (1) < 0.05f, "Dual mono: quiet channel left alone");
        }
    }

    // Auto release recovers fast after a short burst, slower after a long one
    {
        auto recover = [&] (double burstSec) {
            CoreSettings t = s; t.stereo = StereoMode::link; t.ch[0].releaseIdx = t.ch[1].releaseIdx = 5; t.ch[0].attackIdx = t.ch[1].attackIdx = 2;
            CompressorCore core; core.prepare (fs); core.update (t);
            int b = (int) (burstSec * fs);
            for (int i = 0; i < b; ++i) { float v = 0.5f * std::sin (2.0f * kPi * 1000.0f * i / (float) fs); float fr[2] = { v, v }; core.processFrame (fr, 2); }
            float g0 = core.getGainReductionDb (0);
            for (int i = 0; i < (int) (0.25 * fs); ++i) { float fr[2] = { 0, 0 }; core.processFrame (fr, 2); }
            return core.getGainReductionDb (0) / g0; };
        float shortB = recover (0.05), longB = recover (3.0);
        std::printf ("  auto release: remaining after 250 ms  short burst %.0f%%  long burst %.0f%%\n", shortB * 100, longB * 100);
        check (longB > shortB, "Auto release is program dependent");
    }

    std::printf (fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails;
}
