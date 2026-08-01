//
// Created by tgian on 26. 7. 28..
//

#ifndef FIRMWARE_NETWORKMANAGER_HPP
#define FIRMWARE_NETWORKMANAGER_HPP
#include <iostream>
#include <vector>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_client.h"

namespace oasis {
    static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

    class NetworkManager {
    public:
        static void init_wifi(const char* ssid, const char* password);
        static bool send_binary_data(const char* url, const std::array<uint8_t, 1024>& data, int length);
        static bool send_signal(const char* url);
    private:
        static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

    };
}
#endif //FIRMWARE_NETWORKMANAGER_HPP
