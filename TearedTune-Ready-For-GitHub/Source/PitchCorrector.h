// TearedTune - pitch correction engine. Header-only, NO JUCE dependency (so it can be tested offline).
//
//  Detector : YIN-style cumulative-mean-normalised difference on a decimated signal, refined at full rate
//             with parabolic interpolation.
//  Shifter  : pitch-synchronous two-tap delay-line shifter. The crossfade window is exactly TWO detected
//             periods, so both taps always read the waveform in phase (no comb filtering), and the tap
//             jump happens only while its gain is zero.
//  Control  : key/scale note mask, retune speed, amount, tolerance ("flex"), vibrato preservation,
//             transpose, detune, reference tuning.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace tt
{
constexpr double kPi = 3.14159265358979323846;

//==============================================================================
struct Params
{
    float retuneMs   = 20.0f;    // 0..400  (values below 3 ms behave like 3 ms)
    float amount     = 1.0f;     // 0..1    how much of the correction is applied
    float vibrato    = 0.0f;     // 0..1    preserve natural vibrato
    float toleranceC = 0.0f;     // 0..50   cents around the target that are left alone
    float transpose  = 0.0f;     // semitones
    float detuneC    = 0.0f;     // cents
    float refA       = 440.0f;   // Hz
    float mix        = 1.0f;     // 0..1
    float inGain     = 1.0f;     // linear
    float outGain    = 1.0f;     // linear
    int   range      = 1;        // 0 low, 1 mid, 2 high, 3 wide
    uint16_t mask    = 0x0FFF;   // allowed pitch classes, bit 0 = C ... bit 11 = B
    bool  bypass     = false;
};

inline void rangeHz (int range, float& fmin, float& fmax)
{
    switch (range)
    {
        case 0:  fmin = 60.0f;  fmax = 400.0f;  break;
        case 2:  fmin = 150.0f; fmax = 1400.0f; break;
        case 3:  fmin = 50.0f;  fmax = 1500.0f; break;
        default: fmin = 80.0f;  fmax = 800.0f;  break;
    }
}

// scale tables: intervals above the root
inline uint16_t scaleMask (int scale, int root)
{
    static const uint16_t base[] = {
        0xFFF,                                                           // 0 Chromatic
        (1<<0)|(1<<2)|(1<<4)|(1<<5)|(1<<7)|(1<<9)|(1<<11),              // 1 Major
        (1<<0)|(1<<2)|(1<<3)|(1<<5)|(1<<7)|(1<<8)|(1<<10),              // 2 Natural minor
        (1<<0)|(1<<2)|(1<<3)|(1<<5)|(1<<7)|(1<<8)|(1<<11),              // 3 Harmonic minor
        (1<<0)|(1<<2)|(1<<4)|(1<<7)|(1<<9),                              // 4 Major pentatonic
        (1<<0)|(1<<3)|(1<<5)|(1<<7)|(1<<10),                             // 5 Minor pentatonic
        (1<<0)|(1<<3)|(1<<5)|(1<<6)|(1<<7)|(1<<10),                      // 6 Blues
        (1<<0)|(1<<2)|(1<<3)|(1<<5)|(1<<7)|(1<<9)|(1<<10),               // 7 Dorian
        (1<<0)|(1<<2)|(1<<4)|(1<<5)|(1<<7)|(1<<9)|(1<<10),               // 8 Mixolydian
    };
    if (scale < 0 || scale > 8) return 0xFFF;
    const uint16_t b = base[scale];
    uint16_t out = 0;
    for (int i = 0; i < 12; ++i) if (b & (1 << i)) out |= (uint16_t) (1 << ((i + root) % 12));
    return out;
}

//==============================================================================
class PitchDetector
{
public:
    struct Result { float f0 = 0; float confidence = 0; bool voiced = false; };

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        dec = std::max (1, (int) std::lround (fs / 11025.0));
        fsd = fs / (float) dec;
        hist.assign (kHist, 0.0f); decBuf.assign (kDec, 0.0f);
        d.assign (kDec, 0.0f); cm.assign (kDec, 0.0f);
        histPos = decPos = 0; acc = 0; accCount = 0; hpX = hpY = 0;
        hpR = 1.0f - (float) (2.0 * kPi * 30.0 / fs);
        med[0] = med[1] = med[2] = 0; medN = 0;
    }
    void setRange (float lo, float hi) { fmin = lo; fmax = hi; }

    inline void push (float x)
    {
        const float y = x - hpX + hpR * hpY;       // DC blocker
        hpX = x; hpY = y;
        hist[(size_t) (histPos & (kHist - 1))] = y; ++histPos;
        acc += y;
        if (++accCount == dec) { decBuf[(size_t) (decPos & (kDec - 1))] = acc / (float) dec; ++decPos; acc = 0; accCount = 0; }
    }

    Result analyse()
    {
        Result r;
        const int tauMin = std::max (2, (int) std::floor (fsd / fmax));
        int tauMax = (int) std::ceil (fsd / fmin);
        tauMax = std::min (tauMax, kDec / 2 - 8);
        const int Wd = tauMax + 16;
        if (decPos < (long) (Wd + tauMax + 4) || histPos < (long) kHist / 2) return r;

        const long newest = decPos - 1;
        auto X = [&] (long i) { return decBuf[(size_t) (i & (kDec - 1))]; };

        double energy = 0;
        for (int j = 0; j < Wd; ++j) { const float a = X (newest - j); energy += (double) a * a; }
        const float rms = (float) std::sqrt (energy / Wd);
        if (rms < 0.0015f) return r;                       // below roughly -56 dBFS: treat as silence

        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float sum = 0;
            for (int j = 0; j < Wd; ++j) { const float df = X (newest - j) - X (newest - j - tau); sum += df * df; }
            d[(size_t) tau] = sum;
        }
        cm[1] = 1.0f; float run = 0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            run += d[(size_t) tau];
            cm[(size_t) tau] = run > 1e-12f ? d[(size_t) tau] * (float) tau / run : 1.0f;
        }

        int est = -1;
        for (int tau = tauMin; tau <= tauMax; ++tau)
            if (cm[(size_t) tau] < 0.18f)
            {
                while (tau + 1 <= tauMax && cm[(size_t) tau + 1] < cm[(size_t) tau]) ++tau;
                est = tau; break;
            }
        if (est < 0)
        {
            int best = tauMin;
            for (int tau = tauMin; tau <= tauMax; ++tau) if (cm[(size_t) tau] < cm[(size_t) best]) best = tau;
            if (cm[(size_t) best] < 0.30f) est = best;
        }
        if (est < 0) return r;

        // ---- full-rate refinement around the coarse estimate ----
        const int tc = est * dec;
        const int lo = std::max (2, tc - dec - 1), hi = tc + dec + 1;
        const int Wf = 768;
        if (histPos < (long) (Wf + hi + 4)) return r;
        const long hn = histPos - 1;
        auto H = [&] (long i) { return hist[(size_t) (i & (kHist - 1))]; };
        std::vector<double>& df = dfull; df.assign ((size_t) (hi - lo + 1), 0.0);
        int bi = 0;
        for (int tau = lo; tau <= hi; ++tau)
        {
            double s = 0;
            for (int j = 0; j < Wf; ++j) { const double a = (double) H (hn - j) - (double) H (hn - j - tau); s += a * a; }
            df[(size_t) (tau - lo)] = s;
            if (s < df[(size_t) bi]) bi = tau - lo;
        }
        double tauF = (double) (lo + bi);
        if (bi > 0 && bi < (int) df.size() - 1)
        {
            const double a = df[(size_t) bi - 1], b = df[(size_t) bi], c = df[(size_t) bi + 1];
            const double den = a - 2.0 * b + c;
            if (std::fabs (den) > 1e-18) tauF += 0.5 * (a - c) / den;
        }
        r.f0 = (float) ((double) fs / tauF);
        r.confidence = 1.0f - cm[(size_t) est];
        r.voiced = r.f0 >= fmin * 0.9f && r.f0 <= fmax * 1.1f;

        if (r.voiced)                                       // median-of-3 on f0 rejects single octave glitches
        {
            if (medN == 0) med[0] = med[1] = med[2] = r.f0; else { med[0] = med[1]; med[1] = med[2]; med[2] = r.f0; }
            ++medN;
            std::array<float, 3> s { med[0], med[1], med[2] }; std::sort (s.begin(), s.end());
            r.f0 = s[1];
        }
        else medN = 0;
        return r;
    }

    void resetMedian() { medN = 0; }

