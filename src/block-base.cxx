#include "block-base.hxx"
#include "blocks.hxx"

std::shared_ptr<Block> Block::withEnoughInfoAboutType(
    std::string_view type,
    const pugi::xml_node& elem
) {
    std::string name;
    size_t sid;
    for (const auto& attr: elem.attributes()) {
        if (std::string_view(attr.name()) == "Name") {
            name = attr.as_string();
            std::erase_if(name, ::isspace);
        }
        if (std::string_view(attr.name()) == "SID") {
            sid = attr.as_ullong();
        }
    }

    if (type == "Inport") {
        return Inport::parseFromXmlNode(elem, name, sid);
    } else if (type == "Gain") {
        return Gain::parseFromXmlNode(elem, name, sid);
    } else if (type == "UnitDelay") {
        return UnitDelay::parseFromXmlNode(elem, name, sid);
    } else if (type == "Sum") {
        return Sum::parseFromXmlNode(elem, name, sid);
    } else if (type == "Outport") {
        return Outport::parseFromXmlNode(elem, name, sid);
    } else {
        throw std::runtime_error("Unknown block type: "s + type.data());
    }
}