//
// Created by tgian on 26. 7. 24..
//

#ifndef FIRMWARE_ADCUNIT_HPP
#define FIRMWARE_ADCUNIT_HPP
#include <cinttypes>
#include <esp_adc/adc_oneshot.h>
namespace oasis {
    class ADCUnit {
    public:
        ADCUnit(int32_t adc_num);
        ~ADCUnit();
        void add_channel(uint32_t channel);
        int raw_read(uint32_t channel);
        //unit: mv
        int read(uint32_t channel);

    private:
        int32_t adc_num;
        adc_oneshot_unit_handle_t adc_handle;
        adc_cali_handle_t cali_handle;

    };
}

#endif //FIRMWARE_ADCUNIT_HPP
