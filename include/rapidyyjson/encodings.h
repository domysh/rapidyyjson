/*
 * rapidyyjson - a RapidJSON-compatible API implemented on top of yyjson.
 *
 * Character encodings, mirroring `rapidjson/encodings.h`.
 */

#ifndef RAPIDYYJSON_ENCODINGS_H_
#define RAPIDYYJSON_ENCODINGS_H_

#include "rapidyyjson.h"

RAPIDYYJSON_NAMESPACE_BEGIN

///////////////////////////////////////////////////////////////////////////////
// Encoding
/*! \class rapidyyjson::Encoding
    \brief Concept for encoding of Unicode characters.

    \code
    concept Encoding {
        typename Ch;    //! Type of character. A "character" is actually a code unit in
                        //! unicode's definition.

        enum { supportUnicode = 1 }; // or 0 if not supporting unicode

        //! Encode a Unicode codepoint to an output stream.
        template<typename OutputStream>
        static void Encode(OutputStream& os, unsigned codepoint);

        //! Decode a Unicode codepoint from an input stream.
        template <typename InputStream>
        static bool Decode(InputStream& is, unsigned* codepoint);

        //! Validate one Unicode codepoint from an encoded stream.
        template <typename InputStream, typename OutputStream>
        static bool Validate(InputStream& is, OutputStream& os);

        // The following functions are deal with byte streams.

        //! Take a character from input byte stream, skipping BOM if exist.
        template <typename InputByteStream>
        static Ch TakeBOM(InputByteStream& is);

        //! Take a character from input byte stream.
        template <typename InputByteStream>
        static Ch Take(InputByteStream& is);

        //! Put BOM to output byte stream.
        template <typename OutputByteStream>
        static void PutBOM(OutputByteStream& os);

        //! Put a character to output byte stream.
        template <typename OutputByteStream>
        static void Put(OutputByteStream& os, Ch c);
    };
    \endcode
*/

///////////////////////////////////////////////////////////////////////////////
// UTF8

//! UTF-8 encoding.
template <typename CharType = char>
struct UTF8
{
    typedef CharType Ch;

    enum
    {
        supportUnicode = 1
    };

    template <typename OutputStream>
    static void Encode(OutputStream& os, unsigned codepoint)
    {
        if (codepoint <= 0x7F)
            os.Put(static_cast<Ch>(codepoint & 0xFF));
        else if (codepoint <= 0x7FF)
        {
            os.Put(static_cast<Ch>(0xC0 | ((codepoint >> 6) & 0xFF)));
            os.Put(static_cast<Ch>(0x80 | ((codepoint & 0x3F))));
        }
        else if (codepoint <= 0xFFFF)
        {
            os.Put(static_cast<Ch>(0xE0 | ((codepoint >> 12) & 0xFF)));
            os.Put(static_cast<Ch>(0x80 | ((codepoint >> 6) & 0x3F)));
            os.Put(static_cast<Ch>(0x80 | (codepoint & 0x3F)));
        }
        else
        {
            RAPIDYYJSON_ASSERT(codepoint <= 0x10FFFF);
            os.Put(static_cast<Ch>(0xF0 | ((codepoint >> 18) & 0xFF)));
            os.Put(static_cast<Ch>(0x80 | ((codepoint >> 12) & 0x3F)));
            os.Put(static_cast<Ch>(0x80 | ((codepoint >> 6) & 0x3F)));
            os.Put(static_cast<Ch>(0x80 | (codepoint & 0x3F)));
        }
    }

    template <typename OutputStream>
    static void EncodeUnsafe(OutputStream& os, unsigned codepoint)
    {
        Encode(os, codepoint);
    }

    //! Number of trailing bytes expected for a given lead byte, or 0 when the byte cannot start
    //! a well-formed UTF-8 sequence.
    static unsigned char GetRange(unsigned char c)
    {
        if (c < 0x80)
            return 1; // single byte sequence
        if (c < 0xC2)
            return 0; // continuation byte or overlong two-byte lead
        if (c < 0xE0)
            return 2;
        if (c < 0xF0)
            return 3;
        if (c < 0xF5)
            return 4;
        return 0;
    }

