// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>

/**
 * @file FilterFunctions.h
 * @brief Arduino sensor filtering library — ten IIR/FIR/statistical filters in one class.
 *
 * Provides a collection of filters suitable for real-time sensor signal conditioning on
 * microcontrollers and host-side (g++ -std=c++11) builds:
 *
 * | Filter                 | Type         | Primary use                          |
 * |------------------------|--------------|--------------------------------------|
 * | Moving Average         | FIR          | General noise reduction              |
 * | Exponential MA         | IIR (1-pole) | Low-memory noise reduction           |
 * | Median                 | Statistical  | Impulse / spike rejection            |
 * | Low-Pass               | IIR (1-pole) | Slow drift tracking                  |
 * | High-Pass              | IIR (1-pole) | DC removal / vibration detection     |
 * | Savitzky-Golay         | FIR (poly)   | Peak-preserving smoothing            |
 * | Kalman                 | IIR (1-D)    | Optimal scalar state estimation      |
 * | Complementary          | IIR          | IMU tilt fusion (accel + gyro)       |
 * | Biquad Low-Pass        | IIR (2-pole) | Sharp, audio-quality cutoff          |
 * | Notch                  | IIR (2-pole) | Narrow band rejection (50/60 Hz …)  |
 *
 * **Platform compatibility** — when compiled without `ARDUINO` defined the header
 * provides C++11 equivalents for `Arduino.h` symbols (`constrain`, `PI`) so every
 * filter compiles and runs identically under a standard toolchain.
 *
 * **Usage pattern** — call `begin*()` once during initialisation, then call the
 * matching filter function each sample period:
 * @code
 * FilterFunctions f;
 * f.beginMovingAverage(8);
 * f.beginKalmanFilter(0.01f, 0.1f);
 *
 * // in loop:
 * float raw = analogRead(A0) * (5.0f / 1023.0f);
 * float smooth = f.movingAverage(raw);
 * float est    = f.kalmanFilter(raw);
 * @endcode
 *
 * All ten filters share one `FilterFunctions` instance and maintain independent state.
 * Dynamic memory (circular buffers for MA, Median, SG) is allocated by `begin*()` and
 * released by the destructor.
 */

#ifndef FILTERFUNCTIONS_H
#define FILTERFUNCTIONS_H

#ifdef ARDUINO
#include <Arduino.h>
#else
// Standard C++11 compatibility for host-side builds and unit testing.
#include <cstdint>
#include <cmath>
#include <algorithm>
#ifndef PI
static constexpr float PI = 3.14159265358979323846f; ///< π, matches Arduino's value.
#endif
/**
 * @brief Arduino-style value clamping — identical to Arduino's constrain() macro.
 * @tparam T Arithmetic type.
 * @param x  Value to clamp.
 * @param lo Lower bound (inclusive).
 * @param hi Upper bound (inclusive).
 * @return   x clamped to [lo, hi].
 */
template <typename T>
inline T constrain(T x, T lo, T hi) { return x < lo ? lo : (x > hi ? hi : x); }
#endif

/**
 * @brief Ten-filter signal-processing toolkit for Arduino sensor data.
 *
 * Each filter maintains its own state so all ten may run concurrently on a single
 * instance.  Heap allocation occurs only inside `begin*()` calls that need a circular
 * buffer (Moving Average, Median, Savitzky-Golay); all other filters are constant-size.
 *
 * @note Only one instance of each filter type is supported per `FilterFunctions` object.
 *       Instantiate multiple objects if parallel independent filter chains are required.
 */
class FilterFunctions
{
public:
    FilterFunctions();
    ~FilterFunctions();

    // -----------------------------------------------------------------------
    /** @name Moving Average
     *  Arithmetic mean of the last @p size samples via a circular buffer.
     *  @{
     */

