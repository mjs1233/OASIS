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

            m_queue_set = xQueueCreateSet(NetworkQueue::QUEUE_INFO_LENGTH + NetworkQueue::QUEUE_CRITICAL_LENGTH);

            xQueueAddToSet(m_queue_info, m_queue_set);
            xQueueAddToSet(m_queue_critical, m_queue_set);
        }
        ~NetworkQueue();

        template<typename T>
        bool enqueue(const T& item) {

            network_item_variant data = item;

            if constexpr (T::type == NetworkItemType::INFO) {
                return xQueueSendToBack(m_queue_info, &data, ENQUEUE_WAIT_TICK) == pdPASS;
            }
            else if constexpr (T::type == NetworkItemType::CRITICAL) {
                return xQueueSendToBack(m_queue_critical, &data, ENQUEUE_WAIT_TICK) == pdPASS;
            }
            else {
                static_assert(sizeof(T) == 0, "unhandled network item type\n");
                return false;
            }
        }

        void recv_and_serialize() {
            //dispatch.

            QueueSetMemberHandle_t activated =
              xQueueSelectFromSet(m_queue_set, portMAX_DELAY);

            if (activated == m_queue_critical) {
                network_item_variant critical;
                while (xQueueReceive(m_queue_critical, &critical, 0) == pdTRUE) {
                    dispatch(critical);
                }
            } else if (activated == m_queue_info) {
                network_item_variant info;
                if (xQueueReceive(m_queue_info, &info, 0) == pdTRUE) {
                    dispatch(info);
                }
            }

        }

    private:
        void dispatch(network_item_variant& item) {
            std::visit([](auto&& obj) {
                std::array<uint8_t, 1024> buf{};
                uint32_t length = 0;
                if (obj.serialize(buf,length))
                    printf("transmit %ld bytes\n", length);
            }, item);
        }
        void drain() {
            //TODO) IMPL. drain.
        }

        QueueHandle_t m_queue_critical;
        QueueHandle_t m_queue_info;
        QueueSetHandle_t m_queue_set;
        static constexpr int QUEUE_CRITICAL_LENGTH = 16;
        static constexpr int QUEUE_INFO_LENGTH = 16;
        static constexpr int ENQUEUE_WAIT_TICK = pdMS_TO_TICKS(100);
    };


}


#endif //FIRMWARE_NETWORKQUEUE_HPP
