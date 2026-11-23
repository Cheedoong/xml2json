#ifndef XML2JSON_HPP_INCLUDED
#define XML2JSON_HPP_INCLUDED

// Copyright (C) 2015 Alan Zhuang (Cheedoong)	HKUST.  [Updated to the latest version of rapidjson]
// Copyright (C) 2013 Alan Zhuang (Cheedoong)	Tencent, Inc.

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_utils.hpp"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "rapidjson/error/en.h"

/* [Start] This part is configurable */
static const char xml2json_text_additional_name[] = "#text";
static const char xml2json_attribute_name_prefix[] = "@";
static const char xml2json_order_additional_name[] = "#order";
/* Example:
   <node_name attribute_name="attribute_value">value</node_name> ---> "node_name":{"#text":"value","@attribute_name":"attribute_value"}
*/
static const bool xml2json_numeric_support = false;
/* Example:
   xml2json_numeric_support = false:
   <number>26.026</number>  ---> "number":"26.026"
   xml2json_numeric_support = true:
   <number>26.026</number>  ---> "number":26.026
*/
static const bool xml2json_keep_element_order = false;
/* Example (xml2json_keep_element_order = true):
   <a><b/><c/><b/></a> ---> "a":{"b":[...],"c":{...},"#order":["b","c","b"]}
   Child elements with the same name are regrouped into one array, which loses
   the original sequence.  When enabled, a "#order" array records the order of
   the child nodes, but only for nodes where such a regrouping actually happens.
*/
static const bool xml2json_always_array = false;
/* Example (xml2json_always_array = true):
   <a><b/><c/></a> ---> "a":{"b":[...],"c":[...]}
   Every child element becomes a JSON array, even when it occurs only once, so
   that look-ups have the same shape for one or many children.
*/
static const bool xml2json_qualify_element_names = false;
/* Example (xml2json_qualify_element_names = true):
   <ns:e xmlns:ns="urn:x"><ns:b>1</ns:b></ns:e> ---> "ns:e":{"ns:b":"1"}
   The bundled rapidxml is namespace aware: for an element it keeps the local
   name in name() and the prefix separately (xml_node::prefix(), with the URI
   resolved through xml_node::xmlns()).  Because only the local name is used by
   default, the "ns:" of <ns:e> does not show up in the output (issue #12);
   enable this option to emit the qualified "prefix:local" name instead.
   Attribute names keep their prefix in either case.
*/
static const std::size_t xml2json_default_max_xml_depth = 1024;
/* XML nesting deeper than this is rejected before RapidXML's recursive parser
   runs.  The default is deliberately conservative because the bundled parser is
   still recursive.  Set it to 0 to disable the guard (deeper input can overflow
   the call stack).
*/
/* [End]   This part is configurable */

//! Options for the XML to JSON conversion.  The default constructor takes its
//! defaults from the compile-time configuration block above.  Apart from the
//! deliberate max_xml_depth safety guard, it reproduces the historical
//! behaviour so that existing code keeps working unchanged.
struct xml2json_options
{
    int  indent;                 //!< spaces per level when pretty printing; <= 0 keeps the output compact
    bool numeric;                //!< emit numeric looking text as JSON numbers
    bool order;                  //!< add a "#order" array where sibling order would otherwise be lost
    bool array;                  //!< always emit child elements as JSON arrays
    bool qualify_element_names;  //!< emit element names as "prefix:local" instead of just "local"
    //! Maximum XML element nesting depth; 0 disables the guard.  XML deeper than
    //! this is rejected before the recursive RapidXML parser runs.
    std::size_t max_xml_depth;

    //! JSON member name used for an element's own text (default "#text").
    //! It must not be empty.
    std::string text_name;
    //! Prefix prepended to attribute names (default "@").  It may be empty,
    //! in which case attributes are written without any prefix; note that they
    //! can then collide with child element names of the same spelling.
    std::string attribute_prefix;
    //! JSON member name used for the sibling order array (default "#order").
    //! Only emitted when the `order' option is on.  An empty name means
    //! "do not emit the order array".
    std::string order_name;

