#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "xml2json.hpp"

namespace
{

void convert(std::istream &input, std::ostream &output, bool to_xml)
{
    std::ostringstream oss;
    oss << input.rdbuf();

    const std::string text = oss.str();

    if (to_xml)
        output << json2xml(text.c_str()) << std::endl;
    else
        output << xml2json(text.c_str()) << std::endl;
}

void usage(const char *program)
{
    std::cerr << "usage: " << program << " [--json2xml] [input.xml]" << std::endl
              << "       --json2xml, -j  convert JSON to XML instead of XML to JSON" << std::endl
              << "       (reads from standard input when no file is given)" << std::endl;
}

} // namespace

int main(int argc, const char *const argv[])
{
    bool to_xml = false;
    const char *path = nullptr;

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "-j") == 0 || std::strcmp(argv[i], "--json2xml") == 0)
        {
            to_xml = true;
        }
        else if (path == nullptr)
        {
            path = argv[i];
        }
        else
        {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    std::ifstream file;
    std::istream *input = &std::cin;

    if (path != nullptr)
    {
        file.open(path);
        if (!file)
        {
            std::cerr << "error: cannot open '" << path << "'" << std::endl;
            return EXIT_FAILURE;
        }
        input = &file;
    }

    try
    {
        convert(*input, std::cout, to_xml);
    }
    catch (const std::exception &e)
    {
        // e.g. rapidxml::parse_error for malformed XML, std::runtime_error for
        // malformed JSON: report it instead of letting it escape main() and
        // abort the process.
        std::cerr << "error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
