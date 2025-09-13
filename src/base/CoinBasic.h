#ifndef COIN_BASIC_INTERNAL_H
#define COIN_BASIC_INTERNAL_H

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
 * \file CoinBasic.h
 * \brief C++17 replacement for Inventor/C/basic.h
 * 
 * This file provides C++17-based replacements for the basic types,
 * constants, and macros that were defined in the legacy C API.
 * This is for internal use only and maintains API compatibility
 * through the public SbBasic.h header.
 */

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <algorithm>

// For CMake-generated version info - this will be included
// through the build system
// #include "config.h" // Will be added through build system

// ===== LEGACY TYPE COMPATIBILITY =====

// C++17 modernization: Use native bool for SbBool
// This aligns internal code with the modernized public API
using SbBool = bool;

// Note: FALSE and TRUE constants are already defined in the C basic.h header
// No need to redefine them here to avoid conflicts

// Unique ID type - already C++17 compatible
#ifndef COIN_UNIQUE_ID_UINT32
using SbUniqueId = std::uint64_t;
#else
using SbUniqueId = std::uint32_t;
#endif

// ===== C++17 REPLACEMENTS FOR LEGACY MACROS =====

// But provide C++17 template versions for new code
namespace CoinInternal {
    template<typename T>
    constexpr const T& coin_min(const T& a, const T& b) noexcept {
        return std::min(a, b);
    }
    
    template<typename T> 
    constexpr const T& coin_max(const T& a, const T& b) noexcept {
        return std::max(a, b);
    }
}

// ===== MATHEMATICAL CONSTANTS =====

// C++17 compatible math constants
// Note: These maintain the exact same names as the original for compatibility
#ifndef M_E
constexpr double M_E = 2.7182818284590452354;
#endif
#ifndef M_LOG2E
constexpr double M_LOG2E = 1.4426950408889634074;
#endif
#ifndef M_LOG10E
constexpr double M_LOG10E = 0.43429448190325182765;
#endif
#ifndef M_LN2
constexpr double M_LN2 = 0.69314718055994530942;
#endif
#ifndef M_LN10
constexpr double M_LN10 = 2.30258509299404568402;
#endif
#ifndef M_PI
constexpr double M_PI = 3.14159265358979323846;
#endif
#ifndef M_TWOPI
constexpr double M_TWOPI = (M_PI * 2.0);
#endif
#ifndef M_PI_2
constexpr double M_PI_2 = 1.57079632679489661923;
#endif
#ifndef M_PI_4
constexpr double M_PI_4 = 0.78539816339744830962;
#endif
#ifndef M_3PI_4
constexpr double M_3PI_4 = 2.3561944901923448370E0;
#endif
#ifndef M_SQRTPI
constexpr double M_SQRTPI = 1.77245385090551602792981;
#endif
#ifndef M_1_PI
constexpr double M_1_PI = 0.31830988618379067154;
#endif
#ifndef M_2_PI
constexpr double M_2_PI = 0.63661977236758134308;
#endif
#ifndef M_2_SQRTPI
constexpr double M_2_SQRTPI = 1.12837916709551257390;
#endif
#ifndef M_SQRT2
constexpr double M_SQRT2 = 1.41421356237309504880;
#endif
#ifndef M_SQRT1_2
constexpr double M_SQRT1_2 = 0.70710678118654752440;
#endif
#ifndef M_LN2LO
constexpr double M_LN2LO = 1.9082149292705877000E-10;
#endif
#ifndef M_LN2HI
constexpr double M_LN2HI = 6.9314718036912381649E-1;
#endif
#ifndef M_SQRT3
constexpr double M_SQRT3 = 1.73205080756887719000;
#endif
#ifndef M_IVLN10
constexpr double M_IVLN10 = 0.43429448190325182765; /* 1 / log(10) */
#endif
#ifndef M_LOG2_E
constexpr double M_LOG2_E = 0.693147180559945309417;
#endif
#ifndef M_INVLN2
constexpr double M_INVLN2 = 1.4426950408889633870E0; /* 1 / log(2) */
#endif

// ===== COMPILER AND PLATFORM COMPATIBILITY =====

// Library identification
#define __COIN__

// Macro utilities for string handling - keep these as-is for compatibility
// Most compilers should have "hash quoting", as it is part of the ANSI standard.
#ifndef HAVE_HASH_QUOTING
#define HAVE_HASH_QUOTING 1
#endif

#ifdef HAVE_HASH_QUOTING
#define SO__QUOTE(str)           #str
#define SO__CONCAT(str1, str2)   str1##str2
#elif defined(HAVE_APOSTROPHES_QUOTING)
#define SO__QUOTE(str)           "str"
#define SO__CONCAT(str1, str2)   str1/**/str2
#else
#error No valid way to do macro quoting!
#endif

// ===== DLL/EXPORT MACROS =====

// These need to be maintained for Windows DLL support
// Keep the same logic as the original for compatibility
// Only define if not already defined to avoid conflicts

#ifndef COIN_DLL_API
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
# ifdef COIN_INTERNAL
#  ifdef COIN_MAKE_DLL
#   define COIN_DLL_API __declspec(dllexport)
#  endif /* COIN_MAKE_DLL */
# else /* !COIN_INTERNAL */
#  ifdef COIN_DLL
#   ifdef COIN_NOT_DLL
#     error Define _either_ COIN_DLL _or_ COIN_NOT_DLL as appropriate for your linkage!
#   endif /* COIN_NOT_DLL */
#   define COIN_DLL_API __declspec(dllimport)
#  else /* !COIN_DLL */
#   ifndef COIN_NOT_DLL
#    error Define either COIN_DLL or COIN_NOT_DLL as appropriate for your linkage!
#   endif /* !COIN_NOT_DLL */
#  endif /* !COIN_DLL */
# endif /* !COIN_INTERNAL */
#endif /* Microsoft Windows */

// Empty define for non-Windows platforms
#ifndef COIN_DLL_API
# define COIN_DLL_API
#endif /* !COIN_DLL_API */
#endif /* !COIN_DLL_API */

#endif // COIN_BASIC_INTERNAL_H
