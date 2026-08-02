//
// Created by tgian on 26. 8. 2..
//
#include "PulseProcess.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include "LogProcess.hpp"

namespace oasis {

static const char* TAG = "PulseProcess";

bool PulseProcess::create(ADCUnit* adc_unit) {
    return create_impl(adc_unit);
}

bool PulseProcess::create_impl(ADCUnit* adc_unit) {

    BaseType_t result = xTaskCreatePinnedToCore(
        &PulseProcess::run,
        "PulseProcessTask",
        STACK_SIZE,
        this,
        PRIORITY,
        &m_task_handle,
        CORE
    );

    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create PulseProcess task");
        return false;
    }
    m_adc_unit = adc_unit;

    return true;
}

void PulseProcess::run(void* pvParameter) {
    auto* self = static_cast<PulseProcess*>(pvParameter);
    if (!self) {
        vTaskDelete(nullptr);
        return;
    }

    self->init_impl();
    self->update_impl();

}

bool IRAM_ATTR PulseProcess::isr_callback(
    gptimer_handle_t timer,
    const gptimer_alarm_event_data_t* edata,
    void* user_ctx
) {
    BaseType_t high_task_awoken = pdFALSE;

    // user_ctx로 전달된 PulseProcess 인스턴스 획득
    auto* self = static_cast<PulseProcess*>(user_ctx);
    if (self && self->m_task_handle) {
        // ISR 내부에서 Safe하게 Task Notification 전송
        vTaskNotifyGiveFromISR(self->m_task_handle, &high_task_awoken);
    }

    // Higher Priority Task가 깨어났다면 Context Switch 요청
    return (high_task_awoken == pdTRUE);
}

void PulseProcess::init_impl() {
    ESP_LOGI(TAG, "Initializing PulseProcess hardware & timers...");

    // GPTimer (General Purpose Timer) 초기화
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1'000'000, // 1MHz (1us resolution)
        .intr_priority = 0,
        .flags = {
            .intr_shared = 0,
            .allow_pd = false
        }
    };

    gptimer_handle_t gptimer = nullptr;
    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    gptimer_event_callbacks_t cbs = {
        .on_alarm = PulseProcess::isr_callback
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, this));

    gptimer_alarm_config_t alarm_config = {
        .alarm_count = 1000,
        .reload_count = 0,
        .flags = {
            .auto_reload_on_alarm = true
        }
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));

    m_adc_unit->add_channel(ADC_PULSE_CHANNEL_NUM);
    ESP_LOGI(TAG, "PulseProcess initialized successfully");
}

void PulseProcess::update_impl() {

    for (;;) {
        uint32_t notified_value = 0;
        BaseType_t notified = xTaskNotifyWait(
            0x00,
            ULONG_MAX,
            &notified_value,
            portMAX_DELAY
        );

        if (notified == pdTRUE) {
            int pulse_value = m_adc_unit->read(ADC_PULSE_CHANNEL_NUM);
            printf("pulse ADC : %f\n", static_cast<float>(pulse_value) / 4096.f);
        }
    }
}

} // namespace oasis