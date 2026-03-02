// ==========================================================================
// LangevinDSP.h — Langevin EQ-251A analog-modeled DSP engine
//
// No-compromise implementation:
//   • 64-bit double-precision throughout
//   • 8× oversampling with least-squares optimal FIR anti-aliasing
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
static constexpr int    OVERSAMPLING_FACTOR = 8;
static constexpr int    FIR_TAPS = 384;      // linear-phase anti-alias FIR
static constexpr double PI = 3.14159265358979323846;
static constexpr double TWO_PI = 2.0 * PI;

// ======================================================================
// Oversampler — 8× with linear-phase FIR anti-aliasing
//
// Uses a least-squares optimal FIR design (384 taps).
// Minimizes total squared error across passband and stopband,
// deliberately ignoring the transition band. This produces the
// smoothest possible passband (monotonic rolloff, <0.01 dB ripple)
// at the cost of slightly reduced stopband rejection (~-80 dB vs
// ~-100 dB for Kaiser). At 8× oversampling the stopband tradeoff
// is inaudible — alias products at -80 dB are 0.01% of signal.
//
// Design method: weighted least-squares for Type II (even-length)
// symmetric FIR. Solved via Cholesky decomposition of the 192×192
// Gram matrix with closed-form band integrals. Runs once in prepare().
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
        return (FIR_TAPS - 1) / OVERSAMPLING_FACTOR;
    }

    // Upsample: insert one input sample, get OVERSAMPLING_FACTOR output samples
    void upsample(double input, double* output) {
        for (int i = 0; i < OVERSAMPLING_FACTOR; ++i) {
            upBuffer_[upPos_] = (i == 0) ? input * OVERSAMPLING_FACTOR : 0.0;
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
        // Least-squares optimal FIR lowpass for Type II (even length N)
        //
        // Band specification (angular frequency, 0 to π):
        //   Passband: [0, ωp]  desired = 1
        //   Stopband: [ωs, π]  desired = 0
        //   Transition: [ωp, ωs] — excluded from optimization (don't-care)
        //
        // ωp = π/8 (original Nyquist at 8× oversampling)
        // ωs = ωp × 1.35 (wider transition than Kaiser → smoother passband)

        const int M = FIR_TAPS / 2;  // 192 half-coefficients
        const double wp = PI / OVERSAMPLING_FACTOR;  // passband edge
        const double ws = wp * 1.35;                  // stopband edge
        const double Ws = 1.0;  // stopband weight (equal to passband)

        // --- Allocate working memory ---
        // Using heap to avoid 300+ KB on stack
        double* Q = new double[M * M]();
        double* L = new double[M * M]();
        double* d = new double[M]();
        double* y = new double[M]();
        double* b = new double[M]();

        // --- Helper: ∫₀^ω cos((i+0.5)t) · cos((j+0.5)t) dt ---
        // Uses product-to-sum identity for closed-form evaluation
        auto cosIntegral = [](int i, int j, double omega) -> double {
            double ai = i + 0.5;
            double aj = j + 0.5;
            if (i == j) {
                return 0.5 * (omega + std::sin(2.0 * ai * omega) / (2.0 * ai));
            } else {
                double diff = ai - aj;  // = i - j (integer)
                double sum  = ai + aj;  // = i + j + 1 (integer)
                return 0.5 * (std::sin(diff * omega) / diff
                            + std::sin(sum * omega) / sum);
            }
        };

        // --- Build Gram matrix Q and RHS vector d ---
        // Q[i][j] = ∫_passband φi·φj dω + Ws · ∫_stopband φi·φj dω
        // d[i]    = ∫_passband φi dω
        // where φk(ω) = cos((k+0.5)ω)
        //
        // Key identity: ∫₀^π cos((i+0.5)ω)cos((j+0.5)ω) dω = (π/2)·δ(i,j)
        // So stopband integral [ωs,π] = (π/2)·δ(i,j) - ∫₀^{ωs} φi·φj dω

        for (int i = 0; i < M; ++i) {
            // RHS
            d[i] = std::sin((i + 0.5) * wp) / (i + 0.5);

            for (int j = i; j < M; ++j) {
                // Passband [0, ωp]
                double Ip = cosIntegral(i, j, wp);

                // Stopband [ωs, π] = orthogonality - [0, ωs]
                double Is;
                if (i == j)
                    Is = PI * 0.5 - cosIntegral(i, j, ws);
                else
                    Is = -cosIntegral(i, j, ws);

                double val = Ip + Ws * Is;
                Q[i * M + j] = val;
                Q[j * M + i] = val;  // symmetric
            }
        }

        // --- Cholesky decomposition: Q = L · Lᵀ ---
        for (int i = 0; i < M; ++i) {
            for (int j = 0; j <= i; ++j) {
                double s = Q[i * M + j];
                for (int k = 0; k < j; ++k)
                    s -= L[i * M + k] * L[j * M + k];
                if (i == j)
                    L[i * M + j] = std::sqrt(std::max(1e-30, s));
                else
                    L[i * M + j] = s / L[j * M + j];
            }
        }

        // --- Forward substitution: L · y = d ---
        for (int i = 0; i < M; ++i) {
            double s = d[i];
            for (int k = 0; k < i; ++k)
                s -= L[i * M + k] * y[k];
            y[i] = s / L[i * M + i];
        }

        // --- Back substitution: Lᵀ · b = y ---
        for (int i = M - 1; i >= 0; --i) {
            double s = y[i];
            for (int k = i + 1; k < M; ++k)
                s -= L[k * M + i] * b[k];
            b[i] = s / L[i * M + i];
        }

        // --- Convert half-coefficients b[k] to full symmetric FIR ---
        // Type II: H(ω) = Σ b[k]·cos((k+0.5)ω)
        // h[M-1-k] = b[k]/2,  h[M+k] = b[k]/2
        for (int k = 0; k < M; ++k) {
            firCoeffs_[M - 1 - k] = b[k] * 0.5;
            firCoeffs_[M + k]     = b[k] * 0.5;
        }

        // --- Normalize for unity gain at DC ---
        double dcSum = 0.0;
        for (int n = 0; n < FIR_TAPS; ++n)
            dcSum += firCoeffs_[n];
        if (std::abs(dcSum) > 1e-20) {
            for (int n = 0; n < FIR_TAPS; ++n)
                firCoeffs_[n] /= dcSum;
        }

        // --- Clean up ---
        delete[] Q;
        delete[] L;
        delete[] d;
        delete[] y;
        delete[] b;
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
// Processing mode
// ======================================================================
enum class ProcessMode : int {
    Stereo = 0,   // Both channels receive identical EQ (linked)
    MidSide = 1   // L/R encoded to M/S, processed independently, decoded
};

// ======================================================================
// Stereo processor — two independent channel strips
//
// In Stereo mode: both strips receive the same parameters.
// In Mid/Side mode: L/R is encoded to M/S before processing.
//   Strip 0 processes Mid, Strip 1 processes Side.
//   Each has independent EQ parameters.
//   After processing, M/S is decoded back to L/R.
//
// M/S encoding is mathematically lossless:
//   Encode: M = (L + R) × 0.5,  S = (L − R) × 0.5
//   Decode: L = M + S,  R = M − S
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

    void setProcessMode(ProcessMode mode) { mode_ = mode; }
    ProcessMode processMode() const { return mode_; }

    void snapToCurrentParams() {
        for (auto& ch : channels_)
            ch.snapToCurrentParams();
    }

    // Set parameters for main channel (both channels in Stereo, Mid in M/S)
    void setLFParams(double gainDb, int freqIdx) {
        double freq = lfFreqToHz(freqIdx);
        channels_[0].setLFParams(gainDb, freq);
        if (mode_ == ProcessMode::Stereo)
            channels_[1].setLFParams(gainDb, freq);
    }

    void setHFParams(double gainDb, int freqIdx) {
        double freq = hfFreqToHz(freqIdx);
        channels_[0].setHFParams(gainDb, freq);
        if (mode_ == ProcessMode::Stereo)
            channels_[1].setHFParams(gainDb, freq);
    }

    // Set parameters for Side channel (only used in M/S mode)
    void setSideLFParams(double gainDb, int freqIdx) {
        if (mode_ == ProcessMode::MidSide) {
            double freq = lfFreqToHz(freqIdx);
            channels_[1].setLFParams(gainDb, freq);
        }
    }

    void setSideHFParams(double gainDb, int freqIdx) {
        if (mode_ == ProcessMode::MidSide) {
            double freq = hfFreqToHz(freqIdx);
            channels_[1].setHFParams(gainDb, freq);
        }
    }

    // Direct channel access — bypasses mode checks entirely
    void setChannelLFParams(int channel, double gainDb, int freqIdx) {
        double freq = lfFreqToHz(freqIdx);
        channels_[channel].setLFParams(gainDb, freq);
    }

    void setChannelHFParams(int channel, double gainDb, int freqIdx) {
        double freq = hfFreqToHz(freqIdx);
        channels_[channel].setHFParams(gainDb, freq);
    }

    // Direct mono processing — uses channel strip 0, no M/S encoding
    double processMonoSample(double input) {
        return channels_[0].process(input);
    }

    // Process stereo 64-bit
    void process(const double* const* inputs, double* const* outputs,
                 int numChannels, int numSamples) {
        if (numChannels < 2 || mode_ == ProcessMode::Stereo) {
            // Stereo linked or mono: process each channel independently
            int ch = std::min(numChannels, 2);
            for (int c = 0; c < ch; ++c)
                channels_[c].processBlock(inputs[c], outputs[c], numSamples);
        } else {
            // Mid/Side processing
            processMidSide(inputs, outputs, numSamples);
        }
    }

    // Process stereo 32-bit
    void process(const float* const* inputs, float* const* outputs,
                 int numChannels, int numSamples) {
        if (numChannels < 2 || mode_ == ProcessMode::Stereo) {
            int ch = std::min(numChannels, 2);
            for (int c = 0; c < ch; ++c)
                channels_[c].processBlock(inputs[c], outputs[c], numSamples);
        } else {
            processMidSide(inputs, outputs, numSamples);
        }
    }

private:
    // M/S processing — sample-by-sample for perfect accuracy
    void processMidSide(const double* const* inputs, double* const* outputs,
                        int numSamples) {
        for (int i = 0; i < numSamples; ++i) {
            // Encode: L/R → M/S
            double mid  = (inputs[0][i] + inputs[1][i]) * 0.5;
            double side = (inputs[0][i] - inputs[1][i]) * 0.5;

            // Process independently
            mid  = channels_[0].process(mid);
            side = channels_[1].process(side);

            // Decode: M/S → L/R
            outputs[0][i] = mid + side;
            outputs[1][i] = mid - side;
        }
    }

    void processMidSide(const float* const* inputs, float* const* outputs,
                        int numSamples) {
        for (int i = 0; i < numSamples; ++i) {
            double mid  = ((double)inputs[0][i] + (double)inputs[1][i]) * 0.5;
            double side = ((double)inputs[0][i] - (double)inputs[1][i]) * 0.5;

            mid  = channels_[0].process(mid);
            side = channels_[1].process(side);

            outputs[0][i] = (float)(mid + side);
            outputs[1][i] = (float)(mid - side);
        }
    }

    ProcessMode mode_ = ProcessMode::Stereo;
    ChannelStrip channels_[2];
};

} // namespace Langevin