    /**
     * @brief Initialise (or reinitialise) the moving-average filter.
     *
     * Allocates a circular buffer of @p size floats, zeroed on entry.  The buffer
     * is also zeroed, so the first @p size outputs will be biased toward zero until
     * the window is full.
     *
     * @param[in] size Window length in samples; clamped to a minimum of 1.
     */
    void beginMovingAverage(uint8_t size);

    /**
     * @brief Push one sample and return the current moving average.
     *
     * Runs in O(1): subtracts the oldest sample, adds the new one, divides by @p size.
     *
     * @param[in] input New sample value.
     * @return Mean of the last @p size samples.
     */
    float movingAverage(float input);

    /** @} */

    // -----------------------------------------------------------------------
    /** @name Exponential Moving Average
     *  Single-accumulator IIR low-pass with exponential weighting.
     *  @{
     */

    /**
     * @brief Initialise the exponential moving average and store a default alpha.
     *
     * @param[in] alpha Smoothing factor ∈ [0, 1].  Stored as default; may be
     *                  overridden per call via exponentialMovingAverage().
     */
    void beginExponentialMovingAverage(float alpha);

    /**
     * @brief Push one sample and return the current EMA.
     *
     * Formula: `EMA = alpha * input + (1 - alpha) * EMA`
     *
     * The first call seeds the accumulator to @p input (no transient spike).
     * The @p alpha argument overrides the value set in beginExponentialMovingAverage(),
     * allowing dynamic alpha changes without calling begin again.
     *
     * @param[in] input New sample value.
     * @param[in] alpha Smoothing factor ∈ [0, 1].  Values near 0 give heavy smoothing;
     *                  values near 1 give minimal smoothing (output ≈ input).
     * @return Current EMA.
     */
    float exponentialMovingAverage(float input, float alpha);

    /** @} */

    // -----------------------------------------------------------------------
    /** @name Median Filter
     *  Returns the middle value of a sliding window — optimal impulse-noise rejection.
     *  @{
     */

    /**
     * @brief Initialise (or reinitialise) the median filter.
     *
     * Allocates a circular buffer of @p size floats, zeroed on entry.
     *
     * @param[in] size Window length; clamped to a minimum of 1 and a maximum of 16.
     */
    void beginMedianFilter(uint8_t size);

    /**
     * @brief Push one sample and return the current median.
     *
     * Copies the window into a 16-element stack buffer, sorts it with an
     * O(n²) insertion sort (acceptable for n ≤ 16), and returns the middle element.
     * During warmup (before the window fills) only the samples received so far are
     * considered.
     *
     * @param[in] input New sample value.
     * @return Median of the current window.
     */
    float medianFilter(float input);

    /** @} */

    // -----------------------------------------------------------------------
    /** @name Low-Pass Filter
     *  Single-pole IIR low-pass (RC equivalent) — minimal memory, fast.
     *  @{
     */

    /**
     * @brief Initialise the one-pole low-pass filter and store a default alpha.
     *
     * @param[in] alpha Smoothing coefficient ∈ [0, 1].
     */
    void beginLowPassFilter(float alpha);

    /**
     * @brief Push one sample and return the filtered output.
     *
     * Formula: `y[n] = y[n-1] + alpha * (x[n] - y[n-1])`
     *
     * Approximate -3 dB cutoff: `f_c ≈ (alpha * f_s) / (2π)`
     *
     * The first call seeds the output to @p input.  The @p alpha argument overrides
     * the stored value, permitting dynamic cutoff adjustment.
     *
     * @param[in] input New sample value.
     * @param[in] alpha Smoothing coefficient ∈ [0, 1].  Smaller values give heavier
     *                  smoothing and a lower cutoff frequency.
     * @return Low-pass filtered output.
     */
    float lowPassFilter(float input, float alpha);

    /** @} */

    // -----------------------------------------------------------------------
    /** @name High-Pass Filter
     *  Single-pole IIR high-pass — removes DC offset and slow drift.
     *  @{
     */

