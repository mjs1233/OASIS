#pragma once
#include <cstdint>
#include"core/ADCUnit.hpp"

namespace oasis {

    // ADCUnit 클래스가 다른 곳에 선언되어 있다고 가정합니다.
    class ADCUnit;

    // NTC 센서 설정 정보를 담는 구조체 선언
    struct NTCConfig {
        float v_cc_mv;
        float r_fixed_ohm;
        float r_0_ohm;
        float t_0_kelvin;
        float beta;
    };

    // NTC 센서 클래스 선언
    class NTCSensor {
    public:
        NTCSensor(ADCUnit& adc_unit, uint32_t channel, const NTCConfig& config);
        void init();
        float read();

    private:
        float calculate_resistance(float v_adc_mv) const;
        float calculate_temperature(float r_ntc_ohm) const;

        ADCUnit& adc_unit;
        uint32_t channel;
        NTCConfig config;
    };

} // namespace oasis