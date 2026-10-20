#ifndef XML2JSON_HPP_INCLUDED
#define XML2JSON_HPP_INCLUDED

// Copyright (C) 2015 Alan Zhuang (Cheedoong)	HKUST.  [Updated to the latest version of rapidjson]
// Copyright (C) 2013 Alan Zhuang (Cheedoong)	Tencent, Inc.

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
/* [End]   This part is configurable */

//! Options for the XML to JSON conversion.  The default constructor reproduces
//! the historical behaviour (i.e. it takes its defaults from the compile-time
//! configuration block above) so that existing code keeps working unchanged.
struct xml2json_options
{
    int  indent;                 //!< spaces per level when pretty printing; <= 0 keeps the output compact
    bool numeric;                //!< emit numeric looking text as JSON numbers
    bool order;                  //!< add a "#order" array where sibling order would otherwise be lost
    bool array;                  //!< always emit child elements as JSON arrays
    bool qualify_element_names;  //!< emit element names as "prefix:local" instead of just "local"

    xml2json_options()
        : indent(0)
        , numeric(xml2json_numeric_support)
        , order(xml2json_keep_element_order)
        , array(xml2json_always_array)
        , qualify_element_names(xml2json_qualify_element_names)
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
        jn.SetString((std::string(xml2json_attribute_name_prefix) + myattr->name()).c_str(), allocator);
        xml2json_set_text(jv, myattr->value(), allocator, options.numeric);
        jsvalue.AddMember(jn, jv, allocator);
    }
}

