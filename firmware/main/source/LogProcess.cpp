//
// Created by tgian on 26. 8. 2..
//
#include "LogProcess.hpp"
#include "driver/uart.h"
#include <atomic>
#include <ostream>

namespace oasis {

    bool LogProcess::create() {
        return create_impl();
    }

    void LogProcess::run(void* pvParameter) {
        auto* pLogProcess = static_cast<LogProcess*>(pvParameter);
        pLogProcess->init_impl();
        pLogProcess->update_impl();

        vTaskDelete(NULL);
    }

    bool LogProcess::create_impl() {
        BaseType_t ret = xTaskCreatePinnedToCore(
            LogProcess::run,
            "log process",
            LogProcess::STACK_SIZE,
            static_cast<void*>(this),
            LogProcess::PRIORITY | portPRIVILEGE_BIT,
            &m_task_handle,
            LogProcess::CORE
        );

        if (ret != pdPASS) {
            return false;
        }

        return true;
    }

    void LogProcess::init_impl() {
        uart_config_t uart_config = {
            .baud_rate = 115200,
            .data_bits = UART_DATA_8_BITS,
            .parity    = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 0,
            .source_clk = UART_SCLK_DEFAULT,
            .flags = {},
        };

        if (uart_is_driver_installed(UART_NUM_0)) {
            uart_driver_delete(UART_NUM_0);
        }

        ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));
        ESP_ERROR_CHECK(uart_set_pin(UART_NUM_0, 43, 44, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

        ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 2048, 0, 0, NULL, 0));

        //esp_log_set_vprintf(redirected_vprintf);
        m_uart_mutex = xSemaphoreCreateMutex();
    }

    void LogProcess::update_impl() {
        while (true) {
            ////>>>
            uint32_t notification_value = 0;
            if (xTaskNotifyWait(0x0, 0xFFFFFFFF,&notification_value, portMAX_DELAY) == true) {
                std::atomic_thread_fence(std::memory_order_acquire);
            }
        }
    }

    bool LogProcess::write_text(std::string_view tag,std::string_view msg) {
        //make text packet.
        uint16_t length = tag.length() + msg.length();
        if (length > 512) {
            return false;
        }


        uint8_t bytes[512] {};
        uint8_t* next = bytes;

        next = std::copy_n(tag.data(), tag.length(), next);
        next = std::copy_n(msg.data(), msg.length(), next);

        size_t total_length = next - bytes;
        if (total_length != length) {
            //WTF?
            return false;
        }
        if (xPortInIsrContext()) {
            return 0;
        }

        if (m_uart_mutex != NULL) {
            if (xSemaphoreTake(m_uart_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                if (uart_write_bytes(UART_NUM_0, bytes, length) == -1) {
                    return false;
                }
                xSemaphoreGive(m_uart_mutex);
            }
            return false;
        }
        return true;
    }

    bool LogProcess::write_bytes(uint8_t type ,void* data,size_t size) {


        uint16_t length = size;
        uint16_t estimated_total_length = length + LOG_HEADER_SIZE + LOG_FOOTER_SIZE;

        if (estimated_total_length > 512) {
            return false;
        }

        LogHeader header = {
            .magic = 0x88E4,
            .type = type,
            .length = length,
            .cobs = 0
        };

        LogFooter footer = {
            .magic = 0x8BDA
        };


        uint8_t bytes[512] {};
        uint8_t* next = bytes;
        const auto header_ptr = reinterpret_cast<const uint8_t*>(&header);
        const auto data_ptr = static_cast<const uint8_t*>(data);
        const auto footer_ptr = reinterpret_cast<const uint8_t*>(&footer);

        next = std::copy_n(header_ptr, LOG_HEADER_SIZE, next);
        next = std::copy_n(data_ptr, size, next);
        next = std::copy_n(footer_ptr, LOG_FOOTER_SIZE, next);

        size_t total_length = next - bytes;
        if (total_length != estimated_total_length) {
            //WTF?
            return false;
        }
        if (xPortInIsrContext()) {
            return 0;
        }

        if (m_uart_mutex != NULL) {
            if (xSemaphoreTake(m_uart_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                if (uart_write_bytes(UART_NUM_0, bytes, length) == -1) {
                    return false;
                }
                xSemaphoreGive(m_uart_mutex);
            }
            return false;
        }

        return true;
    }

} // namespace oasis

int redirected_vprintf(const char *fmt, va_list args) {
    char log_buffer[256];
    int len = vsnprintf(log_buffer, sizeof(log_buffer), fmt, args);

    if (len > 0) {
        oasis::LogProcess::write_text("nina", log_buffer);
    }

    return len;
}