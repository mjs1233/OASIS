#include "SHT31.hpp"

#include <array>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

namespace oasis {
    namespace {
        constexpr std::array<uint8_t, 2> SINGLE_SHOT_HIGH_REPEATABILITY {0x24, 0x00};
        constexpr char TAG[] = "SHT31";
        // High-repeatability single-shot conversion needs up to about 16 ms.
        // Use 25 ms to remain safe with FreeRTOS tick rounding and bus latency.
        constexpr TickType_t MEASUREMENT_WAIT_TICKS = pdMS_TO_TICKS(25);

        uint8_t crc8(std::span<const uint8_t, 2> bytes) {
            uint8_t crc = 0xFF;
            for (uint8_t value : bytes) {
                crc ^= value;
                for (int bit = 0; bit < 8; ++bit) {
                    crc = (crc & 0x80) != 0 ? static_cast<uint8_t>((crc << 1) ^ 0x31) : crc << 1;
                }
            }
            return crc;
        }
    }

    esp_err_t SHT31::read_measurement(SHT31Measurement& measurement) {
        esp_err_t result = m_i2c.write(ADDRESS, SINGLE_SHOT_HIGH_REPEATABILITY);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "measurement command failed (addr=0x%02X): %s", ADDRESS,
                     esp_err_to_name(result));
            return result;
        }

        // The conversion runs inside SHT31; do not hold the shared I2C bus while waiting.
        vTaskDelay(MEASUREMENT_WAIT_TICKS);

        std::array<uint8_t, 6> response {};
        result = m_i2c.read(ADDRESS, response);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "measurement read failed (addr=0x%02X): %s", ADDRESS,
                     esp_err_to_name(result));
            return result;
        }
        ESP_LOGI(TAG, "raw: %02X %02X %02X %02X %02X %02X", response[0], response[1],
                 response[2], response[3], response[4], response[5]);
        if (crc8(std::span<const uint8_t, 2>(response.data(), 2)) != response[2] ||
            crc8(std::span<const uint8_t, 2>(response.data() + 3, 2)) != response[5]) {
            ESP_LOGE(TAG, "CRC validation failed");
            return ESP_ERR_INVALID_CRC;
        }

        const uint16_t raw_temperature = static_cast<uint16_t>(response[0] << 8 | response[1]);
        const uint16_t raw_humidity = static_cast<uint16_t>(response[3] << 8 | response[4]);
        measurement.temperature_c = -45.0f + 175.0f * raw_temperature / 65535.0f;
        measurement.humidity_percent = 100.0f * raw_humidity / 65535.0f;
        ESP_LOGI(TAG, "measurement: temperature=%.2f C humidity=%.2f %%RH",
                 measurement.temperature_c, measurement.humidity_percent);
        return ESP_OK;
    }
} // namespace oasis
