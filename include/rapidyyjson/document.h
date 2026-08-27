/*
 * rapidyyjson - a RapidJSON-compatible API implemented on top of yyjson.
 *
 * DOM API, mirroring `rapidjson/document.h`.
 */

#ifndef RAPIDYYJSON_DOCUMENT_H_
#define RAPIDYYJSON_DOCUMENT_H_

#include "allocators.h"
#include "encodings.h"
#include "internal/meta.h"
#include "internal/stack.h"
#include "internal/strfunc.h"
#include "internal/swap.h"
#include "rapidyyjson.h"
#include "encodedstream.h"
#include "memorystream.h"
#include "reader.h"
#include "stream.h"

#include <cstring>
#include <iterator>
#include <limits>
#include <new>
#include <utility>

#if RAPIDYYJSON_HAS_STDSTRING
#include <string>
#endif

///////////////////////////////////////////////////////////////////////////////
// Configuration

#ifndef RAPIDYYJSON_DEFAULT_ALLOCATOR
#define RAPIDYYJSON_DEFAULT_ALLOCATOR ::RAPIDYYJSON_NAMESPACE::MemoryPoolAllocator<::RAPIDYYJSON_NAMESPACE::CrtAllocator>
#endif

#ifndef RAPIDYYJSON_DEFAULT_STACK_ALLOCATOR
#define RAPIDYYJSON_DEFAULT_STACK_ALLOCATOR ::RAPIDYYJSON_NAMESPACE::CrtAllocator
#endif

#ifndef RAPIDYYJSON_VALUE_DEFAULT_OBJECT_CAPACITY
#define RAPIDYYJSON_VALUE_DEFAULT_OBJECT_CAPACITY 16
#endif

#ifndef RAPIDYYJSON_VALUE_DEFAULT_ARRAY_CAPACITY
#define RAPIDYYJSON_VALUE_DEFAULT_ARRAY_CAPACITY 16
#endif

RAPIDYYJSON_NAMESPACE_BEGIN

///////////////////////////////////////////////////////////////////////////////
// Forward declarations

template <typename Encoding, typename Allocator>
class GenericValue;

template <typename Encoding, typename Allocator, typename StackAllocator>
class GenericDocument;

template <bool Const, typename ValueT>
class GenericArray;

template <bool Const, typename ValueT>
class GenericObject;

//! Name-value pair in a JSON object value.
template <typename Encoding, typename Allocator>
class GenericMember
{
  public:
    GenericValue<Encoding, Allocator> name;  //!< name of member (must be a string)
    GenericValue<Encoding, Allocator> value; //!< value of member.

    //! Move constructor in C++11
    GenericMember(GenericMember&& rhs) RAPIDYYJSON_NOEXCEPT : name(std::move(rhs.name)),
                                                              value(std::move(rhs.value))
    {
    }

    //! Move assignment in C++11
    GenericMember& operator=(GenericMember&& rhs) RAPIDYYJSON_NOEXCEPT
    {
        return *this = static_cast<GenericMember&>(rhs);
    }

    //! Assignment with move semantics.
    /*! \param rhs Source of the assignment. Its name and value will become a null value after
                  assignment.
    */
    GenericMember& operator=(GenericMember& rhs) RAPIDYYJSON_NOEXCEPT
    {
        if (this != &rhs)
        {
            name = rhs.name;
            value = rhs.value;
        }
        return *this;
    }

    // swap() for std::sort() and other potential use in STL.
    friend inline void swap(GenericMember& a, GenericMember& b) RAPIDYYJSON_NOEXCEPT
    {
        a.name.Swap(b.name);
        a.value.Swap(b.value);
    }

  private:
    //! Copy constructor is not permitted.
    GenericMember(const GenericMember& rhs);

    template <typename, typename>
    friend class GenericValue;
    template <typename, typename, typename>
    friend class GenericDocument;
};

///////////////////////////////////////////////////////////////////////////////
// GenericMemberIterator

//! (Constant) member iterator for a JSON object value
/*!
    \tparam Const Is this a constant iterator?
    \tparam Encoding    Encoding of the value. (Even non-string values need to have the same
                        encoding in a document)
    \tparam Allocator   Allocator type for allocating memory of object, array and string.

    This class implements a Random Access Iterator for GenericMember elements
    of a GenericValue, see ISO/IEC 14882:2003(E) C++ standard, 24.1 [lib.iterator.requirements].
 */
template <bool Const, typename Encoding, typename Allocator>
class GenericMemberIterator
{
    friend class GenericValue<Encoding, Allocator>;
    template <bool, typename, typename>
    friend class GenericMemberIterator;

    typedef GenericMember<Encoding, Allocator> PlainType;
    typedef typename internal::MaybeAddConst<Const, PlainType>::Type ValueType;

  public:
    //! Iterator type itself
    typedef GenericMemberIterator Iterator;
    //! Constant iterator type
    typedef GenericMemberIterator<true, Encoding, Allocator> ConstIterator;
    //! Non-constant iterator type
    typedef GenericMemberIterator<false, Encoding, Allocator> NonConstIterator;

    /** \name std::iterator_traits support */
    //@{
    typedef ValueType value_type;
    typedef ValueType* pointer;
    typedef ValueType& reference;
    typedef std::ptrdiff_t difference_type;
    typedef std::random_access_iterator_tag iterator_category;
    //@}

    //! Pointer to (const) GenericMember
    typedef pointer Pointer;
    //! Reference to (const) GenericMember
    typedef reference Reference;
    //! Signed integer type (e.g. \c ptrdiff_t)
    typedef difference_type DifferenceType;

    //! Default constructor (singular value)
    /*! Creates an iterator pointing to no element.
        \note All operations, except for comparisons, are undefined on such values.
     */
    GenericMemberIterator()
        : ptr_()
    {
    }

    //! Iterator conversions to more const
    /*!
        \param it (Non-const) iterator to copy from

        Allows the creation of an iterator from another GenericMemberIterator
        that is "less const".  Especially, creating a non-constant iterator
        from a constant iterator is not permitted.
     */
    GenericMemberIterator(const NonConstIterator& it)
        : ptr_(it.ptr_)
    {
    }

    Iterator& operator=(const NonConstIterator& it)
    {
        ptr_ = it.ptr_;
        return *this;
    }

    //! @name stepping
    //@{
    Iterator& operator++()
    {
        ++ptr_;
        return *this;
    }

    Iterator& operator--()
    {
        --ptr_;
        return *this;
    }

    Iterator operator++(int)
    {
        Iterator old(*this);
        ++ptr_;
        return old;
    }

    Iterator operator--(int)
    {
        Iterator old(*this);
        --ptr_;
        return old;
    }
    //@}

    //! @name increment/decrement
    //@{
    Iterator operator+(DifferenceType n) const
    {
        return Iterator(ptr_ + n);
    }

    Iterator operator-(DifferenceType n) const
    {
        return Iterator(ptr_ - n);
    }

    Iterator& operator+=(DifferenceType n)
    {
        ptr_ += n;
        return *this;
    }

    Iterator& operator-=(DifferenceType n)
    {
        ptr_ -= n;
        return *this;
    }
    //@}

    //! @name relations
    //@{
    template <bool Const_>
    bool operator==(const GenericMemberIterator<Const_, Encoding, Allocator>& that) const
    {
        return ptr_ == that.ptr_;
    }

    template <bool Const_>
    bool operator!=(const GenericMemberIterator<Const_, Encoding, Allocator>& that) const
    {
        return ptr_ != that.ptr_;
    }

    template <bool Const_>
    bool operator<=(const GenericMemberIterator<Const_, Encoding, Allocator>& that) const
    {
        return ptr_ <= that.ptr_;
    }

    template <bool Const_>
    bool operator>=(const GenericMemberIterator<Const_, Encoding, Allocator>& that) const
    {
        return ptr_ >= that.ptr_;
    }

    template <bool Const_>
    bool operator<(const GenericMemberIterator<Const_, Encoding, Allocator>& that) const
    {
        return ptr_ < that.ptr_;
    }

    template <bool Const_>
    bool operator>(const GenericMemberIterator<Const_, Encoding, Allocator>& that) const
    {
        return ptr_ > that.ptr_;
    }
    //@}

    //! @name dereference
    //@{
    Reference operator*() const
    {
        return *ptr_;
    }

    Pointer operator->() const
    {
        return ptr_;
    }

    Reference operator[](DifferenceType n) const
    {
        return ptr_[n];
    }
    //@}

    //! Distance
    DifferenceType operator-(ConstIterator that) const
    {
        return ptr_ - that.ptr_;
    }

  private:
    //! Internal constructor from plain pointer
    explicit GenericMemberIterator(Pointer p)
        : ptr_(p)
    {
    }

    Pointer ptr_; //!< raw pointer
};

///////////////////////////////////////////////////////////////////////////////
// GenericStringRef

//! Reference to a constant string (not taking a copy)
/*!
    \tparam CharType character type of the string

    This helper class is used to automatically infer constant string
    references for string literals, especially from \c const \b (!)
    character arrays.

    \see StringRef, GenericValue::SetString
 */
template <typename CharType>
struct GenericStringRef
{
    typedef CharType Ch; //!< character type of the string

    //! Create string reference from \c const character array
    template <SizeType N>
    GenericStringRef(const CharType (&str)[N]) RAPIDYYJSON_NOEXCEPT : s(str),
                                                                      length(N - 1)
    {
    }

    //! Explicitly create string reference from \c const character pointer
    explicit GenericStringRef(const CharType* str)
        : s(str),
          length(NotNullStrLen(str))
    {
    }

    //! Create constant string reference from pointer and length
    GenericStringRef(const CharType* str, SizeType len)
        : s(RAPIDYYJSON_LIKELY(str != 0) ? str : emptyString),
          length(len)
    {
        RAPIDYYJSON_ASSERT(str != 0 || len == 0u);
    }

    GenericStringRef(const GenericStringRef& rhs)
        : s(rhs.s),
          length(rhs.length)
    {
    }

    //! implicit conversion to plain CharType pointer
    operator const Ch*() const
    {
        return s;
    }

    const Ch* const s; //!< plain CharType pointer
    const SizeType length; //!< length of the string (excluding the trailing NULL terminator)

  private:
    static SizeType NotNullStrLen(const CharType* str)
    {
        RAPIDYYJSON_ASSERT(str != 0);
        return internal::StrLen(str);
    }

    /// Empty string - used when passing in a NULL pointer
    static const Ch emptyString[];

    //! Disallow construction from non-const array
    template <SizeType N>
    GenericStringRef(CharType (&str)[N]) /* = delete */;
    //! Copy assignment operator not permitted - immutable type
    GenericStringRef& operator=(const GenericStringRef& rhs) /* = delete */;
};

template <typename CharType>
const CharType GenericStringRef<CharType>::emptyString[] = {CharType()};

//! Mark a character pointer as constant string
/*! Mark a plain character pointer as a "string literal".  This function
    can be used to avoid copying a character string to be referenced as a
    value in a JSON GenericValue object, if the string's lifetime is known
    to be valid long enough.
*/
template <typename CharType>
inline GenericStringRef<CharType>
StringRef(const CharType* str)
{
    return GenericStringRef<CharType>(str);
}

//! Mark a character pointer as constant string
template <typename CharType>
inline GenericStringRef<CharType>
StringRef(const CharType* str, size_t length)
{
    return GenericStringRef<CharType>(str, SizeType(length));
}

#if RAPIDYYJSON_HAS_STDSTRING
//! Mark a string object as constant string
template <typename CharType>
inline GenericStringRef<CharType>
StringRef(const std::basic_string<CharType>& str)
{
    return GenericStringRef<CharType>(str.data(), SizeType(str.size()));
}
#endif

///////////////////////////////////////////////////////////////////////////////
// internal helpers

namespace internal
{

template <typename T, typename Encoding = void, typename Allocator = void>
struct IsGenericValueImpl : FalseType
{
};

// select candidates according to nested encoding and allocator types
template <typename T>
struct IsGenericValueImpl<T,
                          typename Void<typename T::EncodingType>::Type,
                          typename Void<typename T::AllocatorType>::Type>
    : IsBaseOf<GenericValue<typename T::EncodingType, typename T::AllocatorType>, T>::Type
{
};

// helper to match arbitrary GenericValue instantiations, including derived classes
template <typename T>
struct IsGenericValue : IsGenericValueImpl<T>::Type
{
};

///////////////////////////////////////////////////////////////////////////////
// TypeHelper

template <typename ValueType, typename T>
struct TypeHelper
{
};

template <typename ValueType>
struct TypeHelper<ValueType, bool>
{
    static bool Is(const ValueType& v)
    {
        return v.IsBool();
    }

