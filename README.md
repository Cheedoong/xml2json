# xml2json

A header-only C++11 library that converts XML to JSON — and JSON back to XML.

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

`xml2json` is small, fast and has no external dependencies: RapidXML and
RapidJSON are bundled with it.  It started as the XML/JSON converter of the
soft-subtitle pipeline at Tencent Video and is still used there.

## Features

* Header-only — copy `include/` into your project and `#include "xml2json.hpp"`.
* XML → JSON (`xml2json`) and JSON → XML (`json2xml`).
* Returns either a JSON string or a `rapidjson::Document` you can walk.
* Optional pretty printing, numeric conversion, sibling-order recording,
  always-array output and qualified namespace names.
* Malformed input is reported as an exception instead of aborting the process.
* Works with any C++11 (or later) compiler.

## Quick start

```cpp
#include <iostream>
#include "xml2json.hpp"

int main()
{
    std::cout << xml2json("<a id=\"1\"><b>text</b></a>") << std::endl;
    // {"a":{"@id":"1","b":"text"}}
}
```

Compile with any C++11 compiler and the `include` directory on the include path:

```sh
c++ -std=c++11 -Iinclude my_app.cpp -o my_app
```

## Building the command line tool and the tests

With CMake:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

or with GNU make / mingw32-make:

```sh
make            # builds the ./xml2json tool
make test       # builds and runs the regression suite
make install    # copies the tool to $(PREFIX)/bin (default /usr/local)
```

Using the library from CMake:

```cmake
add_subdirectory(xml2json)              # or: find_package(xml2json)
target_link_libraries(my_app PRIVATE xml2json::xml2json)
```

## Command line tool

```
usage: xml2json [--json2xml] [input.xml]
       --json2xml, -j  convert JSON to XML instead of XML to JSON
       (reads from standard input when no file is given)
```

```sh
./xml2json input.xml            # XML -> JSON on stdout
./xml2json -j input.json        # JSON -> XML on stdout
cat input.xml | ./xml2json
```

The tool exits with a non-zero status and prints `error: ...` on stderr when the
file cannot be opened or the input is malformed.

## Conversion rules

| # | XML | JSON | Access |
|---|-----|------|--------|
| 1 | `<e/>` | `"e": null` | `o.e` |
| 2 | `<e>text</e>` | `"e": "text"` | `o.e` |
| 3 | `<e name="value"/>` | `"e": {"@name": "value"}` | `o.e["@name"]` |
| 4 | `<e name="value">text</e>` | `"e": {"#text": "text", "@name": "value"}` | `o.e["#text"]`, `o.e["@name"]` |
| 5 | `<e><a>text</a><b>text</b></e>` | `"e": {"a": "text", "b": "text"}` | `o.e.a`, `o.e.b` |
| 6 | `<e><a>text</a><a>text</a></e>` | `"e": {"a": ["text", "text"]}` | `o.e.a[0]`, `o.e.a[1]` |
| 7 | `<e>text<a>text</a></e>` | `"e": {"#text": "text", "a": "text"}` | `o.e["#text"]`, `o.e.a` |

In short: attributes become `@name` members, text becomes a `#text` member, an
element that only has text collapses to that text, and elements that repeat
become an array.

## API

All functions live in the single header and are `inline`.

```cpp
// XML -> JSON
std::string         xml2json(const char *xml_str);
std::string         xml2json(const char *xml_str, int indent);
std::string         xml2json(const char *xml_str, const xml2json_options &options);

// XML -> JSON, returning the parsed document instead of a string
rapidjson::Document xml2json_document(const char *xml_str,
                                      const xml2json_options &options);

// JSON -> XML
std::string         json2xml(const char *json_str);
std::string         json2xml(const char *json_str, int indent);
```

### Options

```cpp
struct xml2json_options
{
    int  indent;                 // spaces per level for pretty printing; <= 0 is compact
    bool numeric;                // emit numeric looking text as JSON numbers
    bool order;                  // add a "#order" array where sibling order would be lost
    bool array;                  // always emit child elements as JSON arrays
    bool qualify_element_names;  // emit "prefix:local" element names instead of "local"

    xml2json_options();          // takes its defaults from the configuration block below
};
```