    /**
     * @brief Initialise the one-pole high-pass filter and store a default alpha.
     *
     * @param[in] alpha Filter coefficient ∈ [0, 1]; typically 0.9–0.99.
     */
    void beginHighPassFilter(float alpha);

    /**
     * @brief Push one sample and return the filtered output.
     *
     * Formula: `y[n] = alpha * (y[n-1] + x[n] - x[n-1])`
     *
     * Approximate -3 dB cutoff: `f_c ≈ f_s * (1 - alpha) / (2π)`
     *
     * The first call seeds both state registers to @p input.  The @p alpha argument
     * overrides the stored value.
     *
     * @param[in] input New sample value.
     * @param[in] alpha Filter coefficient ∈ [0, 1].  Values close to 1 pass a wider
     *                  band above DC; values close to 0 pass only very high frequencies.
     * @return High-pass filtered output.
     */
    float highPassFilter(float input, float alpha);

    /** @} */

    // -----------------------------------------------------------------------
    /** @name Savitzky-Golay Filter
     *  Centered polynomial smoothing — preserves peak height and linear trends.
     *  @{
     */

    /**
     * @brief Initialise (or reinitialise) the Savitzky-Golay filter.
     *
     * Allocates a circular buffer of @p windowSize floats, zeroed on entry.
     * The window is forced to the nearest supported odd value ≥ 3:
     *
     * | Requested | Used | Lag      |
     * |-----------|------|----------|
     * | 3         | 3    | 1 sample |
     * | 4         | 5    | 2 samples|
     * | 5         | 5    | 2 samples|
     * | 6         | 7    | 3 samples|
     * | 7         | 7    | 3 samples|
     * | > 7       | 7    | 3 samples|
     *
     * @param[in] windowSize Desired window; clamped to [3, 7] and forced odd.
     */
    void beginSavitzkyGolay(uint8_t windowSize);

    /**
     * @brief Push one sample and return the smoothed output.
     *
     * Applies degree-2/3 polynomial least-squares convolution coefficients from
     * Savitzky & Golay (1964), Table III.  The output corresponds to the sample
     * that arrived `(windowSize - 1) / 2` calls ago (centered filter).
     *
     * Coefficients used:
     * - Window 3: `[ 1,  1,  1] / 3`
     * - Window 5: `[-3, 12, 17, 12, -3] / 35`
     * - Window 7: `[-2,  3,  6,  7,  6,  3, -2] / 21`
     *
     * During warmup (fewer than @p windowSize samples received) a simple moving
     * average over the samples seen so far is returned.
     *
     * @param[in] input New sample value.
     * @return Smoothed estimate of the center sample.
     *
     * @note DC gain is exactly 1 and linear ramps are reproduced without error.
     *       Quadratic peaks are attenuated slightly less than a plain moving average.
     */
    float savitzkyGolay(float input);

    /** @} */

    // -----------------------------------------------------------------------
    /** @name Kalman Filter
     *  Scalar (1-D) discrete Kalman filter — optimal linear state estimation.
     *  @{
     */

    /**
     * @brief Initialise the Kalman filter with process and measurement noise.
     *
     * Resets the error covariance to 1.0 and clears the started flag so the first
     * measurement seeds the state without a transient.
     *
     * @param[in] processNoise     Q — expected variance of state change per sample
     *                             step.  Larger Q lets the estimate track faster
     *                             changes but reduces smoothing.
     * @param[in] measurementNoise R — expected variance of sensor readings.  Larger R
     *                             trusts measurements less (more smoothing).
     */
    void beginKalmanFilter(float processNoise, float measurementNoise);

    /**
     * @brief Push one measurement and return the state estimate.
     *
     * Predict–update cycle:
     * @code
     * P = P + Q                   // covariance prediction
     * K = P / (P + R)             // Kalman gain
     * x = x + K * (z - x)        // state update
     * P = (1 - K) * P             // covariance update
     * @endcode
     *
     * The first call seeds the state to @p input.
     *
     * @param[in] input New measurement.
     * @return Filtered state estimate.
     */
    float kalmanFilter(float input);

