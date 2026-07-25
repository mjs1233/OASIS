#include "../include/MainProcess.hpp"

#include "InitialConfigData.hpp"
#include <atomic>
namespace oasis {
    MainProcess::MainProcess() {
    }

    MainProcess::~MainProcess() {
    }

    bool MainProcess::create() {
        return create_impl();
    }

    void MainProcess::run(void* pvParameter) {
        auto pMainProcess = static_cast<MainProcess*>(pvParameter);
        pMainProcess->init_impl();
        pMainProcess->update_impl();
        vTaskDelete(NULL);
    }

    bool MainProcess::create_impl() {
        BaseType_t ret =
            xTaskCreatePinnedToCore(
                   MainProcess::run,
                   "main process",
                   MainProcess::STACK_SIZE,
                   static_cast<void*>(this),
                   MainProcess::PRIORITY | portPRIVILEGE_BIT,
                   &m_task_handle,
                   MainProcess::CORE
                   );

        if (ret != pdPASS) {
            return false;
        }
        return true;
    }

    void MainProcess::init_impl() {
        //startup seq.
        //1. init Peripheral
        //init ADC
        m_adc_unit_0.add_channel(ADC_TEMP_0_CHANNEL_NUM);
        m_adc_unit_0.add_channel(ADC_TEMP_1_CHANNEL_NUM);
        m_adc_unit_0.add_channel(ADC_BATTERY_CHANNEL_NUM);

        //2. read ROM

        //3. sync with network core & recv config
        uint32_t notification_value = 0;
        if (xTaskNotifyWait(0x0, notify::NETWORK_INITIAL_CONFIG_SYNC,&notification_value, portMAX_DELAY) == true) {
            std::atomic_thread_fence(std::memory_order_acquire);
            //read config data
            printf("core 1 notify recv. init done\n");
        }else {
            //DO Restart Stuff.
            printf("init fail?\n");
        }

        //start network service

        //start Timer
        TimerHandle_t timer = xTimerCreate(
            "extern condition timer",
            pdMS_TO_TICKS(5000),
            pdTRUE,
            (void*)0,
            extern_condition_timer_callback
            );
        xTimerStart(timer, 0);

        //start interrupt

    }

    void MainProcess::update_impl() {

    }
}
