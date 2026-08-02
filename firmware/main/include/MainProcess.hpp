//
// Created by tgian on 26. 7. 23..
//

#ifndef FIRMWARE_MAINPROCESS_HPP
#define FIRMWARE_MAINPROCESS_HPP

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "core/ADCUnit.hpp"
#include "NotifyFlags.hpp"

#include <inttypes.h>

#include "IMUProcess.hpp"
#include "NTC.hpp"
#include "PulseProcess.hpp"

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

    private:
        bool create_impl();
        void init_impl();
        void update_impl();

        void imu_buffer_handle();
        void process_network_item();

        void read_extern_condition();

        ADCUnit m_adc_unit_0 = {ADC_UNIT_1 };
        static constexpr uint32_t ADC_TEMP_0_CHANNEL_NUM = 1;
        static constexpr uint32_t ADC_BATTERY_CHANNEL_NUM = 3;

        IMUProcess m_imu_process;
        PulseProcess m_pulse_process;
        NTCConfig m_ntc_config {};
        NTCSensor m_ntc {m_adc_unit_0,ADC_TEMP_0_CHANNEL_NUM,m_ntc_config};
        TaskHandle_t m_task_handle = nullptr;
        TimerHandle_t m_extern_cond_timer = nullptr;
        static constexpr uint32_t CORE = 1;
        static constexpr uint32_t STACK_SIZE = 4096;
        static constexpr uint32_t PRIORITY = 2;


        //change channel number!

    };
}
#endif //FIRMWARE_MAINPROCESS_HPP
