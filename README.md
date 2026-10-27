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
  always-array output, qualified namespace names and configurable key names.
* Malformed input is reported as an exception instead of aborting the process.
* Requires C++11 or later.

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
cmake --install build --prefix /path/to/prefix
```

or with GNU make / mingw32-make:

```sh
make            # builds the ./xml2json tool
make test       # builds and runs the regression suite
make install    # copies the tool to $(PREFIX)/bin (default /usr/local)
```

`make install` only installs the command line tool and needs the POSIX `install`
utility; use `cmake --install` to install the headers, the CMake package
configuration and the tool together.

Using the library from CMake:

```cmake
# If xml2json is a subdirectory of your project:
add_subdirectory(xml2json)
target_link_libraries(my_app PRIVATE xml2json::xml2json)

# If xml2json was installed, point CMAKE_PREFIX_PATH at the prefix:
find_package(xml2json CONFIG REQUIRED)
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
file cannot be opened or the input is malformed.  It always uses the default
options; use the library API for numeric conversion, pretty printing, custom key
names and the other options.

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
become an array.  `@`, `#text` and `#order` are only the default names; the
`text_name`, `attribute_prefix` and `order_name` options change them.

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
std::string         json2xml(const char *json_str, const xml2json_options &options);
```

`xml2json_document()` returns a `rapidjson::Document` that owns its allocator;
keep the document alive while using its values.  The conversion functions keep
no global state, so separate calls can run concurrently as long as they do not
share a `Document` or an `xml2json_options` object.

### Options

```cpp
struct xml2json_options
{
    int  indent;                  // spaces per level for pretty printing; <= 0 is compact
    bool numeric;                 // emit numeric looking text as JSON numbers
    bool order;                   // add a "#order" array where sibling order would be lost
    bool array;                   // always emit child elements as JSON arrays
    bool qualify_element_names;   // emit "prefix:local" element names instead of "local"

    std::string text_name;        // member name of an element's own text (default "#text")
    std::string attribute_prefix; // prefix prepended to attribute names (default "@"; may be empty)
    std::string order_name;       // member name of the order array (default "#order")

    xml2json_options();           // takes its defaults from the configuration block below
};
```

The default constructor reproduces the historical behaviour, so existing code
keeps working.  The defaults come from a compile-time configuration block at the
top of `xml2json.hpp` (`xml2json_numeric_support`, `xml2json_keep_element_order`,
`xml2json_always_array`, `xml2json_qualify_element_names`, all `false`, plus
`xml2json_text_additional_name`, `xml2json_attribute_name_prefix` and
`xml2json_order_additional_name`), which can be adjusted there or overridden per
call.  With the default options the output is byte-for-byte identical to the
historical implementation; every new capability is opt-in.

```cpp
xml2json_options options;
options.order = true;       // record the original sibling order
options.indent = 2;         // pretty print

std::cout << xml2json(xml, options);
```

* `numeric` only converts text that is *entirely* a decimal number (`-42`, `+7`,
  `1.5`, `102.50`).  Anything else — `99921-58-10-7`, `12 34`, `0x10`, `1e5` —
  stays a string.  Integers are parsed as 64-bit values; larger integers fall
  back to `double`, and values that do not fit a `double` stay strings instead of
  producing truncated JSON.  Decimal parsing follows the current C locale, so
  set `LC_NUMERIC=C` if your input uses `.` under another locale.
* `order` adds `"#order": ["b", "c", "b"]` to the elements whose children had to
  be regrouped, and only to those, so document order is not lost.
* `array` gives single children the same shape as repeated ones, which keeps
  look-ups simple.
* `qualify_element_names` restores the namespace prefix of element names
  (`<ns:e>` normally yields `"e"`, with this option `"ns:e"`).  Attribute names
  always keep their prefix.
* `text_name`, `attribute_prefix` and `order_name` change the three JSON key
  names that used to be compile-time constants only.  Their defaults are exactly
  those constants, so the output does not change unless you set them.  This is
  what a node/xml2js style consumer needs:

  ```cpp
  xml2json_options options;
  options.text_name        = "_";
  options.attribute_prefix = "$";

  xml2json("<a id=\"1\">x</a>", options);
  // {"a":{"_":"x","$id":"1"}}
  ```

* `attribute_prefix` may be empty, in which case attribute names are emitted
  without any prefix.  They can then collide with child elements of the same
  name, so only use it when the schema makes that unambiguous.  `text_name` must
  not be empty; an empty `order_name` suppresses the `#order` array entirely.

### Errors

* `xml2json()` and `xml2json_document()` throw `rapidxml::parse_error` when the
  XML is malformed.
* `json2xml()` throws `std::runtime_error`, carrying the offset and message, when
  the JSON is malformed.

### json2xml

`json2xml()` implements the inverse of the rules above: members prefixed with
`attribute_prefix` (by default `@`) become attributes, the `text_name` member (by
default `#text`) becomes the text content, other members become child elements,
arrays repeat the element, and the `order_name` member (by default `#order`) is
ignored.  A `null` value becomes a self-closing element:

