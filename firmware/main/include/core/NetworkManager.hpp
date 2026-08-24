#ifndef FIRMWARE_NETWORKMANAGER_HPP
#define FIRMWARE_NETWORKMANAGER_HPP

#include <cstddef>
#include <span>
#include <string_view>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

namespace oasis {
    class NetworkManager {
    public:
        static bool init_wifi(std::string_view ssid, std::string_view password);
        static bool wait_for_connection(TickType_t timeout);
        static bool is_connected();

        static bool send_get(std::string_view base_url,
                             std::string_view query,
                             std::span<char> response,
                             size_t& response_length);

    private:
        struct HttpResponseContext {
            std::span<char> response;
            size_t length = 0;
            bool overflow = false;
        };

        static void wifi_event_handler(void* arg,
                                       esp_event_base_t event_base,
                                       int32_t event_id,
                                       void* event_data);
        static esp_err_t http_event_handler(esp_http_client_event_t* event);

        inline static EventGroupHandle_t s_wifi_event_group = nullptr;
        inline static bool s_wifi_initialized = false;
        static constexpr EventBits_t WIFI_CONNECTED_BIT = BIT0;
        static constexpr size_t REQUEST_URL_BUFFER_SIZE = 512;
    };
}

#endif // FIRMWARE_NETWORKMANAGER_HPP
