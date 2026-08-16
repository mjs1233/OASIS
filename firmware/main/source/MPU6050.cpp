#include "MPU6050.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace oasis {
    namespace {
        constexpr char TAG[] = "MPU6050";
        constexpr uint8_t MPU6050_ADDRESS = 0x68;

        constexpr uint8_t REG_SMPLRT_DIV = 0x19;
        constexpr uint8_t REG_CONFIG = 0x1A;
        constexpr uint8_t REG_GYRO_CONFIG = 0x1B;
        constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
        constexpr uint8_t REG_FIFO_EN = 0x23;
        constexpr uint8_t REG_SIGNAL_PATH_RESET = 0x68;
        constexpr uint8_t REG_USER_CTRL = 0x6A;
        constexpr uint8_t REG_PWR_MGMT_1 = 0x6B;
        constexpr uint8_t REG_FIFO_COUNTH = 0x72;
        constexpr uint8_t REG_FIFO_R_W = 0x74;
        constexpr uint8_t REG_WHO_AM_I = 0x75;

        esp_err_t write_reg(I2CUnit& i2c, uint8_t reg, uint8_t value) {
            const uint8_t payload[2] = {reg, value};
            return i2c.write(MPU6050_ADDRESS, payload, pdMS_TO_TICKS(1000));
        }

        esp_err_t read_regs(I2CUnit& i2c, uint8_t reg, uint8_t* data, size_t length) {
            return i2c.write_read(MPU6050_ADDRESS, std::span<const uint8_t>(&reg, 1),
                                  std::span<uint8_t>(data, length), pdMS_TO_TICKS(1000));
        }
    }

    MPU6050::MPU6050(I2CUnit& i2c) : m_i2c(i2c) {
        ESP_ERROR_CHECK(init());
        ESP_LOGI(TAG, "MPU6050 construction complete, FIFO sampling running.");
    }

    esp_err_t MPU6050::init() {
        uint8_t who_am_i = 0;
        esp_err_t result = read_regs(m_i2c, REG_WHO_AM_I, &who_am_i, 1);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "WHO_AM_I read failed: %s", esp_err_to_name(result));
            return result;
        }
        if (who_am_i != MPU6050_ADDR) {
            ESP_LOGE(TAG, "unexpected WHO_AM_I: 0x%02X", who_am_i);
            return ESP_ERR_INVALID_RESPONSE;
        }

        if ((result = write_reg(m_i2c, REG_PWR_MGMT_1, 0x80)) != ESP_OK) return result;
        vTaskDelay(pdMS_TO_TICKS(100));
        if ((result = write_reg(m_i2c, REG_SIGNAL_PATH_RESET, 0x07)) != ESP_OK) return result;
        vTaskDelay(pdMS_TO_TICKS(100));
        if ((result = write_reg(m_i2c, REG_PWR_MGMT_1, 0x01)) != ESP_OK) return result;
        if ((result = write_reg(m_i2c, REG_CONFIG, 0x03)) != ESP_OK) return result;
        if ((result = write_reg(m_i2c, REG_SMPLRT_DIV, 4)) != ESP_OK) return result;
        if ((result = write_reg(m_i2c, REG_GYRO_CONFIG, 0x00)) != ESP_OK) return result;
        if ((result = write_reg(m_i2c, REG_ACCEL_CONFIG, 0x00)) != ESP_OK) return result;
        if ((result = write_reg(m_i2c, REG_FIFO_EN, 0xF8)) != ESP_OK) return result;
        if ((result = write_reg(m_i2c, REG_USER_CTRL, 0x04)) != ESP_OK) return result;
        vTaskDelay(pdMS_TO_TICKS(10));
        return write_reg(m_i2c, REG_USER_CTRL, 0x40);
    }

    bool MPU6050::read_fifo_buffer(IMUData* data) {
        if (data == nullptr) return false;

        uint8_t count_buf[2] {};
        esp_err_t result = read_regs(m_i2c, REG_FIFO_COUNTH, count_buf, sizeof(count_buf));
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "FIFO_COUNT read failed: %s", esp_err_to_name(result));
            return false;
        }

        const uint16_t count = (static_cast<uint16_t>(count_buf[0]) << 8) | count_buf[1];
        if (count < BYTES_PER_SAMPLE) return false;

        uint16_t sample_count = count / BYTES_PER_SAMPLE;
        if (sample_count > MAX_SAMPLES) {
            ESP_LOGW(TAG, "FIFO has %u samples; reading newest %u", sample_count,
                     static_cast<unsigned>(MAX_SAMPLES));
            sample_count = MAX_SAMPLES;
        }

        uint8_t fifo_buf[MAX_SAMPLES * BYTES_PER_SAMPLE] {};
        result = read_regs(m_i2c, REG_FIFO_R_W, fifo_buf, sample_count * BYTES_PER_SAMPLE);
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "FIFO burst read failed: %s", esp_err_to_name(result));
            return false;
        }

        for (uint16_t i = 0; i < sample_count; ++i) {
            const uint8_t* sample = &fifo_buf[i * BYTES_PER_SAMPLE];
            const int16_t accel_x = static_cast<int16_t>((sample[0] << 8) | sample[1]);
            const int16_t accel_y = static_cast<int16_t>((sample[2] << 8) | sample[3]);
            const int16_t accel_z = static_cast<int16_t>((sample[4] << 8) | sample[5]);
            const int16_t temp = static_cast<int16_t>((sample[6] << 8) | sample[7]);
            const int16_t gyro_x = static_cast<int16_t>((sample[8] << 8) | sample[9]);
            const int16_t gyro_y = static_cast<int16_t>((sample[10] << 8) | sample[11]);
            const int16_t gyro_z = static_cast<int16_t>((sample[12] << 8) | sample[13]);

            data[i].accel_x = accel_x / 16384.0f;
            data[i].accel_y = accel_y / 16384.0f;
            data[i].accel_z = accel_z / 16384.0f;
            data[i].gyro_x = gyro_x / 131.0f;
            data[i].gyro_y = gyro_y / 131.0f;
            data[i].gyro_z = gyro_z / 131.0f;
            data[i].temp_c = temp / 340.0f + 36.53f;
            data[i].is_last = i + 1 == sample_count;
        }
        return true;
    }
} // namespace oasis
