//
// Created by tgian on 26. 7. 29..
//

#ifndef GATEWAY_LOG_HPP
#define GATEWAY_LOG_HPP
#include <filesystem>
#include <string>
#include <iostream>
#include <fstream>
#include <string_view>
#include <mutex>
#include <chrono>
#include <print>  // C++23 std::print, std::println
#include <format> // C++20 std::format
namespace oasis {
    class Logger {
    public:
        explicit Logger(std::filesystem::path log_path) : m_log_path(std::move(log_path)) {
            if (m_log_path.has_parent_path()) {
                std::filesystem::create_directories(m_log_path.parent_path());
            }

            m_file_stream.open(m_log_path, std::ios::out | std::ios::app);
            if (!m_file_stream.is_open()) {
                std::println(stderr, "[Logger Error] Failed to open log file: {}", m_log_path.string());
            }
        }

        ~Logger() {
            if (m_file_stream.is_open()) {
                m_file_stream.flush();
                m_file_stream.close();
            }
        }

        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        void write_info(std::string_view tag, std::string_view msg) {
            write_log("INFO", tag, msg);
        }

        void write_warn(std::string_view tag, std::string_view msg) {
            write_log("WARN", tag, msg);
        }

        void write_error(std::string_view tag, std::string_view msg) {
            write_log("ERROR", tag, msg);
        }

        void set_stdout(bool use_stdout) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_use_stdout = use_stdout;
        }

        static void create(const std::filesystem::path& log_path) {
            if (logger == nullptr) {
                logger = new Logger(log_path);
            }
        }

        static Logger& instance() {
            return *logger;
        }

        static void terminate() {
            delete logger;
            logger = nullptr;
        }

    private:
        void write_log(std::string_view level, std::string_view tag, std::string_view msg) {

            auto now = std::chrono::system_clock::now();

            std::string formatted_entry = std::format(
                "[{:%Y-%m-%d %H:%M:%S}] [{}] [{}] {}",
                std::chrono::floor<std::chrono::milliseconds>(now),
                level, tag, msg
            );

            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_file_stream.is_open()) {
                std::println(m_file_stream, "{}", formatted_entry);

                if (level == "ERROR") {
                    m_file_stream.flush();
                }
            }

            if (m_use_stdout) {
                std::println("{}", formatted_entry);
            }
        }

        inline static Logger* logger = nullptr;

        std::mutex m_mutex;
        std::ofstream m_file_stream;
        std::filesystem::path m_log_path;
        bool m_use_stdout = true;
    };
}

#endif //GATEWAY_LOG_HPP
