//
// Created by tgian on 26. 7. 24..
//
#include "NetworkProcess.hpp"
#include "NotifyFlags.hpp"
#include "InitialConfigData.hpp"
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
        vTaskDelay(pdMS_TO_TICKS(5000));
        //notify & sync with main process

        //assign recv data
        //g_initial_config_data

        //notify to main process
        m_main_process_task_handle = xTaskGetHandle("main process");
        std::atomic_thread_fence(std::memory_order_release);
        xTaskNotify(m_main_process_task_handle, notify::NETWORK_INITIAL_CONFIG_SYNC, eSetBits);
    }

    void NetworkProcess::update_impl() {
        //TODO) NETWORK LOOP
    }

}
