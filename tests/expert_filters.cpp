/*
  Expert Filter Example
  BiquadLowPass       – frequency-selective smoothing
  NotchFilter         – 60 Hz power-line interference rejection
  ComplementaryFilter – IMU sensor fusion (accelerometer + gyroscope)

  Compile from project root:
    g++ -std=c++11 tests/expert_filters.cpp src/FilterFunctions.cpp -I. -o expert_filters -lm
*/
#include "../src/FilterFunctions.h"
#include <cstdio>
#include <cmath>

// Returns the biquad output on the final sample.
float run_expert_example()
{
    FilterFunctions f;

    const float SR    = 1000.0f;   // sample rate Hz
    const float TWOPI = 2.0f * PI;

    // ---- 1. Biquad low-pass: separate DC signal from Nyquist noise ----
    f.beginBiquadLowPass(100.0f, SR, 0.707f);
    printf("--- Biquad Low-Pass (100 Hz cutoff, 1 kHz SR) ---\n");
    printf("%-6s  %-10s  %-10s\n", "Sample", "Raw", "BiquadLP");

    // 60 DC samples then 20 Nyquist (alternating ±1)
    float bqOut = 0.0f;
    for (int i = 0; i < 80; ++i)
    {
        float raw = (i < 60) ? 1.0f : (i % 2 == 0 ? 1.0f : -1.0f);
        bqOut = f.biquadLowPass(raw);
        if (i >= 55)  // show the interesting transition region
            printf("%-6d  %-10.4f  %-10.4f\n", i, raw, bqOut);
    }

    // ---- 2. Notch filter: remove 60 Hz power-line tone ----
    f.beginNotchFilter(60.0f, 10.0f, SR);
    printf("\n--- Notch Filter (60 Hz, Q=10, 1 kHz SR) ---\n");
    printf("%-6s  %-10s  %-10s\n", "Sample", "Raw", "Notched");

    // DC=1.0 + 60 Hz tone of amplitude 0.5
    for (int i = 0; i < 50; ++i)
    {
        float tone = 0.5f * sinf(TWOPI * 60.0f * i / SR);
        float raw  = 1.0f + tone;
        float out  = f.notchFilter(raw);
        if (i >= 40)  // show settled output
            printf("%-6d  %-10.4f  %-10.4f\n", i, raw, out);
    }

    // ---- 3. Complementary filter: IMU fusion ----
    f.beginComplementaryFilter(0.98f);
    printf("\n--- Complementary Filter (alpha=0.98, dt=0.01s, 100 Hz) ---\n");
    printf("%-6s  %-10s  %-10s  %-10s\n",
           "Sample", "AccelAng", "GyroRate", "CompAngle");

    // Simulate: true angle tilts from 0 to 30° linearly over 50 samples.
    // Accelerometer: measures true angle + ±0.5° noise.
    // Gyro: measures angular velocity (dAngle/dt) + small offset drift.
    const float dt       = 0.01f;   // 10 ms
    const float dAngPerStep = 30.0f / 50.0f;
    float compOut = 0.0f;
    const float accelNoise[] = { 0.3f,-0.2f, 0.1f,-0.3f, 0.4f,
                                 -0.1f, 0.2f,-0.4f, 0.1f, 0.0f };
    for (int i = 0; i < 50; ++i)
    {
        float trueAngle  = dAngPerStep * i;
        float accelAngle = trueAngle + accelNoise[i % 10];
        float gyroRate   = dAngPerStep / dt + 0.2f;   // slight drift
        compOut = f.complementaryFilter(accelAngle, gyroRate, dt);
        if (i >= 45)
            printf("%-6d  %-10.3f  %-10.3f  %-10.3f\n",
                   i, accelAngle, gyroRate, compOut);
    }
    return bqOut;
}

#ifndef EXAMPLE_NO_MAIN
int main()
{
    printf("=== Expert Filter Example ===\n\n");
    float result = run_expert_example();
    printf("\nFinal biquad output: %.4f  (expected near 1.0 for DC input)\n", result);
    return 0;
}
#endif
