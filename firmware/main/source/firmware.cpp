#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "core/StagingBuffer.hpp"
#include "firmware.h"
#include "MainProcess.hpp"
#include "NetworkProcess.hpp"

std::optional<oasis::MainProcess> g_main_process = std::nullopt;
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
    g_network_process->create();
}
