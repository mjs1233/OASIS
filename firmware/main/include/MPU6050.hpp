#ifndef FIRMWARE_MPU6050_HPP
#define FIRMWARE_MPU6050_HPP
#include "IMUData.hpp"
#include "core/I2CUnit.hpp"
namespace oasis {
    class MPU6050 {

    public:
        explicit MPU6050(I2CUnit& i2c);
        ~MPU6050() = default;

        MPU6050(const MPU6050&) = delete;
        MPU6050(MPU6050&&) = delete;
        MPU6050& operator=(const MPU6050&) = delete;
        MPU6050& operator=(MPU6050&&) = delete;

        bool read_fifo_buffer(IMUData* data);

    private:
        esp_err_t init();

        static constexpr uint8_t  MPU6050_ADDR      = 0x68;
        static constexpr size_t   BYTES_PER_SAMPLE  = 14; // TEMP(2)+GYRO(6)+ACCEL(6)
        static constexpr size_t   MAX_SAMPLES       = 24; //

        I2CUnit& m_i2c;
    };
}
#endif
