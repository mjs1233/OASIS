#include "core/NetworkManager.hpp"

#include <algorithm>
#include <array>
#include <cstring>

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

namespace oasis {
    namespace {
        constexpr char TAG[] = "NetworkManager";

        bool accepted_init_result(esp_err_t result) {
            return result == ESP_OK || result == ESP_ERR_INVALID_STATE;
        }
    }

    bool NetworkManager::init_wifi(std::string_view ssid, std::string_view password) {
        wifi_config_t wifi_config {};
        if (ssid.empty() || ssid.size() > sizeof(wifi_config.sta.ssid) ||
            password.size() > sizeof(wifi_config.sta.password)) {
            ESP_LOGE(TAG, "invalid Wi-Fi credentials");
            return false;
        }

        if (s_wifi_initialized) return true;

        esp_err_t result = nvs_flash_init();
        if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            result = nvs_flash_erase();
            if (result == ESP_OK) result = nvs_flash_init();
        }
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "NVS initialization failed: %s", esp_err_to_name(result));
            return false;
        }

        s_wifi_event_group = xEventGroupCreate();
        if (s_wifi_event_group == nullptr) {
            ESP_LOGE(TAG, "Wi-Fi event group allocation failed");
            return false;
        }

        result = esp_netif_init();
        if (!accepted_init_result(result)) {
            ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(result));
            return false;
        }
        result = esp_event_loop_create_default();
        if (!accepted_init_result(result)) {
            ESP_LOGE(TAG, "event loop creation failed: %s", esp_err_to_name(result));
            return false;
        }
        if (esp_netif_create_default_wifi_sta() == nullptr) {
            ESP_LOGE(TAG, "default Wi-Fi station creation failed");
            return false;
        }

        wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
        if ((result = esp_wifi_init(&wifi_init)) != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(result));
            return false;
        }

        if ((result = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                 &wifi_event_handler, nullptr)) != ESP_OK ||
            (result = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                 &wifi_event_handler, nullptr)) != ESP_OK) {
            ESP_LOGE(TAG, "Wi-Fi event handler registration failed: %s", esp_err_to_name(result));
            return false;
        }

        std::memcpy(wifi_config.sta.ssid, ssid.data(), ssid.size());
        std::memcpy(wifi_config.sta.password, password.data(), password.size());
        wifi_config.sta.threshold.authmode = password.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

        if ((result = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK ||
            (result = esp_wifi_set_config(WIFI_IF_STA, &wifi_config)) != ESP_OK ||
            (result = esp_wifi_start()) != ESP_OK) {
            ESP_LOGE(TAG, "Wi-Fi start failed: %s", esp_err_to_name(result));
            return false;
        }

        s_wifi_initialized = true;
        return true;
    }

    bool NetworkManager::wait_for_connection(TickType_t timeout) {
        if (s_wifi_event_group == nullptr) return false;
        const EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                                                     pdFALSE, pdTRUE, timeout);
        return (bits & WIFI_CONNECTED_BIT) != 0;
    }

    bool NetworkManager::is_connected() {
        return s_wifi_event_group != nullptr &&
               (xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT) != 0;
    }

    bool NetworkManager::send_get(std::string_view base_url,
                                  std::string_view query,
                                  std::span<char> response,
                                  size_t& response_length) {
        response_length = 0;
        if (!is_connected() || base_url.empty() || query.empty() || response.empty()) return false;

        std::array<char, REQUEST_URL_BUFFER_SIZE> request_url {};
        const bool needs_separator = base_url.back() != '?' && base_url.back() != '&';
        const char separator = base_url.find('?') == std::string_view::npos ? '?' : '&';
        const size_t total_length = base_url.size() + (needs_separator ? 1 : 0) + query.size();
        if (total_length >= request_url.size()) {
            ESP_LOGE(TAG, "GET URL exceeds %u bytes", static_cast<unsigned>(request_url.size() - 1));
            return false;
        }

        char* cursor = std::copy(base_url.begin(), base_url.end(), request_url.begin());
        if (needs_separator) *cursor++ = separator;
        cursor = std::copy(query.begin(), query.end(), cursor);
        *cursor = '\0';

        HttpResponseContext context {response};
        response.front() = '\0';

        esp_http_client_config_t config {};
        config.url = request_url.data();
        config.method = HTTP_METHOD_GET;
        config.timeout_ms = 5000;
        config.event_handler = &http_event_handler;
        config.user_data = &context;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (client == nullptr) {
            ESP_LOGE(TAG, "HTTP client initialization failed");
            return false;
        }

        esp_http_client_set_header(client, "Accept", "text/plain");
        const esp_err_t result = esp_http_client_perform(client);
        const int status_code = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (result != ESP_OK) {
            ESP_LOGW(TAG, "HTTP GET failed: %s", esp_err_to_name(result));
            return false;
        }
        if (context.overflow) {
            ESP_LOGE(TAG, "HTTP response exceeded %u bytes", static_cast<unsigned>(response.size() - 1));
            return false;
        }
        if (status_code < 200 || status_code >= 300) {
            ESP_LOGW(TAG, "HTTP GET returned status %d", status_code);
            return false;
        }

        response_length = context.length;
        ESP_LOGI(TAG, "HTTP GET completed: status=%d response=%u bytes", status_code,
                 static_cast<unsigned>(response_length));
        return true;
    }

    esp_err_t NetworkManager::http_event_handler(esp_http_client_event_t* event) {
        if (event->event_id != HTTP_EVENT_ON_DATA || event->data_len <= 0) return ESP_OK;

        auto* context = static_cast<HttpResponseContext*>(event->user_data);
        if (context == nullptr || context->response.empty()) return ESP_ERR_INVALID_ARG;

        const size_t incoming = static_cast<size_t>(event->data_len);
        const size_t available = context->response.size() - context->length - 1;
        if (incoming > available) {
            context->overflow = true;
            return ESP_OK;
        }

        std::memcpy(context->response.data() + context->length, event->data, incoming);
        context->length += incoming;
        context->response[context->length] = '\0';
        return ESP_OK;
    }

    void NetworkManager::wifi_event_handler(void*, esp_event_base_t event_base,
                                            int32_t event_id, void*) {
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            ESP_LOGW(TAG, "Wi-Fi disconnected; reconnecting");
            esp_wifi_connect();
        } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            ESP_LOGI(TAG, "Wi-Fi connected");
        }
    }
}
