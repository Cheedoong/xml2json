#ifndef XML2JSON_HPP_INCLUDED
#define XML2JSON_HPP_INCLUDED

// Copyright (C) 2015 Alan Zhuang (Cheedoong)	HKUST.  [Updated to the latest version of rapidjson]
// Copyright (C) 2013 Alan Zhuang (Cheedoong)	Tencent, Inc.

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_utils.hpp"
#include "rapidxml/rapidxml_print.hpp"

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/encodedstream.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/reader.h"
#include "rapidjson/writer.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
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

    if (hasDecimal)
    {
        jsvalue.SetDouble(std::strtod(text, nullptr));
        return;
    }

    errno = 0;
    long long value = std::strtoll(text, nullptr, 10);
    if (errno == ERANGE)
        jsvalue.SetDouble(std::strtod(text, nullptr)); // out of long long range - keep full value
    else
        jsvalue.SetInt64(value);
}

// Element name as it should appear in the JSON: the local name by default, or
// the qualified "prefix:local" name when qualify_element_names is enabled.
inline std::string xml2json_element_name(rapidxml::xml_node<> *xmlnode, const xml2json_options &options)
{
    if (options.qualify_element_names && xmlnode->prefix() && xmlnode->prefix_size() > 0)
        return std::string(xmlnode->prefix(), xmlnode->prefix_size()) + ":" + xmlnode->name();
    return std::string(xmlnode->name());
}

