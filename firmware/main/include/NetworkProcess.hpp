//
// Created by tgian on 26. 7. 23..
//

#ifndef FIRMWARE_NETWORKPROCESS_HPP
#define FIRMWARE_NETWORKPROCESS_HPP
#include <freertos/FreeRTOS.h>
#include <string_view>

#include "NetworkQueue.hpp"

namespace oasis {
    class NetworkProcess {
    public:
        NetworkProcess();
        ~NetworkProcess();
        NetworkProcess(const NetworkProcess&) = delete;
        NetworkProcess(NetworkProcess&&) = delete;
        NetworkProcess& operator=(NetworkProcess&&) = delete;
        NetworkProcess& operator=(const NetworkProcess&) = delete;

        bool create();
        static void run(void* pvParameter);
    private:
        bool create_impl();
        void init_impl();
        void update_impl();
        void send_query(std::string_view query);
        void apply_response(std::string_view response);


        TaskHandle_t m_task_handle = nullptr;
        TaskHandle_t m_main_process_task_handle = nullptr;
        std::string_view m_server_url {};
        bool m_network_ready = false;

        static constexpr uint32_t CORE = 0;
        static constexpr uint32_t STACK_SIZE = 6144;
        static constexpr uint32_t PRIORITY = 2;
    };
}
#endif //FIRMWARE_NETWORKPROCESS_HPP
