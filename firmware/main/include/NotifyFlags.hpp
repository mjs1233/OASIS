//
// Created by tgian on 26. 7. 24..
//

#ifndef FIRMWARE_NOTIFYFLAGS_HPP
#define FIRMWARE_NOTIFYFLAGS_HPP
#include <cinttypes>
namespace oasis::notify {
    enum NotifyFlags {
        NETWORK_INITIAL_CONFIG_SYNC = 1 << 1
    };
}

#endif //FIRMWARE_NOTIFYFLAGS_HPP
