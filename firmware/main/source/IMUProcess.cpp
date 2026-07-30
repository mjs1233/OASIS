#include <freertos/FreeRTOS.h>
#include "IMUProcess.hpp"
#include "IMUData.hpp"
#include "NotifyFlags.hpp"


namespace oasis {

    bool IMUProcess::create() {
        return create_impl();
    }

    void IMUProcess::run(void* pvParameter) {
        auto pIMUProcess = static_cast<IMUProcess*>(pvParameter);
        pIMUProcess->init_impl();
        pIMUProcess->update_impl();
        vTaskDelete(nullptr);
    }

    bool IMUProcess::isr_callback(
        gptimer_handle_t timer,
        const gptimer_alarm_event_data_t *edata,
        void *user_ctx) {

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        auto IMU_task_handle = static_cast<TaskHandle_t>(user_ctx);
        if (IMU_task_handle != nullptr) {
            vTaskNotifyGiveFromISR(IMU_task_handle, &xHigherPriorityTaskWoken);
        }

        return (xHigherPriorityTaskWoken == pdTRUE);
    }

    bool IMUProcess::create_impl() {
        BaseType_t ret = xTaskCreatePinnedToCore(
            IMUProcess::run,
            "imu process",
            IMUProcess::STACK_SIZE,
            static_cast<void*>(this),
            IMUProcess::PRIORITY | portPRIVILEGE_BIT,
            &m_task_handle,
            IMUProcess::CORE
        );

        if (ret != pdPASS) {
            return false;
        }
        return true;
    }

    void IMUProcess::init_impl() {

        m_main_process_task_handle = xTaskGetHandle("main process");
        //Init MPU6050 I2C
        //m_mpu6050.emplace();

        gptimer_config_t timer_config = {
            .clk_src = GPTIMER_CLK_SRC_DEFAULT,
            .direction = GPTIMER_COUNT_UP,
            .resolution_hz = 1000000,
            .intr_priority = 0,
            .flags = {}
        };
        gptimer_handle_t gptimer = nullptr;
        ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

        gptimer_alarm_config_t alarm_config = {
            .alarm_count = 100000,
            .reload_count = 0,
            .flags = {
                .auto_reload_on_alarm = true,
            },
        };
        ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

        gptimer_event_callbacks_t timer_callback = {
            .on_alarm = IMUProcess::isr_callback,
        };

        ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &timer_callback,m_task_handle));
        ESP_ERROR_CHECK(gptimer_enable(gptimer));
        ESP_ERROR_CHECK(gptimer_start(gptimer));

    }

    void IMUProcess::update_impl() {
        while(true) {
            ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
            IMUData* data = m_buffer.acquire_free();
            //m_mpu6050->read_fifo_buffer(data);
            //TODO) some filtering stuff
            //TODO) some value pushing stuff.
            printf("IMU Process push data\n");
            if (m_buffer.publish_ready(data) == false) {
                printf("IMU buffer push failed\n");
                continue;
            }
            xTaskNotify(m_main_process_task_handle, notify::ISR_IMU_BUFFER_FULL, eSetBits);
        }
    }


    IMUProcess::IMUBuffer& IMUProcess::get_buffer_pool(){
        return m_buffer;
    }

} // namespace oasis