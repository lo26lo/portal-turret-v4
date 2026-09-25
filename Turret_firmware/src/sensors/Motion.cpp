#include "board/Log.h"
#include "Motion.h"

namespace {
const uint8_t IMU_ADDRESS = 0x6A;
const ulong READ_PERIOD_MS = 20; // the IMU runs at 104 Hz; no need to poll every loop
const float FILTER = 0.2f;       // weight of a new sample in the gravity estimate
// Upright when at least this share of 1 g lies along the calibrated up axis (~37 deg tilt).
const float UPRIGHT_RATIO = 0.8f;
const float ONE_G = SENSORS_GRAVITY_STANDARD;

float AxisValue(const sensors_vec_t &v, uint8_t axis) {
  switch (axis) {
  case 1: return v.x;
  case 2: return -v.x;
  case 3: return v.y;
  case 4: return -v.y;
  case 5: return v.z;
  case 6: return -v.z;
  }
  return 0;
}

const char *AxisName(uint8_t axis) {
  static const char *names[] = {"?", "+X", "-X", "+Y", "-Y", "+Z", "-Z"};
  return names[axis <= 6 ? axis : 0];
}
} // namespace

void Motion::Initialize(Settings &settingsIn)
{
    settings = &settingsIn;

    // The bus is started by I2cBus::Begin() on PIN_SDA / PIN_SCL (IO35 / IO36),
    // explicitly, before this call (boot step 5). begin_I2C also checks WHO_AM_I.
    available = imu.begin_I2C(IMU_ADDRESS, &Wire);
    if (!available)
    {
        // Not retried: polling a missing sensor every loop slows audio and servos down
        Log.println("Motion: LSM6DSOX not found at 0x6A, disabled");
        return;
    }
    // Library defaults, set again so that they do not depend on its version.
    imu.setAccelRange(LSM6DS_ACCEL_RANGE_4_G);
    imu.setAccelDataRate(LSM6DS_RATE_104_HZ);
    imu.setGyroDataRate(LSM6DS_RATE_104_HZ);

    // First reading straight away, so that the boot check has a value.
    Update(READ_PERIOD_MS);
    Log.printf("Motion: LSM6DSOX ok, gravity %.1f %.1f %.1f m/s2, dominant axis %s, up axis setting %s\n",
                  gravity.x, gravity.y, gravity.z, AxisName(DominantAxis()),
                  AxisName(settings->GetInt(SettingId::ImuUpAxis)));
    if (settings->GetInt(SettingId::ImuUpAxis) != 0 && !IsUpright())
    {
        Log.println("Motion: turret not upright, wings will stay closed");
    }
}

void Motion::Update(ulong deltaTime)
{
    if (!available)
    {
        return;
    }
    sinceRead += deltaTime;
    if (sinceRead < READ_PERIOD_MS)
    {
        return;
    }
    sinceRead = 0;

    sensors_event_t accelEvent, gyroEvent, tempEvent;
    // Three pointers: the README's "getEvent() is unchanged" is wrong (plan §2.4).
    if (!imu.getEvent(&accelEvent, &gyroEvent, &tempEvent))
    {
        return;
    }
    const sensors_vec_t &a = accelEvent.acceleration;
    if (!hasReading)
    {
        gravity = a;
        hasReading = true;
    }
    else
    {
        gravity.x += FILTER * (a.x - gravity.x);
        gravity.y += FILTER * (a.y - gravity.y);
        gravity.z += FILTER * (a.z - gravity.z);
    }
    gyro = gyroEvent.gyro;
    temperature = tempEvent.temperature;
}

uint8_t Motion::DominantAxis() const
{
    if (!hasReading)
    {
        return 0;
    }
    uint8_t best = 0;
    float bestValue = 0;
    for (uint8_t axis = 1; axis <= 6; axis++)
    {
        float value = AxisValue(gravity, axis);
        if (value > bestValue)
        {
            bestValue = value;
            best = axis;
        }
    }
    return best;
}

bool Motion::IsUpright() const
{
    uint8_t upAxis = settings ? settings->GetInt(SettingId::ImuUpAxis) : 0;
    if (!hasReading || upAxis == 0)
    {
        return false;
    }
    return AxisValue(gravity, upAxis) >= UPRIGHT_RATIO * ONE_G;
}

bool Motion::CanDeploy() const
{
    if (!available || !hasReading)
    {
        return true; // not checkable: red LED code 3 already reports it
    }
    if (settings->GetInt(SettingId::ImuUpAxis) == 0)
    {
        return true;
    }
    return IsUpright();
}
