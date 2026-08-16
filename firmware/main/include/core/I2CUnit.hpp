#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace oasis {

    struct I2CUnitConfig {
        i2c_port_t port;
        gpio_num_t sda_gpio;
        gpio_num_t scl_gpio;
        uint32_t frequency_hz = 400'000;
        bool enable_internal_pullups = false;
    };

    // Owns one ESP-IDF I2C master bus. All device transactions are serialized
    // through one mutex, while device handles are cached by their 7-bit address.
    class I2CUnit {
    public:
        explicit I2CUnit(const I2CUnitConfig& config);
        ~I2CUnit();

        I2CUnit(const I2CUnit&) = delete;
        I2CUnit(I2CUnit&&) = delete;
        I2CUnit& operator=(const I2CUnit&) = delete;
        I2CUnit& operator=(I2CUnit&&) = delete;

        [[nodiscard]] esp_err_t write(uint8_t address,
                                      std::span<const uint8_t> payload,
                                      TickType_t timeout = pdMS_TO_TICKS(20));
        [[nodiscard]] esp_err_t read(uint8_t address,
                                     std::span<uint8_t> payload,
                                     TickType_t timeout = pdMS_TO_TICKS(20));
        [[nodiscard]] esp_err_t write_read(uint8_t address,
                                           std::span<const uint8_t> write_payload,
                                           std::span<uint8_t> read_payload,
                                           TickType_t timeout = pdMS_TO_TICKS(20));

    private:
        struct DeviceSlot {
            uint8_t address = 0;
            i2c_master_dev_handle_t handle = nullptr;
        };

        [[nodiscard]] esp_err_t lock(TickType_t timeout);
        void unlock();
        [[nodiscard]] esp_err_t get_device_locked(uint8_t address, i2c_master_dev_handle_t& device);

        I2CUnitConfig m_config;
        i2c_master_bus_handle_t m_bus = nullptr;
        SemaphoreHandle_t m_mutex = nullptr;
        std::array<DeviceSlot, 8> m_devices {};
    };

} // namespace oasis
