//
// Created by tgian on 26. 7. 24..
//

#ifndef FIRMWARE_INITIALCONFIGDATA_HPP
#define FIRMWARE_INITIALCONFIGDATA_HPP
#include <cstdint>
namespace oasis {
    struct InitialConfigData {
        uint8_t worker_age = 0;
        uint8_t worker_weight = 0;
        uint8_t ttd = 0;
    };

    inline InitialConfigData g_initial_config_data = {};
}



#endif //FIRMWARE_INITIALCONFIGDATA_HPP
