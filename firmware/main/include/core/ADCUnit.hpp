//
// Created by tgian on 26. 7. 24..
//

#ifndef FIRMWARE_ADCUNIT_HPP
#define FIRMWARE_ADCUNIT_HPP
#include <cinttypes>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_continuous.h>
#include <array>
#include <functional>
#include <freertos/FreeRTOS.h>
#include "freertos/semphr.h"

namespace oasis {
    namespace __details {
        constexpr uint32_t default_adc_unit_num = 0;

        class OneshotADC {

        };

        class ContinuousADC {
        public:
            ContinuousADC() {

            }
        };
    }


    class ADCUnit {
    public:
        ADCUnit() {
            m_adc_mutex = xSemaphoreCreateMutex();
            configASSERT(m_adc_mutex != nullptr);
        }
        ~ADCUnit(){
            if (m_adc_mutex != nullptr) {
                vSemaphoreDelete(m_adc_mutex);
            }
        };
        int32_t add_oneshot_gpio(int gpio) {
            if (m_oneshot_gpio_count == 4) {
                return -1;
            }
            m_oneshot_gpio[m_oneshot_gpio_count] = gpio;
            m_oneshot_gpio_count++;
            return m_oneshot_gpio_count - 1;
        }

        void set_continuous_gpio(int gpio) {
            m_continuous_gpio = gpio;
        }

        bool read_oneshot(std::array<int, 4>& buffer, int wait_ms) {
            if (xSemaphoreTake(m_adc_mutex,pdMS_TO_TICKS(wait_ms)) == pdTRUE) {

                read_oneshot_impl(buffer);
                configASSERT(xSemaphoreGive(m_adc_mutex) == pdTRUE);
                return true;
            }
            else {
                return false;
            }

        };

        bool read_continuous(size_t length, const std::function<bool(const std::array<uint8_t,256>& buffer)>& callback) {
            return false;
        };

    private:
        void read_oneshot_impl(std::array<int, 4>& buffer) {
            adc_oneshot_unit_handle_t adc1_handle;
            adc_oneshot_unit_init_cfg_t config = {
                .unit_id = ADC_UNIT_1,
                .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
                .ulp_mode = ADC_ULP_MODE_DISABLE,

            };
            ESP_ERROR_CHECK(adc_oneshot_new_unit(&config, &adc1_handle));
            adc_oneshot_chan_cfg_t channel_config = {
                .atten = ADC_ATTEN_DB_12,
                .bitwidth = ADC_BITWIDTH_DEFAULT,
            };
            for (uint32_t i = 0; i < 4; i++) {
                if (m_oneshot_gpio[i] != -1) {
                    adc_channel_t channel;
                    ESP_ERROR_CHECK(adc_oneshot_io_to_channel(m_oneshot_gpio[i],&config.unit_id, &channel));
                    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, channel, &channel_config));
                    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, channel, &buffer[i]));
                    printf("ADC read ch : %d\n", channel);
                }
            }
            ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
        }


        std::array<int, 4> m_oneshot_gpio{-1,-1,-1,-1};
        int32_t m_oneshot_gpio_count = 0;

        int m_continuous_gpio{};

        SemaphoreHandle_t m_adc_mutex;


        adc_oneshot_unit_handle_t m_adc_handle;
        adc_cali_handle_t m_cali_handle;

    };
}

#endif //FIRMWARE_ADCUNIT_HPP
