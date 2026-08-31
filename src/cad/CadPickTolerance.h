#ifndef OBOL_CAD_PICK_TOLERANCE_H
#define OBOL_CAD_PICK_TOLERANCE_H

#include <Inventor/SbBox3f.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbViewVolume.h>

namespace Obol {
namespace internal {

/** Convert a screen-space edge radius to conservative assembly-local units. */
float cadEdgePickTolerance(
    const SbViewVolume& viewVolume,
    const SbVec2s& viewportPixels,
    const SbBox3f& assemblyBounds,
    const SbMatrix& assemblyModel,
    float tolerancePixels) noexcept;

} // namespace internal
} // namespace Obol

#endif // OBOL_CAD_PICK_TOLERANCE_H