    template <typename InputStream>
    static bool Decode(InputStream& is, unsigned* codepoint)
    {
        const unsigned char c0 = static_cast<unsigned char>(is.Take());
        if (!(c0 & 0x80))
        {
            *codepoint = c0;
            return true;
        }

        const unsigned char len = GetRange(c0);
        if (len < 2)
        {
            *codepoint = 0;
            return false;
        }

        static const unsigned leadMask[5] = {0, 0, 0x1Fu, 0x0Fu, 0x07u};
        unsigned cp = c0 & leadMask[len];
        bool result = true;
        for (unsigned char i = 1; i < len; i++)
        {
            const unsigned char t = static_cast<unsigned char>(is.Take());
            result &= ((t & 0xC0) == 0x80);
            cp = (cp << 6) | (t & 0x3Fu);
        }
        *codepoint = cp;

        // Reject overlong forms, surrogates and out-of-range codepoints.
        static const unsigned minCodepoint[5] = {0, 0, 0x80u, 0x800u, 0x10000u};
        result &= (cp >= minCodepoint[len]);
        result &= (cp <= 0x10FFFFu);
        result &= !(cp >= 0xD800u && cp <= 0xDFFFu);
        return result;
    }

    template <typename InputStream, typename OutputStream>
    static bool Validate(InputStream& is, OutputStream& os)
    {
        const Ch c0 = is.Take();
        os.Put(c0);
        const unsigned char u0 = static_cast<unsigned char>(c0);
        if (!(u0 & 0x80))
            return true;

        const unsigned char len = GetRange(u0);
        if (len < 2)
            return false;

        static const unsigned leadMask[5] = {0, 0, 0x1Fu, 0x0Fu, 0x07u};
        unsigned cp = u0 & leadMask[len];
        bool result = true;
        for (unsigned char i = 1; i < len; i++)
        {
            const Ch c = is.Take();
            os.Put(c);
            const unsigned char t = static_cast<unsigned char>(c);
            result &= ((t & 0xC0) == 0x80);
            cp = (cp << 6) | (t & 0x3Fu);
        }

        static const unsigned minCodepoint[5] = {0, 0, 0x80u, 0x800u, 0x10000u};
        result &= (cp >= minCodepoint[len]);
        result &= (cp <= 0x10FFFFu);
        result &= !(cp >= 0xD800u && cp <= 0xDFFFu);
        return result;
    }

    template <typename InputByteStream>
    static CharType TakeBOM(InputByteStream& is)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename InputByteStream::Ch) == 1);
        typename InputByteStream::Ch c = Take(is);
        if (static_cast<unsigned char>(c) != 0xEFu)
            return static_cast<Ch>(c);
        c = is.Take();
        if (static_cast<unsigned char>(c) != 0xBBu)
            return static_cast<Ch>(c);
        c = is.Take();
        if (static_cast<unsigned char>(c) != 0xBFu)
            return static_cast<Ch>(c);
        c = is.Take();
        return static_cast<Ch>(c);
    }

    template <typename InputByteStream>
    static Ch Take(InputByteStream& is)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename InputByteStream::Ch) == 1);
        return static_cast<Ch>(is.Take());
    }

    template <typename OutputByteStream>
    static void PutBOM(OutputByteStream& os)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename OutputByteStream::Ch) == 1);
        os.Put(static_cast<typename OutputByteStream::Ch>(0xEFu));
        os.Put(static_cast<typename OutputByteStream::Ch>(0xBBu));
        os.Put(static_cast<typename OutputByteStream::Ch>(0xBFu));
    }

    template <typename OutputByteStream>
    static void Put(OutputByteStream& os, Ch c)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename OutputByteStream::Ch) == 1);
        os.Put(static_cast<typename OutputByteStream::Ch>(c));
    }
};

///////////////////////////////////////////////////////////////////////////////
// UTF16

//! UTF-16 encoding.
template <typename CharType = wchar_t>
struct UTF16
{
    typedef CharType Ch;
    RAPIDYYJSON_STATIC_ASSERT(sizeof(Ch) >= 2);

    enum
    {
        supportUnicode = 1
    };

