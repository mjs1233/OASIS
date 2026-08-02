//
// Created by tgian on 26. 8. 2..
//

#ifndef FIRMWARE_PULSEPROCESS_HPP
#define FIRMWARE_PULSEPROCESS_HPP
#include "freertos/FreeRTOS.h"
#include "driver/gptimer.h"
#include "core/ADCUnit.hpp"
namespace oasis {

    class PulseProcess {
    public:
        PulseProcess() = default;
        ~PulseProcess() = default;

        PulseProcess(const PulseProcess&) = delete;
        PulseProcess(PulseProcess&&) = delete;
        PulseProcess& operator=(const PulseProcess&) = delete;
        PulseProcess& operator=(PulseProcess&&) = delete;

        bool create(ADCUnit* adc_unit);

        static void run(void* pvParameter);
        static bool isr_callback(
            gptimer_handle_t timer,
            const gptimer_alarm_event_data_t *edata,
            void *user_ctx
            );

    private:
        bool create_impl(ADCUnit* adc_unit);
        void init_impl();
        void update_impl();


        TaskHandle_t m_task_handle = nullptr;
        TaskHandle_t m_main_process_task_handle = nullptr;

        oasis::ADCUnit* m_adc_unit = nullptr;

        static constexpr uint32_t CORE = 1;
        static constexpr uint32_t STACK_SIZE = 4096;
        static constexpr uint32_t PRIORITY = 7;
        static constexpr uint32_t ADC_PULSE_CHANNEL_NUM = 2;
    };

}
#endif //FIRMWARE_PULSEPROCESS_HPP
