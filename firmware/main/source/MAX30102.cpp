#include "MAX30102.hpp"

#include <algorithm>
#include <array>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

namespace oasis {
    namespace {
        constexpr char TAG[] = "MAX30102";
        constexpr uint8_t REG_FIFO_WR_PTR = 0x04;
        constexpr uint8_t REG_OVF_COUNTER = 0x05;
        constexpr uint8_t REG_FIFO_RD_PTR = 0x06;
        constexpr uint8_t REG_FIFO_DATA = 0x07;
        constexpr uint8_t REG_FIFO_CONFIG = 0x08;
        constexpr uint8_t REG_MODE_CONFIG = 0x09;
        constexpr uint8_t REG_SPO2_CONFIG = 0x0A;
        constexpr uint8_t REG_LED1_PA = 0x0C;
        constexpr uint8_t REG_LED2_PA = 0x0D;
        constexpr uint8_t REG_REVISION_ID = 0xFE;
        constexpr uint8_t REG_PART_ID = 0xFF;
        constexpr uint8_t EXPECTED_PART_ID = 0x15;
        constexpr uint8_t MODE_SHUTDOWN = 0x80;
        constexpr uint8_t MODE_RESET = 0x40;
        constexpr uint8_t MODE_SPO2 = 0x03;
        constexpr size_t FIFO_DEPTH = 32;
    }

    esp_err_t MAX30102::write_register(uint8_t reg, uint8_t value) {
        const std::array<uint8_t, 2> payload {reg, value};
        return m_i2c.write(ADDRESS, payload);
    }

    esp_err_t MAX30102::read_register(uint8_t reg, uint8_t& value) {
        return m_i2c.write_read(ADDRESS, std::span<const uint8_t>(&reg, 1), std::span<uint8_t>(&value, 1));
    }

    esp_err_t MAX30102::check_connection() {
        esp_err_t result = m_i2c.probe(ADDRESS, 20);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "health check: no ACK at 0x%02X: %s", ADDRESS,
                     esp_err_to_name(result));
            return result;
        }

        uint8_t part_id = 0;
        result = read_register(REG_PART_ID, part_id);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "health check: PART_ID read failed: %s", esp_err_to_name(result));
            return result;
        }
        if (part_id != EXPECTED_PART_ID) {
            ESP_LOGE(TAG, "health check: unexpected PART_ID=0x%02X (expected 0x%02X)",
                     part_id, EXPECTED_PART_ID);
            return ESP_ERR_INVALID_RESPONSE;
        }

        uint8_t revision_id = 0;
        result = read_register(REG_REVISION_ID, revision_id);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "health check: REVISION_ID read failed: %s", esp_err_to_name(result));
            return result;
        }

        ESP_LOGI(TAG, "health check OK: address=0x%02X part=0x%02X revision=0x%02X",
                 ADDRESS, part_id, revision_id);
        return ESP_OK;
    }

    esp_err_t MAX30102::initialize() {
        esp_err_t result = write_register(REG_MODE_CONFIG, MODE_RESET);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "initialization failed at reset: %s", esp_err_to_name(result));
            return result;
        }
        vTaskDelay(pdMS_TO_TICKS(10));

        // Keep one ADC conversion per FIFO sample so FIFO output remains at
        // 100 SPS, matching the 10-second/1024-sample buffer and BPM math.
        if ((result = write_register(REG_FIFO_CONFIG, 0x00)) != ESP_OK) {
            ESP_LOGE(TAG, "initialization failed at FIFO_CONFIG: %s", esp_err_to_name(result));
            return result;
        }
        // 4096 nA range, 100 SPS, 411 us pulse width / 18-bit ADC.
        if ((result = write_register(REG_SPO2_CONFIG, 0x27)) != ESP_OK) {
            ESP_LOGE(TAG, "initialization failed at SPO2_CONFIG: %s", esp_err_to_name(result));
            return result;
        }
        if ((result = write_register(REG_LED1_PA, 0x24)) != ESP_OK) {
            ESP_LOGE(TAG, "initialization failed at RED_LED_CURRENT: %s", esp_err_to_name(result));
            return result;
        }
        if ((result = write_register(REG_LED2_PA, 0x24)) != ESP_OK) {
            ESP_LOGE(TAG, "initialization failed at IR_LED_CURRENT: %s", esp_err_to_name(result));
            return result;
        }
        result = stop_measurement();
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "initialization failed while entering shutdown: %s",
                     esp_err_to_name(result));
            return result;
        }
        ESP_LOGI(TAG, "initialization complete");
        return ESP_OK;
    }

    esp_err_t MAX30102::start_measurement() {
        esp_err_t result = write_register(REG_FIFO_WR_PTR, 0);
        if (result != ESP_OK) return result;
        if ((result = write_register(REG_OVF_COUNTER, 0)) != ESP_OK) return result;
        if ((result = write_register(REG_FIFO_RD_PTR, 0)) != ESP_OK) return result;
        return write_register(REG_MODE_CONFIG, MODE_SPO2);
    }

    esp_err_t MAX30102::stop_measurement() {
        return write_register(REG_MODE_CONFIG, MODE_SHUTDOWN | MODE_SPO2);
    }

    esp_err_t MAX30102::read_fifo(std::span<PPGSample> destination, size_t& sample_count) {
        sample_count = 0;
        uint8_t write_pointer = 0;
        uint8_t read_pointer = 0;
        uint8_t overflow_counter = 0;
        esp_err_t result = read_register(REG_FIFO_WR_PTR, write_pointer);
        if (result != ESP_OK) return result;
        if ((result = read_register(REG_FIFO_RD_PTR, read_pointer)) != ESP_OK) return result;
        if ((result = read_register(REG_OVF_COUNTER, overflow_counter)) != ESP_OK) return result;

        size_t available = (write_pointer - read_pointer + FIFO_DEPTH) % FIFO_DEPTH;
        if (overflow_counter != 0) {
            // Equal pointers normally mean empty, but after overflow they mean
            // that all 32 FIFO slots contain unread samples.
            if (write_pointer == read_pointer) available = FIFO_DEPTH;
            ESP_LOGW(TAG, "FIFO overflow detected: counter=%u, available=%u; samples were lost",
                     overflow_counter, static_cast<unsigned>(available));
            result = write_register(REG_OVF_COUNTER, 0);
            if (result != ESP_OK) {
                ESP_LOGE(TAG, "FIFO overflow counter reset failed: %s", esp_err_to_name(result));
                return result;
            }
        }
        const size_t count = std::min(available, destination.size());
        if (count == 0) return ESP_OK;

        std::array<uint8_t, FIFO_DEPTH * 6> raw {};
        const uint8_t fifo_reg = REG_FIFO_DATA;
        result = m_i2c.write_read(ADDRESS, std::span<const uint8_t>(&fifo_reg, 1),
                                  std::span<uint8_t>(raw.data(), count * 6));
        if (result != ESP_OK) return result;

        for (size_t i = 0; i < count; ++i) {
            const uint8_t* bytes = raw.data() + i * 6;
            destination[i].red = (static_cast<uint32_t>(bytes[0]) << 16 | bytes[1] << 8 | bytes[2]) & 0x3FFFF;
            destination[i].infrared = (static_cast<uint32_t>(bytes[3]) << 16 | bytes[4] << 8 | bytes[5]) & 0x3FFFF;
        }
        sample_count = count;
        return ESP_OK;
    }
} // namespace oasis
