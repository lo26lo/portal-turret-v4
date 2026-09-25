#pragma once

#include <Adafruit_LSM6DSOX.h>
#include <Arduino.h>
#include <Wire.h>

#include "settings/Settings.h"

// Turret2 IMU: LSM6DSOX on the shared I2C bus, address 0x6A (SA0 to GND),
// WHO_AM_I 0x6C, 104 Hz, +-4 g (plan §2.4, boot step 6).
// The board is mounted vertically: which axis points up is the ImuUpAxis
// setting (0 = not calibrated, 1..6 = +X, -X, +Y, -Y, +Z, -Z).
class Motion {
public:
  void Initialize(Settings &settings);
  void Update(ulong deltaTime);

  bool IsAvailable() const { return available; }
  // Low-pass filtered acceleration (m/s^2) and latest angular rate (rad/s).
  const sensors_vec_t &GetAcceleration() const { return gravity; }
  const sensors_vec_t &GetGyro() const { return gyro; }
  float GetTemperature() const { return temperature; }

  // Axis carrying most of the gravity, as an ImuUpAxis value (1..6), 0 if unknown.
  uint8_t DominantAxis() const;
  // Up axis calibrated and gravity along it.
  bool IsUpright() const;
  // Wings may open: upright, or not checkable (IMU missing, axis not calibrated).
  bool CanDeploy() const;

private:
  Adafruit_LSM6DSOX imu;
  Settings *settings = nullptr;
  bool available = false;
  bool hasReading = false;
  ulong sinceRead = 0;
  sensors_vec_t gravity = {};
  sensors_vec_t gyro = {};
  float temperature = 0;
};