    /** @} */

    // -----------------------------------------------------------------------
    /** @name Complementary Filter
     *  Sensor-fusion filter for IMU tilt estimation (accelerometer + gyroscope).
     *  @{
     */

    /**
     * @brief Initialise the complementary filter and store the fusion weight.
     *
     * @param[in] alpha Gyro trust weight ∈ [0, 1]; typically 0.95–0.99.
     *                  Higher values favour the gyro (better short-term tracking);
     *                  lower values correct drift faster via the accelerometer.
     */
    void beginComplementaryFilter(float alpha);

    /**
     * @brief Push one IMU sample pair and return the fused angle estimate.
     *
     * Formula: `angle = alpha * (angle + gyroRate * dt) + (1 - alpha) * accelAngle`
     *
     * The first call seeds the angle to @p accelAngle.
     *
     * @param[in] accelAngle Tilt angle derived from the accelerometer (degrees).
     * @param[in] gyroRate   Angular rate from the gyroscope (degrees / second).
     * @param[in] dt         Time elapsed since the last call (seconds); must match
     *                       the actual loop period for accurate integration.
     * @return Fused tilt angle estimate (degrees).
     *
     * @note Keep @p dt consistent with the real loop timing.  A constant value is
     *       fine for a fixed-period loop; use elapsed millis() for variable timing.
     */
    float complementaryFilter(float accelAngle, float gyroRate, float dt);

    /** @} */

    // -----------------------------------------------------------------------
    /** @name Biquad Low-Pass Filter
     *  Second-order IIR low-pass — -40 dB/decade rolloff, RBJ Audio EQ Cookbook.
     *  @{
     */

    /**
     * @brief Compute and store biquad low-pass coefficients for the given parameters.
     *
     * Uses the Robert Bristow-Johnson (RBJ) Audio EQ Cookbook low-pass formulation,
     * implemented as Direct Form I.  Resets all four delay registers to zero.
     *
     * @param[in] cutoffHz   -3 dB cutoff frequency in Hz.
     * @param[in] sampleRate Sample rate in Hz; must match the actual loop rate.
     * @param[in] q          Quality factor (default 0.707 = Butterworth, maximally
     *                       flat passband).  Values above 1 produce a resonant peak
     *                       just below the cutoff.
     *
     * @note Call this function again if the sample rate changes; state is reset.
     */
    void beginBiquadLowPass(float cutoffHz, float sampleRate, float q = 0.707f);

    /**
     * @brief Push one sample through the biquad low-pass and return the output.
     *
     * Direct Form I difference equation:
     * `y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]`
     *
     * @param[in] input New sample value.
     * @return Filtered output.
     */
    float biquadLowPass(float input);

    /** @} */

    // -----------------------------------------------------------------------
    /** @name Notch Filter
     *  Second-order IIR band-reject — rejects a narrow frequency band.
     *  @{
     */

    /**
     * @brief Compute and store notch filter coefficients for the given parameters.
     *
     * Uses the RBJ Audio EQ Cookbook band-reject formulation, Direct Form I.
     * Resets all four delay registers to zero.
     *
     * @param[in] centerHz   Center (notch) frequency to reject, in Hz.
     * @param[in] q          Quality factor controlling notch width.  Higher Q gives
     *                       a narrower notch and a longer settling time
     *                       (τ ≈ Q / (π · centerHz) seconds).
     * @param[in] sampleRate Sample rate in Hz; must match the actual loop rate.
     *
     * @note For Q = 10 at 60 Hz the settling time constant is ≈ 53 ms.  Allow
     *       approximately 5τ (265 ms / 265 samples at 1 kHz) before evaluating
     *       steady-state attenuation.
     */
    void beginNotchFilter(float centerHz, float q, float sampleRate);

