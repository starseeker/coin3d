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
#include <mutex>
#include <string>

namespace {

std::mutex cc_debugerror_handler_mutex;
cc_debugerror_cb * cc_debugerror_callback = NULL;
void * cc_debugerror_callback_data = NULL;

void
cc_debugerror_post_arglist(CC_DEBUGERROR_SEVERITY severity,
                           const char * source,
                           const char * format,
                           va_list args)
{
  va_list copy;
  va_copy(copy, args);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
  const int required = std::vsnprintf(NULL, 0, format, copy);
  va_end(copy);
  if (required < 0) return;

  std::string message(static_cast<size_t>(required) + 1, '\0');
  (void)std::vsnprintf(message.data(), message.size(), format, args);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
  message.resize(static_cast<size_t>(required));

  const char * kind = severity == CC_DEBUGERROR_ERROR ? "error" :
                       severity == CC_DEBUGERROR_WARNING ? "warning" : "info";
  std::string debugstring = "Coin ";
  debugstring += kind;
  if (source && source[0]) {
    debugstring += " in ";
    debugstring += source;
    debugstring += "()";
  }
  debugstring += ": ";
  debugstring += message;

  cc_debugerror error;
  cc_debugerror_init(&error);
  error.severity = severity;
  cc_error_set_debug_string(&error.super, debugstring.c_str());

  void * data = NULL;
  cc_debugerror_cb * function = cc_debugerror_get_handler(&data);
  if (function) {
    function(&error, data);
  }
  else {
    cc_error_handle(&error.super);
  }
  cc_debugerror_clean(&error);
}

} // namespace

void
cc_debugerror_post(const char * source, const char * format, ...)
{
  va_list args;
  va_start(args, format);
  cc_debugerror_post_arglist(CC_DEBUGERROR_ERROR, source, format, args);
  va_end(args);
}

void
cc_debugerror_postwarning(const char * source, const char * format, ...)
{
  va_list args;
  va_start(args, format);
  cc_debugerror_post_arglist(CC_DEBUGERROR_WARNING, source, format, args);
  va_end(args);
}

void
cc_debugerror_postinfo(const char * source, const char * format, ...)
{
  va_list args;
  va_start(args, format);
  cc_debugerror_post_arglist(CC_DEBUGERROR_INFO, source, format, args);
  va_end(args);
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

void
cc_debugerror_set_handler_callback(cc_debugerror_cb * function, void * data)
{
  const std::lock_guard<std::mutex> guard(cc_debugerror_handler_mutex);
  cc_debugerror_callback = function;
  cc_debugerror_callback_data = data;
}

cc_debugerror_cb *
cc_debugerror_get_handler_callback(void)
{
  const std::lock_guard<std::mutex> guard(cc_debugerror_handler_mutex);
  return cc_debugerror_callback;
}

void *
cc_debugerror_get_handler_data(void)
{
  const std::lock_guard<std::mutex> guard(cc_debugerror_handler_mutex);
  return cc_debugerror_callback_data;
}

cc_debugerror_cb *
cc_debugerror_get_handler(void ** data)
{
  const std::lock_guard<std::mutex> guard(cc_debugerror_handler_mutex);
  *data = cc_debugerror_callback_data;
  return cc_debugerror_callback;
}