    template <typename OutputStream>
    static void Encode(OutputStream& os, unsigned codepoint)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename OutputStream::Ch) >= 2);
        if (codepoint <= 0xFFFF)
        {
            RAPIDYYJSON_ASSERT(codepoint < 0xD800 || codepoint > 0xDFFF);
            os.Put(static_cast<typename OutputStream::Ch>(codepoint));
        }
        else
        {
            RAPIDYYJSON_ASSERT(codepoint <= 0x10FFFF);
            unsigned v = codepoint - 0x10000;
            os.Put(static_cast<typename OutputStream::Ch>((v >> 10) | 0xD800));
            os.Put(static_cast<typename OutputStream::Ch>((v & 0x3FF) | 0xDC00));
        }
    }

    template <typename OutputStream>
    static void EncodeUnsafe(OutputStream& os, unsigned codepoint)
    {
        Encode(os, codepoint);
    }

    template <typename InputStream>
    static bool Decode(InputStream& is, unsigned* codepoint)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename InputStream::Ch) >= 2);
        typename InputStream::Ch c = is.Take();
        if (c < 0xD800 || c > 0xDFFF)
        {
            *codepoint = static_cast<unsigned>(c);
            return true;
        }
        else if (c <= 0xDBFF)
        {
            *codepoint = (static_cast<unsigned>(c) & 0x3FF) << 10;
            c = is.Take();
            *codepoint |= (static_cast<unsigned>(c) & 0x3FF);
            *codepoint += 0x10000;
            return c >= 0xDC00 && c <= 0xDFFF;
        }
        return false;
    }

    template <typename InputStream, typename OutputStream>
    static bool Validate(InputStream& is, OutputStream& os)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename InputStream::Ch) >= 2);
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename OutputStream::Ch) >= 2);
        typename InputStream::Ch c;
        os.Put(static_cast<typename OutputStream::Ch>(c = is.Take()));
        if (c < 0xD800 || c > 0xDFFF)
            return true;
        else if (c <= 0xDBFF)
        {
            os.Put(static_cast<typename OutputStream::Ch>(c = is.Take()));
            return c >= 0xDC00 && c <= 0xDFFF;
        }
        return false;
    }
};

//! UTF-16 little endian encoding.
template <typename CharType = wchar_t>
struct UTF16LE : UTF16<CharType>
{
    template <typename InputByteStream>
    static CharType TakeBOM(InputByteStream& is)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename InputByteStream::Ch) == 1);
        CharType c = Take(is);
        return static_cast<uint16_t>(c) == 0xFEFFu ? Take(is) : c;
    }

    template <typename InputByteStream>
    static CharType Take(InputByteStream& is)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename InputByteStream::Ch) == 1);
        unsigned c = static_cast<uint8_t>(is.Take());
        c |= static_cast<unsigned>(static_cast<uint8_t>(is.Take())) << 8;
        return static_cast<CharType>(c);
    }

    template <typename OutputByteStream>
    static void PutBOM(OutputByteStream& os)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename OutputByteStream::Ch) == 1);
        os.Put(static_cast<typename OutputByteStream::Ch>(0xFFu));
        os.Put(static_cast<typename OutputByteStream::Ch>(0xFEu));
    }

    template <typename OutputByteStream>
    static void Put(OutputByteStream& os, CharType c)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename OutputByteStream::Ch) == 1);
        os.Put(static_cast<typename OutputByteStream::Ch>(static_cast<unsigned>(c) & 0xFFu));
        os.Put(
            static_cast<typename OutputByteStream::Ch>((static_cast<unsigned>(c) >> 8) & 0xFFu));
    }
};

//! UTF-16 big endian encoding.
template <typename CharType = wchar_t>
struct UTF16BE : UTF16<CharType>
{
    template <typename InputByteStream>
    static CharType TakeBOM(InputByteStream& is)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename InputByteStream::Ch) == 1);
        CharType c = Take(is);
        return static_cast<uint16_t>(c) == 0xFEFFu ? Take(is) : c;
    }

    template <typename InputByteStream>
    static CharType Take(InputByteStream& is)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename InputByteStream::Ch) == 1);
        unsigned c = static_cast<unsigned>(static_cast<uint8_t>(is.Take())) << 8;
        c |= static_cast<uint8_t>(is.Take());
        return static_cast<CharType>(c);
    }

    template <typename OutputByteStream>
    static void PutBOM(OutputByteStream& os)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename OutputByteStream::Ch) == 1);
        os.Put(static_cast<typename OutputByteStream::Ch>(0xFEu));
        os.Put(static_cast<typename OutputByteStream::Ch>(0xFFu));
    }

    template <typename OutputByteStream>
    static void Put(OutputByteStream& os, CharType c)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename OutputByteStream::Ch) == 1);
        os.Put(
            static_cast<typename OutputByteStream::Ch>((static_cast<unsigned>(c) >> 8) & 0xFFu));
        os.Put(static_cast<typename OutputByteStream::Ch>(static_cast<unsigned>(c) & 0xFFu));
    }
};

