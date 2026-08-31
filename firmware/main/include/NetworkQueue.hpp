//
// Created by tgian on 26. 7. 25..
//

#ifndef FIRMWARE_NETWORKQUEUE_HPP
#define FIRMWARE_NETWORKQUEUE_HPP
#include "freertos/FreeRTOS.h"
#include <freertos/queue.h>
#include <concepts>
#include <type_traits>

#include "esp_log.h"

#include "NetworkItem.hpp"

namespace oasis {

    struct NetworkItem;

    class NetworkQueue {
    public:
        NetworkQueue() {
            m_queue_info = xQueueCreate(NetworkQueue::QUEUE_INFO_LENGTH, sizeof(network_item_variant));
            m_queue_critical = xQueueCreate(NetworkQueue::QUEUE_CRITICAL_LENGTH, sizeof(network_item_variant));
            configASSERT(m_queue_info != NULL && m_queue_critical != NULL);
            m_queue_set = xQueueCreateSet(NetworkQueue::QUEUE_INFO_LENGTH + NetworkQueue::QUEUE_CRITICAL_LENGTH);
            configASSERT(m_queue_set != NULL);

            configASSERT(xQueueAddToSet(m_queue_info, m_queue_set) == pdPASS);
            configASSERT(xQueueAddToSet(m_queue_critical, m_queue_set) == pdPASS);
        }
        ~NetworkQueue() {
            vQueueDelete(m_queue_info);
            vQueueDelete(m_queue_critical);
            vQueueDelete(m_queue_set);
        }

        static void create() {
            if (g_instance == nullptr) {
                g_instance = new NetworkQueue();
            }
        }

        static NetworkQueue& instance() {
            configASSERT(g_instance != nullptr);
            return *g_instance;
        }

        static void terminate() {
            delete g_instance;
            g_instance = nullptr;
        }

        template<network_item_trait T>
        bool enqueue(const T& item) {

            network_item_variant data = item;

            bool queued = false;
            if constexpr (T::type == NetworkItemType::INFO) {
                queued = xQueueSendToBack(m_queue_info, &data, ENQUEUE_WAIT_TICK) == pdPASS;
            }
            else if constexpr (T::type == NetworkItemType::CRITICAL) {
                queued = xQueueSendToBack(m_queue_critical, &data, ENQUEUE_WAIT_TICK) == pdPASS;
            }
            else {
                static_assert(sizeof(T) == 0, "unhandled network item type\n");
            }

            if constexpr (std::is_same_v<T, network_item::worker_data>) {
                ESP_LOGI("NetworkQueue", "enqueue=%s worker_data temp=%u hum=%u bpm=%u battery=%u",
                         queued ? "true" : "false", item.raw_temp, item.raw_hum, item.raw_bpm,
                         item.battery_voltage);
            }
            return queued;
        }

        size_t recv_and_serialize_query(network_query_buffer& buffer) {

            network_item_variant critical;
            if (xQueueReceive(m_queue_critical, &critical, 0) == pdTRUE) {
                return dispatch(critical, buffer);
            }

            QueueSetMemberHandle_t activated =
              xQueueSelectFromSet(m_queue_set, portMAX_DELAY);

            if (activated == m_queue_critical) {
                if (xQueueReceive(m_queue_critical, &critical, 0) == pdTRUE) {
                    return dispatch(critical, buffer);
                }
            }
            else if (activated == m_queue_info) {
                network_item_variant info;
                if (xQueueReceive(m_queue_info, &info, 0) == pdTRUE) {
                    return dispatch(info, buffer);
                }
            }
            return 0;

        }

    private:
        size_t dispatch(const network_item_variant& item, network_query_buffer& buffer) {
            size_t length = 0;
            return serialize_network_item(item, buffer, length) ? length : 0;
        }
        void drain() {
            //TODO) IMPL. drain.
        }

        QueueHandle_t m_queue_critical;
        QueueHandle_t m_queue_info;

        QueueSetHandle_t m_queue_set;
        static NetworkQueue* g_instance;
        static constexpr int QUEUE_CRITICAL_LENGTH = 64;
        static constexpr int QUEUE_INFO_LENGTH = 16;
        static constexpr int ENQUEUE_WAIT_TICK = pdMS_TO_TICKS(100);
    };


}


#endif //FIRMWARE_NETWORKQUEUE_HPP
