#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "core/StagingBuffer.hpp"
#include "firmware.h"
#include "MainProcess.hpp"
std::optional<oasis::MainProcess> g_main_process = std::nullopt;

extern "C" {

void app_main(void) {

    oasis::startup();
    oasis::run();
    oasis::terminate();

}

}

void oasis::startup() {

    g_main_process.emplace();
    g_main_process->create();
}

void oasis::run() {

}

void oasis::terminate() {

}