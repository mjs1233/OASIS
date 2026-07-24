//
// Created by tgian on 26. 7. 24..
//
#include "core/ADCUnit.hpp"

#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"


namespace oasis {

ADCUnit::ADCUnit(int32_t adc_num)
    : adc_num(adc_num), adc_handle(nullptr), cali_handle(nullptr) {

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = (adc_num == 1) ? ADC_UNIT_1 : ADC_UNIT_2,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    esp_err_t ret = adc_oneshot_new_unit(&init_config, &adc_handle);
    if (ret != ESP_OK) {
        printf("adc init fail. (num : %ld)\n", adc_num);
    }
}

ADCUnit::~ADCUnit() {
    if (cali_handle != nullptr) {
#if CONFIG_ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        adc_cali_delete_scheme_curve_fitting(cali_handle);
#elif CONFIG_ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        adc_cali_delete_scheme_line_fitting(cali_handle);
#endif
        cali_handle = nullptr;
    }

    // 2. ADC Unit 핸들 해제
    if (adc_handle != nullptr) {
        adc_oneshot_del_unit(adc_handle);
        adc_handle = nullptr;
    }
}

void ADCUnit::add_channel(uint32_t channel) {
    if (adc_handle == nullptr) {
        printf("adc is not initialized. (num : %ld, ch : %ld)\n",adc_num, channel);
        return;
    }

    auto chan = static_cast<adc_channel_t>(channel);

    //no attenuation.
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_0,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    esp_err_t ret = adc_oneshot_config_channel(adc_handle, chan, &config);
    if (ret != ESP_OK) {
        printf("configuration fail (num : %ld, ch : %ld)\n", adc_num, channel);
        return;
    }

    if (cali_handle == nullptr) {
        adc_unit_t unit = (adc_num == 1) ? ADC_UNIT_1 : ADC_UNIT_2;

#if CONFIG_ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = chan,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle);
#elif CONFIG_ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &cali_handle);
#endif

        if (ret != ESP_OK) {
            printf("adc cali create fail (num : %ld, ch : %ld)\n", adc_num, channel);
            cali_handle = nullptr;
        }
    }
}

int ADCUnit::raw_read(uint32_t channel) {
    if (adc_handle == nullptr) return -1;

    int raw_val = 0;
    auto chan = static_cast<adc_channel_t>(channel);
    esp_err_t ret = adc_oneshot_read(adc_handle, chan, &raw_val);

    if (ret != ESP_OK) {
            printf("adc read fail (num : %ld, ch : %ld)\n", adc_num, channel);
        return -1;
    }

    return raw_val;
}

int ADCUnit::read(uint32_t channel) {
    int raw = raw_read(channel);
    if (raw < 0) return -1;

    if (cali_handle != nullptr) {
        int voltage_mv = 0;
        esp_err_t ret = adc_cali_raw_to_voltage(cali_handle, raw, &voltage_mv);
        if (ret == ESP_OK) {
            return voltage_mv;
        }
            printf("adc conversion fail (num : %ld, ch : %ld)\n", adc_num, channel);
    }

    return (raw * 3300) / 4095;
}

} // namespace oasis