private:
    static constexpr int kHist = 8192, kDec = 1024;
    float fs = 44100, fsd = 11025, fmin = 80, fmax = 800, hpR = 0.99f, hpX = 0, hpY = 0, acc = 0;
    int dec = 4, accCount = 0;
    long histPos = 0, decPos = 0;
    int medN = 0; float med[3] = { 0, 0, 0 };
    std::vector<float> hist, decBuf, d, cm;
    std::vector<double> dfull;
};

//==============================================================================
class PitchCorrector
{
public:
    static constexpr int kMaxCh = 2;

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        det.prepare (sampleRate);
        for (auto& b : ring) b.assign (kRing, 0.0f);
        wPos = 0;
        dmin = std::max (16.0, 0.003 * fs);
        hop = std::max (32, (int) std::lround (fs * 0.003));
        hopCount = 0;
        const double W0 = 2.0 * fs / 200.0;
        Wtarget = W0;
        head[0] = { 0.25, W0 }; head[1] = { 0.75, W0 };
        sApplied = sTarget = 0; gate = 0; gateTarget = 0; wetAmt = 1; outG = 1;
        voicedNow = false; holdCount = 0; curNote = -1; midiSlow = 0; wasVoiced = false;
        telemDetected = telemTarget = 0; telemVoiced = false; peakIn = peakOut = 0;
    }

    int latencySamples() const { return (int) std::lround (dmin + fs / 200.0); }

    // in-place processing of up to kMaxCh channels
    void process (float* const* io, int numCh, int n, const Params& p)
    {
        numCh = std::min (numCh, kMaxCh);
        float lo, hi; rangeHz (p.range, lo, hi); det.setRange (lo, hi);

        const double retune = std::max (3.0, (double) p.retuneMs) * 0.001;
        const float sCoef = (float) (1.0 - std::exp (-1.0 / (retune * fs)));
        const float gCoef = (float) (1.0 - std::exp (-1.0 / (0.008 * fs)));
        const float mCoef = (float) (1.0 - std::exp (-1.0 / (0.010 * fs)));
        const double Ldry = dmin + fs / 200.0;
        const float targetWet = p.bypass ? 0.0f : p.mix;

        for (int i = 0; i < n; ++i)
        {
            float x[kMaxCh] = { 0, 0 };
            float mono = 0;
            for (int c = 0; c < numCh; ++c)
            {
                float v = io[c][i];
                if (! std::isfinite (v)) v = 0.0f;
                v *= p.inGain;
                x[c] = v; mono += v;
                peakIn = std::max (peakIn * 0.9997f, std::fabs (v));
            }
            mono /= (float) std::max (1, numCh);
            det.push (mono);
            for (int c = 0; c < numCh; ++c) ring[(size_t) c][(size_t) (wPos & (kRing - 1))] = x[c];

            if (++hopCount >= hop) { hopCount = 0; onAnalysis (det.analyse(), p); }

            // smooth control signals
            sApplied += (sTarget - sApplied) * sCoef;
            gate += (gateTarget - gate) * gCoef;
            wetAmt += (targetWet - wetAmt) * mCoef;
            outG += (p.outGain - outG) * mCoef;

            // advance the two taps
            const double r = std::exp2 ((double) sApplied / 12.0);
            for (auto& h : head)
            {
                h.u += (1.0 - r) / h.W;
                if (h.u >= 1.0)     { h.u -= 1.0; h.W = Wtarget; }
                else if (h.u < 0.0) { h.u += 1.0; h.W = Wtarget; }
            }
            const double dA = dmin + head[0].W * head[0].u, dB = dmin + head[1].W * head[1].u;
            const double sA = std::sin (kPi * head[0].u);
            const float gA = (float) (sA * sA), gB = 1.0f - gA;

            for (int c = 0; c < numCh; ++c)
            {
                const float wet = gA * readHermite (c, (double) wPos - dA) + gB * readHermite (c, (double) wPos - dB);
                const float dry = readHermite (c, (double) wPos - Ldry);
                const float proc = gate * wet + (1.0f - gate) * dry;
                const float y = (wetAmt * proc + (1.0f - wetAmt) * dry) * outG;
                io[c][i] = y;
                peakOut = std::max (peakOut * 0.9997f, std::fabs (y));
            }
            ++wPos;
        }
    }

    // telemetry for the UI (written in process())
    float telemDetected = 0, telemTarget = 0, peakIn = 0, peakOut = 0;
    bool telemVoiced = false;

