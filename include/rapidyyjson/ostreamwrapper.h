/*
 * rapidyyjson - a RapidJSON-compatible API implemented on top of yyjson.
 *
 * Mirrors `rapidjson/ostreamwrapper.h`.
 */

#ifndef RAPIDYYJSON_OSTREAMWRAPPER_H_
#define RAPIDYYJSON_OSTREAMWRAPPER_H_

#include "rapidyyjson.h"
#include "stream.h"

#include <iosfwd>
#include <ostream>

RAPIDYYJSON_NAMESPACE_BEGIN

//! Wrapper of \c std::basic_ostream into RapidYYJSON's Stream concept.
/*!
    \tparam StreamType Class derived from \c std::basic_ostream.
*/
template <typename StreamType>
class BasicOStreamWrapper
{
  public:
    typedef typename StreamType::char_type Ch;

    BasicOStreamWrapper(StreamType& stream)
        : stream_(stream)
    {
    }

    void Put(Ch c)
    {
        stream_.put(c);
    }

    void Flush()
    {
        stream_.flush();
    }

    // Not implemented
    char Peek() const
    {
        RAPIDYYJSON_ASSERT(false);
        return 0;
    }

    char Take()
    {
        RAPIDYYJSON_ASSERT(false);
        return 0;
    }

    size_t Tell() const
    {
        RAPIDYYJSON_ASSERT(false);
        return 0;
    }

    char* PutBegin()
    {
        RAPIDYYJSON_ASSERT(false);
        return 0;
    }

    size_t PutEnd(char*)
    {
        RAPIDYYJSON_ASSERT(false);
        return 0;
    }

  private:
    BasicOStreamWrapper(const BasicOStreamWrapper&);
    BasicOStreamWrapper& operator=(const BasicOStreamWrapper&);

    StreamType& stream_;
};

typedef BasicOStreamWrapper<std::ostream> OStreamWrapper;
typedef BasicOStreamWrapper<std::wostream> WOStreamWrapper;

RAPIDYYJSON_NAMESPACE_END

#endif // RAPIDYYJSON_OSTREAMWRAPPER_H_
