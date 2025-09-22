#ifndef COIN_INTERNAL_ERROR_H
#define COIN_INTERNAL_ERROR_H

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

/*!
 * \file CoinInternalError.h
 * \brief Internal error handling API consolidating C and C++ error interfaces
 * 
 * This file consolidates the error handling functionality that was previously
 * exposed as public C API in include/Inventor/C/errors/. The functionality
 * is now internal implementation detail only.
 * 
 * The implementation can leverage the modern C++17 CoinDebugError system
 * but provides a clean C API interface for legacy code compatibility.
 */

/* ********************************************************************** */

#include <Inventor/C/base/string.h>
#include <string>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ********************************************************************** */
/* Base error structures and functions */
/* ********************************************************************** */

#ifndef CC_ERROR_STRUCT_DEFINED
#define CC_ERROR_STRUCT_DEFINED

typedef struct cc_error {
  std::string debugstring;
} cc_error;

typedef void cc_error_cb(const cc_error * err, void * data);

#endif /* CC_ERROR_STRUCT_DEFINED */

/* Basic error functions */
COIN_DLL_API void cc_error_init(cc_error * me);
COIN_DLL_API void cc_error_clean(cc_error * me);
COIN_DLL_API void cc_error_copy(const cc_error * src, cc_error * dst);

  /*   const SbString & getDebugString(void) const; */
COIN_DLL_API const std::string * cc_error_get_debug_string(const cc_error * me);
COIN_DLL_API void cc_error_set_debug_string(cc_error * me, const char * str);
COIN_DLL_API void cc_error_append_to_debug_string(cc_error * me, const char * str);

/*   static void setHandlerCallback(SoErrorCB * const func, void * const data); */

COIN_DLL_API void cc_error_set_handler_callback(cc_error_cb * func, void * data);
COIN_DLL_API cc_error_cb * cc_error_get_handler_callback(void);
COIN_DLL_API void * cc_error_get_handler_data(void);

COIN_DLL_API void cc_error_post(const char * format, ...);
COIN_DLL_API void cc_error_post_arglist(const char * format, va_list args);

COIN_DLL_API void cc_error_handle(cc_error * me);
COIN_DLL_API cc_error_cb * cc_error_get_handler(void ** data);
COIN_DLL_API void cc_error_default_handler_cb(const cc_error * err, void * data);

/* ********************************************************************** */
/* Debug error structures and functions */
/* ********************************************************************** */

#ifndef CC_DEBUGERROR_STRUCT_DEFINED
#define CC_DEBUGERROR_STRUCT_DEFINED

typedef enum CC_DEBUGERROR_SEVERITY {
  CC_DEBUGERROR_ERROR,
  CC_DEBUGERROR_WARNING,
  CC_DEBUGERROR_INFO
} CC_DEBUGERROR_SEVERITY;

typedef struct cc_debugerror {
  cc_error super; /* make struct is-A cc_error */
  CC_DEBUGERROR_SEVERITY severity;
} cc_debugerror;

#endif /* CC_DEBUGERROR_STRUCT_DEFINED */

typedef void cc_debugerror_cb(const cc_debugerror * err, void * data);

/* Debug error functions */
COIN_DLL_API void cc_debugerror_post(const char * source, const char * format, ...);
COIN_DLL_API void cc_debugerror_postwarning(const char * source, const char * format, ...);
COIN_DLL_API void cc_debugerror_postinfo(const char * source, const char * format, ...);

COIN_DLL_API void cc_debugerror_init(cc_debugerror * me);
COIN_DLL_API void cc_debugerror_clean(cc_debugerror * me);

COIN_DLL_API CC_DEBUGERROR_SEVERITY cc_debugerror_get_severity(const cc_debugerror * me);

COIN_DLL_API void cc_debugerror_set_handler_callback(cc_debugerror_cb * function, void * data);
COIN_DLL_API cc_debugerror_cb * cc_debugerror_get_handler_callback(void);
COIN_DLL_API void * cc_debugerror_get_handler_data(void);
COIN_DLL_API cc_debugerror_cb * cc_debugerror_get_handler(void ** data);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* ! COIN_INTERNAL_ERROR_H */
