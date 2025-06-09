#pragma once

#include <pugixml.hpp>

#include <cstddef>
#include <fstream>
#include <ranges>
#include <set>
#include <sstream>
#include <string_view>
#include <vector>

#include <fmt/core.h>

#include "blocks.hxx"
#include "system-graph.hxx"
#include "overload.hxx"

class CodeGenerator {
public:
  CodeGenerator(const SystemGraph &graph) : m_graph(graph) {}

  CodeGenerator &withPackageName(std::string_view name) {
    m_package_name = std::move(name);
    return *this;
  }

  CodeGenerator &withProlog(const std::string_view preamble) {
    m_prolog = std::string(preamble);
    return *this;
  }

  CodeGenerator &withEpilogue(std::string_view epilogue) {
    m_epilogue = std::string(epilogue);
    return *this;
  }

  void into(std::string_view path) {
    std::ofstream file(path.data());

    flushProlog();

    m_res << generatePipelineVars();
    m_res << generateInitialStateSetter();
    m_res << generateStepRunner();
    m_res << generateExportedPorts();

    flushEpilogue();

    file << m_res.str();
  }

private:
  void flushProlog() {
    m_res << fmt::vformat(m_prolog, fmt::make_format_args(m_package_name))
          << '\n';
  }

  std::string generatePipelineVars() {
    std::stringstream ss;

    ss << "static struct\n{\n";
    for (auto [id, block] : m_graph.nodes()) {
      ss << fmt::format("    double {};\n", block->m_name);
    }

    ss << "} " << m_package_name << ";\n\n";

    return ss.str();
  }

  std::string generateInitialStateSetter() {
    std::stringstream ss;

    ss << "void " << m_package_name << "_generated_init()\n{\n";
    for (auto [id, block] : m_graph.nodes()) {
      // get code that is required to setup a node
      std::string code = std::visit(
          overload{[&](const UnitDelay &ud) -> std::string {
                     return m_package_name + "." + block->m_name + " = 0;";
                   },

                   [](const Outport &) -> std::string { return ""; },
                   [](const Inport &) -> std::string { return ""; },
                   [](const Gain &) -> std::string { return ""; },
                   [](const Sum &) -> std::string { return ""; }},
          block->m_kind);

      if (!code.empty())
        ss << "    " << code;
    }

    ss << "\n}\n\n";

    return ss.str();
  }

  void dfs(size_t sid, std::set<size_t> &visited, std::vector<size_t> &order) {
    visited.insert(sid);

    if (!m_graph.edges().contains(sid)) {
      order.push_back(sid);
      return;
    }

    for (auto connection : m_graph.edges().at(sid)) {
      size_t to_sid = connection.to.SID;
      if (!visited.contains(to_sid))
        dfs(to_sid, visited, order);
    }
    order.push_back(sid);
  }

  std::vector<size_t> sortTopologically() {
     std::vector<size_t> order;
     std::set<size_t> visited;

     for (auto [sid, block] : m_graph.nodes()) {
       if (!visited.contains(sid))
         dfs(sid, visited, order);
     }
     return order;
  }

  std::string generateStepRunner() {
    std::stringstream ss;
    ss << "void " << m_package_name << "_generated_step()\n{\n";

    std::vector<size_t> order = sortTopologically();

    // print evaluation propogation
    for (auto sid : std::ranges::reverse_view(order)) {
      auto node = m_graph.nodes().at(sid);
      std::string code = generateCode(node);
      if (!code.empty()) {
        ss << "    " << code;
      }
    }

    ss << "\n";

    // and only then update delayed units
    for (auto sid_delayed : m_graph.delayed()) {
      auto node = m_graph.nodes().at(sid_delayed);
      std::string code = generateCode(node);

      if (!code.empty()) {
        ss << "    " << code;
      }
    }

    ss << "}\n\n";
    return ss.str();
  }

  std::string generateCode(const std::shared_ptr<Block> &node) {
    return std::visit(
        overload{[&](const UnitDelay &ud) -> std::string {
                   return fmt::format("{}.{} = {}.{};\n",
                        m_package_name,
                        node->m_name,
                        m_package_name,
                        node->m_ports[0].name_of_connector);
                 },
                 [](const Outport &) -> std::string { return ""; },
                 [](const Inport &) -> std::string { return ""; },
                 [&](const Gain &g) -> std::string {
                   return fmt::format("{}.{} = {}.{} * {};\n",
                        m_package_name,
                        node->m_name,
                        m_package_name,
                        node->m_ports[0].name_of_connector,
                        g.gain());
                 },
                 [&](const Sum &s) -> std::string {
                   std::string sign_first =
                       (s.first() == Sum::Op::Minus ? " - " : +"");
                   std::string sign_scd =
                       (s.scd() == Sum::Op::Minus ? " - " : +" + ");
                   return fmt::format("{}.{} = {}{}.{}{}{}.{};\n",
                        m_package_name,
                        node->m_name,
                        sign_first,
                        m_package_name,
                        node->m_ports[0].name_of_connector,
                        sign_scd,
                        m_package_name,
                        node->m_ports[1].name_of_connector);
                 }},
        node->m_kind);
  }

  std::string generateExportedPorts() {
    std::stringstream ss;

    ss << "static const " << m_package_name << "_ExtPort ext_ports[] = {\n";
    for (auto [id, block] : m_graph.nodes()) {
      std::string code = std::visit(
          overload{[](const UnitDelay &ud) -> std::string { return ""; },
                   [&](const Outport &) -> std::string {
                     return fmt::format(R"({{ "{}", &{}.{}, 0 }},)"
                                        "\n",
                                        block->m_name, m_package_name,
                                        block->m_ports[0].name_of_connector);
                   },
                   [&](const Inport &) -> std::string {
                     return fmt::format(R"({{ "{}", &{}.{}, 1 }},)"
                                        "\n",
                                        block->m_name, m_package_name,
                                        block->m_name);
                     ;
                   },
                   [](const Gain &) -> std::string { return ""; },
                   [](const Sum &) -> std::string { return ""; }},
          block->m_kind);

      if (!code.empty()) {
        ss << "    " << code;
      }
    }

    ss << "    " << "{0, 0, 0}\n";
    ss << "};\n";

    return ss.str();
  }

  void flushEpilogue() {
    m_res << fmt::vformat(m_epilogue, fmt::make_format_args(m_package_name))
          << "\n";
  }

  const SystemGraph &m_graph;
  std::stringstream m_res;
  std::string m_package_name;
  std::string m_prolog;
  std::string m_epilogue;
};
