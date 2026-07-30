//
// Created by tgian on 26. 7. 25..
//

#ifndef FIRMWARE_NETWORKQUEUE_HPP
#define FIRMWARE_NETWORKQUEUE_HPP
#include "freertos/FreeRTOS.h"
#include <freertos/queue.h>
#include <concepts>
#include <array>

#include "NetworkItem.hpp"

namespace oasis {

    struct NetworkItem;

    class NetworkQueue {
    public:
        NetworkQueue() {
            m_queue_info = xQueueCreate(NetworkQueue::QUEUE_INFO_LENGTH, sizeof(network_item_variant));
            m_queue_critical = xQueueCreate(NetworkQueue::QUEUE_CRITICAL_LENGTH, sizeof(network_item_variant));
            m_semaphore_critical = xSemaphoreCreateBinary();
            configASSERT(m_semaphore_critical != NULL);
            m_queue_set = xQueueCreateSet(NetworkQueue::QUEUE_INFO_LENGTH + 1);

            xQueueAddToSet(m_queue_info, m_queue_set);
            xQueueAddToSet(m_semaphore_critical, m_queue_set);
        }
        ~NetworkQueue()=default;

        static void create() {
            if (g_instance == nullptr) {
                g_instance = new NetworkQueue();
            }
        }

        static NetworkQueue& instance() {
            if (g_instance == nullptr) {
                printf("init queue first!\n");
            }
            return *g_instance;
        }

        static void terminate() {
            delete g_instance;
        }

        template<typename T>
        bool enqueue(const T& item) {

            network_item_variant data = item;

            if constexpr (T::type == NetworkItemType::INFO) {
                return xQueueSendToBack(m_queue_info, &data, ENQUEUE_WAIT_TICK) == pdPASS;
            }
            else if constexpr (T::type == NetworkItemType::CRITICAL) {
                bool result = xQueueSendToBack(m_queue_critical, &data, ENQUEUE_WAIT_TICK) == pdPASS;
                if (result) {
                    xSemaphoreGive(m_semaphore_critical);
                }
            }
            else {
                static_assert(sizeof(T) == 0, "unhandled network item type\n");
                return false;
            }
        }

        uint32_t recv_and_serialize(std::array<uint8_t, 1024>& buf) {

            network_item_variant critical;
            if (xQueueReceive(m_queue_critical, &critical, 0) == pdTRUE) {
                return dispatch(critical,buf);
            }

            QueueSetMemberHandle_t activated =
              xQueueSelectFromSet(m_queue_set, portMAX_DELAY);

            if (activated == m_semaphore_critical) {

                xSemaphoreTake(m_semaphore_critical, 0);
                if (xQueueReceive(m_queue_critical, &critical, 0) == pdTRUE) {
                    return dispatch(critical,buf);
                }
            }
            else if (activated == m_queue_info) {
                network_item_variant info;
                if (xQueueReceive(m_queue_info, &info, 0) == pdTRUE) {
                    return dispatch(info,buf);
                }
            }
            return 0;

        }

    private:
        uint32_t dispatch(network_item_variant& item,std::array<uint8_t, 1024>& buf) {
            uint32_t length = 0;
            std::visit([&buf, &length](auto&& obj) {

                if (!obj.serialize(buf,length)) {
                    length = 0;
                }
            }, item);
            return length;
        }
        void drain() {
            //TODO) IMPL. drain.
        }

        QueueHandle_t m_queue_critical;
        QueueHandle_t m_queue_info;

        SemaphoreHandle_t m_semaphore_critical;
        QueueSetHandle_t m_queue_set;
        static NetworkQueue* g_instance;
        static constexpr int QUEUE_CRITICAL_LENGTH = 16;
        static constexpr int QUEUE_INFO_LENGTH = 16;
        static constexpr int ENQUEUE_WAIT_TICK = pdMS_TO_TICKS(100);
    };


}


#endif //FIRMWARE_NETWORKQUEUE_HPP