private:
    struct Head { double u; double W; };

    static constexpr int kRing = 32768;

    inline float readHermite (int c, double pos) const
    {
        const double fl = std::floor (pos);
        const float t = (float) (pos - fl);
        const long i = (long) fl;
        const auto& b = ring[(size_t) c];
        const float y0 = b[(size_t) ((i - 1) & (kRing - 1))], y1 = b[(size_t) (i & (kRing - 1))];
        const float y2 = b[(size_t) ((i + 1) & (kRing - 1))], y3 = b[(size_t) ((i + 2) & (kRing - 1))];
        const float c0 = y1, c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + c0;
    }

    static int pickNote (float ref, uint16_t mask, int current, float hyst)
    {
        if (mask == 0) return -1;
        const int base = (int) std::lround (ref);
        int best = -1; float bestD = 1e9f;
        for (int off = -7; off <= 7; ++off)
        {
            const int n = base + off;
            if (mask & (1 << (((n % 12) + 12) % 12)))
            {
                const float dist = std::fabs (ref - (float) n);
                if (dist < bestD) { bestD = dist; best = n; }
            }
        }
        if (current >= 0 && best != current && (mask & (1 << (((current % 12) + 12) % 12))))
            if (std::fabs (ref - (float) current) < bestD + hyst) return current;   // hysteresis: avoid flip-flopping
        return best;
    }

    void onAnalysis (const PitchDetector::Result& r, const Params& p)
    {
        const int kHold = 20;                    // analyses (~60 ms) to bridge short dropouts
        if (r.voiced)
        {
            const float m = 69.0f + 12.0f * std::log2 (r.f0 / p.refA);
            if (! wasVoiced) midiSlow = m;
            else midiSlow += (m - midiSlow) * (float) (1.0 - std::exp (-(double) hop / fs / 0.12));
            wasVoiced = true;

            const float ref = m + (midiSlow - m) * p.vibrato;
            const int n = pickNote (ref, p.mask, curNote, 0.12f);
            curNote = n;
            float err = 0.0f;
            if (n >= 0)
            {
                err = (float) n - ref;
                const float tol = p.toleranceC * 0.01f;
                const float a = std::fabs (err);
                err = a <= tol ? 0.0f : std::copysign (a - tol, err);
            }
            sTarget = p.amount * err + p.transpose + p.detuneC * 0.01f;
            Wtarget = std::clamp (2.0 * (double) fs / (double) r.f0, 2.0 * fs / 1500.0, 2.0 * fs / 50.0);
            holdCount = kHold; voicedNow = true; gateTarget = 1.0f;

            telemDetected = m;
            telemTarget = (n >= 0 ? (float) n : m) + p.transpose + p.detuneC * 0.01f;
        }
        else
        {
            if (holdCount > 0) --holdCount;
            else { voicedNow = false; gateTarget = 0.0f; curNote = -1; wasVoiced = false; sTarget = 0.0f; }
        }
        telemVoiced = voicedNow;
    }

    float fs = 44100.0f;
    double dmin = 132.0;
    int hop = 132, hopCount = 0;
    long wPos = 0;
    PitchDetector det;
    std::array<std::vector<float>, kMaxCh> ring;
    Head head[2] = { { 0.25, 441.0 }, { 0.75, 441.0 } };
    double Wtarget = 441.0;
    float sApplied = 0, sTarget = 0, gate = 0, gateTarget = 0, wetAmt = 1, outG = 1;
    bool voicedNow = false, wasVoiced = false;
    int holdCount = 0, curNote = -1;
    float midiSlow = 0;
};

} // namespace tt
