/*
  FilterFunctions - Arduino sensor filtering library
  Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
  Licensed under the GNU Affero General Public License v3.0
*/

#include "FilterFunctions.h"

#ifndef ARDUINO
using std::max;
using std::sin;
using std::cos;
#endif

FilterFunctions::FilterFunctions()
    : _maBuffer(nullptr), _maSize(0), _maIndex(0), _maSum(0.0f),
      _emaValue(0.0f), _emaAlpha(0.0f), _emaStarted(false),
      _medBuffer(nullptr), _medSize(0), _medIndex(0), _medFilled(false),
      _lpValue(0.0f), _lpAlpha(0.0f), _lpStarted(false),
      _hpValue(0.0f), _hpPrev(0.0f), _hpAlpha(0.0f), _hpStarted(false),
      _sgSize(0), _sgBuffer(nullptr), _sgIndex(0), _sgFilled(false),
      _kalmanX(0.0f), _kalmanP(1.0f), _kalmanQ(0.001f), _kalmanR(0.1f), _kalmanStarted(false),
      _compAngle(0.0f), _compAlpha(0.98f), _compStarted(false),
      _biquadB0(0.0f), _biquadB1(0.0f), _biquadB2(0.0f), _biquadA1(0.0f), _biquadA2(0.0f),
      _biquadX1(0.0f), _biquadX2(0.0f), _biquadY1(0.0f), _biquadY2(0.0f),
      _notchB0(0.0f), _notchB1(0.0f), _notchB2(0.0f), _notchA1(0.0f), _notchA2(0.0f),
      _notchX1(0.0f), _notchX2(0.0f), _notchY1(0.0f), _notchY2(0.0f)
{
}

FilterFunctions::~FilterFunctions()
{
    if (_maBuffer)
        delete[] _maBuffer;
    if (_medBuffer)
        delete[] _medBuffer;
    if (_sgBuffer)
        delete[] _sgBuffer;
}

void FilterFunctions::beginMovingAverage(uint8_t size)
{
    if (size == 0)
        size = 1;
    if (_maBuffer)
        delete[] _maBuffer;
    _maBuffer = new float[size];
    _maSize = size;
    _maIndex = 0;
    _maSum = 0.0f;
    for (uint8_t i = 0; i < _maSize; ++i)
    {
        _maBuffer[i] = 0.0f;
    }
}

float FilterFunctions::movingAverage(float input)
{
    if (_maSize == 0)
        return input;
    _maSum -= _maBuffer[_maIndex];
    _maBuffer[_maIndex] = input;
    _maSum += input;
    _maIndex = (_maIndex + 1) % _maSize;
    return _maSum / _maSize;
}

void FilterFunctions::beginExponentialMovingAverage(float alpha)
{
    _emaAlpha = constrain(alpha, 0.0f, 1.0f);
    _emaStarted = false;
}

float FilterFunctions::exponentialMovingAverage(float input, float alpha)
{
    if (!_emaStarted)
    {
        _emaValue = input;
        _emaStarted = true;
    }
    _emaAlpha = constrain(alpha, 0.0f, 1.0f);
    _emaValue = (_emaAlpha * input) + ((1.0f - _emaAlpha) * _emaValue);
    return _emaValue;
}

void FilterFunctions::beginMedianFilter(uint8_t size)
{
    if (size == 0)
        size = 1;
    if (_medBuffer)
        delete[] _medBuffer;
    _medBuffer = new float[size];
    _medSize = size;
    _medIndex = 0;
    _medFilled = false;
    for (uint8_t i = 0; i < _medSize; ++i)
    {
        _medBuffer[i] = 0.0f;
    }
}

float FilterFunctions::medianFilter(float input)
{
    if (_medSize == 0)
        return input;
    _medBuffer[_medIndex++] = input;
    if (_medIndex >= _medSize)
    {
        _medIndex = 0;
        _medFilled = true;
    }
    uint8_t count = _medFilled ? _medSize : _medIndex;
    float temp[16];
    if (count > sizeof(temp) / sizeof(temp[0]))
    {
        count = sizeof(temp) / sizeof(temp[0]);
    }
    for (uint8_t i = 0; i < count; ++i)
    {
        temp[i] = _medBuffer[i];
    }
    for (uint8_t i = 0; i < count - 1; ++i)
    {
        for (uint8_t j = i + 1; j < count; ++j)
        {
            if (temp[j] < temp[i])
            {
                float tmp = temp[i];
                temp[i] = temp[j];
                temp[j] = tmp;
            }
        }
    }
    return temp[count / 2];
}

