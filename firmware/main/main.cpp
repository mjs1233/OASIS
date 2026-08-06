#include "MainProcess.hpp" // ✅ 올바른 경로
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" {
    void app_main(void) {
        oasis::MainProcess main_process;
        if (main_process.create()) {
            printf("MainProcess task created successfully.\n");
        } else {
            printf("Failed to create MainProcess task!\n");
        }

        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}