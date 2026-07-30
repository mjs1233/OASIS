#pragma once
#include <cstdint>
#include"core/ADCUnit.hpp"

namespace oasis {

    struct NTCConfig {
        float v_cc_mv;
        float r_fixed_ohm;
        float r_0_ohm;
        float t_0_kelvin;
        float beta;
    };

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

}