#ifndef FIRMWARE_FIRMWARE_H
#define FIRMWARE_FIRMWARE_H

#include <optional>

// Set to 0 to build without accessing the MAX30102/PPG hardware. The 30-second
// measurement cycle and worker-data packet remain active; BPM is sent as zero.
#ifndef OASIS_ENABLE_PPG_SENSOR
#define OASIS_ENABLE_PPG_SENSOR 0
#endif

namespace oasis {
    void startup();
}
#endif //FIRMWARE_FIRMWARE_H
