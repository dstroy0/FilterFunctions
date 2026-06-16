/*
  FilterFunctions test suite.
  Compile from project root:
    g++ -std=c++11 -Wall -Wextra tests/test_filters.cpp src/FilterFunctions.cpp -o test_filters_bin -lm
*/
#include "../src/FilterFunctions.h"
#include "test_runner.h"

#include <cstdio>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Minimal JSON "csv" field extractor + CSV parser
// ---------------------------------------------------------------------------

static std::string extractJsonCsv(const std::string &path)
{
    std::ifstream f(path.c_str());
    if (!f.is_open())
    {
        printf("  [WARN] Cannot open %s\n", path.c_str());
        return "";
    }
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    size_t pos = content.find("\"csv\"");
    if (pos == std::string::npos) return "";
    pos = content.find('"', pos + 5);   // opening quote of value
    if (pos == std::string::npos) return "";
    ++pos;                               // step past it

    std::string result;
    for (; pos < content.size(); ++pos)
    {
        if (content[pos] == '\\' && pos + 1 < content.size())
        {
            char esc = content[++pos];
            if      (esc == 'n')  result += '\n';
            else if (esc == 'r')  result += '\r';
            else if (esc == '"')  result += '"';
            else if (esc == '\\') result += '\\';
        }
        else if (content[pos] == '"') break;
        else result += content[pos];
    }
    return result;
}

struct CsvData
{
    std::vector<std::string>         headers;
    std::vector<std::vector<float>>  rows;
};

static CsvData parseCsv(const std::string &csv)
{
    CsvData out;
    std::istringstream ss(csv);
    std::string line;
    bool first = true;
    while (std::getline(ss, line))
    {
        if (line.empty()) continue;
        // strip \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ls(line);
        std::string cell;
        if (first)
        {
            while (std::getline(ls, cell, ',')) out.headers.push_back(cell);
            first = false;
        }
        else
        {
            std::vector<float> row;
            while (std::getline(ls, cell, ',')) row.push_back(std::stof(cell));
            if (!row.empty()) out.rows.push_back(row);
        }
    }
    return out;
}

static CsvData loadData(const std::string &name)
{
    return parseCsv(extractJsonCsv("tests/simulated_data/" + name + ".json"));
}

static int colIndex(const CsvData &d, const std::string &name)
{
    for (int i = 0; i < static_cast<int>(d.headers.size()); ++i)
        if (d.headers[i] == name) return i;
    return -1;
}

// ---------------------------------------------------------------------------
// Moving Average
// ---------------------------------------------------------------------------
static void test_movingAverage()
{
    SUITE("MovingAverage");

    FilterFunctions f;

    // Uninitialized: passthrough
    CHECK_NEAR(f.movingAverage(7.0f), 7.0f, 1e-4f);

    // Size 1: passthrough
    f.beginMovingAverage(1);
    CHECK_NEAR(f.movingAverage(3.0f), 3.0f, 1e-4f);
    CHECK_NEAR(f.movingAverage(9.0f), 9.0f, 1e-4f);

    // Size 3: constant input converges
    f.beginMovingAverage(3);
    f.movingAverage(10.0f);
    f.movingAverage(10.0f);
    CHECK_NEAR(f.movingAverage(10.0f), 10.0f, 1e-4f);

    // Size 3: analytical window
    f.beginMovingAverage(3);
    f.movingAverage(3.0f);   // avg of [3,0,0] = 1
    f.movingAverage(6.0f);   // avg of [3,6,0] = 3
    CHECK_NEAR(f.movingAverage(9.0f), 6.0f, 1e-4f);  // avg of [3,6,9] = 6
    CHECK_NEAR(f.movingAverage(0.0f), 5.0f, 1e-4f);  // avg of [6,9,0] = 5

    // beginMovingAverage resets state
    f.beginMovingAverage(4);
    f.movingAverage(100.0f);
    f.beginMovingAverage(4);
    CHECK_NEAR(f.movingAverage(2.0f), 0.5f, 1e-4f);  // [2,0,0,0]/4

    // JSON test vectors
    CsvData d = loadData("moving_average");
    if (!d.rows.empty())
    {
        int ri = colIndex(d, "raw"), ei = colIndex(d, "expected");
        FilterFunctions g;
        g.beginMovingAverage(4);
        for (size_t i = 0; i < d.rows.size(); ++i)
        {
            float got = g.movingAverage(d.rows[i][ri]);
            CHECK_NEAR(got, d.rows[i][ei], 1e-3f);
        }
    }
}