The default constructor reproduces the historical behaviour, so existing code
keeps working.  The defaults come from a compile-time configuration block at the
top of `xml2json.hpp` (`xml2json_numeric_support`, `xml2json_keep_element_order`,
`xml2json_always_array`, `xml2json_qualify_element_names`, all `false`), which can
be adjusted there or overridden per call:

```cpp
xml2json_options options;
options.order = true;       // record the original sibling order
options.indent = 2;         // pretty print

std::cout << xml2json(xml, options);
```

* `numeric` only converts text that is *entirely* a number (`-42`, `+7`, `1.5`).
  Anything else — `99921-58-10-7`, `12 34`, `0x10`, `1e5` — stays a string.
  Integers are handled with 64-bit precision.
* `order` adds `"#order": ["b", "c", "b"]` to the elements whose children had to
  be regrouped, and only to those, so document order is not lost.
* `array` gives single children the same shape as repeated ones, which keeps
  look-ups simple.
* `qualify_element_names` restores the namespace prefix of element names
  (`<ns:e>` normally yields `"e"`, with this option `"ns:e"`).  Attribute names
  always keep their prefix.

### Errors

* `xml2json()` throws `rapidxml::parse_error` when the XML is malformed.
* `json2xml()` throws `std::runtime_error`, carrying the offset and message, when
  the JSON is malformed.

### json2xml

`json2xml()` implements the inverse of the rules above: `@name` members become
attributes, `#text` becomes the text content, other members become child
elements, arrays repeat the element, and the `#order` member is ignored.  A
`null` value becomes a self-closing element:

```cpp
json2xml("{\"a\":{\"@id\":\"9\",\"b\":[1,2]}}");  // <a id="9"><b>1</b><b>2</b></a>
```

Text and attribute values are escaped, and numbers keep their exact
representation because they are rendered by RapidJSON's own writer.  Because no
XML declaration is written, `xml2json(json2xml(json))` round-trips.

## Limitations

* XML with mixed content (an element holding both text and child elements) does
  not round-trip byte for byte: `xml2json` already collects the text nodes into a
  single `#text` member, losing its position relative to the children.
* Namespace prefixes are not resolved to their URIs in the output.  RapidXML
  exposes them (`xml_node::prefix()`, `xml_node::xmlns()`), and
  `qualify_element_names` keeps the prefix, but no `xmlns` resolution is
  performed.
* On Windows the input is read in text mode, so CRLF inside text nodes is
  normalised to LF, which makes the output differ from POSIX for such files
  (this is why `test/windows_style_results` exists).

## Bundled dependencies

| Library | Version |
|---------|---------|
| [RapidXML](http://rapidxml.sourceforge.net/) | 1.13, namespace aware |
| [RapidJSON](https://github.com/Tencent/rapidjson) | master snapshot `24b5e7a8` (2025-02-05) |

RapidJSON has not published a release since 1.1.0 (2016), but master is actively
maintained and is required for C++17 and later, where the 1.1.0 headers still use
the removed `std::iterator`.

## Tests

`test/test.cpp` is a self-contained regression suite (no test framework needed).
It checks the bundled documents against the expected output, plus the numeric
detection, malformed input handling, the `xml2json_document()` API, every option,
`json2xml` and a full round trip.  It also links a second translation unit that
includes the header, which guards against duplicate symbols.

Run it with `make test`, or `ctest --test-dir build` after a CMake build.  Passing
`--write` regenerates the expected `*.js.txt` files instead of checking them.

## Tested under

Linux, Mac OS X, Windows and OpenWRT.

## License

MIT, see [LICENSE](LICENSE).

## Donate

![WeChat Pay](https://raw.githubusercontent.com/Cheedoong/xml2json/master/donate/wechatpay_cheedoong.png)
![AliPay](https://raw.githubusercontent.com/Cheedoong/xml2json/master/donate/alipay_cheedoong.jpg)
