//
// SHT31 humidity/temperature sensor driver (I2C).
//
#include "SHT31.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace oasis {
    namespace {
        constexpr char TAG[] = "SHT31";

        // SHT3x commands (Sensirion SHT3x datasheet, section 4.3)
        constexpr uint16_t CMD_SOFT_RESET              = 0x30A2;
        constexpr uint16_t CMD_READ_STATUS             = 0xF32D;
        constexpr uint16_t CMD_MEASURE_HIGH_NO_STRETCH = 0x2400;

        // Max conversion time, high repeatability, single shot (datasheet 4.5)
        constexpr uint32_t MEASURE_DELAY_MS = 15;
        // Soft reset time (datasheet 4.9)
        constexpr uint32_t RESET_DELAY_MS = 10;

        esp_err_t write_cmd(i2c_master_dev_handle_t dev, uint16_t cmd) {
            uint8_t buf[2] = {
                static_cast<uint8_t>(cmd >> 8),
                static_cast<uint8_t>(cmd & 0xFF)
            };
            return i2c_master_transmit(dev, buf, sizeof(buf), pdMS_TO_TICKS(1000));
        }

        esp_err_t read_bytes(i2c_master_dev_handle_t dev, uint8_t* data, size_t len) {
            return i2c_master_receive(dev, data, len, pdMS_TO_TICKS(1000));
        }
    }  // namespace

    SHT31::SHT31() = default;

    SHT31::~SHT31() {
        if (dev_handle_) {
            i2c_master_bus_rm_device(dev_handle_);
            dev_handle_ = nullptr;
        }
        if (bus_handle_) {
            i2c_del_master_bus(bus_handle_);
            bus_handle_ = nullptr;
        }
    }

    bool SHT31::start() {
        if (m_started) {
            return true;
        }

        i2c_master_bus_config_t bus_config = {};
        bus_config.i2c_port = I2C_PORT;
        bus_config.sda_io_num = I2C_SDA_IO;
        bus_config.scl_io_num = I2C_SCL_IO;
        bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_config.glitch_ignore_cnt = 7;
        bus_config.flags.enable_internal_pullup = true;
        if (i2c_new_master_bus(&bus_config, &bus_handle_) != ESP_OK) {
            ESP_LOGE(TAG, "failed to create I2C bus");
            return false;
        }

        i2c_device_config_t dev_config = {};
        dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        dev_config.device_address = SHT31_ADDR;
        dev_config.scl_speed_hz = I2C_FREQ_HZ;
        if (i2c_master_bus_add_device(bus_handle_, &dev_config, &dev_handle_) != ESP_OK) {
            ESP_LOGE(TAG, "failed to add I2C device");
            return false;
        }

        if (!soft_reset()) {
            ESP_LOGE(TAG, "soft reset failed");
            return false;
        }

        if (!check_status()) {
            ESP_LOGE(TAG, "status check failed - check wiring/address");
            return false;
        }

        m_started = true;
        ESP_LOGI(TAG, "SHT31 started");
        return true;
    }

    bool SHT31::soft_reset() {
        if (write_cmd(dev_handle_, CMD_SOFT_RESET) != ESP_OK) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(RESET_DELAY_MS));
        return true;
    }

    bool SHT31::check_status() {
        if (write_cmd(dev_handle_, CMD_READ_STATUS) != ESP_OK) {
            return false;
        }

        uint8_t status[3] = {};
        if (read_bytes(dev_handle_, status, sizeof(status)) != ESP_OK) {
            return false;
        }

        if (crc8(status, 2) != status[2]) {
            ESP_LOGW(TAG, "status register CRC mismatch");
            return false;
        }

        return true;
    }

    bool SHT31::read() {
        if (!m_started) {
            ESP_LOGW(TAG, "read() called before start()");
            return false;
        }

        if (write_cmd(dev_handle_, CMD_MEASURE_HIGH_NO_STRETCH) != ESP_OK) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(MEASURE_DELAY_MS));

        uint8_t raw[6] = {};
        if (read_bytes(dev_handle_, raw, sizeof(raw)) != ESP_OK) {
            return false;
        }

        if (crc8(raw, 2) != raw[2] || crc8(raw + 3, 2) != raw[5]) {
            ESP_LOGW(TAG, "measurement CRC mismatch");
            return false;
        }

        const uint16_t raw_temp = (static_cast<uint16_t>(raw[0]) << 8) | raw[1];
        const uint16_t raw_hum  = (static_cast<uint16_t>(raw[3]) << 8) | raw[4];

        // Conversion formulas (datasheet section 4.13)
        const float temperature_c = -45.0f + 175.0f * (static_cast<float>(raw_temp) / 65535.0f);
        const float humidity_rh   = 100.0f * (static_cast<float>(raw_hum) / 65535.0f);

        m_temperature_c.push(temperature_c);
        m_humidity_rh.push(humidity_rh);

        return true;
    }

    float SHT31::get_temperature_c() {
        return m_temperature_c.get();
    }

    float SHT31::get_humidity_rh() {
        return m_humidity_rh.get();
    }

    uint8_t SHT31::crc8(const uint8_t* data, size_t len) {
        // CRC-8, polynomial 0x31, init 0xFF (datasheet section 4.12)
        uint8_t crc = 0xFF;
        for (size_t i = 0; i < len; ++i) {
            crc ^= data[i];
            for (int bit = 0; bit < 8; ++bit) {
                if (crc & 0x80) {
                    crc = static_cast<uint8_t>((crc << 1) ^ 0x31);
                } else {
                    crc = static_cast<uint8_t>(crc << 1);
                }
            }
        }
        return crc;
    }
}  // namespace oasis
