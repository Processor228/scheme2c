#pragma once

#include "pugixml.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <fmt/core.h>

#include "blocks.hxx"

struct Connection {
    BlockSocket from;
    BlockSocket to;

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

class SystemGraph {
public:

    using Nodes = std::unordered_map<size_t, std::shared_ptr<Block>>;
    using Edges = std::unordered_map<size_t, std::vector<Connection>>;

    void fillFromXmlDoc(const pugi::xml_document& doc) {
        for (const auto& elem : doc.document_element()) {
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
                    if (std::find(m_delayed.begin(), m_delayed.end(), dst.SID) == m_delayed.end()) {
                        m_edges[from.SID].push_back({from, dst});
                    }
                }
            }
        }
    }

    inline const Nodes& nodes() const {
        return m_nodes;
    }

    inline const Edges& edges() const {
        return m_edges;
    }

    inline const std::vector<size_t>& inports() const {
        return m_inports;
    }

    inline const std::vector<size_t>& outports() const {
        return m_outports;
    }

    inline const std::vector<size_t>& delayed() const {
        return m_delayed;
    }

private:
    std::shared_ptr<Block> parseBlock(const pugi::xml_node& elem) {
        std::string_view type;
        for (const auto& attr: elem.attributes()) {
            if (std::string_view(attr.name()) == "BlockType") {
                type = attr.as_string();
            }
        }
        auto block = withEnoughInfoAboutType(type, elem);
        if (type == "Inport") {
            m_inports.push_back(block->m_sid);
        } else if (type == "Outport") {
            m_outports.push_back(block->m_sid);
        } else if (type == "UnitDelay") {
            m_delayed.push_back(block->m_sid);
        }
        return block;
    }

    std::pair<BlockSocket, std::vector<BlockSocket>> parseLine(const pugi::xml_node& line_node) {
        std::vector<BlockSocket> dsts;
        BlockSocket src;

        auto src_node = line_node.select_node("./P[@Name='Src']");
        if (src_node) {
            src = Connection::parseSocket(src_node.node().text().as_string());
        }
        std::string_view source_name = m_nodes.at(src.SID)->m_name;

        auto dst_node = line_node.select_node("./P[@Name='Dst']");
        if (dst_node) {
            dsts.push_back(Connection::parseSocket(dst_node.node().text().as_string()));
        } else {
            for (auto branch_node : line_node.select_nodes("./Branch")) {
                auto branch_dst_node = branch_node.node().select_node("./P[@Name='Dst']");
                if (branch_dst_node) {
                    dsts.push_back(Connection::parseSocket(branch_dst_node.node().text().as_string()));
                }
            }
        }

        for (auto [sid, port] : dsts) {
            // if such sid is not yet defined, error will occur
            m_nodes.at(sid)->ports[port].name_of_connector = source_name;
        }


        return {src, dsts};
    }

    // SID -> block*
    Nodes m_nodes;
    // SID -> block outgoing connections
    Edges m_edges;

    std::vector<size_t> m_inports;
    std::vector<size_t> m_outports;
    std::vector<size_t> m_delayed;
};
