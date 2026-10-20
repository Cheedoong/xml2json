// Second translation unit for the test suite.
//
// It includes the same header as test.cpp and links together with it, so the
// test binary only builds if the header-only functions really have inline
// linkage (issue #23: they used to be plain definitions and caused
// "multiple definition" link errors as soon as the header was included twice).

#include <string>

#include "xml2json.hpp"

std::string xml2json_from_second_translation_unit(const char *xml)
{
    return xml2json(xml);
}
