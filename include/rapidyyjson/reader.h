/*
 * rapidyyjson - a RapidJSON-compatible API implemented on top of yyjson.
 *
 * SAX parser, mirroring `rapidjson/reader.h`. The scanning itself is performed
 * by yyjson; the resulting tree is replayed as the SAX event sequence RapidJSON
 * handlers expect.
 */

#ifndef RAPIDYYJSON_READER_H_
#define RAPIDYYJSON_READER_H_

#include "allocators.h"
#include "encodings.h"
#include "error/error.h"
#include "internal/ieee754.h"
#include "internal/meta.h"
#include "internal/stack.h"
#include "rapidyyjson.h"
#include "stream.h"

#include <string>
#include <vector>
#include <yyjson.h>

// The scanner needs YYJSON_TYPE_RAW, which yyjson gained in 0.5.0.
#if defined(YYJSON_VERSION_HEX) && YYJSON_VERSION_HEX < 0x000500
#error "rapidyyjson requires yyjson >= 0.5.0"
#endif

RAPIDYYJSON_NAMESPACE_BEGIN

namespace internal
{

///////////////////////////////////////////////////////////////////////////////
// StreamLocalCopy

//! Works on a stack copy of a stream, writing it back on destruction.
/*! Selected by StreamTraits<Stream>::copyOptimization: streams that are cheap
    to copy are pulled into a local so the hot loop touches a stack object
    instead of a reference the compiler cannot prove is unaliased.
*/
template <typename Stream, int = StreamTraits<Stream>::copyOptimization>
class StreamLocalCopy;

//! Keeps a local copy and writes it back.
template <typename Stream>
class StreamLocalCopy<Stream, 1>
{
  public:
    StreamLocalCopy(Stream& original)
        : s(original),
          original_(original)
    {
    }

    ~StreamLocalCopy()
    {
        original_ = s;
    }

    Stream s;

  private:
    StreamLocalCopy& operator=(const StreamLocalCopy&) = delete;

    Stream& original_;
};

//! Works on the original stream.
template <typename Stream>
class StreamLocalCopy<Stream, 0>
{
  public:
    StreamLocalCopy(Stream& original)
        : s(original)
    {
    }

    Stream& s;

  private:
    StreamLocalCopy& operator=(const StreamLocalCopy&) = delete;
};

} // namespace internal

///////////////////////////////////////////////////////////////////////////////
// SkipWhitespace

//! Advance a stream past JSON whitespace (space, LF, CR, tab).
template <typename InputStream>
void
SkipWhitespace(InputStream& is)
{
    internal::StreamLocalCopy<InputStream> copy(is);
    InputStream& s(copy.s);

    typename InputStream::Ch c;
    while ((c = s.Peek()) == ' ' || c == '\n' || c == '\r' || c == '\t')
        s.Take();
}