    xml2json_options()
        : indent(0)
        , numeric(xml2json_numeric_support)
        , order(xml2json_keep_element_order)
        , array(xml2json_always_array)
        , qualify_element_names(xml2json_qualify_element_names)
        , max_xml_depth(xml2json_default_max_xml_depth)
        , text_name(xml2json_text_additional_name)
        , attribute_prefix(xml2json_attribute_name_prefix)
        , order_name(xml2json_order_additional_name)
    {}
};

// Avoided any namespace pollution.
//
// Returns true only if the *whole* string is a well-formed decimal number,
// e.g. "123", "-123", "+1.5", "0.5", "5.".  Empty strings, surrounding or
// embedded whitespace, embedded separators ("99921-58-10-7"), hexadecimal /
// exponent forms ("0x10", "1e5") and any other trailing junk are rejected,
// so that such values remain JSON strings instead of being silently truncated.
inline bool xml2json_has_digits_only(const char * input, bool *hasDecimal)
{
    *hasDecimal = false;

    if (input == nullptr)
        return false;  // treat empty input as a string (probably will be an empty string)

    const char * runPtr = input;

    if (*runPtr == '+' || *runPtr == '-') // optional leading sign
        runPtr++;

    bool hasDigit = false;
    while (*runPtr != '\0')
    {
        if (*runPtr == '.')
        {
            if (*hasDecimal)
                return false; // we found two dots - not a number
            *hasDecimal = true;
        }
        else if (isdigit(static_cast<unsigned char>(*runPtr)))
        {
            hasDigit = true;
        }
        else
        {
            return false; // any other character (letter, sign, space, ...) - not a number
        }
        runPtr++;
    }

    return hasDigit;
}

// Stores a piece of XML text into @jsvalue.  When numeric support is enabled and
// the whole text is numeric it becomes a JSON number, otherwise it stays a JSON
// string.  Integer values use strtoll() so 64-bit (and negative) values are not
// truncated, falling back to double only on overflow.
inline void xml2json_set_text(rapidjson::Value &jsvalue, const char *text, rapidjson::Document::AllocatorType& allocator, bool numeric)
{
    if (text == nullptr)
    {
        jsvalue.SetNull();
        return;
    }

    bool hasDecimal = false;
    if (numeric == false || xml2json_has_digits_only(text, &hasDecimal) == false)
    {
        jsvalue.SetString(text, allocator);
        return;
    }

    // strtod() reports overflow and underflow through ERANGE.  A value that does
    // not fit into a double becomes infinity, and RapidJSON's writer refuses to
    // serialise infinity: Accept() would stop in the middle of the document and
    // leave truncated JSON behind.  Such text therefore stays a string.
    if (hasDecimal)
    {
        errno = 0;
        const double value = std::strtod(text, nullptr);
        if (errno == ERANGE || std::isfinite(value) == false)
            jsvalue.SetString(text, allocator);
        else
            jsvalue.SetDouble(value);
        return;
    }

    errno = 0;
    const long long value = std::strtoll(text, nullptr, 10);
    if (errno != ERANGE)
    {
        jsvalue.SetInt64(value);
        return;
    }

    // Out of long long range: fall back to double, but keep the original text
    // when even that would overflow (see above).
    errno = 0;
    const double fallback = std::strtod(text, nullptr);
    if (errno == ERANGE || std::isfinite(fallback) == false)
        jsvalue.SetString(text, allocator);
    else
        jsvalue.SetDouble(fallback);
}

// Element name as it should appear in the JSON: the local name by default, or
// the qualified "prefix:local" name when qualify_element_names is enabled.
inline std::string xml2json_element_name(rapidxml::xml_node<> *xmlnode, const xml2json_options &options)
{
    if (options.qualify_element_names && xmlnode->prefix() && xmlnode->prefix_size() > 0)
        return std::string(xmlnode->prefix(), xmlnode->prefix_size()) + ":" + xmlnode->name();
    return std::string(xmlnode->name());
}

