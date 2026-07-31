//
// Created by tgian on 26. 7. 30..
//

#ifndef GATEWAY_WORKERREGISTRY_HPP
#define GATEWAY_WORKERREGISTRY_HPP
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include "nlohmann/json.hpp"

struct Worker {

    uint32_t id;
    std::string name;
    int32_t age;
    float weight;
    float height;
    float pr3 = 0.0f;
    float pr4 = 0.0f;
    float pr7 = 0.0f;

    // Runtime-only values. These are deliberately excluded from json().
    float core_temp = 0.0f;
    float ttd = 0.0f;
    int32_t stage = 0;
    int64_t last_seen = 0;
    float raw_temp = 0.0f;
    float raw_hum = 0.0f;
    float raw_bpm = 0.0f;
    float battery = 0.0f;

    Worker(std::string json);
    Worker(std::string name, int32_t age, float weight, float height);
    std::string json() const;
    std::string status_json() const;
};


class WorkerRegistry {
public:
    WorkerRegistry();
    explicit WorkerRegistry(const std::filesystem::path& worker_registry_path);

    Worker* get(uint32_t worker_id) noexcept;
    const Worker* get(uint32_t worker_id) const noexcept;

    uint32_t add(Worker worker);
    uint32_t add(const std::string& worker_json);
    bool remove(uint32_t worker_id) noexcept;
    void status_json(nlohmann::json& json_) const;
    void save() const;

private:
    void load(const std::filesystem::path& worker_registry_path);

    // A removed worker remains an empty slot, so its index is never reused.
    std::vector<std::optional<Worker>> m_workers;
    std::filesystem::path m_worker_data_path;

};



#endif //GATEWAY_WORKERREGISTRY_HPP
