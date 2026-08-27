/*
 * rapidyyjson - a RapidJSON-compatible API implemented on top of yyjson.
 *
 * Parse error codes and ParseResult, mirroring `rapidjson/error/error.h`.
 */

#ifndef RAPIDYYJSON_ERROR_ERROR_H_
#define RAPIDYYJSON_ERROR_ERROR_H_

#include "../rapidyyjson.h"

///////////////////////////////////////////////////////////////////////////////
// RAPIDYYJSON_ERROR_CHARTYPE

//! Character type of error messages.
#ifndef RAPIDYYJSON_ERROR_CHARTYPE
#define RAPIDYYJSON_ERROR_CHARTYPE char
#endif

///////////////////////////////////////////////////////////////////////////////
// RAPIDYYJSON_ERROR_STRING

//! Macro for converting string literal to \ref RAPIDYYJSON_ERROR_CHARTYPE[].
#ifndef RAPIDYYJSON_ERROR_STRING
#define RAPIDYYJSON_ERROR_STRING(x) x
#endif

RAPIDYYJSON_NAMESPACE_BEGIN

///////////////////////////////////////////////////////////////////////////////
// ParseErrorCode

//! Error code of parsing.
enum ParseErrorCode
{
    kParseErrorNone = 0, //!< No error.

    kParseErrorDocumentEmpty,            //!< The document is empty.
    kParseErrorDocumentRootNotSingular,  //!< The document root must not follow by other values.

    kParseErrorValueInvalid, //!< Invalid value.

    kParseErrorObjectMissName,               //!< Missing a name for object member.
    kParseErrorObjectMissColon,              //!< Missing a colon after a name of object member.
    kParseErrorObjectMissCommaOrCurlyBracket,//!< Missing a comma or '}' after an object member.

    kParseErrorArrayMissCommaOrSquareBracket, //!< Missing a comma or ']' after an array element.

    kParseErrorStringUnicodeEscapeInvalidHex, //!< Incorrect hex digit after \\u escape in string.
    kParseErrorStringUnicodeSurrogateInvalid, //!< The surrogate pair in string is invalid.
    kParseErrorStringEscapeInvalid,           //!< Invalid escape character in string.
    kParseErrorStringMissQuotationMark,       //!< Missing a closing quotation mark in string.
    kParseErrorStringInvalidEncoding,         //!< Invalid encoding in string.

    kParseErrorNumberTooBig,       //!< Number too big to be stored in double.
    kParseErrorNumberMissFraction, //!< Miss fraction part in number.
    kParseErrorNumberMissExponent, //!< Miss exponent in number.

    kParseErrorTermination,           //!< Parsing was terminated.
    kParseErrorUnspecificSyntaxError  //!< Unspecific syntax error.
};

//! Result of parsing (wraps ParseErrorCode)
/*!
    \code
        Document doc;
        ParseResult ok = doc.Parse("[42]");
        if (!ok) {
            fprintf(stderr, "JSON parse error: %s (%u)",
                    GetParseError_En(ok.Code()), ok.Offset());
            exit(EXIT_FAILURE);
        }
    \endcode
*/
struct ParseResult
{
    //!! Unspecified boolean type
    typedef bool (ParseResult::*BooleanType)() const;

  public:
    //! Default constructor, no error.
    ParseResult()
        : code_(kParseErrorNone),
          offset_(0)
    {
    }

    //! Constructor to set an error.
    ParseResult(ParseErrorCode code, size_t offset)
        : code_(code),
          offset_(offset)
    {
    }

    //! Get the error code.
    ParseErrorCode Code() const
    {
        return code_;
    }

    //! Get the error offset, if \ref IsError(), 0 otherwise.
    size_t Offset() const
    {
        return offset_;
    }

    //! Explicit conversion to \c bool, returns \c true, iff !\ref IsError().
    operator BooleanType() const
    {
        return !IsError() ? &ParseResult::IsError : NULL;
    }

    //! Whether the result is an error.
    bool IsError() const
    {
        return code_ != kParseErrorNone;
    }

    bool operator==(const ParseResult& that) const
    {
        return code_ == that.code_;
    }

    bool operator==(ParseErrorCode code) const
    {
        return code_ == code;
    }

    friend bool operator==(ParseErrorCode code, const ParseResult& err)
    {
        return code == err.code_;
    }

    bool operator!=(const ParseResult& that) const
    {
        return !(*this == that);
    }

    bool operator!=(ParseErrorCode code) const
    {
        return !(*this == code);
    }

    friend bool operator!=(ParseErrorCode code, const ParseResult& err)
    {
        return err != code;
    }

    //! Reset error code.
    void Clear()
    {
        Set(kParseErrorNone);
    }

    //! Update error code and offset.
    void Set(ParseErrorCode code, size_t offset = 0)
    {
        code_ = code;
        offset_ = offset;
    }

  private:
    ParseErrorCode code_;
    size_t offset_;
};

//! Function pointer type of GetParseError().
/*! This is the prototype for \c GetParseError_X(), where \c X is a locale.
    User can dynamically change locale in runtime, e.g.:
    \code
        GetParseErrorFunc GetParseError = GetParseError_En; // or whatever
        const RAPIDYYJSON_ERROR_CHARTYPE* s = GetParseError(document.GetParseErrorCode());
    \endcode
*/
typedef const RAPIDYYJSON_ERROR_CHARTYPE* (*GetParseErrorFunc)(ParseErrorCode);

RAPIDYYJSON_NAMESPACE_END

#endif // RAPIDYYJSON_ERROR_ERROR_H_
