#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "core/I2CUnit.hpp"

namespace oasis {
    struct PPGSample {
        uint32_t red = 0;
        uint32_t infrared = 0;
    };

    class MAX30102 {
    public:
        explicit MAX30102(I2CUnit& i2c) : m_i2c(i2c) {}

        // Sets a 100 SPS Red+IR configuration. Call start_measurement() only
        // after environment sensors have completed their cycle-start reads.
        [[nodiscard]] esp_err_t check_connection();
        [[nodiscard]] esp_err_t initialize();
        [[nodiscard]] esp_err_t start_measurement();
        [[nodiscard]] esp_err_t stop_measurement();
        [[nodiscard]] esp_err_t read_fifo(std::span<PPGSample> destination, size_t& sample_count);

    private:
        [[nodiscard]] esp_err_t write_register(uint8_t reg, uint8_t value);
        [[nodiscard]] esp_err_t read_register(uint8_t reg, uint8_t& value);

        static constexpr uint8_t ADDRESS = 0x57;
        I2CUnit& m_i2c;
    };
} // namespace oasis
