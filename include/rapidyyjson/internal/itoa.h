/*
 * rapidyyjson - a RapidJSON-compatible API implemented on top of yyjson.
 *
 * Integer-to-string conversion, mirroring `rapidjson/internal/itoa.h`.
 */

#ifndef RAPIDYYJSON_INTERNAL_ITOA_H_
#define RAPIDYYJSON_INTERNAL_ITOA_H_

#include "../rapidyyjson.h"

RAPIDYYJSON_NAMESPACE_BEGIN
namespace internal
{

inline const char*
GetDigitsLut()
{
    static const char cDigitsLut[200] = {
        '0', '0', '0', '1', '0', '2', '0', '3', '0', '4', '0', '5', '0', '6', '0', '7', '0', '8',
        '0', '9', '1', '0', '1', '1', '1', '2', '1', '3', '1', '4', '1', '5', '1', '6', '1', '7',
        '1', '8', '1', '9', '2', '0', '2', '1', '2', '2', '2', '3', '2', '4', '2', '5', '2', '6',
        '2', '7', '2', '8', '2', '9', '3', '0', '3', '1', '3', '2', '3', '3', '3', '4', '3', '5',
        '3', '6', '3', '7', '3', '8', '3', '9', '4', '0', '4', '1', '4', '2', '4', '3', '4', '4',
        '4', '5', '4', '6', '4', '7', '4', '8', '4', '9', '5', '0', '5', '1', '5', '2', '5', '3',
        '5', '4', '5', '5', '5', '6', '5', '7', '5', '8', '5', '9', '6', '0', '6', '1', '6', '2',
        '6', '3', '6', '4', '6', '5', '6', '6', '6', '7', '6', '8', '6', '9', '7', '0', '7', '1',
        '7', '2', '7', '3', '7', '4', '7', '5', '7', '6', '7', '7', '7', '8', '7', '9', '8', '0',
        '8', '1', '8', '2', '8', '3', '8', '4', '8', '5', '8', '6', '8', '7', '8', '8', '8', '9',
        '9', '0', '9', '1', '9', '2', '9', '3', '9', '4', '9', '5', '9', '6', '9', '7', '9', '8',
        '9', '9'};
    return cDigitsLut;
}

inline char*
u32toa(uint32_t value, char* buffer)
{
    RAPIDYYJSON_ASSERT(buffer != 0);

    char temp[10];
    char* p = temp;
    do
    {
        *p++ = static_cast<char>('0' + static_cast<char>(value % 10));
        value /= 10;
    } while (value > 0);

    while (p != temp)
        *buffer++ = *--p;

    return buffer;
}

inline char*
i32toa(int32_t value, char* buffer)
{
    RAPIDYYJSON_ASSERT(buffer != 0);
    uint32_t u = static_cast<uint32_t>(value);
    if (value < 0)
    {
        *buffer++ = '-';
        u = ~u + 1;
    }
    return u32toa(u, buffer);
}

inline char*
u64toa(uint64_t value, char* buffer)
{
    RAPIDYYJSON_ASSERT(buffer != 0);

    char temp[20];
    char* p = temp;
    do
    {
        *p++ = static_cast<char>('0' + static_cast<char>(value % 10));
        value /= 10;
    } while (value > 0);

    while (p != temp)
        *buffer++ = *--p;

    return buffer;
}

inline char*
i64toa(int64_t value, char* buffer)
{
    RAPIDYYJSON_ASSERT(buffer != 0);
    uint64_t u = static_cast<uint64_t>(value);
    if (value < 0)
    {
        *buffer++ = '-';
        u = ~u + 1;
    }
    return u64toa(u, buffer);
}

} // namespace internal
RAPIDYYJSON_NAMESPACE_END

#endif // RAPIDYYJSON_INTERNAL_ITOA_H_
