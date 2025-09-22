#ifndef COIN_TIDBITS_INTERNAL_H
#define COIN_TIDBITS_INTERNAL_H

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
 * \file CoinTidbits.h
 * \brief Consolidated C++17 utility functions for Coin
 * 
 * This file consolidates all utility functions from the former tidbits files:
 * - include/Inventor/C/tidbits.h 
 * - src/C/tidbits.h
 * - src/tidbitsp.h 
 * - src/tidbits.cpp
 */

#include "Inventor/C/basic.h"
#include <string>
#include <cstdint>
#include <cstdarg>
#include <cstdio>

#ifndef COIN_INTERNAL
#error this is a private header file
#endif

// Forward declare SbBool to avoid header dependencies
#ifndef SBBOOL_HEADER_FILE
typedef bool SbBool;
#define COIN_TRUE true
#define COIN_FALSE false
#else
#define COIN_TRUE TRUE
#define COIN_FALSE FALSE
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* ********************************************************************** */

/* Endianness constants */
enum CoinEndiannessValues {
    COIN_HOST_IS_UNKNOWNENDIAN = -1,
    COIN_HOST_IS_LITTLEENDIAN = 0,
    COIN_HOST_IS_BIGENDIAN = 1
};

/* OS detection constants */
enum CoinOSType {
    COIN_UNIX,
    COIN_OS_X,
    COIN_MSWINDOWS
};

/* Atexit priorities enum */
enum coin_atexit_priorities {
    /* Absolute priorities goes first */
    CC_ATEXIT_EXTERNAL = 2147483647,
    CC_ATEXIT_NORMAL = 0,
    CC_ATEXIT_DYNLIBS = -2147483647,
    
    /* Relative priorities */
    CC_ATEXIT_REALTIME_FIELD = CC_ATEXIT_NORMAL + 10,
    CC_ATEXIT_DRAGGERDEFAULTS = CC_ATEXIT_NORMAL + 2,
    CC_ATEXIT_TRACK_SOBASE_INSTANCES = CC_ATEXIT_NORMAL + 1,
    CC_ATEXIT_NORMAL_LOWPRIORITY = CC_ATEXIT_NORMAL - 1,
    CC_ATEXIT_STATIC_DATA = CC_ATEXIT_NORMAL - 10,
    CC_ATEXIT_SODB = CC_ATEXIT_NORMAL - 20,
    CC_ATEXIT_SOBASE = CC_ATEXIT_NORMAL - 30,
    CC_ATEXIT_SOTYPE = CC_ATEXIT_NORMAL - 40,
    CC_ATEXIT_FONT_SUBSYSTEM = CC_ATEXIT_NORMAL - 100,
    CC_ATEXIT_FONT_SUBSYSTEM_HIGHPRIORITY = CC_ATEXIT_FONT_SUBSYSTEM + 1,
    CC_ATEXIT_FONT_SUBSYSTEM_LOWPRIORITY = CC_ATEXIT_FONT_SUBSYSTEM - 1,
    CC_ATEXIT_MSG_SUBSYSTEM = CC_ATEXIT_NORMAL - 200,
    CC_ATEXIT_SBNAME = CC_ATEXIT_NORMAL - 500,
    CC_ATEXIT_THREADING_SUBSYSTEM = CC_ATEXIT_NORMAL - 1000,
    CC_ATEXIT_THREADING_SUBSYSTEM_LOWPRIORITY = CC_ATEXIT_THREADING_SUBSYSTEM - 1,
    CC_ATEXIT_THREADING_SUBSYSTEM_VERYLOWPRIORITY = CC_ATEXIT_THREADING_SUBSYSTEM - 2,
    CC_ATEXIT_ENVIRONMENT = CC_ATEXIT_DYNLIBS + 10
};

/* Function pointer type for atexit cleanup functions */
typedef void coin_atexit_f(void);

/* ********************************************************************** */
/* Initialization */

void coin_init_tidbits(void);

/* ********************************************************************** */
/* Basic utility functions */

int coin_host_get_endianness(void);

/* ********************************************************************** */
/* Environment variable functions */

const char* coin_getenv(const char* name);
SbBool coin_setenv(const char* name, const char* value, int overwrite);
void coin_unsetenv(const char* name);

/* ********************************************************************** */
/* Network byte order conversion functions */

std::uint16_t coin_hton_uint16(std::uint16_t value);
std::uint16_t coin_ntoh_uint16(std::uint16_t value);
std::uint32_t coin_hton_uint32(std::uint32_t value);
std::uint32_t coin_ntoh_uint32(std::uint32_t value);
std::uint64_t coin_hton_uint64(std::uint64_t value);
std::uint64_t coin_ntoh_uint64(std::uint64_t value);

void coin_hton_float_bytes(float value, char* result);
float coin_ntoh_float_bytes(const char* value);
void coin_hton_double_bytes(double value, char* result);
double coin_ntoh_double_bytes(const char* value);

/* ********************************************************************** */
/* Power of two functions */

SbBool coin_is_power_of_two(std::uint32_t x);
std::uint32_t coin_next_power_of_two(std::uint32_t x);
std::uint32_t coin_geq_power_of_two(std::uint32_t x);

/* ********************************************************************** */
/* Jitter functions */

void coin_viewvolume_jitter(int numpasses, int curpass, const int* vpsize, float* jitter);

/* ********************************************************************** */
/* Atexit functions */

#define coin_atexit(func, priority) \
        coin_atexit_func(SO__QUOTE(func), func, priority)

void coin_atexit_func(const char* name, coin_atexit_f* fp, coin_atexit_priorities priority);
void coin_atexit_cleanup(void);
SbBool coin_is_exiting(void);

void cc_coin_atexit(coin_atexit_f* fp);
void cc_coin_atexit_static_internal(coin_atexit_f* fp);

/* ********************************************************************** */
/* File descriptor functions */

FILE* coin_get_stdin(void);
FILE* coin_get_stdout(void);
FILE* coin_get_stderr(void);

/* ********************************************************************** */
/* Locale functions */

SbBool coin_locale_set_portable(std::string* storeold);
void coin_locale_reset(std::string* storedold);
double coin_atof(const char* ptr);

/* ********************************************************************** */
/* ASCII85 encoding functions */

void coin_output_ascii85(FILE* fp, const unsigned char val, unsigned char* tuple,
                        unsigned char* linebuf, int* tuplecnt, int* linecnt,
                        const int rowlen, const SbBool flush);
void coin_flush_ascii85(FILE* fp, unsigned char* tuple, unsigned char* linebuf,
                       int* tuplecnt, int* linecnt, const int rowlen);

/* ********************************************************************** */
/* Version parsing */

SbBool coin_parse_versionstring(const char* versionstr, int* major, int* minor, int* patch);

/* ********************************************************************** */
/* Utility functions */

SbBool coin_getcwd(std::string* str);
int coin_isinf(double value);
int coin_isnan(double value);
int coin_finite(double value);
unsigned long coin_geq_prime_number(unsigned long num);

/* ********************************************************************** */
/* OS detection */

int coin_runtime_os(void);

#define COIN_MAC_FRAMEWORK_IDENTIFIER_CSTRING ("org.coin3d.Coin.framework")

/* ********************************************************************** */
/* Debug functions */

int coin_debug_caching_level(void);
int coin_debug_extra(void);
int coin_debug_normalize(void);

/* ********************************************************************** */

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* !COIN_TIDBITS_INTERNAL_H */
