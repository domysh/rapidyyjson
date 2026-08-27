/*
 * rapidyyjson - a RapidJSON-compatible API implemented on top of yyjson.
 *
 * Common definitions and configuration macros.
 *
 * The public interface mirrors RapidJSON's `rapidjson/rapidjson.h`, with the
 * RAPIDJSON_ prefix replaced by RAPIDYYJSON_ and the `rapidjson` namespace
 * replaced by `rapidyyjson`.
 */

#ifndef RAPIDYYJSON_RAPIDYYJSON_H_
#define RAPIDYYJSON_RAPIDYYJSON_H_

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

///////////////////////////////////////////////////////////////////////////////
// RAPIDYYJSON_VERSION_STRING
//
// The API level tracked here is the one of RapidJSON 1.1.0, the release whose
// public interface this library reproduces.

#define RAPIDYYJSON_MAJOR_VERSION 1
#define RAPIDYYJSON_MINOR_VERSION 1
#define RAPIDYYJSON_PATCH_VERSION 0
#define RAPIDYYJSON_VERSION_STRING "1.1.0"

///////////////////////////////////////////////////////////////////////////////
// RAPIDYYJSON_NAMESPACE_(BEGIN|END)

#ifndef RAPIDYYJSON_NAMESPACE
#define RAPIDYYJSON_NAMESPACE rapidyyjson
#endif
#ifndef RAPIDYYJSON_NAMESPACE_BEGIN
#define RAPIDYYJSON_NAMESPACE_BEGIN namespace RAPIDYYJSON_NAMESPACE {
#endif
#ifndef RAPIDYYJSON_NAMESPACE_END
#define RAPIDYYJSON_NAMESPACE_END }
#endif

///////////////////////////////////////////////////////////////////////////////
// Feature switches

#ifndef RAPIDYYJSON_HAS_STDSTRING
#define RAPIDYYJSON_HAS_STDSTRING 1
#endif
#if RAPIDYYJSON_HAS_STDSTRING
#include <string>
#endif

#ifndef RAPIDYYJSON_HAS_CXX11_RVALUE_REFS
#define RAPIDYYJSON_HAS_CXX11_RVALUE_REFS 1
#endif
#ifndef RAPIDYYJSON_HAS_CXX11_NOEXCEPT
#define RAPIDYYJSON_HAS_CXX11_NOEXCEPT 1
#endif
#ifndef RAPIDYYJSON_HAS_CXX11_TYPETRAITS
#define RAPIDYYJSON_HAS_CXX11_TYPETRAITS 1
#endif
#ifndef RAPIDYYJSON_HAS_CXX11_RANGE_FOR
#define RAPIDYYJSON_HAS_CXX11_RANGE_FOR 1
#endif

///////////////////////////////////////////////////////////////////////////////
// Endianness

#define RAPIDYYJSON_LITTLEENDIAN 0
#define RAPIDYYJSON_BIGENDIAN 1

#ifndef RAPIDYYJSON_ENDIAN
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__)
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define RAPIDYYJSON_ENDIAN RAPIDYYJSON_BIGENDIAN
#else
#define RAPIDYYJSON_ENDIAN RAPIDYYJSON_LITTLEENDIAN
#endif
#elif defined(_MSC_VER)
#define RAPIDYYJSON_ENDIAN RAPIDYYJSON_LITTLEENDIAN
#else
#define RAPIDYYJSON_ENDIAN RAPIDYYJSON_LITTLEENDIAN
#endif
#endif // RAPIDYYJSON_ENDIAN

///////////////////////////////////////////////////////////////////////////////
// RAPIDYYJSON_64BIT

//! Whether the target is a 64-bit architecture.
#ifndef RAPIDYYJSON_64BIT
#if defined(__LP64__) || (defined(__x86_64__) && defined(__ILP32__)) || defined(_WIN64) ||          \
    defined(__EMSCRIPTEN__)
#define RAPIDYYJSON_64BIT 1
#else
#define RAPIDYYJSON_64BIT 0
#endif
#endif // RAPIDYYJSON_64BIT

///////////////////////////////////////////////////////////////////////////////
// Alignment / integer helpers

//! Data alignment of the machine: 4 bytes on 32-bit platforms, 8 on 64-bit ones.
#ifndef RAPIDYYJSON_ALIGN
#if RAPIDYYJSON_64BIT == 1
#define RAPIDYYJSON_ALIGN(x) (((x) + static_cast<uint64_t>(7u)) & ~static_cast<uint64_t>(7u))
#else
#define RAPIDYYJSON_ALIGN(x) (((x) + 3u) & ~3u)
#endif
#endif

#ifndef RAPIDYYJSON_UINT64_C2
#define RAPIDYYJSON_UINT64_C2(high32, low32)                                                       \
    ((static_cast<uint64_t>(high32) << 32) | static_cast<uint64_t>(low32))
#endif

///////////////////////////////////////////////////////////////////////////////
// Compiler attributes

