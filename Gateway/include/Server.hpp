//
// Created by tgian on 26. 7. 29..
//

#ifndef GATEWAY_SERVER_HPP
#define GATEWAY_SERVER_HPP

#include <mutex>

#include "ServerConfig.hpp"
#include "WorkerRegistry.hpp"
#include "httplib.h"

namespace oasis {
    class Server {
    public:
        explicit Server(const ServerConfig& config);
        void start();

    private:
        void init();

        ServerConfig m_config;
        httplib::Server m_server;
        WorkerRegistry m_workers;
        std::mutex m_worker_mutex;
    };
}

#endif //GATEWAY_SERVER_HPP
