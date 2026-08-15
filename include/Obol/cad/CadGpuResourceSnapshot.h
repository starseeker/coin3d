/**************************************************************************\
 * Copyright (c) 2026
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
\**************************************************************************/

#ifndef OBOL_CAD_CADGPURESOURCESNAPSHOT_H
#define OBOL_CAD_CADGPURESOURCESNAPSHOT_H

#include <cstddef>
#include <cstdint>

namespace Obol {

/**
 * Exact, constant-time accounting for buffers owned by one CAD assembly in
 * one OpenGL context.  The values intentionally describe only allocations
 * Obol owns and can reclaim; textures, driver-private storage, and unrelated
 * Coin resources are outside this contract.
 *
 * A renderer publishes a new snapshot only after a complete frame.  An
 * interrupted frame therefore cannot expose a half-maintained atlas as the
 * basis for a scene-level memory decision.
 */
struct CadGpuResourceSnapshot {
    uint64_t frameSerial = 0;
    size_t trackedBufferBytes = 0;
    size_t ordinaryPartBufferBytes = 0;
    size_t progressiveCutBufferBytes = 0;
    size_t progressiveActiveCutBytes = 0;
    size_t batchBufferBytes = 0;
    size_t triangleAtlasAllocatedBytes = 0;
    size_t triangleAtlasLiveBytes = 0;
    size_t triangleAtlasBudgetBytes = 0;
    size_t triangleAtlasPartCount = 0;
    size_t triangleAtlasPageCount = 0;
    /** Cumulative producer geometry bytes in complete ordinary VBO uploads. */
    uint64_t ordinaryPartFullUploadBytes = 0;
    /** Cumulative producer geometry bytes in ordinary progressive tails. */
    uint64_t ordinaryPartSuffixUploadBytes = 0;
    /** Cumulative retained-prefix bytes copied within the GPU during growth. */
    uint64_t ordinaryPartGpuCopyBytes = 0;
    /** Ordinary VBO generation swaps which reused a certified prefix. */
    uint64_t ordinaryPartLineageReuseCount = 0;
    /** Ordinary VBO generation swaps which explicitly changed lineage. */
    uint64_t ordinaryPartLineageReplacementCount = 0;
    /** Cumulative producer geometry bytes in complete atlas populations. */
    uint64_t triangleAtlasFullUploadBytes = 0;
    /** Cumulative producer geometry bytes in append-only progressive tails. */
    uint64_t triangleAtlasSuffixUploadBytes = 0;
    /** Number of immutable-generation swaps which reused a certified prefix. */
    uint64_t triangleAtlasLineageReuseCount = 0;
    size_t pressureProxyCount = 0;
    uint64_t progressiveEvictionCount = 0;
    uint64_t triangleAtlasReclamationCount = 0;
    bool atlasAdmissionPressure = false;
};

} // namespace Obol

#endif // OBOL_CAD_CADGPURESOURCESNAPSHOT_H
