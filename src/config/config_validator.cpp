#include <string>

#include <fmt/format.h>

#include "config/config_validator.hpp"

namespace config {

std::string Validate(const Config &config) {
    if (config.title.empty()) {
        return "title must not be empty";
    }

    if (config.divide.b == 0 && config.divide.a != 0) {
        return fmt::format("divide: division by zero (a={}, b=0)", config.divide.a);
    }

    return {};
}

} // namespace config
