/*
 * rapidyyjson - a RapidJSON-compatible API implemented on top of yyjson.
 *
 * Double-to-string conversion, mirroring the output format of
 * `rapidjson/internal/dtoa.h`.
 *
 * The shortest round-trippable decimal representation is obtained by an
 * increasing-precision search with the C library's printf/strtod pair, then
 * formatted with RapidJSON's Prettify() layout rules so that the emitted text
 * is identical to RapidJSON's.
 */

#ifndef RAPIDYYJSON_INTERNAL_DTOA_H_
#define RAPIDYYJSON_INTERNAL_DTOA_H_

#include "../rapidyyjson.h"
#include "itoa.h"

#include <cmath>
#include <cstdio>

RAPIDYYJSON_NAMESPACE_BEGIN
namespace internal
{

//! Writes the shortest decimal digit sequence of \c value (which must be finite and > 0).
/*!
    \param value    the value to convert.
    \param digits   receives the significant decimal digits, without any decimal point.
    \param length   receives the number of significant digits written.
    \param K        receives the decimal exponent such that
                    value == 0.d[0]d[1]..d[length-1] * 10^(length + K).
*/
inline void
ShortestDigits(double value, char* digits, int* length, int* K)
{
    char buf[40];
    int prec = 0;
    for (; prec < 17; ++prec)
    {
        std::snprintf(buf, sizeof(buf), "%.*e", prec, value);
        if (std::strtod(buf, 0) == value)
            break;
    }
    if (prec >= 17)
        std::snprintf(buf, sizeof(buf), "%.17e", value);

    // buf is of the form "d.dddddde[+-]dd"; extract mantissa digits and exponent.
    int n = 0;
    const char* p = buf;
    for (; *p && *p != 'e' && *p != 'E'; ++p)
        if (*p != '.')
            digits[n++] = *p;

    int exp10 = 0;
    if (*p == 'e' || *p == 'E')
        exp10 = static_cast<int>(std::strtol(p + 1, 0, 10));

    // Strip trailing zeros, keeping at least one digit.
    while (n > 1 && digits[n - 1] == '0')
        --n;

    *length = n;
    *K = exp10 + 1 - n; // so that length + K == exp10 + 1 == kk
}

inline char*
WriteExponent(int K, char* buffer)
{
    if (K < 0)
    {
        *buffer++ = '-';
        K = -K;
    }

    if (K >= 100)
    {
        *buffer++ = static_cast<char>('0' + static_cast<char>(K / 100));
        K %= 100;
        const char* d = GetDigitsLut() + K * 2;
        *buffer++ = d[0];
        *buffer++ = d[1];
    }
    else if (K >= 10)
    {
        const char* d = GetDigitsLut() + K * 2;
        *buffer++ = d[0];
        *buffer++ = d[1];
    }
    else
        *buffer++ = static_cast<char>('0' + static_cast<char>(K));

    return buffer;
}

inline char*
Prettify(char* buffer, int length, int k, int maxDecimalPlaces)
{
    const int kk = length + k; // 10^(kk-1) <= v < 10^kk

    if (0 <= k && kk <= 21)
    {
        // 1234e7 -> 12340000000
        for (int i = length; i < kk; i++)
            buffer[i] = '0';
        buffer[kk] = '.';
        buffer[kk + 1] = '0';
        return &buffer[kk + 2];
    }
    else if (0 < kk && kk <= 21)
    {
        // 1234e-2 -> 12.34
        std::memmove(&buffer[kk + 1], &buffer[kk], static_cast<size_t>(length - kk));
        buffer[kk] = '.';
        if (0 > k + maxDecimalPlaces)
        {
            // When maxDecimalPlaces = 2, 1.2345 -> 1.23, 1.102 -> 1.1
            // Remove extra trailing zeros (at least one) after truncation.
            for (int i = kk + maxDecimalPlaces; i > kk + 1; i--)
                if (buffer[i] != '0')
                    return &buffer[i + 1];
            return &buffer[kk + 2]; // Reserve one zero
        }
        else
            return &buffer[length + 1];
    }
    else if (-6 < kk && kk <= 0)
    {
        // 1234e-6 -> 0.001234
        const int offset = 2 - kk;
        std::memmove(&buffer[offset], &buffer[0], static_cast<size_t>(length));
        buffer[0] = '0';
        buffer[1] = '.';
        for (int i = 2; i < offset; i++)
            buffer[i] = '0';
        if (length + offset > maxDecimalPlaces + 2)
        {
            // When maxDecimalPlaces = 2, 0.123 -> 0.12, 0.102 -> 0.1
            for (int i = maxDecimalPlaces + 1; i > 2; i--)
                if (buffer[i] != '0')
                    return &buffer[i + 1];
            return &buffer[3]; // Reserve one zero
        }
        else
            return &buffer[length + offset];
    }
    else if (kk < -maxDecimalPlaces)
    {
        // Truncate to zero
        buffer[0] = '0';
        buffer[1] = '.';
        buffer[2] = '0';
        return &buffer[3];
    }
    else if (length == 1)
    {
        // 1e30
        buffer[1] = 'e';
        return WriteExponent(kk - 1, &buffer[2]);
    }
    else
    {
        // 1234e30 -> 1.234e33
        std::memmove(&buffer[2], &buffer[1], static_cast<size_t>(length - 1));
        buffer[1] = '.';
        buffer[length + 1] = 'e';
        return WriteExponent(kk - 1, &buffer[length + 2]);
    }
}

//! Converts \c value to its shortest round-trippable decimal text representation.
/*! \param buffer must have room for at least 25 characters plus the terminator. */
inline char*
dtoa(double value, char* buffer, int maxDecimalPlaces = 324)
{
    RAPIDYYJSON_ASSERT(buffer != 0);
    RAPIDYYJSON_ASSERT(maxDecimalPlaces >= 1);

    if (value == 0)
    {
        if (std::signbit(value))
            *buffer++ = '-';
        buffer[0] = '0';
        buffer[1] = '.';
        buffer[2] = '0';
        return &buffer[3];
    }

    if (value < 0)
    {
        *buffer++ = '-';
        value = -value;
    }

    int length, K;
    ShortestDigits(value, buffer, &length, &K);
    return Prettify(buffer, length, K, maxDecimalPlaces);
}

} // namespace internal
RAPIDYYJSON_NAMESPACE_END

#endif // RAPIDYYJSON_INTERNAL_DTOA_H_
