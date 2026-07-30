//
// Created by tgian on 26. 7. 30..
//
#include "Parser.hpp"

namespace oasis {
    bool get_request(const httplib::Request& req, const std::string& key, std::string& result) {
        std::string value = req.get_param_value(key);
        if (value.empty()) {
            result = {};
            return false;
        }
        result = value;
        return true;
    }

    bool get_request(const httplib::Request& req, const std::string& key, int& result) {
        std::string value = req.get_param_value(key);
        if (value.empty()) {
            result = {};
            return false;
        }
        try {
            result = std::stoi(value);
        }
        catch (...) {
            result = {};
            return false;
        }
        return true;
    }

    bool get_request(const httplib::Request& req, const std::string& key, float& result) {
        std::string value = req.get_param_value(key);
        if (value.empty()) {
            result = {};
            return false;
        }
        try {
            result = std::stof(value);
        }
        catch (...) {
            result = {};
            return false;
        }
        return true;
    }
}