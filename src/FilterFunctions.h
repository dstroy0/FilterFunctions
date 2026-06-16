/*
  FilterFunctions - Arduino sensor filtering library
  Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
  Licensed under the GNU Affero General Public License v3.0
*/

#ifndef FILTERFUNCTIONS_H
#define FILTERFUNCTIONS_H

#ifdef ARDUINO
#include <Arduino.h>
#else
// Standard C++11 compatibility for host-side builds and testing
#include <cstdint>
#include <cmath>
#include <algorithm>
#ifndef PI
static constexpr float PI = 3.14159265358979323846f;
#endif
// Arduino-style constrain: clamp x to [lo, hi]
template <typename T>
inline T constrain(T x, T lo, T hi) { return x < lo ? lo : (x > hi ? hi : x); }
#endif

class FilterFunctions
{
public:
    FilterFunctions();
    ~FilterFunctions();

    // Simple moving average
    float movingAverage(float input);
    void beginMovingAverage(uint8_t size);

    // Exponential moving average
    float exponentialMovingAverage(float input, float alpha);
    void beginExponentialMovingAverage(float alpha);

    // Median filter
    float medianFilter(float input);
    void beginMedianFilter(uint8_t size);

    // Low-pass filter (one-pole)
    float lowPassFilter(float input, float alpha);
    void beginLowPassFilter(float alpha);

    // High-pass filter (one-pole)
    float highPassFilter(float input, float alpha);
    void beginHighPassFilter(float alpha);

    // Savitzky-Golay filter (optional small window)
    float savitzkyGolay(float input);
    void beginSavitzkyGolay(uint8_t windowSize);

    // Kalman filter
    float kalmanFilter(float input);
    void beginKalmanFilter(float processNoise, float measurementNoise);

    // Complementary filter
    float complementaryFilter(float accelAngle, float gyroRate, float dt);
    void beginComplementaryFilter(float alpha);

    // Biquad low-pass filter
    float biquadLowPass(float input);
    void beginBiquadLowPass(float cutoffHz, float sampleRate, float q = 0.707f);

    // Notch filter
    float notchFilter(float input);
    void beginNotchFilter(float centerHz, float q, float sampleRate);

private:
    // Moving average
    float *_maBuffer;
    uint8_t _maSize;
    uint8_t _maIndex;
    float _maSum;

    // Exponential moving average
    float _emaValue;
    float _emaAlpha;
    bool _emaStarted;

    // Median filter
    float *_medBuffer;
    uint8_t _medSize;
    uint8_t _medIndex;
    bool _medFilled;

    // Low-pass filter
    float _lpValue;
    float _lpAlpha;
    bool _lpStarted;

    // High-pass filter
    float _hpValue;
    float _hpPrev;
    float _hpAlpha;
    bool _hpStarted;

    // Savitzky-Golay
    uint8_t _sgSize;
    float *_sgBuffer;
    uint8_t _sgIndex;
    bool _sgFilled;

    // Kalman filter
    float _kalmanX;
    float _kalmanP;
    float _kalmanQ;
    float _kalmanR;
    bool _kalmanStarted;

    // Complementary filter
    float _compAngle;
    float _compAlpha;
    bool _compStarted;

    // Biquad low-pass filter
    float _biquadB0;
    float _biquadB1;
    float _biquadB2;
    float _biquadA1;
    float _biquadA2;
    float _biquadX1;
    float _biquadX2;
    float _biquadY1;
    float _biquadY2;

    // Notch filter
    float _notchB0;
    float _notchB1;
    float _notchB2;
    float _notchA1;
    float _notchA2;
    float _notchX1;
    float _notchX2;
    float _notchY1;
    float _notchY2;
};

#endif // FILTERFUNCTIONS_H