    static bool Get(const ValueType& v)
    {
        return v.GetBool();
    }

    static ValueType& Set(ValueType& v, bool data)
    {
        return v.SetBool(data);
    }

    static ValueType& Set(ValueType& v, bool data, typename ValueType::AllocatorType&)
    {
        return v.SetBool(data);
    }
};

template <typename ValueType>
struct TypeHelper<ValueType, int>
{
    static bool Is(const ValueType& v)
    {
        return v.IsInt();
    }

    static int Get(const ValueType& v)
    {
        return v.GetInt();
    }

    static ValueType& Set(ValueType& v, int data)
    {
        return v.SetInt(data);
    }

    static ValueType& Set(ValueType& v, int data, typename ValueType::AllocatorType&)
    {
        return v.SetInt(data);
    }
};

template <typename ValueType>
struct TypeHelper<ValueType, unsigned>
{
    static bool Is(const ValueType& v)
    {
        return v.IsUint();
    }

    static unsigned Get(const ValueType& v)
    {
        return v.GetUint();
    }

    static ValueType& Set(ValueType& v, unsigned data)
    {
        return v.SetUint(data);
    }

    static ValueType& Set(ValueType& v, unsigned data, typename ValueType::AllocatorType&)
    {
        return v.SetUint(data);
    }
};

template <typename ValueType>
struct TypeHelper<ValueType, int64_t>
{
    static bool Is(const ValueType& v)
    {
        return v.IsInt64();
    }

    static int64_t Get(const ValueType& v)
    {
        return v.GetInt64();
    }

    static ValueType& Set(ValueType& v, int64_t data)
    {
        return v.SetInt64(data);
    }

    static ValueType& Set(ValueType& v, int64_t data, typename ValueType::AllocatorType&)
    {
        return v.SetInt64(data);
    }
};

template <typename ValueType>
struct TypeHelper<ValueType, uint64_t>
{
    static bool Is(const ValueType& v)
    {
        return v.IsUint64();
    }

    static uint64_t Get(const ValueType& v)
    {
        return v.GetUint64();
    }

    static ValueType& Set(ValueType& v, uint64_t data)
    {
        return v.SetUint64(data);
    }

    static ValueType& Set(ValueType& v, uint64_t data, typename ValueType::AllocatorType&)
    {
        return v.SetUint64(data);
    }
};

template <typename ValueType>
struct TypeHelper<ValueType, double>
{
    static bool Is(const ValueType& v)
    {
        return v.IsDouble();
    }

    static double Get(const ValueType& v)
    {
        return v.GetDouble();
    }

    static ValueType& Set(ValueType& v, double data)
    {
        return v.SetDouble(data);
    }

    static ValueType& Set(ValueType& v, double data, typename ValueType::AllocatorType&)
    {
        return v.SetDouble(data);
    }
};

template <typename ValueType>
struct TypeHelper<ValueType, float>
{
    static bool Is(const ValueType& v)
    {
        return v.IsFloat();
    }

    static float Get(const ValueType& v)
    {
        return v.GetFloat();
    }

    static ValueType& Set(ValueType& v, float data)
    {
        return v.SetFloat(data);
    }

    static ValueType& Set(ValueType& v, float data, typename ValueType::AllocatorType&)
    {
        return v.SetFloat(data);
    }
};

template <typename ValueType>
struct TypeHelper<ValueType, const typename ValueType::Ch*>
{
    typedef const typename ValueType::Ch* StringType;

    static bool Is(const ValueType& v)
    {
        return v.IsString();
    }

    static StringType Get(const ValueType& v)
    {
        return v.GetString();
    }

    static ValueType& Set(ValueType& v, const StringType data)
    {
        return v.SetString(typename ValueType::StringRefType(data));
    }

    static ValueType& Set(ValueType& v, const StringType data, typename ValueType::AllocatorType& a)
    {
        return v.SetString(data, a);
    }
};

#if RAPIDYYJSON_HAS_STDSTRING
template <typename ValueType>
struct TypeHelper<ValueType, std::basic_string<typename ValueType::Ch>>
{
    typedef std::basic_string<typename ValueType::Ch> StringType;

    static bool Is(const ValueType& v)
    {
        return v.IsString();
    }

    static StringType Get(const ValueType& v)
    {
        return StringType(v.GetString(), v.GetStringLength());
    }

    static ValueType& Set(ValueType& v, const StringType& data, typename ValueType::AllocatorType& a)
    {
        return v.SetString(data, a);
    }
};
#endif

template <typename ValueType>
struct TypeHelper<ValueType, typename ValueType::Array>
{
    typedef typename ValueType::Array ArrayType;

    static bool Is(const ValueType& v)
    {
        return v.IsArray();
    }

    static ArrayType Get(ValueType& v)
    {
        return v.GetArray();
    }

    static ValueType& Set(ValueType& v, ArrayType data)
    {
        return v = data;
    }

    static ValueType& Set(ValueType& v, ArrayType data, typename ValueType::AllocatorType&)
    {
        return v = data;
    }
};

template <typename ValueType>
struct TypeHelper<ValueType, typename ValueType::ConstArray>
{
    typedef typename ValueType::ConstArray ArrayType;

    static bool Is(const ValueType& v)
    {
        return v.IsArray();
    }

    static ArrayType Get(const ValueType& v)
    {
        return v.GetArray();
    }
};

template <typename ValueType>
struct TypeHelper<ValueType, typename ValueType::Object>
{
    typedef typename ValueType::Object ObjectType;

    static bool Is(const ValueType& v)
    {
        return v.IsObject();
    }

    static ObjectType Get(ValueType& v)
    {
        return v.GetObject();
    }

    static ValueType& Set(ValueType& v, ObjectType data)
    {
        return v = data;
    }

    static ValueType& Set(ValueType& v, ObjectType data, typename ValueType::AllocatorType&)
    {
        return v = data;
    }
};

template <typename ValueType>
struct TypeHelper<ValueType, typename ValueType::ConstObject>
{
    typedef typename ValueType::ConstObject ObjectType;

    static bool Is(const ValueType& v)
    {
        return v.IsObject();
    }

    static ObjectType Get(const ValueType& v)
    {
        return v.GetObject();
    }
};

} // namespace internal

///////////////////////////////////////////////////////////////////////////////
// GenericValue

//! Represents a JSON value. Use Value for UTF8 encoding and default allocator.
/*!
    A JSON value can be one of 7 types. This class is a variant type supporting
    these types.

    Use the Value type alias for the common case.

    \tparam Encoding    Encoding of the value. (Even non-string values need to have the same
                        encoding in a document)
    \tparam Allocator   Allocator type for allocating memory of object, array and string.
*/
template <typename Encoding, typename Allocator = RAPIDYYJSON_DEFAULT_ALLOCATOR>
class GenericValue
{
  public:
    //! Name-value pair in an object.
    typedef GenericMember<Encoding, Allocator> Member;
    //! Encoding type from template parameter.
    typedef Encoding EncodingType;
    //! Allocator type from template parameter.
    typedef Allocator AllocatorType;
    //! Character type derived from Encoding.
    typedef typename Encoding::Ch Ch;
    //! Reference to a constant string
    typedef GenericStringRef<Ch> StringRefType;
    //! Member iterator for iterating in object.
    typedef typename GenericMemberIterator<false, Encoding, Allocator>::Iterator MemberIterator;
    //! Constant member iterator for iterating in object.
    typedef typename GenericMemberIterator<true, Encoding, Allocator>::Iterator ConstMemberIterator;
    //! Value iterator for iterating in array.
    typedef GenericValue* ValueIterator;
    //! Constant value iterator for iterating in array.
    typedef const GenericValue* ConstValueIterator;
    //! Value type of itself.
    typedef GenericValue<Encoding, Allocator> ValueType;
    //! Helper class for working with an array.
    typedef GenericArray<false, ValueType> Array;
    //! Helper class for working with a constant array.
    typedef GenericArray<true, ValueType> ConstArray;
    //! Helper class for working with an object.
    typedef GenericObject<false, ValueType> Object;
    //! Helper class for working with a constant object.
    typedef GenericObject<true, ValueType> ConstObject;

    //!@name Constructors and destructor.
    //@{

    //! Default constructor creates a null value.
    GenericValue() RAPIDYYJSON_NOEXCEPT : data_(), flags_(kNullFlag)
    {
    }

    //! Move constructor in C++11
    GenericValue(GenericValue&& rhs) RAPIDYYJSON_NOEXCEPT : data_(rhs.data_),
                                                            flags_(rhs.flags_)
    {
        rhs.flags_ = kNullFlag; // give up contents
    }

  private:
    //! Copy constructor is not permitted.
    GenericValue(const GenericValue& rhs);

  public:
    //! Constructor with JSON value type.
    /*! This creates a Value of specified type with default content.
        \param type Type of the value.
        \note Default content for number is zero.
    */
    explicit GenericValue(Type type) RAPIDYYJSON_NOEXCEPT : data_(), flags_()
    {
        static const uint16_t defaultFlags[] = {kNullFlag,
                                                kFalseFlag,
                                                kTrueFlag,
                                                kObjectFlag,
                                                kArrayFlag,
                                                kShortStringFlag,
                                                kNumberAnyFlag};
        RAPIDYYJSON_ASSERT(type >= kNullType && type <= kNumberType);
        flags_ = defaultFlags[type];

        // An empty string value points at a shared, statically allocated terminator, so that a
        // default-constructed string Value is usable without an allocator.
        if (type == kStringType)
        {
            data_.s.str = emptyString_;
            data_.s.length = 0;
            flags_ = kConstStringFlag;
        }
        else if (type == kObjectType)
        {
            data_.o.size = data_.o.capacity = 0;
            data_.o.members = 0;
        }
        else if (type == kArrayType)
        {
            data_.a.size = data_.a.capacity = 0;
            data_.a.elements = 0;
        }
        else
            data_.n.u64 = 0;
    }

    //! Explicit copy constructor (with allocator)
    /*! Creates a copy of a Value by using the given Allocator
        \tparam SourceAllocator allocator of \c rhs
        \param rhs Value to copy from (read-only)
        \param allocator Allocator for allocating copied elements and buffers. Commonly use
                         GenericDocument::GetAllocator().
        \param copyConstStrings Force copying of constant strings (e.g. referencing an in-situ
                                buffer)
        \see CopyFrom()
    */
    template <typename SourceAllocator>
    GenericValue(const GenericValue<Encoding, SourceAllocator>& rhs,
                 Allocator& allocator,
                 bool copyConstStrings = false)
        : data_(),
          flags_()
    {
        switch (rhs.GetType())
        {
        case kObjectType: {
            SizeType count = rhs.MemberCount();
            Member* lm = 0;
            if (count)
            {
                lm = static_cast<Member*>(allocator.Malloc(count * sizeof(Member)));
                const typename GenericValue<Encoding, SourceAllocator>::Member* rm =
                    rhs.data_.o.members;
                for (SizeType i = 0; i < count; i++)
                {
                    new (&lm[i].name) GenericValue(rm[i].name, allocator, copyConstStrings);
                    new (&lm[i].value) GenericValue(rm[i].value, allocator, copyConstStrings);
                }
            }
            data_.o.members = lm;
            data_.o.size = data_.o.capacity = count;
            flags_ = kObjectFlag;
        }
        break;
        case kArrayType: {
            SizeType count = rhs.Size();
            GenericValue* le = 0;
            if (count)
            {
                le = static_cast<GenericValue*>(allocator.Malloc(count * sizeof(GenericValue)));
                const GenericValue<Encoding, SourceAllocator>* re = rhs.data_.a.elements;
                for (SizeType i = 0; i < count; i++)
                    new (&le[i]) GenericValue(re[i], allocator, copyConstStrings);
            }
            data_.a.elements = le;
            data_.a.size = data_.a.capacity = count;
            flags_ = kArrayFlag;
        }
        break;
        case kStringType:
            if (rhs.IsCopyString() || copyConstStrings)
                SetStringRaw(StringRef(rhs.GetString(), rhs.GetStringLength()), allocator);
            else
                SetStringRaw(StringRef(rhs.GetString(), rhs.GetStringLength()));
            break;
        default:
            data_.n.u64 = rhs.data_.n.u64;
            flags_ = rhs.flags_;
            break;
        }
    }

