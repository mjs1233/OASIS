#ifndef FIRMWARE_MPU6050_HPP
#define FIRMWARE_MPU6050_HPP
#include "IMUData.hpp"
#include "driver/i2c_master.h"
namespace oasis {
    class MPU6050 {

    public:
        MPU6050();
        ~MPU6050();

        MPU6050(const MPU6050&) = delete;
        MPU6050(MPU6050&&) = delete;
        MPU6050& operator=(const MPU6050&) = delete;
        MPU6050& operator=(MPU6050&&) = delete;

        bool read_fifo_buffer(IMUData* data);

    private:
        void init();

        static constexpr uint8_t  MPU6050_ADDR      = 0x68;
        static constexpr size_t   BYTES_PER_SAMPLE  = 14; // TEMP(2)+GYRO(6)+ACCEL(6)
        static constexpr size_t   MAX_SAMPLES       = 24; //

        i2c_master_bus_handle_t bus_handle_ = nullptr;
        i2c_master_dev_handle_t dev_handle_ = nullptr;
    };
}
#endif