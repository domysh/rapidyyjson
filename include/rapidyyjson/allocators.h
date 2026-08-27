/*
 * rapidyyjson - a RapidJSON-compatible API implemented on top of yyjson.
 *
 * Allocator concept implementations, mirroring `rapidjson/allocators.h`.
 */

#ifndef RAPIDYYJSON_ALLOCATORS_H_
#define RAPIDYYJSON_ALLOCATORS_H_

#include "rapidyyjson.h"

RAPIDYYJSON_NAMESPACE_BEGIN

///////////////////////////////////////////////////////////////////////////////
// Allocator
//
/*! \class rapidyyjson::Allocator
    \brief Concept for allocating, resizing and freeing memory block.

    \code
    concept Allocator {
        static const bool kNeedFree;    //!< Whether this allocator needs to call Free().

        // Allocate a memory block.
        // \param size of the memory block in bytes.
        // \returns pointer to the memory block.
        void* Malloc(size_t size);

        // Resize a memory block.
        void* Realloc(void* originalPtr, size_t originalSize, size_t newSize);

        // Free a memory block.
        static void Free(void *ptr);
    };
    \endcode
*/

///////////////////////////////////////////////////////////////////////////////
// CrtAllocator

//! C-runtime library allocator.
class CrtAllocator
{
  public:
    static const bool kNeedFree = true;

    void* Malloc(size_t size)
    {
        if (size)
            return std::malloc(size);
        return NULL; // standardize to returning NULL.
    }

    void* Realloc(void* originalPtr, size_t originalSize, size_t newSize)
    {
        (void)originalSize;
        if (newSize == 0)
        {
            std::free(originalPtr);
            return NULL;
        }
        return std::realloc(originalPtr, newSize);
    }

    static void Free(void* ptr) RAPIDYYJSON_NOEXCEPT
    {
        std::free(ptr);
    }

    bool operator==(const CrtAllocator&) const RAPIDYYJSON_NOEXCEPT
    {
        return true;
    }

    bool operator!=(const CrtAllocator&) const RAPIDYYJSON_NOEXCEPT
    {
        return false;
    }
};

///////////////////////////////////////////////////////////////////////////////
// MemoryPoolAllocator

//! Default chunk capacity of MemoryPoolAllocator.
#ifndef RAPIDYYJSON_ALLOCATOR_DEFAULT_CHUNK_CAPACITY
#define RAPIDYYJSON_ALLOCATOR_DEFAULT_CHUNK_CAPACITY (64 * 1024)
#endif

/*! \brief Default memory allocator used by the parser and DOM.

    This allocator allocates memory blocks from pre-allocated memory chunks.
    It does not free memory blocks. And Realloc() only allocates new memory.
    The memory chunks are allocated by BaseAllocator, which is CrtAllocator by
    default. User may also supply a buffer as the first chunk.
    All memory is freed when the allocator is destructed or when Clear() is
    called.
*/
template <typename BaseAllocator = CrtAllocator>
class MemoryPoolAllocator
{
    //! Chunk header for perpending to each chunk.
    /*! Chunks are stored as a singly linked list. */
    struct ChunkHeader
    {
        size_t capacity; //!< Capacity of the chunk in bytes (excluding the header itself).
        size_t size;     //!< Current size of allocated memory in bytes.
        ChunkHeader* next; //!< Next chunk in the linked list.
    };

  public:
    static const bool kNeedFree = false; //!< Tell users that no need to call Free().

    //! Constructor with chunkSize.
    MemoryPoolAllocator(size_t chunkSize = RAPIDYYJSON_ALLOCATOR_DEFAULT_CHUNK_CAPACITY,
                        BaseAllocator* baseAllocator = 0)
        : chunkHead_(0),
          chunk_capacity_(chunkSize),
          userBuffer_(0),
          baseAllocator_(baseAllocator),
          ownBaseAllocator_(0)
    {
    }

    //! Constructor with user-supplied buffer.
    MemoryPoolAllocator(void* buffer,
                        size_t size,
                        size_t chunkSize = RAPIDYYJSON_ALLOCATOR_DEFAULT_CHUNK_CAPACITY,
                        BaseAllocator* baseAllocator = 0)
        : chunkHead_(0),
          chunk_capacity_(chunkSize),
          userBuffer_(buffer),
          baseAllocator_(baseAllocator),
          ownBaseAllocator_(0)
    {
        RAPIDYYJSON_ASSERT(buffer != 0);
        RAPIDYYJSON_ASSERT(size > sizeof(ChunkHeader));
        chunkHead_ = reinterpret_cast<ChunkHeader*>(buffer);
        chunkHead_->capacity = size - sizeof(ChunkHeader);
        chunkHead_->size = 0;
        chunkHead_->next = 0;
    }

    //! Destructor. This deallocates all memory chunks, excluding the user-supplied buffer.
    ~MemoryPoolAllocator()
    {
        Clear();
        RAPIDYYJSON_DELETE(ownBaseAllocator_);
    }