inline void xml2json_add_attributes(rapidxml::xml_node<> *xmlnode, rapidjson::Value &jsvalue, rapidjson::Document::AllocatorType& allocator, const xml2json_options &options)
{
    rapidxml::xml_attribute<> *myattr;
    for(myattr = xmlnode->first_attribute(); myattr; myattr = myattr->next_attribute())
    {
        rapidjson::Value jn, jv;
        jn.SetString((options.attribute_prefix + myattr->name()).c_str(), allocator);
        xml2json_set_text(jv, myattr->value(), allocator, options.numeric);
        jsvalue.AddMember(jn, jv, allocator);
    }
}

inline void xml2json_traverse_node(rapidxml::xml_node<> *xmlnode, rapidjson::Value &jsvalue, rapidjson::Document::AllocatorType& allocator, const xml2json_options &options)
{
    // The traversal used to call itself for every child.  Keep the same
    // post-order assembly, but make the call stack explicit: a Frame owns the
    // result and the child-grouping state of one XML node.
    struct Child
    {
        std::string name;
        rapidjson::Value value;
    };

    struct Frame
    {
        rapidxml::xml_node<> *node;
        std::string name;
        bool is_element;
        rapidjson::Value value;
        rapidxml::xml_node<> *next_child;
        std::vector<Child> children;                          // child index -> child
        std::unordered_map<std::string, std::size_t> slot_of; // name -> position in slots
        std::vector<std::size_t> slots;                       // position -> child index
        std::vector<std::string> child_order;
        bool duplicate_names;
        bool done;

        Frame()
            : node(0)
            , is_element(false)
            , next_child(0)
            , duplicate_names(false)
            , done(false)
        {}
    };

    // Handles every terminal case without descending into children.  The
    // conditions deliberately mirror the old recursive implementation.
    const auto init_frame = [&](Frame &frame)
    {
        rapidxml::xml_node<> *node = frame.node;
        frame.value.SetObject();

        if((node->type() == rapidxml::node_data || node->type() == rapidxml::node_cdata) && node->value())
        {
            // case: pure_text
            frame.value.SetString(node->value(), allocator);
            frame.done = true;
            return;
        }

        if(node->type() != rapidxml::node_element)
        {
            frame.done = true;
            return;
        }

        if(node->first_attribute())
        {
            if(node->first_node() && node->first_node()->type() == rapidxml::node_data && count_children(node) == 1)
            {
                // case: <e attr="xxx">text</e>
                rapidjson::Value jn, jv;
                jn.SetString(options.text_name.c_str(), allocator);
                xml2json_set_text(jv, node->first_node()->value(), allocator, options.numeric);
                frame.value.AddMember(jn, jv, allocator);
                xml2json_add_attributes(node, frame.value, allocator, options);
                frame.done = true;
                return;
            }
            // case: <e attr="xxx">...</e>
            xml2json_add_attributes(node, frame.value, allocator, options);
        }
        else
        {
            if(!node->first_node())
            {
                // case: <e />
                frame.value.SetNull();
                frame.done = true;
                return;
            }
            if(node->first_node()->type() == rapidxml::node_data && count_children(node) == 1)
            {
                // case: <e>text</e>
                xml2json_set_text(frame.value, node->first_node()->value(), allocator, options.numeric);
                frame.done = true;
                return;
            }
        }

        frame.next_child = node->first_node();
    };

    // Merges a completed child into its parent, preserving the old
    // RemoveMember()-style ordering for repeated names.
    const auto add_child = [&](Frame &frame, std::string name, bool is_element, rapidjson::Value &child_value)
    {
        std::unordered_map<std::string, std::size_t>::iterator existing = frame.slot_of.find(name);
        if(existing == frame.slot_of.end())
        {
            if(options.array && is_element)
            {
                rapidjson::Value array_value;
                array_value.SetArray();
                array_value.PushBack(child_value, allocator);
                child_value = std::move(array_value);
            }
            const std::size_t child_idx = frame.children.size();
            frame.slot_of.insert(std::make_pair(name, frame.slots.size()));
            frame.slots.push_back(child_idx);
            Child child;
            child.name = std::move(name);
            child.value = std::move(child_value);
            frame.children.push_back(std::move(child));
        }
        else
        {
            frame.duplicate_names = true;
            const std::size_t slot = existing->second;
            const std::size_t child_idx = frame.slots[slot];
            Child &child = frame.children[child_idx];
            if(child.value.IsArray())
            {
                child.value.PushBack(child_value, allocator);
            }
            else
            {
                rapidjson::Value array_value;
                array_value.SetArray();
                array_value.PushBack(child.value, allocator);
                array_value.PushBack(child_value, allocator);
                child.value = std::move(array_value);
            }

            // Mirror RemoveMember(): the last slot moves into the hole.
            const std::size_t last = frame.slots.size() - 1;
            if(slot != last)
            {
                const std::size_t moved_child = frame.slots[last];
                frame.slots[slot] = moved_child;
                frame.slot_of[frame.children[moved_child].name] = slot;
                frame.slots[last] = child_idx;
                frame.slot_of[name] = last;
            }
        }
    };

    // Emits the grouped children and the optional #order array.
    const auto finish_frame = [&](Frame &frame)
    {
        for(std::vector<std::size_t>::const_iterator it = frame.slots.begin(); it != frame.slots.end(); ++it)
        {
            Child &child = frame.children[*it];
            rapidjson::Value jn;
            jn.SetString(child.name.c_str(), static_cast<rapidjson::SizeType>(child.name.size()), allocator);
            frame.value.AddMember(jn, child.value, allocator);
        }

        if(options.order && frame.duplicate_names && options.order_name.empty() == false)
        {
            rapidjson::Value jn, jorder;
            jn.SetString(options.order_name.c_str(), allocator);
            jorder.SetArray();
            for(std::vector<std::string>::const_iterator it = frame.child_order.begin(); it != frame.child_order.end(); ++it)
            {
                rapidjson::Value jitem;
                jitem.SetString(it->c_str(), allocator);
                jorder.PushBack(jitem, allocator);
            }
            frame.value.AddMember(jn, jorder, allocator);
        }

        frame.done = true;
    };

    std::vector<Frame> stack;
    stack.reserve(64);

    Frame root;
    root.node = xmlnode;
    root.is_element = (xmlnode->type() == rapidxml::node_element);
    init_frame(root);
    stack.push_back(std::move(root));

    while(stack.empty() == false)
    {
        Frame &frame = stack.back();

        if(frame.done)
        {
            if(stack.size() == 1)
            {
                jsvalue = std::move(frame.value);
                stack.pop_back();
                break;
            }

            Frame &parent = stack[stack.size() - 2];
            parent.next_child = frame.node->next_sibling();

            std::string name = std::move(frame.name);
            const bool is_element = frame.is_element;
            rapidjson::Value child_value = std::move(frame.value);
            stack.pop_back();
            add_child(parent, std::move(name), is_element, child_value);
            continue;
        }

        if(frame.next_child == 0)
        {
            finish_frame(frame);
            continue;
        }

        rapidxml::xml_node<> *child = frame.next_child;
        std::string current_name;
        bool is_element = false;

        if(child->type() == rapidxml::node_data || child->type() == rapidxml::node_cdata)
        {
            current_name = options.text_name;
        }
        else if(child->type() == rapidxml::node_element)
        {
            current_name = xml2json_element_name(child, options);
            is_element = true;
        }
        else
        {
            // Ignore anything else (comments, PIs, ...); defensive, not
            // produced by parse<0>.
            frame.next_child = child->next_sibling();
            continue;
        }

        if(options.order)
            frame.child_order.push_back(current_name);

        Frame child_frame;
        child_frame.node = child;
        child_frame.name = std::move(current_name);
        child_frame.is_element = is_element;
        init_frame(child_frame);
        stack.push_back(std::move(child_frame));
    }
}