///////////////////////////////////////////////////////////////////////////////
// UTF32

//! UTF-32 encoding.
template <typename CharType = unsigned>
struct UTF32
{
    typedef CharType Ch;
    RAPIDYYJSON_STATIC_ASSERT(sizeof(Ch) >= 4);

    enum
    {
        supportUnicode = 1
    };

    template <typename OutputStream>
    static void Encode(OutputStream& os, unsigned codepoint)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename OutputStream::Ch) >= 4);
        RAPIDYYJSON_ASSERT(codepoint <= 0x10FFFF);
        os.Put(codepoint);
    }

    template <typename OutputStream>
    static void EncodeUnsafe(OutputStream& os, unsigned codepoint)
    {
        Encode(os, codepoint);
    }

    template <typename InputStream>
    static bool Decode(InputStream& is, unsigned* codepoint)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename InputStream::Ch) >= 4);
        Ch c = is.Take();
        *codepoint = static_cast<unsigned>(c);
        return c <= 0x10FFFF;
    }

    template <typename InputStream, typename OutputStream>
    static bool Validate(InputStream& is, OutputStream& os)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename InputStream::Ch) >= 4);
        Ch c;
        os.Put(c = is.Take());
        return c <= 0x10FFFF;
    }
};

//! UTF-32 little endian enocoding.
template <typename CharType = unsigned>
struct UTF32LE : UTF32<CharType>
{
    template <typename InputByteStream>
    static CharType TakeBOM(InputByteStream& is)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename InputByteStream::Ch) == 1);
        CharType c = Take(is);
        return static_cast<uint32_t>(c) == 0x0000FEFFu ? Take(is) : c;
    }

    template <typename InputByteStream>
    static CharType Take(InputByteStream& is)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename InputByteStream::Ch) == 1);
        unsigned c = static_cast<uint8_t>(is.Take());
        c |= static_cast<unsigned>(static_cast<uint8_t>(is.Take())) << 8;
        c |= static_cast<unsigned>(static_cast<uint8_t>(is.Take())) << 16;
        c |= static_cast<unsigned>(static_cast<uint8_t>(is.Take())) << 24;
        return static_cast<CharType>(c);
    }

    template <typename OutputByteStream>
    static void PutBOM(OutputByteStream& os)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename OutputByteStream::Ch) == 1);
        os.Put(static_cast<typename OutputByteStream::Ch>(0xFFu));
        os.Put(static_cast<typename OutputByteStream::Ch>(0xFEu));
        os.Put(static_cast<typename OutputByteStream::Ch>(0x00u));
        os.Put(static_cast<typename OutputByteStream::Ch>(0x00u));
    }

    template <typename OutputByteStream>
    static void Put(OutputByteStream& os, CharType c)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename OutputByteStream::Ch) == 1);
        os.Put(static_cast<typename OutputByteStream::Ch>(c & 0xFFu));
        os.Put(static_cast<typename OutputByteStream::Ch>((c >> 8) & 0xFFu));
        os.Put(static_cast<typename OutputByteStream::Ch>((c >> 16) & 0xFFu));
        os.Put(static_cast<typename OutputByteStream::Ch>((c >> 24) & 0xFFu));
    }
};

