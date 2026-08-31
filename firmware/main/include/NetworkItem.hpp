#ifndef FIRMWARE_NETWORKITEM_HPP
#define FIRMWARE_NETWORKITEM_HPP

#include <array>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>
#include <variant>

namespace oasis {
    constexpr size_t NETWORK_QUERY_BUFFER_SIZE = 256;
    using network_query_buffer = std::array<char, NETWORK_QUERY_BUFFER_SIZE>;

    enum class NetworkItemType {
        INFO,
        CRITICAL
    };

    namespace network_item::detail {
        class QueryWriter {
        public:
            explicit QueryWriter(network_query_buffer& output) : m_output(output) {
                m_output.fill('\0');
            }

            bool field(std::string_view key, std::string_view value) {
                if (!m_first && !append("&")) return false;
                m_first = false;
                return append(key) && append("=") && append(value);
            }

            bool field(std::string_view key, uint8_t value) {
                std::array<char, 3> digits {};
                const auto result = std::to_chars(digits.data(), digits.data() + digits.size(), value);
                return result.ec == std::errc{} &&
                       field(key, {digits.data(), static_cast<size_t>(result.ptr - digits.data())});
            }

            bool finish(size_t& length) {
                if (m_length >= m_output.size()) return false;
                m_output[m_length] = '\0';
                length = m_length;
                return true;
            }

        private:
            bool append(std::string_view value) {
                if (value.size() > m_output.size() - m_length - 1) return false;
                for (const char character : value) m_output[m_length++] = character;
                return true;
            }

            network_query_buffer& m_output;
            size_t m_length = 0;
            bool m_first = true;
        };

        inline std::string_view query_part(std::string_view input) {
            if (const size_t question = input.find('?'); question != std::string_view::npos) {
                input.remove_prefix(question + 1);
            } else if (!input.empty() && input.front() == '?') {
                input.remove_prefix(1);
            }
            while (!input.empty() && (input.back() == '\r' || input.back() == '\n' || input.back() == ' ')) {
                input.remove_suffix(1);
            }
            return input;
        }

        inline std::optional<std::string_view> value_of(std::string_view input, std::string_view key) {
            input = query_part(input);
            while (!input.empty()) {
                const size_t separator = input.find('&');
                const std::string_view field = input.substr(0, separator);
                const size_t equals = field.find('=');
                if (equals != std::string_view::npos && field.substr(0, equals) == key) {
                    return field.substr(equals + 1);
                }
                if (separator == std::string_view::npos) break;
                input.remove_prefix(separator + 1);
            }
            return std::nullopt;
        }

        inline bool parse_u8(std::string_view text, uint8_t& value) {
            unsigned int parsed = 0;
            const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
            if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || parsed > 255) {
                return false;
            }
            value = static_cast<uint8_t>(parsed);
            return true;
        }
    }

    template <typename T>
    concept network_item_trait = std::is_copy_constructible_v<T> &&
        requires (const T item, network_query_buffer& output, size_t& length) {
            { T::type } -> std::convertible_to<NetworkItemType>;
            { item.serialize(output, length) } -> std::same_as<bool>;
        };

    template <typename T>
    concept network_item_recv_trait = std::is_copy_constructible_v<T> &&
        requires (T item, std::string_view query) {
            { item.deserialize(query) } -> std::same_as<bool>;
        };

    namespace network_item {
        struct null {
            static constexpr NetworkItemType type = NetworkItemType::INFO;

            bool serialize(network_query_buffer& output, size_t& length) const {
                detail::QueryWriter writer(output);
                return writer.field("type", "null") && writer.finish(length);
            }
        };

        struct device_init {
            static constexpr NetworkItemType type = NetworkItemType::CRITICAL;
            std::array<uint8_t, 6> mac {};

            bool serialize(network_query_buffer& output, size_t& length) const {
                static constexpr char HEX[] = "0123456789ABCDEF";
                std::array<char, 12> encoded_mac {};
                for (size_t i = 0; i < mac.size(); ++i) {
                    encoded_mac[i * 2] = HEX[mac[i] >> 4];
                    encoded_mac[i * 2 + 1] = HEX[mac[i] & 0x0F];
                }

                detail::QueryWriter writer(output);
                return writer.field("type", "device_init") &&
                       writer.field("mac", std::string_view(encoded_mac.data(), encoded_mac.size())) &&
                       writer.finish(length);
            }
        };

        struct device_init_recv {
            uint8_t worker_age = 0;
            uint8_t worker_weight = 0;

            bool deserialize(std::string_view query) {
                const auto age = detail::value_of(query, "worker_age");
                const auto weight = detail::value_of(query, "worker_weight");
                return age && weight && detail::parse_u8(*age, worker_age) &&
                       detail::parse_u8(*weight, worker_weight);
            }
        };

        struct worker_data {
            static constexpr NetworkItemType type = NetworkItemType::CRITICAL;
            uint8_t raw_temp = 0;
            uint8_t raw_hum = 0;
            uint8_t raw_bpm = 0;
            uint8_t battery_voltage = 0;

            bool serialize(network_query_buffer& output, size_t& length) const {
                detail::QueryWriter writer(output);
                return writer.field("type", "worker_data") &&
                       writer.field("temp", raw_temp) &&
                       writer.field("hum", raw_hum) &&
                       writer.field("bpm", raw_bpm) &&
                       writer.field("battery", battery_voltage) &&
                       writer.finish(length);
            }
        };

        struct worker_data_recv {
            uint8_t ttd = 0;

            bool deserialize(std::string_view query) {
                const auto value = detail::value_of(query, "ttd");
                return value && detail::parse_u8(*value, ttd);
            }
        };
    }

    using network_item_variant = std::variant<
        network_item::null,
        network_item::device_init,
        network_item::worker_data
    >;

    using network_item_recv_variant = std::variant<
        network_item::device_init_recv,
        network_item::worker_data_recv
    >;

    inline bool serialize_network_item(const network_item_variant& item,
                                       network_query_buffer& output,
                                       size_t& length) {
        return std::visit([&output, &length](const auto& concrete_item) {
            return concrete_item.serialize(output, length);
        }, item);
    }

    inline std::optional<network_item_recv_variant> deserialize_network_item(std::string_view query) {
        const auto type = network_item::detail::value_of(query, "type");
        if (!type) return std::nullopt;

        if (*type == "device_init") {
            network_item::device_init_recv item {};
            if (item.deserialize(query)) return item;
        } else if (*type == "worker_data") {
            network_item::worker_data_recv item {};
            if (item.deserialize(query)) return item;
        }
        return std::nullopt;
    }
}

#endif // FIRMWARE_NETWORKITEM_HPP
