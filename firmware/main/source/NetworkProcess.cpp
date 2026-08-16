//
// Created by tgian on 26. 7. 24..
//
#include "NetworkProcess.hpp"
#include "NotifyFlags.hpp"
#include "InitialConfigData.hpp"
#include "core/NetworkManager.hpp"
#include <atomic>
#include "esp_log.h"
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
        const BaseType_t result = xTaskCreatePinnedToCore(
                  NetworkProcess::run,
                  "network process",
                  NetworkProcess::STACK_SIZE,
                  static_cast<void*>(this),
                  NetworkProcess::PRIORITY | portPRIVILEGE_BIT,
                  &m_task_handle,
                  NetworkProcess::CORE
                  );

        if (result != pdPASS) {
            ESP_LOGE("NetworkProcess", "network task creation failed");
            return false;
        }
        return true;
    }

    void NetworkProcess::init_impl() {

        //send & recv network task.
        //vTaskDelay(pdMS_TO_TICKS(5000));
        //notify & sync with main process

        //NetworkManager::init_wifi("SSID", "PASSWORD");
        //NetworkManager::send_signal("http://192.168.0.4:8080/");
        //assign recv data
        //g_initial_config_data

        //notify to main process
        m_main_process_task_handle = xTaskGetHandle("main process");
        if (m_main_process_task_handle == nullptr) {
            ESP_LOGE("NetworkProcess", "main process task handle not found");
            return;
        }
        std::atomic_thread_fence(std::memory_order_release);
        if (xTaskNotify(m_main_process_task_handle, notify::NETWORK_INITIAL_CONFIG_SYNC, eSetBits) != pdPASS) {
            ESP_LOGE("NetworkProcess", "initial-config notification failed");
        }
    }

    void NetworkProcess::update_impl() {
        constexpr char TARGET_URL[] = "http://192.168.0.4:8080/";
        while (true) {
            std::array<std::uint8_t, 1024> buffer {};
            const uint32_t length = NetworkQueue::instance().recv_and_serialize(buffer);
            if (length == 0) continue;

            if (!NetworkManager::send_binary_data(TARGET_URL, buffer, length)) {
                printf("network worker_data POST failed\n");
            }

        }
    }

}
