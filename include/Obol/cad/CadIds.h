#ifndef OBOL_CADIDS_H
#define OBOL_CADIDS_H

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

/**
 * @file CadIds.h
 * @brief Domain-safe 128-bit stable IDs for CAD parts and instances.
 *
 * IDs are generated deterministically from a hierarchical path of
 * (name, occurrenceIndex, boolOp) tuples, mirroring BRL-CAD comb-tree
 * traversal but with no BRL-CAD dependency.
 *
 * Hash algorithm: FNV-1a 128-bit variant.
 *
 * Thread safety: CadIdBuilder is stateless; all methods are pure functions
 * that can be called from multiple threads without synchronisation.
 */

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>

namespace Obol {

// ---------------------------------------------------------------------------
// Domain-safe opaque 128-bit identifiers
// ---------------------------------------------------------------------------

/**
 * @brief 128-bit identifier whose domain is part of its C++ type.
 *
 * Comparison is bitwise; hash folding is provided via std::hash.
 * The zero value is invalid for PartId and is the intentional root sentinel
 * for InstanceId.  Use CadIdBuilder::rootInstance() where that distinction
 * matters rather than treating every zero ID as an ordinary entity.
 */
template <typename Domain>
struct CadId {
    uint64_t w0 = 0;
    uint64_t w1 = 0;

    bool operator==(const CadId& o) const noexcept { return w0 == o.w0 && w1 == o.w1; }
    bool operator!=(const CadId& o) const noexcept { return !(*this == o); }
    bool operator< (const CadId& o) const noexcept {
        return w0 < o.w0 || (w0 == o.w0 && w1 < o.w1);
    }

    /** Returns true for a valid (non-zero) ID. */
    bool isValid() const noexcept { return w0 != 0 || w1 != 0; }
};

struct PartIdDomain;
struct InstanceIdDomain;

using PartId = CadId<PartIdDomain>;
using InstanceId = CadId<InstanceIdDomain>;

// ---------------------------------------------------------------------------
// CadIdBuilder – deterministic ID factory
// ---------------------------------------------------------------------------

/**
 * @brief Stateless factory for building deterministic domain-safe IDs.
 *
 * All methods are static.  The hash primitive is FNV-1a 128-bit.  The
 * design mirrors the BRL-CAD below_path_hash_root + below_path_hash_extend
 * pattern but has no BRL-CAD dependency.
 *
 * ### Usage example
 * @code
 *   using namespace Obol;
 *   InstanceId root = CadIdBuilder::rootInstance();
 *   InstanceId child = CadIdBuilder::childInstance(root, "wheel", 0, 0);
 *   InstanceId grandChild = CadIdBuilder::childInstance(child, "bolt", 2, 0);
 * @endcode
 */
class CadIdBuilder {
public:
    /** The root / parent-of-top-level-components sentinel ID (all-zeros). */
    static InstanceId rootInstance() noexcept { return InstanceId{}; }

    /**
     * @brief Hash an arbitrary byte buffer into a PartId.
     *
     * Suitable for producing a PartId from a stable byte key (e.g. a UUID,
     * a BRL-CAD database hash, or a canonical path string).
     */
    static PartId partId(const uint8_t* bytes, size_t length) noexcept;

    /** Convenience overload for std::string keys. */
    static PartId partId(const std::string& key) noexcept {
        return partId(reinterpret_cast<const uint8_t*>(key.data()), key.size());
    }

    /**
     * @brief Hash an application key into a top-level InstanceId.
     */
    static InstanceId instanceId(
        const uint8_t* bytes, size_t length) noexcept;

    static InstanceId instanceId(const std::string& key) noexcept {
        return instanceId(
            reinterpret_cast<const uint8_t*>(key.data()), key.size());
    }

    /**
     * @brief Extend a parent InstanceId using (childName, occurrenceIndex, boolOp).
     *
     * This is the primary function for building instance IDs that mirror a
     * CAD comb-tree traversal where no stable per-node GUID is available.
     *
     * @param parent          Parent InstanceId (use rootInstance() at top level).
     * @param childName       Name string of the child comb/primitive.
     * @param occurrenceIndex Zero-based index disambiguating sibling references
     *                        to the same child name under the same parent.
     * @param boolOp          Boolean operation code (0 = union, 1 = subtract,
     *                        2 = intersect); included so that two children with
     *                        the same name and occurrence but different boolOp
     *                        yield different IDs.
     */
    static InstanceId childInstance(InstanceId parent,
                                    const std::string& childName,
                                    uint32_t occurrenceIndex,
                                    uint8_t boolOp) noexcept;

private:
    // FNV-1a 128-bit constants
    static constexpr uint64_t kFNV128_OffsetBasisHi = 0x6c62272e07bb0142ULL;
    static constexpr uint64_t kFNV128_OffsetBasisLo = 0x62b821756295c58dULL;
    static constexpr uint64_t kFNV128_PrimeHi       = 0x0000000001000000ULL;
    static constexpr uint64_t kFNV128_PrimeLo       = 0x000000000000013bULL;

    static void fnv1a128_update(uint64_t& hi, uint64_t& lo, uint8_t byte) noexcept;
    static void hashWords(const uint8_t* bytes, size_t length,
                          uint64_t& hi, uint64_t& lo) noexcept;
    static InstanceId extendInstance(InstanceId parent,
                                     const uint8_t* stepBytes,
                                     size_t length) noexcept;
};

} // namespace Obol

// ---------------------------------------------------------------------------
// std::hash specialisations
// ---------------------------------------------------------------------------
namespace std {
template<typename Domain>
struct hash<Obol::CadId<Domain>> {
    size_t operator()(const Obol::CadId<Domain>& id) const noexcept {
        // Fold 128 bits into size_t via XOR with a rotation
        uint64_t h = id.w0 ^ (id.w1 * 0x9e3779b97f4a7c15ULL);
        h ^= h >> 30;
        h *= 0xbf58476d1ce4e5b9ULL;
        h ^= h >> 27;
        h *= 0x94d049bb133111ebULL;
        h ^= h >> 31;
        return static_cast<size_t>(h);
    }
};
} // namespace std

#endif // OBOL_CADIDS_H