inline void xml2json_to_array_form(const char *name, rapidjson::Value &jsvalue, rapidjson::Value &jsvalue_chd, rapidjson::Document::AllocatorType& allocator)
{
    rapidjson::Value jn;             // this is a must, partially because of the latest version of rapidjson
    jn.SetString(name, allocator);

    rapidjson::Value::MemberIterator existing = jsvalue.FindMember(name);
    if (existing == jsvalue.MemberEnd())
    {
        // first occurrence of this name (only reachable with xml2json_always_array):
        // start a fresh array instead of dereferencing a missing member.
        rapidjson::Value jsvalue_array;
        jsvalue_array.SetArray();
        jsvalue_array.PushBack(jsvalue_chd, allocator);
        jsvalue.AddMember(jn, jsvalue_array, allocator);
        return;
    }

    rapidjson::Value jsvalue_target; // target to do some operation
    jsvalue_target = existing->value;
    if(jsvalue_target.IsArray())
    {
        jsvalue_target.PushBack(jsvalue_chd, allocator);
        jsvalue.RemoveMember(name);
        jsvalue.AddMember(jn, jsvalue_target, allocator);
    }
    else
    {
        rapidjson::Value jsvalue_array;
        //jsvalue_array = jsvalue_target;
        jsvalue_array.SetArray();
        jsvalue_array.PushBack(jsvalue_target, allocator);
        jsvalue_array.PushBack(jsvalue_chd, allocator);
        jsvalue.RemoveMember(name);
        jsvalue.AddMember(jn, jsvalue_array, allocator);
    }
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
    rapidjson::Value jsvalue_chd;

    jsvalue.SetObject();
    jsvalue_chd.SetObject();
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
            std::map<std::string, int> name_count;
            std::vector<std::string> child_order;
            bool duplicate_names = false;

            for(xmlnode_chd = xmlnode->first_node(); xmlnode_chd; xmlnode_chd = xmlnode_chd->next_sibling())
            {
                std::string current_name;
                const char *name_ptr = NULL;
                rapidjson::Value jn, jv;
                bool is_element = false;

                if(xmlnode_chd->type() == rapidxml::node_data || xmlnode_chd->type() == rapidxml::node_cdata)
                {
                    current_name = xml2json_text_additional_name;
                    name_ptr = xml2json_text_additional_name;
                }
                else if(xmlnode_chd->type() == rapidxml::node_element)
                {
                    current_name = xml2json_element_name(xmlnode_chd, options);
                    name_ptr = current_name.c_str();
                    is_element = true;
                }
                else
                {
                    continue; // ignore anything else (comments, PIs, ...); defensive, not produced by parse<0>
                }

                if(++name_count[current_name] > 1)
                    duplicate_names = true;
                if(options.order)
                    child_order.push_back(current_name);

                xml2json_traverse_node(xmlnode_chd, jsvalue_chd, allocator, options);

                if(name_ptr && ((options.array && is_element) || name_count[current_name] > 1))
                    xml2json_to_array_form(name_ptr, jsvalue, jsvalue_chd, allocator);
                else
                {
                    jn.SetString(name_ptr, allocator);
                    jsvalue.AddMember(jn, jsvalue_chd, allocator);
                }
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
    else
    {
        std::cerr << "err data!!" << std::endl;
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
    rapidxml::xml_document<> xml_doc;
    xml_doc.parse<0>(&xml_buffer[0]);

    rapidjson::Document::AllocatorType& allocator = js_doc.GetAllocator();

    for(rapidxml::xml_node<> *xmlnode_chd = xml_doc.first_node(); xmlnode_chd; xmlnode_chd = xmlnode_chd->next_sibling())
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
    if(options.indent > 0)
    {
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        writer.SetIndent(' ', options.indent);
        js_doc.Accept(writer);
    }
    else
    {
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        js_doc.Accept(writer);
    }
    return buffer.GetString();
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
    for(std::size_t i = 0; i < size; ++i)
    {
        switch(text[i])
        {
        case '&': out += "&amp;";  break;
        case '<': out += "&lt;";   break;
        case '>': out += "&gt;";   break;
        case '"': if(attribute) out += "&quot;"; else out += text[i]; break;
        default:  out += text[i];  break;
        }
    }
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

inline void json2xml_write_element(const std::string &name, const rapidjson::Value &value, std::string &out, int indent_width, int depth)
{
    if (value.IsArray())
    {
        // an array is the same element repeated once per item
        for(rapidjson::Value::ConstValueIterator it = value.Begin(); it != value.End(); ++it)
            json2xml_write_element(name, *it, out, indent_width, depth);
        return;
    }

    const bool pretty = indent_width > 0;
    const std::string pad(pretty ? static_cast<std::size_t>(indent_width) * static_cast<std::size_t>(depth) : 0, ' ');

    std::string attributes;
    const rapidjson::Value *text = 0;
    int child_count = 0;

    if (value.IsObject())
    {
        for(rapidjson::Value::ConstMemberIterator it = value.MemberBegin(); it != value.MemberEnd(); ++it)
        {
            const char *member = it->name.GetString();
            if (it->name.GetStringLength() > 0 && member[0] == '@')
            {
                const std::string attribute_value = json2xml_value_text(it->value);
                attributes += ' ';
                attributes += (member + 1);
                attributes += "=\"";
                attributes += json2xml_escape(attribute_value.c_str(), attribute_value.size(), true);
                attributes += '"';
            }
            else if (std::strcmp(member, xml2json_text_additional_name) == 0)
            {
                if (it->value.IsNull() == false)
                    text = &it->value;
            }
            else if (std::strcmp(member, xml2json_order_additional_name) == 0)
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

    out += pad;
    out += '<';
    out += name;
    out += attributes;

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
            if (it->name.GetStringLength() > 0 && member[0] == '@')
                continue;
            if (std::strcmp(member, xml2json_text_additional_name) == 0 || std::strcmp(member, xml2json_order_additional_name) == 0)
                continue;
            json2xml_write_element(std::string(member, it->name.GetStringLength()), it->value, out, indent_width, depth + 1);
        }
        out += pad;
    }

    out += "</";
    out += name;
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
    if (js_doc.IsObject())
    {
        for(rapidjson::Value::ConstMemberIterator it = js_doc.MemberBegin(); it != js_doc.MemberEnd(); ++it)
            json2xml_write_element(std::string(it->name.GetString(), it->name.GetStringLength()), it->value, out, indent_width, 0);
    }
    else if (js_doc.IsNull() == false)
    {
        json2xml_write_element("root", js_doc, out, indent_width, 0); // a bare array/scalar needs a name
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