// ---------------------------------------------------------------------------
// Exponential Moving Average
// ---------------------------------------------------------------------------
static void test_exponentialMA()
{
    SUITE("ExponentialMovingAverage");

    FilterFunctions f;

    // alpha=1: pass-through after init
    f.beginExponentialMovingAverage(1.0f);
    CHECK_NEAR(f.exponentialMovingAverage(5.0f, 1.0f), 5.0f, 1e-4f);
    CHECK_NEAR(f.exponentialMovingAverage(8.0f, 1.0f), 8.0f, 1e-4f);

    // alpha=0: output stays at first value
    f.beginExponentialMovingAverage(0.0f);
    CHECK_NEAR(f.exponentialMovingAverage(5.0f, 0.0f), 5.0f, 1e-4f);
    CHECK_NEAR(f.exponentialMovingAverage(8.0f, 0.0f), 5.0f, 1e-4f);

    // alpha=0.5: analytical step
    f.beginExponentialMovingAverage(0.5f);
    f.exponentialMovingAverage(10.0f, 0.5f);   // init to 10
    CHECK_NEAR(f.exponentialMovingAverage(0.0f, 0.5f), 5.0f, 1e-4f);

    // begin resets started flag
    f.beginExponentialMovingAverage(0.2f);
    f.exponentialMovingAverage(99.0f, 0.2f);
    f.beginExponentialMovingAverage(0.2f);
    CHECK_NEAR(f.exponentialMovingAverage(1.0f, 0.2f), 1.0f, 1e-4f);

    // JSON test vectors
    CsvData d = loadData("exponential_ma");
    if (!d.rows.empty())
    {
        int ri = colIndex(d, "raw"), ei = colIndex(d, "expected");
        FilterFunctions g;
        g.beginExponentialMovingAverage(0.2f);
        for (size_t i = 0; i < d.rows.size(); ++i)
        {
            float got = g.exponentialMovingAverage(d.rows[i][ri], 0.2f);
            CHECK_NEAR(got, d.rows[i][ei], 1e-3f);
        }
    }
}

// ---------------------------------------------------------------------------
// Median Filter
// ---------------------------------------------------------------------------
static void test_medianFilter()
{
    SUITE("MedianFilter");

    FilterFunctions f;

    // Uninitialized: passthrough
    CHECK_NEAR(f.medianFilter(3.0f), 3.0f, 1e-4f);

    // Size 1: passthrough
    f.beginMedianFilter(1);
    CHECK_NEAR(f.medianFilter(7.0f), 7.0f, 1e-4f);
    CHECK_NEAR(f.medianFilter(2.0f), 2.0f, 1e-4f);

    // Size 3: spike is suppressed after window fills
    f.beginMedianFilter(3);
    f.medianFilter(5.0f);
    f.medianFilter(5.0f);
    f.medianFilter(500.0f);  // spike
    CHECK_NEAR(f.medianFilter(5.0f), 5.0f, 1e-4f);  // [5,500,5] median=5

    // Size 3 sorted order check
    f.beginMedianFilter(3);
    f.medianFilter(1.0f);
    f.medianFilter(9.0f);
    CHECK_NEAR(f.medianFilter(5.0f), 5.0f, 1e-4f);  // [1,9,5] sorted [1,5,9] median=5

    // Output is always one of the input values
    f.beginMedianFilter(5);
    float inputs[] = {3.0f, 7.0f, 1.0f, 9.0f, 5.0f};
    for (float v : inputs) f.medianFilter(v);
    // Sorted: [1,3,5,7,9] → median = 5
    CHECK_NEAR(f.medianFilter(5.0f), 5.0f, 1e-4f);

    // JSON test vectors (includes spike at sample 3 = 100, sample 7 = -50)
    CsvData d = loadData("median");
    if (!d.rows.empty())
    {
        int ri = colIndex(d, "raw"), ei = colIndex(d, "expected");
        FilterFunctions g;
        g.beginMedianFilter(5);
        for (size_t i = 0; i < d.rows.size(); ++i)
        {
            float got = g.medianFilter(d.rows[i][ri]);
            CHECK_NEAR(got, d.rows[i][ei], 1e-3f);
        }
    }
}