// Checks XML element nesting before RapidXML parses the document.  RapidXML is
// still recursive, so this cheap iterative scan is the last line of defence
// against a parser stack overflow.  Comments, CDATA sections,
// processing instructions, DOCTYPE internal subsets and quoted attribute values
// are skipped, and self-closing elements do not increase the depth.
//
// A limit of 0 disables the check.  Malformed XML is left to RapidXML, which
// reports its own parse_error; the scan only needs to be good enough to count
// element starts in well-formed input.
inline void xml2json_check_depth(const char *text, std::size_t max_depth)
{
    if (text == nullptr || max_depth == 0)
        return;

    std::size_t depth = 0;
    const char *p = text;

    while (*p != '\0')
    {
        if (*p != '<')
        {
            ++p;
            continue;
        }

        if (p[1] == '!')
        {
            if (std::strncmp(p, "<!--", 4) == 0)
            {
                const char *end = std::strstr(p + 2, "-->");
                if (end == nullptr)
                    return; // unterminated: RapidXML will reject it
                p = end + 3;
                continue;
            }

            if (std::strncmp(p, "<![CDATA[", 9) == 0)
            {
                const char *end = std::strstr(p + 2, "]]>");
                if (end == nullptr)
                    return; // unterminated: RapidXML will reject it
                p = end + 3;
                continue;
            }

            // <!DOCTYPE ...> may contain an internal subset in [...].  Ignore
            // any '>' inside the subset or inside a quoted literal.
            const char *q = p + 2;
            std::size_t bracket_depth = 0;
            char quote = '\0';
            while (*q != '\0')
            {
                const char c = *q;
                if (quote != '\0')
                {
                    if (c == quote)
                        quote = '\0';
                }
                else if (c == '"' || c == '\'')
                {
                    quote = c;
                }
                else if (c == '[')
                {
                    ++bracket_depth;
                }
                else if (c == ']' && bracket_depth > 0)
                {
                    --bracket_depth;
                }
                else if (c == '>' && bracket_depth == 0)
                {
                    ++q;
                    break;
                }
                ++q;
            }
            p = q;
            continue;
        }

        if (p[1] == '?')
        {
            const char *end = std::strstr(p + 2, "?>");
            if (end == nullptr)
                return; // unterminated: RapidXML will reject it
            p = end + 2;
            continue;
        }

        if (p[1] == '/')
        {
            const char *end = std::strchr(p + 2, '>');
            if (end == nullptr)
                return; // unterminated: RapidXML will reject it
            if (depth > 0)
                --depth;
            p = end + 1;
            continue;
        }

        // Opening tag: find the closing '>' outside quoted attribute values.
        const char *q = p + 1;
        char quote = '\0';
        while (*q != '\0')
        {
            const char c = *q;
            if (quote != '\0')
            {
                if (c == quote)
                    quote = '\0';
            }
            else if (c == '"' || c == '\'')
            {
                quote = c;
            }
            else if (c == '>')
            {
                break;
            }
            ++q;
        }

        if (*q == '\0')
            return; // unterminated: RapidXML will reject it

        // A tag is self-closing when the last non-space character before '>'
        // is '/'.  Such a tag does not add a level.
        const char *last = q;
        while (last > p + 1 && (last[-1] == ' ' || last[-1] == '\t' || last[-1] == '\r' || last[-1] == '\n'))
            --last;
        const bool self_closing = last > p + 1 && last[-1] == '/';

        if (self_closing == false)
        {
            ++depth;
            if (depth > max_depth)
                throw std::runtime_error("xml2json: XML nesting depth exceeds " + std::to_string(max_depth));
        }

        p = q + 1;
    }
}