```cpp
json2xml("{\"a\":{\"@id\":\"9\",\"b\":[1,2]}}");  // <a id="9"><b>1</b><b>2</b></a>
```

The `json2xml(json, options)` overload applies the same `indent`, `text_name`,
`attribute_prefix` and `order_name` as `xml2json`, so a customised conversion
stays symmetric.  The remaining options (`numeric`, `order`, `array` and
`qualify_element_names`) only affect XML to JSON.  When `attribute_prefix` is
empty there is no way to tell an attribute from a child element, so every member
is written as a child element rather than being dropped:

```cpp
xml2json_options options;
options.attribute_prefix = "";           // no attribute prefix
json2xml("{\"a\":{\"id\":\"9\"}}", options);  // <a><id>9</id></a>
```

Text and attribute values are escaped (`&`, `<` and `>`; `"` is additionally
escaped in attributes), and no XML declaration is written.  The output is best
effort: for JSON produced by `xml2json()` with the same options,
`xml2json(json2xml(json))` reproduces the original JSON for the supported
structures.  Arbitrary JSON is not guaranteed to round-trip:

* JSON numbers are rendered by RapidJSON's writer, so they use the shortest
  round-trip form rather than the original spelling: `1e5` becomes `100000.0`
  and `1.2300` becomes `1.23`.  Integers that fit `int64`/`uint64` stay exact.
* A top-level array or scalar is wrapped in `<root>`; a top-level `null`
  produces an empty string.
* `#text` arrays are concatenated and `#order` (or the configured
  `order_name`) is ignored.
* Mixed content cannot round-trip because `xml2json()` already loses the text
  position relative to the child elements.

## Limitations

* XML with mixed content (an element holding both text and child elements) does
  not round-trip byte for byte: `xml2json` already collects the text nodes into a
  single `#text` member, losing its position relative to the children.
* Namespace prefixes are not resolved to their URIs in the output.  RapidXML
  exposes them (`xml_node::prefix()`, `xml_node::xmlns()`), and
  `qualify_element_names` keeps the prefix, but no `xmlns` resolution is
  performed.
* A CDATA-only element is not collapsed like plain text:
  `<e><![CDATA[x]]></e>` becomes `{"e":{"#text":"x"}}`, not `{"e":"x"}`.
* On Windows the input is read in text mode, so CRLF inside text nodes is
  normalised to LF, which makes the output differ from POSIX for such files
  (this is why `test/windows_style_results` exists).
* `json2xml()` does not validate XML names.  Object keys that are not valid XML
  names (for example `"1a"`, or a name containing a space) are copied verbatim
  and can produce malformed XML.  Attributes are recognised purely by the
  configured prefix, so `"@"` alone yields an empty attribute name.
* Only `&`, `<` and `>` (plus `"` in attribute values) are escaped.  Other
  characters are copied as-is, so JSON strings containing control characters
  that XML 1.0 forbids (such as `\u0001`) produce invalid XML.
* An attribute value is converted to text: arrays are concatenated and objects
  become an empty string.  Attributes therefore cannot represent structured
  values.
* Duplicate JSON object keys are not detected; each occurrence is written as a
  separate element (or attribute).
* Both conversions are recursive and have no explicit depth limit.  On a 1 MB
  stack, roughly 4,000 nested XML elements (XML to JSON) or 5,000 nested JSON
  values (JSON to XML) can be processed; deeper input can overflow the stack.

## Bundled dependencies

| Library | Version |
|---------|---------|
| [RapidXML](http://rapidxml.sourceforge.net/) | 1.13, namespace-aware variant with local patches ([details](include/rapidxml/VERSION.md)) |
| [RapidJSON](https://github.com/Tencent/rapidjson) | 1.1.0 plus post-1.1.0 `master` fixes, commit `24b5e7a` (2024-12-22) ([details](include/rapidjson/VERSION.md)) |

RapidJSON has not published a release since 1.1.0 (2016), so the bundled copy
tracks upstream `master`; the `VERSION.md` files record the exact commits, CVE
status and local patches.

## Tests

`test/test.cpp` is a self-contained regression suite (no test framework needed).
It checks the bundled documents against the expected output, plus numeric
detection, malformed input handling, the `xml2json_document()` API, every option
(including the configurable key names), `json2xml` and full round trips.  It also
links a second translation unit that includes the header, which guards against
duplicate symbols.

Run it with `make test`, or `ctest --test-dir build` after a CMake build.  Passing
`--write` regenerates the expected `*.js.txt` files instead of checking them.

The suite is currently verified manually on Windows with MinGW-w64 GCC; the
repository has no CI configuration, so other platforms are expected to work but
are not continuously verified.

## License

MIT, see [LICENSE](LICENSE).

## Donate

![WeChat Pay](https://raw.githubusercontent.com/Cheedoong/xml2json/master/donate/wechatpay_cheedoong.png)
![AliPay](https://raw.githubusercontent.com/Cheedoong/xml2json/master/donate/alipay_cheedoong.jpg)