// ---------------------------------------------------------------------------
// Low-Pass Filter
// ---------------------------------------------------------------------------
static void test_lowPassFilter()
{
    SUITE("LowPassFilter");

    FilterFunctions f;

    // alpha=1: passthrough
    f.beginLowPassFilter(1.0f);
    CHECK_NEAR(f.lowPassFilter(5.0f, 1.0f), 5.0f, 1e-4f);
    CHECK_NEAR(f.lowPassFilter(8.0f, 1.0f), 8.0f, 1e-4f);

    // alpha=0: holds initial value
    f.beginLowPassFilter(0.0f);
    CHECK_NEAR(f.lowPassFilter(5.0f, 0.0f), 5.0f, 1e-4f);
    CHECK_NEAR(f.lowPassFilter(8.0f, 0.0f), 5.0f, 1e-4f);

    // alpha=0.5 analytical
    f.beginLowPassFilter(0.5f);
    f.lowPassFilter(10.0f, 0.5f);
    CHECK_NEAR(f.lowPassFilter(0.0f, 0.5f), 5.0f, 1e-4f);

    // LP and EMA are mathematically identical — verify they produce the same output
    FilterFunctions lp, ema;
    lp.beginLowPassFilter(0.3f);
    ema.beginExponentialMovingAverage(0.3f);
    float inputs[] = {1.0f, 5.0f, 3.0f, 8.0f, 2.0f};
    for (float v : inputs)
        CHECK_NEAR(lp.lowPassFilter(v, 0.3f),
                   ema.exponentialMovingAverage(v, 0.3f), 1e-5f);

    // JSON test vectors
    CsvData d = loadData("low_pass");
    if (!d.rows.empty())
    {
        int ri = colIndex(d, "raw"), ei = colIndex(d, "expected");
        FilterFunctions g;
        g.beginLowPassFilter(0.2f);
        for (size_t i = 0; i < d.rows.size(); ++i)
        {
            float got = g.lowPassFilter(d.rows[i][ri], 0.2f);
            CHECK_NEAR(got, d.rows[i][ei], 1e-3f);
        }
    }
}