    //! Constructor for boolean value.
    /*! \param b Boolean value
        \note This constructor is limited to \em real boolean values and rejects
            implicitly converted pointers.
     */
    template <typename T>
    explicit GenericValue(T b, RAPIDYYJSON_ENABLEIF((internal::IsSame<bool, T>)))
        RAPIDYYJSON_NOEXCEPT : data_(),
                               flags_(b ? kTrueFlag : kFalseFlag)
    {
        // safe-guard against failing SFINAE
        RAPIDYYJSON_STATIC_ASSERT((internal::IsSame<bool, T>::Value));
        data_.n.u64 = 0;
    }

    //! Constructor for int value.
    explicit GenericValue(int i) RAPIDYYJSON_NOEXCEPT : data_(), flags_(kNumberIntFlag)
    {
        data_.n.i64 = i;
        if (i >= 0)
            flags_ |= kUintFlag | kUint64Flag;
    }

    //! Constructor for unsigned value.
    explicit GenericValue(unsigned u) RAPIDYYJSON_NOEXCEPT : data_(), flags_(kNumberUintFlag)
    {
        data_.n.u64 = u;
        if (!(u & 0x80000000))
            flags_ |= kIntFlag;
    }

    //! Constructor for int64_t value.
    explicit GenericValue(int64_t i64) RAPIDYYJSON_NOEXCEPT : data_(), flags_(kNumberInt64Flag)
    {
        data_.n.i64 = i64;
        if (i64 >= 0)
        {
            flags_ |= kNumberUint64Flag;
            if (!(static_cast<uint64_t>(i64) & RAPIDYYJSON_UINT64_C2(0xFFFFFFFF, 0x00000000)))
                flags_ |= kUintFlag;
            if (!(static_cast<uint64_t>(i64) & RAPIDYYJSON_UINT64_C2(0xFFFFFFFF, 0x80000000)))
                flags_ |= kIntFlag;
        }
        else if (i64 >= static_cast<int64_t>(RAPIDYYJSON_UINT64_C2(0xFFFFFFFF, 0x80000000)))
            flags_ |= kIntFlag;
    }

    //! Constructor for uint64_t value.
    explicit GenericValue(uint64_t u64) RAPIDYYJSON_NOEXCEPT : data_(), flags_(kNumberUint64Flag)
    {
        data_.n.u64 = u64;
        if (!(u64 & RAPIDYYJSON_UINT64_C2(0x80000000, 0x00000000)))
            flags_ |= kInt64Flag;
        if (!(u64 & RAPIDYYJSON_UINT64_C2(0xFFFFFFFF, 0x00000000)))
            flags_ |= kUintFlag;
        if (!(u64 & RAPIDYYJSON_UINT64_C2(0xFFFFFFFF, 0x80000000)))
            flags_ |= kIntFlag;
    }

    //! Constructor for double value.
    explicit GenericValue(double d) RAPIDYYJSON_NOEXCEPT : data_(), flags_(kNumberDoubleFlag)
    {
        data_.n.d = d;
    }

    //! Constructor for float value.
    explicit GenericValue(float f) RAPIDYYJSON_NOEXCEPT : data_(), flags_(kNumberDoubleFlag)
    {
        data_.n.d = static_cast<double>(f);
    }

    //! Constructor for constant string (i.e. do not make a copy of string)
    GenericValue(const Ch* s, SizeType length) RAPIDYYJSON_NOEXCEPT : data_(), flags_()
    {
        SetStringRaw(StringRef(s, length));
    }

    //! Constructor for constant string (i.e. do not make a copy of string)
    explicit GenericValue(StringRefType s) RAPIDYYJSON_NOEXCEPT : data_(), flags_()
    {
        SetStringRaw(s);
    }

    //! Constructor for copy-string (i.e. do make a copy of string)
    GenericValue(const Ch* s, SizeType length, Allocator& allocator)
        : data_(),
          flags_()
    {
        SetStringRaw(StringRef(s, length), allocator);
    }

    //! Constructor for copy-string (i.e. do make a copy of string)
    GenericValue(const Ch* s, Allocator& allocator)
        : data_(),
          flags_()
    {
        SetStringRaw(StringRef(s), allocator);
    }

#if RAPIDYYJSON_HAS_STDSTRING
    //! Constructor for copy-string from a string object (i.e. do make a copy of string)
    GenericValue(const std::basic_string<Ch>& s, Allocator& allocator)
        : data_(),
          flags_()
    {
        SetStringRaw(StringRef(s), allocator);
    }
#endif

    //! Constructor for Array.
    GenericValue(Array a) RAPIDYYJSON_NOEXCEPT : data_(a.value_.data_), flags_(kArrayFlag)
    {
        a.value_.data_ = Data();
        a.value_.flags_ = kArrayFlag;
        a.value_.data_.a.size = a.value_.data_.a.capacity = 0;
        a.value_.data_.a.elements = 0;
    }

    //! Constructor for Object.
    GenericValue(Object o) RAPIDYYJSON_NOEXCEPT : data_(o.value_.data_), flags_(kObjectFlag)
    {
        o.value_.data_ = Data();
        o.value_.flags_ = kObjectFlag;
        o.value_.data_.o.size = o.value_.data_.o.capacity = 0;
        o.value_.data_.o.members = 0;
    }

    //! Destructor.
    /*! Need to destruct elements of array, members of object, or copy-string.
    */
    ~GenericValue()
    {
        // With RAPIDYYJSON_DEFAULT_ALLOCATOR (MemoryPoolAllocator) the whole memory is released
        // when the allocator dies, so nothing has to be done here.
        if (Allocator::kNeedFree)
        {
            switch (flags_)
            {
            case kArrayFlag: {
                GenericValue* e = data_.a.elements;
                for (GenericValue* v = e; v != e + data_.a.size; ++v)
                    v->~GenericValue();
                Allocator::Free(e);
            }
            break;

            case kObjectFlag:
                for (MemberIterator m = MemberBegin(); m != MemberEnd(); ++m)
                    m->~Member();
                Allocator::Free(data_.o.members);
                break;

            case kCopyStringFlag:
                Allocator::Free(const_cast<Ch*>(data_.s.str));
                break;

            default:
                break; // Do nothing for other types.
            }
        }
    }

    //@}

    //!@name Assignment operators
    //@{

    //! Assignment with move semantics.
    /*! \param rhs Source of the assignment. It will become a null value after assignment.
    */
    GenericValue& operator=(GenericValue& rhs) RAPIDYYJSON_NOEXCEPT
    {
        if (RAPIDYYJSON_LIKELY(this != &rhs))
        {
            this->~GenericValue();
            RawAssign(rhs);
        }
        return *this;
    }

    //! Move assignment in C++11
    GenericValue& operator=(GenericValue&& rhs) RAPIDYYJSON_NOEXCEPT
    {
        return *this = rhs.Move();
    }

    //! Assignment of constant string reference (no copy)
    GenericValue& operator=(StringRefType str) RAPIDYYJSON_NOEXCEPT
    {
        GenericValue s(str);
        return *this = s;
    }

    //! Assignment with primitive types.
    /*! \tparam T Either \ref Type, \c int, \c unsigned, \c int64_t, \c uint64_t
        \param value The value to be assigned.
        \note The source type \c T explicitly disallows all pointer types,
            especially (\c const) \ref Ch*.  This helps avoiding implicitly
            referencing character strings with insufficient lifetime.
    */
    template <typename T>
    RAPIDYYJSON_DISABLEIF_RETURN((internal::IsPointer<T>), (GenericValue&))
    operator=(T value)
    {
        GenericValue v(value);
        return *this = v;
    }

    //! Deep-copy assignment from Value
    /*! Assigns a \b copy of the Value to the current Value object
        \tparam SourceAllocator Allocator type of \c rhs
        \param rhs Value to copy from (read-only)
        \param allocator Allocator to use for copying
        \param copyConstStrings Force copying of constant strings (e.g. referencing an in-situ
                                buffer)
     */
    template <typename SourceAllocator>
    GenericValue& CopyFrom(const GenericValue<Encoding, SourceAllocator>& rhs,
                           Allocator& allocator,
                           bool copyConstStrings = false)
    {
        RAPIDYYJSON_ASSERT(static_cast<void*>(this) !=
                           static_cast<void const*>(&rhs));
        this->~GenericValue();
        new (this) GenericValue(rhs, allocator, copyConstStrings);
        return *this;
    }

    //! Exchange the contents of this value with those of other.
    GenericValue& Swap(GenericValue& other) RAPIDYYJSON_NOEXCEPT
    {
        GenericValue temp;
        temp.RawAssign(*this);
        RawAssign(other);
        other.RawAssign(temp);
        return *this;
    }

    //! free-standing swap function helper
    friend inline void swap(GenericValue& a, GenericValue& b) RAPIDYYJSON_NOEXCEPT
    {
        a.Swap(b);
    }

    //! Prepare Value for move semantics
    /*! \return *this */
    GenericValue& Move() RAPIDYYJSON_NOEXCEPT
    {
        return *this;
    }
    //@}

    //!@name Equal-to and not-equal-to operators
    //@{
    //! Equal-to operator
    /*!
        \note If an object contains duplicated named member, comparing equality with any object is
              always \c false.
        \note Complexity is quadratic in Object's member number and linear for the rest (number of
              all values in the subtree and total lengths of all strings).
    */
    template <typename SourceAllocator>
    bool operator==(const GenericValue<Encoding, SourceAllocator>& rhs) const
    {
        typedef GenericValue<Encoding, SourceAllocator> RhsType;
        if (GetType() != rhs.GetType())
            return false;

        switch (GetType())
        {
        case kObjectType: // Warning: O(n^2) inner-loop
            if (MemberCount() != rhs.MemberCount())
                return false;
            for (ConstMemberIterator lhsMemberItr = MemberBegin(); lhsMemberItr != MemberEnd();
                 ++lhsMemberItr)
            {
                typename RhsType::ConstMemberIterator rhsMemberItr =
                    rhs.FindMember(lhsMemberItr->name);
                if (rhsMemberItr == rhs.MemberEnd() || lhsMemberItr->value != rhsMemberItr->value)
                    return false;
            }
            return true;

        case kArrayType:
            if (Size() != rhs.Size())
                return false;
            for (SizeType i = 0; i < Size(); i++)
                if ((*this)[i] != rhs[i])
                    return false;
            return true;

        case kStringType:
            return StringEqual(rhs);

        case kNumberType:
            if (IsDouble() || rhs.IsDouble())
            {
                double a = GetDouble();  // May convert from integer to double.
                double b = rhs.GetDouble(); // Ditto
                return a >= b && a <= b;    // Prevent -Wfloat-equal
            }
            else
                return data_.n.u64 == rhs.data_.n.u64;

        default:
            return true;
        }
    }

    //! Equal-to operator with const C-string pointer
    bool operator==(const Ch* rhs) const
    {
        return *this == GenericValue(StringRef(rhs));
    }

#if RAPIDYYJSON_HAS_STDSTRING
    //! Equal-to operator with string object
    bool operator==(const std::basic_string<Ch>& rhs) const
    {
        return *this == GenericValue(StringRef(rhs));
    }
#endif

    //! Equal-to operator with primitive types
    /*! \tparam T Either \c bool, \c int, \c unsigned, \c int64_t, \c uint64_t, \c double,
                  \c true, \c false
    */
    template <typename T>
    RAPIDYYJSON_DISABLEIF_RETURN(
        (internal::OrExpr<internal::IsPointer<T>, internal::IsGenericValue<T>>),
        (bool))
    operator==(const T& rhs) const
    {
        return *this == GenericValue(rhs);
    }

    //! Not-equal-to operator
    template <typename SourceAllocator>
    bool operator!=(const GenericValue<Encoding, SourceAllocator>& rhs) const
    {
        return !(*this == rhs);
    }

    //! Not-equal-to operator with const C-string pointer
    bool operator!=(const Ch* rhs) const
    {
        return !(*this == rhs);
    }

    //! Not-equal-to operator with arbitrary types
    template <typename T>
    RAPIDYYJSON_DISABLEIF_RETURN((internal::IsGenericValue<T>), (bool))
    operator!=(const T& rhs) const
    {
        return !(*this == rhs);
    }

    //! Equal-to operator with arbitrary types (symmetric version)
    template <typename T>
    friend RAPIDYYJSON_DISABLEIF_RETURN((internal::IsGenericValue<T>), (bool))
    operator==(const T& lhs, const GenericValue& rhs)
    {
        return rhs == lhs;
    }

