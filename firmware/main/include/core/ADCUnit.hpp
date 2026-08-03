//
// Created by tgian on 26. 7. 24..
//

#ifndef FIRMWARE_ADCUNIT_HPP
#define FIRMWARE_ADCUNIT_HPP
#include <cinttypes>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_continuous.h>
#include <array>
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
        ADCUnit() {}
        ~ADCUnit();
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

        bool read_oneshot(std::array<uint8_t, 4>& buffer, int wait) {
            if (xSemaphoreTake(m_adc_mutex,pdMS_TO_TICKS(wait)) == pdTRUE) {


            }
            else {
                return false;
            }
        };

        bool read_continuous(uint8_t* data,size_t length, uint32_t read_time) {

        };

    private:
        std::array<int, 4> m_oneshot_gpio{};
        int32_t m_oneshot_gpio_count = 0;

        int m_continuous_gpio{};

        SemaphoreHandle_t m_adc_mutex;


        adc_oneshot_unit_handle_t m_adc_handle;
        adc_cali_handle_t m_cali_handle;

    };
}

#endif //FIRMWARE_ADCUNIT_HPP
