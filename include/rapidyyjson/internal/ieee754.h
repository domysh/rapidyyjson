/*
 * rapidyyjson - a RapidJSON-compatible API implemented on top of yyjson.
 *
 * Inspection of the IEEE-754 binary64 bit layout, mirroring
 * `rapidjson/internal/ieee754.h`.
 *
 * `Double` is a thin view over the 64 bits of a `double`: it converts between
 * the value and its bit pattern and answers the classification questions the
 * number parsing and formatting code asks.
 */

#ifndef RAPIDYYJSON_INTERNAL_IEEE754_H_
#define RAPIDYYJSON_INTERNAL_IEEE754_H_

#include "../rapidyyjson.h"

RAPIDYYJSON_NAMESPACE_BEGIN
namespace internal
{

//! A double and its IEEE-754 binary64 bit pattern, viewed as the same object.
class Double
{
  public:
    Double()
        : d_(0.0)
    {
    }

    Double(double d)
        : d_(d)
    {
    }

    Double(uint64_t u)
        : u_(u)
    {
    }

    //! The value.
    double Value() const
    {
        return d_;
    }

    //! The 64 bits underneath the value.
    uint64_t Uint64Value() const
    {
        return u_;
    }

    //! The next representable double away from zero. Only valid for +0 and above.
    double NextPositiveDouble() const
    {
        RAPIDYYJSON_ASSERT(!Sign());
        return Double(static_cast<uint64_t>(u_ + 1)).Value();
    }

    //! Sign bit: true when the value is negative (-0.0 included).
    bool Sign() const
    {
        return (u_ & kSignMask) != 0;
    }

    //! The stored significand, without the implicit leading bit.
    uint64_t Significand() const
    {
        return u_ & kSignificandMask;
    }

    //! The unbiased exponent.
    int Exponent() const
    {
        return static_cast<int>(((u_ & kExponentMask) >> kSignificandSize) - kExponentBias);
    }

    bool IsNan() const
    {
        return (u_ & kExponentMask) == kExponentMask && Significand() != 0;
    }

    bool IsInf() const
    {
        return (u_ & kExponentMask) == kExponentMask && Significand() == 0;
    }

    bool IsNanOrInf() const
    {
        return (u_ & kExponentMask) == kExponentMask;
    }

    //! False only for a subnormal, i.e. a zero exponent field with a non-zero significand.
    bool IsNormal() const
    {
        return (u_ & kExponentMask) != 0 || Significand() == 0;
    }

    bool IsZero() const
    {
        return (u_ & (kExponentMask | kSignificandMask)) == 0;
    }

    //! The significand with the implicit leading bit restored, where there is one.
    uint64_t IntegerSignificand() const
    {
        return IsNormal() ? Significand() | kHiddenBit : Significand();
    }

    //! The exponent that pairs with IntegerSignificand() to give the value.
    int IntegerExponent() const
    {
        return (IsNormal() ? Exponent() : kDenormalExponent) - kSignificandSize;
    }

    //! The bit pattern remapped so that unsigned comparison orders doubles by value.
    uint64_t ToBias() const
    {
        return (u_ & kSignMask) ? ~u_ + 1 : u_ | kSignMask;
    }

    //! How many significand bits survive at the given binary order of magnitude.
    static unsigned EffectiveSignificandSize(int order)
    {
        if (order >= -1021)
            return 53;
        else if (order <= -1074)
            return 0;
        else
            return static_cast<unsigned>(order) + 1074;
    }

  private:
    static const int kSignificandSize = 52;
    static const int kExponentBias = 0x3FF;
    static const int kDenormalExponent = 1 - kExponentBias;
    static const uint64_t kSignMask = RAPIDYYJSON_UINT64_C2(0x80000000, 0x00000000);
    static const uint64_t kExponentMask = RAPIDYYJSON_UINT64_C2(0x7FF00000, 0x00000000);
    static const uint64_t kSignificandMask = RAPIDYYJSON_UINT64_C2(0x000FFFFF, 0xFFFFFFFF);
    static const uint64_t kHiddenBit = RAPIDYYJSON_UINT64_C2(0x00100000, 0x00000000);

    union
    {
        double d_;
        uint64_t u_;
    };
};

} // namespace internal
RAPIDYYJSON_NAMESPACE_END

#endif // RAPIDYYJSON_INTERNAL_IEEE754_H_
