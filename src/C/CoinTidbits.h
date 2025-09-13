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
 * \brief C++17 replacement for Inventor/C/tidbits.h
 * 
 * This file provides C++17-based replacements for the utility functions
 * from the legacy C tidbits API, using standard library features and
 * modern C++ practices.
 */

// Minimal includes to avoid conflicts
#include <cstdint>
#include <cstdarg>
#include <cstdio>  // For FILE*

// Forward declaration for compatibility
#ifndef _WIN32
#include <unistd.h>
#endif

// Use the existing SbBool definition to maintain compatibility
#ifndef SBBOOL_HEADER_FILE
typedef bool SbBool;
#endif

// ===== C API COMPATIBILITY LAYER =====

extern "C" {

// Legacy C types for compatibility
typedef void coin_atexit_f(void);

// The coin_atexit_priorities enum and coin_atexit macro are defined in tidbitsp.h
// which includes this header. This avoids conflicts when both headers are included.

// C API functions that wrap the C++ implementation
int coin_host_get_endianness(void);
int coin_snprintf(char* dst, unsigned int n, const char* fmtstr, ...);
int coin_vsnprintf(char* dst, unsigned int n, const char* fmtstr, va_list args);

const char* coin_getenv(const char* name);
SbBool coin_setenv(const char* name, const char* value, int overwrite);
void coin_unsetenv(const char* name);

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

SbBool coin_is_power_of_two(std::uint32_t x);
std::uint32_t coin_next_power_of_two(std::uint32_t x);
std::uint32_t coin_geq_power_of_two(std::uint32_t x);

void coin_viewvolume_jitter(int numpasses, int curpass, const int* vpsize, float* jitter);

void cc_coin_atexit(coin_atexit_f* fp);
void cc_coin_atexit_static_internal(coin_atexit_f* fp);

// These functions are implemented by CoinTidbits.cpp but declared in tidbitsp.h:
// - void coin_atexit_func(const char* name, coin_atexit_f* fp, coin_atexit_priorities priority);
// - void coin_atexit_cleanup(void);
// - SbBool coin_is_exiting(void);
// - coin_atexit macro and coin_atexit_priorities enum

// Additional utility functions from tidbitsp.h
FILE* coin_get_stdin(void);
FILE* coin_get_stdout(void);
FILE* coin_get_stderr(void);

// Locale functions (forward declarations, avoid cc_string dependency)
struct cc_string;
SbBool coin_locale_set_portable(struct cc_string* storeold);
void coin_locale_reset(struct cc_string* storedold);
double coin_atof(const char* ptr);

// ASCII85 encoding functions
void coin_output_ascii85(FILE* fp, const unsigned char val, unsigned char* tuple,
                        unsigned char* linebuf, int* tuplecnt, int* linecnt,
                        const int rowlen, const SbBool flush);
void coin_flush_ascii85(FILE* fp, unsigned char* tuple, unsigned char* linebuf,
                       int* tuplecnt, int* linecnt, const int rowlen);

// Version parsing
SbBool coin_parse_versionstring(const char* versionstr, int* major, int* minor, int* patch);

// Utility functions
SbBool coin_getcwd(struct cc_string* str);
int coin_isinf(double value);
int coin_isnan(double value);
int coin_finite(double value);
unsigned long coin_geq_prime_number(unsigned long num);

// OS detection (enum defined in tidbitsp.h)
int coin_runtime_os(void);

#define COIN_MAC_FRAMEWORK_IDENTIFIER_CSTRING ("org.coin3d.Coin.framework")

int coin_debug_caching_level(void);
int coin_debug_extra(void);
int coin_debug_normalize(void);

} // extern "C"

#endif // COIN_TIDBITS_INTERNAL_H