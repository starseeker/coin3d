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

#include <obol/cad/CadIds.h>

#include <cstring>

namespace obol {

// ---------------------------------------------------------------------------
// CadIdBuilder – FNV-1a 128-bit implementation
// ---------------------------------------------------------------------------
// We implement a 128-bit FNV-1a hash.  The 128-bit multiply is decomposed
// into 64-bit operations to avoid __int128 dependency.

void
CadIdBuilder::fnv1a128_update(uint64_t& hi, uint64_t& lo, uint8_t byte) noexcept
{
    // XOR the low 64 bits with the byte
    lo ^= static_cast<uint64_t>(byte);

    // 128-bit multiply by FNV prime
    // prime = kFNV128_PrimeHi * 2^64 + kFNV128_PrimeLo
    // We compute (hi:lo) * prime mod 2^128 using schoolbook 64×128 mult.
    //
    // (hi:lo) * (pH:pL) where pH = kFNV128_PrimeHi, pL = kFNV128_PrimeLo
    //   = lo*pL  (low 64 bits go to result_lo, upper go to carry)
    //   + hi*pL  << 64
    //   + lo*pH  << 64
    //   + hi*pH  << 128  (dropped — mod 2^128)
    //
    // Since pH is tiny (= 0x0000000001000000) we can use plain 64-bit:
    //   lo*pL : compute 128-bit product via __uint128_t or manual split
    //   Then fold hi*pL + lo*pH into the upper 64 bits.

    // Compute lo * pL → 128 bit result; keep both halves
    const uint64_t pL = kFNV128_PrimeLo;
    const uint64_t pH = kFNV128_PrimeHi;

    // Split lo into 32-bit halves for multiplication
    uint64_t lo_hi = lo >> 32;
    uint64_t lo_lo = lo & 0xFFFFFFFFULL;

    // lo * pL: (lo_hi * pL) << 32 + lo_lo * pL
    uint64_t mid  = lo_hi * pL;
    uint64_t low  = lo_lo * pL;
    uint64_t carry = (low >> 32) + (mid & 0xFFFFFFFFULL);
    uint64_t new_lo = (low & 0xFFFFFFFFULL) | (carry << 32);
    uint64_t new_hi = (mid >> 32) + (carry >> 32);

    // Add hi*pL + lo*pH to the upper 64 bits (all go into new_hi, overflow dropped)
    new_hi += hi * pL;
    new_hi += lo * pH;

    lo = new_lo;
    hi = new_hi;
}

CadId128
CadIdBuilder::hash128(const uint8_t* bytes, size_t length) noexcept
{
    uint64_t hi = kFNV128_OffsetBasisHi;
    uint64_t lo = kFNV128_OffsetBasisLo;
    for (size_t i = 0; i < length; ++i) {
        fnv1a128_update(hi, lo, bytes[i]);
    }
    CadId128 id;
    id.w0 = hi;
    id.w1 = lo;
    return id;
}

CadId128
CadIdBuilder::extend(CadId128 parent, const uint8_t* stepBytes, size_t length) noexcept
{
    // Start from the parent's state rather than the FNV offset basis.
    // This chains parent → child deterministically.
    uint64_t hi = parent.w0;
    uint64_t lo = parent.w1;
    // Guard against a zero parent (root): use offset basis instead
    if (hi == 0 && lo == 0) {
        hi = kFNV128_OffsetBasisHi;
        lo = kFNV128_OffsetBasisLo;
    }
    for (size_t i = 0; i < length; ++i) {
        fnv1a128_update(hi, lo, stepBytes[i]);
    }
    CadId128 id;
    id.w0 = hi;
    id.w1 = lo;
    return id;
}

CadId128
CadIdBuilder::extendNameOccBool(CadId128     parent,
                                const std::string& childName,
                                uint32_t     occurrenceIndex,
                                uint8_t      boolOp) noexcept
{
    // Pack step data: name bytes + NUL separator + occurrence (4 bytes LE) + boolOp
    // Using a fixed-format encoding ensures determinism independent of endianness.
    const size_t nameLen = childName.size();
    // 4 bytes for occurrence, 1 byte boolOp, 1 byte NUL separator
    const size_t totalLen = nameLen + 1 + 4 + 1;
    // Use a stack buffer for common short names; heap for long ones
    uint8_t  stack_buf[256];
    uint8_t* buf = totalLen <= sizeof(stack_buf) ? stack_buf : new uint8_t[totalLen];

    size_t pos = 0;
    std::memcpy(buf + pos, childName.data(), nameLen);
    pos += nameLen;
    buf[pos++] = 0x00;  // NUL separator between name and numeric fields
    // occurrence index, little-endian
    buf[pos++] = static_cast<uint8_t>( occurrenceIndex        & 0xFF);
    buf[pos++] = static_cast<uint8_t>((occurrenceIndex >>  8) & 0xFF);
    buf[pos++] = static_cast<uint8_t>((occurrenceIndex >> 16) & 0xFF);
    buf[pos++] = static_cast<uint8_t>((occurrenceIndex >> 24) & 0xFF);
    buf[pos++] = boolOp;

    CadId128 result = extend(parent, buf, pos);
    if (buf != stack_buf) {
        delete[] buf;
    }
    return result;
}

} // namespace obol
