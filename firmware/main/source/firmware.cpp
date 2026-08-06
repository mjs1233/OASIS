#include "firmware.h"
#include "MainProcess.hpp"
#include "NetworkProcess.hpp"
#include <optional>

namespace oasis {
    std::optional<MainProcess> g_main_process = std::nullopt;
    std::optional<NetworkProcess> g_network_process = std::nullopt;

    void startup() {
        g_main_process.emplace();
        g_main_process->create();
        g_network_process.emplace();
        g_network_process->create();
    }
}

extern "C" {
void app_main(void) {
    oasis::startup();
}
}

/*std::optional<oasis::MainProcess> g_main_process = std::nullopt;
std::optional<oasis::NetworkProcess> g_network_process = std::nullopt;
extern "C" {

void app_main(void) {
    oasis::startup();
}
}

void oasis::startup() {

    g_main_process.emplace();
    g_main_process->create();
    g_network_process.emplace();
    g_network_process->create();*/
