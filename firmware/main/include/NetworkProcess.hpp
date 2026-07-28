//
// Created by tgian on 26. 7. 23..
//

#ifndef FIRMWARE_NETWORKPROCESS_HPP
#define FIRMWARE_NETWORKPROCESS_HPP
#include <freertos/FreeRTOS.h>

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


        TaskHandle_t m_task_handle = nullptr;
        TaskHandle_t m_main_process_task_handle = nullptr;

        NetworkQueue m_network_queue;

        static constexpr uint32_t CORE = 0;
        static constexpr uint32_t STACK_SIZE = 4096;
        static constexpr uint32_t PRIORITY = 2;
    };
}
#endif //FIRMWARE_NETWORKPROCESS_HPP
