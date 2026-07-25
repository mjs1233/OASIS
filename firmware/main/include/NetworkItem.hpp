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
        struct null_item {
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
    }

    using network_item_variant = std::variant<network_item::null_item>;
}

#endif //FIRMWARE_NETWORKITEM_HPP
