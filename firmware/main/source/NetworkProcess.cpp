//
// Created by tgian on 26. 7. 24..
//
#include "NetworkProcess.hpp"
#include "NotifyFlags.hpp"
#include "InitialConfigData.hpp"
#include "core/NetworkManager.hpp"
#include <atomic>
namespace oasis {
    NetworkProcess::NetworkProcess() {

    }

    NetworkProcess::~NetworkProcess() {

    }

    bool NetworkProcess::create() {
        return create_impl();
    }

    void NetworkProcess::run(void* pvParameter) {
        auto pNetworkProcess = static_cast<NetworkProcess*>(pvParameter);
        pNetworkProcess->init_impl();
        pNetworkProcess->update_impl();
        vTaskDelete(NULL);
    }

    bool NetworkProcess::create_impl() {
        xTaskCreatePinnedToCore(
                  NetworkProcess::run,
                  "network process",
                  NetworkProcess::STACK_SIZE,
                  static_cast<void*>(this),
                  NetworkProcess::PRIORITY | portPRIVILEGE_BIT,
                  &m_task_handle,
                  NetworkProcess::CORE
                  );

        return true;
    }

    void NetworkProcess::init_impl() {

        //send & recv network task.
        //vTaskDelay(pdMS_TO_TICKS(5000));
        //notify & sync with main process

        NetworkManager::init_wifi("가209호", "kimch1084");
        const char* target_url = "http://192.168.0.4/";
        std::array<std::uint8_t, 1024> buffer {};
        NetworkManager::send_binary_data(target_url,buffer, 0);
        //assign recv data
        //g_initial_config_data

        //notify to main process
        m_main_process_task_handle = xTaskGetHandle("main process");
        std::atomic_thread_fence(std::memory_order_release);
        xTaskNotify(m_main_process_task_handle, notify::NETWORK_INITIAL_CONFIG_SYNC, eSetBits);
    }

    void NetworkProcess::update_impl() {
        //TODO) NETWORK LOOP
        while (true) {
            std::array<std::uint8_t, 1024> buffer {};
            m_network_queue.recv_and_serialize(buffer);

        }
    }

}
