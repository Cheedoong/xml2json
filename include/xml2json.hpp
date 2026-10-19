#ifndef XML2JSON_HPP_INCLUDED
#define XML2JSON_HPP_INCLUDED

// Copyright (C) 2015 Alan Zhuang (Cheedoong)	HKUST.  [Updated to the latest version of rapidjson]
// Copyright (C) 2013 Alan Zhuang (Cheedoong)	Tencent, Inc.

#include <iostream>
#include <map>
#include <string>
#include <cctype>
#include <cerrno>
#include <cstdlib>

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
/* [End]   This part is configurable */

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

// Stores a piece of XML text into @jsvalue.  When xml2json_numeric_support is
// enabled and the whole text is numeric it becomes a JSON number, otherwise it
// stays a JSON string.  Integer values use strtoll() so 64-bit (and negative)
// values are not truncated, falling back to double only on overflow.
inline void xml2json_set_text(rapidjson::Value &jsvalue, const char *text, rapidjson::Document::AllocatorType& allocator)
{
    if (text == nullptr)
    {
        jsvalue.SetNull();
        return;
    }

    bool hasDecimal = false;
    if (xml2json_numeric_support == false || xml2json_has_digits_only(text, &hasDecimal) == false)
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

inline void xml2json_to_array_form(const char *name, rapidjson::Value &jsvalue, rapidjson::Value &jsvalue_chd, rapidjson::Document::AllocatorType& allocator)
{
    rapidjson::Value jsvalue_target; // target to do some operation
    rapidjson::Value jn;             // this is a must, partially because of the latest version of rapidjson
    jn.SetString(name, allocator);
    jsvalue_target = jsvalue.FindMember(name)->value;
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

inline void xml2json_add_attributes(rapidxml::xml_node<> *xmlnode, rapidjson::Value &jsvalue, rapidjson::Document::AllocatorType& allocator)
{
    rapidxml::xml_attribute<> *myattr;
    for(myattr = xmlnode->first_attribute(); myattr; myattr = myattr->next_attribute())
    {
        rapidjson::Value jn, jv;
        jn.SetString((std::string(xml2json_attribute_name_prefix) + myattr->name()).c_str(), allocator);
        xml2json_set_text(jv, myattr->value(), allocator);
        jsvalue.AddMember(jn, jv, allocator);
    }
}

inline void xml2json_traverse_node(rapidxml::xml_node<> *xmlnode, rapidjson::Value &jsvalue, rapidjson::Document::AllocatorType& allocator)
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
                xml2json_set_text(jv, xmlnode->first_node()->value(), allocator);
                jsvalue.AddMember(jn, jv, allocator);
                xml2json_add_attributes(xmlnode, jsvalue, allocator);
                return;
            }
            else
            {
                // case: <e attr="xxx">...</e>
                xml2json_add_attributes(xmlnode, jsvalue, allocator);
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
                xml2json_set_text(jsvalue, xmlnode->first_node()->value(), allocator);
                return;
            }
        }
        if(xmlnode->first_node())
        {
            // case: complex else...
            std::map<std::string, int> name_count;
            for(xmlnode_chd = xmlnode->first_node(); xmlnode_chd; xmlnode_chd = xmlnode_chd->next_sibling())
            {
                std::string current_name;
                const char *name_ptr = NULL;
                rapidjson::Value jn, jv;
                if(xmlnode_chd->type() == rapidxml::node_data || xmlnode_chd->type() == rapidxml::node_cdata)
                {
                    current_name = xml2json_text_additional_name;
                    name_count[current_name]++;
                    jv.SetString(xml2json_text_additional_name, allocator);
                    name_ptr = jv.GetString();
                }
                else if(xmlnode_chd->type() == rapidxml::node_element)
                {
                    current_name = xmlnode_chd->name();
                    name_count[current_name]++;
                    name_ptr = xmlnode_chd->name();
                }
                xml2json_traverse_node(xmlnode_chd, jsvalue_chd, allocator);
                if(name_count[current_name] > 1 && name_ptr)
                    xml2json_to_array_form(name_ptr, jsvalue, jsvalue_chd, allocator);
                else
                {
                    jn.SetString(name_ptr, allocator);
                    jsvalue.AddMember(jn, jsvalue_chd, allocator);
                }
            }
        }
    }
    else
    {
        std::cerr << "err data!!" << std::endl;
    }
}

inline std::string xml2json(const char *xml_str)
{
    rapidjson::Document js_doc;
    js_doc.SetObject();

    if (xml_str == nullptr)
        return "{}";

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
        rapidjson::Value jsvalue_chd;
        jsvalue_chd.SetObject();
        xml2json_traverse_node(xmlnode_chd, jsvalue_chd, allocator);
        js_doc.AddMember(rapidjson::StringRef(xmlnode_chd->name()), jsvalue_chd, allocator);
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    js_doc.Accept(writer);
    return buffer.GetString();
}

#endif
