#ifndef FIRMWARE_FIRMWARE_H
#define FIRMWARE_FIRMWARE_H

#include <optional>

// Set to 0 to build without accessing the MAX30102/PPG hardware. The 30-second
// measurement cycle and worker-data packet remain active; BPM is sent as zero.
#ifndef OASIS_ENABLE_PPG_SENSOR
#define OASIS_ENABLE_PPG_SENSOR 1
#endif

// Set to 0 to keep the IMU task alive without initializing or accessing the
// MPU6050, its I2C bus, or its sampling timer.
#ifndef OASIS_ENABLE_IMU_SENSOR
#define OASIS_ENABLE_IMU_SENSOR 0
#endif

// Set to 0 to build without initializing the NTC ADC path or reading the NTC.
#ifndef OASIS_ENABLE_NTC_SENSOR
#define OASIS_ENABLE_NTC_SENSOR 1
#endif

// Set to 0 to build without constructing or reading the SHT31 sensor.
#ifndef OASIS_ENABLE_SHT31_SENSOR
#define OASIS_ENABLE_SHT31_SENSOR 1
#endif

namespace oasis {
    void startup();
}
#endif //FIRMWARE_FIRMWARE_H
