/*
  Basic Filter Example
  Demonstrates MovingAverage, ExponentialMovingAverage, and LowPassFilter
  on a noisy step signal (0 → 10 at sample 15).

  Compile from project root:
    g++ -std=c++11 tests/basic_filters.cpp src/FilterFunctions.cpp -I. -o basic_filters -lm
*/
#include "../src/FilterFunctions.h"
#include <cstdio>

// Returns the low-pass output on the final sample.
float run_basic_example()
{
    FilterFunctions f;

    // Initialise all three filters
    f.beginMovingAverage(8);
    f.beginExponentialMovingAverage(0.2f);
    f.beginLowPassFilter(0.15f);

    // Deterministic noise pattern (avoids <cmath> sin dependency)
    const float noise[] = { 0.3f, -0.2f, 0.1f, -0.3f };

    printf("%-6s  %-8s  %-10s  %-10s  %-10s\n",
           "Sample", "Raw", "MA(8)", "EMA(0.2)", "LP(0.15)");
    printf("%-6s  %-8s  %-10s  %-10s  %-10s\n",
           "------", "--------", "----------", "----------", "----------");

    float lpOut = 0.0f;
    for (int i = 0; i < 40; ++i)
    {
        float raw    = (i >= 15 ? 10.0f : 0.0f) + noise[i % 4];
        float maOut  = f.movingAverage(raw);
        float emaOut = f.exponentialMovingAverage(raw, 0.2f);
        lpOut        = f.lowPassFilter(raw, 0.15f);

        printf("%-6d  %-8.3f  %-10.3f  %-10.3f  %-10.3f\n",
               i, raw, maOut, emaOut, lpOut);
    }
    return lpOut;
}

#ifndef EXAMPLE_NO_MAIN
int main()
{
    printf("=== Basic Filter Example ===\n\n");
    float result = run_basic_example();
    printf("\nFinal low-pass output: %.3f  (expected near 10.0)\n", result);
    return 0;
}
#endif
