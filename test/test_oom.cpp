// OOM regression tests for the vendored RapidJSON.
//
// These tests exercise the downstream allocation-failure handling added by
// the R2 patch.  They use a failing stack allocator for parsing and a
// MemoryPoolAllocator with a tiny user buffer for document building.  The
// baseline (before the patch) aborts or crashes; the patched tree must either
// report kParseErrorTermination or return a valid, truncated container.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "rapidjson/document.h"

namespace
{

int g_failures = 0;

void check(bool condition, const char *what)
{
    if (condition)
    {
        std::printf("ok   : %s\n", what);
        return;
    }
    ++g_failures;
    std::printf("FAIL : %s\n", what);
}

struct FailingAllocator
{
    static const bool kNeedFree = true;

    FailingAllocator() : budget_(0) {}
    explicit FailingAllocator(int budget) : budget_(budget) {}

    void *Malloc(std::size_t size)
    {
        if (budget_-- <= 0)
            return 0;
        return std::malloc(size);
    }

    void *Realloc(void *ptr, std::size_t old_size, std::size_t new_size)
    {
        (void)old_size;
        if (budget_-- <= 0)
            return 0;
        return std::realloc(ptr, new_size);
    }

    static void Free(void *ptr)
    {
        std::free(ptr);
    }

    int budget_;
};

struct NullAllocator
{
    static const bool kNeedFree = false;

    void *Malloc(std::size_t) { return 0; }
    void *Realloc(void *, std::size_t, std::size_t) { return 0; }
    static void Free(void *) {}
};

std::string make_json()
{
    std::string json("[");
    for (int i = 0; i < 500; ++i)
    {
        if (i)
            json += ",";
        json += "{\"a\":1}";
    }
    json += "]";
    return json;
}

void test_parse_budget(int budget)
{
    typedef rapidjson::GenericDocument<rapidjson::UTF8<>, rapidjson::CrtAllocator, FailingAllocator> Document;
    FailingAllocator stack_allocator(budget);
    Document doc(0, 64, &stack_allocator);
    const std::string json = make_json();
    doc.Parse(json.c_str());

    if (budget < 20)
    {
        check(doc.HasParseError() && doc.GetParseError() == rapidjson::kParseErrorTermination,
              "exhausted parse stack returns kParseErrorTermination");
    }
    else
    {
        check(!doc.HasParseError(), "sufficient parse stack still parses normally");
    }
}

void test_build_path()
{
    typedef rapidjson::MemoryPoolAllocator<NullAllocator> Allocator;
    typedef rapidjson::GenericValue<rapidjson::UTF8<>, Allocator> Value;

    char buffer[1024];
    Allocator allocator(buffer, sizeof(buffer));

    Value array(rapidjson::kArrayType);
    for (int i = 0; i < 1000; ++i)
        array.PushBack(i, allocator);
    check(array.Size() > 0 && array.Size() <= array.Capacity(),
          "array building drops values instead of writing through NULL");

    Value object(rapidjson::kObjectType);
    for (int i = 0; i < 1000; ++i)
        object.AddMember("k", i, allocator);
    check(object.MemberCount() <= object.MemberCapacity(),
          "object building keeps member capacity consistent");

    const std::string text(2000, 'x');
    Value string(text.c_str(), static_cast<rapidjson::SizeType>(text.size()), allocator);
    check(string.IsString() && string.GetStringLength() == 0,
          "string building falls back to an empty string");
}

} // namespace

int main()
{
    std::printf("=== RapidJSON OOM regression ===\n");
    for (int budget = 1; budget <= 10; ++budget)
        test_parse_budget(budget);
    test_parse_budget(20);
    test_build_path();

    if (g_failures != 0)
    {
        std::printf("FAILED: %d OOM check(s)\n", g_failures);
        return 1;
    }
    std::printf("PASSED: RapidJSON OOM checks\n");
    return 0;
}
