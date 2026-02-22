// ==========================================================================
// LangevinDSP.h — Langevin EQ-251A analog-modeled DSP engine
//
// No-compromise implementation:
//   • 64-bit double-precision throughout
//   • 4× oversampling with linear-phase FIR anti-aliasing
//   • Bilinear transform with frequency pre-warping
//   • Proportional-Q from passive LC bridged-T topology
//   • Per-sample parameter smoothing (no zipper noise)
//   • Transposed Direct Form II for numerical stability
//
// Filter models derived from Rick Chinn schematic analysis:
//   LF: First-order low shelf (L3 + C3 in shunt, 40/100 Hz)
//   HF: Second-order peaking bell (LC resonant shunt, 3k/5k/10k/15k Hz)
//       Peaking in BOTH boost and cut (confirmed by topology + Rane Note 122)
//
// Copyright 2026 Morris Collaboratives. MIT License.
// ==========================================================================

#pragma once

#include <cmath>
#include <cstring>
#include <algorithm>

namespace Langevin {

// ======================================================================
// Compile-time constants
// ======================================================================
static constexpr int    OVERSAMPLING_FACTOR = 4;
static constexpr int    FIR_TAPS = 192;      // linear-phase anti-alias FIR
static constexpr double PI = 3.14159265358979323846;
static constexpr double TWO_PI = 2.0 * PI;

// ======================================================================
// Kaiser window for FIR design
// ======================================================================
inline double besselI0(double x) {
    // Modified Bessel function of the first kind, order 0
    // Polynomial approximation (Abramowitz & Stegun)
    double sum = 1.0, term = 1.0;
    const double x2 = x * x * 0.25;
    for (int k = 1; k < 30; ++k) {
        term *= x2 / (double)(k * k);
        sum += term;
        if (term < 1e-20 * sum) break;
    }
    return sum;
}

inline double kaiserWindow(int n, int N, double beta) {
    double mid = (N - 1) * 0.5;
    double r = (n - mid) / mid;
    return besselI0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / besselI0(beta);
}

// ======================================================================
// Oversampler — 4× with linear-phase FIR
//
// Uses a Kaiser-windowed sinc (beta=10, ~100 dB stopband attenuation).
// Cutoff at 0.125× oversampled Nyquist (= exactly original Nyquist),
// giving a steep transition band that preserves the full audible spectrum
// while providing ~100 dB rejection at image frequencies.
// ======================================================================
class Oversampler {
public:
    void prepare(double sampleRate) {
        sampleRate_ = sampleRate;
        oversampledRate_ = sampleRate * OVERSAMPLING_FACTOR;
        designFilter();
        reset();
    }

    void reset() {
        std::memset(upBuffer_, 0, sizeof(upBuffer_));
        std::memset(downBuffer_, 0, sizeof(downBuffer_));
        upPos_ = 0;
        downPos_ = 0;
    }

    double oversampledRate() const { return oversampledRate_; }

    // Latency in base-rate samples from upsample + downsample FIR
    int latencySamples() const {
        // Each FIR pass has (N-1)/2 samples group delay at the OS rate.
        // Up + down = 2 × (N-1)/2 / OS_FACTOR base-rate samples.
        return (FIR_TAPS - 1) / OVERSAMPLING_FACTOR;
    }

    // Upsample: insert one input sample, get OVERSAMPLING_FACTOR output samples
    void upsample(double input, double* output) {
        // Insert input with zero-stuffing
        for (int i = 0; i < OVERSAMPLING_FACTOR; ++i) {
            upBuffer_[upPos_] = (i == 0) ? input * OVERSAMPLING_FACTOR : 0.0;
            // Apply FIR (brute-force convolution on zero-stuffed buffer)
            double sum = 0.0;
            int pos = upPos_;
            for (int t = 0; t < FIR_TAPS; ++t) {
                sum += firCoeffs_[t] * upBuffer_[pos];
                if (--pos < 0) pos = FIR_TAPS - 1;
            }
            output[i] = sum;
            if (++upPos_ >= FIR_TAPS) upPos_ = 0;
        }
    }

