#ifndef COIN_SBBASIC_H
#define COIN_SBBASIC_H

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

#include <Inventor/C/basic.h>
#ifndef NDEBUG
#include <Inventor/C/errors/debugerror.h>
#endif // !NDEBUG

// Start migrating to C++17 - internal use only
#ifdef COIN_INTERNAL
#ifndef NDEBUG
#include "errors/CoinDebugError.h"
#endif // !NDEBUG
#endif // COIN_INTERNAL

/* ********************************************************************** */
/* Trap people trying to use Inventor headers while compiling C source code.
 * (we get support mail about this from time to time)
 */
#ifndef __cplusplus
#error You are not compiling C++ - maybe your source file is named <file>.c
#endif

/* ********************************************************************** */

/* Some useful inline template functions:
 *   SbAbs(Val)              - returns absolute value
 *   SbMin(Val1, Val2)       - returns minimum value
 *   SbMax(Val1, Val2)       - returns maximum value
 *   SbClamp(Val, Min, Max)  - returns clamped value
 *   SbSwap(Val1, Val2)      - swaps the two values (no return value)
 *   SbSqr(val)              - returns squared value
 */

template <typename Type>
constexpr Type SbAbs( Type Val ) noexcept {
  return (Val < 0) ? 0 - Val : Val;
}

template <typename Type>
constexpr Type SbMax( const Type A, const Type B ) noexcept {
  return (A < B) ? B : A;
}

template <typename Type>
constexpr Type SbMin( const Type A, const Type B ) noexcept {
  return (A < B) ? A : B;
}

template <typename Type>
constexpr Type SbClamp( const Type Val, const Type Min, const Type Max ) noexcept {
  return (Val < Min) ? Min : (Val > Max) ? Max : Val;
}

template <typename Type>
constexpr void SbSwap( Type & A, Type & B ) noexcept {
  Type T = A; A = B; B = T;
}

template <typename Type>
constexpr Type SbSqr(const Type val) noexcept {
  return val * val;
}

/* *********************************************************************** */

// SbDividerChk() - checks if divide-by-zero is attempted, and emits a
// warning if so for debug builds.  inlined like this to not take much
// screenspace in inline functions.

#ifndef NDEBUG
template <typename Type>
constexpr void SbDividerChk(const char * funcname, Type divider) noexcept {
  if (!(divider != static_cast<Type>(0)))
    cc_debugerror_post(funcname, "divide by zero error.", divider);
}
#else
template <typename Type>
constexpr void SbDividerChk(const char *, Type) noexcept {}
#endif // !NDEBUG

/* ********************************************************************** */

// C++17 modernization: Remove legacy compiler workarounds
// Since we target modern C++17 development, legacy compiler 
// compatibility is no longer needed.

// Modern STATIC_SOTYPE_INIT - always use proper initialization  
#define STATIC_SOTYPE_INIT = SoType::badType()

/* ********************************************************************** */

// Modern do-while wrapper - C++17 compilers handle this correctly
#define WHILE_0 while (0)

#endif /* !COIN_SBBASIC_H */
