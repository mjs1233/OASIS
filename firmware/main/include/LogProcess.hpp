//
// Created by tgian on 26. 8. 2..
//

#ifndef FIRMWARE_LOGPROCESS_HPP
#define FIRMWARE_LOGPROCESS_HPP
#include <string_view>
#include "freertos/FreeRTOS.h"
namespace oasis {

    namespace LogType {
        enum type : uint8_t {
            text,

        };
    }


    class LogProcess {
    public:
        bool create();
        static void run(void* pvParameter);
        static bool write_text(std::string_view tag,std::string_view msg);
        static bool write_bytes(uint8_t type ,void* data,size_t size);
    private:

        struct __attribute__((packed)) LogHeader {
            uint16_t magic = {};
            uint8_t type = 0;
            uint16_t length = 0;
            uint8_t cobs = 0;
        };

        struct __attribute__((packed)) LogFooter {
            uint16_t magic = {};
        };
        static constexpr size_t LOG_HEADER_SIZE = sizeof(LogHeader);
        static constexpr size_t LOG_FOOTER_SIZE = sizeof(LogFooter);


        bool create_impl();
        void init_impl();
        void update_impl();


        TaskHandle_t m_task_handle = nullptr;
        QueueHandle_t m_uart_queue = nullptr;
        inline static SemaphoreHandle_t m_uart_mutex = nullptr;

        static constexpr uint32_t CORE = 1;
        static constexpr uint32_t STACK_SIZE = 2048;
        static constexpr uint32_t PRIORITY = 2;

        static constexpr LogProcess* instance = nullptr;
    };
}

int redirected_vprintf(const char *fmt, va_list args);
#endif //FIRMWARE_LOGPROCESS_HPP
