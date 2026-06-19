#include <Obol/cad/CadAssembly.h>
#include <Obol/compat/cad/SoCADAssembly.h>

#include <Inventor/SbBox3f.h>
#include <Inventor/SbColor4f.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SoType.h>

namespace obol {
namespace {

SoCADAssembly::DrawMode toLegacyDrawMode(CadDrawMode mode)
{
    switch (mode) {
    case CadDrawMode::Shaded:
        return SoCADAssembly::SHADED;
    case CadDrawMode::Wireframe:
        return SoCADAssembly::WIREFRAME;
    case CadDrawMode::ShadedWithEdges:
        return SoCADAssembly::SHADED_WITH_EDGES;
    }
    return SoCADAssembly::WIREFRAME;
}

SoCADAssembly::PickMode toLegacyPickMode(CadPickMode mode)
{
    switch (mode) {
    case CadPickMode::Auto:
        return SoCADAssembly::PICK_AUTO;
    case CadPickMode::Edge:
        return SoCADAssembly::PICK_EDGE;
    case CadPickMode::Triangle:
        return SoCADAssembly::PICK_TRIANGLE;
    case CadPickMode::Bounds:
        return SoCADAssembly::PICK_BOUNDS;
    case CadPickMode::Hybrid:
        return SoCADAssembly::PICK_HYBRID;
    }
    return SoCADAssembly::PICK_AUTO;
}

void ensureCadNodeClassInitialized()
{
    if (SoCADAssembly::getClassTypeId().isBad()) {
        SoCADAssembly::initClass();
    }
}

SbVec3f
toLegacyVec3(const Vec3 & value)
{
    return SbVec3f(value.x, value.y, value.z);
}

SbColor4f
toLegacyColor(const Color & value)
{
    return SbColor4f(value.r, value.g, value.b, value.a);
}

SbMatrix
toLegacyMatrix(const CadMatrix4 & value)
{
    return SbMatrix(value.values[0], value.values[1], value.values[2], value.values[3],
                    value.values[4], value.values[5], value.values[6], value.values[7],
                    value.values[8], value.values[9], value.values[10], value.values[11],
                    value.values[12], value.values[13], value.values[14], value.values[15]);
}

SbBox3f
toLegacyBounds(const CadBounds3 & bounds)
{
    SbBox3f result;
    if (!bounds.empty) {
        result.setBounds(toLegacyVec3(bounds.minimum),
                         toLegacyVec3(bounds.maximum));
    }
    return result;
}

InstanceStyle
toLegacyStyle(const CadInstanceStyle & style)
{
    InstanceStyle result;
    result.hasColorOverride = style.hasColorOverride;
    result.color = toLegacyColor(style.color);
    result.lineWidth = style.lineWidth;
    return result;
}

PartGeometry
toLegacyGeometry(const CadPartGeometry & geometry)
{
    PartGeometry result;
    if (geometry.wire) {
        WireRep wire;
        wire.bounds = toLegacyBounds(geometry.wire->bounds);
        for (const CadWirePolyline & polyline : geometry.wire->polylines) {
            WirePolyline legacyPolyline;
            legacyPolyline.edgeId = polyline.edgeId;
            legacyPolyline.points.reserve(polyline.points.size());
            for (const Vec3 & point : polyline.points) {
                legacyPolyline.points.push_back(toLegacyVec3(point));
            }
            wire.polylines.push_back(legacyPolyline);
        }
        result.wire = wire;
    }
    if (geometry.shaded) {
        TriMesh mesh;
        mesh.positions.reserve(geometry.shaded->positions.size());
        for (const Vec3 & position : geometry.shaded->positions) {
            mesh.positions.push_back(toLegacyVec3(position));
        }
        mesh.normals.reserve(geometry.shaded->normals.size());
        for (const Vec3 & normal : geometry.shaded->normals) {
            mesh.normals.push_back(toLegacyVec3(normal));
        }
        mesh.indices = geometry.shaded->indices;
        mesh.bounds = toLegacyBounds(geometry.shaded->bounds);
        result.shaded = mesh;
    }
    return result;
}

InstanceRecord
toLegacyRecord(const CadInstanceRecord & record)
{
    InstanceRecord result;
    result.part = record.part;
    result.localToRoot = toLegacyMatrix(record.localToRoot);
    result.parent = record.parent;
    result.childName = record.childName;
    result.occurrenceIndex = record.occurrenceIndex;
    result.boolOp = record.boolOp;
    result.style = toLegacyStyle(record.style);
    return result;
}

} // namespace

void
CadAssembly::setDrawMode(CadDrawMode mode)
{
    drawMode_ = mode;
}

CadDrawMode
CadAssembly::drawMode() const
{
    return drawMode_;
}

void
CadAssembly::setPickMode(CadPickMode mode)
{
    pickMode_ = mode;
}

CadPickMode
CadAssembly::pickMode() const
{
    return pickMode_;
}

void
CadAssembly::setEdgePickTolerance(float pixels)
{
    edgePickTolerancePx_ = pixels;
}

float
CadAssembly::edgePickTolerance() const
{
    return edgePickTolerancePx_;
}

void
CadAssembly::setWireframeOcclusion(bool enabled)
{
    wireframeOcclusion_ = enabled;
}

bool
CadAssembly::wireframeOcclusion() const
{
    return wireframeOcclusion_;
}

void
CadAssembly::setLodEnabled(bool enabled)
{
    lodEnabled_ = enabled;
}

bool
CadAssembly::lodEnabled() const
{
    return lodEnabled_;
}

void
CadAssembly::upsertPart(PartId id, const CadPartGeometry & geometry)
{
    parts_[id] = geometry;
}

void
CadAssembly::removePart(PartId id)
{
    parts_.erase(id);
}

InstanceId
CadAssembly::upsertInstanceAuto(const CadInstanceRecord & record)
{
    const InstanceId id = CadIdBuilder::extendNameOccBool(record.parent,
                                                          record.childName,
                                                          record.occurrenceIndex,
                                                          record.boolOp);
    upsertInstance(id, record);
    return id;
}

void
CadAssembly::upsertInstance(InstanceId id, const CadInstanceRecord & record)
{
    instances_[id] = record;
}

void
CadAssembly::removeInstance(InstanceId id)
{
    instances_.erase(id);
}

void
CadAssembly::updateInstanceTransform(InstanceId id, const CadMatrix4 & localToRoot)
{
    auto it = instances_.find(id);
    if (it != instances_.end()) {
        it->second.localToRoot = localToRoot;
    }
}

void
CadAssembly::updateInstanceStyle(InstanceId id, const CadInstanceStyle & style)
{
    auto it = instances_.find(id);
    if (it != instances_.end()) {
        it->second.style = style;
    }
}

void
CadAssembly::setSelectedInstances(const std::vector<InstanceId> & ids)
{
    selectedInstances_ = ids;
}

void
CadAssembly::setHiddenInstances(const std::vector<InstanceId> & ids)
{
    hiddenInstances_ = ids;
}

size_t
CadAssembly::partCount() const
{
    return parts_.size();
}

size_t
CadAssembly::instanceCount() const
{
    return instances_.size();
}

std::vector<PartId>
CadAssembly::partIds() const
{
    std::vector<PartId> ids;
    ids.reserve(parts_.size());
    for (const auto & entry : parts_) {
        ids.push_back(entry.first);
    }
    return ids;
}

std::vector<InstanceId>
CadAssembly::instanceIds() const
{
    std::vector<InstanceId> ids;
    ids.reserve(instances_.size());
    for (const auto & entry : instances_) {
        ids.push_back(entry.first);
    }
    return ids;
}

const CadPartGeometry *
CadAssembly::partGeometry(PartId id) const
{
    auto it = parts_.find(id);
    return it == parts_.end() ? nullptr : &it->second;
}

std::optional<CadInstanceRecord>
CadAssembly::getInstanceRecord(InstanceId id) const
{
    auto it = instances_.find(id);
    return it == instances_.end()
        ? std::optional<CadInstanceRecord>{}
        : std::optional<CadInstanceRecord>{it->second};
}

const std::vector<InstanceId> &
CadAssembly::selectedInstances() const
{
    return selectedInstances_;
}

const std::vector<InstanceId> &
CadAssembly::hiddenInstances() const
{
    return hiddenInstances_;
}

NativeNodeHandle
CadAssembly::createLegacyNode() const
{
    ensureCadNodeClassInitialized();

    SoCADAssembly * node = new SoCADAssembly;
    node->drawMode.setValue(toLegacyDrawMode(drawMode_));
    node->pickMode.setValue(toLegacyPickMode(pickMode_));
    node->edgePickTolerancePx.setValue(edgePickTolerancePx_);
    node->wireframeOcclusion.setValue(wireframeOcclusion_ ? TRUE : FALSE);
    node->lodEnabled.setValue(lodEnabled_ ? TRUE : FALSE);

    node->beginUpdate();
    for (const auto & entry : parts_) {
        node->upsertPart(entry.first, toLegacyGeometry(entry.second));
    }
    for (const auto & entry : instances_) {
        node->upsertInstance(entry.first, toLegacyRecord(entry.second));
    }
    node->setSelectedInstances(selectedInstances_);
    node->setHiddenInstances(hiddenInstances_);
    node->endUpdate();

    return node;
}

} // namespace obol
