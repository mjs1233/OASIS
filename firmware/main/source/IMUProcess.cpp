#include <freertos/FreeRTOS.h>
#include "IMUProcess.hpp"
#include "IMUData.hpp"
#include "LogProcess.hpp"
#include <cmath>
#include "NotifyFlags.hpp"


namespace oasis {
    namespace {

        struct EulerAngle {
            float roll;   // deg, X축 기준 회전
            float pitch;  // deg, Y축 기준 회전
        };

        EulerAngle accel_to_angle(float ax, float ay, float az) {
            EulerAngle e;
            e.roll  = atan2f(ay, az) * 180.0f / M_PI;
            e.pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / M_PI;
            return e;
        }
    }


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
        m_mpu6050.emplace(m_imu_i2c);

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
        oasis::LogProcess::write_text("[MAIN]", "MAIN PROCESS INITIALIZED");

    }

    void IMUProcess::update_impl() {
        while(true) {
            ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
            IMUData* data = m_buffer.acquire_free();
            m_mpu6050->read_fifo_buffer(data);
            //TODO) some filtering stuff
            //TODO) some value pushing stuff.
            //printf("IMU Process push data\n");
            int i = 0;
            while (true) {

                if (i % 100 == 0) {
                    printf("acc (%f %f %f) gyro (%f %f %f)\n", data[i].accel_x, data[i].accel_y, data[i].accel_z, data[i].gyro_x, data[i].gyro_y, data[i].gyro_z);
                    EulerAngle e = accel_to_angle(data[i].accel_x, data[i].accel_y, data[i].accel_z);
                    printf("angle : roll %f pitch %f\n", e.roll, e.pitch);
                }
                if (data[i].is_last)
                    break;
                i++;
            }
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