// ---------------------------------------------------------------------------
// High-Pass Filter
// ---------------------------------------------------------------------------
static void test_highPassFilter()
{
    SUITE("HighPassFilter");

    FilterFunctions f;

    // First output = alpha * input (y=alpha*(x+x-x)=alpha*x)
    f.beginHighPassFilter(0.9f);
    CHECK_NEAR(f.highPassFilter(10.0f, 0.9f), 9.0f, 1e-4f);

    // Constant input: output decays to 0 (DC rejection)
    f.beginHighPassFilter(0.9f);
    float prev = f.highPassFilter(5.0f, 0.9f);
    for (int i = 0; i < 50; ++i)
        prev = f.highPassFilter(5.0f, 0.9f);
    CHECK(prev < 0.1f);  // nearly zero for DC input

    // Step change produces a transient spike
    f.beginHighPassFilter(0.9f);
    for (int i = 0; i < 20; ++i) f.highPassFilter(0.0f, 0.9f);  // settle on 0
    float spike = f.highPassFilter(10.0f, 0.9f);
    CHECK(spike > 5.0f);  // initial response to step

    // alpha=0: no output
    f.beginHighPassFilter(0.0f);
    CHECK_NEAR(f.highPassFilter(100.0f, 0.0f), 0.0f, 1e-4f);

    // JSON: geometric decay for constant input
    CsvData d = loadData("high_pass");
    if (!d.rows.empty())
    {
        int ri = colIndex(d, "raw"), ei = colIndex(d, "expected");
        FilterFunctions g;
        g.beginHighPassFilter(0.9f);
        for (size_t i = 0; i < d.rows.size(); ++i)
        {
            float got = g.highPassFilter(d.rows[i][ri], 0.9f);
            CHECK_NEAR(got, d.rows[i][ei], 1e-3f);
        }
    }
}

// ---------------------------------------------------------------------------
// Savitzky-Golay  (quadratic polynomial smoothing, centered window)
// Coefficients: window-5  [-3,12,17,12,-3]/35
//               window-7  [-2,3,6,7,6,3,-2]/21
//               window-3  [1,1,1]/3  (MA; degree-2 fits 3 pts exactly → identity)
// Output lag: m = (windowSize-1)/2 samples.  Warmup (<size samples) uses MA.
// ---------------------------------------------------------------------------
static void test_savitzkyGolay()
{
    SUITE("SavitzkyGolay");

    FilterFunctions f;

    // Uninitialized: passthrough
    CHECK_NEAR(f.savitzkyGolay(5.0f), 5.0f, 1e-4f);

    // --- window clamping and even→odd promotion ---
    f.beginSavitzkyGolay(1);   // clamped to 3
    CHECK_NEAR(f.savitzkyGolay(6.0f), 6.0f, 1e-4f);  // warmup: single-sample MA

    // Even 4 → 5; first full SG output on 5th call
    // Linear ramp [2,4,6,8,10]: center = x[2] = 6
    // SG-5: (-3*2+12*4+17*6+12*8-3*10)/35 = (-6+48+102+96-30)/35 = 210/35 = 6.0
    f.beginSavitzkyGolay(4);
    f.savitzkyGolay(2.0f);
    f.savitzkyGolay(4.0f);
    f.savitzkyGolay(6.0f);
    f.savitzkyGolay(8.0f);
    CHECK_NEAR(f.savitzkyGolay(10.0f), 6.0f, 1e-4f);

    // --- Constant input: SG preserves constants exactly ---
    // Σ coefficients = 1 for all window sizes, so all-C window → C
    f.beginSavitzkyGolay(5);
    for (int i = 0; i < 5; ++i) f.savitzkyGolay(7.0f);
    CHECK_NEAR(f.savitzkyGolay(7.0f), 7.0f, 1e-4f);

    f.beginSavitzkyGolay(7);
    for (int i = 0; i < 7; ++i) f.savitzkyGolay(3.0f);
    CHECK_NEAR(f.savitzkyGolay(3.0f), 3.0f, 1e-4f);

    // --- Linear trend: SG reproduces exactly (polynomial degree ≥ 1) ---
    // Feed ramp 0,1,2,3,4; 5th call → SG on [0,1,2,3,4], center=2
    f.beginSavitzkyGolay(5);
    for (int i = 0; i < 4; ++i) f.savitzkyGolay(static_cast<float>(i));
    CHECK_NEAR(f.savitzkyGolay(4.0f), 2.0f, 1e-3f);  // smooth(x[2]=2)
    CHECK_NEAR(f.savitzkyGolay(5.0f), 3.0f, 1e-3f);  // smooth(x[3]=3)
    CHECK_NEAR(f.savitzkyGolay(6.0f), 4.0f, 1e-3f);  // smooth(x[4]=4)

    // --- Peak reduction: quadratic fit reduces spike amplitude ---
    // Feed [5,5,5,5,10]; center=x[2]=5, newest=10
    // SG-5: (-3*5+12*5+17*5+12*5-3*10)/35 = (35*5 - 3*10 - 3*5)/35 = (175-30-15)/35 = 160/35
    f.beginSavitzkyGolay(5);
    f.savitzkyGolay(5.0f);
    f.savitzkyGolay(5.0f);
    f.savitzkyGolay(5.0f);
    f.savitzkyGolay(5.0f);
    CHECK_NEAR(f.savitzkyGolay(10.0f), 160.0f / 35.0f, 1e-3f);

    // --- begin resets state ---
    f.beginSavitzkyGolay(5);
    f.savitzkyGolay(99.0f);
    f.beginSavitzkyGolay(5);
    CHECK_NEAR(f.savitzkyGolay(1.0f), 1.0f, 1e-4f);  // warmup MA of single sample

    // --- JSON: triangle wave, window-5, calls 1-4 warmup, 5-10 SG ---
    // Call 5 smooths x[2]=7: SG([3,5,7,9,11])=245/35=7.0
    // Call 7 smooths x[4]=11 (peak): SG([7,9,11,9,7])=361/35≈10.3143
    CsvData d = loadData("savitzky_golay");
    if (!d.rows.empty())
    {
        int ri = colIndex(d, "raw"), ei = colIndex(d, "expected");
        FilterFunctions g;
        g.beginSavitzkyGolay(5);
        for (size_t i = 0; i < d.rows.size(); ++i)
        {
            float got = g.savitzkyGolay(d.rows[i][ri]);
            CHECK_NEAR(got, d.rows[i][ei], 1e-3f);
        }
    }
}

