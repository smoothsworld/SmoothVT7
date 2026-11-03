// SmoothVT-7 — DSP core (no JUCE dependency, so it can be unit-tested on its own)
// Smooth Life
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace smooth
{
constexpr float kPi = 3.14159265358979f;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

//==============================================================================
// RBJ biquad, transposed direct form II. Coefficients are computed in place,
// so changing them from the audio thread never allocates.
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;
    bool  active = false;

    void reset() noexcept { z1 = z2 = 0.0f; }
    void setBypass() noexcept { active = false; }

    void setHighPass (double fs, double f, double q = 0.7071) noexcept { design (fs, f, q, true); }
    void setLowPass  (double fs, double f, double q = 0.7071) noexcept { design (fs, f, q, false); }

    float process (float x) noexcept
    {
        if (! active) return x;
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

private:
    void design (double fs, double f, double q, bool highPass) noexcept
    {
        f = std::min (f, fs * 0.45);
        const double w0 = 2.0 * 3.141592653589793 * f / fs;
        const double cs = std::cos (w0), sn = std::sin (w0);
        const double alpha = sn / (2.0 * q);
        const double a0 = 1.0 + alpha;
        double nb0, nb1, nb2;
        if (highPass) { nb0 = (1.0 + cs) * 0.5; nb1 = -(1.0 + cs); nb2 = nb0; }
        else          { nb0 = (1.0 - cs) * 0.5; nb1 =  (1.0 - cs); nb2 = nb0; }
        b0 = (float) (nb0 / a0); b1 = (float) (nb1 / a0); b2 = (float) (nb2 / a0);
        a1 = (float) (-2.0 * cs / a0); a2 = (float) ((1.0 - alpha) / a0);
        active = true;
    }
};

//==============================================================================
// Simple DC blocker (removes the offset the asymmetric tube stage creates)
struct DcBlocker
{
    float x1 = 0, y1 = 0, r = 0.9995f;
    void prepare (double fs) noexcept { r = (float) std::exp (-2.0 * 3.141592653589793 * 8.0 / fs); reset(); }
    void reset() noexcept { x1 = y1 = 0.0f; }
    float process (float x) noexcept { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
};

//==============================================================================
// Class-A triode stage: asymmetric soft saturation (mostly 2nd harmonic at low
// drive, rounder clipping when pushed). Small-signal gain is normalised to 1 so
// the Tube control changes colour, not level.
inline float triode (float x, float drive, float bias) noexcept
{
    const float tb   = std::tanh (drive * bias);
    const float norm = drive * (1.0f - tb * tb);           // slope at the operating point
    return (std::tanh (drive * (x + bias)) - tb) / norm;
}

// Transformer: gentle odd-order rounding of peaks
inline float transformer (float x, float k) noexcept
{
    return std::tanh (k * x) / k;
}

//==============================================================================
// Choice tables shared by the processor and the faceplate scales
namespace tables
{
    constexpr std::array<float, 6> attackMs  { 0.5f, 1.0f, 3.0f, 10.0f, 30.0f, 80.0f };
    constexpr std::array<float, 6> releaseMs { 50.0f, 100.0f, 200.0f, 400.0f, 800.0f, -1.0f }; // -1 = Auto
    constexpr std::array<float, 6> scHpfHz   { 0.0f, 60.0f, 90.0f, 120.0f, 150.0f, 200.0f };   // 0 = Off
    constexpr std::array<float, 7> lowCutHz  { 0.0f, 20.0f, 30.0f, 40.0f, 60.0f, 80.0f, 120.0f };
    constexpr std::array<float, 7> highCutHz { 8000.0f, 10000.0f, 12000.0f, 14000.0f, 16000.0f, 18000.0f, 0.0f };
    constexpr std::array<int, 4>   osFactor  { 1, 2, 4, 8 };
}

enum class StereoMode { dualMono = 0, link = 1, linkHpf = 2 };

struct ChannelSettings
{
    float thresholdDb = -18.0f;
    float makeupDb    = 4.0f;
    int   attackIdx   = 2;
    int   releaseIdx  = 3;
    float tube        = 0.5f;     // 0 = Harder, 1 = Softer
};

struct CoreSettings
{
    ChannelSettings ch[2];
    StereoMode stereo = StereoMode::link;
    int  scHpfIdx = 2;
    bool compIn   = true;
};

//==============================================================================
// The compressor itself. Runs at the oversampled rate, one stereo frame at a time.
//
// Signal path per channel:
//   input transformer -> PWM-style gain cell (feed-forward, soft knee)
//   -> make-up gain -> class-A triode -> output transformer -> DC block
//
// Detector: RMS-ish level -> soft-knee gain computer -> attack/release ballistics.
// "Auto" release is program dependent: a fast follower rides transients while a
// slow follower holds sustained reduction, so dense material recovers gently.
// Harder <-> Softer moves ratio, knee width and tube drive together.
class CompressorCore
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        for (auto& c : chan) c = ChannelState {};
        for (auto& h : scHpf) h.reset();
        for (auto& d : dc) d.prepare (fs);
        lastScHpfIdx = -1;
        rmsCoef = coef (2.0f);
        slowAttCoef = coef (400.0f);
        slowRelCoef = coef (1500.0f);
        autoFastRelCoef = coef (90.0f);
        smoothCoef = coef (20.0f);
        primed = false;
    }

    void update (const CoreSettings& s) noexcept
    {
        settings = s;
        for (int i = 0; i < 2; ++i)
        {
            const auto& cs = s.ch[i];
            auto& c = chan[(size_t) i];
            c.attCoef = coef (tables::attackMs[(size_t) std::clamp (cs.attackIdx, 0, 5)]);
            const float rel = tables::releaseMs[(size_t) std::clamp (cs.releaseIdx, 0, 5)];
            c.autoRelease = rel < 0.0f;
            c.relCoef = c.autoRelease ? autoFastRelCoef : coef (rel);

            const float t = std::clamp (cs.tube, 0.0f, 1.0f);
            c.ratio  = 8.0f + (2.0f - 8.0f) * t;      // Harder 8:1  ->  Softer 2:1
            c.kneeDb = 4.0f + (14.0f - 4.0f) * t;     // tight knee  ->  wide knee
            c.drive  = 2.4f + (0.9f - 2.4f) * t;      // more tube   ->  cleaner
            c.bias   = 0.28f + (0.12f - 0.28f) * t;   // more 2nd-harmonic when harder
            c.thrTarget = cs.thresholdDb;
            c.mkTarget  = cs.makeupDb;
            if (! primed) { c.thr = c.thrTarget; c.mk = c.mkTarget; }
        }
        primed = true;

        const int idx = std::clamp (s.scHpfIdx, 0, 5);
        if (idx != lastScHpfIdx)
        {
            lastScHpfIdx = idx;
            for (auto& h : scHpf)
            {
                if (idx == 0) h.setBypass();
                else          h.setHighPass (fs, tables::scHpfHz[(size_t) idx], 0.6);
            }
        }
    }

    // x: array of numChannels samples, processed in place
    void processFrame (float* x, int numChannels) noexcept
    {
        const int n = std::min (numChannels, 2);
        const bool linked = settings.stereo != StereoMode::dualMono && n == 2;
        const bool useHpf = settings.stereo == StereoMode::linkHpf;

        float target[2] = { 0.0f, 0.0f };
        float in[2] = { 0.0f, 0.0f };

        for (int i = 0; i < n; ++i)
        {
            auto& c = chan[(size_t) i];
            c.thr += smoothCoefInv() * (c.thrTarget - c.thr);
            c.mk  += smoothCoefInv() * (c.mkTarget  - c.mk);

            in[i] = transformer (x[i], 0.55f);
            const float sc = useHpf ? scHpf[(size_t) i].process (in[i]) : in[i];
            c.ms = rmsCoef * c.ms + (1.0f - rmsCoef) * sc * sc;
            const float levelDb = 10.0f * std::log10 (c.ms * 2.0f + 1.0e-12f); // sine RMS -> peak-equivalent
            target[i] = gainComputer (levelDb, c.thr, c.ratio, c.kneeDb);
        }

        if (linked)
        {
            const float m = std::max (target[0], target[1]);
            target[0] = target[1] = m;
        }

        for (int i = 0; i < n; ++i)
        {
            auto& c = chan[(size_t) i];
            float gr;
            if (c.autoRelease)
            {
                // slow follower holds the sustained part, fast follower the rest
                c.slow = ballistic (c.slow, target[i], slowAttCoef, slowRelCoef);
                const float fastTarget = std::max (0.0f, target[i] - c.slow);
                c.fast = ballistic (c.fast, fastTarget, c.attCoef, c.relCoef);
                gr = c.fast + c.slow;
            }
            else
            {
                c.fast = ballistic (c.fast, target[i], c.attCoef, c.relCoef);
                gr = c.fast;
            }
            c.grDb = gr;

            float y = in[i];
            if (settings.compIn) y *= dbToGain (-gr);
            y *= dbToGain (c.mk);
            y = triode (y, c.drive, c.bias);
            y = transformer (y, 0.45f);
            x[i] = dc[(size_t) i].process (y);
        }
    }

    float getGainReductionDb (int ch) const noexcept { return settings.compIn ? chan[(size_t) ch].grDb : 0.0f; }

    static float gainComputer (float levelDb, float thrDb, float ratio, float kneeDb) noexcept
    {
        const float over = levelDb - thrDb;
        const float slope = 1.0f - 1.0f / ratio;
        if (2.0f * over < -kneeDb) return 0.0f;
        if (2.0f * std::abs (over) <= kneeDb)
        {
            const float k = over + kneeDb * 0.5f;
            return slope * k * k / (2.0f * kneeDb);
        }
        return slope * over;
    }

private:
    struct ChannelState
    {
        float ms = 0, fast = 0, slow = 0, grDb = 0;
        float attCoef = 0, relCoef = 0;
        bool  autoRelease = false;
        float ratio = 4, kneeDb = 8, drive = 1.5f, bias = 0.2f;
        float thr = -18, mk = 0, thrTarget = -18, mkTarget = 0;
    };

    static float ballistic (float state, float target, float att, float rel) noexcept
    {
        const float a = target > state ? att : rel;
        return a * state + (1.0f - a) * target;
    }

    float coef (float ms) const noexcept { return (float) std::exp (-1.0 / (0.001 * ms * fs)); }
    float smoothCoefInv() const noexcept { return 1.0f - smoothCoef; }

    double fs = 48000.0;
    CoreSettings settings;
    std::array<ChannelState, 2> chan;
    std::array<Biquad, 2> scHpf;
    std::array<DcBlocker, 2> dc;
    int lastScHpfIdx = -1;
    float rmsCoef = 0, slowAttCoef = 0, slowRelCoef = 0, autoFastRelCoef = 0, smoothCoef = 0;
    bool primed = false;
};

} // namespace smooth
