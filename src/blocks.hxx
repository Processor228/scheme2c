#pragma once

#include <fmt/format.h>
#include <linux/limits.h>
#include <pugixml.hpp>

#include <cctype>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

using namespace std::string_literals;

struct Dependency {

    operator std::string() {
        return name_of_connector;
    }

    std::string as_from(std::string package) const {
        return package + "." + name_of_connector;
    }

  std::string name_of_connector;
};

struct BlockSocket {
  size_t SID;
  size_t port_ID;

  static BlockSocket parseSocket(std::string s) {
    size_t hash_pos = s.find('#');
    if (hash_pos == std::string::npos)
      throw std::runtime_error("Invalid Src format: missing #");

    size_t colon_pos = s.find(':', hash_pos);
    if (colon_pos == std::string::npos)
      throw std::runtime_error("Invalid Src format: missing :");

    return {static_cast<size_t>(std::stoul(s.substr(0, hash_pos))),
            static_cast<size_t>(std::stoul(s.substr(colon_pos + 1))) - 1};
  }
};

class Inport {
public:
  static Inport parseFromXmlNode(const pugi::xml_node &inport,
                                 std::string_view name, size_t sid) {
    return Inport{};
  }
};

class Sum {
public:
  enum class Op { Plus, Minus };

  static Sum parseFromXmlNode(const pugi::xml_node &elem, std::string_view name,
                              size_t sid) {
    std::pair<Op, Op> operations;
    auto inputs_node = elem.select_node("./P[@Name='Inputs']");
    if (inputs_node) {
      std::string inputs = inputs_node.node().text().as_string();
      for (char op : inputs) {
        switch (op) {
        case '+':
          operations.first = Op::Plus;
          break;
        case '-':
          operations.second = Op::Minus;
          break;
        default:
          throw std::runtime_error("Unknown sum operation: " +
                                   std::string(1, op));
        }
      }
    }

    return Sum{operations};
  }

  Sum(std::pair<Op, Op> operations) : m_operations(operations) {}

  Op first() const { return m_operations.first; }

  Op scd() const { return m_operations.second; }

private:
  std::pair<Op, Op> m_operations;
};

class Gain {
public:
  static Gain parseFromXmlNode(const pugi::xml_node &elem,
                               std::string_view name, size_t sid) {
    double gain_value = 1.0;

    auto gain_node = elem.select_node("./P[@Name='Gain']");
    if (gain_node) {
      gain_value = gain_node.node().text().as_double();
    }

    return Gain{gain_value};
  }

  Gain(double gain) : m_gain(gain) {}

  double gain() const {
    return m_gain;
  }

  double m_gain;
};

class UnitDelay {
public:
  static UnitDelay parseFromXmlNode(const pugi::xml_node &elem,
                                    std::string_view name, size_t sid) {
    double sample_time = -1.0;

    auto st_node = elem.select_node("./P[@Name='SampleTime']");
    if (st_node) {
      sample_time = st_node.node().text().as_double();
    }

    return UnitDelay{sample_time};
  }

  UnitDelay(double sample_time) : m_sample_time(sample_time) {}

  double sample_time() const {
    return m_sample_time;
  }

  double m_sample_time;
};

class Outport {
public:
  static Outport parseFromXmlNode(const pugi::xml_node &elem,
                                  std::string_view name, size_t sid) {
    return Outport{};
  }
};

// clang-format off
using Kind = std::variant<Outport,
                          Inport,
                          UnitDelay,
                          Gain,
                          Sum>;
// clang-format on

struct Block {
  static std::shared_ptr<Block>
  withEnoughInfoAboutType(std::string_view type, const pugi::xml_node &elem) {
    std::string name;
    size_t sid;
    for (const auto &attr : elem.attributes()) {
      if (std::string_view(attr.name()) == "Name") {
        name = attr.as_string();
        std::erase_if(name, ::isspace);
      }
      if (std::string_view(attr.name()) == "SID") {
        sid = attr.as_ullong();
      }
    }

    std::vector<Dependency> ports(2, Dependency{""});
    Kind kind;

    if (type == "Inport") {
      kind = Inport::parseFromXmlNode(elem, name, sid);
    } else if (type == "Gain") {
      kind = Gain::parseFromXmlNode(elem, name, sid);
    } else if (type == "UnitDelay") {
      kind = UnitDelay::parseFromXmlNode(elem, name, sid);
    } else if (type == "Sum") {
      kind = Sum::parseFromXmlNode(elem, name, sid);
    } else if (type == "Outport") {
      kind = Outport::parseFromXmlNode(elem, name, sid);
    } else {
      throw std::runtime_error("Unknown block type: "s + type.data());
    }

    return std::make_shared<Block>(std::move(name), sid, std::move(ports), kind);
  }

  Block(std::string name, size_t sid, std::vector<Dependency> ports, Kind kind)
      : m_name(std::move(name)), m_sid(sid), m_ports(std::move(ports)),
        m_kind(std::move(kind)) {}

  std::string as_from(std::string package) const {
    return package + "." + m_name;
  }

  std::string_view name() const {
    return m_name;
  }

  size_t sid() const {
    return m_sid;
  }

  std::vector<Dependency> &deps() {
    return m_ports;
  }

  const std::vector<Dependency> &deps() const {
    return m_ports;
  }

  const Kind &kind() const {
    return m_kind;
  }

private:
  std::string m_name;
  size_t m_sid;

  // other blocks that this one depends on
  std::vector<Dependency> m_ports;
  // the block type-specific part
  Kind m_kind;
};