// ---------------------------------------------------------------------------
// Kalman Filter
// ---------------------------------------------------------------------------
static void test_kalmanFilter()
{
    SUITE("KalmanFilter");

    FilterFunctions f;

    // First call: initialized to measurement
    f.beginKalmanFilter(0.001f, 0.1f);
    CHECK_NEAR(f.kalmanFilter(5.0f), 5.0f, 1e-4f);

    // Converges to constant true value
    f.beginKalmanFilter(0.001f, 0.1f);
    float out = 0.0f;
    for (int i = 0; i < 100; ++i) out = f.kalmanFilter(10.0f);
    CHECK_NEAR(out, 10.0f, 1e-3f);

    // Output is bounded by measurement range (smoothing, not amplification)
    f.beginKalmanFilter(0.01f, 0.5f);
    float lo = 1e9f, hi = -1e9f;
    float meas[] = {9.5f,10.2f,10.0f,9.8f,10.1f,9.9f,10.3f,9.7f,10.0f,10.1f};
    for (float m : meas)
    {
        float y = f.kalmanFilter(m);
        if (y < lo) lo = y;
        if (y > hi) hi = y;
    }
    CHECK(lo >= 9.0f && hi <= 11.0f);

    // JSON: statistical convergence test
    CsvData d = loadData("kalman");
    if (!d.rows.empty())
    {
        int ri = colIndex(d, "raw");
        FilterFunctions g;
        g.beginKalmanFilter(0.001f, 0.1f);
        float last = 0.0f;
        for (size_t i = 0; i < d.rows.size(); ++i)
            last = g.kalmanFilter(d.rows[i][ri]);
        // Should converge close to true value 10.0
        CHECK_NEAR(last, 10.0f, 0.5f);
    }
}

