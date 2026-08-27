/*
 * rapidyyjson - a RapidJSON-compatible API implemented on top of yyjson.
 *
 * In-memory output stream, mirroring `rapidjson/stringbuffer.h`.
 */

#ifndef RAPIDYYJSON_STRINGBUFFER_H_
#define RAPIDYYJSON_STRINGBUFFER_H_

#include "internal/stack.h"
#include "rapidyyjson.h"
#include "stream.h"

#include <utility>

RAPIDYYJSON_NAMESPACE_BEGIN

//! Represents an in-memory output stream.
/*!
    \tparam Encoding Encoding of the stream.
    \tparam Allocator type for allocating memory buffer.
    \note implements Stream concept
*/
template <typename Encoding, typename Allocator = CrtAllocator>
class GenericStringBuffer
{
  public:
    typedef typename Encoding::Ch Ch;

    GenericStringBuffer(Allocator* allocator = 0, size_t capacity = kDefaultCapacity)
        : stack_(allocator, capacity)
    {
    }

    GenericStringBuffer(GenericStringBuffer&& rhs)
        : stack_(std::move(rhs.stack_))
    {
    }

    GenericStringBuffer& operator=(GenericStringBuffer&& rhs)
    {
        if (&rhs != this)
            stack_ = std::move(rhs.stack_);
        return *this;
    }

    void Put(Ch c)
    {
        *stack_.template Push<Ch>() = c;
    }

    void PutUnsafe(Ch c)
    {
        *stack_.template PushUnsafe<Ch>() = c;
    }

    void Flush()
    {
    }

    void Clear()
    {
        stack_.Clear();
    }

    void ShrinkToFit()
    {
        // Push and pop a null terminator. This is safe.
        *stack_.template Push<Ch>() = '\0';
        stack_.ShrinkToFit();
        stack_.template Pop<Ch>(1);
    }

    void Reserve(size_t count)
    {
        stack_.template Reserve<Ch>(count);
    }

    Ch* Push(size_t count)
    {
        return stack_.template Push<Ch>(count);
    }

    Ch* PushUnsafe(size_t count)
    {
        return stack_.template PushUnsafe<Ch>(count);
    }

    void Pop(size_t count)
    {
        stack_.template Pop<Ch>(count);
    }

    const Ch* GetString() const
    {
        // Push and pop a null terminator. This is safe.
        *stack_.template Push<Ch>() = '\0';
        stack_.template Pop<Ch>(1);

        return stack_.template Bottom<Ch>();
    }

    //! Get the size of string in bytes in the string buffer.
    size_t GetSize() const
    {
        return stack_.GetSize();
    }

    //! Get the maximum size of the string buffer in bytes.
    size_t GetLength() const
    {
        return stack_.GetSize() / sizeof(Ch);
    }

    static const size_t kDefaultCapacity = 256;
    mutable internal::Stack<Allocator> stack_;

  private:
    // Prohibit copy constructor & assignment operator.
    GenericStringBuffer(const GenericStringBuffer&);
    GenericStringBuffer& operator=(const GenericStringBuffer&);
};

//! String buffer with UTF8 encoding
typedef GenericStringBuffer<UTF8<>> StringBuffer;

template <typename Encoding, typename Allocator>
inline void
PutReserve(GenericStringBuffer<Encoding, Allocator>& stream, size_t count)
{
    stream.Reserve(count);
}

template <typename Encoding, typename Allocator>
inline void
PutUnsafe(GenericStringBuffer<Encoding, Allocator>& stream, typename Encoding::Ch c)
{
    stream.PutUnsafe(c);
}

//! Implement specialized version of PutN() with memset() for better performance.
template <>
inline void
PutN(GenericStringBuffer<UTF8<>>& stream, char c, size_t n)
{
    std::memset(stream.stack_.Push<char>(n), c, n * sizeof(c));
}

RAPIDYYJSON_NAMESPACE_END

#endif // RAPIDYYJSON_STRINGBUFFER_H_
