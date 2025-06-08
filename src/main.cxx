#include <cstdlib>
#include <exception>
#include <pugixml.hpp>
#include <string_view>
#include <iostream>

#include "generator.hxx"
#include "system-graph.hxx"

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " /path/to/file.xml";
        exit(EXIT_FAILURE);
    }
    const auto filepath = std::string_view(argv[1]);

    pugi::xml_document doc;
    pugi::xml_parse_result err = doc.load_file(filepath.data());
    if (!err) {
        exit(EXIT_FAILURE);
    }

    SystemGraph graph;
    try {
        graph.fillFromXmlDoc(doc);

        CodeGenerator{graph}
            .withPackageName("nwocg")
            .withProlog(R"(
#include "{0}_run.h"
#include <math.h>
                )")
            .withEpilogue(R"(
const {0}_ExtPort * const
    {0}_generated_ext_ports = ext_ports;

const size_t
    {0}_generated_ext_ports_size = sizeof(ext_ports);
                )")
            .into("autogen.c");

    } catch(const std::exception& e) {
        std::cout << e.what();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
