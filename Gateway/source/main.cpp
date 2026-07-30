#include <iostream>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include "Log.hpp"
#include "httplib.h"
#include "Server.hpp"

int main(int argc, char** argv) {

    const char* default_server_config_path = "./server_config.json";
    std::filesystem::path server_config_path;
    if (argc < 2) {
       server_config_path = default_server_config_path;
    }
    else
        server_config_path = argv[1];

    if(!std::filesystem::exists(server_config_path)) {
        std::print("cannot find server config file\n");
        return 0;
    }
    oasis::ServerConfig server_config;
    if (!server_config.read_config(server_config_path)) {
        std::print("cannot read server config file\n");
        return 0;
    }

    oasis::Logger::create(server_config.log_path);
    oasis::Logger::instance().set_stdout(true);
    oasis::Logger::instance().write_info("global", "init start");

    try {
        oasis::Server server(server_config);
        server.start();
    } catch (const std::exception& exception) {
        oasis::Logger::instance().write_error("global",
            "server startup failed: " + std::string(exception.what()));
        return 1;
    }
    return 0;
}
