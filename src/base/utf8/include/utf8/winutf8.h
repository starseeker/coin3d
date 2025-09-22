/*
  Copyright (c) Mircea Neacsu (2014-2024) Licensed under MIT License.
  This is part of UTF8 project. See LICENSE file for full license terms.
*/

/// \file winutf8.h Windows specific parts
#pragma once

#include <Windows.h>
#include <string>
#include <ostream>

#undef MessageBox
#undef CopyFile
#undef LoadString
#undef ShellExecute
#undef GetTempPath
#undef GetTempFileName
#undef GetFullPathName
#undef GetModuleFileName
#undef RegCreateKey
#undef RegDeleteKey
#undef RegOpenKey
#undef RegSetValue
#undef RegQueryValue
#undef RegGetValue
#undef RegDeleteTree
#undef RegEnumKey
#undef RegDeleteValue
#undef RegEnumValue

namespace utf8 {

std::vector<std::string> get_argv ();
char** get_argv (int* argc);
void free_argv (int argc, char** argv);

bool chmod (const char* filename, int mode);
bool chmod (const std::string& filename, int mode);

bool access (const char* filename, int mode);
bool access (const std::string& filename, int mode);

bool splitpath (const std::string& path, char* drive, char* dir, char* fname, char* ext);
bool splitpath (const std::string& path, std::string& drive, std::string& dir,
                std::string& fname, std::string& ext);
bool makepath (std::string& path, const std::string& drive, const std::string& dir,
                const std::string& fname, const std::string& ext);
std::string fullpath (const std::string& relpath);

std::string getenv (const std::string& var);
bool putenv (const std::string& str);
bool putenv (const std::string& var, const std::string& val);

bool symlink (const char* path, const char* link, bool directory);
bool symlink (const std::string& path, const std::string& target, bool directory);

int system (const std::string& cmd);

int MessageBox (HWND hWnd, const std::string& text, const std::string& caption,
  unsigned int type);
bool CopyFile (const std::string& from, const std::string& to, bool fail_exist);
std::string LoadString (UINT id, HINSTANCE hInst = NULL);

//registry functions
LSTATUS RegCreateKey (HKEY key, const std::string& subkey, HKEY& result,
  DWORD options = REG_OPTION_NON_VOLATILE, REGSAM sam = KEY_ALL_ACCESS,
  const SECURITY_ATTRIBUTES* psa = nullptr, DWORD* disp = nullptr);
LSTATUS RegOpenKey (HKEY key, const std::string& subkey, HKEY& result,
  REGSAM sam = KEY_ALL_ACCESS, bool link = false);
LSTATUS RegDeleteKey (HKEY key, const std::string& subkey, REGSAM sam = 0);
LSTATUS RegDeleteValue (HKEY key, const std::string& value);
LSTATUS RegDeleteTree (HKEY key, const std::string& subkey = std::string ());
LSTATUS RegRenameKey (HKEY key, const std::string& subkey, const std::string& new_name);
LSTATUS RegSetValue (HKEY key, const std::string& value, DWORD type, const void* data, DWORD size);
LSTATUS RegSetValue (HKEY key, const std::string& value, const std::string& data);
LSTATUS RegSetValue (HKEY key, const std::string& value, const std::vector<std::string>& data);
LSTATUS RegQueryValue (HKEY key, const std::string& value, DWORD* type, void* data, DWORD* size);
LSTATUS RegGetValue (HKEY key, const std::string& subkey, const std::string& value,
  DWORD flags, void* data, DWORD* size, DWORD* type = NULL);
LSTATUS RegGetValue (HKEY key, const std::string& subkey, const std::string& value,
  std::string& data, bool expand = false);
LSTATUS RegGetValue (HKEY key, const std::string& subkey, const std::string& value,
  std::vector<std::string>& data);
LSTATUS RegEnumKey (HKEY key, DWORD index, std::string& name, DWORD maxlen = 0, FILETIME* last_write_time = NULL);
LSTATUS RegEnumKey (HKEY key, std::vector<std::string>& names);
LSTATUS RegEnumValue (HKEY key, DWORD index, std::string& value, DWORD maxlen = 0, DWORD* type = 0, void* data = 0, DWORD* data_len = 0);
LSTATUS RegEnumValue (HKEY key, std::vector<std::string>& values);

HINSTANCE ShellExecute (const std::string& file, const std::string& verb = std::string (),
                        const std::string& parameters = std::string (),
                        const std::string& directory = std::string ("."),
                        HWND hWnd = NULL, int show = SW_SHOW);
std::string GetTempPath ();
std::string GetTempFileName (const std::string& path, const std::string& prefix, UINT unique=0);
std::string GetFullPathName (const std::string& rel_path);
bool GetModuleFileName (HMODULE hModule, std::string& filename);
std::string GetModuleFileName (HMODULE hModule = NULL);

/// File enumeration structure used by find_first() and find_next() functions 
struct find_data {
  find_data ()                        ///< Initializes the structure
    : handle{ INVALID_HANDLE_VALUE }
    , attributes{ 0 }
    , creation_time{ 0, 0 }
    , access_time{ 0,0 }
    , write_time{ 0,0 }
    , size{ 0 }
  {}
  HANDLE   handle;                    ///< search handle
  DWORD    attributes;                ///< file attributes
  FILETIME creation_time;             ///< file creation time
  FILETIME access_time;               ///< file last access time
  FILETIME write_time;                ///< file last write time
  __int64  size;                      ///< file size
  std::string  filename;              ///< file name
  std::string  short_name;            ///< 8.3 file name
};

bool find_first (const std::string& name, find_data& fdat);
bool find_next (find_data& fdat);
void find_close (find_data& fdat);

/*!
  An object - oriented wrapper for find_... functions

  This object wraps a Windows search handle used in find_first/find_next
  functions.

  Use like in the following example:
  \code
  utf8::file_enumerator collection("sample.*");
  while (collection.ok())
  {
    cout << collection.filename () << endl;
    collection.next ();
  }
  \endcode
*/
class file_enumerator : protected find_data
{
public:
  explicit file_enumerator (const std::string& name);
  ~file_enumerator ();
  bool ok () const;
  bool next ();