    //! Not-Equal-to operator with arbitrary types (symmetric version)
    template <typename T>
    friend RAPIDYYJSON_DISABLEIF_RETURN((internal::IsGenericValue<T>), (bool))
    operator!=(const T& lhs, const GenericValue& rhs)
    {
        return !(rhs == lhs);
    }
    //@}

    //!@name Type
    //@{

    Type GetType() const
    {
        return static_cast<Type>(flags_ & kTypeMask);
    }

    bool IsNull() const
    {
        return flags_ == kNullFlag;
    }

    bool IsFalse() const
    {
        return flags_ == kFalseFlag;
    }

    bool IsTrue() const
    {
        return flags_ == kTrueFlag;
    }

    bool IsBool() const
    {
        return (flags_ & kBoolFlag) != 0;
    }

    bool IsObject() const
    {
        return flags_ == kObjectFlag;
    }

    bool IsArray() const
    {
        return flags_ == kArrayFlag;
    }

    bool IsNumber() const
    {
        return (flags_ & kNumberFlag) != 0;
    }

    bool IsInt() const
    {
        return (flags_ & kIntFlag) != 0;
    }

    bool IsUint() const
    {
        return (flags_ & kUintFlag) != 0;
    }

    bool IsInt64() const
    {
        return (flags_ & kInt64Flag) != 0;
    }

    bool IsUint64() const
    {
        return (flags_ & kUint64Flag) != 0;
    }

    bool IsDouble() const
    {
        return (flags_ & kDoubleFlag) != 0;
    }

    bool IsString() const
    {
        return (flags_ & kStringFlag) != 0;
    }

    //! Whether a number can be losslessly converted to a double.
    bool IsLosslessDouble() const
    {
        if (!IsNumber())
            return false;
        if (IsUint64())
        {
            uint64_t u = GetUint64();
            volatile double d = static_cast<double>(u);
            return (d >= 0.0) && (d < static_cast<double>((std::numeric_limits<uint64_t>::max)())) &&
                   (u == static_cast<uint64_t>(d));
        }
        if (IsInt64())
        {
            int64_t i = GetInt64();
            volatile double d = static_cast<double>(i);
            return (d >= static_cast<double>((std::numeric_limits<int64_t>::min)())) &&
                   (d < static_cast<double>((std::numeric_limits<int64_t>::max)())) &&
                   (i == static_cast<int64_t>(d));
        }
        return true; // double, int, uint are always lossless
    }

    //! Whether a number is a float (possible lossy).
    bool IsFloat() const
    {
        if ((flags_ & kDoubleFlag) == 0)
            return false;
        double d = GetDouble();
        return d >= -3.4028234e38 && d <= 3.4028234e38;
    }

    //! Whether a number can be losslessly converted to a float.
    bool IsLosslessFloat() const
    {
        if (!IsNumber())
            return false;
        double a = GetDouble();
        if (a < static_cast<double>(-(std::numeric_limits<float>::max)()) ||
            a > static_cast<double>((std::numeric_limits<float>::max)()))
            return false;
        double b = static_cast<double>(static_cast<float>(a));
        return a >= b && a <= b; // Prevent -Wfloat-equal
    }

    //@}

    //!@name Null
    //@{

    GenericValue& SetNull()
    {
        this->~GenericValue();
        new (this) GenericValue();
        return *this;
    }

    //@}

    //!@name Bool
    //@{

    bool GetBool() const
    {
        RAPIDYYJSON_ASSERT(IsBool());
        return flags_ == kTrueFlag;
    }

    GenericValue& SetBool(bool b)
    {
        this->~GenericValue();
        new (this) GenericValue(b);
        return *this;
    }

    //@}

    //!@name Object
    //@{

    //! Set this value as an empty object.
    GenericValue& SetObject()
    {
        this->~GenericValue();
        new (this) GenericValue(kObjectType);
        return *this;
    }

    //! Get the number of members in the object.
    SizeType MemberCount() const
    {
        RAPIDYYJSON_ASSERT(IsObject());
        return data_.o.size;
    }

    //! Get the capacity of object.
    SizeType MemberCapacity() const
    {
        RAPIDYYJSON_ASSERT(IsObject());
        return data_.o.capacity;
    }

    //! Check whether the object is empty.
    bool ObjectEmpty() const
    {
        RAPIDYYJSON_ASSERT(IsObject());
        return data_.o.size == 0;
    }

    //! Get a value from an object associated with the name.
    /*! \note In version 0.1x, if the member is not found, this function returns a null value.
              This makes issues when the stored value is not null (e.g. an empty object).
              Since 0.2, if the name is not correct, it will assert.
              If user is unsure whether a member exists, user should use HasMember() first.
              A better approach is to use FindMember().
    */
    template <typename T>
    RAPIDYYJSON_DISABLEIF_RETURN(
        (internal::NotExpr<internal::IsSame<typename internal::RemoveConst<T>::Type, Ch>>),
        (GenericValue&))
    operator[](T* name)
    {
        GenericValue n(StringRef(name));
        return (*this)[n];
    }

    template <typename T>
    RAPIDYYJSON_DISABLEIF_RETURN(
        (internal::NotExpr<internal::IsSame<typename internal::RemoveConst<T>::Type, Ch>>),
        (const GenericValue&))
    operator[](T* name) const
    {
        return const_cast<GenericValue&>(*this)[name];
    }

    //! Get a value from an object associated with the name.
    template <typename SourceAllocator>
    GenericValue& operator[](const GenericValue<Encoding, SourceAllocator>& name)
    {
        MemberIterator member = FindMember(name);
        if (member != MemberEnd())
            return member->value;
        else
        {
            RAPIDYYJSON_ASSERT(false); // see above note

            // This will generate -Wexit-time-destructors in clang
            // static GenericValue NullValue;
            // return NullValue;

            // Use static buffer and placement-new to prevent destruction
            static char buffer[sizeof(GenericValue)];
            return *new (buffer) GenericValue();
        }
    }

    template <typename SourceAllocator>
    const GenericValue& operator[](const GenericValue<Encoding, SourceAllocator>& name) const
    {
        return const_cast<GenericValue&>(*this)[name];
    }

#if RAPIDYYJSON_HAS_STDSTRING
    //! Get a value from an object associated with name (string object).
    GenericValue& operator[](const std::basic_string<Ch>& name)
    {
        return (*this)[GenericValue(StringRef(name))];
    }

    const GenericValue& operator[](const std::basic_string<Ch>& name) const
    {
        return (*this)[GenericValue(StringRef(name))];
    }
#endif

    //! Const member iterator
    ConstMemberIterator MemberBegin() const
    {
        RAPIDYYJSON_ASSERT(IsObject());
        return ConstMemberIterator(data_.o.members);
    }

    //! Const \em past-the-end member iterator
    ConstMemberIterator MemberEnd() const
    {
        RAPIDYYJSON_ASSERT(IsObject());
        return ConstMemberIterator(data_.o.members + data_.o.size);
    }

    //! Member iterator
    MemberIterator MemberBegin()
    {
        RAPIDYYJSON_ASSERT(IsObject());
        return MemberIterator(data_.o.members);
    }

    //! \em Past-the-end member iterator
    MemberIterator MemberEnd()
    {
        RAPIDYYJSON_ASSERT(IsObject());
        return MemberIterator(data_.o.members + data_.o.size);
    }

    //! Request the object to have enough capacity to store members.
    GenericValue& MemberReserve(SizeType newCapacity, Allocator& allocator)
    {
        RAPIDYYJSON_ASSERT(IsObject());
        if (newCapacity > data_.o.capacity)
        {
            data_.o.members = static_cast<Member*>(
                allocator.Realloc(data_.o.members,
                                  data_.o.capacity * sizeof(Member),
                                  newCapacity * sizeof(Member)));
            data_.o.capacity = newCapacity;
        }
        return *this;
    }

    //! Check whether a member exists in the object.
    bool HasMember(const Ch* name) const
    {
        return FindMember(name) != MemberEnd();
    }

#if RAPIDYYJSON_HAS_STDSTRING
    //! Check whether a member exists in the object with string object.
    bool HasMember(const std::basic_string<Ch>& name) const
    {
        return FindMember(name) != MemberEnd();
    }
#endif

    //! Check whether a member exists in the object with GenericValue name.
    template <typename SourceAllocator>
    bool HasMember(const GenericValue<Encoding, SourceAllocator>& name) const
    {
        return FindMember(name) != MemberEnd();
    }

    //! Find member by name.
    MemberIterator FindMember(const Ch* name)
    {
        GenericValue n(StringRef(name));
        return FindMember(n);
    }

    ConstMemberIterator FindMember(const Ch* name) const
    {
        return const_cast<GenericValue&>(*this).FindMember(name);
    }

    //! Find member by name.
    template <typename SourceAllocator>
    MemberIterator FindMember(const GenericValue<Encoding, SourceAllocator>& name)
    {
        RAPIDYYJSON_ASSERT(IsObject());
        RAPIDYYJSON_ASSERT(name.IsString());
        MemberIterator member = MemberBegin();
        for (; member != MemberEnd(); ++member)
            if (name.StringEqual(member->name))
                break;
        return member;
    }

    template <typename SourceAllocator>
    ConstMemberIterator FindMember(const GenericValue<Encoding, SourceAllocator>& name) const
    {
        return const_cast<GenericValue&>(*this).FindMember(name);
    }

#if RAPIDYYJSON_HAS_STDSTRING
    //! Find member by string object name.
    MemberIterator FindMember(const std::basic_string<Ch>& name)
    {
        return FindMember(GenericValue(StringRef(name)));
    }

    ConstMemberIterator FindMember(const std::basic_string<Ch>& name) const
    {
        return FindMember(GenericValue(StringRef(name)));
    }
#endif

    //! Add a member (name-value pair) to the object.
    /*! \param name A string value as name of member.
        \param value Value of any type.
        \param allocator Allocator for reallocating memory. It must be the same one as used before.
        \return The value itself for fluent API.
        \note The ownership of \c name and \c value will be transferred to this object on success.
    */
    GenericValue& AddMember(GenericValue& name, GenericValue& value, Allocator& allocator)
    {
        RAPIDYYJSON_ASSERT(IsObject());
        RAPIDYYJSON_ASSERT(name.IsString());

        ObjectData& o = data_.o;
        if (o.size >= o.capacity)
            MemberReserve(o.capacity == 0 ? kDefaultObjectCapacity
                                          : (o.capacity + (o.capacity + 1) / 2),
                          allocator);
        Member* members = o.members;
        members[o.size].name.RawAssign(name);
        members[o.size].value.RawAssign(value);
        o.size++;
        return *this;
    }

    //! Add a constant string value as member (name-value pair) to the object.
    GenericValue& AddMember(GenericValue& name, StringRefType value, Allocator& allocator)
    {
        GenericValue v(value);
        return AddMember(name, v, allocator);
    }

#if RAPIDYYJSON_HAS_STDSTRING
    //! Add a string object as member (name-value pair) to the object.
    GenericValue& AddMember(GenericValue& name,
                            std::basic_string<Ch>& value,
                            Allocator& allocator)
    {
        GenericValue v(value, allocator);
        return AddMember(name, v, allocator);
    }
#endif

    //! Add any primitive value as member (name-value pair) to the object.
    template <typename T>
    RAPIDYYJSON_DISABLEIF_RETURN(
        (internal::OrExpr<internal::IsPointer<T>, internal::IsGenericValue<T>>),
        (GenericValue&))
    AddMember(GenericValue& name, T value, Allocator& allocator)
    {
        GenericValue v(value);
        return AddMember(name, v, allocator);
    }

    GenericValue& AddMember(GenericValue&& name, GenericValue&& value, Allocator& allocator)
    {
        return AddMember(name, value, allocator);
    }

    GenericValue& AddMember(GenericValue&& name, GenericValue& value, Allocator& allocator)
    {
        return AddMember(name, value, allocator);
    }

    GenericValue& AddMember(GenericValue& name, GenericValue&& value, Allocator& allocator)
    {
        return AddMember(name, value, allocator);
    }

    GenericValue& AddMember(StringRefType name, GenericValue&& value, Allocator& allocator)
    {
        GenericValue n(name);
        return AddMember(n, value, allocator);
    }

