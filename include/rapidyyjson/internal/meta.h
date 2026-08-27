/*
 * rapidyyjson - a RapidJSON-compatible API implemented on top of yyjson.
 *
 * Template metaprogramming helpers, mirroring `rapidjson/internal/meta.h`.
 */

#ifndef RAPIDYYJSON_INTERNAL_META_H_
#define RAPIDYYJSON_INTERNAL_META_H_

#include "../rapidyyjson.h"

#include <type_traits>

RAPIDYYJSON_NAMESPACE_BEGIN
namespace internal
{

// Helper to wrap/convert arbitrary types to void, useful for arbitrary type matching
template <typename T>
struct Void
{
    typedef void Type;
};

///////////////////////////////////////////////////////////////////////////////
// BoolType, TrueType, FalseType

template <bool Cond>
struct BoolType
{
    static const bool Value = Cond;
    typedef BoolType Type;
};

typedef BoolType<true> TrueType;
typedef BoolType<false> FalseType;

///////////////////////////////////////////////////////////////////////////////
// SelectIf, BoolExpr, NotExpr, AndExpr, OrExpr

template <bool C>
struct SelectIfImpl
{
    template <typename T1, typename T2>
    struct Apply
    {
        typedef T1 Type;
    };
};

template <>
struct SelectIfImpl<false>
{
    template <typename T1, typename T2>
    struct Apply
    {
        typedef T2 Type;
    };
};

template <typename C, typename T1, typename T2>
struct SelectIfCond : SelectIfImpl<C::Value>::template Apply<T1, T2>
{
};

template <typename C, typename T1, typename T2>
struct SelectIf : SelectIfCond<C, T1, T2>
{
};

template <bool Cond1, bool Cond2>
struct AndExprCond : FalseType
{
};

template <>
struct AndExprCond<true, true> : TrueType
{
};

template <bool Cond1, bool Cond2>
struct OrExprCond : TrueType
{
};

template <>
struct OrExprCond<false, false> : FalseType
{
};

template <typename C>
struct BoolExpr : SelectIf<C, TrueType, FalseType>::Type
{
};

template <typename C>
struct NotExpr : SelectIf<C, FalseType, TrueType>::Type
{
};

template <typename C1, typename C2>
struct AndExpr : AndExprCond<C1::Value, C2::Value>::Type
{
};

template <typename C1, typename C2>
struct OrExpr : OrExprCond<C1::Value, C2::Value>::Type
{
};

///////////////////////////////////////////////////////////////////////////////
// AddConst, MaybeAddConst, RemoveConst

template <typename T>
struct AddConst
{
    typedef const T Type;
};

template <bool Constify, typename T>
struct MaybeAddConst : SelectIfCond<BoolType<Constify>, const T, T>
{
};

template <typename T>
struct RemoveConst
{
    typedef T Type;
};

template <typename T>
struct RemoveConst<const T>
{
    typedef T Type;
};

///////////////////////////////////////////////////////////////////////////////
// IsSame, IsConst, IsMoreConst, IsPointer

template <typename T, typename U>
struct IsSame : FalseType
{
};

template <typename T>
struct IsSame<T, T> : TrueType
{
};

template <typename T>
struct IsConst : FalseType
{
};

template <typename T>
struct IsConst<const T> : TrueType
{
};

template <typename CT, typename T>
struct IsMoreConst
    : AndExpr<IsSame<typename RemoveConst<CT>::Type, typename RemoveConst<T>::Type>,
              BoolType<IsConst<CT>::Value >= IsConst<T>::Value>>::Type
{
};

template <typename T>
struct IsPointer : FalseType
{
};

template <typename T>
struct IsPointer<T*> : TrueType
{
};

///////////////////////////////////////////////////////////////////////////////
// IsBaseOf

template <typename B, typename D>
struct IsBaseOf : BoolType<std::is_base_of<B, D>::value>::Type
{
};

///////////////////////////////////////////////////////////////////////////////
// EnableIf / DisableIf

template <bool Condition, typename T = void>
struct EnableIfCond
{
    typedef T Type;
};

template <typename T>
struct EnableIfCond<false, T>
{
    /* empty */
};

template <bool Condition, typename T = void>
struct DisableIfCond
{
    typedef T Type;
};

template <typename T>
struct DisableIfCond<true, T>
{
    /* empty */
};

template <typename Condition, typename T = void>
struct EnableIf : EnableIfCond<Condition::Value, T>
{
};

template <typename Condition, typename T = void>
struct DisableIf : DisableIfCond<Condition::Value, T>
{
};

// SFINAE helpers
struct SfinaeTag
{
};

template <typename T>
struct RemoveSfinaeTag;

template <typename T>
struct RemoveSfinaeTag<SfinaeTag& (*)(T)>
{
    typedef T Type;
};

#define RAPIDYYJSON_REMOVEFPTR_(type)                                                              \
    typename ::RAPIDYYJSON_NAMESPACE::internal::RemoveSfinaeTag<                                   \
        ::RAPIDYYJSON_NAMESPACE::internal::SfinaeTag& (*)type>::Type

#define RAPIDYYJSON_ENABLEIF(cond)                                                                 \
    typename ::RAPIDYYJSON_NAMESPACE::internal::EnableIf<RAPIDYYJSON_REMOVEFPTR_(cond)>::Type* =   \
        NULL

#define RAPIDYYJSON_DISABLEIF(cond)                                                                \
    typename ::RAPIDYYJSON_NAMESPACE::internal::DisableIf<RAPIDYYJSON_REMOVEFPTR_(cond)>::Type* =  \
        NULL

#define RAPIDYYJSON_ENABLEIF_RETURN(cond, returntype)                                              \
    typename ::RAPIDYYJSON_NAMESPACE::internal::EnableIf<RAPIDYYJSON_REMOVEFPTR_(cond),            \
                                                         RAPIDYYJSON_REMOVEFPTR_(returntype)>::Type

#define RAPIDYYJSON_DISABLEIF_RETURN(cond, returntype)                                             \
    typename ::RAPIDYYJSON_NAMESPACE::internal::DisableIf<                                         \
        RAPIDYYJSON_REMOVEFPTR_(cond),                                                             \
        RAPIDYYJSON_REMOVEFPTR_(returntype)>::Type

} // namespace internal
RAPIDYYJSON_NAMESPACE_END

#endif // RAPIDYYJSON_INTERNAL_META_H_
