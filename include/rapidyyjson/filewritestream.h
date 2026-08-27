/*
 * rapidyyjson - a RapidJSON-compatible API implemented on top of yyjson.
 *
 * Mirrors `rapidjson/filewritestream.h`.
 */

#ifndef RAPIDYYJSON_FILEWRITESTREAM_H_
#define RAPIDYYJSON_FILEWRITESTREAM_H_

#include "rapidyyjson.h"
#include "stream.h"

#include <cstdio>

RAPIDYYJSON_NAMESPACE_BEGIN

//! Wrapper of C file stream for output using fwrite().
/*!
    \note implements Stream concept
*/
class FileWriteStream
{
  public:
    typedef char Ch; //!< Character type. Only support char.

    FileWriteStream(std::FILE* fp, char* buffer, size_t bufferSize)
        : fp_(fp),
          buffer_(buffer),
          bufferEnd_(buffer + bufferSize),
          current_(buffer_)
    {
        RAPIDYYJSON_ASSERT(fp_ != 0);
    }

    void Put(char c)
    {
        if (current_ >= bufferEnd_)
            Flush();

        *current_++ = c;
    }

    void PutN(char c, size_t n)
    {
        size_t avail = static_cast<size_t>(bufferEnd_ - current_);
        while (n > avail)
        {
            std::memset(current_, c, avail);
            current_ += avail;
            Flush();
            n -= avail;
            avail = static_cast<size_t>(bufferEnd_ - current_);
        }

        if (n > 0)
        {
            std::memset(current_, c, n);
            current_ += n;
        }
    }

    void Flush()
    {
        if (current_ != buffer_)
        {
            size_t result =
                std::fwrite(buffer_, 1, static_cast<size_t>(current_ - buffer_), fp_);
            if (result < static_cast<size_t>(current_ - buffer_))
            {
                // failure deliberately ignored at this time
                // added to avoid warn_unused_result build errors
            }
            current_ = buffer_;
        }
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
    // Prohibit copy constructor & assignment operator.
    FileWriteStream(const FileWriteStream&);
    FileWriteStream& operator=(const FileWriteStream&);

    std::FILE* fp_;
    char* buffer_;
    char* bufferEnd_;
    char* current_;
};

//! Implement specialized version of PutN() with memset() for better performance.
template <>
inline void
PutN(FileWriteStream& stream, char c, size_t n)
{
    stream.PutN(c, n);
}

RAPIDYYJSON_NAMESPACE_END

#endif // RAPIDYYJSON_FILEWRITESTREAM_H_
