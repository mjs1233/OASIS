#pragma once

#include "core/I2CUnit.hpp"

namespace oasis {

    struct SHT31Measurement {
        float temperature_c = 0.0f;
        float humidity_percent = 0.0f;
    };

    class SHT31 {
    public:
        explicit SHT31(I2CUnit& i2c) : m_i2c(i2c) {}

        [[nodiscard]] esp_err_t read_measurement(SHT31Measurement& measurement);

    private:
        static constexpr uint8_t ADDRESS = 0x44;
        I2CUnit& m_i2c;
    };

} // namespace oasis