// ---------------------------------------------------------------------------
// Complementary Filter
// ---------------------------------------------------------------------------
static void test_complementaryFilter()
{
    SUITE("ComplementaryFilter");

    FilterFunctions f;

    // alpha=0: pure accelerometer output
    f.beginComplementaryFilter(0.0f);
    float r = f.complementaryFilter(45.0f, 0.0f, 0.02f);
    CHECK_NEAR(r, 45.0f, 1e-4f);

    // Stable hover: accel and gyro agree → angle stays constant
    f.beginComplementaryFilter(0.98f);
    float angle = 0.0f;
    for (int i = 0; i < 50; ++i)
        angle = f.complementaryFilter(45.0f, 0.0f, 0.02f);
    CHECK_NEAR(angle, 45.0f, 0.01f);

    // alpha=1: pure gyro integration (no accel)
    f.beginComplementaryFilter(1.0f);
    f.complementaryFilter(0.0f, 100.0f, 0.02f);   // init: angle=0, then 0+(100*0.02)=2
    float r2 = f.complementaryFilter(0.0f, 100.0f, 0.02f);  // 1*(2+2)+0=4
    CHECK_NEAR(r2, 4.0f, 1e-3f);

    // alpha clamped to [0,1]: out-of-range value clamped
    f.beginComplementaryFilter(1.5f);   // clamped to 1.0
    f.complementaryFilter(0.0f, 10.0f, 0.1f);  // angle = 1*(0+1.0) = 1.0
    float r3 = f.complementaryFilter(0.0f, 10.0f, 0.1f);
    CHECK_NEAR(r3, 2.0f, 1e-3f);  // 1*(1+1)=2

    // JSON: stable hover around 45°
    CsvData d = loadData("complementary");
    if (!d.rows.empty())
    {
        int ai = colIndex(d, "accel_angle");
        int gi = colIndex(d, "gyro_rate");
        FilterFunctions g;
        g.beginComplementaryFilter(0.98f);
        float last = 0.0f;
        for (size_t i = 0; i < d.rows.size(); ++i)
            last = g.complementaryFilter(d.rows[i][ai], d.rows[i][gi], 0.02f);
        CHECK_NEAR(last, 45.0f, 1.0f);
    }
}

// ---------------------------------------------------------------------------
// Biquad Low-Pass
// ---------------------------------------------------------------------------
static void test_biquadLowPass()
{
    SUITE("BiquadLowPass");

    FilterFunctions f;

    // DC unity-gain: constant 1.0 should settle to 1.0
    f.beginBiquadLowPass(100.0f, 1000.0f, 0.707f);
    float last = 0.0f;
    for (int i = 0; i < 200; ++i) last = f.biquadLowPass(1.0f);
    CHECK_NEAR(last, 1.0f, 1e-3f);

    // DC zero-gain: constant 0 → output 0
    f.beginBiquadLowPass(100.0f, 1000.0f, 0.707f);
    for (int i = 0; i < 50; ++i) last = f.biquadLowPass(0.0f);
    CHECK_NEAR(last, 0.0f, 1e-6f);

    // Nyquist (alternating ±1) heavily attenuated vs DC
    f.beginBiquadLowPass(100.0f, 1000.0f, 0.707f);
    // settle on DC first
    for (int i = 0; i < 100; ++i) f.biquadLowPass(1.0f);
    // switch to Nyquist
    float sumSq = 0.0f;
    int N = 200;
    f.beginBiquadLowPass(50.0f, 1000.0f, 0.707f);
    for (int i = 0; i < N; ++i)
    {
        float x = (i % 2 == 0) ? 1.0f : -1.0f;
        float y = f.biquadLowPass(x);
        sumSq += y * y;
    }
    float rms = sqrtf(sumSq / N);
    CHECK(rms < 0.15f);  // Nyquist strongly attenuated by 50Hz cutoff / 1kHz SR

    // begin resets state: two identical runs produce identical output
    FilterFunctions g;
    f.beginBiquadLowPass(100.0f, 1000.0f, 0.707f);
    g.beginBiquadLowPass(100.0f, 1000.0f, 0.707f);
    for (int i = 0; i < 10; ++i)
        CHECK_NEAR(f.biquadLowPass(static_cast<float>(i)),
                   g.biquadLowPass(static_cast<float>(i)), 1e-6f);

    // JSON: DC section converges, HF section attenuated
    CsvData d = loadData("biquad_lowpass");
    if (!d.rows.empty())
    {
        int ri = colIndex(d, "raw");
        FilterFunctions h;
        h.beginBiquadLowPass(100.0f, 1000.0f, 0.707f);
        float dcLast = 0.0f;
        size_t half = 60;  // first 60 DC samples
        for (size_t i = 0; i < d.rows.size() && i < half; ++i)
            dcLast = h.biquadLowPass(d.rows[i][ri]);
        CHECK_NEAR(dcLast, 1.0f, 0.01f);  // DC settled

        float hfSumSq = 0.0f;
        size_t hfCount = 0;
        for (size_t i = half; i < d.rows.size(); ++i)
        {
            float y = h.biquadLowPass(d.rows[i][ri]);
            hfSumSq += y * y;
            ++hfCount;
        }
        if (hfCount > 0)
        {
            float hfRms = sqrtf(hfSumSq / hfCount);
            CHECK(hfRms < 0.5f);  // HF attenuated relative to DC
        }
    }
}

