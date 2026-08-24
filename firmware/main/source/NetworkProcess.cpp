#include "NetworkProcess.hpp"

#include <array>
#include <atomic>
#include <string_view>
#include <type_traits>

#include "InitialConfigData.hpp"
#include "NotifyFlags.hpp"
#include "core/NetworkManager.hpp"
#include "esp_log.h"
#include "esp_mac.h"
#include "sdkconfig.h"

namespace oasis {
    namespace {
        constexpr char TAG[] = "NetworkProcess";
        constexpr size_t HTTP_RESPONSE_BUFFER_SIZE = 256;
        constexpr TickType_t CONNECTION_WAIT = pdMS_TO_TICKS(5000);
        constexpr TickType_t RETRY_DELAY = pdMS_TO_TICKS(1000);
    }

    NetworkProcess::NetworkProcess() = default;
    NetworkProcess::~NetworkProcess() = default;

    bool NetworkProcess::create() {
        return create_impl();
    }

    void NetworkProcess::run(void* pvParameter) {
        auto* process = static_cast<NetworkProcess*>(pvParameter);
        process->init_impl();
        process->update_impl();
        vTaskDelete(nullptr);
    }

    bool NetworkProcess::create_impl() {
        const BaseType_t result = xTaskCreatePinnedToCore(
            NetworkProcess::run,
            "network process",
            NetworkProcess::STACK_SIZE,
            this,
            NetworkProcess::PRIORITY | portPRIVILEGE_BIT,
            &m_task_handle,
            NetworkProcess::CORE
        );
        if (result != pdPASS) {
            ESP_LOGE(TAG, "network task creation failed");
            return false;
        }
        return true;
    }

    void NetworkProcess::init_impl() {
        m_main_process_task_handle = xTaskGetHandle("main process");
        if (m_main_process_task_handle == nullptr) {
            ESP_LOGE(TAG, "main process task handle not found");
            return;
        }

        const std::string_view ssid = CONFIG_OASIS_WIFI_SSID;
        const std::string_view password = CONFIG_OASIS_WIFI_PASSWORD;
        m_server_url = CONFIG_OASIS_SERVER_URL;

        if (ssid.empty() || m_server_url.empty()) {
            ESP_LOGE(TAG, "network is not configured; set OASIS_WIFI_SSID and OASIS_SERVER_URL");
        } else if (!NetworkManager::init_wifi(ssid, password)) {
            ESP_LOGE(TAG, "Wi-Fi initialization failed");
        } else {
            m_network_ready = true;
        }

        network_item::device_init init_item {};
        if (esp_read_mac(init_item.mac.data(), ESP_MAC_WIFI_STA) != ESP_OK) {
            ESP_LOGE(TAG, "station MAC read failed");
        } else {
            if (!NetworkQueue::instance().enqueue(init_item)) {
                ESP_LOGE(TAG, "device-init enqueue failed");
            }
        }

        std::atomic_thread_fence(std::memory_order_release);
        if (xTaskNotify(m_main_process_task_handle, notify::NETWORK_INITIAL_CONFIG_SYNC,
                        eSetBits) != pdPASS) {
            ESP_LOGE(TAG, "initial-config notification failed");
        }
    }

    void NetworkProcess::update_impl() {
        while (true) {
            if (!m_network_ready) {
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }
            if (!NetworkManager::wait_for_connection(CONNECTION_WAIT)) {
                ESP_LOGW(TAG, "Wi-Fi unavailable; queued packets are being retained");
                continue;
            }

            network_query_buffer query {};
            const size_t length = NetworkQueue::instance().recv_and_serialize_query(query);
            if (length == 0) continue;

            // Once removed from the queue, this query remains on the task stack until
            // it is delivered. New items continue to accumulate in NetworkQueue.
            send_query({query.data(), length});
        }
    }

    void NetworkProcess::send_query(std::string_view query) {
        std::array<char, HTTP_RESPONSE_BUFFER_SIZE> response {};
        uint32_t attempt = 0;
        while (true) {
            ++attempt;
            if (!NetworkManager::wait_for_connection(CONNECTION_WAIT)) {
                ESP_LOGW(TAG, "Wi-Fi unavailable; retaining in-flight packet (attempt %lu)",
                         static_cast<unsigned long>(attempt));
            } else {
                size_t response_length = 0;
                if (NetworkManager::send_get(m_server_url, query, response, response_length)) {
                    if (response_length != 0) {
                        apply_response({response.data(), response_length});
                    }
                    return;
                }
                ESP_LOGW(TAG, "GET failed; retaining in-flight packet (attempt %lu)",
                         static_cast<unsigned long>(attempt));
            }
            vTaskDelay(RETRY_DELAY);
        }
    }

    void NetworkProcess::apply_response(std::string_view response) {
        const auto item = deserialize_network_item(response);
        if (!item) {
            ESP_LOGW(TAG, "unrecognized GET response: %.*s", static_cast<int>(response.size()),
                     response.data());
            return;
        }

        std::visit([](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, network_item::device_init_recv>) {
                g_initial_config_data.worker_age = value.worker_age;
                g_initial_config_data.worker_weight = value.worker_weight;
                ESP_LOGI(TAG, "initial config received: age=%u weight=%u",
                         value.worker_age, value.worker_weight);
            } else if constexpr (std::is_same_v<T, network_item::worker_data_recv>) {
                g_initial_config_data.ttd = value.ttd;
                ESP_LOGI(TAG, "worker response received: ttd=%u", value.ttd);
            }
        }, *item);
    }
}
