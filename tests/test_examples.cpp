/*
  Test suite for the example programs.
  Includes each example source file (suppressing their main()) and verifies
  that the returned outputs meet the expected filtering properties.

  Compile from project root:
    g++ -std=c++11 -Wall -Wextra \
        tests/test_examples.cpp src/FilterFunctions.cpp \
        -I. -o test_examples_bin -lm
*/
#define EXAMPLE_NO_MAIN
#include "basic_filters.cpp"
#include "advanced_filters.cpp"
#include "expert_filters.cpp"

#include "test_runner.h"

static void test_basic_example()
{
    SUITE("BasicExample");

    // run_basic_example() returns the low-pass output on the final sample.
    // After 25 samples above the step target of 10.0, the LP filter with
    // alpha=0.15 should have converged well into [9.0, 11.0].
    float result = run_basic_example();
    CHECK(result >= 9.0f && result <= 11.0f);

    // Run twice to verify the example is deterministic (no global state).
    float result2 = run_basic_example();
    CHECK_NEAR(result, result2, 1e-5f);
}

static void test_advanced_example()
{
    SUITE("AdvancedExample");

    // run_advanced_example() returns the final Kalman output.
    // After 15 noisy measurements of true value 10.0 (Q=0.001, R=0.1),
    // the Kalman estimate should be within 0.3 of the true value.
    float result = run_advanced_example();
    CHECK(result >= 9.7f && result <= 10.3f);
}

static void test_expert_example()
{
    SUITE("ExpertExample");

    // run_expert_example() returns the biquad LP output on the last sample
    // of a 500 Hz (Nyquist) alternating signal after 60 DC samples.
    // The alternating section (±1) should be attenuated to near zero
    // by the 100 Hz cutoff / 1 kHz SR biquad filter.
    // The value at sample 79 is deep in the alternating region.
    float result = run_expert_example();
    // After transition, biquad should suppress Nyquist heavily
    CHECK(result > -0.5f && result < 0.5f);
}

int main()
{
    test_basic_example();
    test_advanced_example();
    test_expert_example();
    REPORT();
}
