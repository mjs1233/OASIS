//
// Created by tgian on 26. 7. 25..
//

#ifndef FIRMWARE_NETWORKITEM_HPP
#define FIRMWARE_NETWORKITEM_HPP
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
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

    // Concept for items produced by parsing an inbound buffer.
    template <typename T>
    concept network_item_recv_trait = requires (T item, std::array<uint8_t,1024>& arr, uint32_t& length)
    {
        T::type;
        typename std::bool_constant<(T::type, true)>;
        { item.deserialize(arr, length) } -> std::same_as<bool>;
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
                length = 5;
                return true;
            }
        };

        struct  __attribute__((packed)) device_init {
            static constexpr NetworkItemType type = NetworkItemType::CRITICAL;

            uint8_t magic[2] = {0x89, 0x5E};
            uint8_t MAC[6] = {0,};

            bool serialize(std::array<uint8_t,1024>& arr,uint32_t& length) {
                // struct is packed, so its byte layout matches the wire format exactly (no padding)
                std::memcpy(arr.data(), this, sizeof(device_init));
                length = sizeof(device_init);
                return true;
            }
        };

        struct __attribute__((packed)) device_init_recv {
            static constexpr NetworkItemType type = NetworkItemType::CRITICAL;

            uint8_t magic[2] = {0x96, 0xBD};
            uint8_t worker_age = 0;
            uint8_t worker_weight = 0;

            bool deserialize(std::array<uint8_t,1024>& arr,uint32_t& length) {
                if (length != sizeof(device_init_recv)) {
                    return false;
                }
                if (arr[0] != magic[0] || arr[1] != magic[1]) {
                    return false;
                }
                std::memcpy(this, arr.data(), sizeof(device_init_recv));
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
                std::memcpy(arr.data(), this, sizeof(worker_data));
                length = sizeof(worker_data);
                return true;
            }
        };

        struct __attribute__((packed)) worker_data_recv {
            static constexpr NetworkItemType type = NetworkItemType::CRITICAL;

            uint8_t magic[2] = {0x89, 0xD8};
            uint8_t ttd = 0;
            bool deserialize(std::array<uint8_t,1024>& arr,uint32_t& length) {
                if (length != sizeof(worker_data_recv)) {
                    return false;
                }
                if (arr[0] != magic[0] || arr[1] != magic[1]) {
                    return false;
                }
                std::memcpy(this, arr.data(), sizeof(worker_data_recv));
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

    // Variant of items that can result from parsing an inbound buffer.
    using network_item_recv_variant =
        std::variant<
        network_item::device_init_recv,
        network_item::worker_data_recv
    >;

    // Serializes whichever alternative is currently held by item.
    inline bool serialize_network_item(network_item_variant& item, std::array<uint8_t,1024>& arr, uint32_t& length) {
        return std::visit([&arr, &length](auto& concrete_item) {
            return concrete_item.serialize(arr, length);
        }, item);
    }

    // Reads the 2-byte magic at the front of arr and dispatches to the matching recv item.
    // Returns std::nullopt if no known item matches the magic, or the length does not fit.
    inline std::optional<network_item_recv_variant> deserialize_network_item(std::array<uint8_t,1024>& arr, uint32_t& length) {
        if (length < 2) {
            return std::nullopt;
        }

        network_item::device_init_recv device_init_item{};
        if (arr[0] == device_init_item.magic[0] && arr[1] == device_init_item.magic[1]) {
            if (device_init_item.deserialize(arr, length)) {
                return device_init_item;
            }
            return std::nullopt;
        }

        network_item::worker_data_recv worker_data_item{};
        if (arr[0] == worker_data_item.magic[0] && arr[1] == worker_data_item.magic[1]) {
            if (worker_data_item.deserialize(arr, length)) {
                return worker_data_item;
            }
            return std::nullopt;
        }

        return std::nullopt;
    }

}

#endif //FIRMWARE_NETWORKITEM_HPP
