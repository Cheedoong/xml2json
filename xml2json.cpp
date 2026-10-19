#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "xml2json.hpp"

namespace
{

void convert(std::istream &input, std::ostream &output)
{
    std::ostringstream oss;
    oss << input.rdbuf();

    const std::string xml_str = oss.str();
    output << xml2json(xml_str.c_str()) << std::endl;
}

void usage(const char *program)
{
    std::cerr << "usage: " << program << " [input.xml]" << std::endl
              << "       (reads from standard input when no file is given)" << std::endl;
}

} // namespace

int main(int argc, const char *const argv[])
{
    if (argc > 2)
    {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    std::ifstream file;
    std::istream *input = &std::cin;

    if (argc == 2)
    {
        file.open(argv[1]);
        if (!file)
        {
            std::cerr << "error: cannot open '" << argv[1] << "'" << std::endl;
            return EXIT_FAILURE;
        }
        input = &file;
    }

    try
    {
        convert(*input, std::cout);
    }
    catch (const std::exception &e)
    {
        // e.g. rapidxml::parse_error thrown for malformed XML: report it
        // instead of letting it escape main() and abort the process.
        std::cerr << "error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