    // Downsample: take OVERSAMPLING_FACTOR input samples, return one output
    double downsample(const double* input) {
        double result = 0.0;
        for (int i = 0; i < OVERSAMPLING_FACTOR; ++i) {
            downBuffer_[downPos_] = input[i];
            if (i == OVERSAMPLING_FACTOR - 1) {
                // Apply FIR only on the output sample
                double sum = 0.0;
                int pos = downPos_;
                for (int t = 0; t < FIR_TAPS; ++t) {
                    sum += firCoeffs_[t] * downBuffer_[pos];
                    if (--pos < 0) pos = FIR_TAPS - 1;
                }
                result = sum;
            }
            if (++downPos_ >= FIR_TAPS) downPos_ = 0;
        }
        return result;
    }

private:
    void designFilter() {
        // Kaiser-windowed sinc lowpass
        // Cutoff must be at original Nyquist relative to oversampled rate:
        //   fc = 0.5 / OVERSAMPLING_FACTOR = 0.125 (normalized to OS Nyquist)
        // Transition band: 0.125 to 0.25 (first image band starts at 0.25)
        // Using fc = 0.125 places the -6dB point at original Nyquist,
        // giving ~100dB rejection at the first alias frequency.
        const double fc = 0.5 / OVERSAMPLING_FACTOR;  // 0.125
        const double beta = 10.0;  // ~100 dB stopband attenuation

        double sum = 0.0;
        for (int n = 0; n < FIR_TAPS; ++n) {
            double mid = (FIR_TAPS - 1) * 0.5;
            double x = n - mid;
            double sinc = (std::abs(x) < 1e-10) ? 1.0 : std::sin(TWO_PI * fc * x) / (PI * x);
            firCoeffs_[n] = sinc * kaiserWindow(n, FIR_TAPS, beta);
            sum += firCoeffs_[n];
        }
        // Normalize for unity gain at DC
        for (int n = 0; n < FIR_TAPS; ++n)
            firCoeffs_[n] /= sum;
    }

    double sampleRate_ = 44100.0;
    double oversampledRate_ = 44100.0 * OVERSAMPLING_FACTOR;
    double firCoeffs_[FIR_TAPS] = {};
    double upBuffer_[FIR_TAPS] = {};
    double downBuffer_[FIR_TAPS] = {};
    int upPos_ = 0;
    int downPos_ = 0;
};

// ======================================================================
// Parameter smoother — exponential one-pole
// Provides per-sample interpolation for all parameters.
// Time constant: ~5 ms (fast enough for responsive control,
// slow enough to prevent zipper noise).
// ======================================================================
class ParamSmoother {
public:
    void prepare(double sampleRate) {
        // 5ms time constant
        const double tau = 0.005;
        coeff_ = 1.0 - std::exp(-1.0 / (sampleRate * tau));
    }

    void setTarget(double target) { target_ = target; }
    double current() const { return current_; }

    double next() {
        current_ += coeff_ * (target_ - current_);
        return current_;
    }

    void snapTo(double value) { current_ = target_ = value; }
    void snapToTarget() { current_ = target_; }

    bool isSettled() const {
        return std::abs(target_ - current_) < 1e-6;
    }

private:
    double coeff_ = 0.01;
    double target_ = 0.0;
    double current_ = 0.0;
};

// ======================================================================
// First-order shelf filter (Transposed Direct Form II)
//
// Analog prototype (low shelf):
//   H(s) = (s + ωz) / (s + ωp)
//
// For boost (gain G > 1):
//   ωz = ωc · √G,  ωp = ωc / √G
//   → H(0) = ωz/ωp = G,  H(∞) = 1
//
// For cut (gain G < 1, expressed as 1/G_abs):
//   ωp = ωc · √G_abs,  ωz = ωc / √G_abs
//   → H(0) = ωz/ωp = 1/G_abs,  H(∞) = 1
//
// Digitized via bilinear transform with pre-warping at ωc.
// ======================================================================
class ShelfFilter {
public:
    void prepare(double sampleRate) {
        fs_ = sampleRate;
        reset();
    }

    void reset() { z1_ = 0.0; }

