#ifndef OBOL_CAD_WIRE_SOURCE_H
#define OBOL_CAD_WIRE_SOURCE_H

#include <Obol/cad/CadGeometry.h>

#include <cstdint>

namespace Obol {
namespace internal {

/* Wire geometry has one effective progressive source: its own segment table,
 * or the admitted triangle snapshot from which its edges are derived.  Keep
 * this distinction centralized so plan invalidation and GPU residency cannot
 * disagree about whether a cut may grow without a part-generation change. */
inline bool
cadWireSourceIsProgressive(const Obol::WireRep& wire) noexcept
{
    const Obol::TriMesh *triangles = wire.triangleEdges();
    return triangles ? triangles->isProgressive() : wire.isProgressive();
}

inline uint64_t
cadWireSourceProgressiveLineage(const Obol::WireRep& wire) noexcept
{
    const Obol::TriMesh *triangles = wire.triangleEdges();
    if (triangles)
        return triangles->isProgressive() ? triangles->progressiveLineage : 0u;
    return wire.isProgressive() ? wire.progressiveLineage : 0u;
}

} // namespace internal
} // namespace Obol

#endif // OBOL_CAD_WIRE_SOURCE_H
