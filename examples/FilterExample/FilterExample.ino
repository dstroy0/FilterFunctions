/*
  FilterExample - demonstrates FilterFunctions library usage
  Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
  Licensed under the GNU Affero General Public License v3.0

  Loop runs at ~50 Hz (20 ms delay).  Analog pin A0 is read and scaled
  to a 0–5 V float; simulated accel/gyro values are derived from it.

  Serial output format (one line per loop, 9600 baud):
    raw=x.xxx ma=x.xxx ema=x.xxx med=x.xxx lp=x.xxx hp=x.xxx
    sg=x.xxx kf=x.xxx comp=x.xxx bq=x.xxx notch=x.xxx

  Notes
  -----
  - SavitzkyGolay output is delayed by (windowSize-1)/2 samples (the
    filter is centered; during warmup a moving average is returned).
  - ComplementaryFilter integrates gyroRate*dt on the gyro branch; keep
    dt consistent with the actual loop period.
  - BiquadLowPass and NotchFilter state is accumulated across calls;
    call begin*() again to reset if the sample rate changes.
*/

#include <FilterFunctions.h>

FilterFunctions filter;

// Sample rate derived from the loop delay below.
static const float SAMPLE_RATE_HZ = 50.0f;   // 1000 ms / 20 ms
static const float DT             = 0.020f;   // loop period in seconds

void setup()
{
    Serial.begin(9600);

    // --- Moving average: window of 10 samples (200 ms at 50 Hz) ---
    filter.beginMovingAverage(10);

    // --- Exponential moving average: alpha=0.1 (heavy smoothing) ---
    filter.beginExponentialMovingAverage(0.1f);

    // --- Median filter: window of 5 samples, rejects impulse spikes ---
    filter.beginMedianFilter(5);

    // --- One-pole low-pass: alpha=0.2 (equivalent to ~8 Hz cutoff at 50 Hz) ---
    filter.beginLowPassFilter(0.2f);

    // --- One-pole high-pass: alpha=0.95 (passes signals above ~0.4 Hz) ---
    filter.beginHighPassFilter(0.95f);

    // --- Savitzky-Golay: window=5, quadratic polynomial smoothing.
    //     Output is delayed by (5-1)/2 = 2 samples (40 ms at 50 Hz).
    //     Preserves peak shapes better than a plain moving average. ---
    filter.beginSavitzkyGolay(5);

    // --- Kalman: processNoise=0.01, measurementNoise=0.5 ---
    filter.beginKalmanFilter(0.01f, 0.5f);

    // --- Complementary: alpha=0.98 trusts gyro 98 %, accel 2 % per step ---
    filter.beginComplementaryFilter(0.98f);

    // --- Biquad low-pass: 5 Hz cutoff, 50 Hz sample rate, Q=0.707 (-3 dB Butterworth) ---
    filter.beginBiquadLowPass(5.0f, SAMPLE_RATE_HZ, 0.707f);

    // --- Notch: reject 10 Hz (e.g. motor vibration), Q=5 for a narrow notch ---
    filter.beginNotchFilter(10.0f, 5.0f, SAMPLE_RATE_HZ);
}

void loop()
{
    // Read sensor: scale 10-bit ADC to 0–5 V
    float raw = analogRead(A0) * (5.0f / 1023.0f);

    // Derive simulated IMU inputs from the same sensor reading
    float accelAngle = raw * 50.0f - 25.0f;   // maps 0–5 V → -25°…+25°
    float gyroRate   = raw * 20.0f - 10.0f;   // maps 0–5 V → -10…+10 °/s

    float ma    = filter.movingAverage(raw);
    float ema   = filter.exponentialMovingAverage(raw, 0.1f);
    float med   = filter.medianFilter(raw);
    float lp    = filter.lowPassFilter(raw, 0.2f);
    float hp    = filter.highPassFilter(raw, 0.95f);
    float sg    = filter.savitzkyGolay(raw);        // 2-sample lag
    float kf    = filter.kalmanFilter(raw);
    float comp  = filter.complementaryFilter(accelAngle, gyroRate, DT);
    float bq    = filter.biquadLowPass(raw);
    float notch = filter.notchFilter(raw);

    Serial.print("raw=");   Serial.print(raw,   3);
    Serial.print(" ma=");   Serial.print(ma,    3);
    Serial.print(" ema=");  Serial.print(ema,   3);
    Serial.print(" med=");  Serial.print(med,   3);
    Serial.print(" lp=");   Serial.print(lp,    3);
    Serial.print(" hp=");   Serial.print(hp,    3);
    Serial.print(" sg=");   Serial.print(sg,    3);
    Serial.print(" kf=");   Serial.print(kf,    3);
    Serial.print(" comp="); Serial.print(comp,  3);
    Serial.print(" bq=");   Serial.print(bq,    3);
    Serial.print(" notch=");Serial.println(notch, 3);

    delay(20);
}
