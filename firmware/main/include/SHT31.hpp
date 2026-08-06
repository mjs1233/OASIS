//
// SHT31 humidity/temperature sensor driver (I2C).
//

#ifndef FIRMWARE_SHT31_HPP
#define FIRMWARE_SHT31_HPP
#include <cinttypes>
#include <cstddef>
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "core/StagingBuffer.hpp"

namespace oasis {
    class SHT31 {
    public:
        SHT31();
        ~SHT31();

        bool start();

        bool read();

        // Returns the last successfully staged temperature, in Celsius.
        float get_temperature_c();

        // Returns the last successfully staged relative humidity, in %RH.
        float get_humidity_rh();

    private:
        bool soft_reset();
        bool check_status();
        static uint8_t crc8(const uint8_t* data, size_t len);

        i2c_master_bus_handle_t bus_handle_ = nullptr;
        i2c_master_dev_handle_t dev_handle_ = nullptr;

        bool m_started = false;

        core::StagingBuffer<float> m_temperature_c {};
        core::StagingBuffer<float> m_humidity_rh {};

        // TODO: confirm actual board wiring / I2C port assignment.
        static constexpr gpio_num_t I2C_SDA_IO  = GPIO_NUM_8;
        static constexpr gpio_num_t I2C_SCL_IO  = GPIO_NUM_9;
        static constexpr i2c_port_t I2C_PORT    = I2C_NUM_1;
        static constexpr uint32_t   I2C_FREQ_HZ = 400000;
        static constexpr uint16_t   SHT31_ADDR  = 0x44;
    };
}

#endif //FIRMWARE_SHT31_HPP
