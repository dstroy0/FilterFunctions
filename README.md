# FilterFunctions

Arduino sensor filtering library providing ten signal-processing filters for smoothing, noise reduction, and signal conditioning. Compiles under both the Arduino IDE and standard C++11 (`g++ -std=c++11`) with no external dependencies.

**Version:** 1.0.0  
**Author:** Douglas Quigg (dstroy0) \<dquigg123@gmail.com\>  
**License:** GNU Affero General Public License v3.0

---

## Table of Contents

1. [Installation](#installation)
2. [Quick Start](#quick-start)
3. [API Reference](#api-reference)
   - [Moving Average](#1-moving-average)
   - [Exponential Moving Average](#2-exponential-moving-average)
   - [Median Filter](#3-median-filter)
   - [Low-Pass Filter](#4-low-pass-filter)
   - [High-Pass Filter](#5-high-pass-filter)
   - [Savitzky-Golay Filter](#6-savitzky-golay-filter)
   - [Kalman Filter](#7-kalman-filter)
   - [Complementary Filter](#8-complementary-filter)
   - [Biquad Low-Pass Filter](#9-biquad-low-pass-filter)
   - [Notch Filter](#10-notch-filter)
4. [Choosing a Filter](#choosing-a-filter)
5. [Running Tests](#running-tests)
6. [License](#license)

---

## Installation

Copy the `FilterFunctions` folder into your Arduino `libraries` directory:

```
<Arduino sketchbook>/libraries/FilterFunctions/
    src/FilterFunctions.h
    src/FilterFunctions.cpp
    library.properties
    examples/FilterExample/FilterExample.ino
```

Restart the Arduino IDE and include the library:

```cpp
#include <FilterFunctions.h>
```

---

## Quick Start

```cpp
#include <FilterFunctions.h>

FilterFunctions filter;

void setup() {
    Serial.begin(9600);
    filter.beginMovingAverage(10);
    filter.beginKalmanFilter(0.01f, 0.1f);
}

void loop() {
    float raw = analogRead(A0) * (5.0f / 1023.0f);
    float smoothed = filter.movingAverage(raw);
    float estimated = filter.kalmanFilter(raw);
    Serial.print("smooth="); Serial.print(smoothed, 3);
    Serial.print("  kf=");   Serial.println(estimated, 3);
    delay(20);
}
```

One `FilterFunctions` instance manages all ten filters simultaneously. Call `begin*()` once in `setup()`, then call the filter function each loop iteration.

---

## API Reference

### 1. Moving Average

Computes the mean of the last `size` samples using a circular buffer. Reduces random noise but introduces lag proportional to the window size.

```cpp
void  beginMovingAverage(uint8_t size);
float movingAverage(float input);
```

| Parameter | Type      | Description                          |
|-----------|-----------|--------------------------------------|
| `size`    | `uint8_t` | Window length in samples (minimum 1) |
| `input`   | `float`   | New sample                           |
| **returns** | `float` | Mean of the last `size` samples      |

**Algorithm:** `MA[n] = (x[n] + x[n-1] + ... + x[n-size+1]) / size`

Buffer is zero-initialized, so the first `size - 1` outputs are biased low until the window is full.

```cpp
filter.beginMovingAverage(8);        // 8-sample window
float out = filter.movingAverage(sensor);
```

---

### 2. Exponential Moving Average

Applies exponentially decaying weights to past samples with a single accumulator. No buffer allocation; constant memory.

```cpp
void  beginExponentialMovingAverage(float alpha);
float exponentialMovingAverage(float input, float alpha);
```

| Parameter | Type    | Description                                                    |
|-----------|---------|----------------------------------------------------------------|
| `alpha`   | `float` | Smoothing factor, clamped to [0, 1]                           |
| `input`   | `float` | New sample                                                     |
| **returns** | `float` | Current EMA value                                           |

**Algorithm:** `EMA[n] = alpha * x[n] + (1 - alpha) * EMA[n-1]`

The `alpha` passed to `exponentialMovingAverage()` overrides the value set in `beginExponentialMovingAverage()`, allowing dynamic changes. On the first call the output is seeded to the first input sample.

| `alpha` | Behavior                                    |
|---------|---------------------------------------------|
| 0.0     | Output never changes (infinite smoothing)   |
| 0.1     | Heavy smoothing, slow response              |
| 0.5     | Moderate smoothing                          |
| 1.0     | No smoothing, output equals input           |

```cpp
filter.beginExponentialMovingAverage(0.1f);
float out = filter.exponentialMovingAverage(sensor, 0.1f);
```

---

### 3. Median Filter

Returns the middle value of the last `size` samples. Highly effective at rejecting isolated spike noise (impulse noise) without blurring edges.

```cpp
void  beginMedianFilter(uint8_t size);
float medianFilter(float input);
```

| Parameter | Type      | Description                                  |
|-----------|-----------|----------------------------------------------|
| `size`    | `uint8_t` | Window length in samples (max 16, minimum 1) |
| `input`   | `float`   | New sample                                   |
| **returns** | `float` | Median of the current window                 |

Uses an insertion sort on a local copy of the window. During warmup (before the window is full), only the samples received so far are sorted.

```cpp
filter.beginMedianFilter(5);
float out = filter.medianFilter(sensor);
```

Window size is capped internally at 16 to keep the sort buffer on the stack.

---

### 4. Low-Pass Filter

Single-pole IIR low-pass filter. Attenuates high-frequency noise while passing slow signal changes. Minimal memory (one state variable).

```cpp
void  beginLowPassFilter(float alpha);
float lowPassFilter(float input, float alpha);
```

| Parameter | Type    | Description                          |
|-----------|---------|--------------------------------------|
| `alpha`   | `float` | Smoothing factor, clamped to [0, 1]  |
| `input`   | `float` | New sample                           |
| **returns** | `float` | Filtered output                    |

**Algorithm:** `y[n] = y[n-1] + alpha * (x[n] - y[n-1])`

Approximate cutoff frequency: `f_c ≈ (alpha * f_s) / (2 * pi)`

| `alpha` | Behavior                                   |
|---------|--------------------------------------------|
| 0.01    | Very heavy smoothing, very slow response   |
| 0.1     | Moderate smoothing (good general default)  |
| 0.5     | Light smoothing                            |
| 1.0     | No smoothing, output equals input          |

```cpp
filter.beginLowPassFilter(0.1f);
float out = filter.lowPassFilter(sensor, 0.1f);
```

---

### 5. High-Pass Filter

Single-pole IIR high-pass filter. Removes DC bias and slow drift; passes rapid changes. Useful for detecting motion events or vibration above a threshold frequency.

```cpp
void  beginHighPassFilter(float alpha);
float highPassFilter(float input, float alpha);
```

| Parameter | Type    | Description                          |
|-----------|---------|--------------------------------------|
| `alpha`   | `float` | Filter coefficient, clamped to [0, 1] |
| `input`   | `float` | New sample                           |
| **returns** | `float` | Filtered output                    |

**Algorithm:** `y[n] = alpha * (y[n-1] + x[n] - x[n-1])`

Approximate cutoff frequency (high-pass): `f_c ≈ f_s * (1 - alpha) / (2 * pi)`

| `alpha` | Behavior                                              |
|---------|-------------------------------------------------------|
| 0.5     | Aggressive — only very high frequencies pass          |
| 0.9     | Moderate — removes DC and slow drift                  |
| 0.95    | Light — passes signals above ~0.4 Hz at 50 Hz SR      |
| 1.0     | All-pass (derivative of input)                        |

```cpp
filter.beginHighPassFilter(0.95f);
float out = filter.highPassFilter(sensor, 0.95f);
```

---

### 6. Savitzky-Golay Filter

Fits a degree-2 (quadratic) polynomial through a symmetric window and evaluates it at the center point. Preserves peak heights and linear trends better than a plain moving average.

```cpp
void  beginSavitzkyGolay(uint8_t windowSize);
float savitzkyGolay(float input);
```

| Parameter    | Type      | Description                                        |
|--------------|-----------|----------------------------------------------------|
| `windowSize` | `uint8_t` | Window length: 3, 5, or 7 (forced odd, clamped)   |
| `input`      | `float`   | New sample                                         |
| **returns**  | `float`   | Smoothed estimate of the center sample             |

**Algorithm:** Centered least-squares polynomial convolution using Savitzky & Golay (1964) Table III coefficients:

| Window | Coefficients (oldest → newest)       | Sum check |
|--------|--------------------------------------|-----------|
| 3      | `[1, 1, 1] / 3`                      | 1.0       |
| 5      | `[-3, 12, 17, 12, -3] / 35`          | 1.0       |
| 7      | `[-2, 3, 6, 7, 6, 3, -2] / 21`       | 1.0       |

**Output lag:** The filter is centered, so it outputs a smoothed estimate of the sample that arrived `(windowSize - 1) / 2` calls ago (1 sample lag for window-3, 2 for window-5, 3 for window-7).

**Warmup:** During the first `windowSize - 1` calls before the buffer is full, a simple moving average is returned.

```cpp
filter.beginSavitzkyGolay(5);   // 2-sample lag, best balance of smoothing/fidelity
float out = filter.savitzkyGolay(sensor);
```

---

### 7. Kalman Filter

Scalar (1-D) discrete Kalman filter. Optimally estimates the true value of a noisy scalar measurement when process and measurement noise statistics are known.

```cpp
void  beginKalmanFilter(float processNoise, float measurementNoise);
float kalmanFilter(float input);
```

| Parameter           | Type    | Description                                                  |
|---------------------|---------|--------------------------------------------------------------|
| `processNoise`      | `float` | Q — expected variance of state change per step (≥ 0)        |
| `measurementNoise`  | `float` | R — expected variance of sensor readings (≥ 0)              |
| `input`             | `float` | New measurement                                              |
| **returns**         | `float` | Filtered state estimate                                      |

**Algorithm:**

```
Predict:  P = P + Q
Update:   K = P / (P + R)
          x = x + K * (measurement - x)
          P = (1 - K) * P
```

Tuning guide:

| Scenario                        | Q            | R           |
|---------------------------------|--------------|-------------|
| Slow-moving signal, noisy ADC   | 0.001–0.01   | 0.1–1.0     |
| Fast-moving signal, clean ADC   | 0.1–1.0      | 0.01–0.1    |
| Aggressively smooth             | very small Q | large R     |
| Track quickly                   | large Q      | small R     |

On the first call the state is seeded to the first measurement.

```cpp
filter.beginKalmanFilter(0.01f, 0.1f);
float est = filter.kalmanFilter(sensor);
```

---

### 8. Complementary Filter

Fuses an accelerometer angle and a gyroscope rate to estimate tilt angle, commonly used for IMU attitude estimation (roll/pitch). Blends the gyro's short-term accuracy with the accelerometer's long-term stability.

```cpp
void  beginComplementaryFilter(float alpha);
float complementaryFilter(float accelAngle, float gyroRate, float dt);
```

| Parameter    | Type    | Description                                                        |
|--------------|---------|--------------------------------------------------------------------|
| `alpha`      | `float` | Gyro trust weight, clamped to [0, 1]; typically 0.95–0.99         |
| `accelAngle` | `float` | Angle derived from accelerometer (degrees)                         |
| `gyroRate`   | `float` | Angular rate from gyroscope (degrees/second)                       |
| `dt`         | `float` | Time since last call in seconds; must match actual loop period     |
| **returns**  | `float` | Fused angle estimate (degrees)                                     |

**Algorithm:** `angle = alpha * (angle + gyroRate * dt) + (1 - alpha) * accelAngle`

On the first call the angle is seeded to `accelAngle`.

| `alpha` | Behavior                                          |
|---------|---------------------------------------------------|
| 0.90    | More trust on accelerometer; drifts less          |
| 0.95    | Balanced                                          |
| 0.98    | Strong gyro trust; slower accel correction        |
| 0.99    | Very strong gyro trust; slow drift correction     |

`dt` should match the actual elapsed time between calls. At a fixed loop rate, use `dt = loopPeriodMs / 1000.0f`.

```cpp
filter.beginComplementaryFilter(0.98f);
float angle = filter.complementaryFilter(accelAngle, gyroRate, 0.02f);
```

---

### 9. Biquad Low-Pass Filter

Second-order IIR low-pass filter using the Audio EQ Cookbook (RBJ) design, implemented as Direct Form I. Provides -40 dB/decade rolloff above the cutoff, sharper than the single-pole filter.

```cpp
void  beginBiquadLowPass(float cutoffHz, float sampleRate, float q = 0.707f);
float biquadLowPass(float input);
```

| Parameter    | Type    | Description                                                           |
|--------------|---------|-----------------------------------------------------------------------|
| `cutoffHz`   | `float` | -3 dB cutoff frequency in Hz                                          |
| `sampleRate` | `float` | Sample rate in Hz (must match your loop rate)                         |
| `q`          | `float` | Quality factor; default 0.707 (Butterworth, maximally flat passband)  |
| `input`      | `float` | New sample                                                            |
| **returns**  | `float` | Filtered output                                                        |

**Q factor guide:**

| Q      | Behavior                                                  |
|--------|-----------------------------------------------------------|
| 0.5    | Overdamped; gentle rolloff, no peak                       |
| 0.707  | Butterworth; flattest passband, -3 dB at cutoff           |
| 1.0    | Slight peak just below cutoff                             |
| >1.0   | Resonant peak; useful for tone shaping, not sensor work   |

Call `beginBiquadLowPass()` again if the sample rate changes. State (delay registers) is reset on each `begin*` call.

```cpp
filter.beginBiquadLowPass(5.0f, 50.0f, 0.707f);   // 5 Hz cutoff at 50 Hz SR
float out = filter.biquadLowPass(sensor);
```

---

### 10. Notch Filter

Second-order IIR band-reject (notch) filter using the Audio EQ Cookbook (RBJ) design, Direct Form I. Attenuates a narrow frequency band while passing everything else. Useful for removing powerline interference (50/60 Hz) or motor vibration.

```cpp
void  beginNotchFilter(float centerHz, float q, float sampleRate);
float notchFilter(float input);
```

| Parameter    | Type    | Description                                                       |
|--------------|---------|-------------------------------------------------------------------|
| `centerHz`   | `float` | Center (notch) frequency in Hz                                    |
| `q`          | `float` | Quality factor; higher Q = narrower notch                         |
| `sampleRate` | `float` | Sample rate in Hz                                                 |
| `input`      | `float` | New sample                                                        |
| **returns**  | `float` | Filtered output                                                   |

**Q and settling time:**

Settling time ≈ `Q / (pi * centerHz)` seconds. A narrow notch (high Q) takes longer to reach steady state after a transient.

| Q   | 60 Hz notch: -3 dB bandwidth | Settling time |
|-----|------------------------------|---------------|
| 1   | 60 Hz                        | ~5 ms         |
| 5   | 12 Hz                        | ~27 ms        |
| 10  | 6 Hz                         | ~53 ms        |
| 30  | 2 Hz                         | ~159 ms       |

```cpp
filter.beginNotchFilter(60.0f, 5.0f, 1000.0f);   // reject 60 Hz at 1 kHz SR
float out = filter.notchFilter(sensor);
```

---

## Choosing a Filter

| Situation                                      | Recommended filter               |
|------------------------------------------------|----------------------------------|
| General noise reduction, simple               | Moving Average or EMA            |
| Impulse / spike rejection                     | Median Filter                    |
| Smooth peaks and step responses               | Savitzky-Golay (window 5 or 7)  |
| Unknown noise level, optimal estimation       | Kalman Filter                    |
| Remove DC offset / slow drift                 | High-Pass Filter                 |
| Remove powerline hum or motor frequency       | Notch Filter                     |
| Sharp frequency cutoff on audio-rate signals  | Biquad Low-Pass                  |
| IMU tilt angle (accel + gyro fusion)          | Complementary Filter             |

---

## Running Tests

The test suite runs under standard C++11 (no Arduino hardware required).

**Windows (PowerShell):**
```powershell
.\tests\run_tests.ps1
```

**Linux / macOS (Bash):**
```bash
bash tests/run_tests.sh
```

Both scripts compile five binaries, run the unit test suite (142 tests) and example integration tests, print a pass/fail report, then delete the binaries. Requires `g++` with C++11 support in `PATH`; override with `-Compiler` (PowerShell) or `CXX=` (Bash).

---

## License

Licensed under the [GNU Affero General Public License v3.0](https://www.gnu.org/licenses/agpl-3.0.html).

© 2026 Douglas Quigg (dstroy0) \<dquigg123@gmail.com\>
