//
// Created by tgian on 26. 7. 29..
//

#ifndef GATEWAY_SERVER_CONFIG_HPP
#define GATEWAY_SERVER_CONFIG_HPP
#include <filesystem>
#include <string>
namespace oasis {

    struct ServerConfig {
        std::string host;
        int port;
        std::string worker_data_path;
        bool debug_log_enabled;
        std::string log_path;

        bool read_config(std::filesystem::path _path);
    };
}
#endif //GATEWAY_SERVER_CONFIG_HPP