void FilterFunctions::beginLowPassFilter(float alpha)
{
    _lpAlpha = constrain(alpha, 0.0f, 1.0f);
    _lpStarted = false;
}

float FilterFunctions::lowPassFilter(float input, float alpha)
{
    if (!_lpStarted)
    {
        _lpValue = input;
        _lpStarted = true;
    }
    _lpAlpha = constrain(alpha, 0.0f, 1.0f);
    _lpValue += _lpAlpha * (input - _lpValue);
    return _lpValue;
}

void FilterFunctions::beginHighPassFilter(float alpha)
{
    _hpAlpha = constrain(alpha, 0.0f, 1.0f);
    _hpStarted = false;
}

float FilterFunctions::highPassFilter(float input, float alpha)
{
    if (!_hpStarted)
    {
        _hpValue = input;
        _hpPrev = input;
        _hpStarted = true;
    }
    _hpAlpha = constrain(alpha, 0.0f, 1.0f);
    float filtered = _hpAlpha * (_hpValue + input - _hpPrev);
    _hpPrev = input;
    _hpValue = filtered;
    return filtered;
}

// Savitzky-Golay quadratic smoothing coefficients (centered, degree 2/3 polynomial).
// Derived by least-squares fit of a quadratic through the symmetric window; coefficients
// are symmetric so degree-3 and degree-2 smoothing are identical.
// DC gain = 1 (Σ coefficients = 1) and linear trends are preserved exactly.
// Source: Savitzky & Golay (1964) Analytical Chemistry, Table III.
//
// The filter is centered: feeding sample x[n] returns smooth(x[n − m]) where m = (size−1)/2.
// During the warmup period (< size samples received), a simple moving average is returned.
static const float _sg_c3[3] = { 1.0f/3.0f,  1.0f/3.0f,  1.0f/3.0f };
static const float _sg_c5[5] = { -3.0f/35.0f, 12.0f/35.0f, 17.0f/35.0f, 12.0f/35.0f, -3.0f/35.0f };
static const float _sg_c7[7] = { -2.0f/21.0f,  3.0f/21.0f,  6.0f/21.0f,  7.0f/21.0f,
                                   6.0f/21.0f,  3.0f/21.0f, -2.0f/21.0f };

void FilterFunctions::beginSavitzkyGolay(uint8_t windowSize)
{
    if (windowSize < 3)
        windowSize = 3;
    if (windowSize > 7)
        windowSize = 7;
    if (windowSize % 2 == 0)
        windowSize += 1;     // must be odd
    if (_sgBuffer)
        delete[] _sgBuffer;
    _sgSize = windowSize;
    _sgBuffer = new float[_sgSize];
    _sgIndex = 0;
    _sgFilled = false;
    for (uint8_t i = 0; i < _sgSize; ++i)
        _sgBuffer[i] = 0.0f;
}

float FilterFunctions::savitzkyGolay(float input)
{
    if (_sgSize == 0)
        return input;

    _sgBuffer[_sgIndex] = input;
    _sgIndex = (_sgIndex + 1) % _sgSize;

    if (_sgIndex == 0)
        _sgFilled = true;

    if (!_sgFilled)
    {
        // Warmup: return simple moving average of samples received so far.
        float sum = 0.0f;
        for (uint8_t i = 0; i < _sgIndex; ++i)
            sum += _sgBuffer[i];
        return sum / _sgIndex;
    }

    // Select the coefficient table for the current window size.
    const float *c = (_sgSize == 3) ? _sg_c3 : (_sgSize == 5) ? _sg_c5 : _sg_c7;

    // Apply: _sgIndex now points at the oldest sample in the circular buffer.
    // Iterate oldest→newest, multiply by c[0]→c[size-1].
    float out = 0.0f;
    for (uint8_t i = 0; i < _sgSize; ++i)
        out += c[i] * _sgBuffer[(_sgIndex + i) % _sgSize];

    return out;
}

