/*
 * rapidyyjson - a RapidJSON-compatible API implemented on top of yyjson.
 *
 * Mirrors `rapidjson/istreamwrapper.h`.
 */

#ifndef RAPIDYYJSON_ISTREAMWRAPPER_H_
#define RAPIDYYJSON_ISTREAMWRAPPER_H_

#include "rapidyyjson.h"
#include "stream.h"

#include <iosfwd>
#include <istream>
#include <ios>

RAPIDYYJSON_NAMESPACE_BEGIN

//! Wrapper of \c std::basic_istream into RapidYYJSON's Stream concept.
/*!
    The classes can be wrapped including but not limited to:

    - \c std::istringstream
    - \c std::stringstream
    - \c std::wistringstream
    - \c std::wstringstream
    - \c std::ifstream
    - \c std::fstream
    - \c std::wifstream
    - \c std::wfstream

    \tparam StreamType Class derived from \c std::basic_istream.
*/
template <typename StreamType>
class BasicIStreamWrapper
{
  public:
    typedef typename StreamType::char_type Ch;

    //! Constructor.
    BasicIStreamWrapper(StreamType& stream)
        : stream_(stream),
          buffer_(peekBuffer_),
          bufferSize_(4),
          bufferLast_(0),
          current_(buffer_),
          readCount_(0),
          count_(0),
          eof_(false)
    {
        Read();
    }

    //! Constructor.
    BasicIStreamWrapper(StreamType& stream, char* buffer, size_t bufferSize)
        : stream_(stream),
          buffer_(buffer),
          bufferSize_(bufferSize),
          bufferLast_(0),
          current_(buffer_),
          readCount_(0),
          count_(0),
          eof_(false)
    {
        RAPIDYYJSON_ASSERT(bufferSize >= 4);
        Read();
    }

    Ch Peek() const
    {
        return *current_;
    }

    Ch Take()
    {
        Ch c = *current_;
        Read();
        return c;
    }

    // tellg() may return -1 when failed. So we count by ourself.
    size_t Tell() const
    {
        return count_ + static_cast<size_t>(current_ - buffer_);
    }

    // Not implemented
    void Put(Ch)
    {
        RAPIDYYJSON_ASSERT(false);
    }

    void Flush()
    {
        RAPIDYYJSON_ASSERT(false);
    }

    Ch* PutBegin()
    {
        RAPIDYYJSON_ASSERT(false);
        return 0;
    }

    size_t PutEnd(Ch*)
    {
        RAPIDYYJSON_ASSERT(false);
        return 0;
    }

    // For encoding detection only.
    const Ch* Peek4() const
    {
        return (current_ + 4 - !eof_ <= bufferLast_) ? current_ : 0;
    }

  private:
    BasicIStreamWrapper();
    BasicIStreamWrapper(const BasicIStreamWrapper&);
    BasicIStreamWrapper& operator=(const BasicIStreamWrapper&);

    void Read()
    {
        if (current_ < bufferLast_)
            ++current_;
        else if (!eof_)
        {
            count_ += readCount_;
            readCount_ = bufferSize_;
            bufferLast_ = buffer_ + readCount_ - 1;
            current_ = buffer_;

            if (!stream_.read(buffer_, static_cast<std::streamsize>(bufferSize_)))
            {
                readCount_ = static_cast<size_t>(stream_.gcount());
                *(bufferLast_ = buffer_ + readCount_) = '\0';
                eof_ = true;
            }
        }
    }

    StreamType& stream_;
    Ch peekBuffer_[4];
    Ch* buffer_;
    size_t bufferSize_;
    Ch* bufferLast_;
    Ch* current_;
    size_t readCount_;
    size_t count_; //!< Number of characters read
    bool eof_;
};

typedef BasicIStreamWrapper<std::istream> IStreamWrapper;
typedef BasicIStreamWrapper<std::wistream> WIStreamWrapper;

RAPIDYYJSON_NAMESPACE_END

#endif // RAPIDYYJSON_ISTREAMWRAPPER_H_