//! UTF-32 big endian encoding.
template <typename CharType = unsigned>
struct UTF32BE : UTF32<CharType>
{
    template <typename InputByteStream>
    static CharType TakeBOM(InputByteStream& is)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename InputByteStream::Ch) == 1);
        CharType c = Take(is);
        return static_cast<uint32_t>(c) == 0x0000FEFFu ? Take(is) : c;
    }

    template <typename InputByteStream>
    static CharType Take(InputByteStream& is)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename InputByteStream::Ch) == 1);
        unsigned c = static_cast<unsigned>(static_cast<uint8_t>(is.Take())) << 24;
        c |= static_cast<unsigned>(static_cast<uint8_t>(is.Take())) << 16;
        c |= static_cast<unsigned>(static_cast<uint8_t>(is.Take())) << 8;
        c |= static_cast<unsigned>(static_cast<uint8_t>(is.Take()));
        return static_cast<CharType>(c);
    }

    template <typename OutputByteStream>
    static void PutBOM(OutputByteStream& os)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename OutputByteStream::Ch) == 1);
        os.Put(static_cast<typename OutputByteStream::Ch>(0x00u));
        os.Put(static_cast<typename OutputByteStream::Ch>(0x00u));
        os.Put(static_cast<typename OutputByteStream::Ch>(0xFEu));
        os.Put(static_cast<typename OutputByteStream::Ch>(0xFFu));
    }

    template <typename OutputByteStream>
    static void Put(OutputByteStream& os, CharType c)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename OutputByteStream::Ch) == 1);
        os.Put(static_cast<typename OutputByteStream::Ch>((c >> 24) & 0xFFu));
        os.Put(static_cast<typename OutputByteStream::Ch>((c >> 16) & 0xFFu));
        os.Put(static_cast<typename OutputByteStream::Ch>((c >> 8) & 0xFFu));
        os.Put(static_cast<typename OutputByteStream::Ch>(c & 0xFFu));
    }
};

///////////////////////////////////////////////////////////////////////////////
// ASCII

//! ASCII encoding.
template <typename CharType = char>
struct ASCII
{
    typedef CharType Ch;

    enum
    {
        supportUnicode = 0
    };

    template <typename OutputStream>
    static void Encode(OutputStream& os, unsigned codepoint)
    {
        RAPIDYYJSON_ASSERT(codepoint <= 0x7F);
        os.Put(static_cast<Ch>(codepoint & 0xFF));
    }

    template <typename OutputStream>
    static void EncodeUnsafe(OutputStream& os, unsigned codepoint)
    {
        Encode(os, codepoint);
    }

    template <typename InputStream>
    static bool Decode(InputStream& is, unsigned* codepoint)
    {
        uint8_t c = static_cast<uint8_t>(is.Take());
        *codepoint = c;
        return c <= 0X7F;
    }

    template <typename InputStream, typename OutputStream>
    static bool Validate(InputStream& is, OutputStream& os)
    {
        uint8_t c = static_cast<uint8_t>(is.Take());
        os.Put(static_cast<typename OutputStream::Ch>(c));
        return c <= 0x7F;
    }

    template <typename InputByteStream>
    static CharType TakeBOM(InputByteStream& is)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename InputByteStream::Ch) == 1);
        uint8_t c = static_cast<uint8_t>(Take(is));
        return static_cast<Ch>(c);
    }

    template <typename InputByteStream>
    static Ch Take(InputByteStream& is)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename InputByteStream::Ch) == 1);
        return static_cast<Ch>(is.Take());
    }

    template <typename OutputByteStream>
    static void PutBOM(OutputByteStream& os)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename OutputByteStream::Ch) == 1);
        (void)os;
    }

    template <typename OutputByteStream>
    static void Put(OutputByteStream& os, Ch c)
    {
        RAPIDYYJSON_STATIC_ASSERT(sizeof(typename OutputByteStream::Ch) == 1);
        os.Put(static_cast<typename OutputByteStream::Ch>(c));
    }
};

///////////////////////////////////////////////////////////////////////////////
// AutoUTF

//! Runtime-specified UTF encoding type of a stream.
enum UTFType
{
    kUTF8 = 0,    //!< UTF-8.
    kUTF16LE = 1, //!< UTF-16 little endian.
    kUTF16BE = 2, //!< UTF-16 big endian.
    kUTF32LE = 3, //!< UTF-32 little endian.
    kUTF32BE = 4  //!< UTF-32 big endian.
};

//! Dynamically select encoding according to stream's runtime-specified UTF encoding type.
template <typename CharType>
struct AutoUTF
{
    typedef CharType Ch;

    enum
    {
        supportUnicode = 1
    };

#define RAPIDYYJSON_ENCODINGS_FUNC(x)                                                              \
    UTF8<Ch>::x, UTF16LE<Ch>::x, UTF16BE<Ch>::x, UTF32LE<Ch>::x, UTF32BE<Ch>::x

