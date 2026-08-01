//
// Created by tgian on 26. 7. 28..
//
#include "core/NetworkManager.hpp"
namespace oasis {
    void NetworkManager::init_wifi(const char* ssid, const char* password) {
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);

        s_wifi_event_group = xEventGroupCreate();

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        // Event Handler 등록
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &wifi_event_handler,
                                                            nullptr,
                                                            nullptr));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                            IP_EVENT_STA_GOT_IP,
                                                            &wifi_event_handler,
                                                            nullptr,
                                                            nullptr));

        wifi_config_t wifi_config = {};
        std::strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
        std::strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        printf("connecting WIFI...\n");
        xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        printf("connected!\n");
    }

    bool NetworkManager::send_binary_data(const char* url,const std::array<uint8_t, 1024>& payload, int length) {
        esp_http_client_config_t config = {};
        config.url = url;
        config.method = HTTP_METHOD_POST;
        config.timeout_ms = 5000;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (client == nullptr) {
            printf("http client) client init fail\n");
            return false;
        }

        esp_http_client_set_header(client, "Content-Type", "application/octet-stream");

        esp_http_client_set_post_field(client, reinterpret_cast<const char*>(payload.data()), length);

        //send
        esp_err_t err = esp_http_client_perform(client);
        bool success = false;

        if (err == ESP_OK) {
            int status_code = esp_http_client_get_status_code(client);
            int64_t content_length = esp_http_client_get_content_length(client);
            printf("HTTP POST send) | Status = %d, Content-Length = %lld\n", status_code, content_length);

            if (status_code >= 200 && status_code < 300) {
                success = true;
            }
        } else {
            printf("HTTP POST fail\n");
        }

        esp_http_client_cleanup(client);
        return success;
    }

    bool NetworkManager::send_signal(const char* url) {
        esp_http_client_config_t config = {};
        config.url = url;
        config.method = HTTP_METHOD_GET;
        config.timeout_ms = 5000;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (client == nullptr) {
            printf("http client) client init fail\n");
            return false;
        }

        // send
        esp_err_t err = esp_http_client_perform(client);
        bool success = false;

        if (err == ESP_OK) {
            int status_code = esp_http_client_get_status_code(client);
            int64_t content_length = esp_http_client_get_content_length(client);
            printf("HTTP GET send) | Status = %d, Content-Length = %lld\n", status_code, content_length);

            if (status_code >= 200 && status_code < 300) {
                success = true;
            }
        } else {
            printf("HTTP GET fail\n");
        }

        esp_http_client_cleanup(client);
        return success;
    }

    void NetworkManager::wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
            printf("reconnect WIFI...\n");
            esp_wifi_connect();
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
            auto event = static_cast<ip_event_got_ip_t*>(event_data);
            printf("WIFI) ip allocated %d.%d.%d.%d\n",IP2STR(&event->ip_info.ip));
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}