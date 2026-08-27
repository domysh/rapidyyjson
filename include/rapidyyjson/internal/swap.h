/*
 * rapidyyjson - a RapidJSON-compatible API implemented on top of yyjson.
 *
 * Mirrors `rapidjson/internal/swap.h`.
 */

#ifndef RAPIDYYJSON_INTERNAL_SWAP_H_
#define RAPIDYYJSON_INTERNAL_SWAP_H_

#include "../rapidyyjson.h"

RAPIDYYJSON_NAMESPACE_BEGIN
namespace internal
{

//! Custom swap() to avoid dependency on C++ <algorithm> header
template <typename T>
inline void
Swap(T& a, T& b) RAPIDYYJSON_NOEXCEPT
{
    T tmp = a;
    a = b;
    b = tmp;
}

} // namespace internal
RAPIDYYJSON_NAMESPACE_END

#endif // RAPIDYYJSON_INTERNAL_SWAP_H_
