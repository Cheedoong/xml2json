// Regression tests for xml2json / json2xml.
//
// The fixtures (*.xml and the expected *.js.txt) are located automatically, so
// the test can be run either from the repository root:
//
//     test/test
//
// or from the test directory:
//
//     cd test && ./test
//
// Pass --write (or set XML2JSON_WRITE=1) to regenerate the expected files
// instead of checking them.

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "xml2json.hpp"

// Defined in test_link.cpp.  Having a second translation unit that also includes
// xml2json.hpp means the test only links if issue #23 (duplicated symbols) is
// really fixed.
std::string xml2json_from_second_translation_unit(const char *xml);

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

static bool file_exists(const std::string &path)
{
    std::ifstream in(path.c_str(), std::ios::binary);
    return in.good();
}

static std::string read_file(const std::string &path, bool binary)
{
    std::ifstream in(path.c_str(), binary ? (std::ios::in | std::ios::binary) : std::ios::in);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static std::string strip_bom(std::string text)
{
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF)
    {
        text.erase(0, 3);
    }
    return text;
}

// Directory that holds the fixtures, either "" (running inside test/) or "test/".
static std::string fixture_dir()
{
    const char *candidates[] = { "./", "test/", "../test/" };
    for(std::size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
    {
        if (file_exists(std::string(candidates[i]) + "test_track_1.xml"))
            return candidates[i];
    }
    return std::string();
}

static void test_bundled_files(const std::string &dir, bool write)
{
    // Reading the XML in text mode is what the command line tool does, and on
    // Windows that normalises CRLF to LF, so the expected output differs from
    // the one on POSIX systems.  Upstream therefore keeps both variants.
#if defined(_WIN32)
    const std::string platform_subdir = "windows_style_results/";
#else
    const std::string platform_subdir = "";
#endif

    for(int t = 1; t <= 3; ++t)
    {
        const std::string name = "test_track_" + std::to_string(t) + ".xml";
        const std::string json = xml2json(read_file(dir + name, false).c_str());

        std::string expected_path = dir + platform_subdir + "test_track_" + std::to_string(t) + ".js.txt";
        if (!file_exists(expected_path))
            expected_path = dir + "test_track_" + std::to_string(t) + ".js.txt";

        if (write)
        {
            std::ofstream out(expected_path.c_str(), std::ios::binary);
            const char bom[3] = { static_cast<char>(0xEF), static_cast<char>(0xBB), static_cast<char>(0xBF) };
            out.write(bom, 3);
            out << json;
            std::cout << "wrote: " << expected_path << "\n";
            ++g_checks;
            continue;
        }

        check_equal(json, strip_bom(read_file(expected_path, true)), name + " matches " + expected_path);
    }
}

int main(int argc, char *argv[])
{
    bool write = false;
    for(int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--write") == 0 || std::strcmp(argv[i], "-w") == 0)
            write = true;
    }
    if (std::getenv("XML2JSON_WRITE") != nullptr)
        write = true;

    const std::string dir = fixture_dir();
    if (dir.empty())
    {
        std::cerr << "error: test fixtures not found (run from the repository root or from the test/ directory)\n";
        return 2;
    }

    std::cout << "=== bundled test files ===\n";
    test_bundled_files(dir, write);

    std::cout << "\n=== input handling ===\n";
    check_equal(xml2json("<a x=\"1\">hi</a>"), "{\"a\":{\"#text\":\"hi\",\"@x\":\"1\"}}",
                "a string literal can be passed safely (was an access violation)");
    check_equal(xml2json("<a><b>1</b></a>"), "{\"a\":{\"b\":\"1\"}}", "one child object");
    check_equal(xml2json("<a><b/></a>"), "{\"a\":{\"b\":null}}", "empty element becomes null");
    check_equal(xml2json("<a><b>1</b><b>2</b></a>"), "{\"a\":{\"b\":[\"1\",\"2\"]}}", "repeated children become an array");
    check_equal(xml2json(""), "{}", "empty input yields an empty object");
    check_equal(xml2json("<a><!--comment--><?pi x?><b>1</b></a>"), "{\"a\":{\"b\":\"1\"}}",
                "non-element children are skipped");
    {
        const char *null_xml = nullptr;
        check_equal(xml2json(null_xml), "{}", "null XML input yields an empty object");
        rapidjson::Document doc = xml2json_document(null_xml, xml2json_options());
        check(doc.IsObject(), "null XML input yields an object document");
    }
    {
        const char *null_json = nullptr;
        check_equal(json2xml(null_json), "", "null JSON input yields an empty string");
    }

    std::cout << "\n=== issue #16: malformed input throws instead of aborting ===\n";
    {
        bool threw = false;
        try
        {
            xml2json("<a><b></a>");
        }
        catch(const rapidxml::parse_error &)
        {
            threw = true;
        }
        check(threw, "malformed XML raises rapidxml::parse_error");
    }

    std::cout << "\n=== issue #17: numeric support ===\n";
    {
        xml2json_options numeric;
        numeric.numeric = true;
        check_equal(xml2json("<r><i>99921-58-10-7</i></r>", numeric), "{\"r\":{\"i\":\"99921-58-10-7\"}}", "dashed value stays a string");
        check_equal(xml2json("<r><i>12 34</i></r>", numeric), "{\"r\":{\"i\":\"12 34\"}}", "value with a space stays a string");
        check_equal(xml2json("<r><i>0x10</i></r>", numeric), "{\"r\":{\"i\":\"0x10\"}}", "hexadecimal stays a string");
        check_equal(xml2json("<r><i>-42</i></r>", numeric), "{\"r\":{\"i\":-42}}", "negative integer");
        check_equal(xml2json("<r><i>+7</i></r>", numeric), "{\"r\":{\"i\":7}}", "explicitly signed integer");
        check_equal(xml2json("<r><i>102.50</i></r>", numeric), "{\"r\":{\"i\":102.5}}", "decimal number");
        check_equal(xml2json("<r><i>123456789012345678</i></r>", numeric), "{\"r\":{\"i\":123456789012345678}}", "64-bit integer is not truncated");
        check_equal(xml2json("<r><i/></r>", numeric), "{\"r\":{\"i\":null}}", "empty element is not a number");
        check_equal(xml2json("<r x=\"99921-58-10-7\"/>", numeric), "{\"r\":{\"@x\":\"99921-58-10-7\"}}", "attributes use the same rule");

        // A literal that does not fit into a double used to become +inf; the
        // RapidJSON writer rejects infinity, which left the JSON truncated.
        const std::string huge(400, '9');
        check_equal(xml2json(("<r><i>" + huge + "</i></r>").c_str(), numeric),
                    "{\"r\":{\"i\":\"" + huge + "\"}}",
                    "integer beyond double range stays a string");
        check_equal(xml2json(("<r><i>" + huge + ".0</i></r>").c_str(), numeric),
                    "{\"r\":{\"i\":\"" + huge + ".0\"}}",
                    "decimal beyond double range stays a string");
    }

    std::cout << "\n=== issue #19: xml2json_document() ===\n";
    {
        rapidjson::Document doc = xml2json_document("<a id=\"9\"><b>7</b></a>", xml2json_options());
        check(doc.IsObject() && doc.HasMember("a") && doc["a"].HasMember("b") &&
              doc["a"]["b"].IsString() && std::strcmp(doc["a"]["b"].GetString(), "7") == 0,
              "the returned document can be inspected directly");
    }

    std::cout << "\n=== issue #13: preserve sibling order (opt-in) ===\n";
    {
        xml2json_options order;
        order.order = true;
        check_equal(xml2json("<a><b>1</b><c>2</c><b>3</b></a>", order),
                    "{\"a\":{\"c\":\"2\",\"b\":[\"1\",\"3\"],\"#order\":[\"b\",\"c\",\"b\"]}}",
                    "#order records the document order");
        check_equal(xml2json("<a><b>1</b><c>2</c></a>", order), "{\"a\":{\"b\":\"1\",\"c\":\"2\"}}",
                    "no #order when nothing had to be regrouped");
    }

    std::cout << "\n=== issue #15: always array (opt-in) ===\n";
    {
        xml2json_options array;
        array.array = true;
        check_equal(xml2json("<a><b>1</b><c>2</c><b>3</b></a>", array),
                    "{\"a\":{\"c\":[\"2\"],\"b\":[\"1\",\"3\"]}}", "single and repeated children have the same shape");
    }

    std::cout << "\n=== issue #12: qualified element names (opt-in) ===\n";
    {
        xml2json_options qualify;
        qualify.qualify_element_names = true;
        check_equal(xml2json("<ns:e xmlns:ns=\"urn:x\"><ns:b>1</ns:b></ns:e>", qualify),
                    "{\"ns:e\":{\"@xmlns:ns\":\"urn:x\",\"ns:b\":\"1\"}}", "element prefixes are kept");
        check_equal(xml2json("<ns:e xmlns:ns=\"urn:x\"><ns:b>1</ns:b></ns:e>"),
                    "{\"e\":{\"@xmlns:ns\":\"urn:x\",\"b\":\"1\"}}", "default is the local name");
    }

    std::cout << "\n=== pretty printing ===\n";
    check_equal(xml2json("<a><b>1</b></a>", 2), "{\n  \"a\": {\n    \"b\": \"1\"\n  }\n}", "indent = 2");

    std::cout << "\n=== issue #14: json2xml() ===\n";
    check_equal(json2xml("{\"a\":{\"@id\":\"9\",\"b\":\"7\"}}"), "<a id=\"9\"><b>7</b></a>", "attributes and children");
    check_equal(json2xml("{\"a\":{\"b\":[\"1\",\"2\"]}}"), "<a><b>1</b><b>2</b></a>", "arrays repeat the element");
    check_equal(json2xml("{\"a\":{\"b\":null}}"), "<a><b/></a>", "null becomes a self closing element");
    check_equal(json2xml("{\"a\":{\"#text\":\"a<b&c\"}}"), "<a>a&lt;b&amp;c</a>", "text is escaped");
    check_equal(json2xml("{\"a\":{\"@x\":\"&\\\"\"}}"), "<a x=\"&amp;&quot;\"/>", "attributes are escaped");
    check_equal(json2xml("{\"a\":{\"n\":42,\"d\":1.5}}"), "<a><n>42</n><d>1.5</d></a>", "numbers keep their form");
    check_equal(json2xml("{\"a\":{\"n\":1e5,\"d\":1.2300}}"), "<a><n>100000.0</n><d>1.23</d></a>", "numbers use the shortest round-trip form");
    check_equal(json2xml("{\"a\":{\"i\":123456789012345678}}"), "<a><i>123456789012345678</i></a>", "64-bit integers stay exact");
    check_equal(json2xml("{\"a\":{\"@x\":[1,2]}}"), "<a x=\"12\"/>", "array attribute values are concatenated");
    check_equal(json2xml("{\"a\":{\"#text\":[\"x\",\"y\"]}}"), "<a>xy</a>", "#text arrays are concatenated");
    check_equal(json2xml("[1,2]"), "<root>1</root><root>2</root>", "top-level arrays are wrapped in root");
    check_equal(json2xml("42"), "<root>42</root>", "top-level scalars are wrapped in root");
    check_equal(json2xml("{\"a\":{\"#order\":[\"b\"],\"b\":\"1\"}}"), "<a><b>1</b></a>", "#order is ignored");
    {
        bool threw = false;
        try
        {
            json2xml("{\"a\": ");
        }
        catch(const std::runtime_error &)
        {
            threw = true;
        }
        check(threw, "invalid JSON raises std::runtime_error");
    }

    std::cout << "\n=== CVE-2024-38517 / 39684: extreme exponents ===\n";
    {
        check_equal(json2xml("{\"a\":1e-2147483648}"), "<a>0.0</a>",
                    "extreme negative exponent is clamped without crashing");
        {
            bool threw = false;
            try
            {
                json2xml("{\"a\":1e9999999999}");
            }
            catch(const std::runtime_error &)
            {
                threw = true;
            }
            check(threw, "huge positive exponent is rejected as invalid JSON");
        }
        {
            bool handled = false;
            try
            {
                const std::string xml = json2xml("{\"a\":0.184467440737095516159}");
                handled = !xml.empty();
            }
            catch(const std::runtime_error &)
            {
                handled = true;
            }
            check(handled, "long fractional literal is handled without crashing");
        }
    }

    std::cout << "\n=== round trip xml -> json -> xml -> json ===\n";
    {
        const char *xml = "<a><b>1</b><c>2</c><b>3</b></a>";
        const std::string first = xml2json(xml);
        check_equal(xml2json(json2xml(first.c_str()).c_str()), first, "json2xml is the inverse of xml2json");
    }

    std::cout << "\n=== configurable key names ===\n";
    {
        xml2json_options text;
        text.text_name = "_";
        check_equal(xml2json("<a id=\"1\">x</a>", text), "{\"a\":{\"_\":\"x\",\"@id\":\"1\"}}",
                    "text_name renames the text member");

        xml2json_options attr;
        attr.attribute_prefix = "$";
        check_equal(xml2json("<a id=\"1\">x</a>", attr), "{\"a\":{\"#text\":\"x\",\"$id\":\"1\"}}",
                    "attribute_prefix renames the attribute member");

        xml2json_options multi;
        multi.attribute_prefix = "attr-";
        check_equal(xml2json("<a id=\"1\"/>", multi), "{\"a\":{\"attr-id\":\"1\"}}",
                    "a multi-character prefix is matched as a whole");

        xml2json_options bare;
        bare.attribute_prefix = "";
        check_equal(xml2json("<a id=\"1\"><b>2</b></a>", bare), "{\"a\":{\"id\":\"1\",\"b\":\"2\"}}",
                    "an empty prefix emits bare attribute names");

        xml2json_options order;
        order.order = true;
        order.order_name = "#seq";
        check_equal(xml2json("<a><b>1</b><c>2</c><b>3</b></a>", order),
                    "{\"a\":{\"c\":\"2\",\"b\":[\"1\",\"3\"],\"#seq\":[\"b\",\"c\",\"b\"]}}",
                    "order_name renames the order member");

        xml2json_options no_order;
        no_order.order = true;
        no_order.order_name = "";
        check_equal(xml2json("<a><b>1</b><c>2</c><b>3</b></a>", no_order),
                    "{\"a\":{\"c\":\"2\",\"b\":[\"1\",\"3\"]}}",
                    "an empty order_name suppresses the order member");
    }

    std::cout << "\n=== configurable key names: json2xml ===\n";
    {
        xml2json_options custom;
        custom.text_name = "_";
        custom.attribute_prefix = "$";
        check_equal(json2xml("{\"a\":{\"_\":\"x\",\"$id\":\"1\"}}", custom), "<a id=\"1\">x</a>",
                    "json2xml honours text_name and attribute_prefix");

        xml2json_options multi;
        multi.attribute_prefix = "attr-";
        check_equal(json2xml("{\"a\":{\"attr-id\":\"1\"}}", multi), "<a id=\"1\"/>",
                    "json2xml strips a multi-character prefix as a whole");

        xml2json_options bare;
        bare.attribute_prefix = "";
        check_equal(json2xml("{\"a\":{\"id\":\"1\",\"b\":\"2\"}}", bare), "<a><id>1</id><b>2</b></a>",
                    "with an empty prefix every member is a child element");

        xml2json_options order;
        order.order_name = "#seq";
        check_equal(json2xml("{\"a\":{\"b\":\"1\",\"#seq\":[\"b\"]}}", order), "<a><b>1</b></a>",
                    "a custom order_name is not written as content");

        xml2json_options defaults;
        const char *json = "{\"a\":{\"@id\":\"1\",\"b\":[\"1\",\"2\"]}}";
        check_equal(json2xml(json, defaults), json2xml(json),
                    "default options match the plain json2xml overload");
    }

    std::cout << "\n=== default options are unchanged ===\n";
    {
        xml2json_options defaults;
        const char *xml = "<a id=\"1\"><b>1</b><b>2</b></a>";
        check_equal(xml2json(xml, defaults), xml2json(xml),
                    "default options match the plain xml2json overload");
    }

    std::cout << "\n=== issue #23: header usable from a second translation unit ===\n";
    check_equal(xml2json_from_second_translation_unit("<a><b>1</b></a>"), "{\"a\":{\"b\":\"1\"}}", "second tu links and works");

    std::cout << "\n" << (g_failures == 0 ? "PASSED" : "FAILED") << ": "
              << (g_checks - g_failures) << "/" << g_checks << " checks\n";
    return g_failures == 0 ? 0 : 1;
}