    void setParams(double freqHz, double gainDb) {
        if (std::abs(gainDb) < 0.01) {
            // Bypass: unity gain
            b0_ = 1.0; b1_ = 0.0; a1_ = 0.0;
            return;
        }

        const double G = std::pow(10.0, std::abs(gainDb) / 20.0);
        const double wc = TWO_PI * freqHz;

        double wz, wp;
        if (gainDb > 0.0) {
            // Boost
            wz = wc * std::sqrt(G);
            wp = wc / std::sqrt(G);
        } else {
            // Cut
            wp = wc * std::sqrt(G);
            wz = wc / std::sqrt(G);
        }

        // Pre-warp both frequencies
        const double T = 1.0 / fs_;
        const double wz_d = (2.0 / T) * std::tan(wz * T / 2.0);
        const double wp_d = (2.0 / T) * std::tan(wp * T / 2.0);

        // Bilinear transform: s = (2/T)(z-1)/(z+1)
        // H(z) = (2/T + wz_d + (wz_d - 2/T)z^-1) / (2/T + wp_d + (wp_d - 2/T)z^-1)
        const double c = 2.0 / T;
        const double num0 = c + wz_d;
        const double num1 = wz_d - c;
        const double den0 = c + wp_d;
        const double den1 = wp_d - c;

        b0_ = num0 / den0;
        b1_ = num1 / den0;
        a1_ = -den1 / den0;
    }

    double process(double x) {
        // Transposed Direct Form II
        double y = b0_ * x + z1_;
        z1_ = b1_ * x + a1_ * y;
        return y;
    }

private:
    double fs_ = 44100.0;
    double b0_ = 1.0, b1_ = 0.0, a1_ = 0.0;
    double z1_ = 0.0;
};

// ======================================================================
// Second-order peaking/bell filter (Transposed Direct Form II)
//
// Analog prototype (parametric peaking EQ):
//   Boost:  H(s) = (s² + s·G·ω₀/Q + ω₀²) / (s² + s·ω₀/Q + ω₀²)
//   Cut:    H(s) = (s² + s·ω₀/(Q·G) + ω₀²) / (s² + s·ω₀/Q + ω₀²)
//
// Proportional Q from passive LC bridged-T:
//   Q increases with gain amount. At low gain, the bell is very broad
//   (Q ≈ 0.25), appearing almost shelf-like. At max gain (14 dB),
//   Q ≈ 1.5–1.8 depending on frequency.
//
// Per-frequency damping from selector resistors (160–1k3 Ω):
//   Higher frequencies get slightly tighter Q due to lower
//   damping resistance relative to LC impedance.
//
// Digitized via bilinear transform with pre-warping at ω₀.
// ======================================================================
class PeakFilter {
public:
    void prepare(double sampleRate) {
        fs_ = sampleRate;
        reset();
    }

    void reset() { z1_ = z2_ = 0.0; }

    void setParams(double freqHz, double gainDb) {
        if (std::abs(gainDb) < 0.01) {
            b0_ = 1.0; b1_ = 0.0; b2_ = 0.0; a1_ = 0.0; a2_ = 0.0;
            return;
        }

        const double absDb = std::abs(gainDb);
        const double G = std::pow(10.0, absDb / 20.0);

        // Proportional Q — passive LC bridged-T behavior
        // Base Q at ~1 dB: very broad (0.25)
        // Max Q at 14 dB: ~1.6, slightly higher at higher frequencies
        const double freqQScale = 1.0 + std::log10(freqHz / 3000.0) * 0.12;
        const double baseQ = 0.25;
        const double maxQ = 1.6 * freqQScale;
        const double Q = baseQ + (maxQ - baseQ) * std::pow(absDb / 14.0, 0.7);

        // Pre-warp center frequency
        const double T = 1.0 / fs_;
        const double w0 = TWO_PI * freqHz;
        const double w0_w = (2.0 / T) * std::tan(w0 * T / 2.0);

        // Analog prototype coefficients
        double bw_num, bw_den;
        bw_den = w0_w / Q;  // denominator bandwidth always w0/Q

        if (gainDb > 0.0) {
            bw_num = G * w0_w / Q;  // boost: wider numerator
        } else {
            bw_num = w0_w / (Q * G);  // cut: narrower numerator
        }

        // Bilinear transform of second-order section
        // s = (2/T)(z-1)/(z+1)
        // H(s) = (s² + bw_num·s + w0²) / (s² + bw_den·s + w0²)
        const double c = 2.0 / T;
        const double c2 = c * c;
        const double w02 = w0_w * w0_w;

        // Denominator: s² + bw_den·s + w0²
        const double d0 = c2 + bw_den * c + w02;
        const double d1 = 2.0 * (w02 - c2);
        const double d2 = c2 - bw_den * c + w02;

        // Numerator: s² + bw_num·s + w0²
        const double n0 = c2 + bw_num * c + w02;
        const double n1 = 2.0 * (w02 - c2);
        const double n2 = c2 - bw_num * c + w02;

        b0_ = n0 / d0;
        b1_ = n1 / d0;
        b2_ = n2 / d0;
        a1_ = -d1 / d0;
        a2_ = -d2 / d0;
    }

