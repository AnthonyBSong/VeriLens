#pragma once
#include <string>

struct Parameter {
    std::string name;
    std::string default_value;  // raw text, empty if no default

    Parameter(const std::string& name, const std::string& default_value = "")
        : name(name), default_value(default_value) {}
};