void FilterFunctions::beginKalmanFilter(float processNoise, float measurementNoise)
{
    _kalmanQ = max(processNoise, 0.0f);
    _kalmanR = max(measurementNoise, 0.0f);
    _kalmanP = 1.0f;
    _kalmanStarted = false;
}

float FilterFunctions::kalmanFilter(float input)
{
    if (!_kalmanStarted)
    {
        _kalmanX = input;
        _kalmanP = 1.0f;
        _kalmanStarted = true;
    }
    _kalmanP += _kalmanQ;
    float k = _kalmanP / (_kalmanP + _kalmanR);
    _kalmanX += k * (input - _kalmanX);
    _kalmanP *= (1.0f - k);
    return _kalmanX;
}

void FilterFunctions::beginComplementaryFilter(float alpha)
{
    _compAlpha = constrain(alpha, 0.0f, 1.0f);
    _compStarted = false;
}

float FilterFunctions::complementaryFilter(float accelAngle, float gyroRate, float dt)
{
    if (!_compStarted)
    {
        _compAngle = accelAngle;
        _compStarted = true;
    }
    _compAngle = _compAlpha * (_compAngle + gyroRate * dt) + (1.0f - _compAlpha) * accelAngle;
    return _compAngle;
}

void FilterFunctions::beginBiquadLowPass(float cutoffHz, float sampleRate, float q)
{
    if (sampleRate <= 0.0f)
        sampleRate = 1.0f;
    if (cutoffHz <= 0.0f)
        cutoffHz = 0.1f;
    q = max(q, 0.001f);
    float omega = 2.0f * PI * cutoffHz / sampleRate;
    float sinOmega = sin(omega);
    float cosOmega = cos(omega);
    float alpha = sinOmega / (2.0f * q);

    float b0 = (1.0f - cosOmega) / 2.0f;
    float b1 = 1.0f - cosOmega;
    float b2 = (1.0f - cosOmega) / 2.0f;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cosOmega;
    float a2 = 1.0f - alpha;

    _biquadB0 = b0 / a0;
    _biquadB1 = b1 / a0;
    _biquadB2 = b2 / a0;
    _biquadA1 = a1 / a0;
    _biquadA2 = a2 / a0;
    _biquadX1 = _biquadX2 = 0.0f;
    _biquadY1 = _biquadY2 = 0.0f;
}

float FilterFunctions::biquadLowPass(float input)
{
    float y = _biquadB0 * input + _biquadB1 * _biquadX1 + _biquadB2 * _biquadX2 - _biquadA1 * _biquadY1 - _biquadA2 * _biquadY2;
    _biquadX2 = _biquadX1;
    _biquadX1 = input;
    _biquadY2 = _biquadY1;
    _biquadY1 = y;
    return y;
}

void FilterFunctions::beginNotchFilter(float centerHz, float q, float sampleRate)
{
    if (sampleRate <= 0.0f)
        sampleRate = 1.0f;
    if (centerHz <= 0.0f)
        centerHz = 0.1f;
    q = max(q, 0.001f);
    float omega = 2.0f * PI * centerHz / sampleRate;
    float sinOmega = sin(omega);
    float cosOmega = cos(omega);
    float alpha = sinOmega / (2.0f * q);

    float b0 = 1.0f;
    float b1 = -2.0f * cosOmega;
    float b2 = 1.0f;
    float a0 = 1.0f + alpha;
    float a1 = -2.0f * cosOmega;
    float a2 = 1.0f - alpha;

    _notchB0 = b0 / a0;
    _notchB1 = b1 / a0;
    _notchB2 = b2 / a0;
    _notchA1 = a1 / a0;
    _notchA2 = a2 / a0;
    _notchX1 = _notchX2 = 0.0f;
    _notchY1 = _notchY2 = 0.0f;
}

float FilterFunctions::notchFilter(float input)
{
    float y = _notchB0 * input + _notchB1 * _notchX1 + _notchB2 * _notchX2 - _notchA1 * _notchY1 - _notchA2 * _notchY2;
    _notchX2 = _notchX1;
    _notchX1 = input;
    _notchY2 = _notchY1;
    _notchY1 = y;
    return y;
}