#ifndef RAPIDYYJSON_FORCEINLINE
#if defined(_MSC_VER)
#define RAPIDYYJSON_FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define RAPIDYYJSON_FORCEINLINE __attribute__((always_inline)) inline
#else
#define RAPIDYYJSON_FORCEINLINE inline
#endif
#endif

#ifndef RAPIDYYJSON_LIKELY
#if defined(__GNUC__) || defined(__clang__)
#define RAPIDYYJSON_LIKELY(x) __builtin_expect(!!(x), 1)
#else
#define RAPIDYYJSON_LIKELY(x) (x)
#endif
#endif

#ifndef RAPIDYYJSON_UNLIKELY
#if defined(__GNUC__) || defined(__clang__)
#define RAPIDYYJSON_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define RAPIDYYJSON_UNLIKELY(x) (x)
#endif
#endif

#ifndef RAPIDYYJSON_NOEXCEPT
#define RAPIDYYJSON_NOEXCEPT noexcept
#endif

///////////////////////////////////////////////////////////////////////////////
// Diagnostics control

#if defined(__clang__)
#define RAPIDYYJSON_PRAGMA(x) _Pragma(#x)
#define RAPIDYYJSON_DIAG_PRAGMA(x) RAPIDYYJSON_PRAGMA(clang diagnostic x)
#define RAPIDYYJSON_DIAG_OFF(x) RAPIDYYJSON_DIAG_PRAGMA(ignored RAPIDYYJSON_STRINGIFY(-W##x))
#define RAPIDYYJSON_DIAG_PUSH RAPIDYYJSON_DIAG_PRAGMA(push)
#define RAPIDYYJSON_DIAG_POP RAPIDYYJSON_DIAG_PRAGMA(pop)
#elif defined(__GNUC__)
#define RAPIDYYJSON_PRAGMA(x) _Pragma(#x)
#define RAPIDYYJSON_DIAG_PRAGMA(x) RAPIDYYJSON_PRAGMA(GCC diagnostic x)
#define RAPIDYYJSON_DIAG_OFF(x) RAPIDYYJSON_DIAG_PRAGMA(ignored RAPIDYYJSON_STRINGIFY(-W##x))
#define RAPIDYYJSON_DIAG_PUSH RAPIDYYJSON_DIAG_PRAGMA(push)
#define RAPIDYYJSON_DIAG_POP RAPIDYYJSON_DIAG_PRAGMA(pop)
#else
#define RAPIDYYJSON_DIAG_OFF(x)
#define RAPIDYYJSON_DIAG_PUSH
#define RAPIDYYJSON_DIAG_POP
#endif

#define RAPIDYYJSON_STRINGIFY_HELPER(x) #x
#define RAPIDYYJSON_STRINGIFY(x) RAPIDYYJSON_STRINGIFY_HELPER(x)

///////////////////////////////////////////////////////////////////////////////
// Assertions

#ifndef RAPIDYYJSON_ASSERT
#define RAPIDYYJSON_ASSERT(x) assert(x)
#endif

#ifndef RAPIDYYJSON_ASSERT_THROWS
#define RAPIDYYJSON_NOEXCEPT_ASSERT(x) RAPIDYYJSON_ASSERT(x)
#else
#define RAPIDYYJSON_NOEXCEPT_ASSERT(x)
#endif

#ifndef RAPIDYYJSON_STATIC_ASSERT
#define RAPIDYYJSON_STATIC_ASSERT(x) static_assert(x, RAPIDYYJSON_STRINGIFY(x))
#endif

///////////////////////////////////////////////////////////////////////////////
// Default parse/write flags

#ifndef RAPIDYYJSON_PARSE_DEFAULT_FLAGS
#define RAPIDYYJSON_PARSE_DEFAULT_FLAGS kParseNoFlags
#endif

#ifndef RAPIDYYJSON_WRITE_DEFAULT_FLAGS
#define RAPIDYYJSON_WRITE_DEFAULT_FLAGS kWriteNoFlags
#endif

///////////////////////////////////////////////////////////////////////////////
// new / delete

#ifndef RAPIDYYJSON_NEW
#define RAPIDYYJSON_NEW(TypeName) new TypeName
#endif
#ifndef RAPIDYYJSON_DELETE
#define RAPIDYYJSON_DELETE(x) delete x
#endif

RAPIDYYJSON_NAMESPACE_BEGIN

///////////////////////////////////////////////////////////////////////////////
// SizeType

//! Size type (unsigned integer) used for string lengths, array sizes, ...
typedef unsigned SizeType;

///////////////////////////////////////////////////////////////////////////////
// Type

//! Type of JSON value
enum Type
{
    kNullType = 0,   //!< null
    kFalseType = 1,  //!< false
    kTrueType = 2,   //!< true
    kObjectType = 3, //!< object
    kArrayType = 4,  //!< array
    kStringType = 5, //!< string
    kNumberType = 6  //!< number
};

RAPIDYYJSON_NAMESPACE_END

#endif // RAPIDYYJSON_RAPIDYYJSON_H_
