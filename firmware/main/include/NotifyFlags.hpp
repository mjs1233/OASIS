//
// Created by tgian on 26. 7. 24..
//

#ifndef FIRMWARE_NOTIFYFLAGS_HPP
#define FIRMWARE_NOTIFYFLAGS_HPP
#include <cinttypes>
namespace oasis::notify {
    enum NotifyFlags {
        NETWORK_INITIAL_CONFIG_SYNC = 1 << 1,

        ISR_IMU_BUFFER_FULL = 1 << 2,
        TIMER_EXTREN_COND_CYCLE = 1 << 3,
        TIMER_PULSE_CAPTURE_END = 1 << 4
    };
}

#endif //FIRMWARE_NOTIFYFLAGS_HPP
