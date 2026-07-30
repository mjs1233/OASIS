//
// Created by tgian on 26. 7. 26..
//

#ifndef FIRMWARE_IMUDATA_HPP
#define FIRMWARE_IMUDATA_HPP

struct IMUData {
    float accel_x = 0;
    float accel_y = 0;
    float accel_z = 0;
    float gyro_x = 0;
    float gyro_y = 0;
    float gyro_z = 0;
    float temp_c = 0;
    bool is_last = true;
};

#endif //FIRMWARE_IMUDATA_HPP
