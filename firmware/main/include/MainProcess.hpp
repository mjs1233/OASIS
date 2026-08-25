//
// Created by tgian on 26. 7. 23..
//

#ifndef FIRMWARE_MAINPROCESS_HPP
#define FIRMWARE_MAINPROCESS_HPP

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "firmware.h"
#include "core/ADCUnit.hpp"
#include "NotifyFlags.hpp"

#include <inttypes.h>
#include <array>
#include <cstddef>

#include "IMUProcess.hpp"
#include "NTC.hpp"
#include "SHT31.hpp"
#include "MAX30102.hpp"

namespace oasis {
    class MainProcess {
    public:
        MainProcess();
        ~MainProcess();
        MainProcess(const MainProcess&) = delete;
        MainProcess(MainProcess&&) = delete;
        MainProcess& operator=(const MainProcess&) = delete;
        MainProcess& operator=(MainProcess&&) = delete;

        bool create();
        static void run(void* pvParameter);
        static void extern_condition_timer_callback(TimerHandle_t xtimer);
        static void pulse_capture_timer_callback(TimerHandle_t xtimer);
#if OASIS_ENABLE_PPG_SENSOR
        static void pulse_fifo_service_timer_callback(TimerHandle_t xtimer);
#endif

    private:
        bool create_impl();
        void init_impl();
        void update_impl();

        void imu_buffer_handle();
        void process_network_item();

        void read_extern_condition();
        void scan_sensor_i2c_bus();
#if OASIS_ENABLE_PPG_SENSOR
        void check_pulse_sensor();
#endif
        void begin_pulse_capture();
        void service_pulse_capture();
        void finish_pulse_capture();
        float calculate_bpm() const;

        // Board-specific defaults: update these three pins to match the PCB.
#if OASIS_ENABLE_NTC_SENSOR
        ADCUnit m_adc_unit_0 { };
        static constexpr gpio_num_t NTC_ADC_GPIO = GPIO_NUM_2;
#endif
        static constexpr gpio_num_t SENSOR_I2C_SDA_GPIO = GPIO_NUM_5;
        static constexpr gpio_num_t SENSOR_I2C_SCL_GPIO = GPIO_NUM_6;
#if OASIS_ENABLE_NTC_SENSOR
        static constexpr uint32_t ADC_TEMP_0_CHANNEL_NUM = 0;
#endif

        IMUProcess m_imu_process;
#if OASIS_ENABLE_NTC_SENSOR
        NTCConfig m_ntc_config {3300.0f, 10'000.0f, 10'000.0f, 298.15f, 3950.0f};
        NTCSensor m_ntc {m_adc_unit_0,ADC_TEMP_0_CHANNEL_NUM,m_ntc_config};
#endif
#if OASIS_ENABLE_SHT31_SENSOR || OASIS_ENABLE_PPG_SENSOR
        I2CUnit m_sensor_i2c {{
            .port = I2C_NUM_1,
            .sda_gpio = SENSOR_I2C_SDA_GPIO,
            .scl_gpio = SENSOR_I2C_SCL_GPIO,
            .frequency_hz = 100'000,
        }};
#endif
#if OASIS_ENABLE_SHT31_SENSOR
        SHT31 m_sht31 {m_sensor_i2c};
#endif
#if OASIS_ENABLE_PPG_SENSOR
        MAX30102 m_max30102 {m_sensor_i2c};
#endif
#if OASIS_ENABLE_NTC_SENSOR
        float m_last_ntc_temperature_c = 0.0f;
#endif
        SHT31Measurement m_last_sht31 {};
        std::array<PPGSample, 1024> m_ppg_samples {};
        size_t m_ppg_sample_count = 0;
        bool m_pulse_capture_active = false;
#if OASIS_ENABLE_PPG_SENSOR
        bool m_max30102_ready = false;
#endif
        TaskHandle_t m_task_handle = nullptr;
        TimerHandle_t m_extern_cond_timer = nullptr;
        TimerHandle_t m_pulse_capture_timer = nullptr;
#if OASIS_ENABLE_PPG_SENSOR
        TimerHandle_t m_pulse_fifo_service_timer = nullptr;
#endif
        static constexpr uint32_t CORE = 1;
        static constexpr uint32_t STACK_SIZE = 4096;
        static constexpr uint32_t PRIORITY = 2;
        static constexpr uint32_t ENVIRONMENT_CYCLE_MS = 30'000;
        static constexpr uint32_t PULSE_CAPTURE_MS = 10'000;
#if OASIS_ENABLE_PPG_SENSOR
        static constexpr uint32_t PULSE_FIFO_SERVICE_MS = 100;
#endif


        //change channel number!

    };
}
#endif //FIRMWARE_MAINPROCESS_HPP
