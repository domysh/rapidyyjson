/*
 * rapidyyjson - a RapidJSON-compatible API implemented on top of yyjson.
 *
 * Mirrors `rapidjson/cursorstreamwrapper.h`.
 */

#ifndef RAPIDYYJSON_CURSORSTREAMWRAPPER_H_
#define RAPIDYYJSON_CURSORSTREAMWRAPPER_H_

#include "stream.h"

RAPIDYYJSON_NAMESPACE_BEGIN

//! Cursor stream wrapper for counting line and column number if error exists.
/*!
    \tparam InputStream     Any stream that implements Stream Concept
*/
template <typename InputStream, typename Encoding = UTF8<>>
class CursorStreamWrapper : public GenericStreamWrapper<InputStream, Encoding>
{
  public:
    typedef typename Encoding::Ch Ch;

    CursorStreamWrapper(InputStream& is)
        : GenericStreamWrapper<InputStream, Encoding>(is),
          line_(1),
          col_(0)
    {
    }

    // counting line and column number
    Ch Take()
    {
        Ch ch = this->is_.Take();
        if (ch == '\n')
        {
            line_++;
            col_ = 0;
        }
        else
        {
            col_++;
        }
        return ch;
    }

    //! Get the error line number, if error exists.
    size_t GetLine() const
    {
        return line_;
    }

    //! Get the error column number, if error exists.
    size_t GetColumn() const
    {
        return col_;
    }

  private:
    size_t line_; //!< Current Line
    size_t col_;  //!< Current Column
};

RAPIDYYJSON_NAMESPACE_END

#endif // RAPIDYYJSON_CURSORSTREAMWRAPPER_H_