//! Parses @xml_str and returns the resulting rapidjson document, so that the
//! JSON can be inspected directly instead of going through a string.
inline rapidjson::Document xml2json_document(const char *xml_str, const xml2json_options &options)
{
    rapidjson::Document js_doc;
    js_doc.SetObject();

    if (xml_str == nullptr)
        return js_doc;

    // Reject excessively deep XML before RapidXML's recursive parser starts.
    xml2json_check_depth(xml_str, options.max_xml_depth);

    // RapidXML parses in place and therefore needs a writable, NUL-terminated
    // buffer.  Copy the caller's input, which is const and may live in
    // read-only memory (a string literal, std::string::c_str(), ...); the old
    // const_cast<char*> version wrote into it and crashed (access violation)
    // whenever a string literal was passed in.
    std::string xml_buffer(xml_str);

    // rapidxml::xml_document<> embeds a RAPIDXML_STATIC_POOL_SIZE (64 KiB)
    // buffer, so a local instance would add ~64 KiB to this function's stack
    // frame (issue #5).  Keep the document on the heap instead; it is
    // destroyed when the function returns, after every name and value has
    // been copied into js_doc's allocator.
    std::unique_ptr<rapidxml::xml_document<> > xml_doc(new rapidxml::xml_document<>());
    xml_doc->parse<0>(&xml_buffer[0]);

    rapidjson::Document::AllocatorType& allocator = js_doc.GetAllocator();

    for(rapidxml::xml_node<> *xmlnode_chd = xml_doc->first_node(); xmlnode_chd; xmlnode_chd = xmlnode_chd->next_sibling())
    {
        if(xmlnode_chd->type() != rapidxml::node_element)
            continue; // ignore non-element nodes at the top level

        rapidjson::Value jsvalue_chd;
        jsvalue_chd.SetObject();
        xml2json_traverse_node(xmlnode_chd, jsvalue_chd, allocator, options);

        rapidjson::Value jn;
        jn.SetString(xml2json_element_name(xmlnode_chd, options).c_str(), allocator);
        js_doc.AddMember(jn, jsvalue_chd, allocator);
    }

    return js_doc;
}

