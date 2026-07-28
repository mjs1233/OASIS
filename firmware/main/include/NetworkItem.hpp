//
// Created by tgian on 26. 7. 25..
//

#ifndef FIRMWARE_NETWORKITEM_HPP
#define FIRMWARE_NETWORKITEM_HPP
#include <variant>
namespace oasis {
    enum class NetworkItemType {
        INFO,
        CRITICAL
    };

    template <typename T>
    concept network_item_trait = requires (T item, std::array<uint8_t,1024>& arr, uint32_t& length)
    {
        T::type;
        typename std::bool_constant<(T::type, true)>;
        { item.serialize(arr, length) } -> std::same_as<bool>;
    }
    && std::is_copy_assignable_v<T>;

    namespace network_item {

        //example item
        struct __attribute__((packed)) null {
            static constexpr NetworkItemType type = NetworkItemType::INFO;
            bool serialize(std::array<uint8_t,1024>& arr,uint32_t& length) {
                arr[0] = 'N';
                arr[1] = 'U';
                arr[2] = 'L';
                arr[3] = 'L';
                arr[4] = '\0';
                return true;
            }
        };

        struct  __attribute__((packed)) device_init {
            static constexpr NetworkItemType type = NetworkItemType::CRITICAL;

            uint8_t magic[2] = {0x89, 0x5E};
            uint8_t MAC[6] = {0,};

            bool serialize(std::array<uint8_t,1024>& arr,uint32_t& length) {
                return true;
            }
        };

        struct __attribute__((packed)) device_init_recv {
            static constexpr NetworkItemType type = NetworkItemType::CRITICAL;

            uint8_t magic[2] = {0x96, 0xBD};
            uint8_t worker_age = 0;
            uint8_t worker_weight = 0;

            bool deserialize(std::array<uint8_t,1024>& arr,uint32_t& length) {
                return true;
            }
        };

        struct __attribute__((packed)) worker_data {
            static constexpr NetworkItemType type = NetworkItemType::CRITICAL;

            uint8_t magic[2] = {0x82, 0xCC};
            uint8_t raw_temp = 0;
            uint8_t raw_hum = 0;
            uint8_t raw_bpm = 0;
            uint8_t battery_voltage = 0;

            bool serialize(std::array<uint8_t,1024>& arr,uint32_t& length) {
                return true;
            }
        };

        struct __attribute__((packed)) worker_data_recv {
            static constexpr NetworkItemType type = NetworkItemType::CRITICAL;

            uint8_t magic[2] = {0x89, 0xD8};
            uint8_t ttd = 0;
            bool deserialize(std::array<uint8_t,1024>& arr,uint32_t& length) {
                return true;
            }
        };
    }

    using network_item_variant =
        std::variant<
        network_item::null,
        network_item::device_init,
        network_item::worker_data
    >;

}

#endif //FIRMWARE_NETWORKITEM_HPP