    //! Add a member (name-value pair) to the object.
    GenericValue& AddMember(StringRefType name, GenericValue& value, Allocator& allocator)
    {
        GenericValue n(name);
        return AddMember(n, value, allocator);
    }

    //! Add a constant string value as member (name-value pair) to the object.
    GenericValue& AddMember(StringRefType name, StringRefType value, Allocator& allocator)
    {
        GenericValue v(value);
        return AddMember(name, v, allocator);
    }

    //! Add any primitive value as member (name-value pair) to the object.
    template <typename T>
    RAPIDYYJSON_DISABLEIF_RETURN(
        (internal::OrExpr<internal::IsPointer<T>, internal::IsGenericValue<T>>),
        (GenericValue&))
    AddMember(StringRefType name, T value, Allocator& allocator)
    {
        GenericValue n(name);
        return AddMember(n, value, allocator);
    }

    //! Remove all members in the object.
    /*! This function do not deallocate memory in the object, i.e. the capacity is unchanged. */
    void RemoveAllMembers()
    {
        RAPIDYYJSON_ASSERT(IsObject());
        for (MemberIterator m = MemberBegin(); m != MemberEnd(); ++m)
            m->~Member();
        data_.o.size = 0;
    }

    //! Remove a member in object by its name.
    /*! \note Removing a member is implemented by moving the last member. So the ordering of
              members is changed. Use \ref EraseMember(ConstMemberIterator) instead for preserving
              the ordering.
    */
    bool RemoveMember(const Ch* name)
    {
        GenericValue n(StringRef(name));
        return RemoveMember(n);
    }

#if RAPIDYYJSON_HAS_STDSTRING
    bool RemoveMember(const std::basic_string<Ch>& name)
    {
        return RemoveMember(GenericValue(StringRef(name)));
    }
#endif

    template <typename SourceAllocator>
    bool RemoveMember(const GenericValue<Encoding, SourceAllocator>& name)
    {
        MemberIterator m = FindMember(name);
        if (m != MemberEnd())
        {
            RemoveMember(m);
            return true;
        }
        else
            return false;
    }

    //! Remove a member in object by iterator.
    /*! \note This function may reorder the object members. Use \ref
              EraseMember(ConstMemberIterator) if you need to preserve the
              relative order of the remaining members.
    */
    MemberIterator RemoveMember(MemberIterator m)
    {
        RAPIDYYJSON_ASSERT(IsObject());
        RAPIDYYJSON_ASSERT(data_.o.size > 0);
        RAPIDYYJSON_ASSERT(data_.o.members != 0);
        RAPIDYYJSON_ASSERT(m >= MemberBegin() && m < MemberEnd());

        MemberIterator last(data_.o.members + (data_.o.size - 1));
        if (data_.o.size > 1 && m != last)
            *m = *last; // Move the last one to this place
        else
            m->~Member(); // Only one left, just destroy
        --data_.o.size;
        return m;
    }

    //! Remove a member from an object by iterator.
    /*! \note Other than \ref RemoveMember(MemberIterator), this function preserves the relative
              order of the remaining object members.
    */
    MemberIterator EraseMember(ConstMemberIterator pos)
    {
        return EraseMember(pos, pos + 1);
    }

    //! Remove members in the range [first, last) from an object.
    MemberIterator EraseMember(ConstMemberIterator first, ConstMemberIterator last)
    {
        RAPIDYYJSON_ASSERT(IsObject());
        RAPIDYYJSON_ASSERT(data_.o.size > 0);
        RAPIDYYJSON_ASSERT(data_.o.members != 0);
        RAPIDYYJSON_ASSERT(first >= MemberBegin());
        RAPIDYYJSON_ASSERT(first <= last);
        RAPIDYYJSON_ASSERT(last <= MemberEnd());

        MemberIterator pos = MemberBegin() + (first - MemberBegin());
        for (MemberIterator itr = pos; itr != last; ++itr)
            itr->~Member();
        std::memmove(static_cast<void*>(&*pos),
                     &*last,
                     static_cast<size_t>(MemberEnd() - last) * sizeof(Member));
        data_.o.size -= static_cast<SizeType>(last - first);
        return pos;
    }

    //! Erase a member in object by its name.
    bool EraseMember(const Ch* name)
    {
        GenericValue n(StringRef(name));
        return EraseMember(n);
    }

#if RAPIDYYJSON_HAS_STDSTRING
    bool EraseMember(const std::basic_string<Ch>& name)
    {
        return EraseMember(GenericValue(StringRef(name)));
    }
#endif

    template <typename SourceAllocator>
    bool EraseMember(const GenericValue<Encoding, SourceAllocator>& name)
    {
        MemberIterator m = FindMember(name);
        if (m != MemberEnd())
        {
            EraseMember(m);
            return true;
        }
        else
            return false;
    }

    Object GetObject()
    {
        RAPIDYYJSON_ASSERT(IsObject());
        return Object(*this);
    }

    Object GetObj()
    {
        RAPIDYYJSON_ASSERT(IsObject());
        return Object(*this);
    }

    ConstObject GetObject() const
    {
        RAPIDYYJSON_ASSERT(IsObject());
        return ConstObject(*this);
    }

    ConstObject GetObj() const
    {
        RAPIDYYJSON_ASSERT(IsObject());
        return ConstObject(*this);
    }

    //@}

    //!@name Array
    //@{

    //! Set this value as an empty array.
    GenericValue& SetArray()
    {
        this->~GenericValue();
        new (this) GenericValue(kArrayType);
        return *this;
    }

    //! Get the number of elements in array.
    SizeType Size() const
    {
        RAPIDYYJSON_ASSERT(IsArray());
        return data_.a.size;
    }

    //! Get the capacity of array.
    SizeType Capacity() const
    {
        RAPIDYYJSON_ASSERT(IsArray());
        return data_.a.capacity;
    }

    //! Check whether the array is empty.
    bool Empty() const
    {
        RAPIDYYJSON_ASSERT(IsArray());
        return data_.a.size == 0;
    }

    //! Remove all elements in the array.
    /*! This function do not deallocate memory in the array, i.e. the capacity is unchanged. */
    void Clear()
    {
        RAPIDYYJSON_ASSERT(IsArray());
        GenericValue* e = data_.a.elements;
        for (GenericValue* v = e; v != e + data_.a.size; ++v)
            v->~GenericValue();
        data_.a.size = 0;
    }

    //! Get an element from array by index.
    GenericValue& operator[](SizeType index)
    {
        RAPIDYYJSON_ASSERT(IsArray());
        RAPIDYYJSON_ASSERT(index < data_.a.size);
        return data_.a.elements[index];
    }

    const GenericValue& operator[](SizeType index) const
    {
        return const_cast<GenericValue&>(*this)[index];
    }

    //! Element iterator
    ValueIterator Begin()
    {
        RAPIDYYJSON_ASSERT(IsArray());
        return data_.a.elements;
    }

    //! \em Past-the-end element iterator
    ValueIterator End()
    {
        RAPIDYYJSON_ASSERT(IsArray());
        return data_.a.elements + data_.a.size;
    }

    //! Constant element iterator
    ConstValueIterator Begin() const
    {
        return const_cast<GenericValue&>(*this).Begin();
    }

    //! Constant \em past-the-end element iterator
    ConstValueIterator End() const
    {
        return const_cast<GenericValue&>(*this).End();
    }

    //! Request the array to have enough capacity to store elements.
    GenericValue& Reserve(SizeType newCapacity, Allocator& allocator)
    {
        RAPIDYYJSON_ASSERT(IsArray());
        if (newCapacity > data_.a.capacity)
        {
            data_.a.elements = static_cast<GenericValue*>(
                allocator.Realloc(data_.a.elements,
                                  data_.a.capacity * sizeof(GenericValue),
                                  newCapacity * sizeof(GenericValue)));
            data_.a.capacity = newCapacity;
        }
        return *this;
    }

    //! Append a GenericValue at the end of the array.
    /*! \note The ownership of \c value will be transferred to this array on success. */
    GenericValue& PushBack(GenericValue& value, Allocator& allocator)
    {
        RAPIDYYJSON_ASSERT(IsArray());
        if (data_.a.size >= data_.a.capacity)
            Reserve(data_.a.capacity == 0
                        ? kDefaultArrayCapacity
                        : (data_.a.capacity + (data_.a.capacity + 1) / 2),
                    allocator);
        data_.a.elements[data_.a.size++].RawAssign(value);
        return *this;
    }

    GenericValue& PushBack(GenericValue&& value, Allocator& allocator)
    {
        return PushBack(value, allocator);
    }

    //! Append a constant string reference at the end of the array.
    GenericValue& PushBack(StringRefType value, Allocator& allocator)
    {
        return (*this).template PushBack<StringRefType>(value, allocator);
    }

    //! Append a primitive value at the end of the array.
    template <typename T>
    RAPIDYYJSON_DISABLEIF_RETURN(
        (internal::OrExpr<internal::IsPointer<T>, internal::IsGenericValue<T>>),
        (GenericValue&))
    PushBack(T value, Allocator& allocator)
    {
        GenericValue v(value);
        return PushBack(v, allocator);
    }

    //! Remove the last element in the array.
    GenericValue& PopBack()
    {
        RAPIDYYJSON_ASSERT(IsArray());
        RAPIDYYJSON_ASSERT(!Empty());
        End()[-1].~GenericValue();
        --data_.a.size;
        return *this;
    }

    //! Remove an element of array by iterator.
    ValueIterator Erase(ConstValueIterator pos)
    {
        return Erase(pos, pos + 1);
    }

    //! Remove elements in the range [first, last) of the array.
    ValueIterator Erase(ConstValueIterator first, ConstValueIterator last)
    {
        RAPIDYYJSON_ASSERT(IsArray());
        RAPIDYYJSON_ASSERT(data_.a.size > 0);
        RAPIDYYJSON_ASSERT(data_.a.elements != 0);
        RAPIDYYJSON_ASSERT(first >= Begin());
        RAPIDYYJSON_ASSERT(first <= last);
        RAPIDYYJSON_ASSERT(last <= End());
        ValueIterator pos = Begin() + (first - Begin());
        for (ValueIterator itr = pos; itr != last; ++itr)
            itr->~GenericValue();
        std::memmove(static_cast<void*>(pos),
                     last,
                     static_cast<size_t>(End() - last) * sizeof(GenericValue));
        data_.a.size -= static_cast<SizeType>(last - first);
        return pos;
    }

    Array GetArray()
    {
        RAPIDYYJSON_ASSERT(IsArray());
        return Array(*this);
    }

    ConstArray GetArray() const
    {
        RAPIDYYJSON_ASSERT(IsArray());
        return ConstArray(*this);
    }

    //@}

    //!@name Number
    //@{

    int GetInt() const
    {
        RAPIDYYJSON_ASSERT(flags_ & kIntFlag);
        return static_cast<int>(data_.n.i64);
    }

    unsigned GetUint() const
    {
        RAPIDYYJSON_ASSERT(flags_ & kUintFlag);
        return static_cast<unsigned>(data_.n.u64);
    }

    int64_t GetInt64() const
    {
        RAPIDYYJSON_ASSERT(flags_ & kInt64Flag);
        return data_.n.i64;
    }

    uint64_t GetUint64() const
    {
        RAPIDYYJSON_ASSERT(flags_ & kUint64Flag);
        return data_.n.u64;
    }

    //! Get the value as double type.
    /*! \note If the value is 64-bit integer type, it may lose precision. Use \c IsLosslessDouble()
              to check whether the conversion is lossless.
    */
    double GetDouble() const
    {
        RAPIDYYJSON_ASSERT(IsNumber());
        if ((flags_ & kDoubleFlag) != 0)
            return data_.n.d; // exact type, no conversion.
        if ((flags_ & kIntFlag) != 0)
            return static_cast<double>(static_cast<int>(data_.n.i64)); // int -> double
        if ((flags_ & kUintFlag) != 0)
            return static_cast<double>(static_cast<unsigned>(data_.n.u64)); // unsigned -> double
        if ((flags_ & kInt64Flag) != 0)
            return static_cast<double>(data_.n.i64); // int64_t -> double (may lose precision)
        RAPIDYYJSON_ASSERT((flags_ & kUint64Flag) != 0);
        return static_cast<double>(data_.n.u64); // uint64_t -> double (may lose precision)
    }

    //! Get the value as float type.
    float GetFloat() const
    {
        return static_cast<float>(GetDouble());
    }

    GenericValue& SetInt(int i)
    {
        this->~GenericValue();
        new (this) GenericValue(i);
        return *this;
    }

