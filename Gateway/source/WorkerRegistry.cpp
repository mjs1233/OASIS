//
// Created by tgian on 26. 7. 30..
//

#include "WorkerRegistry.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>


Worker::Worker(std::string json) {
    const auto data = nlohmann::json::parse(json);

    id = data.at("id").get<uint32_t>();
    name = data.at("name").get<std::string>();
    age = data.at("age").get<int32_t>();
    weight = data.at("weight").get<float>();
    height = data.at("height").get<float>();
    pr3 = data.at("pr3").get<float>();
    pr4 = data.at("pr4").get<float>();
    pr7 = data.at("pr7").get<float>();
}

Worker::Worker(std::string name, const int32_t age, const float weight, const float height)
: id(0), name(std::move(name)), age(age), weight(weight), height(height),
last_seen(std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()) {}

std::string Worker::json() const {
    return nlohmann::json{
        {"id", id},
        {"name", name},
        {"age", age},
        {"weight", weight},
        {"height", height},
        {"pr3", pr3},
        {"pr4", pr4},
        {"pr7", pr7},
    }.dump();
}

std::string Worker::status_json() const {
    return nlohmann::json{
        {"id", id},
        {"name", name},
        {"age", age},
        {"weight", weight},
        {"height", height},
        {"pr3", pr3},
        {"pr4", pr4},
        {"pr7", pr7},
        {"core_temp", core_temp},
        {"ttd", ttd},
        {"stage", stage},
        {"last_seen", last_seen},
        {"raw_temp", raw_temp},
        {"raw_hum", raw_hum},
        {"raw_bpm", raw_bpm},
        {"battery", battery},
    }.dump();
}

WorkerRegistry::WorkerRegistry() = default;

WorkerRegistry::WorkerRegistry(const std::filesystem::path& worker_registry_path) {
    load(worker_registry_path);
}

Worker* WorkerRegistry::get(const uint32_t worker_id) noexcept {
    if (worker_id >= m_workers.size() || !m_workers[worker_id].has_value()) {
        return nullptr;
    }

    return &m_workers[worker_id].value();
}

const Worker* WorkerRegistry::get(const uint32_t worker_id) const noexcept {
    if (worker_id >= m_workers.size() || !m_workers[worker_id].has_value()) {
        return nullptr;
    }

    return &m_workers[worker_id].value();
}

uint32_t WorkerRegistry::add(Worker worker) {
    if (m_workers.size() > std::numeric_limits<uint32_t>::max()) {
        throw std::overflow_error("worker ID limit exceeded");
    }

    const auto worker_id = static_cast<uint32_t>(m_workers.size());
    worker.id = worker_id;
    m_workers.emplace_back(std::move(worker));
    return worker_id;
}

uint32_t WorkerRegistry::add(const std::string& worker_json) {
    return add(Worker{worker_json});
}

bool WorkerRegistry::remove(const uint32_t worker_id) noexcept {
    if (worker_id >= m_workers.size() || !m_workers[worker_id].has_value()) {
        return false;
    }

    m_workers[worker_id].reset();
    return true;
}

void WorkerRegistry::status_json(nlohmann::json& json_) const {
    nlohmann::json workers = nlohmann::json::array();

    for (const auto& worker : m_workers) {
        if (worker.has_value()) {
            workers.push_back(nlohmann::json::parse(worker->status_json()));
        }
    }

    json_["datacount"] = m_workers.size();
    json_["workers"] = std::move(workers);

}

void WorkerRegistry::save() const {
    if (m_worker_data_path.empty()) {
        throw std::runtime_error("worker registry file path is not configured");
    }

    nlohmann::json workers = nlohmann::json::array();
    for (const auto& worker : m_workers) {
        if (worker.has_value()) {
            workers.push_back(nlohmann::json::parse(worker->json()));
        } else {
            // Preserve the index of deleted workers across a restart.
            workers.push_back(nullptr);
        }
    }

    std::ofstream file(m_worker_data_path, std::ios::out | std::ios::trunc);
    if (!file) {
        throw std::runtime_error("cannot write worker registry file: " + m_worker_data_path.string());
    }

    file << workers.dump(4) << '\n';
    if (!file) {
        throw std::runtime_error("failed to write worker registry file: " + m_worker_data_path.string());
    }
}

void WorkerRegistry::load(const std::filesystem::path& worker_registry_path) {
    m_worker_data_path = worker_registry_path;

    if (!std::filesystem::exists(m_worker_data_path)) {
        if (m_worker_data_path.has_parent_path()) {
            std::filesystem::create_directories(m_worker_data_path.parent_path());
        }
        save();
        return;
    }

    std::ifstream file(worker_registry_path);
    if (!file) {
        throw std::runtime_error("cannot open worker registry file: " + worker_registry_path.string());
    }

    file >> std::ws;
    if (file.peek() == std::char_traits<char>::eof()) {
        file.close();
        save();
        return;
    }

    const auto workers = nlohmann::json::parse(file);
    if (!workers.is_array()) {
        throw std::runtime_error("worker registry file must contain a JSON array");
    }

    m_workers.reserve(workers.size());
    for (const auto& worker_json : workers) {
        if (worker_json.is_null()) {
            if (m_workers.size() > std::numeric_limits<uint32_t>::max()) {
                throw std::overflow_error("worker ID limit exceeded");
            }
            m_workers.emplace_back(std::nullopt);
            continue;
        }
        add(Worker{worker_json.dump()});
    }
}
