/*
 * rapidyyjson - a RapidJSON-compatible API implemented on top of yyjson.
 *
 * String helpers, mirroring `rapidjson/internal/strfunc.h`.
 */

#ifndef RAPIDYYJSON_INTERNAL_STRFUNC_H_
#define RAPIDYYJSON_INTERNAL_STRFUNC_H_

#include "../encodings.h"
#include "../stream.h"

RAPIDYYJSON_NAMESPACE_BEGIN
namespace internal
{

//! Custom strlen() which works on different character types.
template <typename Ch>
inline SizeType
StrLen(const Ch* s)
{
    RAPIDYYJSON_ASSERT(s != 0);
    const Ch* p = s;
    while (*p)
        ++p;
    return SizeType(p - s);
}

template <>
inline SizeType
StrLen(const char* s)
{
    return SizeType(std::strlen(s));
}

//! Custom strcmpn() which works on different character types.
template <typename Ch>
inline int
StrCmp(const Ch* s1, const Ch* s2)
{
    RAPIDYYJSON_ASSERT(s1 != 0);
    RAPIDYYJSON_ASSERT(s2 != 0);
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return static_cast<unsigned>(*s1) < static_cast<unsigned>(*s2)
               ? -1
               : static_cast<unsigned>(*s1) > static_cast<unsigned>(*s2);
}

//! Returns number of code points in a encoded string.
template <typename Encoding>
bool
CountStringCodePoint(const typename Encoding::Ch* s, SizeType length, SizeType* outCount)
{
    RAPIDYYJSON_ASSERT(s != 0);
    RAPIDYYJSON_ASSERT(outCount != 0);
    GenericStringStream<Encoding> is(s);
    const typename Encoding::Ch* end = s + length;
    SizeType count = 0;
    while (is.src_ < end)
    {
        unsigned codepoint;
        if (!Encoding::Decode(is, &codepoint))
            return false;
        count++;
    }
    *outCount = count;
    return true;
}

} // namespace internal
RAPIDYYJSON_NAMESPACE_END

#endif // RAPIDYYJSON_INTERNAL_STRFUNC_H_
