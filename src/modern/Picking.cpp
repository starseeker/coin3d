#include <Obol/scene/Picking.h>

#include <Inventor/SbVec2s.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoPickedPoint.h>
#include <Inventor/SoPath.h>
#include <Inventor/actions/SoRayPickAction.h>
#include <Inventor/lists/SoPickedPointList.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Obol/cad/SoCADDetail.h>

#include <cstdlib>
#include <cstring>
#include <memory>

namespace obol {
namespace {

Vec3 toVec3(const SbVec3f & v)
{
    return Vec3{v[0], v[1], v[2]};
}

SbVec3f toSbVec3f(const Vec3 & v)
{
    return SbVec3f(v.x, v.y, v.z);
}

SceneObjectId sceneObjectIdFromPath(const SoPath * path)
{
    if (!path) return InvalidSceneObjectId;

    const char prefix[] = "ObolSceneObject_";
    const size_t prefixLength = sizeof(prefix) - 1;
    for (int i = 0; i < path->getLength(); ++i) {
        const SoNode * node = path->getNode(i);
        if (!node) continue;

        const char * name = node->getName().getString();
        if (!name || std::strncmp(name, prefix, prefixLength) != 0) continue;

        char * end = nullptr;
        const unsigned long value = std::strtoul(name + prefixLength, &end, 10);
        if (end && *end == '\0' && value > 0) {
            return static_cast<SceneObjectId>(value);
        }
    }

    return InvalidSceneObjectId;
}

CadPickPrimitive toCadPickPrimitive(SoCADDetail::PrimType primitive)
{
    switch (primitive) {
    case SoCADDetail::EDGE:
        return CadPickPrimitive::Edge;
    case SoCADDetail::TRIANGLE:
        return CadPickPrimitive::Triangle;
    case SoCADDetail::BOUNDS:
        return CadPickPrimitive::Bounds;
    }
    return CadPickPrimitive::Bounds;
}

std::optional<CadPickDetail> cadDetailFromPickedPoint(const SoPickedPoint & point)
{
    const SoPath * path = point.getPath();
    if (path) {
        for (int i = 0; i < path->getLength(); ++i) {
            const SoNode * node = path->getNode(i);
            const SoCADDetail * detail =
                dynamic_cast<const SoCADDetail *>(point.getDetail(node));
            if (detail) {
                return CadPickDetail{
                    detail->getInstanceId(),
                    detail->getPartId(),
                    toCadPickPrimitive(detail->getPrimType()),
                    detail->getPrimIndex0(),
                    detail->getPrimIndex1(),
                    detail->getU()
                };
            }
        }
    }

    const SoCADDetail * detail =
        dynamic_cast<const SoCADDetail *>(point.getDetail());
    if (!detail) return std::optional<CadPickDetail>{};

    return CadPickDetail{
        detail->getInstanceId(),
        detail->getPartId(),
        toCadPickPrimitive(detail->getPrimType()),
        detail->getPrimIndex0(),
        detail->getPrimIndex1(),
        detail->getU()
    };
}

PickHit toHit(const SoPickedPoint & point)
{
    PickHit hit;
    hit.objectId = sceneObjectIdFromPath(point.getPath());
    hit.point = toVec3(point.getPoint());
    hit.normal = toVec3(point.getNormal());
    hit.materialIndex = point.getMaterialIndex();
    hit.onGeometry = point.isOnGeometry() == TRUE;
    hit.cad = cadDetailFromPickedPoint(point);
    return hit;
}

} // namespace

PickResult
Picker::pick(const Scene & scene, const PickRequest & request)
{
    PickResult result;
    if (request.viewportWidth == 0 || request.viewportHeight == 0) {
        return result;
    }

    std::unique_ptr<SoSeparator, void(*)(SoSeparator *)> root(
        scene.createLegacySceneGraph(),
        [](SoSeparator * node) {
            if (node) node->unref();
        });

    SoRayPickAction action(SbViewportRegion(request.viewportWidth,
                                            request.viewportHeight));
    if (request.useWorldRay) {
        action.setRay(toSbVec3f(request.rayOrigin),
                      toSbVec3f(request.rayDirection),
                      request.nearDistance,
                      request.farDistance);
    } else {
        action.setPoint(SbVec2s(static_cast<short>(request.x),
                                static_cast<short>(request.y)));
    }
    action.setRadius(request.radiusPixels);
    action.setPickAll(request.allHits ? TRUE : FALSE);
    action.apply(root.get());

    if (request.allHits) {
        const SoPickedPointList & list = action.getPickedPointList();
        for (int i = 0; i < list.getLength(); ++i) {
            SoPickedPoint * point = list[i];
            if (point) {
                result.hits.push_back(toHit(*point));
            }
        }
    } else {
        SoPickedPoint * point = action.getPickedPoint();
        if (point) {
            result.hits.push_back(toHit(*point));
        }
    }

    result.hit = !result.hits.empty();
    return result;
}

} // namespace obol
