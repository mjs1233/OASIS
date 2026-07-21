#include "firmware.h"
#include "core/StagingBuffer.hpp"
#include "freertos/task.h"
#include "freertos/FreeRTOS.h"
extern "C" {

void app_main(void) {

    oasis::startup();
    oasis::run();
    oasis::terminate();

}

}

void oasis::startup() {

}

void oasis::run() {

}

void oasis::terminate() {

}