//
// Created by tgian on 26. 7. 30..
//

#include "Server.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include "Log.hpp"
#include "Parser.hpp"

namespace oasis {
    Server::Server(const ServerConfig& config)
        : m_config(config), m_workers(m_config.worker_data_path) {
        Logger::instance().write_info("worker_registry",
            "loaded worker data from: " + m_config.worker_data_path);
        init();
    }

    void Server::start() {
        Logger::instance().write_info("server",
            "listening on " + m_config.host + ":" + std::to_string(m_config.port));
        m_server.listen(m_config.host, m_config.port);
    }

    void Server::init() {



        m_server.Options(".*", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
        res.status = 204; // No Content
        });

        m_server.set_post_routing_handler([](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Credentials", "true");
        });


        const auto bad_request = [this](httplib::Response& res,
                                        const std::string_view endpoint,
                                        const std::string_view message) {
            if (m_config.debug_log_enabled) {
                Logger::instance().write_warn(endpoint, message);
            }
            res.status = 400;
        };

        m_server.Get("/",
            [](const httplib::Request&, httplib::Response& res) {
                res.status = 200;
                res.set_content("I'm here\n", "text/plain");
            });

        m_server.Get("/worker_status",
            [this](const httplib::Request&, httplib::Response& res) {
                std::scoped_lock lock(m_worker_mutex);
                res.status = 200;
                res.set_content(m_workers.status_json(), "application/json");
            });

        m_server.Get("/add_worker",
            [this, bad_request](const httplib::Request& req, httplib::Response& res) {
                std::string name;
                int age;
                float weight;
                float height;

                // A Worker cannot be created with a partial persistent profile.
                if (!get_request(req, "name", name)
                    || !get_request(req, "age", age)
                    || !get_request(req, "weight", weight)
                    || !get_request(req, "height", height)) {
                    bad_request(res, "/add_worker", "missing or invalid worker initialization parameter");
                    return;
                }

                std::scoped_lock lock(m_worker_mutex);
                uint32_t worker_id;
                try {
                    worker_id = m_workers.add(
                        Worker{std::move(name), age, weight, height});
                    m_workers.save();
                } catch (const std::exception& exception) {
                    Logger::instance().write_error("/add_worker", exception.what());
                    res.status = 500;
                    return;
                }
                Logger::instance().write_info("/add_worker",
                    "worker_id : " + std::to_string(worker_id) + " saved");
                res.status = 200;
                res.set_content(std::to_string(worker_id), "text/plain");
            });

        m_server.Get("/set_worker_info",
            [this, bad_request](const httplib::Request& req, httplib::Response& res) {
                const std::string worker_id_text = req.get_param_value("worker_id");
                if (worker_id_text.empty()) {
                    bad_request(res, "/set_worker_info", "missing worker_id");
                    return;
                }

                uint32_t worker_id;
                try {
                    std::size_t parsed_length = 0;
                    const auto parsed_id = std::stoull(worker_id_text, &parsed_length);
                    if (parsed_length != worker_id_text.size()
                        || parsed_id > std::numeric_limits<uint32_t>::max()) {
                        bad_request(res, "/set_worker_info", "invalid worker_id");
                        return;
                    }
                    worker_id = static_cast<uint32_t>(parsed_id);
                } catch (...) {
                    bad_request(res, "/set_worker_info", "invalid worker_id");
                    return;
                }

                std::scoped_lock lock(m_worker_mutex);
                Worker* worker = m_workers.get(worker_id);
                if (worker == nullptr) {
                    res.status = 404;
                    return;
                }

                std::string name;
                float value;
                int age;
                if (get_request(req, "name", name)) {
                    worker->name = std::move(name);
                }
                if (get_request(req, "age", age)) {
                    worker->age = age;
                }
                if (get_request(req, "weight", value)) {
                    worker->weight = value;
                }
                if (get_request(req, "height", value)) {
                    worker->height = value;
                }

                try {
                    m_workers.save();
                } catch (const std::exception& exception) {
                    Logger::instance().write_error("/set_worker_info", exception.what());
                    res.status = 500;
                    return;
                }
                Logger::instance().write_info("/set_worker_info",
                    "worker_id : " + worker_id_text + " saved");
                res.status = 200;
            });

        m_server.Post("/dev_signal",
            [](const httplib::Request&, httplib::Response&) {
                // Device signal.
            });
    }
}