  operator bool () const;

  find_data::attributes;
  find_data::creation_time;
  find_data::access_time;
  find_data::write_time;
  find_data::size;
  find_data::filename;
  find_data::short_name;
};

/// A simple buffer for caching values returned by Windows API 
class buffer {
public:
  explicit buffer (size_t size_);
  ~buffer ();
  buffer (const buffer& other);
  buffer& operator = (const buffer& rhs);
  buffer& operator = (const std::string& rhs);
  operator wchar_t* ();
  operator std::string () const;
  DWORD size () const;

private:
  wchar_t *ptr;
  size_t sz;
};

inline std::ostream&
operator << (std::ostream& s, const buffer& b)
{
  s << (std::string)b;
  return s;
}


//--------------------- INLINE FUNCTIONS --------------------------------------

/*!
  Changes the file access permissions

  \param filename UTF-8 name of file
  \param mode access permissions. Or'ed combination of:
              - _S_IWRITE write permission
              - _S_IREAD  read permission

  \return true if successful, false otherwise
*/
inline
bool chmod (const char* filename, int mode)
{
  return (_wchmod (widen (filename).c_str (), mode) == 0);
}

/// \copydoc utf8::chmod()
inline
bool chmod (const std::string& filename, int mode)
{
  return (_wchmod (widen (filename).c_str (), mode) == 0);
}


/*!
  Determines if a file has the requested access permissions

  \param filename UTF-8 file path of new working directory
  \param mode required access:
              - 0 existence only
              - 2 write permission
              - 4 read permission
              - 6 read/write permission

  \return true if successful, false otherwise

*/
inline
bool access (const char* filename, int mode)
{
  return (_waccess (widen (filename).c_str (), mode) == 0);
}

/// \copydoc utf8::access()
inline
bool access (const std::string& filename, int mode)
{
  return (_waccess (widen (filename).c_str (), mode) == 0);
}


/*!
  Creates, modifies, or removes environment variables.
  This is a wrapper for [_wputenv](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/putenv-wputenv)
  function.

  \param str environment string to modify
  \return true if successful, false otherwise.
*/
inline
bool putenv (const std::string& str)
{
  return (_wputenv (utf8::widen (str).c_str ()) == 0);
}

/*!
  Creates, modifies, or removes environment variables.
  This is a wrapper for [_wputenv_s](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/putenv-s-wputenv-s)
  function.

  \param var  name of environment variable
  \param val  new value of environment variable. If empty, the environment
              variable is removed
  \return true if successful, false otherwise
*/
inline
bool putenv (const std::string& var, const std::string& val)
{
  return (_wputenv_s (widen (var).c_str (),
    widen (val).c_str ()) == 0);
}

/*!
  Passes command to command interpreter

  \param cmd UTF-8 encoded command to pass

  This is a wrapper for [_wsystem](https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/system-wsystem)
  function.
*/
inline
int system (const std::string& cmd)
{
  std::wstring wcmd = utf8::widen (cmd);
  return _wsystem (wcmd.c_str ());
}

// -------------------- file_enumerator ------------------------------------

/// Constructs a file_enumerator object and tries to locate the first file
inline
file_enumerator::file_enumerator (const std::string& name)
{
  find_first (name, *this);
}

/// Closes the search handle associated with this object
inline
file_enumerator::~file_enumerator ()
{
  if (handle != INVALID_HANDLE_VALUE)
    find_close (*this);
}

/// Return _true_ if a file has been enumerated
inline
bool file_enumerator::ok () const
{
  return (handle != INVALID_HANDLE_VALUE);
}

//! Syntactic sugar for ok() function
//! Return _true_ if a file has been enumerated
inline
file_enumerator::operator bool () const
{
  return ok ();
}

/// Advance the enumerator to next file
inline
bool file_enumerator::next ()
{
  return find_next (*this);
}

//---------------------------- buffer ----------------------------------------

/// Return a pointer to buffer
inline
  buffer::operator wchar_t* ()
{
  return ptr;
}

/// Convert buffer to an UTF-8 encoded string
inline
  buffer::operator std::string () const
{
  return utf8::narrow (ptr);
}

/// Return buffer size
inline DWORD
  buffer::size () const
{
  return (DWORD)sz;
}

} //end namespace