    template <typename OutputStream>
    static RAPIDYYJSON_FORCEINLINE void Encode(OutputStream& os, unsigned codepoint)
    {
        typedef void (*EncodeFunc)(OutputStream&, unsigned);
        static const EncodeFunc f[] = {RAPIDYYJSON_ENCODINGS_FUNC(Encode)};
        (*f[os.GetType()])(os, codepoint);
    }

    template <typename OutputStream>
    static RAPIDYYJSON_FORCEINLINE void EncodeUnsafe(OutputStream& os, unsigned codepoint)
    {
        typedef void (*EncodeFunc)(OutputStream&, unsigned);
        static const EncodeFunc f[] = {RAPIDYYJSON_ENCODINGS_FUNC(EncodeUnsafe)};
        (*f[os.GetType()])(os, codepoint);
    }

    template <typename InputStream>
    static RAPIDYYJSON_FORCEINLINE bool Decode(InputStream& is, unsigned* codepoint)
    {
        typedef bool (*DecodeFunc)(InputStream&, unsigned*);
        static const DecodeFunc f[] = {RAPIDYYJSON_ENCODINGS_FUNC(Decode)};
        return (*f[is.GetType()])(is, codepoint);
    }

    template <typename InputStream, typename OutputStream>
    static RAPIDYYJSON_FORCEINLINE bool Validate(InputStream& is, OutputStream& os)
    {
        typedef bool (*ValidateFunc)(InputStream&, OutputStream&);
        static const ValidateFunc f[] = {RAPIDYYJSON_ENCODINGS_FUNC(Validate)};
        return (*f[is.GetType()])(is, os);
    }

#undef RAPIDYYJSON_ENCODINGS_FUNC
};

///////////////////////////////////////////////////////////////////////////////
// Transcoder

//! Encoding conversion.
template <typename SourceEncoding, typename TargetEncoding>
struct Transcoder
{
    //! Take one Unicode codepoint from source encoding, convert it to target encoding and put it
    //! to the output stream.
    template <typename InputStream, typename OutputStream>
    static RAPIDYYJSON_FORCEINLINE bool Transcode(InputStream& is, OutputStream& os)
    {
        unsigned codepoint;
        if (!SourceEncoding::Decode(is, &codepoint))
            return false;
        TargetEncoding::Encode(os, codepoint);
        return true;
    }

    template <typename InputStream, typename OutputStream>
    static RAPIDYYJSON_FORCEINLINE bool TranscodeUnsafe(InputStream& is, OutputStream& os)
    {
        unsigned codepoint;
        if (!SourceEncoding::Decode(is, &codepoint))
            return false;
        TargetEncoding::EncodeUnsafe(os, codepoint);
        return true;
    }

    //! Validate one Unicode codepoint from an encoded stream.
    template <typename InputStream, typename OutputStream>
    static RAPIDYYJSON_FORCEINLINE bool Validate(InputStream& is, OutputStream& os)
    {
        return Transcode(is, os); // Since source/target encoding is different, must transcode.
    }
};

// Forward declaration for the specialization below.
template <typename Stream>
inline void
PutUnsafe(Stream& stream, typename Stream::Ch c);

//! Specialization of Transcoder with same source and target encoding.
template <typename Encoding>
struct Transcoder<Encoding, Encoding>
{
    template <typename InputStream, typename OutputStream>
    static RAPIDYYJSON_FORCEINLINE bool Transcode(InputStream& is, OutputStream& os)
    {
        os.Put(is.Take()); // Just copy one code unit. This semantic is different from
                           // primary template class.
        return true;
    }

    template <typename InputStream, typename OutputStream>
    static RAPIDYYJSON_FORCEINLINE bool TranscodeUnsafe(InputStream& is, OutputStream& os)
    {
        PutUnsafe(os, is.Take()); // Just copy one code unit.
        return true;
    }

    template <typename InputStream, typename OutputStream>
    static RAPIDYYJSON_FORCEINLINE bool Validate(InputStream& is, OutputStream& os)
    {
        return Encoding::Validate(is, os); // source/target encoding are the same
    }
};

RAPIDYYJSON_NAMESPACE_END

#endif // RAPIDYYJSON_ENCODINGS_H_