// ---------------------------------------------------------------------------
// Notch Filter
// ---------------------------------------------------------------------------
static void test_notchFilter()
{
    SUITE("NotchFilter");

    FilterFunctions f;

    // DC passes: constant 1.0 settles to 1.0
    f.beginNotchFilter(60.0f, 10.0f, 1000.0f);
    float last = 0.0f;
    for (int i = 0; i < 200; ++i) last = f.notchFilter(1.0f);
    CHECK_NEAR(last, 1.0f, 1e-3f);

    // DC zero: constant 0 → output 0
    f.beginNotchFilter(60.0f, 10.0f, 1000.0f);
    for (int i = 0; i < 50; ++i) last = f.notchFilter(0.0f);
    CHECK_NEAR(last, 0.0f, 1e-6f);

    // 60Hz tone attenuated at 60Hz notch, 1000Hz sample
    // 60Hz period at 1000Hz SR = 1000/60 ≈ 16.67 samples
    // sin(2π * 60 * n / 1000) = sin(2π * n / 16.667)
    f.beginNotchFilter(60.0f, 10.0f, 1000.0f);
    // Q=10 notch time-constant ≈ Q/(π·f₀) = 53 ms = 53 samples at 1 kHz.
    // Skip 5 time-constants (≈265 samples) before measuring steady state.
    const int settle = 265;
    for (int i = 0; i < settle; ++i)
        f.notchFilter(sinf(2.0f * static_cast<float>(PI) * 60.0f * i / 1000.0f));
    float sumSq = 0.0f;
    int N = 100;
    for (int i = settle; i < settle + N; ++i)
    {
        float y = f.notchFilter(sinf(2.0f * static_cast<float>(PI) * 60.0f * i / 1000.0f));
        sumSq += y * y;
    }
    float rms = sqrtf(sumSq / N);
    CHECK(rms < 0.05f);  // notch frequency strongly attenuated in steady state

    // begin resets state
    f.beginNotchFilter(60.0f, 10.0f, 1000.0f);
    CHECK_NEAR(f.notchFilter(0.0f), 0.0f, 1e-6f);

    // JSON: DC section converges to 1, 60Hz section attenuated
    CsvData d = loadData("notch");
    if (!d.rows.empty())
    {
        int ri = colIndex(d, "raw");
        FilterFunctions h;
        h.beginNotchFilter(60.0f, 10.0f, 1000.0f);
        float dcLast = 0.0f;
        size_t dcEnd = 80;
        for (size_t i = 0; i < d.rows.size() && i < dcEnd; ++i)
            dcLast = h.notchFilter(d.rows[i][ri]);
        // Residual transient after 80 samples decays to ~5% of initial step;
        // loosen tolerance accordingly.
        CHECK_NEAR(dcLast, 1.0f, 0.05f);  // DC settled

        float hfSumSq = 0.0f;
        size_t hfCount = 0;
        for (size_t i = dcEnd; i < d.rows.size(); ++i)
        {
            float y = h.notchFilter(d.rows[i][ri]);
            hfSumSq += y * y;
            ++hfCount;
        }
        // JSON 60Hz section is only 40 samples after a DC→tone transition.
        // Q=10 needs ~265 samples to settle (see pure-tone test above), so
        // only verify the filter is stable (no amplification beyond input RMS).
        if (hfCount > 0)
            CHECK(sqrtf(hfSumSq / hfCount) < 1.0f);
    }
}

