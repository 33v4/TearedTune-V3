// Offline DSP tests for TearedTune (no JUCE needed).
//   g++ -O2 -std=c++17 -I Source tests/dsp_test.cpp -o dsp_test && ./dsp_test <output_dir>
#include "PitchCorrector.h"
#include <chrono>
#include <cstdarg>
#include <limits>
#include <complex>
#include <cstdio>
#include <fstream>
#include <functional>
#include <random>
#include <string>

using namespace tt;
using cd = std::complex<double>;
static int g_pass = 0, g_fail = 0;
static void check (bool ok, const char* name, const std::string& detail)
{
    std::printf ("[%s] %-26s %s\n", ok ? "PASS" : "FAIL", name, detail.c_str());
    ok ? ++g_pass : ++g_fail;
}
static std::string fmt (const char* f, ...)
{
    char b[300]; va_list a; va_start (a, f); std::vsnprintf (b, sizeof b, f, a); va_end (a); return b;
}

static double cents (double f, double ref) { return 1200.0 * std::log2 (f / ref); }

// ---- signal generators -----------------------------------------------------
// voice-like: 24 harmonics, 1/h^1.2 rolloff, slight noise. f0(t) in Hz.
static std::vector<float> voice (double fs, double seconds, const std::function<double (double)>& f0, float amp = 0.3f, uint32_t seed = 1)
{
    const size_t n = (size_t) (seconds * fs); std::vector<float> x (n);
    double ph = 0; std::mt19937 rng (seed); std::uniform_real_distribution<float> U (-1, 1);
    for (size_t i = 0; i < n; ++i)
    {
        const double t = (double) i / fs, f = f0 (t);
        ph += f / fs; ph -= std::floor (ph);
        double s = 0; const int H = std::min (24, (int) (0.45 * fs / f));
        for (int h = 1; h <= H; ++h) s += std::sin (2 * kPi * h * ph + 0.3 * h) / std::pow ((double) h, 1.2);
        x[i] = amp * (float) (s * 0.55) + 0.002f * U (rng);
    }
    return x;
}

// independent f0 estimator (normalised autocorrelation, parabolic interpolation)
static double estF0 (const std::vector<float>& x, size_t start, size_t N, double fs, double fmin = 90, double fmax = 1100)
{
    if (start + N + (size_t) (fs / fmin) + 2 >= x.size()) return 0;
    const int tmin = (int) (fs / fmax), tmax = (int) (fs / fmin);
    std::vector<double> r ((size_t) tmax + 2);
    for (int t = tmin - 1; t <= tmax + 1; ++t)
    {
        double s = 0, e1 = 0, e2 = 0;
        for (size_t i = 0; i < N; ++i) { const double a = x[start + i], b = x[start + i + (size_t) t]; s += a * b; e1 += a * a; e2 += b * b; }
        r[(size_t) t] = s / std::sqrt (e1 * e2 + 1e-18);
    }
    double mx = -2; for (int t = tmin; t <= tmax; ++t) mx = std::max (mx, r[(size_t) t]);
    int bt = tmin;
    for (int t = tmin + 1; t < tmax; ++t)            // first peak reaching 95% of the max => avoids octave-down errors
        if (r[(size_t) t] > 0.95 * mx && r[(size_t) t] >= r[(size_t) t - 1] && r[(size_t) t] >= r[(size_t) t + 1]) { bt = t; break; }
    const double a = r[(size_t) bt - 1], b = r[(size_t) bt], c = r[(size_t) bt + 1];
    const double den = a - 2 * b + c;
    return fs / ((double) bt + (std::fabs (den) > 1e-12 ? 0.5 * (a - c) / den : 0.0));
}

static std::vector<float> run (const std::vector<float>& in, double fs, const Params& p, int block = 512, PitchCorrector* keep = nullptr)
{
    PitchCorrector pc; pc.prepare (fs);
    std::vector<float> out (in);
    for (size_t i = 0; i < out.size(); i += (size_t) block)
    {
        float* ch[1] = { out.data() + i };
        pc.process (ch, 1, (int) std::min ((size_t) block, out.size() - i), p);
    }
    (void) keep;
    return out;
}

