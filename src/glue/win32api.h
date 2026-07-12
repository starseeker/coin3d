#ifndef OBOL_GLUE_WIN32API_H
#define OBOL_GLUE_WIN32API_H

#include <windows.h>

#include <cstdio>

static inline void
cc_win32_print_error(const char * context, const char * function,
                     const DWORD error)
{
  char * message = nullptr;
  const DWORD length = FormatMessageA(
    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS,
    nullptr, error, 0, reinterpret_cast<char *>(&message), 0, nullptr);

  if (length != 0 && message != nullptr) {
    std::fprintf(stderr, "%s: %s failed (error %lu): %s",
                 context, function, static_cast<unsigned long>(error), message);
    LocalFree(message);
  }
  else {
    std::fprintf(stderr, "%s: %s failed (error %lu)\n",
                 context, function, static_cast<unsigned long>(error));
  }
}

#endif /* OBOL_GLUE_WIN32API_H */