    GenericValue& SetUint(unsigned u)
    {
        this->~GenericValue();
        new (this) GenericValue(u);
        return *this;
    }

    GenericValue& SetInt64(int64_t i64)
    {
        this->~GenericValue();
        new (this) GenericValue(i64);
        return *this;
    }

    GenericValue& SetUint64(uint64_t u64)
    {
        this->~GenericValue();
        new (this) GenericValue(u64);
        return *this;
    }

    GenericValue& SetDouble(double d)
    {
        this->~GenericValue();
        new (this) GenericValue(d);
        return *this;
    }

    GenericValue& SetFloat(float f)
    {
        this->~GenericValue();
        new (this) GenericValue(static_cast<double>(f));
        return *this;
    }

    //@}

    //!@name String
    //@{

    const Ch* GetString() const
    {
        RAPIDYYJSON_ASSERT(IsString());
        return data_.s.str;
    }

    //! Get the length of string.
    /*! Since rapidyyjson permits "\\u0000" in the json string, strlen(v.GetString()) may not be
        equal to v.GetStringLength().
    */
    SizeType GetStringLength() const
    {
        RAPIDYYJSON_ASSERT(IsString());
        return data_.s.length;
    }

    //! Set this value as a string without copying source string.
    GenericValue& SetString(const Ch* s, SizeType length)
    {
        return SetString(StringRef(s, length));
    }

    //! Set this value as a string without copying source string.
    GenericValue& SetString(StringRefType s)
    {
        this->~GenericValue();
        SetStringRaw(s);
        return *this;
    }

    //! Set this value as a string by copying from source string.
    GenericValue& SetString(const Ch* s, SizeType length, Allocator& allocator)
    {
        return SetString(StringRef(s, length), allocator);
    }

    //! Set this value as a string by copying from source string.
    GenericValue& SetString(const Ch* s, Allocator& allocator)
    {
        return SetString(StringRef(s), allocator);
    }

    //! Set this value as a string by copying from source string.
    GenericValue& SetString(StringRefType s, Allocator& allocator)
    {
        this->~GenericValue();
        SetStringRaw(s, allocator);
        return *this;
    }

#if RAPIDYYJSON_HAS_STDSTRING
    //! Set this value as a string by copying from source string.
    GenericValue& SetString(const std::basic_string<Ch>& s, Allocator& allocator)
    {
        return SetString(StringRef(s), allocator);
    }
#endif

    //@}

    //!@name Array
    //@{

    //! Templated version for checking whether this value is type T.
    /*!
        \tparam T Either \c bool, \c int, \c unsigned, \c int64_t, \c uint64_t, \c double, \c float,
                  \c const \c char*, \c std::basic_string<Ch>
    */
    template <typename T>
    bool Is() const
    {
        return internal::TypeHelper<ValueType, T>::Is(*this);
    }

    template <typename T>
    T Get() const
    {
        return internal::TypeHelper<ValueType, T>::Get(*this);
    }

    template <typename T>
    T Get()
    {
        return internal::TypeHelper<ValueType, T>::Get(*this);
    }

    template <typename T>
    ValueType& Set(const T& data)
    {
        return internal::TypeHelper<ValueType, T>::Set(*this, data);
    }

    template <typename T>
    ValueType& Set(const T& data, AllocatorType& allocator)
    {
        return internal::TypeHelper<ValueType, T>::Set(*this, data, allocator);
    }

    //@}

    //! Generate events of this value to a Handler.
    /*! This function adopts the GoF visitor pattern.
        Typical usage is to output this JSON value as JSON text via Writer, which is a Handler.
    */
    template <typename Handler>
    bool Accept(Handler& handler) const
    {
        switch (GetType())
        {
        case kNullType:
            return handler.Null();
        case kFalseType:
            return handler.Bool(false);
        case kTrueType:
            return handler.Bool(true);

        case kObjectType:
            if (RAPIDYYJSON_UNLIKELY(!handler.StartObject()))
                return false;
            for (ConstMemberIterator m = MemberBegin(); m != MemberEnd(); ++m)
            {
                RAPIDYYJSON_ASSERT(m->name.IsString()); // User may change the type of name by
                                                        // MemberIterator.
                if (RAPIDYYJSON_UNLIKELY(!handler.Key(m->name.GetString(),
                                                      m->name.GetStringLength(),
                                                      m->name.IsCopyString())))
                    return false;
                if (RAPIDYYJSON_UNLIKELY(!m->value.Accept(handler)))
                    return false;
            }
            return handler.EndObject(data_.o.size);

        case kArrayType:
            if (RAPIDYYJSON_UNLIKELY(!handler.StartArray()))
                return false;
            for (ConstValueIterator v = Begin(); v != End(); ++v)
                if (RAPIDYYJSON_UNLIKELY(!v->Accept(handler)))
                    return false;
            return handler.EndArray(data_.a.size);

        case kStringType:
            return handler.String(GetString(), GetStringLength(), IsCopyString());

        default:
            RAPIDYYJSON_ASSERT(GetType() == kNumberType);
            if (IsDouble())
                return handler.Double(data_.n.d);
            else if (IsInt())
                return handler.Int(GetInt());
            else if (IsUint())
                return handler.Uint(GetUint());
            else if (IsInt64())
                return handler.Int64(data_.n.i64);
            else
                return handler.Uint64(data_.n.u64);
        }
    }

  private:
    template <typename, typename>
    friend class GenericValue;
    template <typename, typename, typename>
    friend class GenericDocument;
    template <bool, typename>
    friend class GenericArray;
    template <bool, typename>
    friend class GenericObject;

    enum
    {
        kBoolFlag = 0x0008,
        kNumberFlag = 0x0010,
        kIntFlag = 0x0020,
        kUintFlag = 0x0040,
        kInt64Flag = 0x0080,
        kUint64Flag = 0x0100,
        kDoubleFlag = 0x0200,
        kStringFlag = 0x0400,
        kCopyFlag = 0x0800,
        kInlineStrFlag = 0x1000,

        // Initial flags of different types.
        kNullFlag = kNullType,
        // These casts are added to suppress the warning on MSVC about bitwise operations between
        // enums of different types.
        kTrueFlag = static_cast<int>(kTrueType) | static_cast<int>(kBoolFlag),
        kFalseFlag = static_cast<int>(kFalseType) | static_cast<int>(kBoolFlag),
        kNumberIntFlag =
            static_cast<int>(kNumberType) | static_cast<int>(kNumberFlag | kIntFlag | kInt64Flag),
        kNumberUintFlag = static_cast<int>(kNumberType) |
                          static_cast<int>(kNumberFlag | kUintFlag | kUint64Flag | kInt64Flag),
        kNumberInt64Flag = static_cast<int>(kNumberType) | static_cast<int>(kNumberFlag | kInt64Flag),
        kNumberUint64Flag =
            static_cast<int>(kNumberType) | static_cast<int>(kNumberFlag | kUint64Flag),
        kNumberDoubleFlag =
            static_cast<int>(kNumberType) | static_cast<int>(kNumberFlag | kDoubleFlag),
        kNumberAnyFlag = static_cast<int>(kNumberType) |
                         static_cast<int>(kNumberFlag | kIntFlag | kInt64Flag | kUintFlag |
                                          kUint64Flag | kDoubleFlag),
        kConstStringFlag = static_cast<int>(kStringType) | static_cast<int>(kStringFlag),
        kCopyStringFlag = static_cast<int>(kStringType) | static_cast<int>(kStringFlag | kCopyFlag),
        kShortStringFlag = static_cast<int>(kStringType) | static_cast<int>(kStringFlag),
        kObjectFlag = kObjectType,
        kArrayFlag = kArrayType,

        kTypeMask = 0x07
    };

    static const SizeType kDefaultArrayCapacity = RAPIDYYJSON_VALUE_DEFAULT_ARRAY_CAPACITY;
    static const SizeType kDefaultObjectCapacity = RAPIDYYJSON_VALUE_DEFAULT_OBJECT_CAPACITY;

    struct String
    {
        const Ch* str;
        SizeType length;
    };

    struct Number
    {
        union {
            int64_t i64;
            uint64_t u64;
            double d;
        };
    };

    struct ObjectData
    {
        SizeType size;
        SizeType capacity;
        Member* members;
    };

    struct ArrayData
    {
        SizeType size;
        SizeType capacity;
        GenericValue* elements;
    };

    union Data {
        String s;
        Number n;
        ObjectData o;
        ArrayData a;
    };

    static const Ch emptyString_[];

    //! Whether the string content is owned by this value (i.e. was copied through an allocator).
    bool IsCopyString() const
    {
        return (flags_ & kCopyFlag) != 0;
    }

    //! Initialize this value as array with initial data, without calling destructor.
    void SetArrayRaw(GenericValue* values, SizeType count, Allocator& allocator)
    {
        flags_ = kArrayFlag;
        if (count)
        {
            GenericValue* e =
                static_cast<GenericValue*>(allocator.Malloc(count * sizeof(GenericValue)));
            data_.a.elements = e;
            std::memcpy(static_cast<void*>(e),
                        static_cast<const void*>(values),
                        count * sizeof(GenericValue));
        }
        else
            data_.a.elements = 0;
        data_.a.size = data_.a.capacity = count;
    }

    //! Initialize this value as object with initial data, without calling destructor.
    void SetObjectRaw(Member* members, SizeType count, Allocator& allocator)
    {
        flags_ = kObjectFlag;
        if (count)
        {
            Member* m = static_cast<Member*>(allocator.Malloc(count * sizeof(Member)));
            data_.o.members = m;
            std::memcpy(static_cast<void*>(m),
                        static_cast<const void*>(members),
                        count * sizeof(Member));
        }
        else
            data_.o.members = 0;
        data_.o.size = data_.o.capacity = count;
    }

    //! Initialize this value as constant string, without calling destructor.
    void SetStringRaw(StringRefType s) RAPIDYYJSON_NOEXCEPT
    {
        data_.s.str = s.s;
        data_.s.length = s.length;
        flags_ = kConstStringFlag;
    }

    //! Initialize this value as copy string with initial data, without calling destructor.
    void SetStringRaw(StringRefType s, Allocator& allocator)
    {
        Ch* str = static_cast<Ch*>(allocator.Malloc((s.length + 1) * sizeof(Ch)));
        if (s.length > 0)
            std::memcpy(str, s.s, s.length * sizeof(Ch));
        str[s.length] = Ch();
        data_.s.str = str;
        data_.s.length = s.length;
        flags_ = kCopyStringFlag;
    }

    //! Assignment without calling destructor
    void RawAssign(GenericValue& rhs) RAPIDYYJSON_NOEXCEPT
    {
        data_ = rhs.data_;
        flags_ = rhs.flags_;
        rhs.flags_ = kNullFlag;
    }

    template <typename SourceAllocator>
    bool StringEqual(const GenericValue<Encoding, SourceAllocator>& rhs) const
    {
        RAPIDYYJSON_ASSERT(IsString());
        RAPIDYYJSON_ASSERT(rhs.IsString());

        const SizeType len1 = GetStringLength();
        const SizeType len2 = rhs.GetStringLength();
        if (len1 != len2)
            return false;

        const Ch* const str1 = GetString();
        const Ch* const str2 = rhs.GetString();
        if (str1 == str2)
            return true; // fast path for constant string

        return (std::memcmp(str1, str2, sizeof(Ch) * len1) == 0);
    }

    Data data_;
    uint16_t flags_;
};

template <typename Encoding, typename Allocator>
const typename GenericValue<Encoding, Allocator>::Ch
    GenericValue<Encoding, Allocator>::emptyString_[] = {
        typename GenericValue<Encoding, Allocator>::Ch()};

//! GenericValue with UTF8 encoding
typedef GenericValue<UTF8<>> Value;

///////////////////////////////////////////////////////////////////////////////
// GenericArray

//! Helper class for accessing Value of array type.
/*!
    Instance of this helper class is obtained by \c GenericValue::GetArray().
    In addition to all APIs for array type, it provides range-based for loop if
    \c RAPIDYYJSON_HAS_CXX11_RANGE_FOR=1.
*/
template <bool Const, typename ValueT>
class GenericArray
{
  public:
    typedef GenericArray<true, ValueT> ConstArray;
    typedef GenericArray<false, ValueT> Array;
    typedef ValueT PlainType;
    typedef typename internal::MaybeAddConst<Const, PlainType>::Type ValueType;
    typedef ValueType* ValueIterator; // This may be const or non-const iterator
    typedef const ValueT* ConstValueIterator;
    typedef typename ValueType::AllocatorType AllocatorType;
    typedef typename ValueType::StringRefType StringRefType;

