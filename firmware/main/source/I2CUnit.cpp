#include "core/I2CUnit.hpp"

#include "esp_log.h"

namespace oasis {

    I2CUnit::I2CUnit(const I2CUnitConfig& config) : m_config(config) {
        m_mutex = xSemaphoreCreateMutex();
        configASSERT(m_mutex != nullptr);

        i2c_master_bus_config_t bus_config {};
        bus_config.i2c_port = m_config.port;
        bus_config.sda_io_num = m_config.sda_gpio;
        bus_config.scl_io_num = m_config.scl_gpio;
        bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_config.glitch_ignore_cnt = 7;
        bus_config.flags.enable_internal_pullup = m_config.enable_internal_pullups;
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &m_bus));
    }

    I2CUnit::~I2CUnit() {
        for (auto& device : m_devices) {
            if (device.handle != nullptr) {
                const esp_err_t result = i2c_master_bus_rm_device(device.handle);
                if (result != ESP_OK) {
                    ESP_LOGE("I2CUnit", "remove device 0x%02X failed: %s", device.address,
                             esp_err_to_name(result));
                }
            }
        }
        if (m_bus != nullptr) {
            const esp_err_t result = i2c_del_master_bus(m_bus);
            if (result != ESP_OK) {
                ESP_LOGE("I2CUnit", "delete bus failed: %s", esp_err_to_name(result));
            }
        }
        if (m_mutex != nullptr) {
            vSemaphoreDelete(m_mutex);
        }
    }

    esp_err_t I2CUnit::lock(TickType_t timeout) {
        return xSemaphoreTake(m_mutex, timeout) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT;
    }

    void I2CUnit::unlock() {
        xSemaphoreGive(m_mutex);
    }

    esp_err_t I2CUnit::get_device_locked(uint8_t address, i2c_master_dev_handle_t& device) {
        for (const auto& slot : m_devices) {
            if (slot.handle != nullptr && slot.address == address) {
                device = slot.handle;
                return ESP_OK;
            }
        }

        DeviceSlot* free_slot = nullptr;
        for (auto& slot : m_devices) {
            if (slot.handle == nullptr) {
                free_slot = &slot;
                break;
            }
        }
        if (free_slot == nullptr) {
            return ESP_ERR_NO_MEM;
        }

        i2c_device_config_t device_config {};
        device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        device_config.device_address = address;
        device_config.scl_speed_hz = m_config.frequency_hz;
        esp_err_t result = i2c_master_bus_add_device(m_bus, &device_config, &free_slot->handle);
        if (result == ESP_OK) {
            free_slot->address = address;
            device = free_slot->handle;
        }
        return result;
    }

    esp_err_t I2CUnit::write(uint8_t address, std::span<const uint8_t> payload, int timeout_ms) {
        esp_err_t result = lock(pdMS_TO_TICKS(timeout_ms));
        if (result != ESP_OK) return result;

        i2c_master_dev_handle_t device = nullptr;
        result = get_device_locked(address, device);
        if (result == ESP_OK) {
            result = i2c_master_transmit(device, payload.data(), payload.size(), timeout_ms);
        }
        unlock();
        return result;
    }

    esp_err_t I2CUnit::read(uint8_t address, std::span<uint8_t> payload, int timeout_ms) {
        esp_err_t result = lock(pdMS_TO_TICKS(timeout_ms));
        if (result != ESP_OK) return result;

        i2c_master_dev_handle_t device = nullptr;
        result = get_device_locked(address, device);
        if (result == ESP_OK) {
            result = i2c_master_receive(device, payload.data(), payload.size(), timeout_ms);
        }
        unlock();
        return result;
    }

    esp_err_t I2CUnit::write_read(uint8_t address, std::span<const uint8_t> write_payload,
                                  std::span<uint8_t> read_payload, int timeout_ms) {
        esp_err_t result = lock(pdMS_TO_TICKS(timeout_ms));
        if (result != ESP_OK) return result;

        i2c_master_dev_handle_t device = nullptr;
        result = get_device_locked(address, device);
        if (result == ESP_OK) {
            result = i2c_master_transmit_receive(device, write_payload.data(), write_payload.size(),
                                                 read_payload.data(), read_payload.size(), timeout_ms);
        }
        unlock();
        return result;
    }

    esp_err_t I2CUnit::probe(uint8_t address, int timeout_ms) {
        esp_err_t result = lock(pdMS_TO_TICKS(timeout_ms));
        if (result != ESP_OK) return result;

        result = i2c_master_probe(m_bus, address, timeout_ms);
        unlock();
        return result;
    }

} // namespace oasis
