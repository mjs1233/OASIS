//
// Created by tgian on 26. 7. 27..
//
#include "MPU6050.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace oasis {
    namespace {
        constexpr char TAG[] = "MPU6050";

        constexpr gpio_num_t I2C_SDA_IO   = GPIO_NUM_21;
        constexpr gpio_num_t I2C_SCL_IO   = GPIO_NUM_20;
        constexpr uint32_t   I2C_FREQ_HZ  = 400000;

        constexpr uint8_t REG_SMPLRT_DIV        = 0x19;
        constexpr uint8_t REG_CONFIG            = 0x1A;
        constexpr uint8_t REG_GYRO_CONFIG       = 0x1B;
        constexpr uint8_t REG_ACCEL_CONFIG      = 0x1C;
        constexpr uint8_t REG_FIFO_EN           = 0x23;
        constexpr uint8_t REG_SIGNAL_PATH_RESET = 0x68;
        constexpr uint8_t REG_USER_CTRL         = 0x6A;
        constexpr uint8_t REG_PWR_MGMT_1        = 0x6B;
        constexpr uint8_t REG_FIFO_COUNTH       = 0x72;
        constexpr uint8_t REG_FIFO_R_W          = 0x74;
        constexpr uint8_t REG_WHO_AM_I          = 0x75;
    }  // namespace

    MPU6050::MPU6050() {
        init();
        ESP_LOGI(TAG, "MPU6050 construction complete, FIFO sampling running.");
    }

    MPU6050::~MPU6050() {
        if (dev_handle_) {
            i2c_master_bus_rm_device(dev_handle_);
            dev_handle_ = nullptr;
        }
        if (bus_handle_) {
            i2c_del_master_bus(bus_handle_);
            bus_handle_ = nullptr;
        }
    }

    namespace {
        esp_err_t write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val) {
            uint8_t buf[2] = {reg, val};
            return i2c_master_transmit(dev, buf, sizeof(buf), pdMS_TO_TICKS(1000));
        }

        esp_err_t read_regs(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t* data, size_t len) {
            return i2c_master_transmit_receive(dev, &reg, 1, data, len, pdMS_TO_TICKS(1000));
        }
    }  // namespace

    void MPU6050::init() {

        i2c_master_bus_config_t bus_config = {};
        bus_config.i2c_port = I2C_NUM_0;
        bus_config.sda_io_num = I2C_SDA_IO;
        bus_config.scl_io_num = I2C_SCL_IO;
        bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_config.glitch_ignore_cnt = 7;
        bus_config.flags.enable_internal_pullup = true;
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle_));

        // add device
        i2c_device_config_t dev_config = {};
        dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        dev_config.device_address = MPU6050_ADDR;
        dev_config.scl_speed_hz = I2C_FREQ_HZ;
        ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle_, &dev_config, &dev_handle_));

        // com check
        uint8_t who_am_i = 0;
        if (read_regs(dev_handle_, REG_WHO_AM_I, &who_am_i, 1) == ESP_OK) {
            ESP_LOGI(TAG, "WHO_AM_I = 0x%02X (expect 0x68)", who_am_i);
        } else {
            ESP_LOGE(TAG, "WHO_AM_I read failed - check wiring/address");
        }

        // reset IMU
        ESP_ERROR_CHECK(write_reg(dev_handle_, REG_PWR_MGMT_1, 0x80));  // DEVICE_RESET
        vTaskDelay(pdMS_TO_TICKS(100));

        // signal reset
        ESP_ERROR_CHECK(write_reg(dev_handle_, REG_SIGNAL_PATH_RESET, 0x07));
        vTaskDelay(pdMS_TO_TICKS(100));

        // set clock source to X PLL
        ESP_ERROR_CHECK(write_reg(dev_handle_, REG_PWR_MGMT_1, 0x01));

        // DLPF: DLPF_CFG=3
        ESP_ERROR_CHECK(write_reg(dev_handle_, REG_CONFIG, 0x03));

        // sample rate = 200hz
        ESP_ERROR_CHECK(write_reg(dev_handle_, REG_SMPLRT_DIV, 4));


        ESP_ERROR_CHECK(write_reg(dev_handle_, REG_GYRO_CONFIG, 0x00));   // ±250 dps
        ESP_ERROR_CHECK(write_reg(dev_handle_, REG_ACCEL_CONFIG, 0x00));  // ±2g

        // set FIFO
        ESP_ERROR_CHECK(write_reg(dev_handle_, REG_FIFO_EN, 0xF8));

        // clear FIFO
        ESP_ERROR_CHECK(write_reg(dev_handle_, REG_USER_CTRL, 0x04));
        vTaskDelay(pdMS_TO_TICKS(10));

        // activate FIFO
        ESP_ERROR_CHECK(write_reg(dev_handle_, REG_USER_CTRL, 0x40));
    }

    bool MPU6050::read_fifo_buffer(IMUData* data) {
        if (!data) return false;

        uint8_t count_buf[2];
        if (read_regs(dev_handle_, REG_FIFO_COUNTH, count_buf, 2) != ESP_OK) {
            ESP_LOGW(TAG, "FIFO_COUNT read failed");
            return false;
        }

        const uint16_t count = (static_cast<uint16_t>(count_buf[0]) << 8) | count_buf[1];
        if (count < BYTES_PER_SAMPLE) {
            return false;
        }

        uint16_t n_samples = count / BYTES_PER_SAMPLE;
        if (n_samples > MAX_SAMPLES) {

            ESP_LOGW(TAG, "FIFO has %u samples, more than MAX_SAMPLES(%u); truncating this poll",
                     n_samples, static_cast<unsigned>(MAX_SAMPLES));
            n_samples = MAX_SAMPLES;
        }
        const uint16_t bytes_to_read = n_samples * BYTES_PER_SAMPLE;

        uint8_t fifo_buf[MAX_SAMPLES * BYTES_PER_SAMPLE];
        if (read_regs(dev_handle_, REG_FIFO_R_W, fifo_buf, bytes_to_read) != ESP_OK) {
            ESP_LOGW(TAG, "FIFO burst read failed");
            return false;
        }

        // ACCEL(59~64) -> TEMP(65~66) -> GYRO(67~72)
        for (uint16_t i = 0; i < n_samples; ++i) {
            const uint8_t* s = &fifo_buf[i * BYTES_PER_SAMPLE];

            const int16_t accel_x = static_cast<int16_t>((s[0] << 8) | s[1]);
            const int16_t accel_y = static_cast<int16_t>((s[2] << 8) | s[3]);
            const int16_t accel_z = static_cast<int16_t>((s[4] << 8) | s[5]);
            const int16_t temp    = static_cast<int16_t>((s[6] << 8) | s[7]);
            const int16_t gyro_x  = static_cast<int16_t>((s[8] << 8) | s[9]);
            const int16_t gyro_y  = static_cast<int16_t>((s[10] << 8) | s[11]);
            const int16_t gyro_z  = static_cast<int16_t>((s[12] << 8) | s[13]);

            data[i].accel_x = accel_x / 16384.0f;  // AFS_SEL=0 (±2g)
            data[i].accel_y = accel_y / 16384.0f;
            data[i].accel_z = accel_z / 16384.0f;
            data[i].gyro_x  = gyro_x / 131.0f;     // FS_SEL=0 (±250dps)
            data[i].gyro_y  = gyro_y / 131.0f;
            data[i].gyro_z  = gyro_z / 131.0f;
            data[i].temp_c  = temp / 340.0f + 36.53f;
            data[i].is_last   = false;
        }

        data[n_samples - 1].is_last = true;

        return true;
    }
}