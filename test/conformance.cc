// Conformance tests for rapidyyjson against documented RapidJSON 1.1 semantics.
//
// Standalone; it needs only the headers in include/ and libyyjson:
//
//   c++ -std=c++17 -Iinclude test/conformance.cc -lyyjson -o conformance && ./conformance
//
// Exits non-zero if any check fails.
#include <rapidyyjson/cursorstreamwrapper.h>
#include <rapidyyjson/document.h>
#include <rapidyyjson/fwd.h>
#include <rapidyyjson/encodedstream.h>
#include <rapidyyjson/error/en.h>
#include <rapidyyjson/filereadstream.h>
#include <rapidyyjson/filewritestream.h>
#include <rapidyyjson/istreamwrapper.h>
#include <rapidyyjson/memorybuffer.h>
#include <rapidyyjson/memorystream.h>
#include <rapidyyjson/ostreamwrapper.h>
#include <rapidyyjson/pointer.h>
#include <rapidyyjson/prettywriter.h>
#include <rapidyyjson/reader.h>
#include <rapidyyjson/stringbuffer.h>
#include <rapidyyjson/writer.h>

#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

using namespace rapidyyjson;

static int failures = 0;
static int checks = 0;
#define CHECK(cond) do { ++checks; if (!(cond)) { ++failures; \
    std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_STR(a, b) do { ++checks; if (std::strcmp((a), (b)) != 0) { ++failures; \
    std::printf("FAIL %s:%d  \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); } } while (0)

static std::string Str(const Value& v) {
    StringBuffer sb; Writer<StringBuffer> w(sb); v.Accept(w);
    return std::string(sb.GetString(), sb.GetSize());
}

static void TestTypeFlags() {
    Document d;
    d.Parse("[0, -1, 2147483647, 2147483648, -2147483649, 4294967295, 4294967296,"
            " 9223372036854775807, 18446744073709551615, 1.5, 1e30, -0.0, true, false, null, \"s\"]");
    CHECK(!d.HasParseError());
    CHECK(d.IsArray());
    const Value& a = d;

    CHECK(a[0].IsInt() && a[0].IsUint() && a[0].IsInt64() && a[0].IsUint64() && !a[0].IsDouble());
    CHECK(a[1].IsInt() && !a[1].IsUint() && a[1].IsInt64() && !a[1].IsUint64());
    CHECK(a[2].IsInt() && a[2].IsUint());
    CHECK(!a[3].IsInt() && a[3].IsUint() && a[3].IsInt64());     // 2147483648
    CHECK(!a[4].IsInt() && !a[4].IsUint() && a[4].IsInt64());    // -2147483649
    CHECK(!a[5].IsInt() && a[5].IsUint());                       // 4294967295
    CHECK(!a[6].IsUint() && a[6].IsInt64() && a[6].IsUint64());  // 4294967296
    CHECK(a[7].IsInt64() && a[7].IsUint64());
    CHECK(!a[8].IsInt64() && a[8].IsUint64());                   // 18446744073709551615
    CHECK(a[9].IsDouble() && !a[9].IsInt());                     // 1.5
    CHECK(a[10].IsDouble());
    CHECK(a[11].IsDouble());                                     // -0.0
    CHECK(a[12].IsTrue() && a[12].IsBool() && a[12].GetBool());
    CHECK(a[13].IsFalse() && a[13].IsBool() && !a[13].GetBool());
    CHECK(a[14].IsNull());
    CHECK(a[15].IsString() && a[15].GetStringLength() == 1);

    // A whole number written without a fraction is NOT a double (RapidJSON semantics).
    Document e; e.Parse("500");
    CHECK(e.IsNumber() && e.IsInt() && !e.IsDouble());
    CHECK(e.GetDouble() == 500.0);   // but GetDouble() converts
    Document f; f.Parse("500.0");
    CHECK(f.IsDouble() && !f.IsInt());

    // Constructed values
    Value vi(42); CHECK(vi.IsInt() && vi.IsUint() && !vi.IsDouble());
    Value vd(42.0); CHECK(vd.IsDouble() && !vd.IsInt());
    Value vu(42u); CHECK(vu.IsUint() && vu.IsInt());
    Value vn(-42); CHECK(vn.IsInt() && !vn.IsUint());
    Value vb(true); CHECK(vb.IsTrue());
    CHECK(Value(kObjectType).IsObject());
    CHECK(Value(kArrayType).IsArray());
    CHECK(Value(kStringType).IsString() && Value(kStringType).GetStringLength() == 0);
    CHECK(Value(kNullType).IsNull());
    CHECK(Value(kNumberType).IsNumber());

    CHECK(Value(1.0).IsLosslessDouble());
    CHECK(Value(int64_t(9007199254740993LL)).IsLosslessDouble() == false);
    CHECK(Value(0.5f).IsLosslessFloat());
    CHECK(Value(0.1).IsLosslessFloat() == false);
}

static void TestMoveSemantics() {
    Document d;
    Value a(kArrayType);
    Value x(1);
    a.PushBack(x, d.GetAllocator());
    CHECK(x.IsNull());              // PushBack moves
    Value y(2);
    Value z;
    z = y;                          // assignment moves
    CHECK(y.IsNull() && z.GetInt() == 2);

    Value p(3), q(4);
    p.Swap(q);
    CHECK(p.GetInt() == 4 && q.GetInt() == 3);
    swap(p, q);
    CHECK(p.GetInt() == 3 && q.GetInt() == 4);

    Value m(5);
    Value n(std::move(m));
    CHECK(m.IsNull() && n.GetInt() == 5);

    // Move via subscript assignment mutates the container
    Document doc; doc.SetObject();
    Value arr(kArrayType);
    doc.AddMember("k", arr, doc.GetAllocator());
    CHECK(doc["k"].IsArray());
    Value replacement(7);
    doc["k"] = replacement;
    CHECK(doc["k"].GetInt() == 7);
    CHECK(Str(doc) == "{\"k\":7}");
}

static void TestObject() {
    Document d(kObjectType);
    Document::AllocatorType& al = d.GetAllocator();

    d.AddMember("a", 1, al);
    d.AddMember("b", Value("txt", al), al);
    Value name("c", al);
    Value val(kArrayType);
    d.AddMember(name, val, al);
    CHECK(name.IsNull() && val.IsNull());
    CHECK(d.MemberCount() == 3);
    CHECK(d.HasMember("a") && d.HasMember(std::string("b")));
    CHECK(!d.HasMember("zzz"));
    CHECK(d.FindMember("b") != d.MemberEnd());
    CHECK(d.FindMember("zzz") == d.MemberEnd());
    CHECK(d["a"].GetInt() == 1);
    CHECK_STR(d["b"].GetString(), "txt");
    CHECK(d["c"].IsArray());
    CHECK(!d.ObjectEmpty());

    // ordering preserved
    {
        int i = 0;
        const char* expect[] = {"a", "b", "c"};
        for (Value::ConstMemberIterator it = d.MemberBegin(); it != d.MemberEnd(); ++it, ++i)
            CHECK_STR(it->name.GetString(), expect[i]);
        CHECK(i == 3);
    }

    CHECK(d.RemoveMember("zzz") == false);
    CHECK(d.EraseMember("b") == true);
    CHECK(d.MemberCount() == 2 && !d.HasMember("b"));
    CHECK(Str(d) == "{\"a\":1,\"c\":[]}");   // EraseMember preserves order

    d.MemberReserve(32, al);
    CHECK(d.MemberCapacity() >= 32);
    d.RemoveAllMembers();
    CHECK(d.MemberCount() == 0 && d.ObjectEmpty());

    // GenericObject helper + range-for
    Document o(kObjectType);
    o.AddMember("x", 1, o.GetAllocator());
    o.AddMember("y", 2, o.GetAllocator());
    Value::Object obj = o.GetObject();
    CHECK(obj.MemberCount() == 2);
    int sum = 0;
    for (auto& m : obj) sum += m.value.GetInt();
    CHECK(sum == 3);
    const Value& co = o;
    for (auto& m : co.GetObject()) sum += m.value.GetInt();
    CHECK(sum == 6);
}

static void TestArray() {
    Document d(kArrayType);
    Document::AllocatorType& al = d.GetAllocator();
    for (int i = 0; i < 5; i++) d.PushBack(i, al);
    CHECK(d.Size() == 5 && !d.Empty());
    CHECK(d[0].GetInt() == 0 && d[4].GetInt() == 4);
    d.PopBack();
    CHECK(d.Size() == 4);
    d.Erase(d.Begin() + 1);
    CHECK(d.Size() == 3 && Str(d) == "[0,2,3]");
    d.Erase(d.Begin(), d.Begin() + 2);
    CHECK(Str(d) == "[3]");
    d.Reserve(64, al);
    CHECK(d.Capacity() >= 64);
    d.Clear();
    CHECK(d.Empty() && d.Size() == 0);

    d.PushBack(StringRef("lit"), al);
    d.PushBack(1.5, al);
    d.PushBack(true, al);
    CHECK(Str(d) == "[\"lit\",1.5,true]");

    int total = 0;
    for (Value::ConstValueIterator it = d.Begin(); it != d.End(); ++it) ++total;
    CHECK(total == 3);
    total = 0;
    for (auto& v : d.GetArray()) { (void)v; ++total; }
    CHECK(total == 3);
}

static void TestString() {
    Document d; Document::AllocatorType& al = d.GetAllocator();
    Value v;
    v.SetString("hello", al);
    CHECK(v.GetStringLength() == 5);
    CHECK_STR(v.GetString(), "hello");

    const char* lit = "static";
    Value r(StringRef(lit));
    CHECK(r.GetString() == lit);      // no copy: same pointer

    // embedded null
    Value z;
    z.SetString("a\0b", 3, al);
    CHECK(z.GetStringLength() == 3);
    CHECK(std::memcmp(z.GetString(), "a\0b", 3) == 0);
    CHECK(Str(z) == "\"a\\u0000b\"");

    std::string s("std");
    Value sv(s, al);
    CHECK(sv == "std");
    CHECK(sv.Get<std::string>() == "std");
    CHECK(sv == s);
}

static void TestEqualityAndCopy() {
    Document a, b;
    a.Parse("{\"x\":[1,2,{\"y\":\"z\"}]}");
    b.Parse("{\"x\":[1,2,{\"y\":\"z\"}]}");
    CHECK(a == b);
    b["x"][0].SetInt(9);
    CHECK(a != b);

    // key order does not matter for object equality
    Document c, e;
    c.Parse("{\"p\":1,\"q\":2}");
    e.Parse("{\"q\":2,\"p\":1}");
    CHECK(c == e);

    Document dst;
    dst.CopyFrom(a, dst.GetAllocator());
    CHECK(dst == a);
    dst["x"][0].SetInt(100);
    CHECK(a["x"][0].GetInt() == 1);   // deep copy, independent

    Value copy(a, dst.GetAllocator());
    CHECK(copy == a);

    CHECK(Value(1) == 1);
    CHECK(1 == Value(1));
    CHECK(Value(1) != 2);
    CHECK(Value("s", dst.GetAllocator()) == "s");
}

static void TestGetSetIs() {
    Document d; Document::AllocatorType& al = d.GetAllocator();
    Value v;
    v.Set<int>(5);
    CHECK(v.Is<int>() && v.Get<int>() == 5);
    v.Set<double>(2.5);
    CHECK(v.Is<double>() && v.Get<double>() == 2.5);
    v.Set<bool>(true);
    CHECK(v.Is<bool>() && v.Get<bool>());
    v.Set<std::string>(std::string("q"), al);
    CHECK(v.Is<std::string>() && v.Get<std::string>() == "q");
    v.Set<const char*>("w", al);
    CHECK(v.Is<const char*>());
    CHECK_STR(v.Get<const char*>(), "w");
    v.SetInt64(1LL << 40);
    CHECK(v.Is<int64_t>() && v.Get<int64_t>() == (1LL << 40));
    v.SetUint64(~uint64_t(0));
    CHECK(v.Is<uint64_t>() && v.Get<uint64_t>() == ~uint64_t(0));
    v.SetFloat(1.5f);
    CHECK(v.Is<float>() && v.Get<float>() == 1.5f);
    v.SetNull();
    CHECK(v.IsNull());
}

static void TestWriter() {
    // Escaping
    {
        StringBuffer sb; Writer<StringBuffer> w(sb);
        w.String("\"\\/\b\f\n\r\t\x01" "\xE2\x82\xAC");
        CHECK(std::string(sb.GetString()) == "\"\\\"\\\\/\\b\\f\\n\\r\\t\\u0001\xE2\x82\xAC\"");
    }
    // Doubles round-trip and formatting
    {
        struct { double d; const char* s; } cases[] = {
            {0.0, "0.0"}, {-0.0, "-0.0"}, {1.0, "1.0"}, {-1.0, "-1.0"},
            {1.5, "1.5"}, {0.1, "0.1"}, {1e30, "1e30"}, {1.234e30, "1.234e30"},
            {1e-7, "1e-7"}, {1e-6, "0.000001"}, {1234567.0, "1234567.0"},
            {3.141592653589793, "3.141592653589793"},
            {1.7976931348623157e308, "1.7976931348623157e308"},
            {5e-324, "5e-324"},
        };
        for (auto& c : cases) {
            StringBuffer sb; Writer<StringBuffer> w(sb);
            w.Double(c.d);
            if (std::string(sb.GetString()) != c.s) {
                ++failures;
                std::printf("FAIL double %.17g -> \"%s\" (expected \"%s\")\n", c.d, sb.GetString(), c.s);
            }
            ++checks;
        }
    }
    // Round-trip through parse for a range of doubles
    {
        double vals[] = {0.3, 1.0/3.0, 1e100, 1e-100, 123456789.123456789, 2.2250738585072014e-308};
        for (double v : vals) {
            StringBuffer sb; Writer<StringBuffer> w(sb); w.Double(v);
            Document d; d.Parse(sb.GetString());
            CHECK(!d.HasParseError() && d.GetDouble() == v);
        }
    }
    // SetMaxDecimalPlaces
    {
        StringBuffer sb; Writer<StringBuffer> w(sb);
        w.SetMaxDecimalPlaces(3);
        w.StartArray(); w.Double(0.12345); w.Double(0.0001); w.Double(1.234567890123456e30); w.EndArray();
        CHECK_STR(sb.GetString(), "[0.123,0.0,1.234567890123456e30]");
    }
    // IsComplete / Reset / RawValue / RawNumber
    {
        StringBuffer sb; Writer<StringBuffer> w(sb);
        CHECK(!w.IsComplete());
        w.StartObject();
        w.Key("raw"); w.RawValue("{\"deep\":[1]}", 12, kObjectType);
        w.Key("n"); w.RawNumber("1e5", 3);
        w.EndObject();
        CHECK(w.IsComplete());
        CHECK_STR(sb.GetString(), "{\"raw\":{\"deep\":[1]},\"n\":\"1e5\"}");
        StringBuffer sb2; w.Reset(sb2);
        CHECK(!w.IsComplete());
        w.Null();
        CHECK_STR(sb2.GetString(), "null");
    }
    // Integers
    {
        StringBuffer sb; Writer<StringBuffer> w(sb);
        w.StartArray();
        w.Int(-2147483647 - 1); w.Uint(4294967295u);
        w.Int64(-9223372036854775807LL - 1); w.Uint64(18446744073709551615ULL);
        w.EndArray();
        CHECK_STR(sb.GetString(),
                  "[-2147483648,4294967295,-9223372036854775808,18446744073709551615]");
    }
    // NaN / Inf policies
    {
        StringBuffer sb; Writer<StringBuffer> w(sb);
        CHECK(w.Double(std::nan("")) == false);
        StringBuffer sb2;
        Writer<StringBuffer, UTF8<>, UTF8<>, CrtAllocator, kWriteNanAndInfFlag> w2(sb2);
        w2.StartArray(); w2.Double(std::nan("")); w2.Double(HUGE_VAL); w2.Double(-HUGE_VAL); w2.EndArray();
        CHECK_STR(sb2.GetString(), "[NaN,Infinity,-Infinity]");
        StringBuffer sb3;
        Writer<StringBuffer, UTF8<>, UTF8<>, CrtAllocator, kWriteNanAndInfNullFlag> w3(sb3);
        w3.StartArray(); w3.Double(HUGE_VAL); w3.EndArray();
        CHECK_STR(sb3.GetString(), "[null]");
    }
    // StringBuffer API
    {
        StringBuffer sb;
        sb.Put('a'); sb.Reserve(10);
        std::memcpy(sb.Push(3), "bcd", 3);
        CHECK(sb.GetSize() == 4 && sb.GetLength() == 4);
        CHECK_STR(sb.GetString(), "abcd");
        sb.Pop(2);
        CHECK_STR(sb.GetString(), "ab");
        sb.ShrinkToFit();
        CHECK_STR(sb.GetString(), "ab");
        sb.Clear();
        CHECK(sb.GetSize() == 0);
    }
}

static void TestPrettyWriter() {
    Document d; d.Parse("{\"a\":[1,2],\"b\":{},\"c\":[]}");
    {
        StringBuffer sb; PrettyWriter<StringBuffer> w(sb); d.Accept(w);
        CHECK_STR(sb.GetString(),
                  "{\n"
                  "    \"a\": [\n"
                  "        1,\n"
                  "        2\n"
                  "    ],\n"
                  "    \"b\": {},\n"
                  "    \"c\": []\n"
                  "}");
    }
    {
        StringBuffer sb; PrettyWriter<StringBuffer> w(sb);
        w.SetIndent('\t', 1);
        w.SetFormatOptions(kFormatSingleLineArray);
        d.Accept(w);
        CHECK_STR(sb.GetString(),
                  "{\n"
                  "\t\"a\": [1, 2],\n"
                  "\t\"b\": {},\n"
                  "\t\"c\": []\n"
                  "}");
    }
}

static void TestParseErrors() {
    { Document d; d.Parse(""); CHECK(d.HasParseError() && d.GetParseError() == kParseErrorDocumentEmpty); }
    { Document d; d.Parse("{} x"); CHECK(d.HasParseError() && d.GetParseError() == kParseErrorDocumentRootNotSingular); }
    { Document d; d.Parse("[1,2"); CHECK(d.HasParseError()); CHECK(d.GetErrorOffset() > 0); }
    { Document d; d.Parse("nul"); CHECK(d.HasParseError()); }
    { Document d; ParseResult ok = d.Parse("[1]"); CHECK(ok); CHECK(!ok.IsError()); CHECK(ok.Code() == kParseErrorNone); }
    { Document d; d.Parse("["); ParseResult r = d; CHECK(r.IsError()); CHECK(!r); }
    CHECK_STR(GetParseError_En(kParseErrorNone), "No error.");
    CHECK(std::strlen(GetParseError_En(kParseErrorValueInvalid)) > 0);
    // a failed parse leaves the document usable and null
    { Document d; d.Parse("{"); CHECK(d.HasParseError()); d.Parse("[1]"); CHECK(!d.HasParseError() && d.Size() == 1); }
}

static void TestParseFlags() {
    { Document d; d.Parse<kParseCommentsFlag>("{ /*c*/ \"a\":1 // line\n }");
      CHECK(!d.HasParseError() && d["a"].GetInt() == 1); }
    { Document d; d.Parse<kParseTrailingCommasFlag>("[1,2,]");
      CHECK(!d.HasParseError() && d.Size() == 2); }
    { Document d; d.Parse<kParseNanAndInfFlag>("[NaN,Infinity,-Infinity]");
      CHECK(!d.HasParseError() && d.Size() == 3 && std::isnan(d[0].GetDouble())); }
    { Document d; d.Parse<kParseNumbersAsStringsFlag>("[1,2.5]");
      CHECK(!d.HasParseError() && d[0].IsString() && d[1].IsString());
      CHECK_STR(d[0].GetString(), "1"); CHECK_STR(d[1].GetString(), "2.5"); }
    { Document d; d.Parse<kParseStopWhenDoneFlag>("[1] trailing garbage");
      CHECK(!d.HasParseError() && d.Size() == 1); }
    { Document d; d.Parse<kParseFullPrecisionFlag>("[1.2345678901234567e300]");
      CHECK(!d.HasParseError() && d[0].GetDouble() == 1.2345678901234567e300); }
    { char buf[] = "{\"a\":\"b\"}"; Document d; d.ParseInsitu(buf);
      CHECK(!d.HasParseError()); CHECK_STR(d["a"].GetString(), "b"); }
    { Document d; d.Parse("[1,2]", 5); CHECK(!d.HasParseError() && d.Size() == 2); }
    { Document d; d.Parse(std::string("[1,2,3]")); CHECK(!d.HasParseError() && d.Size() == 3); }
    // Deep nesting must not blow the stack
    { std::string deep; for (int i = 0; i < 20000; i++) deep += '['; 
      for (int i = 0; i < 20000; i++) deep += ']';
      Document d; d.Parse(deep.c_str()); CHECK(!d.HasParseError()); }
    // Unicode escapes and surrogate pairs
    { Document d; d.Parse("[\"\\u20AC\",\"\\uD834\\uDD1E\"]");
      CHECK(!d.HasParseError());
      CHECK_STR(d[0].GetString(), "\xE2\x82\xAC");
      CHECK_STR(d[1].GetString(), "\xF0\x9D\x84\x9E"); }
    // Invalid UTF-8 is rejected
    { Document d; d.Parse("[\"\xC0\xAF\"]"); CHECK(d.HasParseError()); }
}

static void TestStreams() {
    // FileReadStream
    {
        const char* path = "/tmp/rapidyyjson_conform.json";
        FILE* fp = std::fopen(path, "wb");
        std::fputs("{\"file\":[1,2,3]}", fp);
        std::fclose(fp);
        fp = std::fopen(path, "rb");
        char buf[16];   // deliberately smaller than the document
        FileReadStream is(fp, buf, sizeof(buf));
        Document d; d.ParseStream(is);
        std::fclose(fp);
        CHECK(!d.HasParseError() && d["file"].Size() == 3);
        std::remove(path);
    }
    // FileWriteStream
    {
        const char* path = "/tmp/rapidyyjson_conform_out.json";
        FILE* fp = std::fopen(path, "wb");
        char buf[8];
        FileWriteStream os(fp, buf, sizeof(buf));
        Writer<FileWriteStream> w(os);
        Document d; d.Parse("{\"k\":\"aaaaaaaaaaaaaaaaaaaa\"}");
        d.Accept(w);
        os.Flush();
        std::fclose(fp);
        Document r; FILE* rp = std::fopen(path, "rb");
        char rbuf[64]; FileReadStream is(rp, rbuf, sizeof(rbuf));
        r.ParseStream(is); std::fclose(rp);
        CHECK(!r.HasParseError() && r == d);
        std::remove(path);
    }
    // istream / ostream wrappers
    {
        std::istringstream iss("{\"s\":true}");
        IStreamWrapper isw(iss);
        Document d; d.ParseStream(isw);
        CHECK(!d.HasParseError() && d["s"].IsTrue());

        std::ostringstream oss;
        OStreamWrapper osw(oss);
        Writer<OStreamWrapper> w(osw);
        d.Accept(w);
        CHECK(oss.str() == "{\"s\":true}");
    }
    // MemoryStream + EncodedInputStream (BOM skipping)
    {
        const char json[] = "\xEF\xBB\xBF[1,2]";
        MemoryStream ms(json, sizeof(json) - 1);
        EncodedInputStream<UTF8<>, MemoryStream> eis(ms);
        Document d; d.ParseStream(eis);
        CHECK(!d.HasParseError() && d.Size() == 2);
    }
    // StringStream directly
    {
        StringStream ss("[7]");
        Document d; d.ParseStream(ss);
        CHECK(!d.HasParseError() && d[0].GetInt() == 7);
    }
}

struct Gen {
    template <typename Handler>
    bool operator()(Handler& h) {
        return h.StartObject() && h.Key("z", 1, true) && h.Int(9) && h.EndObject(1);
    }
};

struct CountingHandler : public BaseReaderHandler<UTF8<>, CountingHandler> {
    int values = 0, keys = 0, objects = 0, arrays = 0;
    bool Default() { ++values; return true; }
    bool Key(const char*, SizeType, bool) { ++keys; return true; }
    bool StartObject() { ++objects; return true; }
    bool StartArray() { ++arrays; return true; }
    bool EndObject(SizeType) { return true; }
    bool EndArray(SizeType) { return true; }
};

struct AbortHandler : public BaseReaderHandler<UTF8<>, AbortHandler> {
    bool Default() { return false; }
};

static void TestReader() {
    {
        CountingHandler h;
        Reader reader;
        StringStream ss("{\"a\":[1,null,true],\"b\":\"s\"}");
        ParseResult r = reader.Parse(ss, h);
        CHECK(r);
        CHECK(h.objects == 1 && h.arrays == 1 && h.keys == 2 && h.values == 4);
    }
    {
        AbortHandler h;
        Reader reader;
        StringStream ss("1");
        ParseResult r = reader.Parse(ss, h);
        CHECK(r.IsError() && r.Code() == kParseErrorTermination);
        CHECK(reader.HasParseError());
    }
    {
        // Iterative (token-by-token) interface
        CountingHandler h;
        Reader reader;
        StringStream ss("[1,2]");
        reader.IterativeParseInit();
        int steps = 0;
        while (!reader.IterativeParseComplete() && reader.IterativeParseNext<kParseDefaultFlags>(ss, h))
            ++steps;
        CHECK(steps == 4);   // StartArray, 1, 2, EndArray
        CHECK(h.arrays == 1 && h.values == 2);
    }
    {
        // A Document is a Handler: SAX events are accepted
        Document d;
        Reader reader;
        StringStream ss("{\"z\":9}");
        CHECK(reader.Parse(ss, d));
    }
    {
        // Populate() from a generator
        Gen g;
        Document d;
        d.Populate(g);
        CHECK(d.IsObject() && d["z"].GetInt() == 9);
    }
}

static void TestPointer() {
    Document d;
    d.Parse("{\"foo\":[\"bar\",\"baz\"],\"\":0,\"a/b\":1,\"c%d\":2,\"e^f\":3,"
            "\"g|h\":4,\"i\\\\j\":5,\"k\\\"l\":6,\" \":7,\"m~n\":8}");
    CHECK(!d.HasParseError());

    // RFC 6901 examples
    CHECK(Pointer("").Get(d) == &d);
    CHECK(Pointer("/foo").Get(d)->IsArray());
    CHECK_STR(Pointer("/foo/0").Get(d)->GetString(), "bar");
    CHECK(Pointer("/").Get(d)->GetInt() == 0);
    CHECK(Pointer("/a~1b").Get(d)->GetInt() == 1);
    CHECK(Pointer("/c%d").Get(d)->GetInt() == 2);
    CHECK(Pointer("/e^f").Get(d)->GetInt() == 3);
    CHECK(Pointer("/g|h").Get(d)->GetInt() == 4);
    CHECK(Pointer("/i\\j").Get(d)->GetInt() == 5);
    CHECK(Pointer("/k\"l").Get(d)->GetInt() == 6);
    CHECK(Pointer("/ ").Get(d)->GetInt() == 7);
    CHECK(Pointer("/m~0n").Get(d)->GetInt() == 8);
    // URI fragment form
    CHECK(Pointer("#/a~1b").Get(d)->GetInt() == 1);
    CHECK(Pointer("#/c%25d").Get(d)->GetInt() == 2);
    CHECK(Pointer("#/%20").Get(d)->GetInt() == 7);

    // Not found
    size_t idx = 0;
    CHECK(Pointer("/foo/9").Get(d) == 0);
    CHECK(Pointer("/nope/x").Get(d, &idx) == 0 && idx == 0);

    // Invalid pointers
    CHECK(!Pointer("foo").IsValid());
    CHECK(Pointer("foo").GetParseErrorCode() == kPointerParseErrorTokenMustBeginWithSolidus);
    CHECK(!Pointer("/~2").IsValid());
    CHECK(Pointer("/~2").GetParseErrorCode() == kPointerParseErrorInvalidEscape);

    // Create / Set / GetWithDefault / Erase
    Document m(kObjectType);
    Pointer("/x/y/0").Set(m, 42);
    CHECK(m["x"]["y"][0].GetInt() == 42);
    Pointer("/x/y/-").Create(m, m.GetAllocator()).SetString("app", m.GetAllocator());
    CHECK(m["x"]["y"].Size() == 2);
    CHECK_STR(m["x"]["y"][1].GetString(), "app");
    CHECK(Pointer("/x/z").GetWithDefault(m, "dflt", m.GetAllocator()).GetString() == std::string("dflt"));
    CHECK(Pointer("/x/z").GetWithDefault(m, "other", m.GetAllocator()).GetString() == std::string("dflt"));
    CHECK(Pointer("/x/z").Erase(m));
    CHECK(!m["x"].HasMember("z"));
    CHECK(!Pointer("").Erase(m));

    Value sw(kArrayType);
    Pointer("/x/y").Swap(m, sw);
    CHECK(m["x"]["y"].IsArray() && m["x"]["y"].Size() == 0);
    CHECK(sw.Size() == 2);

    // Free functions
    CHECK(GetValueByPointer(m, "/x")->IsObject());
    SetValueByPointer(m, "/n", 5, m.GetAllocator());
    CHECK(m["n"].GetInt() == 5);
    CHECK(EraseValueByPointer(m, "/n"));

    // Stringify
    {
        Pointer p("/a~1b/c~0d/0");
        StringBuffer sb; CHECK(p.Stringify(sb));
        CHECK_STR(sb.GetString(), "/a~1b/c~0d/0");
        StringBuffer sb2; CHECK(p.StringifyUriFragment(sb2));
        CHECK_STR(sb2.GetString(), "#/a~1b/c~0d/0");
        StringBuffer sb3; CHECK(Pointer("/ ").StringifyUriFragment(sb3));
        CHECK_STR(sb3.GetString(), "#/%20");
    }
    // Append / equality / tokens
    {
        Pointer p("/a");
        Pointer q = p.Append("b").Append(SizeType(3));
        CHECK(q.GetTokenCount() == 3);
        CHECK(q == Pointer("/a/b/3"));
        CHECK(q != p);
        CHECK(p < q);
        Pointer c(q);
        CHECK(c == q);
        Pointer e; e = q;
        CHECK(e == q);
        e.Swap(c);
        CHECK(e == q && c == q);
    }
}

static void TestDocumentSemantics() {
    // Move-only document, usable in containers
    Document a; a.Parse("[1]");
    Document b(std::move(a));
    CHECK(b.Size() == 1);
    Document c; c = std::move(b);
    CHECK(c.Size() == 1);
    std::vector<Document> v;
    v.push_back(std::move(c));
    CHECK(v[0].Size() == 1);

    // Swap
    Document p; p.Parse("1");
    Document q; q.Parse("2");
    p.Swap(q);
    CHECK(p.GetInt() == 2 && q.GetInt() == 1);

    // Values allocated from a document's allocator survive as long as the document
    Document d(kObjectType);
    {
        std::string tmp = "temporary";
        d.AddMember(Value(tmp.c_str(), d.GetAllocator()).Move(),
                    Value(tmp.c_str(), d.GetAllocator()).Move(),
                    d.GetAllocator());
    }
    CHECK_STR(d["temporary"].GetString(), "temporary");

    // GetStackCapacity / allocator access
    CHECK(d.GetAllocator().Capacity() > 0);
}

static void TestAllocators() {
    CrtAllocator crt;
    void* p = crt.Malloc(10);
    p = crt.Realloc(p, 10, 20);
    CrtAllocator::Free(p);
    CHECK(CrtAllocator::kNeedFree == true);

    MemoryPoolAllocator<> pool(1024);
    CHECK(MemoryPoolAllocator<>::kNeedFree == false);
    void* a = pool.Malloc(100);
    CHECK(a != 0);
    CHECK(pool.Size() >= 100 && pool.Capacity() >= 1024);
    void* b = pool.Realloc(a, 100, 200);
    CHECK(b != 0);
    pool.Clear();
    CHECK(pool.Size() == 0);

    char buffer[4096];
    MemoryPoolAllocator<> userPool(buffer, sizeof(buffer));
    Document d(kObjectType, &userPool);
    d.AddMember("k", 1, d.GetAllocator());
    CHECK(d["k"].GetInt() == 1);

    // Value with a freeing allocator releases its memory in the destructor
    {
        typedef GenericDocument<UTF8<>, CrtAllocator> CrtDocument;
        CrtDocument cd;
        cd.Parse("{\"a\":[\"long string value here\",2]}");
        CHECK(!cd.HasParseError());
        CHECK_STR(cd["a"][0].GetString(), "long string value here");
    }
}


static void TestNonStandardExtensions() {
    // kParseEscapedApostropheFlag
    { Document d; d.Parse<kParseEscapedApostropheFlag>("[\"it\\'s\"]");
      CHECK(!d.HasParseError());
      if (!d.HasParseError()) CHECK_STR(d[0].GetString(), "it's"); }
    { Document d; d.Parse("[\"it\\'s\"]"); CHECK(d.HasParseError()); }
    // an escaped backslash before an apostrophe must survive untouched
    { Document d; d.Parse<kParseEscapedApostropheFlag>("[\"a\\\\\\'b\"]");
      CHECK(!d.HasParseError());
      if (!d.HasParseError()) CHECK_STR(d[0].GetString(), "a\\'b"); }
    // combined with comments that themselves contain quotes
    { Document d; d.Parse<kParseEscapedApostropheFlag | kParseCommentsFlag>(
          "{ /* a \" quote */ \"k\": \"v\\'w\" // trailing \" \n }");
      CHECK(!d.HasParseError());
      if (!d.HasParseError()) CHECK_STR(d["k"].GetString(), "v'w"); }

    // CursorStreamWrapper
    { StringStream ss("[1,\n2]");
      CursorStreamWrapper<StringStream> csw(ss);
      Document d; d.ParseStream(csw);
      CHECK(!d.HasParseError() && d.Size() == 2);
      CHECK(csw.GetLine() >= 1); }
}


static void TestEncodingsAndTranscoding() {
    // ASCII target encoding must escape every non-ASCII codepoint.
    {
        Document d; d.Parse("[\"\\u20AC\",\"\\uD834\\uDD1E\"]");
        CHECK(!d.HasParseError());
        StringBuffer sb;
        Writer<StringBuffer, UTF8<>, ASCII<>> w(sb);
        d.Accept(w);
        CHECK_STR(sb.GetString(), "[\"\\u20AC\",\"\\uD834\\uDD1E\"]");
    }
    // UTF-16 output buffer
    {
        Document d; d.Parse("[\"a\\u20ACb\"]");
        CHECK(!d.HasParseError());
        GenericStringBuffer<UTF16<>> sb;
        Writer<GenericStringBuffer<UTF16<>>, UTF8<>, UTF16<>> w(sb);
        d.Accept(w);
        const wchar_t* out = sb.GetString();
        CHECK(out[0] == L'[' && out[1] == L'"' && out[2] == L'a' &&
              out[3] == static_cast<wchar_t>(0x20AC) && out[4] == L'b');
    }
    // UTF-16 DOM, parsed from UTF-8 source
    {
        GenericDocument<UTF16<>> d;
        d.Parse<kParseDefaultFlags, UTF8<>>("{\"k\":\"\\u20AC\"}");
        CHECK(!d.HasParseError());
        if (!d.HasParseError()) {
            const GenericValue<UTF16<>>& v = d[L"k"];
            CHECK(v.IsString() && v.GetStringLength() == 1);
            CHECK(v.GetString()[0] == static_cast<wchar_t>(0x20AC));
        }
    }
    // UTF-8 decoding rejects overlong forms and surrogates
    {
        unsigned cp = 0;
        { StringStream s("\xC0\xAF"); CHECK(!UTF8<>::Decode(s, &cp)); }        // overlong
        { StringStream s("\xED\xA0\x80"); CHECK(!UTF8<>::Decode(s, &cp)); }   // surrogate
        { StringStream s("\xF5\x80\x80\x80"); CHECK(!UTF8<>::Decode(s, &cp)); } // > U+10FFFF
        { StringStream s("\xE2\x82\xAC"); CHECK(UTF8<>::Decode(s, &cp) && cp == 0x20ACu); }
    }
}

static void TestBulkMutation() {
    Document d(kObjectType);
    Document::AllocatorType& al = d.GetAllocator();

    // grow well past the default capacities
    Value arr(kArrayType);
    for (int i = 0; i < 1000; i++) arr.PushBack(i, al);
    CHECK(arr.Size() == 1000 && arr[999].GetInt() == 999);

    for (int i = 0; i < 200; i++) {
        char key[16];
        std::snprintf(key, sizeof(key), "k%d", i);
        d.AddMember(Value(key, al).Move(), Value(i), al);
    }
    CHECK(d.MemberCount() == 200);
    CHECK(d["k199"].GetInt() == 199);

    // erase a contiguous range, order preserved
    Value::MemberIterator first = d.MemberBegin() + 10;
    Value::MemberIterator last = d.MemberBegin() + 20;
    Value::MemberIterator after = d.EraseMember(first, last);
    CHECK(d.MemberCount() == 190);
    CHECK_STR(after->name.GetString(), "k20");
    CHECK_STR(d.MemberBegin()[9].name.GetString(), "k9");

    // RemoveMember(iterator) reorders: the last member fills the hole
    const char* lastName = (d.MemberEnd() - 1)->name.GetString();
    std::string lastNameCopy(lastName);
    d.RemoveMember(d.MemberBegin());
    CHECK(d.MemberCount() == 189);
    CHECK(lastNameCopy == d.MemberBegin()->name.GetString());

    // array range erase
    Value::ValueIterator it = arr.Erase(arr.Begin() + 100, arr.Begin() + 900);
    CHECK(arr.Size() == 200);
    CHECK(it->GetInt() == 900);
    CHECK(arr[0].GetInt() == 0 && arr[100].GetInt() == 900);
}

static void TestCrossAllocatorCopy() {
    GenericDocument<UTF8<>, CrtAllocator> src;
    src.Parse("{\"a\":[1,\"two\",{\"b\":3.5}],\"c\":null}");
    CHECK(!src.HasParseError());

    Document dst;                                  // MemoryPoolAllocator
    dst.CopyFrom(src, dst.GetAllocator());         // cross-allocator deep copy
    CHECK(dst == src);
    CHECK(Str(dst) == "{\"a\":[1,\"two\",{\"b\":3.5}],\"c\":null}");

    // and back again
    GenericDocument<UTF8<>, CrtAllocator> back;
    back.CopyFrom(dst, back.GetAllocator());
    CHECK(back == dst);

    // copyConstStrings forces owned copies of referenced strings
    {
        char buf[] = "volatile";
        Document d(kObjectType);
        Value v;
        v.SetString(buf, 8);                       // const reference into buf
        d.AddMember("k", v, d.GetAllocator());
        Document c;
        Value copied(d, c.GetAllocator(), /*copyConstStrings=*/true);
        std::memset(buf, 'x', 8);
        CHECK_STR(copied["k"].GetString(), "volatile");
    }
}


// Instantiates the corners of the API that the behavioural tests do not reach, so that every
// template advertised in README.md is actually type-checked.
static void TestApiSurface() {
    // MemoryBuffer + EncodedOutputStream + AutoUTFOutputStream
    {
        MemoryBuffer mb;
        EncodedOutputStream<UTF8<>, MemoryBuffer> eos(mb, true);
        Writer<EncodedOutputStream<UTF8<>, MemoryBuffer>> w(eos);
        w.StartArray(); w.Int(1); w.EndArray();
        eos.Flush();
        CHECK(mb.GetSize() == 3 + 3);   // BOM + "[1]"
        CHECK(std::memcmp(mb.GetBuffer(), "\xEF\xBB\xBF[1]", 6) == 0);
    }
    {
        MemoryBuffer mb;
        AutoUTFOutputStream<unsigned, MemoryBuffer> aos(mb, kUTF8, false);
        aos.Put('x');
        aos.Flush();
        CHECK(aos.GetType() == kUTF8);
        CHECK(mb.GetSize() == 1 && mb.GetBuffer()[0] == 'x');
    }
    {
        const char json[] = "\xEF\xBB\xBF[1,2]";
        MemoryStream ms(json, sizeof(json) - 1);
        AutoUTFInputStream<unsigned, MemoryStream> ais(ms);
        CHECK(ais.GetType() == kUTF8 && ais.HasBOM());
        CHECK(ais.Take() == static_cast<unsigned>('['));
    }
    // GenericStreamWrapper
    {
        StringStream ss("[3]");
        GenericStreamWrapper<StringStream> gsw(ss);
        Document d; d.ParseStream(gsw);
        CHECK(!d.HasParseError() && d[0].GetInt() == 3);
    }
    // PutN / PutReserve / PutUnsafe free functions
    {
        StringBuffer sb;
        PutReserve(sb, 8);
        PutUnsafe(sb, 'a');
        PutN(sb, 'b', 3);
        CHECK_STR(sb.GetString(), "abbb");
    }
    // StringRef variants
    {
        const char* p = "abc";
        std::string st("abcd");
        CHECK(StringRef(p).length == 3);
        CHECK(StringRef(p, 2).length == 2);
        CHECK(StringRef(st).length == 4);
        CHECK(StringRef("literal").length == 7);
    }
    // Pointer built from user-supplied tokens, and Append(ValueType)
    {
        typedef Pointer::Token Token;
        static const Token tokens[] = {{"a", 1, kPointerInvalidIndex}, {"0", 1, 0}};
        Pointer p(tokens, 2);
        CHECK(p.IsValid() && p.GetTokenCount() == 2);
        CHECK(p == Pointer("/a/0"));

        Document d(kObjectType);
        Value name("b", d.GetAllocator());
        Pointer q = Pointer("/x").Append(name);
        CHECK(q == Pointer("/x/b"));
        Value idx(2u);
        CHECK(Pointer("/x").Append(idx) == Pointer("/x/2"));
    }
    // BaseReaderHandler with no Derived
    {
        BaseReaderHandler<> h;
        Reader r;
        StringStream ss("{\"a\":[1,2]}");
        CHECK(r.Parse(ss, h));
    }
    // Writer::Flush and GetMaxDecimalPlaces
    {
        StringBuffer sb; Writer<StringBuffer> w(sb);
        CHECK(w.GetMaxDecimalPlaces() == Writer<StringBuffer>::kDefaultMaxDecimalPlaces);
        w.Bool(true); w.Flush();
        CHECK_STR(sb.GetString(), "true");
    }
    // GetObj() aliases and Value::Array/Object conversion constructors
    {
        Document d(kObjectType);
        d.AddMember("arr", Value(kArrayType), d.GetAllocator());
        Value::Object o = d.GetObj();
        CHECK(o.MemberCount() == 1);
        const Document& cd = d;
        CHECK(cd.GetObj().MemberCount() == 1);

        Value moved(d["arr"].GetArray());   // move-construct a Value from an Array view
        CHECK(moved.IsArray() && d["arr"].IsArray() && d["arr"].Empty());
    }
    // ParseResult / GetParseErrorFunc typedef
    {
        GetParseErrorFunc f = GetParseError_En;
        CHECK(std::strlen(f(kParseErrorNone)) > 0);
    }
}

int main() {
    TestTypeFlags();
    TestMoveSemantics();
    TestObject();
    TestArray();
    TestString();
    TestEqualityAndCopy();
    TestGetSetIs();
    TestWriter();
    TestPrettyWriter();
    TestParseErrors();
    TestParseFlags();
    TestStreams();
    TestReader();
    TestPointer();
    TestDocumentSemantics();
    TestAllocators();
    TestNonStandardExtensions();
    TestEncodingsAndTranscoding();
    TestBulkMutation();
    TestCrossAllocatorCopy();
    TestApiSurface();
    std::printf("\n%d checks, %d failures\n", checks, failures);
    return failures != 0;
}
