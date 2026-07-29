#include <iostream>
#include <cmath>
#include <filesystem>
#include <fstream>

#include "httplib.h"

int main() {

    httplib::Server server;
    server.Get("/",
        [](const httplib::Request& req, httplib::Response& res) {

        res.status = 200;
        res.set_content("I'm here\n", "text/plain");

    });
    server.Post("/signal", [](const httplib::Request& req,httplib::Response& res) {

    });

    server.listen("0.0.0.0", 8080);

    return 0;
}