//! Advance a pointer past JSON whitespace, stopping at end.
inline const char*
SkipWhitespace(const char* p, const char* end)
{
    while (p != end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t'))
        ++p;
    return p;
}

///////////////////////////////////////////////////////////////////////////////
// ParseFlag

//! Combination of parseFlags
enum ParseFlag
{
    kParseNoFlags = 0,                //!< No flags are set.
    kParseInsituFlag = 1,             //!< In-situ (destructive) parsing.
    kParseValidateEncodingFlag = 2,   //!< Validate encoding of JSON strings.
    kParseIterativeFlag = 4,          //!< Iterative(constant complexity in terms of function call
                                      //!< stack size) parsing.
    kParseStopWhenDoneFlag = 8,       //!< After parsing a complete JSON root from stream, stop
                                      //!< further processing the rest of stream.
    kParseFullPrecisionFlag = 16,     //!< Parse number in full precision.
    kParseCommentsFlag = 32,          //!< Allow one-line (//) and multi-line (/**/) comments.
    kParseNumbersAsStringsFlag = 64,  //!< Parse all numbers (ints/doubles) as strings.
    kParseTrailingCommasFlag = 128,   //!< Allow trailing commas at the end of objects and arrays.
    kParseNanAndInfFlag = 256,        //!< Allow parsing NaN, Inf, Infinity, -Inf and -Infinity as
                                      //!< doubles.
    kParseEscapedApostropheFlag = 512,//!< Allow escaped apostrophe in strings.
    kParseDefaultFlags = RAPIDYYJSON_PARSE_DEFAULT_FLAGS //!< Default parse flags.
};

///////////////////////////////////////////////////////////////////////////////
// Handler

/*! \class rapidyyjson::Handler
    \brief Concept for receiving events from GenericReader upon parsing.

    \code
    concept Handler {
        typename Ch;

        bool Null();
        bool Bool(bool b);
        bool Int(int i);
        bool Uint(unsigned i);
        bool Int64(int64_t i);
        bool Uint64(uint64_t i);
        bool Double(double d);
        //! Enabled via kParseNumbersAsStringsFlag, string is not null-terminated
        bool RawNumber(const Ch* str, SizeType length, bool copy);
        bool String(const Ch* str, SizeType length, bool copy);
        bool StartObject();
        bool Key(const Ch* str, SizeType length, bool copy);
        bool EndObject(SizeType memberCount);
        bool StartArray();
        bool EndArray(SizeType elementCount);
    };
    \endcode
*/

///////////////////////////////////////////////////////////////////////////////
// BaseReaderHandler

//! Default implementation of Handler.
/*! This can be used as base class of any reader handler.
    \note implements Handler concept
*/
template <typename Encoding = UTF8<>, typename Derived = void>
struct BaseReaderHandler
{
    typedef typename Encoding::Ch Ch;

    typedef typename internal::SelectIf<internal::IsSame<Derived, void>,
                                        BaseReaderHandler,
                                        Derived>::Type Override;

    bool Default()
    {
        return true;
    }

    bool Null()
    {
        return static_cast<Override&>(*this).Default();
    }

    bool Bool(bool)
    {
        return static_cast<Override&>(*this).Default();
    }

    bool Int(int)
    {
        return static_cast<Override&>(*this).Default();
    }

    bool Uint(unsigned)
    {
        return static_cast<Override&>(*this).Default();
    }

    bool Int64(int64_t)
    {
        return static_cast<Override&>(*this).Default();
    }

    bool Uint64(uint64_t)
    {
        return static_cast<Override&>(*this).Default();
    }

    bool Double(double)
    {
        return static_cast<Override&>(*this).Default();
    }

    /// enabled via kParseNumbersAsStringsFlag, string is not null-terminated (use length)
    bool RawNumber(const Ch* str, SizeType len, bool copy)
    {
        return static_cast<Override&>(*this).String(str, len, copy);
    }

    bool String(const Ch*, SizeType, bool)
    {
        return static_cast<Override&>(*this).Default();
    }

    bool StartObject()
    {
        return static_cast<Override&>(*this).Default();
    }

    bool Key(const Ch* str, SizeType len, bool copy)
    {
        return static_cast<Override&>(*this).String(str, len, copy);
    }

    bool EndObject(SizeType)
    {
        return static_cast<Override&>(*this).Default();
    }

    bool StartArray()
    {
        return static_cast<Override&>(*this).Default();
    }

    bool EndArray(SizeType)
    {
        return static_cast<Override&>(*this).Default();
    }
};

namespace internal
{

///////////////////////////////////////////////////////////////////////////////
// Reading the whole input of a Stream

//! Minimal output stream appending UTF-8 code units to a std::string.
struct Utf8StringSink
{
    typedef char Ch;

    void Put(Ch c)
    {
        s->push_back(c);
    }

    std::string* s;
};

//! Drains a generic Stream into a contiguous UTF-8 buffer.
/*! Every Stream in this library, like in RapidJSON, reports end-of-stream by
    returning a null character, which cannot legally appear in JSON text.
*/
template <typename SourceEncoding, typename InputStream>
struct StreamDrain
{
    static void Drain(InputStream& is, std::string& out)
    {
        typedef typename SourceEncoding::Ch Ch;
        if (sizeof(Ch) == 1)
        {
            for (;;)
            {
                Ch c = is.Take();
                if (c == static_cast<Ch>(0))
                    break;
                out += static_cast<char>(c);
            }
        }
        else
        {
            // Transcode wider source encodings into UTF-8 for the scanner.
            Utf8StringSink sink{&out};

            for (;;)
            {
                if (is.Peek() == static_cast<Ch>(0))
                {
                    is.Take();
                    break;
                }
                unsigned codepoint;
                if (!SourceEncoding::Decode(is, &codepoint))
                    break;
                UTF8<char>::Encode(sink, codepoint);
            }
        }
    }

    //! Opaque record of where a drain started. Meaningless for non-seekable streams.
    typedef int Mark;

    static Mark Tell(InputStream&)
    {
        return 0;
    }

    //! Reposition the stream \c consumed UTF-8 bytes past \c mark; a no-op for streams that
    //! cannot be rewound, which are therefore left drained.
    static void Seek(InputStream&, Mark, size_t)
    {
    }
};

//! Specialization for read-only string streams: parse in place and reposition the cursor, so that
//! kParseStopWhenDoneFlag leaves the stream exactly after the parsed root value.
template <typename SourceEncoding>
struct StreamDrain<SourceEncoding, GenericStringStream<SourceEncoding>>
{
    typedef GenericStringStream<SourceEncoding> Stream;

    static void Drain(Stream& is, std::string& out)
    {
        typedef typename SourceEncoding::Ch Ch;
        if (sizeof(Ch) == 1)
        {
            const Ch* p = is.src_;
            while (*p)
                ++p;
            out.assign(reinterpret_cast<const char*>(is.src_), static_cast<size_t>(p - is.src_));
            is.src_ = p; // fully consumed by default
        }
        else
        {
            // Transcode wider source encodings into UTF-8 for the scanner, exactly as the
            // generic drain does. A raw copy of the code units would hand yyjson UTF-16 or
            // UTF-32 bytes, which are not JSON text.
            Utf8StringSink sink{&out};
            for (;;)
            {
                if (is.Peek() == static_cast<Ch>(0))
                {
                    is.Take();
                    break;
                }
                unsigned codepoint;
                if (!SourceEncoding::Decode(is, &codepoint))
                    break;
                UTF8<char>::Encode(sink, codepoint);
            }
        }
    }

    typedef const typename SourceEncoding::Ch* Mark;

    static Mark Tell(Stream& is)
    {
        return is.src_;
    }

    static void Seek(Stream& is, Mark mark, size_t consumed)
    {
        typedef typename SourceEncoding::Ch Ch;
        if (sizeof(Ch) == 1)
        {
            is.src_ = mark + consumed;
        }
        else
        {
            // `consumed` counts UTF-8 bytes, which do not map onto source code units by a
            // fixed ratio, so decode from the mark again until that many have been emitted.
            Stream walk(mark);
            std::string bytes;
            Utf8StringSink sink{&bytes};
            while (bytes.size() < consumed)
            {
                if (walk.Peek() == static_cast<Ch>(0))
                    break;
                unsigned codepoint;
                if (!SourceEncoding::Decode(walk, &codepoint))
                    break;
                UTF8<char>::Encode(sink, codepoint);
            }
            is.src_ = walk.src_;
        }
    }
};

//! Same treatment for in-situ string streams: they are random-access too, so
//! kParseStopWhenDoneFlag can leave the cursor exactly after the parsed root value.
template <typename SourceEncoding>
struct StreamDrain<SourceEncoding, GenericInsituStringStream<SourceEncoding>>
{
    typedef GenericInsituStringStream<SourceEncoding> Stream;
    typedef typename SourceEncoding::Ch* Mark;

    static Mark Tell(Stream& is)
    {
        return is.src_;
    }

    static void Drain(Stream& is, std::string& out)
    {
        typedef typename SourceEncoding::Ch Ch;
        if (sizeof(Ch) == 1)
        {
            const Ch* p = is.src_;
            while (*p)
                ++p;
            out.assign(reinterpret_cast<const char*>(is.src_), static_cast<size_t>(p - is.src_));
            is.src_ = const_cast<Ch*>(p); // fully consumed by default
        }
        else
        {
            Utf8StringSink sink{&out};
            for (;;)
            {
                if (is.Peek() == static_cast<Ch>(0))
                {
                    is.Take();
                    break;
                }
                unsigned codepoint;
                if (!SourceEncoding::Decode(is, &codepoint))
                    break;
                UTF8<char>::Encode(sink, codepoint);
            }
        }
    }

    static void Seek(Stream& is, Mark mark, size_t consumed)
    {
        typedef typename SourceEncoding::Ch Ch;
        if (sizeof(Ch) == 1)
        {
            is.src_ = mark + consumed;
        }
        else
        {
            Stream walk(mark);
            std::string bytes;
            Utf8StringSink sink{&bytes};
            while (bytes.size() < consumed)
            {
                if (walk.Peek() == static_cast<Ch>(0))
                    break;
                unsigned codepoint;
                if (!SourceEncoding::Decode(walk, &codepoint))
                    break;
                UTF8<char>::Encode(sink, codepoint);
            }
            is.src_ = walk.src_;
        }
    }
};

//! Rewrites the non-standard \' escape (kParseEscapedApostropheFlag) into a plain apostrophe,
//! which yyjson accepts. Comments are skipped so that quotes inside them cannot desynchronise the
//! in-string tracking.
inline void
RewriteEscapedApostrophes(std::string& buf, bool allowComments)
{
    size_t w = 0;
    bool inStr = false;
    for (size_t r = 0; r < buf.size(); ++r)
    {
        const char c = buf[r];
        if (inStr)
        {
            if (c == '\\' && r + 1 < buf.size())
            {
                const char n = buf[r + 1];
                ++r;
                if (n == '\'')
                {
                    buf[w++] = '\'';
                    continue;
                }
                buf[w++] = c;
                buf[w++] = n;
                continue;
            }
            if (c == '"')
                inStr = false;
            buf[w++] = c;
            continue;
        }

        if (allowComments && c == '/' && r + 1 < buf.size())
        {
            const char n = buf[r + 1];
            if (n == '/')
            {
                while (r < buf.size() && buf[r] != '\n')
                    buf[w++] = buf[r++];
                if (r < buf.size())
                    buf[w++] = buf[r];
                continue;
            }
            if (n == '*')
            {
                buf[w++] = buf[r++];
                buf[w++] = buf[r++];
                while (r < buf.size() && !(buf[r] == '*' && r + 1 < buf.size() && buf[r + 1] == '/'))
                    buf[w++] = buf[r++];
                if (r < buf.size())
                    buf[w++] = buf[r++];
                if (r < buf.size())
                    buf[w++] = buf[r];
                continue;
            }
        }

        if (c == '"')
            inStr = true;
        buf[w++] = c;
    }
    buf.resize(w);
}

//! Maps a yyjson read error onto the closest RapidJSON parse error code.
inline ParseErrorCode
MapReadError(uint32_t code, const char* msg)
{
    switch (code)
    {
    case YYJSON_READ_ERROR_EMPTY_CONTENT:
        return kParseErrorDocumentEmpty;
    case YYJSON_READ_ERROR_UNEXPECTED_CONTENT:
        return kParseErrorDocumentRootNotSingular;
    case YYJSON_READ_ERROR_INVALID_NUMBER:
        if (msg && std::strstr(msg, "exponent"))
            return kParseErrorNumberMissExponent;
        if (msg && std::strstr(msg, "fraction"))
            return kParseErrorNumberMissFraction;
        if (msg && (std::strstr(msg, "big") || std::strstr(msg, "overflow") ||
                    std::strstr(msg, "range")))
            return kParseErrorNumberTooBig;
        return kParseErrorValueInvalid;
    case YYJSON_READ_ERROR_INVALID_STRING:
        if (msg && std::strstr(msg, "escape"))
            return kParseErrorStringEscapeInvalid;
        if (msg && std::strstr(msg, "UTF-8"))
            return kParseErrorStringInvalidEncoding;
        if (msg && std::strstr(msg, "unicode"))
            return kParseErrorStringUnicodeEscapeInvalidHex;
        return kParseErrorStringInvalidEncoding;
    case YYJSON_READ_ERROR_LITERAL:
        return kParseErrorValueInvalid;
    case YYJSON_READ_ERROR_JSON_STRUCTURE:
        return kParseErrorUnspecificSyntaxError;
    case YYJSON_READ_ERROR_UNEXPECTED_END:
        if (msg && std::strstr(msg, "string"))
            return kParseErrorStringMissQuotationMark;
        return kParseErrorUnspecificSyntaxError;
    case YYJSON_READ_ERROR_UNEXPECTED_CHARACTER:
        if (msg && std::strstr(msg, "colon"))
            return kParseErrorObjectMissColon;
        if (msg && std::strstr(msg, "name"))
            return kParseErrorObjectMissName;
        if (msg && std::strstr(msg, "}"))
            return kParseErrorObjectMissCommaOrCurlyBracket;
        if (msg && std::strstr(msg, "]"))
            return kParseErrorArrayMissCommaOrSquareBracket;
        return kParseErrorValueInvalid;
    default:
        return kParseErrorUnspecificSyntaxError;
    }
}

//! Replays a parsed yyjson tree as a sequence of SAX events, without recursion.
template <typename TargetEncoding, typename Handler>
bool
EmitEvents(yyjson_val* root, Handler& handler)
{
    typedef typename TargetEncoding::Ch Ch;

    struct Frame
    {
        bool isObj;
        SizeType count;
        yyjson_arr_iter a;
        yyjson_obj_iter o;
    };

    // Buffer used when the DOM encoding is not byte-sized and strings have to be transcoded.
    std::vector<Ch> conv;

    struct StringOut
    {
        typedef typename TargetEncoding::Ch Ch;

        void Put(Ch c)
        {
            v->push_back(c);
        }

        std::vector<typename TargetEncoding::Ch>* v;
    };

    std::vector<Frame> stack;
    yyjson_val* v = root;

#define RAPIDYYJSON_EMIT_STR(CALL, S, L)                                                           \
    do                                                                                             \
    {                                                                                              \
        if (sizeof(Ch) == 1)                                                                       \
        {                                                                                          \
            if (!handler.CALL(reinterpret_cast<const Ch*>(S), static_cast<SizeType>(L), true))     \
                return false;                                                                      \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            conv.clear();                                                                          \
            GenericStringStream<UTF8<char>> src((S));                                              \
            StringOut sink{&conv};                                                                 \
            const char* end = (S) + (L);                                                           \
            while (src.src_ < end)                                                                 \
            {                                                                                      \
                unsigned cp;                                                                       \
                if (!UTF8<char>::Decode(src, &cp))                                                 \
                    return false;                                                                  \
                TargetEncoding::Encode(sink, cp);                                                  \
            }                                                                                      \
            conv.push_back(Ch()); /* RapidJSON hands handlers a terminated string. */          \
            if (!handler.CALL(&conv[0], static_cast<SizeType>(conv.size() - 1), true))             \
                return false;                                                                      \
        }                                                                                          \
    } while (0)

descend:
    switch (yyjson_get_type(v))
    {
    case YYJSON_TYPE_NULL:
        if (!handler.Null())
            return false;
        break;
    case YYJSON_TYPE_BOOL:
        if (!handler.Bool(yyjson_get_bool(v)))
            return false;
        break;
    case YYJSON_TYPE_NUM:
        switch (yyjson_get_subtype(v))
        {
        case YYJSON_SUBTYPE_UINT: {
            const uint64_t u = yyjson_get_uint(v);
            if (u <= 0xFFFFFFFFu)
            {
                if (!handler.Uint(static_cast<unsigned>(u)))
                    return false;
            }
            else if (!handler.Uint64(u))
                return false;
            break;
        }
        case YYJSON_SUBTYPE_SINT: {
            const int64_t i = yyjson_get_sint(v);
            if (i >= -2147483647LL - 1 && i <= 2147483647LL)
            {
                if (!handler.Int(static_cast<int>(i)))
                    return false;
            }
            else if (!handler.Int64(i))
                return false;
            break;
        }
        default:
            if (!handler.Double(yyjson_get_real(v)))
                return false;
            break;
        }
        break;
    case YYJSON_TYPE_RAW:
        // Produced by kParseNumbersAsStringsFlag.
        RAPIDYYJSON_EMIT_STR(RawNumber, yyjson_get_raw(v), yyjson_get_len(v));
        break;
    case YYJSON_TYPE_STR:
        RAPIDYYJSON_EMIT_STR(String, yyjson_get_str(v), yyjson_get_len(v));
        break;
    case YYJSON_TYPE_ARR: {
        if (!handler.StartArray())
            return false;
        Frame f;
        f.isObj = false;
        f.count = static_cast<SizeType>(yyjson_arr_size(v));
        yyjson_arr_iter_init(v, &f.a);
        stack.push_back(f);
        break;
    }
    case YYJSON_TYPE_OBJ: {
        if (!handler.StartObject())
            return false;
        Frame f;
        f.isObj = true;
        f.count = static_cast<SizeType>(yyjson_obj_size(v));
        yyjson_obj_iter_init(v, &f.o);
        stack.push_back(f);
        break;
    }
    default:
        return false;
    }

    while (!stack.empty())
    {
        Frame& f = stack.back();
        if (f.isObj)
        {
            yyjson_val* key = yyjson_obj_iter_next(&f.o);
            if (key)
            {
                RAPIDYYJSON_EMIT_STR(Key, yyjson_get_str(key), yyjson_get_len(key));
                v = yyjson_obj_iter_get_val(key);
                goto descend;
            }
            const SizeType n = f.count;
            stack.pop_back();
            if (!handler.EndObject(n))
                return false;
        }
        else
        {
            yyjson_val* e = yyjson_arr_iter_next(&f.a);
            if (e)
            {
                v = e;
                goto descend;
            }
            const SizeType n = f.count;
            stack.pop_back();
            if (!handler.EndArray(n))
                return false;
        }
    }

#undef RAPIDYYJSON_EMIT_STR
    return true;
}

//! Translates RapidJSON parse flags into yyjson read flags.
inline yyjson_read_flag
MapParseFlags(unsigned parseFlags)
{
    yyjson_read_flag flg = 0;
    if (parseFlags & kParseCommentsFlag)
        flg |= YYJSON_READ_ALLOW_COMMENTS;
    if (parseFlags & kParseTrailingCommasFlag)
        flg |= YYJSON_READ_ALLOW_TRAILING_COMMAS;
    if (parseFlags & kParseNanAndInfFlag)
        flg |= YYJSON_READ_ALLOW_INF_AND_NAN;
    if (parseFlags & kParseStopWhenDoneFlag)
        flg |= YYJSON_READ_STOP_WHEN_DONE;
    if (parseFlags & kParseNumbersAsStringsFlag)
        flg |= YYJSON_READ_NUMBER_AS_RAW;
    // kParseFullPrecisionFlag: yyjson always parses numbers at full precision.
    // kParseValidateEncodingFlag: yyjson always validates UTF-8 input.
    // kParseIterativeFlag: the scanner is never recursive.
    return flg;
}

} // namespace internal

///////////////////////////////////////////////////////////////////////////////
// GenericReader

//! SAX-style JSON parser. Use \ref Reader for UTF8 encoding and default allocator.
/*! \tparam SourceEncoding Encoding of the input stream.
    \tparam TargetEncoding Encoding of the parse output.
    \tparam StackAllocator Allocator type for stack.
*/
template <typename SourceEncoding,
          typename TargetEncoding,
          typename StackAllocator = CrtAllocator>
class GenericReader
{
  public:
    typedef typename SourceEncoding::Ch Ch;

    //! Constructor.
    /*! \param stackAllocator Optional allocator for allocating stack memory. (Only use for
                              non-destructive parsing)
        \param stackCapacity stack capacity in bytes for storing a single decoded string.
    */
    GenericReader(StackAllocator* stackAllocator = 0, size_t stackCapacity = kDefaultStackCapacity)
        : stackAllocator_(stackAllocator),
          stackCapacity_(stackCapacity),
          parseResult_(),
          state_(IterativeParsingStartState),
          events_(),
          eventPos_(0)
    {
        (void)stackAllocator_;
        (void)stackCapacity_;
    }

    //! Parse JSON text.
    template <unsigned parseFlags, typename InputStream, typename Handler>
    ParseResult Parse(InputStream& is, Handler& handler)
    {
        parseResult_.Clear();

        std::string buffer;
        typedef internal::StreamDrain<SourceEncoding, InputStream> Drainer;
        const typename Drainer::Mark mark = Drainer::Tell(is);
        Drainer::Drain(is, buffer);

        if (parseFlags & kParseEscapedApostropheFlag)
            internal::RewriteEscapedApostrophes(buffer, (parseFlags & kParseCommentsFlag) != 0);

        if (buffer.empty())
        {
            // yyjson reports a zero-length input as an invalid parameter; RapidJSON reports an
            // empty document.
            parseResult_.Set(kParseErrorDocumentEmpty, 0);
            return parseResult_;
        }

        yyjson_read_err err;
        std::memset(&err, 0, sizeof(err));
        // Always hand yyjson a valid pointer: a zero-length buffer must be reported as an empty
        // document, not as an invalid parameter.
        yyjson_doc* doc = yyjson_read_opts(&buffer[0],
                                           buffer.size(),
                                           internal::MapParseFlags(parseFlags),
                                           0,
                                           &err);
        if (!doc)
        {
            parseResult_.Set(internal::MapReadError(err.code, err.msg), err.pos);
            return parseResult_;
        }

        if (parseFlags & kParseStopWhenDoneFlag)
            Drainer::Seek(is, mark, yyjson_doc_get_read_size(doc));

        const bool ok = internal::EmitEvents<TargetEncoding>(yyjson_doc_get_root(doc), handler);
        const size_t read = yyjson_doc_get_read_size(doc);
        yyjson_doc_free(doc);

        if (!ok)
            parseResult_.Set(kParseErrorTermination, read);
        return parseResult_;
    }

    //! Parse JSON text (with kParseDefaultFlags)
    template <typename InputStream, typename Handler>
    ParseResult Parse(InputStream& is, Handler& handler)
    {
        return Parse<kParseDefaultFlags>(is, handler);
    }

    //! Initialize JSON text token-by-token parsing
    void IterativeParseInit()
    {
        parseResult_.Clear();
        state_ = IterativeParsingStartState;
        events_.clear();
        eventPos_ = 0;
    }

    //! Parse one token from JSON text
    template <unsigned parseFlags, typename InputStream, typename Handler>
    bool IterativeParseNext(InputStream& is, Handler& handler)
    {
        if (state_ == IterativeParsingStartState)
        {
            EventCollector collector(events_);
            Parse<parseFlags>(is, collector);
            if (parseResult_.IsError())
            {
                state_ = IterativeParsingErrorState;
                return false;
            }
            state_ = IterativeParsingIterativeState;
            eventPos_ = 0;
        }

        if (state_ != IterativeParsingIterativeState)
            return false;

        if (eventPos_ >= events_.size())
        {
            state_ = IterativeParsingFinishState;
            return false;
        }

        const Event& e = events_[eventPos_++];
        if (!e.Replay(handler))
        {
            parseResult_.Set(kParseErrorTermination, 0);
            state_ = IterativeParsingErrorState;
            return false;
        }
        if (eventPos_ >= events_.size())
            state_ = IterativeParsingFinishState;
        return true;
    }

    //! Check if token-by-token parsing JSON text is complete
    bool IterativeParseComplete() const
    {
        return state_ == IterativeParsingFinishState || state_ == IterativeParsingErrorState;
    }

    //! Whether a parse error has occurred in the last parsing.
    bool HasParseError() const
    {
        return parseResult_.IsError();
    }

    //! Get the \ref ParseErrorCode of last parsing.
    ParseErrorCode GetParseErrorCode() const
    {
        return parseResult_.Code();
    }

    //! Get the position of last parsing error in input, 0 otherwise.
    size_t GetErrorOffset() const
    {
        return parseResult_.Offset();
    }

  protected:
    void SetParseError(ParseErrorCode code, size_t offset)
    {
        parseResult_.Set(code, offset);
    }

  private:
    // Prohibit copy constructor & assignment operator.
    GenericReader(const GenericReader&);
    GenericReader& operator=(const GenericReader&);

    static const size_t kDefaultStackCapacity = 256;

    enum IterativeParsingState
    {
        IterativeParsingStartState,
        IterativeParsingIterativeState,
        IterativeParsingFinishState,
        IterativeParsingErrorState
    };

    //! One recorded SAX event, used only by the token-by-token interface.
    struct Event
    {
        enum Kind
        {
            kNull,
            kBool,
            kInt,
            kUint,
            kInt64,
            kUint64,
            kDouble,
            kRawNumber,
            kString,
            kStartObject,
            kKey,
            kEndObject,
            kStartArray,
            kEndArray
        };

        Kind kind;
        bool b;
        int64_t i64;
        uint64_t u64;
        double d;
        SizeType count;
        std::basic_string<typename TargetEncoding::Ch> str;

        template <typename Handler>
        bool Replay(Handler& handler) const
        {
            switch (kind)
            {
            case kNull:
                return handler.Null();
            case kBool:
                return handler.Bool(b);
            case kInt:
                return handler.Int(static_cast<int>(i64));
            case kUint:
                return handler.Uint(static_cast<unsigned>(u64));
            case kInt64:
                return handler.Int64(i64);
            case kUint64:
                return handler.Uint64(u64);
            case kDouble:
                return handler.Double(d);
            case kRawNumber:
                return handler.RawNumber(str.c_str(), static_cast<SizeType>(str.size()), true);
            case kString:
                return handler.String(str.c_str(), static_cast<SizeType>(str.size()), true);
            case kStartObject:
                return handler.StartObject();
            case kKey:
                return handler.Key(str.c_str(), static_cast<SizeType>(str.size()), true);
            case kEndObject:
                return handler.EndObject(count);
            case kStartArray:
                return handler.StartArray();
            default:
                return handler.EndArray(count);
            }
        }
    };

    //! Handler that records the event stream for IterativeParseNext().
    struct EventCollector
    {
        typedef typename TargetEncoding::Ch Ch;

        EventCollector(std::vector<Event>& out)
            : out_(out)
        {
        }

        Event& New(typename Event::Kind k)
        {
            out_.push_back(Event());
            Event& e = out_.back();
            e.kind = k;
            e.b = false;
            e.i64 = 0;
            e.u64 = 0;
            e.d = 0;
            e.count = 0;
            return e;
        }

        bool Null()
        {
            New(Event::kNull);
            return true;
        }

        bool Bool(bool v)
        {
            New(Event::kBool).b = v;
            return true;
        }

        bool Int(int v)
        {
            New(Event::kInt).i64 = v;
            return true;
        }

        bool Uint(unsigned v)
        {
            New(Event::kUint).u64 = v;
            return true;
        }

        bool Int64(int64_t v)
        {
            New(Event::kInt64).i64 = v;
            return true;
        }

        bool Uint64(uint64_t v)
        {
            New(Event::kUint64).u64 = v;
            return true;
        }

        bool Double(double v)
        {
            New(Event::kDouble).d = v;
            return true;
        }

        bool RawNumber(const Ch* s, SizeType len, bool)
        {
            New(Event::kRawNumber).str.assign(s, len);
            return true;
        }

        bool String(const Ch* s, SizeType len, bool)
        {
            New(Event::kString).str.assign(s, len);
            return true;
        }

        bool StartObject()
        {
            New(Event::kStartObject);
            return true;
        }

        bool Key(const Ch* s, SizeType len, bool)
        {
            New(Event::kKey).str.assign(s, len);
            return true;
        }

        bool EndObject(SizeType n)
        {
            New(Event::kEndObject).count = n;
            return true;
        }

        bool StartArray()
        {
            New(Event::kStartArray);
            return true;
        }

        bool EndArray(SizeType n)
        {
            New(Event::kEndArray).count = n;
            return true;
        }

        std::vector<Event>& out_;
    };

    StackAllocator* stackAllocator_;
    size_t stackCapacity_;
    ParseResult parseResult_;
    IterativeParsingState state_;
    std::vector<Event> events_;
    size_t eventPos_;
};

//! Reader with UTF8 encoding and default allocator.
typedef GenericReader<UTF8<>, UTF8<>> Reader;

RAPIDYYJSON_NAMESPACE_END

#endif // RAPIDYYJSON_READER_H_
