// Regression tests for deep nesting:
//
//   * json2xml() parses and writes with explicit stacks, so deeply nested
//     JSON must not overflow the call stack.
//   * xml2json() still uses RapidXML's recursive parser, so it rejects XML
//     deeper than max_xml_depth before the parser runs.
//
// The test is self-contained (no framework needed).

#include <cstddef>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include "xml2json.hpp"

static int g_checks = 0;
static int g_failures = 0;

static void check(bool condition, const std::string &what)
{
    ++g_checks;
    if (condition)
    {
        std::cout << "ok   : " << what << "\n";
        return;
    }
    ++g_failures;
    std::cout << "FAIL : " << what << "\n";
}

static void check_equal(const std::string &actual, const std::string &expected, const std::string &what)
{
    ++g_checks;
    if (actual == expected)
    {
        std::cout << "ok   : " << what << "\n";
        return;
    }
    ++g_failures;
    std::cout << "FAIL : " << what << "\n";
    std::cout << "       expected: " << expected.substr(0, 200) << "\n";
    std::cout << "       actual  : " << actual.substr(0, 200) << "\n";
}

static std::string make_nested_xml(std::size_t depth)
{
    std::string xml;
    xml.reserve(depth * 7 + 16);
    for(std::size_t i = 0; i < depth; ++i)
        xml += "<a>";
    for(std::size_t i = 0; i < depth; ++i)
        xml += "</a>";
    return xml;
}

static std::string make_nested_json_object(std::size_t depth)
{
    std::string json;
    json.reserve(depth * 6 + 8);
    for(std::size_t i = 0; i < depth; ++i)
        json += "{\"a\":";
    json += '1';
    for(std::size_t i = 0; i < depth; ++i)
        json += '}';
    return json;
}

static std::string make_nested_json_array(std::size_t depth)
{
    std::string json;
    json.reserve(depth * 2 + 8);
    for(std::size_t i = 0; i < depth; ++i)
        json += '[';
    json += '1';
    for(std::size_t i = 0; i < depth; ++i)
        json += ']';
    return json;
}

static std::size_t count_substring(const std::string &text, const std::string &needle)
{
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string::npos)
    {
        ++count;
        pos += needle.size();
    }
    return count;
}

static bool throws_with_message(const std::string &xml, const xml2json_options &options, const std::string &needle)
{
    try
    {
        xml2json(xml.c_str(), options);
    }
    catch(const std::runtime_error &e)
    {
        return std::string(e.what()).find(needle) != std::string::npos;
    }
    catch(const std::exception &e)
    {
        std::cout << "unexpected exception: " << e.what() << "\n";
    }
    return false;
}

int main()
{
    std::cout << "=== json2xml: deep nesting ===\n";
    {
        const std::size_t depth = 8192;
        const std::string json = make_nested_json_object(depth);
        std::string xml;
        bool threw = false;
        try
        {
            xml = json2xml(json.c_str());
        }
        catch(const std::exception &e)
        {
            threw = true;
            std::cout << "unexpected exception: " << e.what() << "\n";
        }

        check(threw == false, "8k nested JSON objects do not overflow the stack");
        check(count_substring(xml, "<a>") == depth, "8k nested JSON objects emit every opening tag");
        check(count_substring(xml, "</a>") == depth, "8k nested JSON objects emit every closing tag");
    }

    {
        const std::size_t depth = 16384;
        const std::string json = make_nested_json_object(depth);
        std::string xml;
        bool threw = false;
        try
        {
            xml = json2xml(json.c_str());
        }
        catch(const std::exception &e)
        {
            threw = true;
            std::cout << "unexpected exception: " << e.what() << "\n";
        }

        check(threw == false, "16k nested JSON objects do not overflow the stack");
        check(count_substring(xml, "<a>") == depth, "16k nested JSON objects emit every opening tag");
        check(count_substring(xml, "</a>") == depth, "16k nested JSON objects emit every closing tag");
    }

    {
        const std::size_t depth = 8192;
        const std::string json = "{\"a\":{\"#text\":" + make_nested_json_array(depth) + "}}";
        std::string xml;
        bool threw = false;
        try
        {
            xml = json2xml(json.c_str());
        }
        catch(const std::exception &e)
        {
            threw = true;
            std::cout << "unexpected exception: " << e.what() << "\n";
        }

        check(threw == false, "8k nested #text arrays do not overflow the stack");
        check_equal(xml, "<a>1</a>", "8k nested #text arrays are flattened in order");
    }

    std::cout << "\n=== XML depth guard ===\n";
    {
        xml2json_options defaults;
        check(defaults.max_xml_depth == 1024, "default max_xml_depth is 1024");

        bool accepted = false;
        try
        {
            const std::string input = "<?xml version=\"1.0\"?>" + make_nested_xml(16);
            const std::string json = xml2json(input.c_str());
            accepted = json.empty() == false;
        }
        catch(const std::exception &e)
        {
            std::cout << "unexpected exception: " << e.what() << "\n";
        }
        check(accepted, "XML below the default limit is accepted");

        bool at_limit = false;
        try
        {
            const std::string json = xml2json(make_nested_xml(1024).c_str(), defaults);
            at_limit = json.empty() == false;
        }
        catch(const std::exception &e)
        {
            std::cout << "unexpected exception: " << e.what() << "\n";
        }
        check(at_limit, "XML at the default limit is accepted");

        check(throws_with_message(make_nested_xml(1025), defaults, "nesting depth"),
              "XML above the default limit is rejected with a depth error");
    }

    {
        xml2json_options options;
        options.max_xml_depth = 32;
        bool accepted = false;
        try
        {
            const std::string json = xml2json(make_nested_xml(32).c_str(), options);
            accepted = json.empty() == false;
        }
        catch(const std::exception &e)
        {
            std::cout << "unexpected exception: " << e.what() << "\n";
        }
        check(accepted, "a custom limit accepts XML at the limit");
        check(throws_with_message(make_nested_xml(33), options, "nesting depth"),
              "a custom limit rejects XML above the limit");
    }

    {
        xml2json_options options;
        options.max_xml_depth = 0;
        bool accepted = false;
        try
        {
            const std::string json = xml2json(make_nested_xml(128).c_str(), options);
            accepted = json.empty() == false;
        }
        catch(const std::exception &e)
        {
            std::cout << "unexpected exception: " << e.what() << "\n";
        }
        check(accepted, "max_xml_depth = 0 disables the guard");
    }

    {
        xml2json_options options;
        options.max_xml_depth = 1;
        const std::string xml =
            "<a title=\">\">"
            "<!-- <b><c> -->"
            "<![CDATA[<d><e>]]>"
            "<?pi <f>?>"
            "<b/>"
            "</a>";
        bool accepted = false;
        try
        {
            const std::string json = xml2json(xml.c_str(), options);
            accepted = json.empty() == false;
        }
        catch(const std::exception &e)
        {
            std::cout << "unexpected exception: " << e.what() << "\n";
        }
        check(accepted, "comments, CDATA, PIs, quoted '>' and self-closing tags do not count as depth");
    }

    std::cout << "\n" << (g_failures == 0 ? "PASSED" : "FAILED") << ": "
              << (g_checks - g_failures) << "/" << g_checks << " checks\n";
    return g_failures == 0 ? 0 : 1;
}
