//
// Created by tgian on 26. 7. 25..
//

#ifndef FIRMWARE_IMUPROCESS_HPP
#define FIRMWARE_IMUPROCESS_HPP
#include <optional>

#include "freertos/FreeRTOS.h"
#include "driver/gptimer.h"
#include "IMUData.hpp"
#include "core/BufferPool.hpp"
#include "core/I2CUnit.hpp"
#include "MPU6050.hpp"

namespace oasis {

    class IMUProcess {
    public:
        IMUProcess() = default;
        ~IMUProcess() = default;

        IMUProcess(const IMUProcess&) = delete;
        IMUProcess(IMUProcess&&) = delete;
        IMUProcess& operator=(const IMUProcess&) = delete;
        IMUProcess& operator=(IMUProcess&&) = delete;

        bool create();

        static void run(void* pvParameter);
        static bool isr_callback(
            gptimer_handle_t timer,
            const gptimer_alarm_event_data_t *edata,
            void *user_ctx
            );

        using IMUBuffer = BufferPool<IMUData,24,8>;
        IMUBuffer& get_buffer_pool();
    private:
        bool create_impl();
        void init_impl();
        void update_impl();


        IMUBuffer m_buffer;

        // I2C0 is reserved for the MPU6050; MainProcess uses I2C1 for SHT31/MAX30102.
        I2CUnit m_imu_i2c {{
            .port = I2C_NUM_0,
            .sda_gpio = GPIO_NUM_9,
            .scl_gpio = GPIO_NUM_8,
            .frequency_hz = 100'000,
        }};
        std::optional<MPU6050> m_mpu6050;
        TaskHandle_t m_task_handle = nullptr;
        TaskHandle_t m_main_process_task_handle = nullptr;

        static constexpr uint32_t CORE = 1;
        static constexpr uint32_t STACK_SIZE = 4096;
        static constexpr uint32_t PRIORITY = 6;
    };

}
#endif //FIRMWARE_IMUPROCESS_HPP