    //! Deallocates all memory chunks, excluding the user-supplied buffer.
    void Clear()
    {
        while (chunkHead_ && chunkHead_ != userBuffer_)
        {
            ChunkHeader* next = chunkHead_->next;
            baseAllocator_->Free(chunkHead_);
            chunkHead_ = next;
        }
        if (chunkHead_ && chunkHead_ == userBuffer_)
            chunkHead_->size = 0; // Clear user buffer
    }

    //! Computes the total capacity of allocated memory chunks.
    size_t Capacity() const
    {
        size_t capacity = 0;
        for (ChunkHeader* c = chunkHead_; c != 0; c = c->next)
            capacity += c->capacity;
        return capacity;
    }

    //! Computes the memory blocks allocated.
    size_t Size() const
    {
        size_t size = 0;
        for (ChunkHeader* c = chunkHead_; c != 0; c = c->next)
            size += c->size;
        return size;
    }

    //! Allocates a memory block. (concept Allocator)
    void* Malloc(size_t size)
    {
        if (!size)
            return NULL;

        size = RAPIDYYJSON_ALIGN(size);
        if (RAPIDYYJSON_UNLIKELY(chunkHead_ == 0 || chunkHead_->size + size > chunkHead_->capacity))
            if (!AddChunk(chunk_capacity_ > size ? chunk_capacity_ : size))
                return NULL;

        void* buffer = reinterpret_cast<char*>(chunkHead_) + sizeof(ChunkHeader) + chunkHead_->size;
        chunkHead_->size += size;
        return buffer;
    }

    //! Resizes a memory block (concept Allocator)
    void* Realloc(void* originalPtr, size_t originalSize, size_t newSize)
    {
        if (originalPtr == 0)
            return Malloc(newSize);

        if (newSize == 0)
            return NULL;

        originalSize = RAPIDYYJSON_ALIGN(originalSize);
        newSize = RAPIDYYJSON_ALIGN(newSize);

        // Do not shrink if new size is smaller than original
        if (originalSize >= newSize)
            return originalPtr;

        // Simply expand it if it is the last allocation and there is sufficient space
        if (originalPtr ==
            reinterpret_cast<char*>(chunkHead_) + sizeof(ChunkHeader) + chunkHead_->size -
                originalSize)
        {
            size_t increment = static_cast<size_t>(newSize - originalSize);
            if (chunkHead_->size + increment <= chunkHead_->capacity)
            {
                chunkHead_->size += increment;
                return originalPtr;
            }
        }

        // Realloc process: allocate and copy memory, do not free original buffer.
        if (void* newBuffer = Malloc(newSize))
        {
            if (originalSize)
                std::memcpy(newBuffer, originalPtr, originalSize);
            return newBuffer;
        }
        return NULL;
    }

    //! Frees a memory block (concept Allocator)
    static void Free(void* ptr) RAPIDYYJSON_NOEXCEPT
    {
        (void)ptr; // Do nothing
    }

    //! Compare (equality) with another MemoryPoolAllocator
    bool operator==(const MemoryPoolAllocator& rhs) const RAPIDYYJSON_NOEXCEPT
    {
        return chunkHead_ == rhs.chunkHead_;
    }

    //! Compare (inequality) with another MemoryPoolAllocator
    bool operator!=(const MemoryPoolAllocator& rhs) const RAPIDYYJSON_NOEXCEPT
    {
        return !operator==(rhs);
    }

  private:
    //! Copy constructor is not permitted.
    MemoryPoolAllocator(const MemoryPoolAllocator& rhs);
    //! Copy assignment operator is not permitted.
    MemoryPoolAllocator& operator=(const MemoryPoolAllocator& rhs);

    //! Creates a new chunk.
    /*! \param capacity Capacity of the chunk in bytes.
        \return true if success.
    */
    bool AddChunk(size_t capacity)
    {
        if (!baseAllocator_)
            ownBaseAllocator_ = baseAllocator_ = RAPIDYYJSON_NEW(BaseAllocator)();
        if (ChunkHeader* chunk = reinterpret_cast<ChunkHeader*>(
                baseAllocator_->Malloc(sizeof(ChunkHeader) + capacity)))
        {
            chunk->capacity = capacity;
            chunk->size = 0;
            chunk->next = chunkHead_;
            chunkHead_ = chunk;
            return true;
        }
        return false;
    }

    ChunkHeader* chunkHead_;  //!< Head of the chunk linked-list. Only the head chunk serves
                              //!< allocation.
    size_t chunk_capacity_;   //!< The minimum capacity of chunk when they are allocated.
    void* userBuffer_;        //!< User supplied buffer.
    BaseAllocator* baseAllocator_;    //!< base allocator for allocating memory chunks.
    BaseAllocator* ownBaseAllocator_; //!< base allocator created by this object.
};

RAPIDYYJSON_NAMESPACE_END

#endif // RAPIDYYJSON_ALLOCATORS_H_