inline void xml2json_traverse_node(rapidxml::xml_node<> *xmlnode, rapidjson::Value &jsvalue, rapidjson::Document::AllocatorType& allocator, const xml2json_options &options)
{
    //cout << "this: " << xmlnode->type() << " name: " << xmlnode->name() << " value: " << xmlnode->value() << endl;
    jsvalue.SetObject();
    rapidxml::xml_node<> *xmlnode_chd;

    // classified discussion:
    if((xmlnode->type() == rapidxml::node_data || xmlnode->type() == rapidxml::node_cdata) && xmlnode->value())
    {
        // case: pure_text
        jsvalue.SetString(xmlnode->value(), allocator);  // then addmember("#text" , jsvalue, allocator)
    }
    else if(xmlnode->type() == rapidxml::node_element)
    {
        if(xmlnode->first_attribute())
        {
            if(xmlnode->first_node() && xmlnode->first_node()->type() == rapidxml::node_data && count_children(xmlnode) == 1)
            {
                // case: <e attr="xxx">text</e>
                rapidjson::Value jn, jv;
                jn.SetString(xml2json_text_additional_name, allocator);
                xml2json_set_text(jv, xmlnode->first_node()->value(), allocator, options.numeric);
                jsvalue.AddMember(jn, jv, allocator);
                xml2json_add_attributes(xmlnode, jsvalue, allocator, options);
                return;
            }
            else
            {
                // case: <e attr="xxx">...</e>
                xml2json_add_attributes(xmlnode, jsvalue, allocator, options);
            }
        }
        else
        {
            if(!xmlnode->first_node())
            {
                // case: <e />
                jsvalue.SetNull();
                return;
            }
            else if(xmlnode->first_node()->type() == rapidxml::node_data && count_children(xmlnode) == 1)
            {
                // case: <e>text</e>
                xml2json_set_text(jsvalue, xmlnode->first_node()->value(), allocator, options.numeric);
                return;
            }
        }
        if(xmlnode->first_node())
        {
            // case: complex else...
            //
            // Collect the children first and emit them at the end.  The old
            // streaming implementation called FindMember() + RemoveMember() +
            // AddMember() for every repeated child, which is quadratic when a
            // document has many distinct names.  RapidJSON's RemoveMember()
            // moves the *last* member into the hole and AddMember() appends, so
            // `slots` mirrors that order in O(1) per child.
            struct Child
            {
                std::string name;
                rapidjson::Value value;
            };

            std::vector<Child> children;                          // child index -> child
            std::unordered_map<std::string, std::size_t> slot_of; // name -> position in slots
            std::vector<std::size_t> slots;                       // position -> child index
            std::vector<std::string> child_order;
            bool duplicate_names = false;

            for(xmlnode_chd = xmlnode->first_node(); xmlnode_chd; xmlnode_chd = xmlnode_chd->next_sibling())
            {
                std::string current_name;
                bool is_element = false;

                if(xmlnode_chd->type() == rapidxml::node_data || xmlnode_chd->type() == rapidxml::node_cdata)
                {
                    current_name = xml2json_text_additional_name;
                }
                else if(xmlnode_chd->type() == rapidxml::node_element)
                {
                    current_name = xml2json_element_name(xmlnode_chd, options);
                    is_element = true;
                }
                else
                {
                    continue; // ignore anything else (comments, PIs, ...); defensive, not produced by parse<0>
                }

                if(options.order)
                    child_order.push_back(current_name);

                rapidjson::Value child_value;
                xml2json_traverse_node(xmlnode_chd, child_value, allocator, options);

                std::unordered_map<std::string, std::size_t>::iterator existing = slot_of.find(current_name);
                if(existing == slot_of.end())
                {
                    if(options.array && is_element)
                    {
                        rapidjson::Value array_value;
                        array_value.SetArray();
                        array_value.PushBack(child_value, allocator);
                        child_value = array_value;
                    }
                    const std::size_t child_idx = children.size();
                    slot_of.insert(std::make_pair(current_name, slots.size()));
                    slots.push_back(child_idx);
                    Child child;
                    child.name = std::move(current_name);
                    child.value = std::move(child_value);
                    children.push_back(std::move(child));
                }
                else
                {
                    duplicate_names = true;
                    const std::size_t slot = existing->second;
                    const std::size_t child_idx = slots[slot];
                    Child &child = children[child_idx];
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
                        child.value = array_value;
                    }

                    // Mirror RemoveMember(): the last slot moves into the hole.
                    const std::size_t last = slots.size() - 1;
                    if(slot != last)
                    {
                        const std::size_t moved_child = slots[last];
                        slots[slot] = moved_child;
                        slot_of[children[moved_child].name] = slot;
                        slots[last] = child_idx;
                        slot_of[current_name] = last;
                    }
                }
            }

            for(std::vector<std::size_t>::const_iterator it = slots.begin(); it != slots.end(); ++it)
            {
                Child &child = children[*it];
                rapidjson::Value jn;
                jn.SetString(child.name.c_str(), static_cast<rapidjson::SizeType>(child.name.size()), allocator);
                jsvalue.AddMember(jn, child.value, allocator);
            }

            if(options.order && duplicate_names)
            {
                rapidjson::Value jn, jorder;
                jn.SetString(xml2json_order_additional_name, allocator);
                jorder.SetArray();
                for(std::vector<std::string>::const_iterator it = child_order.begin(); it != child_order.end(); ++it)
                {
                    rapidjson::Value jitem;
                    jitem.SetString(it->c_str(), allocator);
                    jorder.PushBack(jitem, allocator);
                }
                jsvalue.AddMember(jn, jorder, allocator);
            }
        }
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
inline std::string json2xml_value_text(const rapidjson::Value &value)
{
    if (value.IsArray())
    {
        // "#text" is an array when an element has several text nodes (mixed
        // content); concatenate them so that no text is lost.
        std::string out;
        for(rapidjson::Value::ConstValueIterator it = value.Begin(); it != value.End(); ++it)
            out += json2xml_value_text(*it);
        return out;
    }
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
    return std::string(); // null, object and array are not text
}

inline void json2xml_write_element(const char *name, std::size_t name_len, const rapidjson::Value &value, std::string &out, int indent_width, int depth)
{
    if (value.IsArray())
    {
        // an array is the same element repeated once per item
        for(rapidjson::Value::ConstValueIterator it = value.Begin(); it != value.End(); ++it)
            json2xml_write_element(name, name_len, *it, out, indent_width, depth);
        return;
    }

    const bool pretty = indent_width > 0;
    const std::size_t pad = pretty ? static_cast<std::size_t>(indent_width) * static_cast<std::size_t>(depth) : 0;
    const std::size_t text_name_len = sizeof(xml2json_text_additional_name) - 1;
    const std::size_t order_name_len = sizeof(xml2json_order_additional_name) - 1;

    out.append(pad, ' ');
    out += '<';
    out.append(name, name_len);

    const rapidjson::Value *text = 0;
    int child_count = 0;

    if (value.IsObject())
    {
        for(rapidjson::Value::ConstMemberIterator it = value.MemberBegin(); it != value.MemberEnd(); ++it)
        {
            const char *member = it->name.GetString();
            const std::size_t member_len = it->name.GetStringLength();
            if (member_len > 0 && member[0] == '@')
            {
                const std::string attribute_value = json2xml_value_text(it->value);
                out += ' ';
                out.append(member + 1, member_len - 1);
                out += "=\"";
                out += json2xml_escape(attribute_value.c_str(), attribute_value.size(), true);
                out += '"';
            }
            else if (member_len == text_name_len && std::memcmp(member, xml2json_text_additional_name, text_name_len) == 0)
            {
                if (it->value.IsNull() == false)
                    text = &it->value;
            }
            else if (member_len == order_name_len && std::memcmp(member, xml2json_order_additional_name, order_name_len) == 0)
            {
                // ordering information emitted by xml2json: not real content
            }
            else
            {
                ++child_count;
            }
        }
    }
    else if (value.IsNull() == false)
    {
        text = &value;
    }

    if (child_count == 0 && text == 0)
    {
        out += "/>";
        if (pretty)
            out += '\n';
        return;
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
        for(rapidjson::Value::ConstMemberIterator it = value.MemberBegin(); it != value.MemberEnd(); ++it)
        {
            const char *member = it->name.GetString();
            const std::size_t member_len = it->name.GetStringLength();
            if (member_len > 0 && member[0] == '@')
                continue;
            if (member_len == text_name_len && std::memcmp(member, xml2json_text_additional_name, text_name_len) == 0)
                continue;
            if (member_len == order_name_len && std::memcmp(member, xml2json_order_additional_name, order_name_len) == 0)
                continue;
            json2xml_write_element(member, member_len, it->value, out, indent_width, depth + 1);
        }
        out.append(pad, ' ');
    }

    out += "</";
    out.append(name, name_len);
    out += '>';
    if (pretty)
        out += '\n';
}

inline std::string json2xml(const char *json_str, int indent_width)
{
    if (json_str == nullptr)
        return std::string();

    rapidjson::Document js_doc;
    js_doc.Parse(json_str);
    if (js_doc.HasParseError())
        throw std::runtime_error(std::string("json2xml: invalid JSON at offset ") +
                                 std::to_string(static_cast<long long>(js_doc.GetErrorOffset())) +
                                 ": " + rapidjson::GetParseError_En(js_doc.GetParseError()));

    std::string out;
    out.reserve(std::strlen(json_str));
    if (js_doc.IsObject())
    {
        for(rapidjson::Value::ConstMemberIterator it = js_doc.MemberBegin(); it != js_doc.MemberEnd(); ++it)
            json2xml_write_element(it->name.GetString(), it->name.GetStringLength(), it->value, out, indent_width, 0);
    }
    else if (js_doc.IsNull() == false)
    {
        json2xml_write_element("root", 4, js_doc, out, indent_width, 0); // a bare array/scalar needs a name
    }

    if (indent_width > 0 && out.empty() == false && out[out.size() - 1] == '\n')
        out.erase(out.size() - 1);

    return out;
}

inline std::string json2xml(const char *json_str)
{
    return json2xml(json_str, 0);
}

#endif