    double process(double x) {
        // Transposed Direct Form II
        double y = b0_ * x + z1_;
        z1_ = b1_ * x + a1_ * y + z2_;
        z2_ = b2_ * x + a2_ * y;
        return y;
    }

private:
    double fs_ = 44100.0;
    double b0_ = 1.0, b1_ = 0.0, b2_ = 0.0;
    double a1_ = 0.0, a2_ = 0.0;
    double z1_ = 0.0, z2_ = 0.0;
};

// ======================================================================
// LF frequency options
// ======================================================================
enum class LFFreq : int {
    Hz40 = 0,
    Hz100 = 1
};

inline double lfFreqToHz(LFFreq f) {
    return (f == LFFreq::Hz40) ? 40.0 : 100.0;
}

inline double lfFreqToHz(int idx) {
    return lfFreqToHz(static_cast<LFFreq>(idx));
}

// ======================================================================
// HF frequency options
// ======================================================================
enum class HFFreq : int {
    kHz3 = 0,
    kHz5 = 1,
    kHz10 = 2,
    kHz15 = 3
};

inline double hfFreqToHz(HFFreq f) {
    switch (f) {
        case HFFreq::kHz3:  return 3000.0;
        case HFFreq::kHz5:  return 5000.0;
        case HFFreq::kHz10: return 10000.0;
        case HFFreq::kHz15: return 15000.0;
    }
    return 3000.0;
}

inline double hfFreqToHz(int idx) {
    return hfFreqToHz(static_cast<HFFreq>(idx));
}

// ======================================================================
// Complete channel strip — one channel of the EQ-251A
//
// Signal flow:
//   Input → 4× upsample → LF shelf → HF peak → 4× downsample → Output
//
// All filtering happens at the oversampled rate to avoid frequency
// warping artifacts, particularly critical for the 10k and 15k HF bands
// which would otherwise cramp significantly near Nyquist at 44.1/48k.
// ======================================================================
class ChannelStrip {
public:
    void prepare(double sampleRate) {
        sampleRate_ = sampleRate;
        oversampler_.prepare(sampleRate);

        const double osRate = oversampler_.oversampledRate();
        lfShelf_.prepare(osRate);
        hfPeak_.prepare(osRate);

        lfGainSmooth_.prepare(sampleRate);
        hfGainSmooth_.prepare(sampleRate);

        // Initialize to flat
        lfGainSmooth_.snapTo(0.0);
        hfGainSmooth_.snapTo(0.0);

        updateFilters(0.0, 40.0, 0.0, 3000.0);
    }

    void reset() {
        oversampler_.reset();
        lfShelf_.reset();
        hfPeak_.reset();
    }

    int latencySamples() const { return oversampler_.latencySamples(); }

    // Snap smoothers to their current targets and update filters immediately.
    // Used on activation to avoid a gain ramp from 0 to stored value.
    void snapToCurrentParams() {
        lfGainSmooth_.snapToTarget();
        hfGainSmooth_.snapToTarget();
        double lfGain = lfGainSmooth_.current();
        double hfGain = hfGainSmooth_.current();
        updateFilters(lfGain, lfFreq_, hfGain, hfFreq_);
        lastLfGain_ = lfGain;
        lastHfGain_ = hfGain;
        lastLfFreq_ = lfFreq_;
        lastHfFreq_ = hfFreq_;
    }

    void setLFParams(double gainDb, double freqHz) {
        lfGainSmooth_.setTarget(gainDb);
        lfFreq_ = freqHz;
    }

    void setHFParams(double gainDb, double freqHz) {
        hfGainSmooth_.setTarget(gainDb);
        hfFreq_ = freqHz;
    }