static double rms (const std::vector<float>& x, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i]; return std::sqrt (s / (double) (b - a)); }

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    std::ofstream f (path, std::ios::binary);
    const uint32_t n = (uint32_t) L.size(), db = n * 4, riff = 36 + db, fl = 16, rate = (uint32_t) sr, br = rate * 4;
    const uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
    f.write ("RIFF", 4); f.write ((const char*) &riff, 4); f.write ("WAVEfmt ", 8); f.write ((const char*) &fl, 4);
    f.write ((const char*) &pcm, 2); f.write ((const char*) &ch, 2); f.write ((const char*) &rate, 4); f.write ((const char*) &br, 4);
    f.write ((const char*) &ba, 2); f.write ((const char*) &bits, 2); f.write ("data", 4); f.write ((const char*) &db, 4);
    for (uint32_t i = 0; i < n; ++i)
    {
        const int16_t l = (int16_t) std::lround (std::clamp (L[i], -1.f, 1.f) * 32767.f), r = (int16_t) std::lround (std::clamp (R[i], -1.f, 1.f) * 32767.f);
        f.write ((const char*) &l, 2); f.write ((const char*) &r, 2);
    }
}

static uint16_t only (int pc) { return (uint16_t) (1 << pc); }

int main (int argc, char** argv)
{
    const std::string out = argc > 1 ? argv[1] : ".";
    const double fs = 44100;

    // 1) Detector accuracy across the range --------------------------------------
    std::printf ("\n== 1. Pitch detector accuracy (voice-like signal) ==\n");
    {
        double worst = 0; int bad = 0; int cnt = 0;
        for (double f = 82; f <= 880; f *= 1.0595)
        {
            PitchDetector det; det.prepare (fs); det.setRange (60, 1400);
            auto x = voice (fs, 0.6, [&] (double) { return f; });
            PitchDetector::Result r;
            for (size_t i = 0; i < x.size(); ++i) { det.push (x[i]); if (i % 132 == 131) r = det.analyse(); }
            ++cnt;
            if (! r.voiced) { ++bad; std::printf ("   not detected at %.1f Hz\n", f); continue; }
            const double e = std::fabs (cents (r.f0, f)); worst = std::max (worst, e); if (e > 10) ++bad;
        }
        check (bad == 0, "detector", fmt ("%d tones 82-880 Hz, worst error %.2f cents, failures %d", cnt, worst, bad));
    }

    // 2) Static detune is pulled to the target note --------------------------------
    std::printf ("\n== 2. Snap to note ==\n");
    for (double detC : { -40.0, -20.0, 30.0, 45.0 })
    {
        Params p; p.mask = only (9); p.retuneMs = 15;                 // only A allowed
        auto in = voice (fs, 2.0, [&] (double) { return 440.0 * std::pow (2.0, detC / 1200.0); });
        auto o = run (in, fs, p);
        const double f = estF0 (o, (size_t) (1.2 * fs), 4096, fs);
        check (std::fabs (cents (f, 440)) < 6.0, "snap A4", fmt ("input %+.0f cents -> output %.2f Hz (%+.2f cents)", detC, f, cents (f, 440)));
    }
    {
        Params p; p.mask = scaleMask (1, 0); p.retuneMs = 15;          // C major: input Eb (311 Hz) is a non-scale note
        const double eb = 311.127;
        auto in = voice (fs, 2.0, [&] (double) { return eb * 1.003; });
        auto o = run (in, fs, p);
        const double f = estF0 (o, (size_t) (1.2 * fs), 4096, fs, 90, 700);
        const double d1 = std::fabs (cents (f, 293.665)), d2 = std::fabs (cents (f, 329.628));   // D4 or E4
        check (std::min (d1, d2) < 8.0, "C major scale", fmt ("Eb4 input -> %.2f Hz (nearest scale note error %.1f cents)", f, std::min (d1, d2)));
    }

    // 3) Retune speed --------------------------------------------------------------
    std::printf ("\n== 3. Retune speed (A4 +50 cents -> 440 Hz) ==\n");
    for (float rt : { 0.0f, 100.0f, 300.0f })
    {
        Params p; p.mask = only (9); p.retuneMs = rt;
        auto in = voice (fs, 2.5, [&] (double t) { return 440.0 * std::pow (2.0, (t < 0.4 ? 0.0 : 0.0) / 1200.0) * std::pow (2.0, 45.0 / 1200.0); });
        auto o = run (in, fs, p);
        double tSettle = -1;
        for (size_t s = 0; s + 2048 + 2200 < o.size(); s += 256)
        {
            const double f = estF0 (o, s, 1024, fs);
            if (f > 0 && std::fabs (cents (f, 440)) < 10.0) { tSettle = ((double) s + 512) / fs; break; }
        }
        const double expectMax = 0.12 + rt / 1000.0 * 2.6 + 0.06;
        check (tSettle > 0 && tSettle < expectMax, "retune time", fmt ("retune %.0f ms: within 10 cents after %.0f ms (limit %.0f ms)", rt, tSettle * 1000, expectMax * 1000));
    }

    // 4) Amount = 0 leaves pitch alone ------------------------------------------------
    std::printf ("\n== 4. Amount / bypass ==\n");
    {
        Params p; p.mask = only (9); p.amount = 0.0f; p.retuneMs = 5;
        auto in = voice (fs, 2.0, [&] (double) { return 440.0 * std::pow (2.0, 40.0 / 1200.0); });
        auto o = run (in, fs, p);
        const double f = estF0 (o, (size_t) (1.2 * fs), 4096, fs);
        check (std::fabs (cents (f, 440.0 * std::pow (2.0, 40.0 / 1200.0))) < 4.0, "amount 0", fmt ("output stays at %+.1f cents (expected +40)", cents (f, 440)));
        p.amount = 0.5f; o = run (in, fs, p);
        const double f2 = estF0 (o, (size_t) (1.2 * fs), 4096, fs);
        check (std::fabs (cents (f2, 440) - 20.0) < 6.0, "amount 50%", fmt ("output %+.1f cents (expected about +20)", cents (f2, 440)));
        Params b; b.bypass = true; b.mask = only (9);
        auto ob = run (in, fs, b);
        const double fb = estF0 (ob, (size_t) (1.2 * fs), 4096, fs);
        check (std::fabs (cents (fb, 440.0 * std::pow (2.0, 40.0 / 1200.0))) < 2.0, "bypass", fmt ("pitch untouched (%+.1f cents)", cents (fb, 440)));
    }

    // 5) Vibrato preservation -------------------------------------------------------
    std::printf ("\n== 5. Vibrato preservation (+/-40 cents @ 5.5 Hz around A4) ==\n");
    {
        auto in = voice (fs, 3.5, [&] (double t) { return 440.0 * std::pow (2.0, 40.0 * std::sin (2 * kPi * 5.5 * t) / 1200.0); });
        auto depth = [&] (const std::vector<float>& o, double& mean)
        {
            double lo = 1e9, hi = -1e9, sum = 0; int n = 0;
            for (size_t s = (size_t) (1.5 * fs); s + 2048 + 2200 < o.size(); s += 256)
            {
                const double c = cents (estF0 (o, s, 1024, fs), 440); lo = std::min (lo, c); hi = std::max (hi, c); sum += c; ++n;
            }
            mean = sum / n; return (hi - lo) / 2.0;
        };
        double m0, m1;
        Params p; p.mask = only (9); p.retuneMs = 0; p.vibrato = 0.0f; const double d0 = depth (run (in, fs, p), m0);
        p.vibrato = 1.0f;                                                       const double d1 = depth (run (in, fs, p), m1);
        check (d0 < 0.6 * d1, "vibrato 0 flattens", fmt ("instant retune: residual depth %.1f cents vs %.1f with vibrato=1 (input 40)", d0, d1));
        check (d1 > 28.0 && std::fabs (m1) < 12.0, "vibrato 1 keeps", fmt ("depth %.1f cents, centre %+.1f cents (input 40, centre 0)", d1, m1));
    }

    // 6) Artefact level on a pure sine ----------------------------------------------
    std::printf ("\n== 6. Artefacts (pure 440 Hz sine, -30 cents, corrected to A4) ==\n");
    {
        const size_t n = (size_t) (3.0 * fs); std::vector<float> in (n); double ph = 0; const double f = 440.0 * std::pow (2.0, -30.0 / 1200.0);
        for (size_t i = 0; i < n; ++i) { ph += f / fs; in[i] = 0.3f * (float) std::sin (2 * kPi * ph); }
        Params p; p.mask = only (9); p.retuneMs = 5;
        auto o = run (in, fs, p);
        const size_t N = 32768, st = (size_t) (1.5 * fs);
        std::vector<cd> a (N);
        for (size_t i = 0; i < N; ++i) a[i] = o[st + i] * (0.5 - 0.5 * std::cos (2 * kPi * (double) i / N));
        // naive DFT around signal band only (cheap): total power via Parseval on windowed data, tone power via bins near 440
        double tot = 0; for (size_t i = 0; i < N; ++i) tot += std::norm (a[i]) ;
        double tone = 0; const double k0 = 440.0 * N / fs;
        for (int k = (int) k0 - 6; k <= (int) k0 + 6; ++k)
        {
            cd s = 0; for (size_t i = 0; i < N; ++i) s += a[i] * std::polar (1.0, -2 * kPi * k * (double) i / (double) N);
            tone += std::norm (s) / (double) N;
        }
        tone *= 2.0;   // one-sided bins hold half of a real tone's power
        const double ratioDb = 10 * std::log10 (std::max (tot - tone, 1e-12) / tone);
        check (ratioDb < -25.0, "sine purity", fmt ("energy outside the tone = %.1f dB relative", ratioDb));
    }

    // 7) Noise / silence ----------------------------------------------------------
    std::printf ("\n== 7. Unvoiced material passes through ==\n");
    {
        std::mt19937 rng (7); std::normal_distribution<float> N (0, 0.1f);
        std::vector<float> in ((size_t) (2 * fs)); for (auto& v : in) v = N (rng);
        Params p; p.retuneMs = 0; p.mask = scaleMask (1, 0);
        auto o = run (in, fs, p);
        const double ri = rms (in, (size_t) fs, in.size()), ro = rms (o, (size_t) fs, o.size());
        check (std::fabs (20 * std::log10 (ro / ri)) < 1.5, "white noise", fmt ("level change %.2f dB", 20 * std::log10 (ro / ri)));
        std::vector<float> sil ((size_t) fs, 0.0f); auto os = run (sil, fs, p);
        double mx = 0; for (float v : os) mx = std::max (mx, (double) std::fabs (v));
        check (mx < 1e-6, "silence", fmt ("max output on digital silence = %.2e", mx));
    }

    // 8) Robustness ----------------------------------------------------------------
    std::printf ("\n== 8. Randomised stress (finite + bounded) ==\n");
    {
        std::mt19937 rng (99); auto U = [&] (float a, float b) { return std::uniform_real_distribution<float> (a, b) (rng); };
        long bad = 0, tot = 0; float mx = 0;
        for (double sr : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
        {
            PitchCorrector pc; pc.prepare (sr);
            std::vector<float> L (512), R (512); Params p;
            for (int blk = 0; blk < (int) (sr * 10 / 512); ++blk)
            {
                if (blk % 10 == 0)
                {
                    p.retuneMs = U (0, 400); p.amount = U (0, 1); p.vibrato = U (0, 1); p.toleranceC = U (0, 50); p.transpose = (float) (int) U (-12, 12);
                    p.detuneC = U (-50, 50); p.refA = U (430, 450); p.mix = U (0, 1); p.inGain = U (0, 4); p.outGain = U (0, 2); p.range = (int) U (0, 3.99f);
                    p.mask = (uint16_t) ((int) U (0, 4095)); p.bypass = U (0, 1) > 0.9f;
                }
                const int kind = (int) U (0, 5);
                for (int i = 0; i < 512; ++i)
                {
                    const double t = (double) (blk * 512 + i) / sr; float v = 0;
                    if (kind == 0) v = (float) (0.4 * std::sin (2 * kPi * (100 + 800 * U (0, 1) * 0.0 + 220 * (1 + std::sin (t))) * t));
                    else if (kind == 1) v = U (-1, 1);
                    else if (kind == 2) v = 0;
                    else if (kind == 3) v = (i % 100 == 0) ? 1.0f : 0.0f;
                    else if (kind == 4) v = U (-1, 1) > 0.999f ? std::numeric_limits<float>::quiet_NaN() : 0.2f * (float) std::sin (2 * kPi * 300 * t);
                    else v = 8.0f * (float) std::sin (2 * kPi * 150 * t);
                    L[(size_t) i] = R[(size_t) i] = v;
                }
                float* ch[2] = { L.data(), R.data() };
                pc.process (ch, 2, 512, p);
                for (int i = 0; i < 512; ++i) for (float s : { L[(size_t) i], R[(size_t) i] }) { ++tot; if (! std::isfinite (s)) ++bad; else mx = std::max (mx, std::fabs (s)); }
            }
        }
        check (bad == 0 && mx < 200.0f, "stress", fmt ("%ld samples, 5 sample rates, non-finite %ld, max |x| %.2f", tot, bad, mx));
    }

    // 9) CPU ----------------------------------------------------------------------
    std::printf ("\n== 9. CPU (stereo, 44.1 kHz, voiced input, Wide range) ==\n");
    {
        auto in = voice (fs, 10.0, [&] (double t) { return 200 + 40 * std::sin (t * 3); });
        PitchCorrector pc; pc.prepare (fs); Params p; p.range = 3;
        std::vector<float> L (in), R (in);
        const auto t0 = std::chrono::steady_clock::now();
        for (size_t i = 0; i < L.size(); i += 512) { float* ch[2] = { L.data() + i, R.data() + i }; pc.process (ch, 2, (int) std::min<size_t> (512, L.size() - i), p); }
        const double s = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
#if defined(__SANITIZE_ADDRESS__)
        std::printf ("[INFO] cpu (sanitizer build, not meaningful) %.3f s for 10 s\n", s);
#else
        check (10.0 / s > 10.0, "cpu", fmt ("10 s rendered in %.3f s = %.0fx realtime (~%.1f%% of one core)", s, 10.0 / s, 100.0 * s / 10.0));
#endif
    }

    // 10) Demo render -------------------------------------------------------------
    {
        // an out-of-tune "sung" line (C major melody, each note 20-45 cents off, with vibrato), before / after
        const int notes[] = { 60, 62, 64, 67, 65, 64, 62, 60 }; const double offs[] = { 35, -30, 25, -45, 40, -20, 30, -35 };
        auto f0 = [&] (double t)
        {
            const int i = std::min (7, (int) (t / 0.5)); const double lt = t - i * 0.5;
            const double vib = lt > 0.15 ? 18.0 * std::sin (2 * kPi * 5.5 * (lt - 0.15)) : 0.0;
            return 440.0 * std::pow (2.0, (notes[i] - 69 + (offs[i] + vib) / 100.0) / 12.0);
        };
        auto in = voice (fs, 4.2, f0, 0.35f);
        Params p; p.mask = scaleMask (1, 0); p.retuneMs = 25; p.vibrato = 0.8f;
        auto o = run (in, fs, p);
        std::vector<float> L, R;                                         // left = original (dry), right = corrected
        for (size_t i = 0; i < in.size(); ++i) { L.push_back (in[i]); R.push_back (o[i]); }
        writeWav (out + "/TearedTune_demo_L-original_R-corrected.wav", L, R, (int) fs);
        Params hard = p; hard.retuneMs = 0; hard.vibrato = 0; auto oh = run (in, fs, hard);
        writeWav (out + "/TearedTune_demo_hard-tune_L-original_R-corrected.wav", L, oh, (int) fs);
        // objective: median absolute deviation from the scale note, before vs after
        auto dev = [&] (const std::vector<float>& s) { std::vector<double> v; for (int i = 0; i < 8; ++i) { const size_t a = (size_t) ((i * 0.5 + 0.30) * fs); const double f = estF0 (s, a, 2048, fs); v.push_back (std::fabs (cents (f, 440.0 * std::pow (2.0, (notes[i] - 69) / 12.0)))); } std::sort (v.begin(), v.end()); return v[4]; };
        const double before = dev (in), after = dev (oh);
        check (after < before * 0.5, "demo accuracy", fmt ("median deviation from the intended note: before %.1f cents, after (hard tune) %.1f cents", before, after));
    }

    std::printf ("\nRESULT: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
