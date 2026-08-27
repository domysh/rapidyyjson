/*
 * rapidyyjson - a RapidJSON-compatible API implemented on top of yyjson.
 *
 * Stream concept and basic in-memory streams, mirroring `rapidjson/stream.h`.
 */

#ifndef RAPIDYYJSON_STREAM_H_
#define RAPIDYYJSON_STREAM_H_

#include "encodings.h"
#include "rapidyyjson.h"

RAPIDYYJSON_NAMESPACE_BEGIN

///////////////////////////////////////////////////////////////////////////////
//  Stream
/*! \class rapidyyjson::Stream
    \brief Concept for reading and writing characters.

    \code
    concept Stream {
        typename Ch;    //!< Character type of the stream.

        //! Read the current character from stream without moving the read cursor.
        Ch Peek() const;

        //! Read the current character from stream and moving the read cursor to next character.
        Ch Take();

        //! Get the current read cursor.
        size_t Tell();

        //! Begin writing operation at the current read pointer.
        Ch* PutBegin();

        //! Write a character.
        void Put(Ch c);

        //! Flush the buffer.
        void Flush();

        //! End the writing operation.
        size_t PutEnd(Ch* begin);
    }
    \endcode
*/

//! Provides additional information for stream.
template <typename Stream>
struct StreamTraits
{
    //! Whether to make local copy of stream for optimization during parsing.
    enum
    {
        copyOptimization = 0
    };
};

//! Reserve n characters for writing to a stream.
template <typename Stream>
inline void
PutReserve(Stream& stream, size_t count)
{
    (void)stream;
    (void)count;
}

//! Write character to a stream, presuming buffer is reserved.
template <typename Stream>
inline void
PutUnsafe(Stream& stream, typename Stream::Ch c)
{
    stream.Put(c);
}

//! Put N copies of a character to a stream.
template <typename Stream, typename Ch>
inline void
PutN(Stream& stream, Ch c, size_t n)
{
    PutReserve(stream, n);
    for (size_t i = 0; i < n; i++)
        PutUnsafe(stream, c);
}

///////////////////////////////////////////////////////////////////////////////
// GenericStreamWrapper

//! A Stream Wrapper
/*! \tThis string stream is a wrapper for any stream by just forwarding any
    \treceived message to the origin stream.
    \note implements Stream concept
*/
template <typename InputStream, typename Encoding = UTF8<>>
class GenericStreamWrapper
{
  public:
    typedef typename Encoding::Ch Ch;

    GenericStreamWrapper(InputStream& is)
        : is_(is)
    {
    }

    Ch Peek() const
    {
        return is_.Peek();
    }

    Ch Take()
    {
        return is_.Take();
    }

    size_t Tell()
    {
        return is_.Tell();
    }

    Ch* PutBegin()
    {
        return is_.PutBegin();
    }

    void Put(Ch ch)
    {
        is_.Put(ch);
    }

    void Flush()
    {
        is_.Flush();
    }

    size_t PutEnd(Ch* ch)
    {
        return is_.PutEnd(ch);
    }

    // wrapper for MemoryStream
    const Ch* Peek4() const
    {
        return is_.Peek4();
    }

    // wrapper for AutoUTFInputStream
    UTFType GetType() const
    {
        return is_.GetType();
    }

    bool HasBOM() const
    {
        return is_.HasBOM();
    }

  protected:
    InputStream& is_;
};

///////////////////////////////////////////////////////////////////////////////
// StringStream

//! Read-only string stream.
/*! \note implements Stream concept
*/
template <typename Encoding>
struct GenericStringStream
{
    typedef typename Encoding::Ch Ch;

    GenericStringStream(const Ch* src)
        : src_(src),
          head_(src)
    {
    }

    Ch Peek() const
    {
        return *src_;
    }

    Ch Take()
    {
        return *src_++;
    }

    size_t Tell() const
    {
        return static_cast<size_t>(src_ - head_);
    }

    Ch* PutBegin()
    {
        RAPIDYYJSON_ASSERT(false);
        return 0;
    }

    void Put(Ch)
    {
        RAPIDYYJSON_ASSERT(false);
    }

    void Flush()
    {
        RAPIDYYJSON_ASSERT(false);
    }

    size_t PutEnd(Ch*)
    {
        RAPIDYYJSON_ASSERT(false);
        return 0;
    }

    const Ch* src_;  //!< Current read position.
    const Ch* head_; //!< Original head of the string.
};

template <typename Encoding>
struct StreamTraits<GenericStringStream<Encoding>>
{
    enum
    {
        copyOptimization = 1
    };
};

//! String stream with UTF8 encoding.
typedef GenericStringStream<UTF8<>> StringStream;

///////////////////////////////////////////////////////////////////////////////
// InsituStringStream

//! A read-write string stream.
/*! This string stream is particularly designed for in-situ parsing.
    \note implements Stream concept
*/
template <typename Encoding>
struct GenericInsituStringStream
{
    typedef typename Encoding::Ch Ch;

    GenericInsituStringStream(Ch* src)
        : src_(src),
          dst_(0),
          head_(src)
    {
    }

    // Read
    Ch Peek()
    {
        return *src_;
    }

    Ch Take()
    {
        return *src_++;
    }

    size_t Tell()
    {
        return static_cast<size_t>(src_ - head_);
    }

    // Write
    void Put(Ch c)
    {
        RAPIDYYJSON_ASSERT(dst_ != 0);
        *dst_++ = c;
    }

    Ch* PutBegin()
    {
        return dst_ = src_;
    }

    size_t PutEnd(Ch* begin)
    {
        return static_cast<size_t>(dst_ - begin);
    }

    void Flush()
    {
    }

    Ch* Push(size_t count)
    {
        Ch* begin = dst_;
        dst_ += count;
        return begin;
    }

    void Pop(size_t count)
    {
        dst_ -= count;
    }

    Ch* src_;
    Ch* dst_;
    Ch* head_;
};

template <typename Encoding>
struct StreamTraits<GenericInsituStringStream<Encoding>>
{
    enum
    {
        copyOptimization = 1
    };
};

//! Insitu string stream with UTF8 encoding.
typedef GenericInsituStringStream<UTF8<>> InsituStringStream;

RAPIDYYJSON_NAMESPACE_END

#endif // RAPIDYYJSON_STREAM_H_