    template <typename, typename>
    friend class GenericValue;

    GenericArray(const GenericArray& rhs)
        : value_(rhs.value_)
    {
    }

    GenericArray& operator=(const GenericArray& rhs)
    {
        value_ = rhs.value_;
        return *this;
    }

    ~GenericArray()
    {
    }

    operator ValueType&() const
    {
        return value_;
    }

    SizeType Size() const
    {
        return value_.Size();
    }

    SizeType Capacity() const
    {
        return value_.Capacity();
    }

    bool Empty() const
    {
        return value_.Empty();
    }

    void Clear() const
    {
        value_.Clear();
    }

    ValueType& operator[](SizeType index) const
    {
        return value_[index];
    }

    ValueIterator Begin() const
    {
        return value_.Begin();
    }

    ValueIterator End() const
    {
        return value_.End();
    }

    GenericArray Reserve(SizeType newCapacity, AllocatorType& allocator) const
    {
        value_.Reserve(newCapacity, allocator);
        return *this;
    }

    GenericArray PushBack(ValueType& value, AllocatorType& allocator) const
    {
        value_.PushBack(value, allocator);
        return *this;
    }

    GenericArray PushBack(ValueType&& value, AllocatorType& allocator) const
    {
        value_.PushBack(value, allocator);
        return *this;
    }

    GenericArray PushBack(StringRefType value, AllocatorType& allocator) const
    {
        value_.PushBack(value, allocator);
        return *this;
    }

    template <typename T>
    RAPIDYYJSON_DISABLEIF_RETURN(
        (internal::OrExpr<internal::IsPointer<T>, internal::IsGenericValue<T>>),
        (const GenericArray&))
    PushBack(T value, AllocatorType& allocator) const
    {
        value_.PushBack(value, allocator);
        return *this;
    }

    GenericArray PopBack() const
    {
        value_.PopBack();
        return *this;
    }

    ValueIterator Erase(ConstValueIterator pos) const
    {
        return value_.Erase(pos);
    }

    ValueIterator Erase(ConstValueIterator first, ConstValueIterator last) const
    {
        return value_.Erase(first, last);
    }

#if RAPIDYYJSON_HAS_CXX11_RANGE_FOR
    ValueIterator begin() const
    {
        return value_.Begin();
    }

    ValueIterator end() const
    {
        return value_.End();
    }
#endif

  private:
    GenericArray();

    GenericArray(ValueType& value)
        : value_(value)
    {
    }

    ValueType& value_;
};

///////////////////////////////////////////////////////////////////////////////
// GenericObject

//! Helper class for accessing Value of object type.
/*!
    Instance of this helper class is obtained by \c GenericValue::GetObject().
    In addition to all APIs for array type, it provides range-based for loop if
    \c RAPIDYYJSON_HAS_CXX11_RANGE_FOR=1.
*/
template <bool Const, typename ValueT>
class GenericObject
{
  public:
    typedef GenericObject<true, ValueT> ConstObject;
    typedef GenericObject<false, ValueT> Object;
    typedef ValueT PlainType;
    typedef typename internal::MaybeAddConst<Const, PlainType>::Type ValueType;
    typedef GenericMemberIterator<Const, typename ValueT::EncodingType, typename ValueT::AllocatorType>
        MemberIterator; // This may be const or non-const iterator
    typedef GenericMemberIterator<true, typename ValueT::EncodingType, typename ValueT::AllocatorType>
        ConstMemberIterator;
    typedef typename ValueType::AllocatorType AllocatorType;
    typedef typename ValueType::StringRefType StringRefType;
    typedef typename ValueType::EncodingType EncodingType;
    typedef typename ValueType::Ch Ch;

    template <typename, typename>
    friend class GenericValue;

    GenericObject(const GenericObject& rhs)
        : value_(rhs.value_)
    {
    }

    GenericObject& operator=(const GenericObject& rhs)
    {
        value_ = rhs.value_;
        return *this;
    }

    ~GenericObject()
    {
    }

    operator ValueType&() const
    {
        return value_;
    }

    SizeType MemberCount() const
    {
        return value_.MemberCount();
    }

    SizeType MemberCapacity() const
    {
        return value_.MemberCapacity();
    }

    bool ObjectEmpty() const
    {
        return value_.ObjectEmpty();
    }

    template <typename T>
    ValueType& operator[](T* name) const
    {
        return value_[name];
    }

    template <typename SourceAllocator>
    ValueType& operator[](
        const GenericValue<EncodingType, SourceAllocator>& name) const
    {
        return value_[name];
    }

#if RAPIDYYJSON_HAS_STDSTRING
    ValueType& operator[](const std::basic_string<Ch>& name) const
    {
        return value_[name];
    }
#endif

    MemberIterator MemberBegin() const
    {
        return MemberIterator(value_.MemberBegin());
    }

    MemberIterator MemberEnd() const
    {
        return MemberIterator(value_.MemberEnd());
    }

    GenericObject MemberReserve(SizeType newCapacity, AllocatorType& allocator) const
    {
        value_.MemberReserve(newCapacity, allocator);
        return *this;
    }

    bool HasMember(const Ch* name) const
    {
        return value_.HasMember(name);
    }

#if RAPIDYYJSON_HAS_STDSTRING
    bool HasMember(const std::basic_string<Ch>& name) const
    {
        return value_.HasMember(name);
    }
#endif

    template <typename SourceAllocator>
    bool HasMember(const GenericValue<EncodingType, SourceAllocator>& name) const
    {
        return value_.HasMember(name);
    }

    MemberIterator FindMember(const Ch* name) const
    {
        return MemberIterator(value_.FindMember(name));
    }

    template <typename SourceAllocator>
    MemberIterator FindMember(const GenericValue<EncodingType, SourceAllocator>& name) const
    {
        return MemberIterator(value_.FindMember(name));
    }

#if RAPIDYYJSON_HAS_STDSTRING
    MemberIterator FindMember(const std::basic_string<Ch>& name) const
    {
        return MemberIterator(value_.FindMember(name));
    }
#endif

    GenericObject AddMember(ValueType& name, ValueType& value, AllocatorType& allocator) const
    {
        value_.AddMember(name, value, allocator);
        return *this;
    }

    GenericObject AddMember(ValueType& name, StringRefType value, AllocatorType& allocator) const
    {
        value_.AddMember(name, value, allocator);
        return *this;
    }

#if RAPIDYYJSON_HAS_STDSTRING
    GenericObject AddMember(ValueType& name,
                            std::basic_string<Ch>& value,
                            AllocatorType& allocator) const
    {
        value_.AddMember(name, value, allocator);
        return *this;
    }
#endif

    template <typename T>
    RAPIDYYJSON_DISABLEIF_RETURN(
        (internal::OrExpr<internal::IsPointer<typename internal::RemoveConst<T>::Type>,
                          internal::IsGenericValue<typename internal::RemoveConst<T>::Type>>),
        (GenericObject))
    AddMember(ValueType& name, T value, AllocatorType& allocator) const
    {
        value_.AddMember(name, value, allocator);
        return *this;
    }

    GenericObject AddMember(ValueType&& name, ValueType&& value, AllocatorType& allocator) const
    {
        value_.AddMember(name, value, allocator);
        return *this;
    }

    GenericObject AddMember(ValueType&& name, ValueType& value, AllocatorType& allocator) const
    {
        value_.AddMember(name, value, allocator);
        return *this;
    }

    GenericObject AddMember(ValueType& name, ValueType&& value, AllocatorType& allocator) const
    {
        value_.AddMember(name, value, allocator);
        return *this;
    }

    GenericObject AddMember(StringRefType name, ValueType&& value, AllocatorType& allocator) const
    {
        value_.AddMember(name, value, allocator);
        return *this;
    }

    GenericObject AddMember(StringRefType name, ValueType& value, AllocatorType& allocator) const
    {
        value_.AddMember(name, value, allocator);
        return *this;
    }

    GenericObject AddMember(StringRefType name, StringRefType value, AllocatorType& allocator) const
    {
        value_.AddMember(name, value, allocator);
        return *this;
    }

    template <typename T>
    RAPIDYYJSON_DISABLEIF_RETURN(
        (internal::OrExpr<internal::IsPointer<T>, internal::IsGenericValue<T>>),
        (GenericObject))
    AddMember(StringRefType name, T value, AllocatorType& allocator) const
    {
        value_.AddMember(name, value, allocator);
        return *this;
    }

    void RemoveAllMembers()
    {
        value_.RemoveAllMembers();
    }

    bool RemoveMember(const Ch* name) const
    {
        return value_.RemoveMember(name);
    }

#if RAPIDYYJSON_HAS_STDSTRING
    bool RemoveMember(const std::basic_string<Ch>& name) const
    {
        return value_.RemoveMember(name);
    }
#endif

    template <typename SourceAllocator>
    bool RemoveMember(const GenericValue<EncodingType, SourceAllocator>& name) const
    {
        return value_.RemoveMember(name);
    }

    MemberIterator RemoveMember(MemberIterator m) const
    {
        return value_.RemoveMember(m);
    }

    MemberIterator EraseMember(ConstMemberIterator pos) const
    {
        return value_.EraseMember(pos);
    }

    MemberIterator EraseMember(ConstMemberIterator first, ConstMemberIterator last) const
    {
        return value_.EraseMember(first, last);
    }

    bool EraseMember(const Ch* name) const
    {
        return value_.EraseMember(name);
    }

#if RAPIDYYJSON_HAS_STDSTRING
    bool EraseMember(const std::basic_string<Ch>& name) const
    {
        return EraseMember(ValueType(StringRef(name)));
    }
#endif

    template <typename SourceAllocator>
    bool EraseMember(const GenericValue<EncodingType, SourceAllocator>& name) const
    {
        return value_.EraseMember(name);
    }

#if RAPIDYYJSON_HAS_CXX11_RANGE_FOR
    MemberIterator begin() const
    {
        return MemberIterator(value_.MemberBegin());
    }

    MemberIterator end() const
    {
        return MemberIterator(value_.MemberEnd());
    }
#endif

  private:
    GenericObject();

    GenericObject(ValueType& value)
        : value_(value)
    {
    }

    ValueType& value_;
};

///////////////////////////////////////////////////////////////////////////////
// GenericDocument

//! A document for parsing JSON text as DOM.
/*!
    \note implements Handler concept
    \tparam Encoding Encoding for both parsing and string storage.
    \tparam Allocator Allocator for allocating memory for the DOM
    \tparam StackAllocator Allocator for allocating memory for stack during parsing.
    \warning Although GenericDocument inherits from GenericValue, the API does \b not provide any
             virtual functions, especially no virtual destructor. To avoid memory leaks, do not
             \c delete a GenericDocument object via a pointer to a GenericValue.
*/
template <typename Encoding,
          typename Allocator = RAPIDYYJSON_DEFAULT_ALLOCATOR,
          typename StackAllocator = RAPIDYYJSON_DEFAULT_STACK_ALLOCATOR>
class GenericDocument : public GenericValue<Encoding, Allocator>
{
  public:
    typedef typename Encoding::Ch Ch;                     //!< Character type derived from Encoding.
    typedef GenericValue<Encoding, Allocator> ValueType;  //!< Value type of the document.
    typedef Allocator AllocatorType;                      //!< Allocator type from template
                                                          //!< parameter.

    //! Constructor
    /*! Creates an empty document of specified type.
        \param type             Mandatory type of object to create.
        \param allocator        Optional allocator for allocating memory.
        \param stackCapacity    Optional initial capacity of stack in bytes.
        \param stackAllocator   Optional allocator for allocating memory for stack.
    */
    explicit GenericDocument(Type type,
                             Allocator* allocator = 0,
                             size_t stackCapacity = kDefaultStackCapacity,
                             StackAllocator* stackAllocator = 0)
        : GenericValue<Encoding, Allocator>(type),
          allocator_(allocator),
          ownAllocator_(0),
          stack_(stackAllocator, stackCapacity),
          parseResult_()
    {
        if (!allocator_)
            ownAllocator_ = allocator_ = RAPIDYYJSON_NEW(Allocator)();
    }

