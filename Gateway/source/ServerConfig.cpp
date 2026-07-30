//
// Created by tgian on 26. 7. 29..
//
#include <fstream>

#include "ServerConfig.hpp"

#include <iostream>

#include "nlohmann/json.hpp"

namespace oasis {
    bool ServerConfig::read_config(std::filesystem::path _path) {
        std::ifstream file(_path, std::ios::in);
        try {
            nlohmann::json json = nlohmann::json::parse(file);
            host = json.at("host").get<std::string>();
            port = json.at("port").get<int>();
            worker_data_path = json.at("data_path").get<std::string>();
            debug_log_enabled = json.at("debug").get<bool>();
            log_path = json.at("log_path").get<std::string>();
        }
        catch (std::exception& e) {
            std::cout << e.what() << '\n';
            file.close();
            return false;
        }
        file.close();
        return true;
    }
}
