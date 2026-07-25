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


        TaskHandle_t m_task_handle = nullptr;
        TimerHandle_t m_extern_cond_timer = nullptr;
        static constexpr uint32_t CORE = 1;
        static constexpr uint32_t STACK_SIZE = 4096;
        static constexpr uint32_t PRIORITY = 2;

        ADCUnit m_adc_unit_0 = {ADC_UNIT_1 };

        //change channel number!
        static constexpr uint32_t ADC_TEMP_0_CHANNEL_NUM = 1;
        static constexpr uint32_t ADC_TEMP_1_CHANNEL_NUM = 2;
        static constexpr uint32_t ADC_BATTERY_CHANNEL_NUM = 3;

    };
}
#endif //FIRMWARE_MAINPROCESS_HPP
