// A tour of the API: parse, inspect, mutate, address by pointer, serialise.
//
// Every line below is valid RapidJSON 1.1 as well -- rename the namespace and
// the includes back and it compiles against RapidJSON unchanged.
#include <rapidyyjson/document.h>
#include <rapidyyjson/pointer.h>
#include <rapidyyjson/prettywriter.h>
#include <rapidyyjson/stringbuffer.h>

#include <cstdio>

int
main()
{
    const char* json = R"({
        "name": "drone-01",
        "battery": 0.87,
        "waypoints": [[0, 0, 10], [50, 0, 10]]
    })";

    rapidyyjson::Document d;
    d.Parse(json);
    if (d.HasParseError())
    {
        std::fprintf(stderr,
                     "parse error %d at offset %zu\n",
                     d.GetParseError(),
                     d.GetErrorOffset());
        return 1;
    }

    // Reading. operator[] asserts on a missing member, so guard it.
    if (d.HasMember("name") && d["name"].IsString())
        std::printf("name       = %s\n", d["name"].GetString());

    // 0.87 is stored as a double; 10 in the waypoints is stored as an integer.
    std::printf("battery    = %.2f (IsDouble=%d)\n",
                d["battery"].GetDouble(),
                d["battery"].IsDouble());
    std::printf("waypoints  = %u\n", d["waypoints"].Size());

    // Mutating. Every insertion needs the document's allocator, and Value is
    // move-only: AddMember/PushBack take ownership of their argument.
    auto& alloc = d.GetAllocator();
    d.AddMember("armed", true, alloc);

    rapidyyjson::Value wp(rapidyyjson::kArrayType);
    wp.PushBack(50, alloc).PushBack(50, alloc).PushBack(20, alloc);
    d["waypoints"].PushBack(wp, alloc); // wp is null after this line

    // A string that must outlive the literal has to be copied explicitly.
    rapidyyjson::Value note;
    note.SetString("returning to base", alloc);
    d.AddMember("note", note, alloc);

    // Addressing by RFC 6901 JSON Pointer.
    if (const auto* v = rapidyyjson::Pointer("/waypoints/2/2").Get(d))
        std::printf("last alt   = %d\n", v->GetInt());

    // Serialising. Writer emits compact output, PrettyWriter indents.
    rapidyyjson::StringBuffer sb;
    rapidyyjson::PrettyWriter<rapidyyjson::StringBuffer> writer(sb);
    d.Accept(writer);
    std::printf("\n%s\n", sb.GetString());

    return 0;
}
