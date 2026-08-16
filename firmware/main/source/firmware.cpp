#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "core/StagingBuffer.hpp"
#include "firmware.h"

#include "LogProcess.hpp"
#include "MainProcess.hpp"
#include "NetworkProcess.hpp"
#include "esp_log.h"

std::optional<oasis::MainProcess> g_main_process = std::nullopt;
std::optional<oasis::NetworkProcess> g_network_process = std::nullopt;
std::optional<oasis::LogProcess> g_log_process = std::nullopt;

extern "C" {

void app_main(void) {
    oasis::startup();
}
}

void oasis::startup() {
    oasis::NetworkQueue::create();
    g_log_process.emplace();
    if (!g_log_process->create()) {
        ESP_LOGE("firmware", "log process creation failed");
    }
    g_main_process.emplace();
    if (!g_main_process->create()) {
        ESP_LOGE("firmware", "main process creation failed");
    }
    g_network_process.emplace();
    if (!g_network_process->create()) {
        ESP_LOGE("firmware", "network process creation failed");
    }
}
