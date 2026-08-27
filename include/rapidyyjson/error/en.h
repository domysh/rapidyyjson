/*
 * rapidyyjson - a RapidJSON-compatible API implemented on top of yyjson.
 *
 * English error messages, mirroring `rapidjson/error/en.h`.
 */

#ifndef RAPIDYYJSON_ERROR_EN_H_
#define RAPIDYYJSON_ERROR_EN_H_

#include "error.h"

RAPIDYYJSON_NAMESPACE_BEGIN

//! Maps error code of parsing into error message.
/*!
    \param parseErrorCode Error code obtained in parsing.
    \return the error message.
    \note User can make a copy of this function for localization.
        Using switch-case is safer for future modification of error codes.
*/
inline const RAPIDYYJSON_ERROR_CHARTYPE*
GetParseError_En(ParseErrorCode parseErrorCode)
{
    switch (parseErrorCode)
    {
    case kParseErrorNone:
        return RAPIDYYJSON_ERROR_STRING("No error.");

    case kParseErrorDocumentEmpty:
        return RAPIDYYJSON_ERROR_STRING("The document is empty.");
    case kParseErrorDocumentRootNotSingular:
        return RAPIDYYJSON_ERROR_STRING("The document root must not be followed by other values.");

    case kParseErrorValueInvalid:
        return RAPIDYYJSON_ERROR_STRING("Invalid value.");

    case kParseErrorObjectMissName:
        return RAPIDYYJSON_ERROR_STRING("Missing a name for object member.");
    case kParseErrorObjectMissColon:
        return RAPIDYYJSON_ERROR_STRING("Missing a colon after a name of object member.");
    case kParseErrorObjectMissCommaOrCurlyBracket:
        return RAPIDYYJSON_ERROR_STRING("Missing a comma or '}' after an object member.");

    case kParseErrorArrayMissCommaOrSquareBracket:
        return RAPIDYYJSON_ERROR_STRING("Missing a comma or ']' after an array element.");

    case kParseErrorStringUnicodeEscapeInvalidHex:
        return RAPIDYYJSON_ERROR_STRING("Incorrect hex digit after \\u escape in string.");
    case kParseErrorStringUnicodeSurrogateInvalid:
        return RAPIDYYJSON_ERROR_STRING("The surrogate pair in string is invalid.");
    case kParseErrorStringEscapeInvalid:
        return RAPIDYYJSON_ERROR_STRING("Invalid escape character in string.");
    case kParseErrorStringMissQuotationMark:
        return RAPIDYYJSON_ERROR_STRING("Missing a closing quotation mark in string.");
    case kParseErrorStringInvalidEncoding:
        return RAPIDYYJSON_ERROR_STRING("Invalid encoding in string.");

    case kParseErrorNumberTooBig:
        return RAPIDYYJSON_ERROR_STRING(
            "Number too big to be stored in double.");
    case kParseErrorNumberMissFraction:
        return RAPIDYYJSON_ERROR_STRING("Miss fraction part in number.");
    case kParseErrorNumberMissExponent:
        return RAPIDYYJSON_ERROR_STRING("Miss exponent in number.");

    case kParseErrorTermination:
        return RAPIDYYJSON_ERROR_STRING("Terminate parsing due to Handler error.");
    case kParseErrorUnspecificSyntaxError:
        return RAPIDYYJSON_ERROR_STRING("Unspecific syntax error.");

    default:
        return RAPIDYYJSON_ERROR_STRING("Unknown error.");
    }
}

RAPIDYYJSON_NAMESPACE_END

#endif // RAPIDYYJSON_ERROR_EN_H_