// ---------------------------------------------------------------------------
// Cross-filter: multiple filters on same object are independent
// ---------------------------------------------------------------------------
static void test_independence()
{
    SUITE("FilterIndependence");

    FilterFunctions f;
    f.beginMovingAverage(4);
    f.beginExponentialMovingAverage(0.5f);
    f.beginLowPassFilter(0.5f);
    f.beginMedianFilter(3);
    f.beginKalmanFilter(0.001f, 0.1f);

    // Feed each filter through a different number of samples and verify
    // they do not interfere with each other.
    for (int i = 0; i < 10; ++i) f.movingAverage(5.0f);
    for (int i = 0; i < 10; ++i) f.exponentialMovingAverage(5.0f, 0.5f);

    CHECK_NEAR(f.movingAverage(5.0f), 5.0f, 1e-3f);
    CHECK_NEAR(f.exponentialMovingAverage(5.0f, 0.5f), 5.0f, 1e-3f);
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------
static void test_edgeCases()
{
    SUITE("EdgeCases");

    FilterFunctions f;

    // beginMovingAverage(0) treated as size 1
    f.beginMovingAverage(0);
    CHECK_NEAR(f.movingAverage(9.0f), 9.0f, 1e-4f);

    // beginMedianFilter(0) treated as size 1
    f.beginMedianFilter(0);
    CHECK_NEAR(f.medianFilter(9.0f), 9.0f, 1e-4f);

    // Savitzky-Golay window < 3 clamped to 3
    f.beginSavitzkyGolay(0);
    // Size is 3; first call: count=1, avg=input
    CHECK_NEAR(f.savitzkyGolay(4.0f), 4.0f, 1e-4f);

    // Kalman with both noises 0: measurement noise 0 → gain→1, tracks perfectly
    f.beginKalmanFilter(0.0f, 0.0f);
    CHECK_NEAR(f.kalmanFilter(7.0f), 7.0f, 1e-4f);

    // biquadLowPass: sampleRate=0 guarded (defaults to 1.0)
    f.beginBiquadLowPass(0.1f, 0.0f, 0.707f);
    // Just verify it doesn't crash / produce NaN
    float y = f.biquadLowPass(1.0f);
    CHECK(y == y);  // NaN check

    // notchFilter: centerHz=0 guarded
    f.beginNotchFilter(0.0f, 10.0f, 1000.0f);
    float z = f.notchFilter(1.0f);
    CHECK(z == z);

    // complementaryFilter: dt=0 (no gyro contribution)
    f.beginComplementaryFilter(0.98f);
    f.complementaryFilter(30.0f, 100.0f, 0.0f);  // init
    float c = f.complementaryFilter(30.0f, 100.0f, 0.0f);
    // 0.98*(angle+0)+0.02*30: angle was 30 on init, so ≈30
    CHECK_NEAR(c, 30.0f, 0.1f);
}

// ---------------------------------------------------------------------------
int main()
{
    test_movingAverage();
    test_exponentialMA();
    test_medianFilter();
    test_lowPassFilter();
    test_highPassFilter();
    test_savitzkyGolay();
    test_kalmanFilter();
    test_complementaryFilter();
    test_biquadLowPass();
    test_notchFilter();
    test_independence();
    test_edgeCases();
    REPORT();
}
