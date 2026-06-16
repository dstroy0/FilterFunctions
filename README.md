# FilterFunctions

FilterFunctions is an Arduino library that provides a wide range of sensor data filters for smoothing, noise reduction, and signal conditioning.

## Features

- Moving average
- Exponential moving average
- Median filter
- One-pole low-pass filter
- One-pole high-pass filter
- Savitzky-Golay smoothing
- Kalman filter
- Complementary filter
- Biquad low-pass filter
- Notch filter

## Installation

Copy the `FilterFunctions` folder into your Arduino `libraries` directory, or use the Arduino Library Manager if added.

## Usage

```cpp
#include <FilterFunctions.h>

FilterFunctions filter;

void setup() {
  Serial.begin(9600);
  filter.beginMovingAverage(10);
  filter.beginKalmanFilter(0.01, 0.1);
}

void loop() {
  float sensor = analogRead(A0) * (5.0 / 1023.0);
  float smoothed = filter.movingAverage(sensor);
  float filtered = filter.kalmanFilter(sensor);
  Serial.println(filtered, 3);
  delay(20);
}
```

## Example

See `examples/FilterExample/FilterExample.ino` for a live demonstration of all filters.

## License

This library is licensed under the GNU Affero General Public License v3.0.

© 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