//! Serialises the document returned by xml2json_document() back to a string.
inline std::string xml2json(const char *xml_str, const xml2json_options &options)
{
    rapidjson::Document js_doc = xml2json_document(xml_str, options);

    rapidjson::StringBuffer buffer;
    bool serialised = false;
    if(options.indent > 0)
    {
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        writer.SetIndent(' ', static_cast<unsigned>(options.indent));
        serialised = js_doc.Accept(writer);
    }
    else
    {
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        serialised = js_doc.Accept(writer);
    }
    if (serialised == false)
        throw std::runtime_error("xml2json: failed to serialise the JSON output");
    return std::string(buffer.GetString(), buffer.GetSize());
}

//! Convenience overload: pretty print with @indent spaces per level when
//! @indent > 0, otherwise behave exactly like xml2json(xml_str).
inline std::string xml2json(const char *xml_str, int indent)
{
    xml2json_options options;
    options.indent = indent;
    return xml2json(xml_str, options);
}

inline std::string xml2json(const char *xml_str)
{
    return xml2json(xml_str, xml2json_options());
}

/* [Start] JSON -> XML (issue #14) */

// json2xml() is the (best effort) inverse of xml2json() and follows the very
// same conventions, so that the two compositions round-trip:
//   * an object member whose name starts with '@' becomes an XML attribute,
//   * the member "#text" becomes the text content of the element,
//   * every other member becomes a child element,
//   * an array becomes the element repeated once per item,
//   * "#order" (produced by xml2json_options::order) is ignored.
// The top level is expected to be an object, as produced by xml2json(); each of
// its members becomes one root element.  No XML declaration is emitted, which is
// what makes xml2json(json2xml(json)) round-trip.  A bare array/scalar at the
// top level is wrapped in an element named "root".
//
// Limitation: XML with mixed content (an element that has both text and element
// children, e.g. "<p>a<b/>c</p>") cannot round-trip exactly, because xml2json
// already loses the relative position of the text: it collects the text nodes
// into a single "#text" member.  json2xml therefore keeps all the text (it
// concatenates a "#text" array) but places it before the child elements.
//
// Throws std::runtime_error when the JSON cannot be parsed.
/* [End]   JSON -> XML */

inline std::string json2xml_escape(const char *text, std::size_t size, bool attribute)
{
    std::string out;
    out.reserve(size);

    // Copy runs of ordinary characters in one go instead of appending them
    // one by one; most text and attribute values contain no markup at all.
    std::size_t run_start = 0;
    for(std::size_t i = 0; i < size; ++i)
    {
        const char c = text[i];
        const bool escape = c == '&' || c == '<' || c == '>' || (attribute && c == '"');
        if(escape == false)
            continue;

        out.append(text + run_start, i - run_start);
        switch(c)
        {
        case '&': out += "&amp;";  break;
        case '<': out += "&lt;";   break;
        case '>': out += "&gt;";   break;
        case '"': out += "&quot;"; break;
        default:  break; // unreachable: only the characters above are escaped
        }
        run_start = i + 1;
    }
    out.append(text + run_start, size - run_start);
    return out;
}

