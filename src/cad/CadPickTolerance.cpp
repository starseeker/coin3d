#include "CadPickTolerance.h"

#include <algorithm>
#include <cmath>

namespace Obol {
namespace internal {

float
cadEdgePickTolerance(
        const SbViewVolume& viewVolume,
        const SbVec2s& viewportPixels,
        const SbBox3f& assemblyBounds,
        const SbMatrix& assemblyModel,
        float tolerancePixels) noexcept
{
    const float safePixels = std::isfinite(tolerancePixels) &&
        tolerancePixels >= 0.0f ? tolerancePixels : 0.0f;
    const float fallback = safePixels * 0.01f;
    if (safePixels != tolerancePixels ||
            viewportPixels[0] <= 0 || viewportPixels[1] <= 0)
        return fallback;

    float depthScale = 1.0f;
    if (viewVolume.getProjectionType() == SbViewVolume::PERSPECTIVE) {
        const float nearDistance = viewVolume.getNearDist();
        if (!(nearDistance > 0.0f) || !std::isfinite(nearDistance) ||
                assemblyBounds.isEmpty())
            return fallback;
        SbVec3f center;
        assemblyModel.multVecMatrix(assemblyBounds.getCenter(), center);
        SbVec3f direction = viewVolume.getProjectionDirection();
        if (direction.normalize() == 0.0f)
            return fallback;
        const float distance =
            (center - viewVolume.getProjectionPoint()).dot(direction);
        if (!std::isfinite(distance))
            return fallback;
        depthScale = std::max(nearDistance, distance) / nearDistance;
    }

    const float pixelWorldX = viewVolume.getWidth() * depthScale /
        static_cast<float>(viewportPixels[0]);
    const float pixelWorldY = viewVolume.getHeight() * depthScale /
        static_cast<float>(viewportPixels[1]);
    if (!(pixelWorldX > 0.0f) || !(pixelWorldY > 0.0f) ||
            !std::isfinite(pixelWorldX) || !std::isfinite(pixelWorldY))
        return fallback;

    SbVec3f up = viewVolume.getViewUp();
    SbVec3f forward = viewVolume.getProjectionDirection();
    if (up.normalize() == 0.0f || forward.normalize() == 0.0f)
        return fallback;
    SbVec3f right = forward.cross(up);
    if (right.normalize() == 0.0f)
        return fallback;

    const SbMatrix worldToAssembly = assemblyModel.inverse();
    SbVec3f localUp;
    SbVec3f localRight;
    worldToAssembly.multDirMatrix(up, localUp);
    worldToAssembly.multDirMatrix(right, localRight);
    const float localPixelX = pixelWorldX * localRight.length();
    const float localPixelY = pixelWorldY * localUp.length();
    const float localPixel = std::max(localPixelX, localPixelY);
    return localPixel > 0.0f && std::isfinite(localPixel) ?
        safePixels * localPixel : fallback;
}

} // namespace internal
} // namespace Obol
