/*
  Advanced Filter Example
  MedianFilter   – rejects large spike outliers
  HighPassFilter – removes slow DC drift
  SavitzkyGolay  – smooths a peaked waveform
  KalmanFilter   – optimal estimation of a noisy sensor

  Compile from project root:
    g++ -std=c++11 tests/advanced_filters.cpp src/FilterFunctions.cpp -I. -o advanced_filters -lm
*/
#include "../src/FilterFunctions.h"
#include <cstdio>

// Returns the Kalman output on the final sample.
float run_advanced_example()
{
    FilterFunctions f;

    // ---- 1. Median filter: constant signal with occasional spikes ----
    f.beginMedianFilter(7);
    printf("--- Median Filter (size=7, spike rejection) ---\n");
    printf("%-6s  %-8s  %-10s\n", "Sample", "Raw", "Median");

    const float spikeData[] = {
        5.0f, 5.1f, 4.9f, 500.0f, 5.0f,
        4.8f, 5.2f, -200.0f, 5.0f, 5.1f,
        4.9f, 5.0f, 5.0f, 5.1f, 4.8f
    };
    for (int i = 0; i < 15; ++i)
    {
        float med = f.medianFilter(spikeData[i]);
        printf("%-6d  %-8.1f  %-10.3f\n", i, spikeData[i], med);
    }

    // ---- 2. High-pass filter: strip DC offset, keep AC ----
    f.beginHighPassFilter(0.95f);
    printf("\n--- High-Pass Filter (alpha=0.95, DC removal) ---\n");
    printf("%-6s  %-10s  %-10s\n", "Sample", "Raw(DC+AC)", "HighPass");

    // DC = 100, AC = small triangle
    const float dcOffset = 100.0f;
    const float ac[]     = { 0.0f,1.0f,2.0f,1.0f,0.0f,-1.0f,-2.0f,-1.0f,
                              0.0f,1.0f,2.0f,1.0f,0.0f,-1.0f,-2.0f };
    for (int i = 0; i < 15; ++i)
    {
        float raw = dcOffset + ac[i];
        float hp  = f.highPassFilter(raw, 0.95f);
        printf("%-6d  %-10.2f  %-10.4f\n", i, raw, hp);
    }

    // ---- 3. Savitzky-Golay: smooth a noisy peak ----
    f.beginSavitzkyGolay(5);
    printf("\n--- Savitzky-Golay (window=5, peak smoothing) ---\n");
    printf("%-6s  %-8s  %-10s\n", "Sample", "Raw", "SG");

    const float noisyPeak[] = {
        0.3f, 1.2f, 2.8f, 4.1f, 5.3f, 6.0f, 5.2f, 4.2f, 2.9f,
        1.3f,-0.2f, 0.1f,-0.1f, 0.2f, 0.0f
    };
    for (int i = 0; i < 15; ++i)
    {
        float sg = f.savitzkyGolay(noisyPeak[i]);
        printf("%-6d  %-8.2f  %-10.4f\n", i, noisyPeak[i], sg);
    }

    // ---- 4. Kalman: estimate true value from noisy sensor ----
    f.beginKalmanFilter(0.001f, 0.1f);
    printf("\n--- Kalman Filter (Q=0.001, R=0.1, true=10.0) ---\n");
    printf("%-6s  %-8s  %-10s\n", "Sample", "Noisy", "Kalman");

    const float noisyMeas[] = {
        10.5f, 9.8f, 10.2f, 9.9f, 10.3f,
        10.1f, 9.7f, 10.4f, 9.8f, 10.1f,
        10.0f, 10.2f, 9.9f, 10.1f, 10.0f
    };
    float kalOut = 0.0f;
    for (int i = 0; i < 15; ++i)
    {
        kalOut = f.kalmanFilter(noisyMeas[i]);
        printf("%-6d  %-8.2f  %-10.4f\n", i, noisyMeas[i], kalOut);
    }
    return kalOut;
}

#ifndef EXAMPLE_NO_MAIN
int main()
{
    printf("=== Advanced Filter Example ===\n\n");
    float result = run_advanced_example();
    printf("\nFinal Kalman output: %.4f  (expected near 10.0)\n", result);
    return 0;
}
#endif