// Text representation of a JSON scalar.  Numbers are rendered through
// rapidjson's own writer so that they keep their exact representation.
// Nested arrays are flattened with an explicit vector-backed stack, so a
// deeply nested "#text" value cannot overflow the call stack.
inline std::string json2xml_value_text(const rapidjson::Value &value)
{
    // The common case is a single scalar.  Avoid allocating a stack for it.
    if (value.IsArray() == false)
    {
        if (value.IsString())
            return std::string(value.GetString(), value.GetStringLength());
        if (value.IsBool())
            return value.GetBool() ? std::string("true") : std::string("false");
        if (value.IsNumber())
        {
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            value.Accept(writer);
            return std::string(buffer.GetString(), buffer.GetSize());
        }
        return std::string(); // null and objects are not text
    }

    // "#text" is an array when an element has several text nodes (mixed
    // content); flatten nested arrays with an explicit LIFO stack.
    std::string out;
    std::vector<const rapidjson::Value *> pending;
    pending.push_back(&value);

    while (pending.empty() == false)
    {
        const rapidjson::Value &current = *pending.back();
        pending.pop_back();

        if (current.IsArray())
        {
            // Push in reverse order so that the first array item is processed
            // first.
            for(rapidjson::Value::ConstValueIterator it = current.End(); it != current.Begin();)
            {
                --it;
                pending.push_back(&*it);
            }
        }
        else if (current.IsString())
        {
            out.append(current.GetString(), current.GetStringLength());
        }
        else if (current.IsBool())
        {
            out += (current.GetBool() ? "true" : "false");
        }
        else if (current.IsNumber())
        {
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            current.Accept(writer);
            out.append(buffer.GetString(), buffer.GetSize());
        }
        // null and objects are not text
    }

    return out;
}

// True when a JSON member name denotes an XML attribute, i.e. it starts with
// the configured prefix.  With an empty prefix nothing is an attribute: the
// caller cannot distinguish attributes from children, so every member is
// written as a child element instead of being silently dropped.
inline bool json2xml_member_is_attribute(const char *member, std::size_t length, const std::string &prefix)
{
    return prefix.empty() == false
        && length >= prefix.size()
        && std::memcmp(member, prefix.c_str(), prefix.size()) == 0;
}

