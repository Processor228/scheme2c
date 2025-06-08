#pragma once

#include <fmt/format.h>
#include <pugixml.hpp>

#include <cctype>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "block-base.hxx"

using namespace std::string_literals;

struct BlockSocket {
    size_t SID;
    size_t port_ID;

    static BlockSocket parseSocket(std::string s) {
        size_t hash_pos = s.find('#');
        if (hash_pos == std::string::npos) throw std::runtime_error("Invalid Src format: missing #");

        size_t colon_pos = s.find(':', hash_pos);
        if (colon_pos == std::string::npos) throw std::runtime_error("Invalid Src format: missing :");

        return {
            static_cast<size_t>(std::stoul(s.substr(0, hash_pos))),
            static_cast<size_t>(std::stoul(s.substr(colon_pos + 1))) - 1
        };
    }
};

class Inport : public Block {
public:
    static std::shared_ptr<Inport> parseFromXmlNode(
        const pugi::xml_node& inport,
        std::string_view name,
        size_t sid
    ) {
        for (auto port_node : inport.select_nodes("./Port")) {
            Port port;

            auto number_node = port_node.node().select_node("./P[@Name='PortNumber']");
            if (number_node) {
                port.id = number_node.node().text().as_ullong();
            }
        }

        return std::make_shared<Inport>(std::string(name), sid);
    }

    virtual std::string exportSignature(std::string package) override {
        return fmt::format(R"({{ "{}", &{}.{}, 1 }},)" "\n", m_name, package, m_name);
    }

    Inport(std::string name, size_t sid)
        : Block(std::move(name), std::move(sid)) {
            ports.push_back({1, ""});
        }

    std::unordered_map<int, std::string> m_ports;
};

class Sum : public Block {
public:
    enum class Op { Plus, Minus };

    static std::shared_ptr<Sum> parseFromXmlNode(
        const pugi::xml_node& elem,
        std::string_view name,
        size_t sid
    ) {
        std::pair<Op, Op> operations;
        auto inputs_node = elem.select_node("./P[@Name='Inputs']");
        if (inputs_node) {
            std::string inputs = inputs_node.node().text().as_string();
            for (char op : inputs) {
                switch (op) {
                    case '+': operations.first = Op::Plus; break;
                    case '-': operations.second = Op::Minus; break;
                    default:
                        throw std::runtime_error("Unknown sum operation: " + std::string(1, op));
                }
            }
        }

        return std::make_shared<Sum>(std::string(name), sid, operations);
    }

    Sum(std::string name, size_t sid, std::pair<Op, Op> operations)
        : Block(std::move(name), sid), m_operations(std::move(operations)) {
            ports.push_back({1, ""});
            ports.push_back({2, ""});
        }

    virtual std::string evaluationCode(std::string package_name) {
        std::string sign_first = (m_operations.first == Op::Minus ? " - " : + "");
        std::string sign_scd = (m_operations.second == Op::Minus ? " - " : + " + ");

        return package_name + "." + m_name + " = " + sign_first + package_name + "." + ports[0].name_of_connector
            + sign_scd + package_name + "." + ports[1].name_of_connector + ";\n";
    }

    const std::pair<Op, Op>& getOperations() const { return m_operations; }

private:
    std::pair<Op, Op> m_operations;
};

class Gain : public Block {
public:
    static std::shared_ptr<Gain> parseFromXmlNode(
        const pugi::xml_node& elem,
        std::string_view name,
        size_t sid
    ) {
        double gain_value = 1.0;

        auto gain_node = elem.select_node("./P[@Name='Gain']");
        if (gain_node) {
            gain_value = gain_node.node().text().as_double();
        }

        return std::make_shared<Gain>(name, sid, gain_value);
    }

    Gain(std::string_view name, size_t sid, double gain)
        : Block(std::string(name), sid), gain(gain) {
            ports.push_back({1, ""});
        }

    virtual std::string evaluationCode(std::string package_name) {
        return package_name + "." + m_name + " = " + package_name + "." + ports[0].name_of_connector + " * " + std::to_string(gain) + ";\n";
    }

    double gain;
};

class UnitDelay : public Block {
public:
    static std::shared_ptr<UnitDelay> parseFromXmlNode(
        const pugi::xml_node& elem,
        std::string_view name,
        size_t sid
    ) {
        double sample_time = -1.0;

        auto st_node = elem.select_node("./P[@Name='SampleTime']");
        if (st_node) {
            sample_time = st_node.node().text().as_double();
        }

        return std::make_shared<UnitDelay>(name, sid, sample_time);
    }

    UnitDelay(std::string_view name, size_t sid, double sample_time)
        : Block(std::string(name), sid), sample_time(sample_time) {
            ports.push_back({1, ""});
        }

    virtual std::string requiredSetupCode(std::string package_name) {
        return package_name + "." + m_name + " = 0;";
    }

    virtual std::string evaluationCode(std::string package_name) {
        return package_name + "." + m_name + " = " + package_name + "." + ports[0].name_of_connector + ";\n";
    }

    double sample_time;
};

class Outport : public Block {
public:
    static std::shared_ptr<Outport> parseFromXmlNode(
        const pugi::xml_node& elem,
        std::string_view name,
        size_t sid
    ) {
        return std::make_shared<Outport>(name, sid);
    }

    virtual std::string exportSignature(std::string package) override {
        return fmt::format(R"({{ "{}", &{}.{}, 0 }},)" "\n", m_name, package, ports[0].name_of_connector);
    }

    Outport(std::string_view name, size_t sid)
        : Block(std::string(name), sid) {
            ports.push_back({1, ""});
        }
};
