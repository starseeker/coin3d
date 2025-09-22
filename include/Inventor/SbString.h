#ifndef COIN_SBSTRING_H
#define COIN_SBSTRING_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <string>
#include <cstring>
#include "utf8/utf8.h"

#ifdef COIN_INTERNAL
 #define COIN_ALLOW_SBINTLIST
 #include <Inventor/lists/SbIntList.h>
 #undef COIN_ALLOW_SBINTLIST
#else
 #include <Inventor/lists/SbIntList.h>
#endif // COIN_INTERNAL

// *************************************************************************

class COIN_DLL_API SbString {
public:
  SbString(void) = default;

  SbString(const char * s) : str_(s ? s : "") {}

  SbString(const wchar_t * s) {
    if (s) {
      // Use neacsum/utf8 library for proper wide character to UTF-8 conversion
      str_ = utf8::narrow(s);
    }
  }

  SbString(const char * s, int start, int end) {
    if (s && start >= 0) {
      std::string temp(s);
      if (end == -1) end = static_cast<int>(temp.length()) - 1;
      if (end >= start && start < static_cast<int>(temp.length())) {
        str_ = temp.substr(start, end - start + 1);
      }
    }
  }

  SbString(const SbString & s) = default;

  SbString(const int digits) : str_(std::to_string(digits)) {}

  ~SbString() = default;

  uint32_t hash(void) const { 
    std::hash<std::string> hasher;
    return static_cast<uint32_t>(hasher(str_));
  }
  
  static uint32_t hash(const char * s) { 
    std::hash<std::string> hasher;
    return static_cast<uint32_t>(hasher(s ? s : ""));
  }

  int getLength(void) const { 
    return static_cast<int>(str_.length());
  }

  void makeEmpty(SbBool freeold = TRUE) {
    str_.clear();
  }

  const char * getString(void) const { return str_.c_str(); }

  SbString getSubString(int startidx, int endidx = -1) const {
    if (startidx < 0) return SbString();
    
    std::string temp = str_;
    if (endidx == -1) endidx = static_cast<int>(temp.length()) - 1;
    
    if (startidx < static_cast<int>(temp.length()) && endidx >= startidx) {
      return SbString(temp.substr(startidx, endidx - startidx + 1).c_str());
    }
    return SbString();
  }
  
  void deleteSubString(int startidx, int endidx = -1) {
    if (startidx < 0) return;
    
    if (endidx == -1) endidx = static_cast<int>(str_.length()) - 1;
    
    if (startidx < static_cast<int>(str_.length()) && endidx >= startidx) {
      str_.erase(startidx, endidx - startidx + 1);
    }
  }

  void addIntString(const int value) { str_ += std::to_string(value); }

  char operator[](int index) const { 
    return (index >= 0 && index < static_cast<int>(str_.length())) ? str_[index] : '\0';
  }

  SbString & operator=(const char * s) {
    str_ = s ? s : "";
    return *this;
  }
  
  SbString & operator=(const SbString & s) = default;

  SbString & operator+=(const char * s) {
    if (s) str_ += s;
    return *this;
  }
  
  SbString & operator+=(const SbString & s) {
    str_ += s.str_;
    return *this;
  }
  
  SbString & operator+=(const char c) {
    str_ += c;
    return *this;
  }

  int operator!(void) const { return str_.empty(); }

  int compareSubString(const char * text, int offset = 0) const {
    if (!text || offset < 0 || offset >= static_cast<int>(str_.length())) {
      return text && *text ? -1 : 0;
    }
    
    std::string substr = str_.substr(offset, std::strlen(text));
    return substr.compare(text);
  }

  SbString & sprintf(const char * formatstr, ...) {
    va_list args;
    va_start(args, formatstr);
    vsprintf(formatstr, args);
    va_end(args);
    return *this;
  }
  
  SbString & vsprintf(const char * formatstr, va_list args) {
    // Determine required size
    va_list args_copy;
    va_copy(args_copy, args);
    int size = std::vsnprintf(nullptr, 0, formatstr, args);
    va_end(args_copy);
    
    if (size > 0) {
      str_.resize(size + 1);
      std::vsnprintf(&str_[0], size + 1, formatstr, args);
      str_.resize(size); // Remove the null terminator
    } else {
      str_.clear();
    }
    
    return *this;
  }

  void apply(char (*func)(char input)) {
    std::transform(str_.begin(), str_.end(), str_.begin(), func);
  }

  int find(const SbString & s) const;
  SbBool findAll(const SbString & s, SbIntList & found) const;

  SbString lower() const;
  SbString upper() const;

  void print(std::FILE * fp) const;

  friend int operator==(const SbString & sbstr, const char * s);
  friend int operator==(const char * s, const SbString & sbstr);
  friend int operator==(const SbString & str1, const SbString & str2);
  friend int operator!=(const SbString & sbstr, const char * s);
  friend int operator!=(const char * s, const SbString & sbstr);
  friend int operator!=(const SbString & str1, const SbString & str2);
  friend int operator<(const SbString & sbstr, const char * s);
  friend int operator<(const char * s, const SbString & sbstr);
  friend int operator<(const SbString & str1, const SbString & str2);
  friend int operator>(const SbString & sbstr, const char * s);
  friend int operator>(const char * s, const SbString & sbstr);
  friend int operator>(const SbString & str1, const SbString & str2);
  friend const SbString operator+(const SbString & str1, const SbString & str2);
  friend const SbString operator+(const SbString & sbstr, const char * s);
  friend const SbString operator+(const char * s, const SbString & sbstr);

private:
  std::string str_;
};

inline int operator==(const SbString & sbstr, const char * s)
{ return (sbstr.str_.compare(s ? s : "") == 0); }
inline int operator==(const char * s, const SbString & sbstr)
{ return (sbstr.str_.compare(s ? s : "") == 0); }
inline int operator==(const SbString & str1, const SbString & str2)
{ return (str1.str_ == str2.str_); }

inline int operator!=(const SbString & sbstr, const char * s)
{ return (sbstr.str_.compare(s ? s : "") != 0); }
inline int operator!=(const char * s, const SbString & sbstr)
{ return (sbstr.str_.compare(s ? s : "") != 0); }
inline int operator!=(const SbString & str1, const SbString & str2)
{ return (str1.str_ != str2.str_); }

inline int operator<(const SbString & sbstr, const char * s)
{ return (sbstr.str_.compare(s ? s : "") < 0); }
inline int operator<(const char * s, const SbString & sbstr)
{ return ((s ? s : "") < sbstr.str_); }
inline int operator<(const SbString & str1, const SbString & str2)
{ return (str1.str_ < str2.str_); }

inline int operator>(const SbString & sbstr, const char * s)
{ return (sbstr.str_.compare(s ? s : "") > 0); }
inline int operator>(const char * s, const SbString & sbstr)
{ return ((s ? s : "") > sbstr.str_); }
inline int operator>(const SbString & str1, const SbString & str2)
{ return (str1.str_ > str2.str_); }

inline const SbString operator+(const SbString & str1, const SbString & str2)
{ 
  SbString newstr(str1);
  newstr += str2;
  return newstr;
}
inline const SbString operator+(const SbString & sbstr, const char * s)
{
  SbString newstr(sbstr);
  newstr += s;
  return newstr;
}
inline const SbString operator+(const char * s, const SbString & sbstr)
{
  SbString newstr(s);
  newstr += sbstr;
  return newstr;
}

#endif // !COIN_SBSTRING_H