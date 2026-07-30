//
// Created by tgian on 26. 7. 30..
//

#ifndef GATEWAY_PARSER_HPP
#define GATEWAY_PARSER_HPP
#include "httplib.h"
#include <string>
namespace oasis {
    bool get_request(const httplib::Request& req, const std::string& key, std::string& result);
    bool get_request(const httplib::Request& req, const std::string& key, int& result);
    bool get_request(const httplib::Request& req, const std::string& key, float& result);
}
#endif //GATEWAY_PARSER_HPP