    //! Constructor
    /*! Creates an empty document which type is Null.
        \param allocator        Optional allocator for allocating memory.
        \param stackCapacity    Optional initial capacity of stack in bytes.
        \param stackAllocator   Optional allocator for allocating memory for stack.
    */
    GenericDocument(Allocator* allocator = 0,
                    size_t stackCapacity = kDefaultStackCapacity,
                    StackAllocator* stackAllocator = 0)
        : allocator_(allocator),
          ownAllocator_(0),
          stack_(stackAllocator, stackCapacity),
          parseResult_()
    {
        if (!allocator_)
            ownAllocator_ = allocator_ = RAPIDYYJSON_NEW(Allocator)();
    }

    //! Move constructor in C++11
    GenericDocument(GenericDocument&& rhs) RAPIDYYJSON_NOEXCEPT
        : ValueType(std::forward<ValueType>(rhs)), // explicit cast, to avoid the ambiguity with
                                                   // the copy constructor
          allocator_(rhs.allocator_),
          ownAllocator_(rhs.ownAllocator_),
          stack_(std::move(rhs.stack_)),
          parseResult_(rhs.parseResult_)
    {
        rhs.allocator_ = 0;
        rhs.ownAllocator_ = 0;
        rhs.parseResult_ = ParseResult();
    }

    ~GenericDocument()
    {
        // Clear the ::ValueType before ownAllocator is destroyed, ~ValueType() might access it.
        Destroy();
    }

    //! Move assignment in C++11
    GenericDocument& operator=(GenericDocument&& rhs) RAPIDYYJSON_NOEXCEPT
    {
        // The cast to ValueType is necessary here, because otherwise it would
        // attempt to call GenericValue's templated assignment operator.
        ValueType::operator=(std::forward<ValueType>(rhs));

        // Calling the destructor here would prematurely call stack_'s destructor
        Destroy();

        allocator_ = rhs.allocator_;
        ownAllocator_ = rhs.ownAllocator_;
        stack_ = std::move(rhs.stack_);
        parseResult_ = rhs.parseResult_;

        rhs.allocator_ = 0;
        rhs.ownAllocator_ = 0;
        rhs.parseResult_ = ParseResult();

        return *this;
    }

    //! Exchange the contents of this document with those of another.
    GenericDocument& Swap(GenericDocument& rhs) RAPIDYYJSON_NOEXCEPT
    {
        ValueType::Swap(rhs);
        stack_.Swap(rhs.stack_);
        internal::Swap(allocator_, rhs.allocator_);
        internal::Swap(ownAllocator_, rhs.ownAllocator_);
        internal::Swap(parseResult_, rhs.parseResult_);
        return *this;
    }

    // Allow Swap with ValueType.
    using ValueType::Swap;

    //! free-standing swap function helper
    friend inline void swap(GenericDocument& a, GenericDocument& b) RAPIDYYJSON_NOEXCEPT
    {
        a.Swap(b);
    }

    //! Populate this document by a generator which produces SAX events.
    template <typename Generator>
    GenericDocument& Populate(Generator& g)
    {
        ClearStackOnExit scope(*this);
        if (g(*this))
        {
            RAPIDYYJSON_ASSERT(stack_.GetSize() == sizeof(ValueType)); // Got one and only one root
                                                                       // object
            ValueType::operator=(*stack_.template Pop<ValueType>(1)); // Move value from stack to
                                                                      // document
        }
        return *this;
    }

    //!@name Parse from stream
    //!@{

    //! Parse JSON text from an input stream (with Encoding conversion)
    template <unsigned parseFlags, typename SourceEncoding, typename InputStream>
    GenericDocument& ParseStream(InputStream& is)
    {
        GenericReader<SourceEncoding, Encoding, StackAllocator> reader(
            stack_.HasAllocator() ? &stack_.GetAllocator() : 0);
        ClearStackOnExit scope(*this);
        parseResult_ = reader.template Parse<parseFlags>(is, *this);
        if (parseResult_)
        {
            RAPIDYYJSON_ASSERT(stack_.GetSize() == sizeof(ValueType)); // Got one and only one root
                                                                       // object
            ValueType::operator=(*stack_.template Pop<ValueType>(1)); // Move value from stack to
                                                                      // document
        }
        return *this;
    }

    //! Parse JSON text from an input stream
    template <unsigned parseFlags, typename InputStream>
    GenericDocument& ParseStream(InputStream& is)
    {
        return ParseStream<parseFlags, Encoding, InputStream>(is);
    }

    //! Parse JSON text from an input stream (with \ref kParseDefaultFlags)
    template <typename InputStream>
    GenericDocument& ParseStream(InputStream& is)
    {
        return ParseStream<kParseDefaultFlags, Encoding, InputStream>(is);
    }
    //!@}

    //!@name Parse in-place from mutable string
    //!@{

    //! Parse JSON text from a mutable string
    template <unsigned parseFlags>
    GenericDocument& ParseInsitu(Ch* str)
    {
        GenericInsituStringStream<Encoding> s(str);
        return ParseStream<parseFlags | kParseInsituFlag>(s);
    }

    //! Parse JSON text from a mutable string (with \ref kParseDefaultFlags)
    GenericDocument& ParseInsitu(Ch* str)
    {
        return ParseInsitu<kParseDefaultFlags>(str);
    }
    //!@}

    //!@name Parse from read-only string
    //!@{

    //! Parse JSON text from a read-only string (with Encoding conversion)
    template <unsigned parseFlags, typename SourceEncoding>
    GenericDocument& Parse(const typename SourceEncoding::Ch* str)
    {
        RAPIDYYJSON_ASSERT(!(parseFlags & kParseInsituFlag));
        GenericStringStream<SourceEncoding> s(str);
        return ParseStream<parseFlags, SourceEncoding>(s);
    }

    //! Parse JSON text from a read-only string
    template <unsigned parseFlags>
    GenericDocument& Parse(const Ch* str)
    {
        return Parse<parseFlags, Encoding>(str);
    }

    //! Parse JSON text from a read-only string (with \ref kParseDefaultFlags)
    GenericDocument& Parse(const Ch* str)
    {
        return Parse<kParseDefaultFlags>(str);
    }

    template <unsigned parseFlags, typename SourceEncoding>
    GenericDocument& Parse(const typename SourceEncoding::Ch* str, size_t length)
    {
        RAPIDYYJSON_ASSERT(!(parseFlags & kParseInsituFlag));
        MemoryStream ms(reinterpret_cast<const char*>(str),
                        length * sizeof(typename SourceEncoding::Ch));
        EncodedInputStream<SourceEncoding, MemoryStream> is(ms);
        ParseStream<parseFlags, SourceEncoding>(is);
        return *this;
    }

    template <unsigned parseFlags>
    GenericDocument& Parse(const Ch* str, size_t length)
    {
        return Parse<parseFlags, Encoding>(str, length);
    }

    GenericDocument& Parse(const Ch* str, size_t length)
    {
        return Parse<kParseDefaultFlags>(str, length);
    }

#if RAPIDYYJSON_HAS_STDSTRING
    template <unsigned parseFlags, typename SourceEncoding>
    GenericDocument& Parse(const std::basic_string<typename SourceEncoding::Ch>& str)
    {
        // c_str() is constant complexity according to standard. Should be faster than Parse(const
        // char*, size_t)
        return Parse<parseFlags, SourceEncoding>(str.c_str());
    }

    template <unsigned parseFlags>
    GenericDocument& Parse(const std::basic_string<Ch>& str)
    {
        return Parse<parseFlags, Encoding>(str.c_str());
    }

    GenericDocument& Parse(const std::basic_string<Ch>& str)
    {
        return Parse<kParseDefaultFlags>(str);
    }
#endif // RAPIDYYJSON_HAS_STDSTRING
    //!@}

    //!@name Handling parse errors
    //!@{

    //! Whether a parse error has occurred in the last parsing.
    bool HasParseError() const
    {
        return parseResult_.IsError();
    }

    //! Get the \ref ParseErrorCode of last parsing.
    ParseErrorCode GetParseError() const
    {
        return parseResult_.Code();
    }

    //! Get the position of last parsing error in input, 0 otherwise.
    size_t GetErrorOffset() const
    {
        return parseResult_.Offset();
    }

    //! Implicit conversion to get the last parse result
    operator ParseResult() const
    {
        return parseResult_;
    }
    //!@}

    //! Get the allocator of this document.
    Allocator& GetAllocator()
    {
        RAPIDYYJSON_ASSERT(allocator_);
        return *allocator_;
    }

    //! Get the capacity of stack in bytes.
    size_t GetStackCapacity() const
    {
        return stack_.GetCapacity();
    }

  private:
    //! Prohibit copying
    GenericDocument(const GenericDocument&);
    //! Prohibit assignment
    GenericDocument& operator=(const GenericDocument&);

    // callers of the following private Handler functions
    template <typename, typename, typename>
    friend class GenericReader; // for parsing
    template <typename, typename>
    friend class GenericValue; // for deep copying

  public:
    // Implementation of Handler
    bool Null()
    {
        new (stack_.template Push<ValueType>()) ValueType();
        return true;
    }

    bool Bool(bool b)
    {
        new (stack_.template Push<ValueType>()) ValueType(b);
        return true;
    }

    bool Int(int i)
    {
        new (stack_.template Push<ValueType>()) ValueType(i);
        return true;
    }

    bool Uint(unsigned i)
    {
        new (stack_.template Push<ValueType>()) ValueType(i);
        return true;
    }

    bool Int64(int64_t i)
    {
        new (stack_.template Push<ValueType>()) ValueType(i);
        return true;
    }

    bool Uint64(uint64_t i)
    {
        new (stack_.template Push<ValueType>()) ValueType(i);
        return true;
    }

    bool Double(double d)
    {
        new (stack_.template Push<ValueType>()) ValueType(d);
        return true;
    }

    bool RawNumber(const Ch* str, SizeType length, bool copy)
    {
        if (copy)
            new (stack_.template Push<ValueType>()) ValueType(str, length, GetAllocator());
        else
            new (stack_.template Push<ValueType>()) ValueType(str, length);
        return true;
    }

    bool String(const Ch* str, SizeType length, bool copy)
    {
        if (copy)
            new (stack_.template Push<ValueType>()) ValueType(str, length, GetAllocator());
        else
            new (stack_.template Push<ValueType>()) ValueType(str, length);
        return true;
    }

    bool StartObject()
    {
        new (stack_.template Push<ValueType>()) ValueType(kObjectType);
        return true;
    }

    bool Key(const Ch* str, SizeType length, bool copy)
    {
        return String(str, length, copy);
    }

    bool EndObject(SizeType memberCount)
    {
        typename ValueType::Member* members =
            stack_.template Pop<typename ValueType::Member>(memberCount);
        stack_.template Top<ValueType>()->SetObjectRaw(members, memberCount, GetAllocator());
        return true;
    }

    bool StartArray()
    {
        new (stack_.template Push<ValueType>()) ValueType(kArrayType);
        return true;
    }

    bool EndArray(SizeType elementCount)
    {
        ValueType* elements = stack_.template Pop<ValueType>(elementCount);
        stack_.template Top<ValueType>()->SetArrayRaw(elements, elementCount, GetAllocator());
        return true;
    }

  private:
    //! Default memory allocation stack capacity.
    static const size_t kDefaultStackCapacity = 1024;

    //! Prohibit swapping with a bare ValueType
    struct ClearStackOnExit
    {
        explicit ClearStackOnExit(GenericDocument& d)
            : d_(d)
        {
        }

        ~ClearStackOnExit()
        {
            d_.ClearStack();
        }

      private:
        ClearStackOnExit(const ClearStackOnExit&);
        ClearStackOnExit& operator=(const ClearStackOnExit&);
        GenericDocument& d_;
    };

    void ClearStack()
    {
        if (Allocator::kNeedFree)
            while (stack_.GetSize() > 0) // Here assumes all elements in stack array are
                                         // GenericValue (Member is actually 2 GenericValue)
                (stack_.template Pop<ValueType>(1))->~ValueType();
        else
            stack_.Clear();
        stack_.ShrinkToFit();
    }

    void Destroy()
    {
        RAPIDYYJSON_DELETE(ownAllocator_);
    }

    Allocator* allocator_;
    Allocator* ownAllocator_;
    internal::Stack<StackAllocator> stack_;
    ParseResult parseResult_;
};

//! GenericDocument with UTF8 encoding
typedef GenericDocument<UTF8<>> Document;

RAPIDYYJSON_NAMESPACE_END

#endif // RAPIDYYJSON_DOCUMENT_H_
