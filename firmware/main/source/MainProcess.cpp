#include "../include/MainProcess.hpp"
#include "InitialConfigData.hpp"
#include <atomic>
#include "../include/NTC.hpp"  // 상대 경로로 강제 지정

namespace oasis {
    MainProcess::MainProcess() {
    }

    MainProcess::~MainProcess() {
        if (m_task_handle != nullptr)
            xTimerDelete(m_extern_cond_timer, portMAX_DELAY);
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

        if (m_imu_process.create() == false) {
            return false;
        }
        return true;

    }

    void MainProcess::extern_condition_timer_callback(TimerHandle_t xtimer) {
        auto* self = static_cast<MainProcess*>(pvTimerGetTimerID(xtimer));
        xTaskNotify(self->m_task_handle, notify::TIMER_EXTREN_COND_CYCLE, eSetBits);
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
        if (xTaskNotifyWait(0x0, 0xFFFFFFFF,&notification_value, portMAX_DELAY) == true) {
            std::atomic_thread_fence(std::memory_order_acquire);
            //read config data
            printf("core 1 notify recv. init done\n");
        } else {
            //DO Restart Stuff.
            printf("init fail?\n");
        }

        //start network service

        //start Timer
        m_extern_cond_timer = xTimerCreate(
            "extern condition timer",
            pdMS_TO_TICKS(5000),
            pdTRUE,
            this,
            extern_condition_timer_callback
            );
        xTimerStart(m_extern_cond_timer, 0);

        //start interrupt

    }

    void MainProcess::update_impl() {

        while (true) {
            uint32_t notification_value = 0;
            if (xTaskNotifyWait(0x0, 0xFFFFFFFF,&notification_value, portMAX_DELAY) == true) {
                std::atomic_thread_fence(std::memory_order_acquire);

                if (notification_value & notify::ISR_IMU_BUFFER_FULL) {
                    //do IMU buffer flush.
                    //printf("Main Process, recv imu buffer\n");
                    imu_buffer_handle();
                    process_network_item();
                }

                if (notification_value & notify::TIMER_EXTREN_COND_CYCLE) {
                    read_extern_condition();
                }
            }
        }

    }




    void MainProcess::imu_buffer_handle() {
        IMUProcess::IMUBuffer& imu_buffer = m_imu_process.get_buffer_pool();
        while (true) {
            IMUData* imu_data = imu_buffer.acquire_ready(0);
            if (imu_data == nullptr) {
                break;
            }
            //read other data.
            //release
            imu_buffer.release_free(imu_data);
        }
    }

    void MainProcess::process_network_item() {
        
        //send to Network Queue
    }

    void MainProcess::read_extern_condition() {

    }

}