    /**
     * @brief Push one sample through the notch filter and return the output.
     *
     * Direct Form I difference equation (same structure as biquadLowPass() but
     * with band-reject b coefficients):
     * `y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]`
     *
     * @param[in] input New sample value.
     * @return Filtered output with the notch frequency attenuated.
     */
    float notchFilter(float input);

    /** @} */

private:
    // Moving average
    float   *_maBuffer; ///< Circular sample buffer.
    uint8_t  _maSize;   ///< Window length.
    uint8_t  _maIndex;  ///< Next write position.
    float    _maSum;    ///< Running sum of buffer contents.

    // Exponential moving average
    float _emaValue;   ///< Current accumulator value.
    float _emaAlpha;   ///< Stored smoothing factor.
    bool  _emaStarted; ///< False until the first sample seeds the accumulator.

    // Median filter
    float   *_medBuffer; ///< Circular sample buffer.
    uint8_t  _medSize;   ///< Window length.
    uint8_t  _medIndex;  ///< Next write position.
    bool     _medFilled; ///< True once the buffer has been filled once.

    // Low-pass filter
    float _lpValue;   ///< Current filtered output.
    float _lpAlpha;   ///< Stored smoothing coefficient.
    bool  _lpStarted; ///< False until the first sample seeds the state.

    // High-pass filter
    float _hpValue;   ///< Previous filtered output y[n-1].
    float _hpPrev;    ///< Previous raw input x[n-1].
    float _hpAlpha;   ///< Stored filter coefficient.
    bool  _hpStarted; ///< False until the first sample seeds the state registers.

    // Savitzky-Golay
    uint8_t  _sgSize;   ///< Active window length (3, 5, or 7).
    float   *_sgBuffer; ///< Circular sample buffer (oldest→newest via _sgIndex).
    uint8_t  _sgIndex;  ///< Points to oldest slot (= next write position after wrap).
    bool     _sgFilled; ///< True once the buffer has been filled once.

    // Kalman filter
    float _kalmanX;       ///< State estimate x̂.
    float _kalmanP;       ///< Error covariance P.
    float _kalmanQ;       ///< Process noise variance Q.
    float _kalmanR;       ///< Measurement noise variance R.
    bool  _kalmanStarted; ///< False until the first measurement seeds the state.

    // Complementary filter
    float _compAngle;   ///< Fused angle estimate (degrees).
    float _compAlpha;   ///< Gyro trust weight α.
    bool  _compStarted; ///< False until the first call seeds the angle.

    // Biquad low-pass filter — Direct Form I coefficients and delay registers.
    float _biquadB0; ///< Feed-forward coefficient b0.
    float _biquadB1; ///< Feed-forward coefficient b1.
    float _biquadB2; ///< Feed-forward coefficient b2.
    float _biquadA1; ///< Feedback coefficient a1 (negated in difference equation).
    float _biquadA2; ///< Feedback coefficient a2 (negated in difference equation).
    float _biquadX1; ///< Input delay x[n-1].
    float _biquadX2; ///< Input delay x[n-2].
    float _biquadY1; ///< Output delay y[n-1].
    float _biquadY2; ///< Output delay y[n-2].

    // Notch filter — Direct Form I coefficients and delay registers.
    float _notchB0; ///< Feed-forward coefficient b0.
    float _notchB1; ///< Feed-forward coefficient b1.
    float _notchB2; ///< Feed-forward coefficient b2.
    float _notchA1; ///< Feedback coefficient a1 (negated in difference equation).
    float _notchA2; ///< Feedback coefficient a2 (negated in difference equation).
    float _notchX1; ///< Input delay x[n-1].
    float _notchX2; ///< Input delay x[n-2].
    float _notchY1; ///< Output delay y[n-1].
    float _notchY2; ///< Output delay y[n-2].
};

#endif // FILTERFUNCTIONS_H
