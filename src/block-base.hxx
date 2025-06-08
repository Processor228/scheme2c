#pragma once

#include <fmt/format.h>
#include <pugixml.hpp>

#include <cctype>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct Port {
    size_t id;
    std::string name_of_connector;
};

struct Block {
    std::string m_name;
    size_t m_sid;

    // others may connect to
    std::vector<Port> ports;
public:
    virtual ~Block() = default;

    static std::shared_ptr<Block> withEnoughInfoAboutType(
        std::string_view type,
        const pugi::xml_node& elem
    );

    Block(std::string name, size_t sid) : m_name(std::move(name)), m_sid(sid)
    {}

    virtual std::string exportSignature(std::string package_name) {
        return "";
    }

    virtual std::string evaluationCode(std::string package_name) {
        return "";
    }

    virtual std::string requiredSetupCode(std::string package_name) {
        return "";
    }

};
