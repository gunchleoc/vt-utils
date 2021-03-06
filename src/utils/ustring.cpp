///////////////////////////////////////////////////////////////////////////////
//            Copyright (C) 2004-2011 by The Allacrost Project
//            Copyright (C) 2012-2016 by Bertram (Valyria Tear)
//                         All Rights Reserved
//
// This code is licensed under the GNU GPL version 2. It is free software
// and you may modify it and/or redistribute it under the terms of this license.
// See https://www.gnu.org/copyleft/gpl.html for details.
///////////////////////////////////////////////////////////////////////////////

/** ****************************************************************************
*** \file    ustring.cpp
*** \author  Tyler Olsen, roots@allacrost.org
*** \author  Yohann Ferreira, yohann ferreira orange fr
*** \brief   Source file for the UTF16 string code.
*** ***************************************************************************/

#include "ustring.h"

#include <iconv.h>

#include <stdexcept>
#include <limits>

// For correct endianess support
#include <SDL2/SDL_endian.h>

namespace vt_utils
{

////////////////////////////////////////////////////////////////////////////////
///// ustring Class
////////////////////////////////////////////////////////////////////////////////

const size_t ustring::npos = ~0;

ustring::ustring()
{
    _str.push_back(0);
}

ustring::ustring(const uint16_t *s)
{
    _str.clear();

    if(!s) {
        _str.push_back(0);
        return;
    }

    // Avoid memory reallocations when pushing back
    size_t i = 0;
    while(s[i] != 0) {
        ++i;
    }
    _str.reserve(i);

    while(*s != 0) {
        _str.push_back(*s);
        ++s;
    }

    _str.push_back(0);
}

// Return a substring starting at pos, continuing for n elements
ustring ustring::substr(size_t pos, size_t n) const
{
    size_t len = length();

    if(pos >= len)
        throw std::out_of_range("pos passed to substr() was too large");

    ustring s;
    if(n == std::numeric_limits<size_t>::max() || pos + n > len) {
        n = len - pos;
    }
    s._str.reserve(n + 1);
    s._str.assign(_str.begin() + pos, _str.begin() + pos + n);
    s._str.push_back(0);

    return s;
}

// Concatenates string to another
ustring ustring::operator + (const ustring &s) const
{
    ustring temp(*this);
    return (temp += s);
}

// Adds a character to end of this string
ustring& ustring::operator += (uint16_t c)
{
    _str.insert(_str.end() - 1, c);
    return *this;
}

// Concatenate another string on to the end of this string
ustring &ustring::operator += (const ustring &s)
{
    // nothing to do for empty string
    if(s.empty())
        return *this;

    _str.insert(_str.end() - 1, s._str.begin(), s._str.end() - 1);
    return *this;
}

// Compare two substrings
bool ustring::operator == (const ustring &s) const
{
    return (s._str == _str);
} // bool ustring::operator == (const ustring &s)

// Finds a character within a string, starting at pos. If nothing is found, npos is returned
size_t ustring::find(uint16_t c, size_t pos) const
{
    size_t len = length();

    for(size_t j = pos; j < len; ++j) {
        if(_str[j] == c)
            return j;
    }

    return npos;
} // size_t ustring::find(uint16_t c, size_t pos) const

// Finds a string within a string, starting at pos. If nothing is found, npos is returned
size_t ustring::find(const ustring &s, size_t pos) const
{
    size_t len = length();
    size_t total_chars = s.length();
    size_t chars_found = 0;

    for(size_t j = pos; j < len; ++j) {
        if(_str[j] == s[chars_found]) {
            ++chars_found;
            if(chars_found == total_chars) {
                return (j - chars_found + 1);
            }
        } else {
            chars_found = 0;
        }
    }

    return npos;
} // size_t ustring::find(const ustring &s, size_t pos) const

////////////////////////////////////////////////////////////////////////////////
///// ustring manipulator functions
////////////////////////////////////////////////////////////////////////////////

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define UTF_16_ICONV_NAME "UTF-16LE"
#else
#define UTF_16_ICONV_NAME "UTF-16BE"
#endif

#define UTF_16_BOM_STD 0xFEFF
#define UTF_16_BOM_REV 0xFFFE

static bool UTF8ToUTF16(const std::string& source, uint16_t *dest)
{
    if (source.empty()) {
        return true;
    }

    iconv_t convertor = iconv_open(UTF_16_ICONV_NAME, "UTF-8");
    if(convertor == (iconv_t) - 1) {
        return false;
    }

#if (defined(_LIBICONV_VERSION) && _LIBICONV_VERSION == 0x0109) || defined(__FreeBSD__)
    // We are using an iconv API that uses const char*
    const char *sourceChar = source.c_str();
#else
    // The iconv API doesn't specify a const source for legacy support reasons.
    // Versions after 0x0109 changed back to char* for POSIX reasons.
    char *sourceChar = const_cast<char *>(source.c_str());
#endif
    char *destChar = reinterpret_cast<char *>(dest);
    size_t sourceLen = source.length() + 1;
    size_t destLen = (source.length() + 1) * 2;
    size_t ret = iconv(convertor, &sourceChar, &sourceLen,
                       &destChar, &destLen);
    iconv_close(convertor);
    if(ret == (size_t) - 1) {
        perror("iconv");
        return false;
    }
    return true;
}

// Creates a ustring from a normal string
ustring MakeUnicodeString(const std::string& text)
{
    size_t length = text.length() + 1;
    std::vector<uint16_t> ubuff(length, 0);
    ubuff.reserve(length);
    // Point to the buffer start after reservation to avoid invalidating it.
    uint16_t *utf16String = &ubuff[0];
    if(UTF8ToUTF16(text, &ubuff[0])) {
        // Skip the "Byte Order Mark" from the UTF16 specification
        if(utf16String[0] == UTF_16_BOM_STD ||  utf16String[0] == UTF_16_BOM_REV) {
            utf16String = &ubuff[1];
        }

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
        // For some reason, using UTF-16BE to iconv on big-endian machines
        // still does not create correctly accented characters, so this
        // byte swapping must be performed (only for irregular characters,
        // hence the mask).

        for(size_t c = 0; c < length; ++c)
            if(utf16String[c] & 0xFF80)
                utf16String[c] = (utf16String[c] << 8) | (utf16String[c] >> 8);
#endif
    } else {
        for(size_t c = 0; c < length; ++c) {
            ubuff.push_back(text[c]);
        }
        ubuff.push_back(0);
    }

    ustring new_ustr(utf16String);
    return new_ustr;
} // ustring MakeUnicodeString(const string& text)


// Creates a normal string from a ustring
std::string MakeStandardString(const ustring &text)
{
    const size_t length = text.length();
    std::vector<unsigned char> strbuff(length + 1,'\0');

    for(size_t c = 0; c < length; ++c) {
        uint16_t curr_char = text[c];

        if(curr_char > 0xff)
            strbuff[c] = '?';
        else
            strbuff[c] = static_cast<unsigned char>(curr_char);
    }

    return std::string(reinterpret_cast<char *>(&strbuff[0]));
} // string MakeStandardString(const ustring& text)

} // namespace utils
