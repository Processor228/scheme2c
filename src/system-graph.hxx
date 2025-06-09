#pragma once

#include "blocks.hxx"
#include <pugixml.hpp>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <fmt/core.h>

struct Connection {
  BlockSocket from;
  BlockSocket to;
};

class SystemGraph {
public:
  using Nodes = std::unordered_map<size_t, std::shared_ptr<Block>>;
  using Edges = std::unordered_map<size_t, std::vector<Connection>>;

  void fillFromXmlDoc(const pugi::xml_document &doc) {
    for (const auto &elem : doc.document_element()) {
      auto type = std::string_view(elem.name());
      if (type == "Block") {
        auto block = parseBlock(elem);
        m_nodes[block->m_sid] = block;
      }
      if (type == "Line") {
        auto [from, destinations] = parseLine(elem);
        for (auto dst : destinations) {
          /* to prevent cycles during topological sorting, lets
           * simply not add edges leading to UnitDelays */
          if (std::find(m_delayed.begin(), m_delayed.end(), dst.SID) ==
              m_delayed.end()) {
            m_edges[from.SID].push_back({from, dst});
          }
        }
      }
    }
  }

  inline const Nodes &nodes() const { return m_nodes; }

  inline const Edges &edges() const { return m_edges; }

  inline const std::vector<size_t> &inports() const { return m_inports; }

  inline const std::unordered_set<size_t> &outports() const { return m_outports; }

  inline const std::vector<size_t> &delayed() const { return m_delayed; }

private:
  std::shared_ptr<Block> parseBlock(const pugi::xml_node &elem) {
    std::string_view type;
    for (const auto &attr : elem.attributes()) {
      if (std::string_view(attr.name()) == "BlockType") {
        type = attr.as_string();
      }
    }
    auto block = Block::withEnoughInfoAboutType(type, elem);
    if (type == "Inport") {
      m_inports.push_back(block->m_sid);
    } else if (type == "Outport") {
      m_outports.insert(block->m_sid);
    } else if (type == "UnitDelay") {
      m_delayed.push_back(block->m_sid);
    }

    return block;
  }

  std::pair<BlockSocket, std::vector<BlockSocket>>
  parseLine(const pugi::xml_node &line_node) {
    std::vector<BlockSocket> dsts;
    BlockSocket src;

    auto src_node = line_node.select_node("./P[@Name='Src']");
    if (src_node) {
      src = BlockSocket::parseSocket(src_node.node().text().as_string());
    }
    std::string_view source_name = m_nodes.at(src.SID)->m_name;

    auto dst_node = line_node.select_node("./P[@Name='Dst']");
    if (dst_node) {
      dsts.push_back(
          BlockSocket::parseSocket(dst_node.node().text().as_string()));
    } else {
      for (auto branch_node : line_node.select_nodes("./Branch")) {
        auto branch_dst_node =
            branch_node.node().select_node("./P[@Name='Dst']");
        if (branch_dst_node) {
          dsts.push_back(BlockSocket::parseSocket(
              branch_dst_node.node().text().as_string()));
        }
      }
    }

    for (auto [sid, port] : dsts) {
      // if such sid is not yet defined, error will occur
      m_nodes.at(sid)->m_ports[port].name_of_connector = source_name;
    }

    return {src, dsts};
  }

  // SID -> block*
  Nodes m_nodes;
  // SID -> block outgoing connections
  Edges m_edges;

  std::vector<size_t> m_inports;
  std::unordered_set<size_t> m_outports;
  std::vector<size_t> m_delayed;
};
