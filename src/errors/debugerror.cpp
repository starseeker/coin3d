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

#include "errors/CoinInternalError.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>
#include <string>

// Simple consolidated implementation that provides exact C API compatibility

void
cc_debugerror_post(const char * source, const char * format, ...)
{
  va_list args;
  va_start(args, format);
  
  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  // Simple stderr output for now
  if (source) {
    fprintf(stderr, "Coin error in %s(): %s\n", source, buffer);
  } else {
    fprintf(stderr, "Coin error: %s\n", buffer);
  }
}

void
cc_debugerror_postwarning(const char * source, const char * format, ...)
{
  va_list args;
  va_start(args, format);
  
  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  // Simple stderr output for now
  if (source) {
    fprintf(stderr, "Coin warning in %s(): %s\n", source, buffer);
  } else {
    fprintf(stderr, "Coin warning: %s\n", buffer);
  }
}

void
cc_debugerror_postinfo(const char * source, const char * format, ...)
{
  va_list args;
  va_start(args, format);
  
  char buffer[1024];
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  
  // Simple stderr output for now
  if (source) {
    fprintf(stderr, "Coin info in %s(): %s\n", source, buffer);
  } else {
    fprintf(stderr, "Coin info: %s\n", buffer);
  }
}

void
cc_debugerror_init(cc_debugerror * me)
{
  cc_error_init(&me->super);
  me->severity = CC_DEBUGERROR_ERROR;
}

void
cc_debugerror_clean(cc_debugerror * me)
{
  cc_error_clean(&me->super);
}

CC_DEBUGERROR_SEVERITY
cc_debugerror_get_severity(const cc_debugerror * me)
{
  return me->severity;
}

static cc_debugerror_cb * debug_callback = NULL;
static void * debug_callback_data = NULL;

void
cc_debugerror_set_handler_callback(cc_debugerror_cb * function, void * data)
{
  debug_callback = function;
  debug_callback_data = data;
}

cc_debugerror_cb *
cc_debugerror_get_handler_callback(void)
{
  return debug_callback;
}

void *
cc_debugerror_get_handler_data(void)
{
  return debug_callback_data;
}

cc_debugerror_cb *
cc_debugerror_get_handler(void ** data)
{
  *data = dbgerr_callback_data;
  return dbgerr_callback;
}

/*!
  \relates cc_debugerror
*/

static void
cc_debugerror_internal_post(const char * source, const std::string & msg,
                            CC_DEBUGERROR_SEVERITY sev, const char * type)
{
  cc_debugerror deberr;

  cc_debugerror_init(&deberr);

  deberr.severity = sev;
  cc_error_set_debug_string(reinterpret_cast<cc_error *>(&deberr), "Coin ");
  cc_error_append_to_debug_string(reinterpret_cast<cc_error *>(&deberr), type);
  cc_error_append_to_debug_string(reinterpret_cast<cc_error *>(&deberr), " in ");
  cc_error_append_to_debug_string(reinterpret_cast<cc_error *>(&deberr), source);
  cc_error_append_to_debug_string(reinterpret_cast<cc_error *>(&deberr), "(): ");
  cc_error_append_to_debug_string(reinterpret_cast<cc_error *>(&deberr), msg.c_str());

  if (dbgerr_callback != reinterpret_cast<cc_debugerror_cb *>(cc_error_default_handler_cb)) {
    dbgerr_callback(&deberr, dbgerr_callback_data);
  }
  else {
    cc_error_handle(reinterpret_cast<cc_error *>(&deberr));
  }

  /* FIXME: port to C. 20020524 mortene. */
  /* check_breakpoints(source);*/

  cc_debugerror_clean(&deberr);
}

/*!
  A macro to simplify posting of debug error messages
*/

#define CC_DEBUGERROR_POST(SEVERITY, TYPE) \
  va_list args; \
  va_start(args, format); \
 \
  /* Use modern C++17 vsnprintf approach */ \
  va_list args_copy; \
  va_copy(args_copy, args); \
  int size = std::vsnprintf(nullptr, 0, format, args) + 1; \
  if (size > 0) { \
    std::string formatted_string(size, '\0'); \
    std::vsnprintf(&formatted_string[0], size, format, args_copy); \
    formatted_string.resize(size - 1); \
    cc_debugerror_internal_post(source, formatted_string, SEVERITY, TYPE); \
  } \
  va_end(args_copy); \
  va_end(args)

/*!
  \relates cc_debugerror
*/

void
cc_debugerror_post(const char * source, const char * format, ...)
{
  CC_DEBUGERROR_POST(CC_DEBUGERROR_ERROR, "error");
}

/*!
  \relates cc_debugerror
*/

void
cc_debugerror_postwarning(const char * source, const char * format, ...)
{
  CC_DEBUGERROR_POST(CC_DEBUGERROR_WARNING, "warning");
}

/*!
  \relates cc_debugerror
*/

void
cc_debugerror_postinfo(const char * source, const char * format, ...)
{
  CC_DEBUGERROR_POST(CC_DEBUGERROR_INFO, "info");
}

#undef CC_DEBUGERROR_POST