    double process(double input) {
        // Advance parameter smoothers
        double lfGain = lfGainSmooth_.next();
        double hfGain = hfGainSmooth_.next();

        // Only recalculate coefficients when parameters have actually changed
        // Threshold of 0.001 dB avoids wasting CPU on transcendentals
        // while keeping zipper-free smoothness
        if (std::abs(lfGain - lastLfGain_) > 0.001 ||
            std::abs(hfGain - lastHfGain_) > 0.001 ||
            lfFreq_ != lastLfFreq_ || hfFreq_ != lastHfFreq_) {
            updateFilters(lfGain, lfFreq_, hfGain, hfFreq_);
            lastLfGain_ = lfGain;
            lastHfGain_ = hfGain;
            lastLfFreq_ = lfFreq_;
            lastHfFreq_ = hfFreq_;
        }

        // Upsample
        double upsampled[OVERSAMPLING_FACTOR];
        oversampler_.upsample(input, upsampled);

        // Process at oversampled rate
        for (int i = 0; i < OVERSAMPLING_FACTOR; ++i) {
            upsampled[i] = lfShelf_.process(upsampled[i]);
            upsampled[i] = hfPeak_.process(upsampled[i]);
        }

        // Downsample
        return oversampler_.downsample(upsampled);
    }

    // Process a block for efficiency
    void processBlock(const double* input, double* output, int numSamples) {
        for (int i = 0; i < numSamples; ++i) {
            output[i] = process(input[i]);
        }
    }

    // Process block with float I/O (for VST3 32-bit mode)
    void processBlock(const float* input, float* output, int numSamples) {
        for (int i = 0; i < numSamples; ++i) {
            output[i] = static_cast<float>(process(static_cast<double>(input[i])));
        }
    }

private:
    void updateFilters(double lfGainDb, double lfFreqHz,
                       double hfGainDb, double hfFreqHz) {
        lfShelf_.setParams(lfFreqHz, lfGainDb);
        hfPeak_.setParams(hfFreqHz, hfGainDb);
    }

    double sampleRate_ = 44100.0;
    double lfFreq_ = 40.0;
    double hfFreq_ = 3000.0;

    // Coefficient update tracking — avoid redundant transcendentals
    double lastLfGain_ = 0.0;
    double lastHfGain_ = 0.0;
    double lastLfFreq_ = 40.0;
    double lastHfFreq_ = 3000.0;

    Oversampler oversampler_;
    ShelfFilter lfShelf_;
    PeakFilter  hfPeak_;

    ParamSmoother lfGainSmooth_;
    ParamSmoother hfGainSmooth_;
};

// ======================================================================
// Stereo processor — two independent channel strips
// ======================================================================
class StereoProcessor {
public:
    void prepare(double sampleRate, int maxBlockSize) {
        (void)maxBlockSize;
        for (auto& ch : channels_)
            ch.prepare(sampleRate);
    }

    void reset() {
        for (auto& ch : channels_)
            ch.reset();
    }

    int latencySamples() const { return channels_[0].latencySamples(); }

    void snapToCurrentParams() {
        for (auto& ch : channels_)
            ch.snapToCurrentParams();
    }

    void setLFParams(double gainDb, int freqIdx) {
        double freq = lfFreqToHz(freqIdx);
        for (auto& ch : channels_)
            ch.setLFParams(gainDb, freq);
    }

    void setHFParams(double gainDb, int freqIdx) {
        double freq = hfFreqToHz(freqIdx);
        for (auto& ch : channels_)
            ch.setHFParams(gainDb, freq);
    }

    // Process stereo 64-bit
    void process(const double* const* inputs, double* const* outputs,
                 int numChannels, int numSamples) {
        int ch = std::min(numChannels, 2);
        for (int c = 0; c < ch; ++c)
            channels_[c].processBlock(inputs[c], outputs[c], numSamples);
    }

    // Process stereo 32-bit
    void process(const float* const* inputs, float* const* outputs,
                 int numChannels, int numSamples) {
        int ch = std::min(numChannels, 2);
        for (int c = 0; c < ch; ++c)
            channels_[c].processBlock(inputs[c], outputs[c], numSamples);
    }

private:
    ChannelStrip channels_[2];
};

} // namespace Langevin
