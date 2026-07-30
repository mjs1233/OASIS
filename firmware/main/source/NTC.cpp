#include "NTC.hpp"
#include <cmath>
#include <algorithm>

namespace oasis {

    NTCSensor::NTCSensor(ADCUnit& adc_unit, uint32_t channel, const NTCConfig& config)
        : adc_unit(adc_unit), channel(channel), config(config) {
    }

    void NTCSensor::init() {
        // Register the ADC channel during initialization
        adc_unit.add_channel(channel);
    }

    float NTCSensor::read() {
        // 1. Read calibrated voltage from ADCUnit in mV
        int raw_mv = adc_unit.read(channel);
        float v_adc_mv = static_cast<float>(raw_mv);

        // 2. Calculate NTC resistance based on high-side circuit topology
        float r_ntc = calculate_resistance(v_adc_mv);

        // 3. Convert resistance to Celsius temperature using the Beta equation
        return calculate_temperature(r_ntc);
    }

    float NTCSensor::calculate_resistance(float v_adc_mv) const {
        // Prevent division by zero and clamp voltage within valid range
        v_adc_mv = std::clamp(v_adc_mv, 0.1f, config.v_cc_mv - 0.1f);

        // High-side circuit equation:
        // V_adc = V_cc * R_fixed / (R_ntc + R_fixed)
        // R_ntc = R_fixed * (V_cc / V_adc - 1)
        return config.r_fixed_ohm * ((config.v_cc_mv / v_adc_mv) - 1.0f);
    }

    float NTCSensor::calculate_temperature(float r_ntc_ohm) const {
        // Return an error value if resistance is invalid
        if (r_ntc_ohm <= 0.0f) {
            return -999.0f; 
        }

        // Beta equation: 1/T = 1/T0 + (1/Beta) * ln(R / R0)
        float inv_t = (1.0f / config.t_0_kelvin) + 
                      (1.0f / config.beta) * std::log(r_ntc_ohm / config.r_0_ohm);

        float temp_kelvin = 1.0f / inv_t;

        // Convert Kelvin to Celsius
        return temp_kelvin - 273.15f;
    }

} // namespace oasis