inline void json2xml_write_element(const char *name, std::size_t name_len, const rapidjson::Value &value, std::string &out, const xml2json_options &options, int depth)
{
    // Each work item is either an element to write or the closing tag of an
    // element whose children have already been queued.  The vector is used as a
    // LIFO stack, so the output order stays identical to the old recursive
    // implementation without consuming the call stack.
    struct Task
    {
        bool close;
        const rapidjson::Value *value;
        const char *name;
        std::size_t name_len;
        int depth;
    };

    std::vector<Task> pending;
    pending.reserve(32);

    Task root = { false, &value, name, name_len, depth };
    pending.push_back(root);

    const bool pretty = options.indent > 0;
    const std::size_t text_name_len = options.text_name.size();
    const std::size_t order_name_len = options.order_name.size();

    while (pending.empty() == false)
    {
        const Task task = pending.back();
        pending.pop_back();

        if (task.close)
        {
            const std::size_t pad = pretty ? static_cast<std::size_t>(options.indent) * static_cast<std::size_t>(task.depth) : 0;
            out.append(pad, ' ');
            out += "</";
            out.append(task.name, task.name_len);
            out += '>';
            if (pretty)
                out += '\n';
            continue;
        }

        const rapidjson::Value &element = *task.value;

        if (element.IsArray())
        {
            // An array is the same element repeated once per item.  Push the
            // items in reverse order so that they are emitted in original order.
            for(rapidjson::Value::ConstValueIterator it = element.End(); it != element.Begin();)
            {
                --it;
                Task item = { false, &*it, task.name, task.name_len, task.depth };
                pending.push_back(item);
            }
            continue;
        }

        const std::size_t pad = pretty ? static_cast<std::size_t>(options.indent) * static_cast<std::size_t>(task.depth) : 0;

        out.append(pad, ' ');
        out += '<';
        out.append(task.name, task.name_len);

        const rapidjson::Value *text = 0;
        int child_count = 0;

        if (element.IsObject())
        {
            for(rapidjson::Value::ConstMemberIterator it = element.MemberBegin(); it != element.MemberEnd(); ++it)
            {
                const char *member = it->name.GetString();
                const std::size_t member_len = it->name.GetStringLength();
                if (json2xml_member_is_attribute(member, member_len, options.attribute_prefix))
                {
                    const std::string attribute_value = json2xml_value_text(it->value);
                    const std::size_t prefix_len = options.attribute_prefix.size();
                    out += ' ';
                    out.append(member + prefix_len, member_len - prefix_len);
                    out += "=\"";
                    out += json2xml_escape(attribute_value.c_str(), attribute_value.size(), true);
                    out += '"';
                }
                else if (text_name_len > 0 && member_len == text_name_len && std::memcmp(member, options.text_name.c_str(), text_name_len) == 0)
                {
                    if (it->value.IsNull() == false)
                        text = &it->value;
                }
                else if (order_name_len > 0 && member_len == order_name_len && std::memcmp(member, options.order_name.c_str(), order_name_len) == 0)
                {
                    // ordering information emitted by xml2json: not real content
                }
                else
                {
                    ++child_count;
                }
            }
        }
        else if (element.IsNull() == false)
        {
            text = &element;
        }

        if (child_count == 0 && text == 0)
        {
            out += "/>";
            if (pretty)
                out += '\n';
            continue;
        }

        out += '>';
        if (text)
        {
            const std::string text_value = json2xml_value_text(*text);
            out += json2xml_escape(text_value.c_str(), text_value.size(), false);
        }

        if (child_count > 0)
        {
            if (pretty)
                out += '\n';

            // The close marker must run after every child, so push it first and
            // then put the children on top of it.
            Task close = { true, 0, task.name, task.name_len, task.depth };
            pending.push_back(close);

            // Walk the members backwards to preserve their original order when
            // they are popped from the stack.
            for(rapidjson::Value::ConstMemberIterator it = element.MemberEnd(); it != element.MemberBegin();)
            {
                --it;
                const char *member = it->name.GetString();
                const std::size_t member_len = it->name.GetStringLength();
                if (json2xml_member_is_attribute(member, member_len, options.attribute_prefix))
                    continue;
                if (text_name_len > 0 && member_len == text_name_len && std::memcmp(member, options.text_name.c_str(), text_name_len) == 0)
                    continue;
                if (order_name_len > 0 && member_len == order_name_len && std::memcmp(member, options.order_name.c_str(), order_name_len) == 0)
                    continue;

                Task child = { false, &it->value, member, member_len, task.depth + 1 };
                pending.push_back(child);
            }
        }
        else
        {
            // Text without children is written inline, exactly like before.
            out += "</";
            out.append(task.name, task.name_len);
            out += '>';
            if (pretty)
                out += '\n';
        }
    }
}

inline std::string json2xml(const char *json_str, const xml2json_options &options)
{
    if (json_str == nullptr)
        return std::string();

    rapidjson::Document js_doc;
    js_doc.Parse<rapidjson::kParseIterativeFlag>(json_str);
    if (js_doc.HasParseError())
        throw std::runtime_error(std::string("json2xml: invalid JSON at offset ") +
                                 std::to_string(static_cast<long long>(js_doc.GetErrorOffset())) +
                                 ": " + rapidjson::GetParseError_En(js_doc.GetParseError()));

    std::string out;
    out.reserve(std::strlen(json_str));
    if (js_doc.IsObject())
    {
        for(rapidjson::Value::ConstMemberIterator it = js_doc.MemberBegin(); it != js_doc.MemberEnd(); ++it)
            json2xml_write_element(it->name.GetString(), it->name.GetStringLength(), it->value, out, options, 0);
    }
    else if (js_doc.IsNull() == false)
    {
        json2xml_write_element("root", 4, js_doc, out, options, 0); // a bare array/scalar needs a name
    }

    if (options.indent > 0 && out.empty() == false && out[out.size() - 1] == '\n')
        out.erase(out.size() - 1);

    return out;
}

inline std::string json2xml(const char *json_str, int indent_width)
{
    xml2json_options options;
    options.indent = indent_width;
    return json2xml(json_str, options);
}

inline std::string json2xml(const char *json_str)
{
    return json2xml(json_str, 0);
}

#endif
